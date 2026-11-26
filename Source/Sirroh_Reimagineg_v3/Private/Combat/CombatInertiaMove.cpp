#include "CombatInertiaMove.h"
#include "Combat/DirectionalCombatComponent.h"
#include "CombatDragonWeaponAdapter.h"
#include "Combat/CombatTrainingNPC.h"
#include "MoverComponent.h"
#include "MoverSimulationTypes.h"
#include "NativeGameplayTags.h"
#include "Sword/ProceduralSword.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_CombatInertia, "Combat.Motion.WeaponInertia");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_CombatHit, "Combat.Motion.HitReaction");

FCombatInertiaMove::FCombatInertiaMove()
{
    MixMode=EMoveMixMode::OverrideVelocity;
    DurationMs=300;
}

bool FCombatInertiaMove::HasGameplayTag(FGameplayTag Tag, bool Exact) const
{
    const FGameplayTag Own=bHitReaction?TAG_CombatHit.GetTag():TAG_CombatInertia.GetTag();
    return Exact?Tag==Own:Own.MatchesTag(Tag);
}

bool FCombatInertiaMove::GenerateMove(const FMoverTickStartData& Start, const FMoverTimeStep& Time,
    const UMoverComponent*, UMoverBlackboard* Blackboard, FProposedMove& Out)
{
    return GenerateMove_Async(Start,Time,Blackboard,Out);
}

bool FCombatInertiaMove::GenerateMove_Async(const FMoverTickStartData& Start, const FMoverTimeStep& Time,
    UMoverBlackboard*, FProposedMove& Out)
{
    const auto* Input=Start.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
    // Authoritative source displacement remains active while locomotion input is held.
    (void)Input;
    if (Start.SyncState.MovementMode!=GroundMode || Time.StepMs<=0 || DurationMs<=0 || TravelMs<=0)
    { DurationMs=0; return false; }
    const auto Integral=[](double X) { X=FMath::Clamp(X,0.0,1.0); return X*X*(3-2*X); };
    const double A=(Time.BaseSimTimeMs-StartSimTimeMs)/TravelMs;
    const double B=(Time.BaseSimTimeMs+Time.StepMs-StartSimTimeMs)/TravelMs;
    if (A>=1) { DurationMs=0; return false; }
    // Integrate displacement over the actual simulation step, including the final partial step.
    Out.MixMode=EMoveMixMode::OverrideVelocity;
    const auto* Sync=Start.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
    if (Input && Sync && !Input->OrientationIntent.IsNearlyZero())
    {
        const double Yaw=FMath::FindDeltaAngleDegrees(Sync->GetOrientation_WorldSpace().Yaw,Input->OrientationIntent.Rotation().Yaw);
        Out.AngularVelocityDegrees.Z=FMath::Clamp(Yaw/(Time.StepMs*0.001),-240.0,240.0);
    }
    if (bReactionTurn && Sync)
    {
        const double TargetYaw=ReactionStartYaw+ReactionTurnYaw*FMath::Clamp(B,0.0,1.0);
        Out.AngularVelocityDegrees.Z=FMath::FindDeltaAngleDegrees(Sync->GetOrientation_WorldSpace().Yaw,TargetYaw)/(Time.StepMs*0.001);
    }
    if (MotionPath.Num()>1)
    {
        const auto Position=[&](double T)
        {
            const double P=FMath::Clamp(T,0.0,1.0)*(MotionPath.Num()-1);
            const int32 I=FMath::FloorToInt(P);
            return FMath::Lerp(MotionPath[I],MotionPath[FMath::Min(I+1,MotionPath.Num()-1)],P-I);
        };
        Out.LinearVelocity=(Position(B)-Position(A))/(Time.StepMs*0.001);
    }
    else Out.LinearVelocity=Displacement*((Integral(B)-Integral(A))/(Time.StepMs*0.001));
    if (bLocalPath && Sync) Out.LinearVelocity=Sync->GetOrientation_WorldSpace().RotateVector(Out.LinearVelocity);
    Out.LinearVelocity.Z=0; // Ground pull never lowers the capsule. Mover resolves slopes/collision.
    return true;
}

