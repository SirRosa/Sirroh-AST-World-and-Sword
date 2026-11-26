#include "Combat/DirectionalCombatComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Combat/CombatMotionCatalog.h"
#include "Placeholder/PlaceholderHandsComponent.h"
#include "Items/SirrohWorldItem.h"

void UDirectionalCombatComponent::OnRep_MotionStyle(const FCombatStyleDefinition& Previous)
{
    FCombatStyleDefinition Comparable=Previous;
    Comparable.bTwoHanded=MotionCoreStyle.bTwoHanded;
    Comparable.CatalogProfile=MotionCoreStyle.CatalogProfile;
    if (FCombatStyleDefinition::StaticStruct()->CompareScriptStruct(&Comparable,&MotionCoreStyle,0))
    {
        // GripMode carries its own transition; changing only the number of hands must
        // not execute the equipment/style reset on simulated or owning clients.
        if (GripMode.Serial>0) OnRep_GripMode();
        return;
    }
    OnRep_MotionCoreConfiguration();
}

void UDirectionalCombatComponent::ToggleGripMode()
{
    if (!bUseMotionCoreV3 || !IsLocal()) return;
    // Do not silently move or destroy a separate item occupying the supporting hand.
    if (Hands && IsValid(Hands->GetItem(1))) return;
    ServerToggleGripMode();
}

void UDirectionalCombatComponent::ServerToggleGripMode_Implementation()
{
    if(!GetEquippedWeaponGeometry().bHasSecondaryGrip)return;
    if (!bUseMotionCoreV3 || Reaction.bDisarmed || (Hands && IsValid(Hands->GetItem(1)))) return;
    PendingGripMode=!(PendingGripMode>=0?bool(PendingGripMode):MotionCoreStyle.bTwoHanded);
    UpdateGripMode();
}

void UDirectionalCombatComponent::UpdateGripMode()
{
    if (!GetOwner()->HasAuthority() || PendingGripMode<0 || !Mesh) return;
    if (Reaction.bDisarmed) { PendingGripMode=-1; return; }
    // Switching cannot cancel a strike, charge, recoil or the weapon's physical settling.
    if (bCombatActive && (GetCombatPhase()!=EDirectionalCombatPhase::Combat
        || CombatTime()<MotionReaction.GuardAt+0.2)) return;
    if (CombatTime()<GripMode.ChangedAt+0.25 && GripMode.Serial>0) return;
    FCombatStyleDefinition Candidate;
    FString Error;
    if (!UCombatMotionCatalog::StyleForGrip(MotionCoreStyle,bool(PendingGripMode),Candidate,Error))
    { UE_LOG(LogTemp,Warning,TEXT("Grip profile rejected: %s"),*Error); PendingGripMode=-1; return; }
    if (Candidate.bTwoHanded==MotionCoreStyle.bTwoHanded) { PendingGripMode=-1; return; }
    FCombatBodyRig Bound;
    const bool Cached=bOtherGripReady && bOtherGripTwoHanded==Candidate.bTwoHanded;
    if (Cached) Bound=MoveTemp(OtherGripRig);
    if (!Cached && !CombatBodySolver::BindRig(*Mesh,Candidate,Bound,Error))
    {
        UE_LOG(LogTemp,Warning,TEXT("Grip mode rejected: %s"),*Error);
        PendingGripMode=-1;
        return;
    }
    OtherGripRig=MoveTemp(MotionRig); bOtherGripReady=true; bOtherGripTwoHanded=MotionCoreStyle.bTwoHanded;
    GripMode.bTwoHanded=Candidate.bTwoHanded;
    MotionCoreStyle=MoveTemp(Candidate);
    GripMode.ChangedAt=CombatTime(); ++GripMode.Serial;
    PendingGripMode=-1;
    OnRep_GripMode();
    MotionRig=MoveTemp(Bound); bMotionRigAttempted=true; bMotionRigReady=true;
    GetOwner()->ForceNetUpdate();
}

