#include "CombatMotionContactSubsystem.h"
#include "Combat/DirectionalCombatComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Sword/ProceduralSword.h"
#include "Animation/AnimSequence.h"
#include "ChooserFunctionLibrary.h"

namespace
{
    float EaseBody(float T) { T=FMath::Clamp(T,0.0f,1.0f); return T*T*(3-2*T); }
}

void UDirectionalCombatComponent::SubmitBodyAttack(uint8 Command, EDirectionalAttack Type)
{
    if (!IsLocal()) return;
    UpdateCursorAim();
    // Ordered reliable commands also preserve press/release when both occur before replication.
    // Authority owns timestamps and tuning. Owner prediction is a separate migration stage.
    ServerBodyAttack(Command,Type,EquippedSword.InstanceId,GetOwner()->GetActorLocation()+FVector(AimOffset));
}

double UDirectionalCombatComponent::BodyChainTime() const
{
    // Debounce only. Reversal recruits the body and preserves momentum; it does not
    // wait for a nominal contact marker or the end of the current source.
    return BodyAttack.PreparedAt+0.12;
}

void UDirectionalCombatComponent::ProcessBufferedBodyAttack()
{
    if (!GetOwner()->HasAuthority()) return;
    const double Now=CombatTime();
    if (BodyAttack.bPrepared && !BodyAttack.bReleased && bCombatActive && !Reaction.bDisarmed
        && Now>=BodyAttack.PreparedAt+BodyAttack.Preparation+0.8)
        ServerBodyAttack_Implementation(1,BodyAttack.Type,BodyAttack.WeaponId,GetOwner()->GetActorLocation()+FVector(AimOffset));
    if (!BufferedBodyAttack.bPending) return;
    if (!bCombatActive || Reaction.bDisarmed || BufferedBodyAttack.WeaponId!=EquippedSword.InstanceId
        || Now>BufferedBodyAttack.ExpiresAt)
    { BufferedBodyAttack=FBufferedBodyAttack(); PendingAttackFacing=FVector::ZeroVector; return; }
    if (Now<BodyChainTime()) return;
    const auto Pending=BufferedBodyAttack;
    BufferedBodyAttack=FBufferedBodyAttack();
    const uint32 PreviousId=BodyAttack.Id;
    ServerBodyAttack_Implementation(0,Pending.Type,Pending.WeaponId,Pending.Target);
    if (BodyAttack.Id==PreviousId && BufferedBodyAttack.bPending)
    { BufferedBodyAttack.bReleased=Pending.bReleased; BufferedBodyAttack.ExpiresAt=Pending.ExpiresAt; }
    if (Pending.bReleased && BodyAttack.Id!=PreviousId)
        ServerBodyAttack_Implementation(1,Pending.Type,Pending.WeaponId,Pending.Target);
}