void FCombatInertiaMove::NetSerialize(FArchive& Ar)
{
    Super::NetSerialize(Ar);
    Ar << Displacement;
    Ar << bReactionTurn << ReactionStartYaw << ReactionTurnYaw;
    Ar << MotionPath;
    Ar << GroundMode;
    Ar << TravelMs;
    Ar << bHitReaction;
    Ar << bLocalPath;
}

void UDirectionalCombatComponent::CancelInertiaMovement()
{
    if (auto* Mover=GetOwner()->FindComponentByClass<UMoverComponent>())
        Mover->CancelFeaturesWithTag(TAG_CombatInertia,true);
    if (auto* NPC=Cast<ACombatTrainingNPC>(GetOwner())) NPC->CancelCombatApproach();
}

void UDirectionalCombatComponent::UpdateWeaponInertia(float Delta, const FVector& RootVelocity)
{
    const float Dt=FMath::Clamp(Delta,0.0f,0.1f);
    MotionInertiaReaction*=FMath::Exp(-5*Dt);
    if (MotionWeaponAdapter)
    {
        FVector LoadVelocity,LoadAngular,LoadCentre;
        if (MotionWeaponAdapter->ReadMotion(LoadVelocity,LoadAngular,LoadCentre))
        {
            const FVector Relative=LoadVelocity-RootVelocity;
            const FVector Acceleration=bHasLoadVelocity && Dt>0.001f?
                ((Relative-PreviousLoadVelocity)/Dt).GetClampedToMaxSize(4000):FVector::ZeroVector;
            PreviousLoadVelocity=Relative; bHasLoadVelocity=true;
            const float Mass=FMath::Clamp(GetEquippedWeaponGeometry().WeaponWeight,0.25f,20.0f);
            const FVector Force=(FVector(0,0,GetWorld()->GetGravityZ())-Acceleration)*Mass;
            const FVector Lever=(LoadCentre-MotionFinalPose.PrimaryGripWorld.GetLocation()).GetClampedToMaxSize(150);
            const FVector Torque=GetOwner()->GetActorQuat().UnrotateVector(FVector::CrossProduct(Lever,Force));
            const FVector BodyLoad=GetOwner()->GetActorQuat().RotateVector(FVector(-Torque.Y,Torque.X,0)/250000.0);
            const FVector Demand=(BodyLoad-Acceleration*Mass/18000.0).GetClampedToMaxSize(0.7);
            MotionInertiaReaction=FMath::VInterpTo(MotionInertiaReaction,Demand,Dt,6);
        }
        else bHasLoadVelocity=false;
    }
    const bool bSource=BodyAttack.bPrepared && !BodyAttack.bLayeredAnimation && MotionRig.Motions.IsValidIndex(BodyAttack.MotionSourceIndex)
        && MotionRig.Motions[BodyAttack.MotionSourceIndex].IsValid();
    bool bAuthoredTravel=false;
    if (bSource)
    {
        const auto& Frames=MotionRig.Motions[BodyAttack.MotionSourceIndex].Frames;
        for (const auto& Frame:Frames)
            if (FVector::DistSquared2D(Frame.RootTranslation,Frames[0].RootTranslation)>1)
            { bAuthoredTravel=true; break; }
    }
    const auto RootPath=[&](float Begin,float End,const FVector& Extra)
    {
        TArray<FVector> Path;
        const auto& Frames=MotionRig.Motions[BodyAttack.MotionSourceIndex].Frames;
        const auto Sample=[&](float T)
        {
            const float P=FMath::Clamp(T,0.0f,1.0f)*(Frames.Num()-1);
            const int32 I=FMath::FloorToInt(P);
            return FMath::Lerp(Frames[I].RootTranslation,Frames[FMath::Min(I+1,Frames.Num()-1)].RootTranslation,P-I);
        };
        const FVector Start=Sample(Begin);
        for (int32 I=0; I<=60; ++I)
        {
            const float T=I/60.0f;
            FVector Point=Mesh->GetComponentTransform().TransformVector(Sample(FMath::Lerp(Begin,End,T))-Start);
            Point=FVector::VectorPlaneProject(Point,FVector::UpVector)+Extra*(T*T*(3-2*T));
            Path.Add(GetOwner()->GetActorQuat().UnrotateVector(Point.GetClampedToMaxSize(500)));
        }
        return Path;
    };
    if (bSource && GetOwner()->HasAuthority() && MotionPreparedRootId!=BodyAttack.Id)
    {
        MotionPreparedRootId=BodyAttack.Id;
        auto* Mover=GetOwner()->FindComponentByClass<UMoverComponent>();
        if (Mover && Mover->HasGameplayTag(Mover_IsOnGround,false))
        {
            auto Move=MakeShared<FCombatInertiaMove>();
            Move->GroundMode=Mover->GetMovementModeName();
            Move->bLocalPath=true;
            Move->MotionPath=RootPath(BodyAttack.SourceEntryTime,MotionRig.Sources[BodyAttack.MotionSourceIndex].FinishTime,FVector::ZeroVector);
            if (RootVelocity.Size2D()>350)
            {
                // Integrate the admitted sprint momentum once, with a short brake.
                // It remains part of the same collision-resolved Mover path.
                const FVector Momentum=GetOwner()->GetActorQuat().UnrotateVector(FVector::VectorPlaneProject(RootVelocity,FVector::UpVector)).GetClampedToMaxSize(800);
                const double Duration=MotionRig.Motions[BodyAttack.MotionSourceIndex].Length*(MotionRig.Sources[BodyAttack.MotionSourceIndex].FinishTime-BodyAttack.SourceEntryTime);
                for (int32 I=0; I<Move->MotionPath.Num(); ++I)
                    Move->MotionPath[I]+=Momentum*(0.15*(1-FMath::Exp(-Duration*I/(Move->MotionPath.Num()-1)/0.15)));
            }
            Move->TravelMs=MotionRig.Motions[BodyAttack.MotionSourceIndex].Length*(MotionRig.Sources[BodyAttack.MotionSourceIndex].FinishTime-BodyAttack.SourceEntryTime)*1000;
            Move->DurationMs=Move->TravelMs+100;
            bool bTravel=false;
            for (const FVector& Point:Move->MotionPath) bTravel|=!Point.IsNearlyZero();
            if (bTravel) Mover->QueueLayeredMove(Move);
        }
    }
    if (!BodyAttack.bPrepared || !BodyAttack.bReleased || BodyAttack.WeaponId!=EquippedSword.InstanceId)
    {
        if (bMotionPullIssued) CancelInertiaMovement();
        bMotionPullIssued=false;
        MotionContinuation=0;
        MotionSettledSeconds=0;
        return;
    }
    if (MotionInertiaAttackId!=BodyAttack.Id)
    {
        CancelInertiaMovement();
        MotionInertiaAttackId=BodyAttack.Id;
        MotionContinuation=0;
        MotionSettledSeconds=0;
        bMotionPullIssued=false;
    }
    // Approach is a body-planner request. A stiff physical grip need not stretch first.
    const double MovementTime=CombatTime();
    if (GetOwner()->HasAuthority() && !bMotionPullIssued && !bAuthoredTravel && MovementTime>=BodyAttack.StrikeAt)
    {
        bMotionPullIssued=true; // consume once, including airborne/blocked/disabled requests
        auto* Mover=GetOwner()->FindComponentByClass<UMoverComponent>();
        if (Mover && Mover->HasGameplayTag(Mover_IsOnGround,false)
            && !BodyAttack.ApproachDisplacement.IsNearlyZero())
        {
            auto Move=MakeShared<FCombatInertiaMove>();
            Move->GroundMode=Mover->GetMovementModeName();
            Move->Displacement=BodyAttack.ApproachDisplacement;
            Move->TravelMs=FMath::Max(250.0f,BodyAttack.ApproachSeconds*1000);
            Move->DurationMs=Move->TravelMs+100; // explicit zero-speed tail before removal
            bool bTravel=!Move->Displacement.IsNearlyZero();
            for (const FVector& Point:Move->MotionPath) bTravel|=!Point.IsNearlyZero();
            if (bTravel) Mover->QueueLayeredMove(Move);
        }
        else if (auto* NPC=Cast<ACombatTrainingNPC>(GetOwner()))
            NPC->QueueCombatDisplacement(BodyAttack.ApproachDisplacement,BodyAttack.ApproachSeconds,false);
    }
    // Completion must not depend on a live physics read (sleeping/missing proxy).
    if (GetOwner()->HasAuthority() && BodyAttack.ReturnAt<0 && CombatTime()>=BodyAttack.StrikeAt+BodyAttack.Duration+0.35)
    { BodyAttack.ReturnAt=CombatTime(); GetOwner()->ForceNetUpdate(); }
    if (!MotionWeaponAdapter) return;
    FVector Velocity, Angular, Centre;
    if (!MotionWeaponAdapter->ReadMotion(Velocity,Angular,Centre)) return;
    FVector RootAngular=FVector::ZeroVector;
    if (bMotionCoreValid && Delta>UE_SMALL_NUMBER)
    {
        FQuat Q=(GetOwner()->GetActorQuat()*MotionFinalPose.ActualRootWorld.GetRotation().Inverse()).GetNormalized();
        if (Q.W<0) Q=Q*-1.0;
        FVector Axis; double Angle; Q.ToAxisAndAngle(Axis,Angle);
        RootAngular=Axis*(Angle/Delta);
    }
    const FVector RelativeVelocity=Velocity-RootVelocity-FVector::CrossProduct(RootAngular,Centre-GetOwner()->GetActorLocation());
    const FVector RelativeAngular=Angular-RootAngular;
    const double Now=CombatTime();
    if (Now<BodyAttack.StrikeAt || BodyAttack.ReturnAt>=0) return;
    if (Now>=BodyAttack.StrikeAt+BodyAttack.Duration)
    {
        const double ForwardSpin=FMath::Max(0.0,FVector::DotProduct(RelativeAngular,MotionSwingAxis));
        MotionContinuation=FMath::Clamp(MotionContinuation+float(ForwardSpin)*Dt,0.0f,FMath::DegreesToRadians(80.0f));
        // A sustained low relative speed is needed; a single turning point is not completion.
        const bool bSettled=RelativeAngular.Size()<0.6 && RelativeVelocity.Size()<35;
        MotionSettledSeconds=bSettled?MotionSettledSeconds+Dt:0;
        if (GetOwner()->HasAuthority() && (MotionSettledSeconds>=0.15f || Now>=BodyAttack.StrikeAt+BodyAttack.Duration+0.35))
        {
            BodyAttack.ReturnAt=Now;
            GetOwner()->ForceNetUpdate();
        }
    }
}

void UDirectionalCombatComponent::QueueMotionKnockback(const FVector& Displacement)
{
    if (!GetOwner()->HasAuthority()) return;
    CancelInertiaMovement();
    if (auto* Mover=GetOwner()->FindComponentByClass<UMoverComponent>())
    {
        Mover->CancelFeaturesWithTag(TAG_CombatHit,true);
        if (!Mover->HasGameplayTag(Mover_IsOnGround,false)) return;
        auto Move=MakeShared<FCombatInertiaMove>();
        Move->bHitReaction=true; Move->MixMode=EMoveMixMode::OverrideVelocity; Move->GroundMode=Mover->GetMovementModeName();
        Move->Displacement=Displacement; Move->Displacement.Z=0;
        Move->TravelMs=FMath::Clamp(float(Displacement.Size2D()/300)*1000,400.0f,750.0f);
        Move->DurationMs=Move->TravelMs+100;
        Mover->QueueLayeredMove(Move);
    }
    else if (auto* NPC=Cast<ACombatTrainingNPC>(GetOwner()))
        NPC->QueueCombatDisplacement(Displacement,FMath::Clamp(float(Displacement.Size2D()/300),0.4f,0.75f),true);
}
