#include "Items/SirrohASTInventoryComponent.h"
#include "Items/SirrohCarryProps.h"
#include "Combat/CombatVitalityComponent.h"
#include "Combat/DirectionalCombatComponent.h"
#include "Combat/CombatContactSubsystem.h"
#include "Placeholder/PlaceholderHandsComponent.h"
#include "Sword/ProceduralSword.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "MoverComponent.h"
#include "DefaultMovementSet/InstantMovementEffects/BasicInstantMovementEffects.h"

AProceduralSword* UDirectionalCombatComponent::ActiveSword() const
{
    if(const auto* I=GetOwner()->FindComponentByClass<USirrohASTInventoryComponent>())return Cast<AProceduralSword>(I->GetNativeItem(0));
    return IsLocal() && !bAIControlled && Hands?Cast<AProceduralSword>(Hands->GetItem(0)):RemoteSword.Get();
}
FCombatWeaponDefinition UDirectionalCombatComponent::GetEquippedWeaponGeometry() const
{
    if(const auto* Weapon=ActiveSword())return Weapon->GetCombatDefinition();
    return AProceduralSword::BuildCombatDefinition(EquippedSword,Rig.PixelScale);
}

FCombatWeaponFrame UDirectionalCombatComponent::GetWeaponFrame() const
{
    if (bUseMotionCoreV3)
    {
        FCombatWeaponFrame Frame;
        Frame.Grip=MotionWeaponSample.PrimaryGripWorld.GetLocation();
        Frame.Rotation=MotionWeaponSample.PrimaryGripWorld.GetRotation();
        Frame.Angle=FMath::Atan2(Frame.Direction().Z,Frame.Direction().X);
        return Frame; // Legacy consumers cannot reconstruct an analytic v3 trajectory.
    }
    return EvaluateWeapon(CombatTime(),GetOwner()->GetActorLocation());
}

void UDirectionalCombatComponent::CacheRig()
{
    if (!CachedMesh || !Mesh || (!IsLocal() && !GetOwner()->HasAuthority())) return;
    const auto& Ref=CachedMesh->GetRefSkeleton();
    TArray<FTransform> Reference=Ref.GetRefBonePose();
    for (int32 I=0;I<Reference.Num();++I)
    {
        const int32 Parent=Ref.GetParentIndex(I);
        if (Parent!=INDEX_NONE) Reference[I]=Reference[I]*Reference[Parent];
    }
    const int32 A=Ref.FindBoneIndex(TEXT("upperarm_r")),B=Ref.FindBoneIndex(TEXT("lowerarm_r")),C=Ref.FindBoneIndex(TEXT("hand_r"));
    if (A==INDEX_NONE || B==INDEX_NONE || C==INDEX_NONE) return;
    const FTransform MeshLocal=Mesh->GetComponentTransform().GetRelativeTransform(GetOwner()->GetActorTransform());
    const FVector Shoulder=MeshLocal.TransformPosition(Reference[A].GetLocation());
    const FVector Elbow=MeshLocal.TransformPosition(Reference[B].GetLocation());
    const FVector Hand=MeshLocal.TransformPosition(Reference[C].GetLocation());
    Rig.Shoulder=Shoulder*GetOwner()->GetActorScale3D();
    Rig.ArmLength=FMath::Clamp(static_cast<float>((FVector::Distance(Shoulder,Elbow)+FVector::Distance(Elbow,Hand))*GetOwner()->GetActorScale3D().GetAbsMax()),10.0f,200.0f);
    const int32 Head=Ref.FindBoneIndex(TEXT("head")),Pelvis=Ref.FindBoneIndex(TEXT("pelvis"));
    if (Head!=INDEX_NONE) Rig.HeadHeight=MeshLocal.TransformPosition(Reference[Head].GetLocation()).Z*GetOwner()->GetActorScale3D().Z;
    if (Pelvis!=INDEX_NONE) Rig.PelvisHeight=MeshLocal.TransformPosition(Reference[Pelvis].GetLocation()).Z*GetOwner()->GetActorScale3D().Z;
}