void UDirectionalCombatComponent::OnRep_GripMode()
{
    GripTransitionStart=MotionFinalPose;
    FCombatStyleDefinition Gripped; FString Error;
    if (!UCombatMotionCatalog::StyleForGrip(MotionCoreStyle,GripMode.bTwoHanded,Gripped,Error))
    { UE_LOG(LogTemp,Warning,TEXT("Replicated grip profile rejected: %s"),*Error); return; }
    MotionCoreStyle=MoveTemp(Gripped);
    MotionCoreStyle.bTwoHanded=GripMode.bTwoHanded;
    if (bOtherGripReady)
    {
        if (bOtherGripTwoHanded==GripMode.bTwoHanded)
        { Swap(MotionRig,OtherGripRig); bOtherGripTwoHanded=!GripMode.bTwoHanded; }
        bMotionRigAttempted=true; bMotionRigReady=true;
    }
    else { bMotionRigAttempted=false; bMotionRigReady=false; }
    LibraryLoopSource=GuidanceSource=INDEX_NONE; GuidanceOffsets.Reset();
    LibraryActionSource=INDEX_NONE;
    // Keep physical weapon, velocity, epoch and attack IDs. This is a grip change,
    // not unequipping/re-equipping the weapon or resetting the Motion Core.
}

void UDirectionalCombatComponent::WarmOtherGrip()
{
    if (bOtherGripReady || !bMotionRigReady || !Mesh) return;
    FString Error;
    FCombatStyleDefinition Other;
    if (!UCombatMotionCatalog::StyleForGrip(MotionCoreStyle,!MotionCoreStyle.bTwoHanded,Other,Error)) return;
    bOtherGripReady=CombatBodySolver::BindRig(*Mesh,Other,OtherGripRig,Error);
    bOtherGripTwoHanded=Other.bTwoHanded;
}

bool UDirectionalCombatComponent::BlendGripTransition(FCombatFinalPose& SolvedPose,double Now)
{
    const float T=FMath::Clamp(float((Now-GripMode.ChangedAt)/0.25),0.0f,1.0f);
    const bool bBlend=GripMode.Serial>0 && T<1 && GripTransitionStart.BonesWorld.Num()==SolvedPose.BonesWorld.Num();
    if (bBlend)
    {
        const float TransitionAlpha=T*T*(3-2*T);
        const TMap<FName,FTransform> Target=SolvedPose.BonesWorld;
        for (int32 I=0; I<MotionRig.Names.Num(); ++I)
        {
            const int32 Parent=MotionRig.Parents[I];
            if (Parent==INDEX_NONE || I==MotionRig.Pelvis || !MotionRig.StyleBones[I]) continue;
            const FName Name=MotionRig.Names[I],ParentName=MotionRig.Names[Parent];
            const FTransform* Start=GripTransitionStart.BonesWorld.Find(Name);
            const FTransform* StartParent=GripTransitionStart.BonesWorld.Find(ParentName);
            if (!Start || !StartParent) continue;
            FTransform Local=Target.FindChecked(Name).GetRelativeTransform(Target.FindChecked(ParentName));
            const FQuat Initial=Start->GetRelativeTransform(*StartParent).GetRotation();
            Local.SetRotation(FQuat::Slerp(Initial,Local.GetRotation(),TransitionAlpha).GetNormalized());
            SolvedPose.BonesWorld[Name]=Local*SolvedPose.BonesWorld.FindChecked(ParentName);
        }
        SolvedPose.PrimaryGripWorld=MotionRig.GripLocal*SolvedPose.BonesWorld.FindChecked(MotionRig.Names[MotionRig.GripBone]);
        SolvedPose.PrimaryGripWorld.SetScale3D(FVector::OneVector);
        // The support hand is in transit. Do not advertise a closed rigid pair yet.
        SolvedPose.bHasSecondaryGrip=false;
    }
    else GripTransitionStart=FCombatFinalPose();
    return !bBlend;
}
