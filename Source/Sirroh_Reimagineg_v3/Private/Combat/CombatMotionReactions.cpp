#include "Combat/CombatVitalityComponent.h"
#include "Combat/CombatReactionCatalog.h"
#include "CombatInertiaMove.h"
#include "MoverComponent.h"
#include "Combat/CombatTrainingNPC.h"
#include "Combat/DirectionalCombatComponent.h"
#include "CombatDragonWeaponAdapter.h"
#include "Sword/ProceduralSword.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "TwoBoneIK.h"
#include "Engine/DataTable.h"

void UDirectionalCombatComponent::ApplyVitalityRestrictions()
{
    if (!GetOwner()->HasAuthority() || CanFight()) return;
    CancelAttack();
    if (!Reaction.bDisarmed && EquippedSword.InstanceId.IsValid())
    {
        FCombatReaction Drop;
        Drop.Serial=Reaction.Serial+1; Drop.bDisarmed=true; Drop.bInterrupt=true;
        Drop.Result=ECombatContactResult::Disarm; Drop.StartTime=CombatTime(); Drop.EndTime=Drop.StartTime+0.1;
        Drop.InterruptedAttackId=BodyAttack.Id;
        Drop.Start.Grip=MotionWeaponSample.PrimaryGripWorld.GetLocation()-GetOwner()->GetActorLocation();
        MulticastReaction(Drop);
    }
    ResetMotionCore(); GetOwner()->ForceNetUpdate();
}

void UDirectionalCombatComponent::ReceiveMotionBodyHit(FCombatMotionBodyHit& Hit)
{
    if (!bUseMotionCoreV3 || !GetOwner()->HasAuthority() || !Hit.Attacker) return;
    const auto* Attacker=Hit.Attacker->FindComponentByClass<UDirectionalCombatComponent>();
    if (!Attacker && !Hit.bRanged) return;
    const float SourceMass=Hit.bRanged?FMath::Max(0.01f,Hit.SourceMass):Attacker->GetEquippedWeaponGeometry().WeaponWeight;
    ReceiveBodyContact(Hit,SourceMass);
    if (Hit.DamageApplied>0 && !Hit.bRanged && Attacker && Hit.Action!=TEXT("Kick") && Attacker->MotionWeaponAdapter)
    {
        const FVector Impulse=(Hit.ContactVelocity*SourceMass*0.9*Hit.ImpulseScale).GetClampedToMaxSize(6500);
        Attacker->MotionWeaponAdapter->ApplyBodyContact(-Impulse,Hit.Point);
    }
}

void UDirectionalCombatComponent::ReceiveBodyContact(FCombatMotionBodyHit& Hit,float SourceMass)
{
    if (!GetOwner()->HasAuthority() || Hit.Target!=GetOwner() || Hit.ContactVelocity.ContainsNaN()) return;
    if (auto* Health=GetOwner()->FindComponentByClass<UCombatVitalityComponent>())
    {
        if (Hit.TargetLifeId!=Health->LifeId || Health->LifeState==ECombatLifeState::Dead) return;
        Hit.DamageApplied=Health->ApplyContact(Hit,SourceMass);
        if (Hit.DamageApplied<=0) return;
        if (Health->IsIncapacitated())
        {
            Health->QueueRagdollImpulse(Hit.ContactVelocity*(Hit.Action==TEXT("Kick")?4.f:FMath::Max(1.f,SourceMass))*Hit.ImpulseScale);
            return;
        }
    }
    // UE impulses are kg*cm/s. A limited fraction of the weapon's measured momentum is transferred.
    const FVector Impulse=(Hit.ContactVelocity*(Hit.Action==TEXT("Kick")?4.f:SourceMass)*0.9*Hit.ImpulseScale).GetClampedToMaxSize(6500);
    const float Strength=FMath::Clamp(float(Impulse.Size()/1800),0.2f,1.0f);
    FName Bone=Hit.BodyRegion;
    if (Bone==TEXT("pelvis") && !MotionRig.Spine.IsEmpty()) Bone=MotionRig.Names[MotionRig.Spine[0]];
    BeginMotionReaction(Bone,Hit.Point,Impulse,Strength,false);
    // Root movement is resolved independently of the visual physics body; no vertical teleport.
    const FVector Horizontal=FVector::VectorPlaneProject(Impulse,FVector::UpVector);
    const float Distance=FMath::Clamp(float(Horizontal.Size()/100),12.0f,35.0f);
    if (!QueueReactionRootMotion()) QueueMotionKnockback(Horizontal.GetSafeNormal()*Distance);

}