void UDirectionalCombatComponent::UpdateWeapon(float Delta)
{
    if (bUseMotionCoreV3) return;
    if (GuardProfiles.Num()!=3) LoadGuardProfiles();
    SelectGuard();
    const float Facing=GetOwner()->GetActorForwardVector().X>=0?1.0f:-1.0f;
    const FVector Shoulder=CombatSpace::Local(Rig.Shoulder,Facing)-FVector(0,0,StanceLowering);
    const auto& Guard=GuardProfiles[static_cast<int32>(CurrentGuard)];
    const float Amount=Delta<=0?1:1-FMath::Exp(-FMath::Max(0.1f,FollowSpeed)*Delta);
    const FVector Grip=Shoulder+CombatSpace::Local(Guard.GuardGrip.GetClampedToMaxSize(0.95)*Rig.ArmLength,Facing);
    const float CursorTilt=FMath::Clamp(static_cast<float>((AimOffset.Z-Rig.Shoulder.Z)/FMath::Max(10.0f,Rig.ArmLength)),-1.0f,1.0f)*0.12f;
    const FQuat Rotation=FQuat(FVector::YAxisVector,CursorTilt*Facing)*CombatSpace::Orientation(CombatSpace::Local(Guard.GuardBlade,Facing));
    RestWeapon.Grip=FMath::Lerp(RestWeapon.Grip,Grip,static_cast<double>(Amount));
    RestWeapon.Rotation=FQuat::Slerp(RestWeapon.Rotation,Rotation,Amount).GetNormalized();
    SmoothedDirection=RestWeapon.Direction();
    RestWeapon.Angle=FMath::Atan2(SmoothedDirection.Z,SmoothedDirection.X);
}

bool UDirectionalCombatComponent::CanFight() const
{
    const auto* H=GetOwner()->FindComponentByClass<UCombatVitalityComponent>();
    return !H || (H->CanUseArm(false) && (!IsTwoHandedGrip() || H->CanUseArm(true)));
}
bool UDirectionalCombatComponent::IsMovementLocked() const
{
    if (const auto* H=GetOwner()->FindComponentByClass<UCombatVitalityComponent>(); H && H->UsesRagdollPose()) return true;
    return (bUseMotionCoreV3 && MotionReaction.bInterruptsAttack && CombatTime()<MotionReaction.GuardAt)
        || GetCombatPhase()==EDirectionalCombatPhase::Staggered;
}

void UDirectionalCombatComponent::SetBlocking(bool bEnabled)
{
    if (bUseMotionCoreV3) return;
    if (!IsLocal() || bEnabled==bBlocking) return;
    if (bEnabled && (!bCombatActive || GetCombatPhase()!=EDirectionalCombatPhase::Combat || CombatTime()<BlockDisabledUntil)) return;
    bBlocking=bEnabled;
    if (Hands && bEnabled) Hands->CancelCharge();
    if (GetOwner()->HasAuthority()) GetOwner()->ForceNetUpdate();
    else ServerSetBlocking(bEnabled);
}

void UDirectionalCombatComponent::ServerSetBlocking_Implementation(bool bEnabled)
{
    if (bUseMotionCoreV3) return;
    if (bEnabled && (!bCombatActive || GetCombatPhase()!=EDirectionalCombatPhase::Combat || CombatTime()<BlockDisabledUntil))
    { ClientCorrectBlocking(bBlocking); return; }
    bBlocking=bEnabled;
    GetOwner()->ForceNetUpdate();
}

void UDirectionalCombatComponent::ClientCorrectBlocking_Implementation(bool bEnabled)
{
    bBlocking=bEnabled;
}

void UDirectionalCombatComponent::StartAICombat(const FSwordInstance& Sword)
{
    if (!GetOwner()->HasAuthority()) return;
    bAIControlled=true;
    EquippedSword=Sword;
    BindComponents(); CacheRig();
    RearmAI();
}

void UDirectionalCombatComponent::RearmAI()
{
    if (!GetOwner()->HasAuthority() || !bAIControlled || !CanFight()) return;
    AttackMotion.bPrepared=false; AttackMotion.bReleased=false;
    bBlocking=false; bCombatActive=true; Reaction.bDisarmed=false;
    Reaction.EndTime=0; ++Reaction.Serial;
    if(!EquippedSword.InstanceId.IsValid())EquippedSword.InstanceId=FGuid::NewGuid();
    UpdateWeapon(0); UpdateRemoteSword();
    GetOwner()->ForceNetUpdate();
}