void UDirectionalCombatComponent::ServerBodyAttack_Implementation(uint8 Command, EDirectionalAttack Type,
    FGuid WeaponId, FVector_NetQuantize10 Target)
{
    if (!bUseMotionCoreV3 || Command>2 || (Command!=2 && !CanFight())) return;
    if (Command==2)
    {
        PendingAttackFacing=FVector::ZeroVector;
        BufferedBodyAttack=FBufferedBodyAttack();
        BodyAttack.bPrepared=false;
        CancelInertiaMovement();
        GetOwner()->ForceNetUpdate();
        return;
    }
    if (!bCombatActive || Reaction.bDisarmed || !bMotionCoreValid || WeaponId!=EquippedSword.InstanceId
        || !WeaponId.IsValid() || FVector(Target).ContainsNaN()
        || FVector::DistSquared(Target,GetOwner()->GetActorLocation())>FMath::Square(5100.0)
        || static_cast<uint8>(Type)>static_cast<uint8>(EDirectionalAttack::Pierce)) return;
    const double Now=CombatTime();
    if (Command==0 && Now<MotionReaction.GuardAt+0.2)
    {
        // New intent blends from the presented pose rather than waiting for a reaction lockout.
        CaptureAttackStart();
        MotionReaction.GuardAt=Now;
        MotionReaction.AnimationRow=NAME_None;
        MotionReaction.bInterruptsAttack=false;
    }
    if (Command==0)
    {
        const FVector ToTarget=FVector::VectorPlaneProject(FVector(Target)-GetOwner()->GetActorLocation(),FVector::UpVector);
        if (ToTarget.SizeSquared()>1600 && FVector::DotProduct(ToTarget.GetSafeNormal(),GetOwner()->GetActorForwardVector())<0.25)
        {
            BufferedBodyAttack.bPending=true; BufferedBodyAttack.Type=Type; BufferedBodyAttack.WeaponId=WeaponId;
            BufferedBodyAttack.Target=Target; BufferedBodyAttack.ExpiresAt=Now+1.5;
            if (FVector(PendingAttackFacing).IsNearlyZero())
            {
                PendingAttackFacing=FVector(ToTarget.X<0?-1:1,0,0);
                ServerLibraryAction_Implementation(TEXT("Turn"),PendingAttackFacing);
            }
            GetOwner()->ForceNetUpdate(); return;
        }
        PendingAttackFacing=FVector::ZeroVector;
        if (IsLibraryActionPlaying())
        {
            CaptureAttackStart();
            LibraryActionSource=INDEX_NONE; LibraryActionVariantId=NAME_None;
            ++LibraryActionSerial; // Replicas also start a fresh source transition.
            CancelInertiaMovement();
        }
        const auto Phase=GetCombatPhase();
        const double ChainAt=BodyChainTime();
        const bool bChain=Phase==EDirectionalCombatPhase::Recovery ||
            (Phase==EDirectionalCombatPhase::Aiming && !BodyAttack.bReleased) ||
            ((Phase==EDirectionalCombatPhase::Attacking || (Phase==EDirectionalCombatPhase::Aiming && BodyAttack.bReleased)) && Now>=ChainAt);
        if (Phase!=EDirectionalCombatPhase::Combat && !bChain)
        {
            if ((Phase==EDirectionalCombatPhase::Attacking || Phase==EDirectionalCombatPhase::Aiming) && BodyAttack.bReleased)
            {
                BufferedBodyAttack.bPending=true; BufferedBodyAttack.bReleased=false;
                BufferedBodyAttack.Type=Type; BufferedBodyAttack.WeaponId=WeaponId;
                BufferedBodyAttack.Target=Target;
                BufferedBodyAttack.ExpiresAt=FMath::Max(Now+0.5,ChainAt+0.5);
            }
            return;
        }
        BufferedBodyAttack=FBufferedBodyAttack();
        const auto PreviousStartRotations=MotionSolverState.AttackStartRotations;
        const auto PreviousStartPelvis=MotionSolverState.AttackStartPelvisLocal;
        CaptureAttackStart();
        FVector StartRay=GetOwner()->GetActorForwardVector();
        if (const FTransform* Shoulder=MotionFinalPose.BonesWorld.Find(TEXT("upperarm_r")))
            StartRay=(MotionFinalPose.PrimaryGripWorld.GetLocation()-Shoulder->GetLocation()).GetSafeNormal(UE_SMALL_NUMBER,StartRay);
        const float StartReach=FMath::Clamp(float(MotionDiagnostics.PrimaryArmExtension),0.2f,0.95f);
        const FCombatBodyAttack PreviousAttack=BodyAttack;
        const uint32 Id=BodyAttack.Id+1;
        int32 Combo=bChain?BodyAttack.ComboIndex+1:0;
        BodyAttack=FCombatBodyAttack();
        BodyAttack.Id=Id;
        BodyAttack.bChained=bChain;
        BodyAttack.StartArmDirection=StartRay;
        BodyAttack.StartExtension=StartReach;
        BodyAttack.WeaponId=WeaponId;
        BodyAttack.Type=Type;
        BodyAttack.ComboIndex=Combo;
        BodyAttack.StartOrigin=GetOwner()->GetActorLocation();
        BodyAttack.PlanningFrame=GetOwner()->GetActorQuat();
        BodyAttack.StartWorkingPoint=FMath::Lerp(MotionWeaponSample.BladeBase,MotionWeaponSample.BladeTip,
            Type==EDirectionalAttack::Pierce?1.0:0.75);
        BodyAttack.StartWorkingVelocity=MotionWeaponSample.LinearVelocity+FVector::CrossProduct(
            MotionWeaponSample.AngularVelocity,BodyAttack.StartWorkingPoint-MotionWeaponSample.WeaponWorld.GetLocation());
        BodyAttack.StartWorkingVelocity=BodyAttack.StartWorkingVelocity.GetClampedToMaxSize(2500);
        const FVector Facing=FVector::VectorPlaneProject(FVector(Target)-GetOwner()->GetActorLocation(),FVector::UpVector);
        (void)Facing; // Facing is commanded through Mover by double-tapping A/D.
        TArray<int32> Candidates;
        for (int32 Index=0; Index<MotionRig.Motions.Num(); ++Index)
        {
            const auto& Source=MotionRig.Sources[Index];
            if (Source.Action==TEXT("Attack") && MotionRig.Motions[Index].IsValid() &&
                (MotionRig.Catalog.IsValid() || (Type==EDirectionalAttack::Pierce?Source.bThrusts:Source.bCuts))) Candidates.Add(Index);
        }
        // A dedicated thrust library takes precedence over legacy multi-family clips.
        if (Type==EDirectionalAttack::Pierce && MotionCoreStyle.MotionLibrary.IsNull())
        {
            const int32 DedicatedStart=FMath::Min(10,MotionCoreStyle.MotionSources.Num());
            const bool bDedicated=Candidates.ContainsByPredicate([&](int32 Index) { return Index>=DedicatedStart; });
            if (bDedicated) Candidates.RemoveAll([&](int32 Index) { return Index<DedicatedStart; });
        }
        if (!Candidates.IsEmpty())
        {
            Combo%=Candidates.Num(); BodyAttack.ComboIndex=Combo;
            BodyAttack.MotionSourceIndex=Candidates[Combo];
        }
        if (!MotionCoreStyle.MotionLibrary.IsNull())
        {
            BodyAttack.MotionSourceIndex=SelectLibraryAttack(Candidates,Type,Target);
            if (BodyAttack.MotionSourceIndex==INDEX_NONE)
            {
                BodyAttack=PreviousAttack;
                MotionSolverState.AttackStartRotations=PreviousStartRotations;
                MotionSolverState.AttackStartPelvisLocal=PreviousStartPelvis;
                return;
            }
        }
        if (UChooserTable* Chooser=MotionCoreStyle.MotionChooser.LoadSynchronous(); Chooser)
        {
            FCombatMotionSelection Selection;
            Selection.StyleId=MotionCoreStyle.StyleId;
            Selection.AttackType=Type; Selection.ComboIndex=Combo; Selection.bChained=bChain;
            Selection.TargetDistance=FVector::Distance(Target,GetOwner()->GetActorLocation());
            Selection.WeaponWeight=GetEquippedWeaponGeometry().WeaponWeight;
            Selection.BladeDirection=GetOwner()->GetActorQuat().UnrotateVector(
                (MotionWeaponSample.BladeTip-MotionWeaponSample.BladeBase).GetSafeNormal());
            FChooserEvaluationContext Context;
            Context.AddStructParam(Selection);
            const FInstancedStruct Evaluation=UChooserFunctionLibrary::MakeEvaluateChooser(Chooser);
            const auto* Selected=Cast<UAnimSequence>(UChooserFunctionLibrary::EvaluateObjectChooserBase(
                Context,Evaluation,UAnimSequence::StaticClass()));
            TArray<int32> Preferred;
            for (int32 Index:Candidates) if (MotionRig.Sources[Index].Animation.Get()==Selected) Preferred.Add(Index);
            if (!Preferred.IsEmpty())
            {
                if (MotionRig.Catalog)
                {
                    const bool PreviousLayering=BodyAttack.bLayeredAnimation;
                    const int32 Choice=SelectLibraryAttack(Preferred,Type,Target);
                    if (Choice!=INDEX_NONE) BodyAttack.MotionSourceIndex=Choice;
                    else BodyAttack.bLayeredAnimation=PreviousLayering;
                }
                else BodyAttack.MotionSourceIndex=Preferred[0];
            }
        }
        if (MotionRig.Sources.IsValidIndex(BodyAttack.MotionSourceIndex))
            BodyAttack.MotionVariantId=MotionRig.Sources[BodyAttack.MotionSourceIndex].CatalogVariantId;
        CancelInertiaMovement();
        MotionInertiaAttackId=Id; bMotionPullIssued=false;
        MotionSnapshotAttackId=Id;
        BodyAttack.bPrepared=true;
        BodyAttack.PreparedAt=Now;
        // Timing only: no legacy guard profiles or weapon paths are evaluated.
        BodyAttack.Preparation=FMath::Max(MotionCoreStyle.MinimumPreparationSeconds,Type==EDirectionalAttack::Heavy?0.32f:Type==EDirectionalAttack::Pierce?0.2f:0.24f);
        BodyAttack.Duration=Type==EDirectionalAttack::Heavy?0.48f:Type==EDirectionalAttack::Pierce?0.3f:0.38f;
        BodyAttack.Recovery=Type==EDirectionalAttack::Heavy?0.7f:0.55f;
        const auto& Hints=MotionRig.Hints[static_cast<int32>(Type)];
        if (Hints.WindTime>=0 && Hints.DestinationTime>Hints.WindTime && Hints.FinishTime>Hints.DestinationTime)
        {
            // Marker intervals are bounded timing hints, not a playback clock.
            BodyAttack.Duration=FMath::Clamp(Hints.DestinationTime-Hints.WindTime,0.22f,0.8f);
            BodyAttack.Recovery=FMath::Clamp(Hints.FinishTime-Hints.DestinationTime,0.45f,1.0f);
        }
        const auto Weapon=GetEquippedWeaponGeometry();
        if (MotionRig.Motions.IsValidIndex(BodyAttack.MotionSourceIndex))
        {
            const auto& Source=MotionRig.Sources[BodyAttack.MotionSourceIndex];
            const float Length=MotionRig.Motions[BodyAttack.MotionSourceIndex].Length;
            const float PlaybackScale=1.0f; // Source timing is authored; family changes geometry, not playback speed.
            // Match only the preparation interval: never skip the contact or follow-through.
            if (bChain && MotionSolverState.AttackStartRotations.Num()==MotionRig.Names.Num())
            {
                double BestCost=TNumericLimits<double>::Max();
                for (int32 Sample=0; Sample<=8; ++Sample)
                {
                    const float Time=Source.LoadTime*Sample/8.0f;
                    TArray<FTransform> Local,Component;
                    if (!CombatBodySolver::SampleMotion(MotionRig,BodyAttack.MotionSourceIndex,Time,Local,Component)) continue;
                    double Cost=0;
                    for (int32 Bone=0; Bone<Local.Num(); ++Bone)
                        if (MotionRig.StyleBones[Bone]) Cost+=FMath::Square(
                            Local[Bone].GetRotation().AngularDistance(MotionSolverState.AttackStartRotations[Bone]));
                    if (Cost<BestCost) { BestCost=Cost; BodyAttack.SourceEntryTime=Time; }
                }
            }
            BodyAttack.Preparation=FMath::Max(0.08f,Length*(Source.LoadTime-BodyAttack.SourceEntryTime)*PlaybackScale);
            if (bChain)
            {
                const FVector Desired=(FVector(Target)-BodyAttack.StartWorkingPoint).GetSafeNormal();
                const float Opposing=FMath::Max(0.0f,float(-FVector::DotProduct(BodyAttack.StartWorkingVelocity,Desired)));
                // A reversal has time to brake; an aligned continuation does not reload from guard.
                BodyAttack.Preparation=FMath::Clamp(0.12f+Opposing/3500.0f,0.12f,0.5f);
            }
            BodyAttack.SourceStrikeTime=Source.LoadTime;
            BodyAttack.Duration=Length*(Source.FinishTime-Source.LoadTime)*PlaybackScale;
            const FTransform* Pelvis=MotionFinalPose.BonesWorld.Find(TEXT("pelvis"));
            BodyAttack.NeutralTargetWorld=(Pelvis?Pelvis->GetLocation():GetOwner()->GetActorLocation())+
                GetOwner()->GetActorQuat().RotateVector(Source.NeutralTarget);
            BodyAttack.NeutralTargetWorld.Z=BodyAttack.StartWorkingPoint.Z+Source.NeutralTarget.Z;
        }
        else
        {
            const float Handling=FMath::Sqrt(FMath::Clamp(Weapon.WeaponWeight,0.5f,4.0f));
            BodyAttack.Duration*=Handling;
            BodyAttack.Preparation*=Handling;
        }
    }
    else
    {
        if (BufferedBodyAttack.bPending && BufferedBodyAttack.Type==Type && BufferedBodyAttack.WeaponId==WeaponId)
        {
            BufferedBodyAttack.bReleased=true; BufferedBodyAttack.Target=Target;
            return;
        }
        if (!BodyAttack.bPrepared || BodyAttack.bReleased || BodyAttack.Type!=Type || BodyAttack.WeaponId!=WeaponId) return;
        BodyAttack.bReleased=true;
        BodyAttack.ReleasedAt=Now;
        BodyAttack.StrikeAt=FMath::Max(Now,BodyAttack.PreparedAt+FMath::Max(double(BodyAttack.Preparation),0.001));
        if (MotionRig.Motions.IsValidIndex(BodyAttack.MotionSourceIndex))
        {
            const auto& Source=MotionRig.Sources[BodyAttack.MotionSourceIndex];
            const float Length=MotionRig.Motions[BodyAttack.MotionSourceIndex].Length;
            const float Hold=FMath::Max(0.0f,float(Now-BodyAttack.PreparedAt)-BodyAttack.Preparation);
            BodyAttack.SourceStrikeTime=FMath::Min(Source.ContactTime-0.001f,Source.LoadTime+Hold*0.2f/Length);
            BodyAttack.Duration=Length*(Source.FinishTime-BodyAttack.SourceStrikeTime);
        }
        BodyAttack.Target=Target;
        BodyAttack.ApproachOrigin=GetOwner()->GetActorLocation();
        const FVector Origin=Mesh?Mesh->GetSocketLocation(TEXT("pelvis")):GetOwner()->GetActorLocation();
        BodyAttack.Direction=(FVector(Target)-Origin).GetSafeNormal(UE_SMALL_NUMBER,GetOwner()->GetActorForwardVector());
        if (MotionCoreStyle.bEnableAttackApproach && !BodyAttack.bLayeredAnimation)
        {
            const auto Weapon=GetEquippedWeaponGeometry();
            const FVector GroundTarget=FVector::VectorPlaneProject(FVector(Target)-GetOwner()->GetActorLocation(),FVector::UpVector);
            const bool Heavy=Type==EDirectionalAttack::Heavy, Thrust=Type==EDirectionalAttack::Pierce;
            const auto& Tuning=Heavy?MotionCoreStyle.HeavyMovement:Thrust?MotionCoreStyle.PierceMovement:MotionCoreStyle.LightMovement;
            const float DefaultCap=Heavy?200.0f:Thrust?170.0f:90.0f;
            const float Cap=FMath::Clamp(Tuning.MaximumDistance>0?Tuning.MaximumDistance:DefaultCap,0.0f,200.0f);
            const FTransform* Shoulder=MotionFinalPose.BonesWorld.Find(TEXT("upperarm_r"));
            const FTransform* Elbow=MotionFinalPose.BonesWorld.Find(TEXT("lowerarm_r"));
            const FTransform* Hand=MotionFinalPose.BonesWorld.Find(TEXT("hand_r"));
            const float ArmLength=Shoulder && Elbow && Hand?
                FVector::Distance(Shoulder->GetLocation(),Elbow->GetLocation())+
                FVector::Distance(Elbow->GetLocation(),Hand->GetLocation()):0;
            const float NaturalReach=ArmLength*FMath::Clamp(MotionCoreStyle.Anatomy.MaximumArmExtension,0.2f,0.98f)
                +Weapon.WeaponReach*(Thrust?1.0f:0.75f);
            const FVector FromShoulder=FVector(Target)-(Shoulder?Shoulder->GetLocation():Origin);
            const float HorizontalReach=FMath::Sqrt(FMath::Max(0.0f,NaturalReach*NaturalReach-float(FromShoulder.Z*FromShoulder.Z)));
            // Close targets need weight transfer, not a mandatory step. Unreachable height
            // is not converted to a longer ground lunge.
            const float Missing=FMath::Max(0.0f,float(FromShoulder.Size2D())-HorizontalReach);
            const float Distance=FMath::Abs(FromShoulder.Z)>NaturalReach?0:
                FMath::Min(Cap,Missing)*EaseBody(Missing/15.0f);
            float Adaptation=1;
            if (MotionRig.Sources.IsValidIndex(BodyAttack.MotionSourceIndex))
                Adaptation=FMath::Clamp(float(FVector::Distance(Target,BodyAttack.NeutralTargetWorld)/
                    FMath::Max(1.0,MotionRig.Sources[BodyAttack.MotionSourceIndex].NeutralTarget.Size())),0.0f,1.0f);
            BodyAttack.ApproachDisplacement=GroundTarget.GetSafeNormal()*Distance*Adaptation;
            BodyAttack.ApproachSeconds=FMath::Clamp(FMath::Max(Tuning.DurationSeconds,Distance/220.0f),0.25f,1.0f);
        }
        if (UsesIndicatorDefense()) GetWorld()->GetSubsystem<UCombatMotionContactSubsystem>()->CommitAttack(this);
    }
    GetOwner()->ForceNetUpdate();
}