void UDirectionalCombatComponent::ReceiveMotionWeaponHit(const FCombatPhysicalContact& Hit)
{
    if (!GetOwner()->HasAuthority() || Reaction.bDisarmed || !Hit.OtherActor) return;
    const auto* Other=Hit.OtherActor->FindComponentByClass<UDirectionalCombatComponent>();
    if (!Other || (!Hit.bOtherAttacking && !Hit.bSelfAttacking)) return;
    if (Hit.Impulse.Size()<80 || (MotionReaction.bWeapon && CombatTime()-MotionReaction.Start<0.1)) return;
    const float Mass=GetEquippedWeaponGeometry().WeaponWeight;
    const float Strength=FMath::Clamp(float(Hit.Impulse.Size()/(1800*FMath::Sqrt(Mass))),0.1f,1.0f);
    BeginMotionReaction(TEXT("hand_r"),Hit.Point,Hit.Impulse,Strength,true);
    // Chaos has already applied recoil. Strong opposing impulses can exceed grip capacity.
    if (Hit.Impulse.Size()>3000*FMath::Sqrt(Mass) && Hit.bOtherAttacking)
    {
        FCombatReaction Drop;
        Drop.Serial=Reaction.Serial+1; Drop.bDisarmed=true; Drop.bInterrupt=true;
        Drop.Result=ECombatContactResult::Disarm; Drop.StartTime=CombatTime(); Drop.EndTime=Drop.StartTime+0.1;
        Drop.InterruptedAttackId=BodyAttack.Id;
        Drop.Start.Grip=MotionWeaponSample.PrimaryGripWorld.GetLocation()-GetOwner()->GetActorLocation();
        if (MotionWeaponAdapter) MotionWeaponAdapter->Stop();
        MulticastReaction(Drop); GetOwner()->ForceNetUpdate();
    }
}

void UDirectionalCombatComponent::BeginMotionReaction(FName Bone,const FVector& Point,const FVector& Impulse,float Strength,bool bWeapon)
{
    MotionReaction.DefenseProfile=NAME_None; MotionReaction.AnimationEntry=0; MotionReaction.AnimationExit=1;
    MotionReaction.Serial++;
    MotionReaction.Start=CombatTime();
    const double PreviousGuard=MotionReaction.GuardAt;
    // Small deflections bend the body but do not erase the source clock/attack identity.
    MotionReaction.bInterruptsAttack=!bWeapon || Strength>=0.65f;
    MotionReaction.GuardAt=MotionReaction.bInterruptsAttack?
        FMath::Max(PreviousGuard,MotionReaction.Start+(bWeapon?0.1:FMath::Lerp(0.2,0.3,double(Strength)))):PreviousGuard;
    MotionReaction.Bone=Bone; MotionReaction.Point=Point; MotionReaction.Impulse=Impulse;
    MotionReaction.Strength=Strength; MotionReaction.bWeapon=bWeapon;
    SelectHitAnimation();
    if (MotionReaction.bInterruptsAttack)
    {
        LibraryActionSource=INDEX_NONE;
        BufferedBodyAttack=FBufferedBodyAttack();
        BodyAttack.bPrepared=false; BodyAttack.bReleased=false;
        bMotionSubmittedAttackActive=false;
        CancelInertiaMovement();
    }
    OnRep_MotionReaction();
    GetOwner()->ForceNetUpdate();
}