void UDirectionalCombatComponent::SetAITarget(const FVector& WorldTarget)
{
    if (!GetOwner()->HasAuthority() || !bAIControlled || WorldTarget.ContainsNaN()) return;
    AimOffset=(WorldTarget-GetOwner()->GetActorLocation()).GetClampedToMaxSize(5000);
}

void UDirectionalCombatComponent::React(ECombatContactResult Result,double Time,float Duration,bool bInterrupt,bool bDisarm)
{
    if (bUseMotionCoreV3) return; // Contact feedback is a later migration phase.
    if (!GetOwner()->HasAuthority()) return;
    // A shake cannot replace an active stagger or resurrect a disarmed weapon.
    if (!bInterrupt && (Reaction.bDisarmed || (Reaction.bInterrupt && CombatTime()<Reaction.EndTime))) return;
    FCombatReaction Next;
    Next.Serial=Reaction.Serial+1;
    Next.InterruptedAttackId=AttackMotion.Id;
    Next.PoseWeight=AttackPoseWeight();
    Next.Result=Result; Next.StartTime=CombatTime(); Next.EndTime=Next.StartTime+FMath::Max(0.08f,Duration);
    Next.Start=EvaluateWeapon(Time,GetOwner()->GetActorLocation());
    Next.Start.Grip-=GetOwner()->GetActorLocation();
    Next.End=Next.Start;
    if (Result==ECombatContactResult::Rebound && AttackMotion.bPrepared)
    {
        Next.End.Grip=FMath::Lerp(Next.Start.Grip,AttackMotion.StartGrip,0.5);
        Next.End.Angle+=FMath::FindDeltaAngleRadians(Next.Start.Angle,AttackMotion.StartAngle)*0.5f;
        Next.End.Rotation=FQuat::Slerp(Next.Start.Rotation,AttackMotion.StartRotation,0.5f).GetNormalized();
    }
    else if (Result==ECombatContactResult::Stagger || bDisarm)
    {
        Next.End.Grip-=Next.Start.Direction()*20;
        Next.End.Angle+=0.6f;
        Next.End.Rotation=FQuat(FVector::YAxisVector,0.6f)*Next.Start.Rotation;
    }
    Next.bInterrupt=bInterrupt; Next.bDisarmed=bDisarm;
    MulticastReaction(Next);
    GetOwner()->ForceNetUpdate();
}

void UDirectionalCombatComponent::MulticastReaction_Implementation(FCombatReaction Value)
{
    if (Value.Serial<LastAppliedReaction) return;
    Reaction=Value;
    OnRep_Reaction();
}

