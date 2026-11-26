#include "POI/POIPushTraversalComponent.h"
#include "POI/POICorridorTools.h"
#include "POI/POIMovingTraversalComponent.h"
#include "Combat/CombatVitalityComponent.h"
#include "PWG/PWGSidescrollerInputComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/UnrealType.h"

UPOIPushTraversalComponent::UPOIPushTraversalComponent()
{
 PrimaryComponentTick.bCanEverTick=true; PrimaryComponentTick.TickInterval=.05f;
}
void UPOIPushTraversalComponent::BeginPlay()
{
 Super::BeginPlay();RefreshEdges();
 if(auto* Body=Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent()))
 {
  UPOICorridorTools::ConfigurePushPlane(GetOwner());
  if(!Body->BodyInstance.bOverrideMass)Body->SetMassOverrideInKg(NAME_None,80.f);
  Body->SetUseCCD(true);
  Body->SetCollisionObjectType(ECC_PhysicsBody);
  Body->SetMaxDepenetrationVelocity(NAME_None,200.f);
  Body->SetSimulatePhysics(true);
 }
}
void UPOIPushTraversalComponent::RefreshEdges()
{
 auto* Body=Cast<UStaticMeshComponent>(GetOwner()->GetRootComponent());
 if (!Body || !Body->GetStaticMesh()) return;
 const FTransform T=Body->GetComponentTransform();
 const FBox B=Body->GetStaticMesh()->GetBoundingBox();
 const FVector E=B.GetExtent();
 FVector N=FVector::UpVector; double Best=-1;
 for (int I=0;I<3;++I) for (double Sign:{-1.,1.})
 { FVector Axis=FVector::ZeroVector;Axis[I]=Sign;const double Up=T.TransformVectorNoScale(Axis).Z;if(Up>Best){Best=Up;N=Axis;} }
 const int Face=FMath::Abs(N.X)>.5?0:(FMath::Abs(N.Y)>.5?1:2);
 const int Along=(Face+1)%3;
 FVector U=FVector::ZeroVector;U[Along]=1;
 const FVector V=FVector::CrossProduct(N,U);
 const double EU=E[Along],EV=FVector::DotProduct(V.GetAbs(),E);
 const FVector C=B.GetCenter()+N*E[Face];
 const FVector P[4]={C+U*EU-V*EV,C-U*EU-V*EV,C-U*EU+V*EV,C+U*EU+V*EV};
 TArray<UChildActorComponent*> Children;GetOwner()->GetComponents(Children);
 int Index=0;
 for(auto* Child:Children)
 {
  if(!Child->GetName().StartsWith(TEXT("ParkourActor")))continue;
  AActor* Edge=Child->GetChildActor();if(!Edge || Index>=4)continue;
  const FVector A=T.TransformPosition(P[Index]),Z=T.TransformVectorNoScale(N);
  const FVector Delta=T.TransformPosition(P[(Index+1)%4])-A; ++Index;
  Child->SetWorldTransform(FTransform(FRotationMatrix::MakeFromXZ(Delta.GetSafeNormal(),Z).ToQuat(),A,FVector::OneVector));
  if(auto* Spline=Edge->FindComponentByClass<USplineComponent>())
  {Spline->SetSplinePoints({FVector::ZeroVector,FVector(Delta.Size(),0,0)},ESplineCoordinateSpace::Local);Spline->SetSplinePointType(0,ESplinePointType::Linear,false);Spline->SetSplinePointType(1,ESplinePointType::Linear,true);}
  TArray<UPrimitiveComponent*> Primitives;Edge->GetComponents(Primitives);
  for(auto* Primitive:Primitives) if(Primitive->GetFName()!=TEXT("POIMovableGrip"))Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  auto* Grip=FindObjectFast<UBoxComponent>(Edge,TEXT("POIMovableGrip"));
  if(!Grip){Grip=NewObject<UBoxComponent>(Edge,TEXT("POIMovableGrip"));Edge->AddInstanceComponent(Grip);Grip->SetupAttachment(Edge->GetRootComponent());Grip->RegisterComponent();}
  Grip->SetRelativeLocation(FVector(Delta.Size()*.5,0,0));Grip->SetBoxExtent(FVector(Delta.Size()*.5,4,5));
  Grip->SetCollisionObjectType(ECC_GameTraceChannel4);Grip->SetCollisionResponseToAllChannels(ECR_Ignore);Grip->SetCollisionResponseToChannel(ECC_GameTraceChannel4,ECR_Block);
  Grip->SetGenerateOverlapEvents(false);Grip->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
 }
 LastRotation=T.GetRotation();bEdgesReady=true;
}
void UPOIPushTraversalComponent::ProtectTarget(AActor* Pawn)
{
 auto* Input=Pawn?Pawn->FindComponentByClass<UPWGSidescrollerInputComponent>():nullptr;
 if(!Input || !Input->IsTraversalActive())return;
 if(auto* Health=Pawn->FindComponentByClass<UCombatVitalityComponent>();Health && Health->UsesRagdollPose())return;
 for(UActorComponent* Part:Pawn->GetComponents())
 {
  if(!Part || !Part->GetClass()->GetName().StartsWith(TEXT("BPC_ParkourSystem")))continue;
  auto* Result=FindFProperty<FStructProperty>(Part->GetClass(),TEXT("Parkour Result"));if(!Result)continue;
  void* Data=Result->ContainerPtrToValuePtr<void>(Part);
  for(TFieldIterator<FObjectProperty> It(Result->Struct);It;++It)if(It->GetName().StartsWith(TEXT("LedgeActor_")))
  {
   auto* Edge=Cast<AActor>(It->GetObjectPropertyValue_InContainer(Data));
   for(AActor* Owner=Edge;Owner;Owner=Owner->GetParentActor()?Owner->GetParentActor():Owner->GetAttachParentActor())
   {
    auto* Support=Owner->FindComponentByClass<UPOIPushTraversalComponent>();
    auto* Body=Cast<UPrimitiveComponent>(Owner->GetRootComponent());
    if(!Body || Owner->GetParentActor() || Body->Mobility!=EComponentMobility::Movable)continue;
    if(Support)Support->Climber=Pawn;
    UPOIMovingTraversalComponent::Track(Pawn,Body);
    return;
   }
  }
 }
 UPOIMovingTraversalComponent::StopTracking(Pawn);
}
void UPOIPushTraversalComponent::Release()
{
 Climber.Reset();
}
void UPOIPushTraversalComponent::TickComponent(float Dt,ELevelTick Type,FActorComponentTickFunction* Tick)
{
 Super::TickComponent(Dt,Type,Tick);
 if(Climber.IsValid())
 {
  auto* Pawn=Climber.Get();auto* Input=Pawn?Pawn->FindComponentByClass<UPWGSidescrollerInputComponent>():nullptr;
  bool Current=false;
  if(Input && Input->IsTraversalActive())for(UActorComponent* Part:Pawn->GetComponents())
   if(Part && Part->GetClass()->GetName().StartsWith(TEXT("BPC_ParkourSystem")))
    if(auto* Result=FindFProperty<FStructProperty>(Part->GetClass(),TEXT("Parkour Result")))
     for(TFieldIterator<FObjectProperty> It(Result->Struct);It;++It)if(It->GetName().StartsWith(TEXT("LedgeActor_")))
      if(auto* Edge=Cast<AActor>(It->GetObjectPropertyValue_InContainer(Result->ContainerPtrToValuePtr<void>(Part)))) Current=Edge->GetParentActor()==GetOwner();
  if(Pawn)if(auto* Health=Pawn->FindComponentByClass<UCombatVitalityComponent>();Health && Health->UsesRagdollPose())Current=false;
  if(!Current)Release();
 }
 // Keep the local edge identity stable while held, even if another box face
 // rotates above it. The native child spline follows its physical parent.
 if(!Climber.IsValid() && (!bEdgesReady || !GetOwner()->GetActorQuat().Equals(LastRotation,.002f)))RefreshEdges();
}
void UPOIPushTraversalComponent::EndPlay(const EEndPlayReason::Type Reason){Release();Super::EndPlay(Reason);}