const FCombatBodyRig* UDirectionalCombatComponent::GetReactionRig(FName Id)
{
    if (!Mesh || Id.IsNone()) return nullptr;
    if (auto* Cached=ReactionRigs.Find(Id))
        if (Cached->Names==MotionRig.Names) return Cached;
    FCombatBodyRig CachedRig;
    if (!CombatReactions::LoadRig(*Mesh,MotionCoreStyle,Id,CachedRig))
    {
        if (MotionReaction.DefenseProfile.IsNone()) return nullptr;
        auto* Table=LoadObject<UDataTable>(nullptr,TEXT("/Game/Systems/Procedural_Combat/DT_Combat_Motion_v3.DT_Combat_Motion_v3"));
        FCombatStyleDefinition Style; FString Error;
        if (!UCombatMotionCatalog::StyleForProfile(Table,MotionReaction.DefenseProfile,Style,Error)
            || !CombatReactions::LoadRig(*Mesh,Style,Id,CachedRig)) return nullptr;
    }
    return &ReactionRigs.Add(Id,MoveTemp(CachedRig));
}
void UDirectionalCombatComponent::SelectHitAnimation()
{
    const FName Previous=MotionReaction.AnimationRow;
    MotionReaction.AnimationRow=NAME_None;
    if (MotionReaction.bWeapon) return;
    const FString Bone=MotionReaction.Bone.ToString();
    const FName Region=Bone.Contains(TEXT("head"))?TEXT("Head"):Bone.Contains(TEXT("neck"))?TEXT("Neck"):
        (Bone.Contains(TEXT("thigh"))||Bone.Contains(TEXT("calf"))||Bone.Contains(TEXT("foot")))?TEXT("Legs"):TEXT("Torso");
    const FVector Incoming=GetOwner()->GetActorQuat().UnrotateVector(-MotionReaction.Impulse).GetSafeNormal();
    const auto* Mover=GetOwner()->FindComponentByClass<UMoverComponent>();
    const bool Supported=!Mover || Mover->HasGameplayTag(Mover_IsOnGround,false);
    MotionReaction.AnimationRow=CombatReactions::Select(TEXT("Hit"),Region,Incoming,IsTwoHandedGrip()?2:1,
        MotionReaction.Strength,false,false,MotionReaction.Serial,Previous,Supported,&MotionCoreStyle);
    ReactionFacing=FVector::ZeroVector;
    MotionReaction.EntryFacing=GetOwner()->GetActorForwardVector();
    if (const auto* Cached=GetReactionRig(MotionReaction.AnimationRow))
    {
        MotionReaction.AnimationDuration=Cached->Motions[0].Length;
        MotionReaction.GuardAt=MotionReaction.Start+MotionReaction.AnimationDuration;
    }
}

