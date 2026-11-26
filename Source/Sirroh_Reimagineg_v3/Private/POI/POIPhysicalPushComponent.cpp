#include "POI/POIPhysicalPushComponent.h"
#include "POI/POICorridorTools.h"
#include "Components/PrimitiveComponent.h"
#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "UObject/UnrealType.h"

UPOIPhysicalPushComponent::UPOIPhysicalPushComponent()
{
 PrimaryComponentTick.bCanEverTick=true;
 PrimaryComponentTick.bStartWithTickEnabled=false;
 PrimaryComponentTick.TickGroup=TG_PrePhysics;
}
bool UPOIPhysicalPushComponent::IsHeld() const
{
 const auto* State=FindFProperty<FByteProperty>(GetOwner()->GetClass(),TEXT("PullpushState"));
 return Driver.IsValid() && State && State->GetPropertyValue_InContainer(GetOwner())==0;
}
bool UPOIPhysicalPushComponent::IsDrivenBy(const AActor* Pawn) const {return Driver.Get()==Pawn && IsHeld();}
void UPOIPhysicalPushComponent::CaptureBeforeGrab()
{
 if(auto* Root=Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent()))
 {GrabTransform=Root->GetComponentTransform();GrabVelocity=Root->GetPhysicsLinearVelocity();bCaptured=true;}
}
void UPOIPhysicalPushComponent::StartDrive()
{
 auto* Root=Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent());
 auto* Proxy=Root?Root->GetAttachParent():nullptr;
 if(!Proxy)return;
 if(DriverMover.IsValid())DriverMover->OnPostMovement.RemoveDynamic(this,&UPOIPhysicalPushComponent::AfterMovement);
 Driver=Proxy->GetOwner();Body=Root;RequestedAxis=0;
 DriverMover=Driver->FindComponentByClass<UMoverComponent>();
 if(DriverMover.IsValid())DriverMover->OnPostMovement.AddDynamic(this,&UPOIPhysicalPushComponent::AfterMovement);
 // AST has already captured its relative transforms and initialized native IK.
 // Never attach a simulated object to the pawn after this point.
 GetOwner()->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
 if(bCaptured)Root->SetWorldTransform(GrabTransform,false,nullptr,ETeleportType::TeleportPhysics);
 LocalGrip=Root->GetComponentTransform().InverseTransformPosition(Driver->GetActorLocation());
 OffsetX=Root->GetComponentLocation().X-Driver->GetActorLocation().X;
 if(auto* Collider=Cast<UPrimitiveComponent>(Proxy))Collider->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 UPOICorridorTools::ConfigurePushPlane(GetOwner());
 Root->SetSimulatePhysics(true);Root->WakeAllRigidBodies();
 if(bCaptured)Root->SetPhysicsLinearVelocity(GrabVelocity);
 bCaptured=false;
 SetComponentTickEnabled(true);
}
float UPOIPhysicalPushComponent::LimitInput(float Axis)
{
 RequestedAxis=FMath::Clamp(Axis,-1.f,1.f);
 if(!IsHeld() || !Body.IsValid())return Axis;
 const double Lag=Driver->GetActorLocation().X+OffsetX-Body->GetComponentLocation().X;
 return Axis*FMath::Clamp(1.-Lag*FMath::Sign(Axis)/12.,0.,1.);
}
void UPOIPhysicalPushComponent::TickComponent(float Dt,ELevelTick Type,FActorComponentTickFunction* Tick)
{
 Super::TickComponent(Dt,Type,Tick);
 auto* B=Body.Get();
 if(!IsHeld() || !B){Driver.Reset();SetComponentTickEnabled(false);return;}
 auto* Mover=Driver->FindComponentByClass<UMoverComponent>();
 const double Error=Driver->GetActorLocation().X+OffsetX-B->GetComponentLocation().X;
 if(!IsWithinReach())
 {UPOICorridorTools::ReleaseHeldPushable(Driver.Get());Driver.Reset();SetComponentTickEnabled(false);return;}
 const double TargetSpeed=FMath::Clamp(RequestedAxis*160.+Error*8.,-200.,200.);
 // Fixed maximum player effort, not mass-independent acceleration. Gravity and
 // object/world contacts remain entirely in Chaos; no vertical spring or snap.
 const double Force=FMath::Clamp((TargetSpeed-B->GetPhysicsLinearVelocity().X)*B->GetMass()*12.,-80000.,80000.); // 800 N maximum player effort.
 B->AddForce(FVector(Force,0,0));
}

bool UPOIPhysicalPushComponent::IsWithinReach() const
{
 if(!IsHeld() || !Body.IsValid())return false;
 const FVector P=Driver->GetActorLocation();FVector Closest;
 const float Distance=Body->GetClosestPointOnCollision(P,Closest);
 return Distance>=0?Distance<=100.f:FVector::Dist(P,Body->Bounds.GetBox().GetClosestPointTo(P))<=100.;
}

void UPOIPhysicalPushComponent::LimitMotion(double BodyX,double BodyVelocity,double Offset,float Axis,FVector& P,FVector& V)
{
 // The grip is a positional limit even when input is zero or supplied by AST.
 // Root motion and residual Mover velocity can continue after input stops.
 const double Center=BodyX-Offset;
 if(P.X>=Center+12.)
 {P.X=Center+12.;V.X=FMath::Min(V.X,BodyVelocity);}
 else if(P.X<=Center-12.)
 {P.X=Center-12.;V.X=FMath::Max(V.X,BodyVelocity);}

}
void UPOIPhysicalPushComponent::AfterMovement(const FMoverTimeStep&,FMoverSyncState& State,FMoverAuxStateContext&)
{
 if(!IsHeld() || !Body.IsValid())return;
 auto* Sync=State.SyncStateCollection.FindMutableDataByType<FMoverDefaultSyncState>();if(!Sync)return;
 FVector P=Sync->GetLocation_WorldSpace(),V=Sync->GetVelocity_WorldSpace();
 LimitMotion(Body->GetComponentLocation().X,Body->GetPhysicsLinearVelocity().X,OffsetX,RequestedAxis,P,V);
 Sync->SetTransforms_WorldSpace(P,Sync->GetOrientation_WorldSpace(),V,Sync->GetAngularVelocityDegrees_WorldSpace(),Sync->GetMovementBase(),Sync->GetMovementBaseBoneName());
}
void UPOIPhysicalPushComponent::EndPlay(const EEndPlayReason::Type Reason)
{
 if(DriverMover.IsValid())DriverMover->OnPostMovement.RemoveDynamic(this,&UPOIPhysicalPushComponent::AfterMovement);
 Super::EndPlay(Reason);
}
