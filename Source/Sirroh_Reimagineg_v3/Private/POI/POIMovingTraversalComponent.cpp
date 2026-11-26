#include "POI/POIMovingTraversalComponent.h"
#include "PWG/PWGSidescrollerInputComponent.h"
#include "Combat/CombatVitalityComponent.h"
#include "Components/PrimitiveComponent.h"
#include "MotionWarpingComponent.h"
#include "MoverDataModelTypes.h"
#include "MoveLibrary/BasedMovementUtils.h"
#include "UObject/UnrealType.h"

void UPOIMovingTraversalComponent::StopTracking(AActor* Pawn)
{
 if(auto* C=Pawn?Pawn->FindComponentByClass<UPOIMovingTraversalComponent>():nullptr)
 {C->ClearTargets();C->Base.Reset();}
}
void UPOIMovingTraversalComponent::PrepareLedge(AActor* Pawn,AActor* Edge)
{
 for(AActor* Owner=Edge;Owner;Owner=Owner->GetParentActor()?Owner->GetParentActor():Owner->GetAttachParentActor())
  if(!Owner->GetParentActor() && !Owner->GetAttachParentActor())
   if(auto* Body=Cast<UPrimitiveComponent>(Owner->GetRootComponent())){Track(Pawn,Body,true);return;}
}
void UPOIMovingTraversalComponent::Track(AActor* Pawn,UPrimitiveComponent* Support,bool FreshSelection)
{
 if(!Pawn || !Support || Support->Mobility!=EComponentMobility::Movable)return;
 auto* C=Pawn->FindComponentByClass<UPOIMovingTraversalComponent>();
 if(!C)
 {
  C=NewObject<UPOIMovingTraversalComponent>(Pawn);Pawn->AddInstanceComponent(C);C->RegisterComponent();
  C->Mover=Pawn->FindComponentByClass<UMoverComponent>();
  C->Warping=Pawn->FindComponentByClass<UMotionWarpingComponent>();
  if(C->Mover.IsValid())C->Mover->OnPostMovement.AddDynamic(C,&UPOIMovingTraversalComponent::AfterMovement);
  if(C->Warping.IsValid())C->Warping->OnPreUpdate.AddDynamic(C,&UPOIMovingTraversalComponent::BeforeWarp);
 }
 if(FreshSelection || C->Base.Get()!=Support)
 {C->ClearTargets();C->Base=Support;C->SelectionTransform=Support->GetComponentTransform();}
 if(!FreshSelection && C->Warping.IsValid())C->BeforeWarp(C->Warping.Get());
}
bool UPOIMovingTraversalComponent::IsActive() const
{
 const auto* Input=GetOwner()->FindComponentByClass<UPWGSidescrollerInputComponent>();
 const auto* Health=GetOwner()->FindComponentByClass<UCombatVitalityComponent>();
 return Base.IsValid() && Input && Input->IsTraversalActive() && !(Health && Health->UsesRagdollPose());
}
void UPOIMovingTraversalComponent::BeforeWarp(UMotionWarpingComponent* Warp)
{
 if(!IsActive()){ClearTargets();Base.Reset();return;}
 // BackFloor belongs to the landing floor, not necessarily to the obstacle.
 // Keep it in AST's original world frame until AST supplies its own floor base.
 for(FName Name:{FName("FrontLedge"),FName("BackLedge"),FName("VaultLedge")})
 {
  const auto* Target=Warp->FindWarpTarget(Name);if(!Target)continue;
  if(Target->Component.IsValid())continue; // Already relative (ours or native AST).
  const FTransform World=Target->GetTargetTrasform();
  auto& Anchor=Anchors.FindOrAdd(Name);
  if(!Anchor)
  {
   Anchor=NewObject<USceneComponent>(GetOwner());GetOwner()->AddInstanceComponent(Anchor);
   Anchor->SetupAttachment(Base.Get());Anchor->RegisterComponent();
  }
  // AST can publish the sampled world target after the support has moved.
  // Convert through the selection-time frame, not the later current frame.
  Anchor->SetRelativeTransform(World.GetRelativeTransform(SelectionTransform));
  Warp->AddOrUpdateWarpTargetFromComponent(Name,Anchor,NAME_None,true,EWarpTargetLocationOffsetDirection::TargetsForwardVector);
 }
}
void UPOIMovingTraversalComponent::AfterMovement(const FMoverTimeStep& TimeStep,FMoverSyncState& State,FMoverAuxStateContext& Aux)
{
 if(!IsActive())
 {
  if(auto* Sync=State.SyncStateCollection.FindMutableDataByType<FMoverDefaultSyncState>();Sync && Base.IsValid() && Sync->GetMovementBase()==Base.Get() && State.MovementMode!=TEXT("Walking"))
  {
   const FVector Location=Sync->GetLocation_WorldSpace();
   const FVector Velocity=Sync->GetVelocity_WorldSpace()+Base->GetPhysicsLinearVelocityAtPoint(Location);
   Sync->SetTransforms_WorldSpace(Location,Sync->GetOrientation_WorldSpace(),Velocity);
  }
  ClearTargets();Base.Reset();return;
 }
 // Animated traversal uses following warp targets. Hanging/shimmy uses Mover's
 // native dynamic base, including its post-physics update; no tick teleport.
 bool Animated=false;
 for(auto* Part:GetOwner()->GetComponents())if(Part && Part->GetClass()->GetName().StartsWith(TEXT("BPC_ParkourSystem")))
  if(auto* Flag=FindFProperty<FBoolProperty>(Part->GetClass(),TEXT("DoingTraversalAction")))Animated=Flag->GetPropertyValue_InContainer(Part);
 if(Animated || State.MovementMode==TEXT("Walking"))return;
 auto* Blackboard=Mover.IsValid()?Mover->GetSimBlackboard_Mutable():nullptr;
 if(!Blackboard)return; // Editor inspection can invoke delegates before simulation exists.
 auto& Sync=State.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();
 Sync.SetTransforms_WorldSpace(Sync.GetLocation_WorldSpace(),Sync.GetOrientation_WorldSpace(),Sync.GetVelocity_WorldSpace(),Base.Get(),NAME_None);
 FRelativeBaseInfo Info;Info.MovementBase=Base;
 Info.Location=Base->GetComponentLocation();Info.Rotation=Base->GetComponentQuat();
 Info.ContactLocalPosition=Info.Rotation.UnrotateVector(Sync.GetLocation_WorldSpace()-Info.Location);
 Blackboard->Set(CommonBlackboard::LastFoundDynamicMovementBase,Info);
}
void UPOIMovingTraversalComponent::ClearTargets()
{
 for(auto& Pair:Anchors)if(Pair.Value)
 {
  if(auto* Warp=Warping.Get())if(const auto* T=Warp->FindWarpTarget(Pair.Key);T && T->Component.Get()==Pair.Value)
   Warp->AddOrUpdateWarpTargetFromTransform(Pair.Key,T->GetTargetTrasform());
  Pair.Value->DestroyComponent();
 }
 Anchors.Reset();
}
void UPOIMovingTraversalComponent::EndPlay(const EEndPlayReason::Type Reason)
{
 ClearTargets();
 if(Mover.IsValid())Mover->OnPostMovement.RemoveDynamic(this,&UPOIMovingTraversalComponent::AfterMovement);
 if(Warping.IsValid())Warping->OnPreUpdate.RemoveDynamic(this,&UPOIMovingTraversalComponent::BeforeWarp);
 Super::EndPlay(Reason);
}