void UDirectionalCombatComponent::OnRep_Reaction()
{
    if (Reaction.Serial<LastAppliedReaction) { Reaction=AppliedReaction; return; }
    if (Reaction.Serial<=LastAppliedReaction) return;
    LastAppliedReaction=Reaction.Serial;
    AppliedReaction=Reaction;
    if (Reaction.bInterrupt)
    {
        if (AttackMotion.Id<=Reaction.InterruptedAttackId)
        { AttackMotion.bPrepared=false; AttackMotion.bReleased=false; }
        bBlocking=false;
    }
    if (Reaction.Result==ECombatContactResult::Stagger) BlockDisabledUntil=FMath::Max(BlockDisabledUntil,Reaction.EndTime);
    if (!Reaction.bDisarmed) return;
    bCombatActive=false;
    if (Hands) Hands->CancelCharge();
    AProceduralSword* Sword=ActiveSword();
    if(Sword && Sword->IsA<ASirrohTorch>() && GetOwner()->HasAuthority())
    {
        const FVector Origin=Sword->GetActorLocation();
        Sword->Throw(Origin,MotionWeaponSample.LinearVelocity.GetClampedToMaxSize(700)+FVector(0,0,80),GetOwner());
        if(auto* Inventory=GetOwner()->FindComponentByClass<USirrohASTInventoryComponent>())
            Inventory->SetNativeItem(0,nullptr);
        if(RemoteSword==Sword)RemoteSword=nullptr;
        return;
    }
    if (GetOwner()->HasAuthority())
    {
        const FVector Position=GetOwner()->GetActorLocation()+Reaction.Start.Grip;
        const FTransform Transform=bUseMotionCoreV3?MotionWeaponSample.WeaponWorld:
            FTransform(FRotationMatrix::MakeFromYZ(FVector::YAxisVector,-Reaction.Start.Direction()).ToQuat(),Position);
        if (auto* Dropped=GetWorld()->SpawnActorDeferred<AProceduralSword>(AProceduralSword::StaticClass(),Transform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
        {
            Dropped->Definition=EquippedSword; Dropped->WorldUnitsPerPixel=Rig.PixelScale;
            Dropped->SetReplicates(true); Dropped->SetReplicateMovement(true);
            UGameplayStatics::FinishSpawningActor(Dropped,Transform);
            Dropped->Throw(Transform.GetLocation(),bUseMotionCoreV3?
                MotionWeaponSample.LinearVelocity.GetClampedToMaxSize(700)+FVector(0,0,80):
                -Reaction.Start.Direction()*160+FVector(0,0,100),GetOwner());
            Dropped->PersistentId=FGuid::NewGuid().ToString(); // A dropped weapon remains available for pickup.
        }
    }
    if (Sword)
    {
        if(auto* I=GetOwner()->FindComponentByClass<USirrohASTInventoryComponent>())
            for(int32 Slot=0;Slot<I->GetNativeSlotCount();++Slot)if(I->GetNativeItem(Slot)==Sword)I->SetNativeItem(Slot,nullptr);
        if (RemoteSword==Sword) RemoteSword=nullptr;
        Sword->Destroy();
    }
}

void UDirectionalCombatComponent::ApplyKnockback(const FVector& Velocity)
{
    if (!GetOwner()->HasAuthority()) return;
    if (auto* Mover=GetOwner()->FindComponentByClass<UMoverComponent>())
    {
        TSharedPtr<FApplyVelocityEffect> Effect=MakeShared<FApplyVelocityEffect>();
        Effect->VelocityToApply=Velocity; Effect->bAdditiveVelocity=true;
        Mover->QueueInstantMovementEffect(Effect);
    }
    OnKnockback.Broadcast(Velocity);
}

void UDirectionalCombatComponent::MulticastContact_Implementation(FCombatContactEvent Event)
{
    OnContact.Broadcast(Event);
}

void UDirectionalCombatComponent::ClaimWorldSword(AProceduralSword* Sword,int32 Slot)
{
    if (!IsLocal() || !IsValid(Sword) || !Sword->GetIsReplicated()) return;
    ServerClaimWorldSword(Sword,Slot);
}

void UDirectionalCombatComponent::ServerClaimWorldSword_Implementation(AProceduralSword* Sword,int32 Slot)
{
    if (!IsValid(Sword) || !Sword->GetIsReplicated() || !Sword->CanPickUpBy(GetOwner()) || Slot<0 || Slot>1) return;
    const FVector Difference=Sword->GetActorLocation()-GetOwner()->GetActorLocation();
    if (FVector2D(Difference.X,Difference.Z).Size()>180 || FMath::Abs(Difference.Y)>80) return;
    auto* Inventory=GetOwner()->FindComponentByClass<USirrohASTInventoryComponent>();
    if(!Inventory || Inventory->GetNativeItem(Slot))return;
    if(const auto* Health=GetOwner()->FindComponentByClass<UCombatVitalityComponent>();Health && (Health->IsIncapacitated() || !Health->CanUseArm(Slot==1)))return;
    Sword->Take(GetOwner());
    if(!Sword->IsHeld() || Sword->GetOwner()!=GetOwner())return;
    if(!Inventory->SetNativeItem(Slot,Sword))Sword->Throw(Sword->GetActorLocation(),FVector::ZeroVector,GetOwner());
    // Replicate the original actor and authoritative slot, never clone/destroy it.

}