bool UDirectionalCombatComponent::BlendHitAnimation(FCombatFinalPose& Final,double Now)
{
    if (MotionReaction.AnimationRow.IsNone() || !Mesh || Now<MotionReaction.Start) return false;
    const double Elapsed=Now-MotionReaction.Start;
    const double Duration=FMath::Max(0.1f,MotionReaction.AnimationDuration);
    if (Elapsed>=Duration) return false;
    const auto* Cached=GetReactionRig(MotionReaction.AnimationRow);
    if (!Cached || Cached->Names!=MotionRig.Names) return false;
    TArray<FTransform> Local,Component;
    if (!CombatBodySolver::SampleMotion(*Cached,0,FMath::Lerp(MotionReaction.AnimationEntry,MotionReaction.AnimationExit,float(Elapsed/Duration)),Local,Component)) return false;
    const float Weight=FMath::Max(0.65f,MotionReaction.Strength)*FMath::Min(float(Elapsed/0.06),1.f)*FMath::Clamp(float((Duration-Elapsed)/0.2),0.f,1.f);
    if (const auto* Entry=CombatReactions::Find(MotionReaction.AnimationRow,&MotionCoreStyle); Entry && Entry->bTurn &&
        GetOwner()->HasAuthority() && !GetOwner()->FindComponentByClass<UMoverComponent>())
        GetOwner()->SetActorRotation(FRotator(0,MotionReaction.EntryFacing.Rotation().Yaw+180.f*float(Elapsed/Duration),0));
    const auto Original=Final.BonesWorld;
    for (int32 I=0;I<MotionRig.Names.Num();++I)
    {
        const int32 Parent=MotionRig.Parents[I];
        const FName Name=MotionRig.Names[I];
        if (MotionReaction.bWeapon)
        {
            bool Lower=I==MotionRig.Pelvis;
            for (int32 A=I;A!=INDEX_NONE;A=MotionRig.Parents[A]) if (A==MotionRig.Hips[0] || A==MotionRig.Hips[1]) Lower=true;
            if (Lower) continue;
        }
        if (Parent==INDEX_NONE || !Local.IsValidIndex(I)) continue;
        const FName ParentName=MotionRig.Names[Parent];
        const FTransform* Bone=Original.Find(Name); const FTransform* ParentBone=Original.Find(ParentName);
        const FTransform* NewParent=Final.BonesWorld.Find(ParentName);
        if (!Bone || !ParentBone || !NewParent) continue;
        FTransform Relative=Bone->GetRelativeTransform(*ParentBone);
        Relative.SetRotation(FQuat::Slerp(Relative.GetRotation(),Local[I].GetRotation(),Weight).GetNormalized());
        if (I==MotionRig.Pelvis) Relative.SetTranslation(FMath::Lerp(Relative.GetTranslation(),Local[I].GetTranslation(),Weight));
        Final.BonesWorld[Name]=Relative*(*NewParent);
    }
    // Correct the reaction's authored steps against terrain at the displaced actor,
    // without pinning both feet or adding a second physical ground solver.
    const auto* GroundMover=GetOwner()->FindComponentByClass<UMoverComponent>();
    const auto* Capsule=GetOwner()->FindComponentByClass<UCapsuleComponent>();
    if (!MotionReaction.bWeapon && Capsule && (!GroundMover || GroundMover->HasGameplayTag(Mover_IsOnGround,false)))
    {
        const double FloorZ=Capsule->GetComponentLocation().Z-Capsule->GetScaledCapsuleHalfHeight();
        FCollisionObjectQueryParams Objects;
        Objects.AddObjectTypesToQuery(ECC_WorldStatic); Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
        Objects.AddObjectTypesToQuery(ECC_GameTraceChannel5);
        for (int32 Side=0;Side<2;++Side)
        {
            const int32 Hip=MotionRig.Hips[Side], Knee=MotionRig.Knees[Side], Foot=MotionRig.Feet[Side];
            if (Hip==INDEX_NONE || Knee==INDEX_NONE || Foot==INDEX_NONE) continue;
            const auto Before=Final.BonesWorld;
            FTransform Upper=Before.FindChecked(MotionRig.Names[Hip]);
            FTransform Lower=Before.FindChecked(MotionRig.Names[Knee]);
            FTransform End=Before.FindChecked(MotionRig.Names[Foot]);
            FVector Start=End.GetLocation(), Finish=Start;
            Start.Z=FMath::Max(Start.Z+40,FloorZ+60); Finish.Z=FloorZ-60;
            FCollisionQueryParams Query(SCENE_QUERY_STAT(CombatReactionFeet),false,GetOwner());
            if (ActiveSword()) Query.AddIgnoredActor(ActiveSword());
            FHitResult Ground; bool Supported=false;
            for (int32 Attempt=0;Attempt<8;++Attempt)
            {
                if (!GetWorld()->LineTraceSingleByObjectType(Ground,Start,Finish,Objects,Query)) break;
                auto* Surface=Ground.GetComponent(); if (!Surface) break;
                if (Surface->ComponentHasTag(TEXT("CombatTerrain"))) { Supported=Ground.ImpactNormal.Z>0.65; break; }
                Query.AddIgnoredComponent(Surface);
            }
            if (!Supported) continue;
            FVector Target=End.GetLocation();
            Target.Z=FMath::Max(Target.Z+FMath::Clamp(Ground.ImpactPoint.Z-FloorZ,-25.0,25.0),Ground.ImpactPoint.Z+2);
            Target.Z=FMath::Lerp(End.GetLocation().Z,Target.Z,Weight);
            AnimationCore::SolveTwoBoneIK(Upper,Lower,End,Lower.GetLocation(),Target,false,1.0,1.0);
            for (int32 I=0;I<MotionRig.Names.Num();++I)
                for (int32 A=I;A!=INDEX_NONE;A=MotionRig.Parents[A])
                {
                    const FTransform* Replacement=A==Foot?&End:A==Knee?&Lower:A==Hip?&Upper:nullptr;
                    if (!Replacement) continue;
                    Final.BonesWorld[MotionRig.Names[I]]=Before.FindChecked(MotionRig.Names[I]).GetRelativeTransform(
                        Before.FindChecked(MotionRig.Names[A])) * (*Replacement);
                    break;
                }
        }
    }
    if (MotionRig.Names.IsValidIndex(MotionRig.GripBone))
        if (const auto* Grip=Final.BonesWorld.Find(MotionRig.Names[MotionRig.GripBone])) Final.PrimaryGripWorld=MotionRig.GripLocal*(*Grip);
    if (Final.bHasSecondaryGrip && MotionRig.Names.IsValidIndex(MotionRig.SupportGripBone))
        if (const auto* Grip=Final.BonesWorld.Find(MotionRig.Names[MotionRig.SupportGripBone])) Final.SecondaryGripWorld=MotionRig.SupportGripLocal*(*Grip);
    return Weight>UE_SMALL_NUMBER;
}
void UDirectionalCombatComponent::OnRep_MotionReaction()
{
    if (MotionReaction.Serial<=MotionAppliedReaction) return;
    MotionAppliedReaction=MotionReaction.Serial;
    const auto* Entry=CombatReactions::Find(MotionReaction.AnimationRow,&MotionCoreStyle);
    ReactionFacing=Entry && Entry->bTurn?-MotionReaction.EntryFacing:FVector::ZeroVector;
    if (MotionReaction.bInterruptsAttack) CaptureAttackStart();
    MotionContactReaction=(MotionContactReaction+MotionReaction.Impulse/2200).GetClampedToMaxSize(1);
}