FCombatBodyDrive UDirectionalCombatComponent::EvaluateBodyAttack(FCombatIntent& Intent) const
{
    FCombatBodyDrive Drive;
    const auto Phase=GetCombatPhase();
    if (Phase!=EDirectionalCombatPhase::Aiming && Phase!=EDirectionalCombatPhase::Attacking
        && Phase!=EDirectionalCombatPhase::Recovery) return Drive;
    const double Now=Intent.SimulationTime;
    const FVector Strike=BodyAttack.bReleased?BodyAttack.Direction:Intent.GuardDirectionWorld;
    if (BodyAttack.bReleased) Intent.AttackTarget=BodyAttack.Target;
    Intent.AttackType=BodyAttack.Type;
    Intent.AttackId=BodyAttack.Id;
    Intent.Phase=Phase==EDirectionalCombatPhase::Aiming?ECombatMotionPhase::Aiming:
        Phase==EDirectionalCombatPhase::Attacking?ECombatMotionPhase::Attacking:ECombatMotionPhase::Recovery;
    Drive.StrikeDirection=Strike;
    const float Preparation=EaseBody((Now-BodyAttack.PreparedAt)/FMath::Max(double(BodyAttack.Preparation),0.001));
    const double ChargeEnd=BodyAttack.bReleased?BodyAttack.ReleasedAt:Now;
    const float Charge=FMath::Clamp(float(ChargeEnd-BodyAttack.PreparedAt-MotionCoreStyle.ChargeStartSeconds)
        /FMath::Max(0.1f,MotionCoreStyle.MaximumChargeSeconds),0.0f,1.0f);
    const bool bHeavy=BodyAttack.Type==EDirectionalAttack::Heavy;
    const bool bPierce=BodyAttack.Type==EDirectionalAttack::Pierce;
    const float Load=FMath::DegreesToRadians(FMath::Clamp(MotionCoreStyle.CounterLoadDegrees
        +(bHeavy?20:0)+Charge*MotionCoreStyle.ChargedExtraLoadDegrees,45.0f,135.0f));
    // Target defines the travel sector continuously. Front cuts use a family-specific
    // plane; oblique cuts preserve the target's full XYZ, with no guard buckets.
    const FVector Forward=GetOwner()->GetActorForwardVector();
    const FVector Sector=FVector::VectorPlaneProject(Strike,Forward);
    const float Obliqueness=FMath::Clamp(float(Sector.Size()),0.0f,1.0f);
    const FVector DefaultPlane=bHeavy?GetOwner()->GetActorUpVector():GetOwner()->GetActorRightVector();
    FVector Tangent=(Sector+DefaultPlane*FMath::Square(1-Obliqueness)*0.15).GetSafeNormal(UE_SMALL_NUMBER,DefaultPlane);
    const FVector Axis=FVector::CrossProduct(Forward,Tangent).GetSafeNormal();
    const float T=BodyAttack.bReleased?FMath::Clamp(float((Now-BodyAttack.StrikeAt)/BodyAttack.Duration),0.0f,1.0f):0;
    // Accelerate out of preparation but retain a nonzero slope through nominal destination.
    // Physical settling, not this curve reaching one, decides when recovery may begin.
    const float Progress=T*T*(2-T);
    Drive.SwingAxis=Axis;
    Drive.FollowThrough=BodyAttack.bReleased && Now>=BodyAttack.StrikeAt+BodyAttack.Duration?1.0f:0.0f;
    if (BodyAttack.bChained) Drive.FollowThrough=FMath::Max(Drive.FollowThrough,1-Preparation);
    Drive.StartPoseBlend=1-Preparation;
    Drive.PhaseAlpha=Progress;
    Drive.Weight=Preparation;
    if (Phase==EDirectionalCombatPhase::Recovery)
    {
        Drive.RecoveryAlpha=EaseBody((Now-BodyAttack.ReturnAt)/BodyAttack.Recovery);
        Drive.Weight*=1-Drive.RecoveryAlpha;
    }
    if (MotionRig.Motions.IsValidIndex(BodyAttack.MotionSourceIndex)
        && MotionRig.Motions[BodyAttack.MotionSourceIndex].IsValid())
    {
        const auto& Source=MotionRig.Sources[BodyAttack.MotionSourceIndex];
        Drive.MotionSourceIndex=BodyAttack.MotionSourceIndex;
        Drive.MotionWeight=FMath::Clamp(Source.PoseWeight,0.0f,1.0f)*(1-Drive.RecoveryAlpha);
        Drive.LegMotionWeight=BodyAttack.bLayeredAnimation?0:Drive.MotionWeight;
        if (BodyAttack.bReleased && Now>BodyAttack.StrikeAt+BodyAttack.Duration)
            Drive.LegMotionWeight*=1-EaseBody((Now-BodyAttack.StrikeAt-BodyAttack.Duration)/0.4);
        Drive.MotionAdaptationDegrees=Source.AdaptationDegrees;
        const float Length=MotionRig.Motions[BodyAttack.MotionSourceIndex].Length;
        // Native family keeps the source clock exactly; adaptations preserve its acceleration profile.
        const float Elapsed=FMath::Max(0.0f,float(Now-BodyAttack.PreparedAt));
        Drive.SupportMotionTime=FMath::Min(Source.FinishTime,BodyAttack.SourceEntryTime+Elapsed/Length);
        Drive.MotionTime=Phase==EDirectionalCombatPhase::Aiming?
            (Elapsed<=BodyAttack.Preparation?FMath::Lerp(BodyAttack.SourceEntryTime,Source.LoadTime,
                Elapsed/FMath::Max(0.001f,BodyAttack.Preparation)):
                FMath::Min(Source.ContactTime-0.001f,Source.LoadTime+(Elapsed-BodyAttack.Preparation)*0.2f/Length)):
            FMath::Clamp(BodyAttack.SourceStrikeTime+float(Now-BodyAttack.StrikeAt)/Length,BodyAttack.SourceStrikeTime,Source.FinishTime);
        Drive.StartPoseBlend=BodyAttack.bChained?1-Preparation:0;
        const float TargetDeviation=FVector::Distance(Intent.AttackTarget,BodyAttack.NeutralTargetWorld)/
            FMath::Max(1.0,Source.NeutralTarget.Size());
        Drive.AdaptationWeight=FMath::Clamp(TargetDeviation,0.0f,1.0f);
        if (BodyAttack.bChained) Drive.AdaptationWeight=FMath::Max(Drive.AdaptationWeight,1-Preparation*0.5f);
        if (CombatBodySolver::SampleMotion(MotionRig,BodyAttack.MotionSourceIndex,Drive.MotionTime,
            Drive.AuthoredLocalPose,Drive.AuthoredComponentPose))
        {
            TArray<FTransform> SupportLocal,SupportComponent;
            if (!BodyAttack.bLayeredAnimation && CombatBodySolver::SampleMotion(MotionRig,BodyAttack.MotionSourceIndex,Drive.SupportMotionTime,SupportLocal,SupportComponent))
                for (int32 Bone=0; Bone<Drive.AuthoredLocalPose.Num(); ++Bone)
                {
                    bool bSupport=Bone==MotionRig.Pelvis;
                    for (int32 Ancestor=Bone; Ancestor!=INDEX_NONE; Ancestor=MotionRig.Parents[Ancestor])
                        bSupport|=Ancestor==MotionRig.Hips[0] || Ancestor==MotionRig.Hips[1];
                    if (bSupport) Drive.AuthoredLocalPose[Bone]=SupportLocal[Bone];
                    const int32 Parent=MotionRig.Parents[Bone];
                    Drive.AuthoredComponentPose[Bone]=Drive.AuthoredLocalPose[Bone]*(Parent==INDEX_NONE?FTransform::Identity:Drive.AuthoredComponentPose[Parent]);
                }
            const auto Weapon=GetEquippedWeaponGeometry();
            const auto Working=[&](const TArray<FTransform>& Pose)
            {
                const FTransform Grip=MotionRig.GripLocal*Pose[MotionRig.GripBone]*Mesh->GetComponentTransform();
                const FTransform WeaponWorld=Weapon.PrimaryGripLocal.Inverse()*Grip;
                return WeaponWorld.TransformPosition(FMath::Lerp(Weapon.BladeBaseLocal,Weapon.BladeTipLocal,bPierce?1.0:0.75));
            };
            const float H=FMath::Min(1.0f/120.0f,Length*0.01f);
            TArray<FTransform> PreviousLocal,Previous,NextLocal,Next;
            if (Drive.MotionTime*Length>=H && Drive.MotionTime*Length<=Length-H &&
                CombatBodySolver::SampleMotion(MotionRig,BodyAttack.MotionSourceIndex,Drive.MotionTime-H/Length,PreviousLocal,Previous) &&
                CombatBodySolver::SampleMotion(MotionRig,BodyAttack.MotionSourceIndex,Drive.MotionTime+H/Length,NextLocal,Next))
            {
                const FVector P=Working(Previous), C=Working(Drive.AuthoredComponentPose), N=Working(Next);
                const float PhysicalH=H*BodyAttack.Duration/(Length*(Source.FinishTime-Source.LoadTime));
                Drive.AuthoredWorkingVelocity=(N-P)/(2*PhysicalH);
                Drive.AuthoredWorkingAcceleration=(N-2*C+P)/(PhysicalH*PhysicalH);
            }
        }
        else { Drive.MotionSourceIndex=INDEX_NONE; Drive.MotionWeight=0; Drive.AdaptationWeight=1; }
        // BlendLibraryGuidance owns entry-pose transport for every source transition.
    }
    const float Follow=FMath::Lerp(FMath::DegreesToRadians(12.0f),Load*0.85f,Obliqueness);
    const float Arc=FMath::Lerp(-Load,Follow,Progress)+MotionContinuation;
    // Counter-load is on the opposite side of the forward chest frame; the strike
    // crosses it toward the requested sector (up intent must not become a depth-only cut).
    Drive.Direction=bPierce?Strike:FQuat(Axis,Arc).RotateVector(Forward).GetSafeNormal();
    if (BodyAttack.bChained)
    {
        const FQuat Turn=FQuat::FindBetweenNormals(BodyAttack.StartArmDirection,Drive.Direction);
        Drive.Direction=FQuat::Slerp(FQuat::Identity,Turn,Progress).RotateVector(BodyAttack.StartArmDirection).GetSafeNormal();
    }
    // Full baseline reach at every click. Charge increases loading/core effort,
    // never gates whether a useful attack is allowed to happen.
    const auto Weapon=GetEquippedWeaponGeometry();
    float DesiredExtension=MotionCoreStyle.Anatomy.MaximumArmExtension;
    const FTransform* Shoulder=MotionFinalPose.BonesWorld.Find(TEXT("upperarm_r"));
    const FTransform* Elbow=MotionFinalPose.BonesWorld.Find(TEXT("lowerarm_r"));
    const FTransform* Hand=MotionFinalPose.BonesWorld.Find(TEXT("hand_r"));
    if (Shoulder && Elbow && Hand)
    {
        const float Length=FVector::Distance(Shoulder->GetLocation(),Elbow->GetLocation())+
            FVector::Distance(Elbow->GetLocation(),Hand->GetLocation());
        DesiredExtension=FMath::Clamp(float(FVector::Distance(Intent.AttackTarget,Shoulder->GetLocation())
            -Weapon.WeaponReach*(bPierce?1.0:0.75))/FMath::Max(1.0f,Length),
            MotionCoreStyle.Anatomy.MinimumArmExtension,MotionCoreStyle.Anatomy.MaximumArmExtension);
    }
    Drive.Extension=FMath::Lerp((bPierce?0.32f:0.42f)-Charge*0.1f,DesiredExtension,EaseBody(Progress/0.7f));
    if (BodyAttack.bChained) Drive.Extension=FMath::Lerp(BodyAttack.StartExtension,DesiredExtension,EaseBody(Progress));
    const FVector LocalTangent=GetOwner()->GetActorQuat().UnrotateVector(Tangent);
    const float Core=FMath::Lerp(-1.0f,1.0f,Progress)*(1+Charge*0.6f);
    Drive.CoreRotation=FRotator((bPierce?1:LocalTangent.Z)*Core*18,LocalTangent.Y*Core*(bHeavy?55:40),0);
    Drive.CoreRotation*=1-Drive.MotionWeight;
    Drive.WeaponLoad=Weapon.WeaponWeight;
    if (!BodyAttack.bLayeredAnimation && Drive.MotionSourceIndex!=INDEX_NONE && Drive.AuthoredLocalPose.Num()==MotionRig.Names.Num())
    {
        const auto& Frames=MotionRig.Motions[Drive.MotionSourceIndex].Frames;
        const float P=Drive.SupportMotionTime*(Frames.Num()-1);
        const int32 I=FMath::Clamp(FMath::FloorToInt(P),0,Frames.Num()-1);
        const float EntryPosition=BodyAttack.SourceEntryTime*(Frames.Num()-1);
        const int32 EntryIndex=FMath::Clamp(FMath::FloorToInt(EntryPosition),0,Frames.Num()-1);
        const FVector EntryRoot=FMath::Lerp(Frames[EntryIndex].RootTranslation,
            Frames[FMath::Min(EntryIndex+1,Frames.Num()-1)].RootTranslation,EntryPosition-EntryIndex);
        FVector Expected=Mesh->GetComponentTransform().TransformVector(FMath::Lerp(Frames[I].RootTranslation,
            Frames[FMath::Min(I+1,Frames.Num()-1)].RootTranslation,P-I)-EntryRoot);
        Expected=FVector::VectorPlaneProject(Expected,FVector::UpVector)+BodyAttack.ApproachDisplacement*EaseBody(T);
        if (Expected.Size()>2)
        {
            const float Actual=FVector::DotProduct(GetOwner()->GetActorLocation()-BodyAttack.StartOrigin,Expected.GetSafeNormal());
            Drive.MovementFulfillment=FMath::Clamp(float(Actual/Expected.Size()),0.0f,1.0f);
            Drive.AdaptationWeight=FMath::Max(Drive.AdaptationWeight,1-Drive.MovementFulfillment);
            // Blocked translation reduces the source's pelvis/leg excursion instead of
            // asking the arms or feet to make up movement which never happened.
            TArray<FTransform> InitialLocal,InitialComponent;
            if (Drive.MovementFulfillment<1 && CombatBodySolver::SampleMotion(MotionRig,Drive.MotionSourceIndex,BodyAttack.SourceEntryTime,InitialLocal,InitialComponent))
                for (int32 Bone=0; Bone<Drive.AuthoredLocalPose.Num(); ++Bone)
                {
                    const bool bSupport=Bone==MotionRig.Pelvis || Bone==MotionRig.Hips[0] || Bone==MotionRig.Hips[1]
                        || Bone==MotionRig.Knees[0] || Bone==MotionRig.Knees[1] || Bone==MotionRig.Feet[0] || Bone==MotionRig.Feet[1];
                    if (bSupport) Drive.AuthoredLocalPose[Bone].Blend(InitialLocal[Bone],Drive.AuthoredLocalPose[Bone],Drive.MovementFulfillment);
                    const int32 Parent=MotionRig.Parents[Bone];
                    Drive.AuthoredComponentPose[Bone]=Drive.AuthoredLocalPose[Bone]*(Parent==INDEX_NONE?FTransform::Identity:Drive.AuthoredComponentPose[Parent]);
                }
        }
    }
    PlanDirectionalAttack(Intent,Drive);
    return Drive;
}

void UDirectionalCombatComponent::CaptureAttackStart()
{
    MotionSolverState.AttackStartRotations.SetNum(MotionRig.Names.Num());
    for (int32 I=0; I<MotionRig.Names.Num(); ++I)
    {
        const FTransform* Bone=MotionPlannedPose.BonesWorld.Find(MotionRig.Names[I]);
        const int32 Parent=MotionRig.Parents[I];
        const FTransform* ParentPose=Parent!=INDEX_NONE?MotionPlannedPose.BonesWorld.Find(MotionRig.Names[Parent]):nullptr;
        if (I==MotionRig.Pelvis && Bone && ParentPose)
            MotionSolverState.AttackStartPelvisLocal=Bone->GetRelativeTransform(*ParentPose).GetLocation();
        MotionSolverState.AttackStartRotations[I]=Bone?
            (ParentPose?Bone->GetRelativeTransform(*ParentPose).GetRotation():
                (Mesh?Bone->GetRelativeTransform(Mesh->GetComponentTransform()).GetRotation():FQuat::Identity)):FQuat::Identity;
    }
}