bool UDirectionalCombatComponent::QueueReactionRootMotion()
{
    const auto* Entry=CombatReactions::Find(MotionReaction.AnimationRow,&MotionCoreStyle);
    const auto* Cached=GetReactionRig(MotionReaction.AnimationRow);
    if (!GetOwner()->HasAuthority() || !Entry || !Cached || !Mesh) return false;
    const auto& Motion=Cached->Motions[0];
    const FTransform RootMotion=Motion.Sequence->ExtractRootMotionFromRange(0,Motion.Length,FAnimExtractContext());
    TArray<FVector> Path;
    float UsableTravel=0;
    for (int32 I=0;I<=30;++I)
    {
        const FVector Root=Mesh->GetComponentQuat().RotateVector(Motion.Sequence->ExtractRootMotionFromRange(
            0,Motion.Length*I/30.f,FAnimExtractContext()).GetTranslation());
        Path.Add(FVector(Root.X,0,0));
        UsableTravel=FMath::Max(UsableTravel,FMath::Abs(float(Root.X)));
    }
    // Validate after projecting onto the actual movement plane. An off-plane root
    // track must not replace a real knockback with a zero-length move.
    if (UsableTravel<1.f) return false; // caller supplies one small fallback, never two competing moves
    if (auto* Mover=GetOwner()->FindComponentByClass<UMoverComponent>())
    {
        if (!Mover->HasGameplayTag(Mover_IsOnGround,false)) return false;
        Mover->CancelFeaturesWithTag(FGameplayTag::RequestGameplayTag(TEXT("Combat.Motion.HitReaction")),true);
        auto Move=MakeShared<FCombatInertiaMove>(); Move->bHitReaction=true;
        Move->GroundMode=Mover->GetMovementModeName(); Move->TravelMs=Motion.Length*1000; Move->DurationMs=Move->TravelMs;
        Move->bReactionTurn=Entry->bTurn; Move->ReactionStartYaw=GetOwner()->GetActorRotation().Yaw;
        Move->ReactionTurnYaw=Entry->bTurn?RootMotion.Rotator().Yaw:0.f;
        if (Entry->bTurn && FMath::Abs(Move->ReactionTurnYaw)<90) Move->ReactionTurnYaw=180;
        Move->MotionPath=Path;
        if (Entry->bTurn) ReactionFacing=FRotator(0,Move->ReactionStartYaw+Move->ReactionTurnYaw,0).Vector();
        Mover->QueueLayeredMove(Move);
        return true;
    }
    else if (auto* NPC=Cast<ACombatTrainingNPC>(GetOwner()))
    {
        NPC->QueueCombatPath(Path,Motion.Length);
        return true;
    }
    return false;
}
