#include "Combat/DirectionalCombatComponent.h"
#include "Combat/CombatReactionCatalog.h"

#include "Character/SirrohPoseBridgeComponent.h"
#include "Combat/CombatContactSubsystem.h"
#include "Combat/CombatGripAdapter.h"
#include "CombatDragonWeaponAdapter.h"
#include "CombatDragonBodyAdapter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "TwoBoneIK.h"
#include "Placeholder/PlaceholderHandsComponent.h"
#include "Sword/ProceduralSword.h"

void UDirectionalCombatComponent::DebugRefreshSolvers()
{
    if (!bDebugBodySolver) ResetMotionCore();
    if (!bDebugBodyPhysics && MotionBodyAdapter) MotionBodyAdapter->Stop();
    if (!bDebugWeaponPhysics && MotionWeaponAdapter) MotionWeaponAdapter->Stop();
    if (!bDebugFeetSolver) { bMotionFeetReady=false; bMotionHoldingSupport=false; }
}

void UDirectionalCombatComponent::ResetMotionCore()
{
    BufferedBodyAttack=FBufferedBodyAttack();
    LibraryLoopSource=GuidanceSource=INDEX_NONE; LibraryLastTime=LibraryLoopTime=0;
    GuidanceOffsets.Reset(); GuidanceBlendAt=-1; SmoothedLookDirection=FVector::ZeroVector; LookWeight=0;
    if (GetOwner()->HasAuthority()) LibraryActionSource=INDEX_NONE;
    ReturnBlendAt=-1; PreviousPresentedPhase=ECombatMotionPhase::Inactive;
    bHasLoadVelocity=false; PreviousLoadVelocity=FVector::ZeroVector;
    bMotionCoreValid=false;
    CancelInertiaMovement();
    MotionContinuation=0;
    if (MotionBodyAdapter) MotionBodyAdapter->Stop();
    bMotionFeetReady=false; bMotionHoldingSupport=false;
    MotionSwingFoot=INDEX_NONE;
    MotionSnapshotAttackId=0;
    MotionSettledSeconds=0;
    MotionInertiaAttackId=0;
    MotionPreparedRootId=0;
    bMotionPullIssued=false;
    MotionInertiaReaction=FVector::ZeroVector;
    if (MotionWeaponAdapter) MotionWeaponAdapter->Stop();
    MotionContactReaction=FVector::ZeroVector;
    MotionSubmittedAttackId=0;
    bMotionSubmittedAttackActive=false;
    if (GetOwner()->HasAuthority()) BodyAttack.bPrepared=false;
    bMotionFailureLogged=false;
    MotionCoreFailure.Reset();
    MotionSolverState=FCombatBodySolverState();
    MotionFinalPose=FCombatFinalPose();
    MotionPlannedPose=FCombatFinalPose();
    MotionWeaponSample=FCombatWeaponSample();
    MotionAttackQuality=FCombatAttackQuality();
    ++MotionEpoch;
}

void UDirectionalCombatComponent::FailMotionCore(const FString& Reason)
{
    if (bMotionCoreValid) ResetMotionCore();
    else if (MotionWeaponAdapter) MotionWeaponAdapter->Stop();
    if (!bMotionFailureLogged || MotionCoreFailure!=Reason)
        UE_LOG(LogTemp,Error,TEXT("Motion Core V3 on %s: %s"),*GetOwner()->GetName(),*Reason);
    MotionCoreFailure=Reason;
    bMotionFailureLogged=true;
    bMotionCoreValid=false;
    if (PoseBridge) PoseBridge->ClearCombatPose();
    // Inventory may have yielded using last frame's successful sample. Reattach to
    // its current grip immediately on failure; never leave the sword in world space.
    if (AProceduralSword* Sword=ActiveSword(); Sword && Mesh)
    {
        FTransform Hand=Mesh->GetSocketTransform(TEXT("hand_r"));
        if (PoseBridge) PoseBridge->TryGetFinalBoneTransform(TEXT("hand_r"),Hand);
        const FName Socket=Sword->HandSocket(0);
        const FTransform Grip=Mesh->DoesSocketExist(Socket)?
            Mesh->GetSocketTransform(Socket).GetRelativeTransform(Mesh->GetSocketTransform(TEXT("hand_r"))):FTransform::Identity;
        Sword->SetHeldTransform(Grip*Hand);
    }
    if (Hands) Hands->UpdateBackItems();
}

void UDirectionalCombatComponent::OnRep_MotionCoreConfiguration()
{
    OtherGripRig=FCombatBodyRig(); bOtherGripReady=false;
    if (GripMode.Serial>0) MotionCoreStyle.bTwoHanded=GripMode.bTwoHanded;
    ResetMotionCore();
    bMotionRigAttempted=false;
    bMotionWarmupAttempted=false;
    bMotionRigReady=false;
    bPoseReady=false;
    if (bUseMotionCoreV3)
    {
        bBlocking=false;
        AttackMotion=FDirectionalAttackMotion();
        if (GetWorld()) GetWorld()->GetSubsystem<UCombatContactSubsystem>()->UnregisterCombatant(this);
    }
    if (PoseBridge) PoseBridge->ClearCombatPose();
}

void UDirectionalCombatComponent::PrepareMotionRig()
{
    if(!Mesh || !Mesh->GetSkeletalMeshAsset() || bMotionWarmupAttempted)return;
    bMotionWarmupAttempted=true;
    const double Started=FPlatformTime::Seconds();
    MotionRigMesh=Mesh->GetSkeletalMeshAsset();
    if (!bMotionRigAttempted)
    {
        bMotionRigAttempted=true;
        MotionRigRelativeTransform=Mesh->GetRelativeTransform();
        FString Error;
        bMotionRigReady=CombatBodySolver::BindRig(*Mesh,MotionCoreStyle,MotionRig,Error);
        if (!bMotionRigReady) MotionCoreFailure=Error;

    }
    if(bMotionRigReady) { WarmOtherGrip(); ReactionRigs.Reset(); for (const auto& Entry:CombatReactions::Catalog(&MotionCoreStyle)) GetReactionRig(Entry.Id); }
    UE_LOG(LogTemp,Log,TEXT("Combat rig preparation %s: %.1f ms (ready=%d)"),*GetNameSafe(GetOwner()),(FPlatformTime::Seconds()-Started)*1000,bMotionRigReady);
}

void UDirectionalCombatComponent::TickMotionCore(float Delta)
{
    if (!bDebugBodySolver) { if (bMotionCoreValid) ResetMotionCore(); if (PoseBridge) PoseBridge->ClearCombatPose(); return; }
    ApplyMoveScale(bCombatActive?FMath::Clamp(CombatMoveSpeed*CombatSpeedMultiplier,0.1f,2.0f):1.0f);
    const bool SingleGrip=ActiveSword() && !ActiveSword()->GetCombatDefinition().bHasSecondaryGrip;
    if(SingleGrip && MotionCoreStyle.bTwoHanded && GetOwner()->HasAuthority())
    {PendingGripMode=0;UpdateGripMode();}
    // Imported props provide their own relaxed overlay; combat takes over only in combat mode.
    if(SingleGrip && !bCombatActive)Alpha=0;
    const bool Carry=!SingleGrip && !MotionCoreStyle.MotionLibrary.IsNull() && EquippedSword.InstanceId.IsValid();
    Alpha=FMath::FInterpConstantTo(Alpha,(bCombatActive || Carry) && !Reaction.bDisarmed?1.0f:0.0f,
        Delta,1.0f/FMath::Max(0.01f,BlendSeconds));
    UpdateRemoteSword();
    if (PoseBridge) PoseBridge->ClearCombatPose();
    // With no equipped weapon, stop its solver immediately; blend alpha is not ownership.
    if ((!bCombatActive && !Carry) || Reaction.bDisarmed)
    {
        if (bMotionCoreValid) ResetMotionCore();
        if (Hands) Hands->UpdateBackItems();
        if (Mesh && PoseBridge && bMotionRigReady)
        {
            FCombatFinalPose HitPose;
            for (const FName Bone:MotionRig.Names) HitPose.BonesWorld.Add(Bone,Mesh->GetSocketTransform(Bone));
            if (BlendHitAnimation(HitPose,CombatTime())) PoseBridge->PublishCombatPose(HitPose.BonesWorld);
        }
        return;
    }
    if (!Mesh || !PoseBridge || !Mesh->GetSkeletalMeshAsset())
    {
        FailMotionCore(TEXT("Missing source mesh, skeletal asset or final-pose bridge."));
        return;
    }
    if (MotionRigMesh!=Mesh->GetSkeletalMeshAsset()
        || (bMotionRigAttempted && !MotionRigRelativeTransform.GetRotation().Equals(Mesh->GetRelativeTransform().GetRotation())))
    {
        MotionRigMesh=Mesh->GetSkeletalMeshAsset();
        OtherGripRig=FCombatBodyRig(); bOtherGripReady=false;
        bMotionRigAttempted=false;
        bMotionWarmupAttempted=false;
        bMotionRigReady=false;
        ResetMotionCore();
    }
    PrepareMotionRig();
    if (!bMotionRigReady)
    {
        const FString Reason=MotionCoreFailure;
        FailMotionCore(Reason);
        return;
    }
    if (LibraryActionSource!=INDEX_NONE && MotionRig.Catalog && !LibraryActionVariantId.IsNone())
        LibraryActionSource=MotionRig.Sources.IndexOfByPredicate([&](const auto& Source)
        { return Source.CatalogVariantId==LibraryActionVariantId; });
    if (BodyAttack.bPrepared && MotionRig.Catalog && !BodyAttack.MotionVariantId.IsNone())
    {
        // Replication carries a stable catalog ID; local array order is not a network contract.
        BodyAttack.MotionSourceIndex=MotionRig.Sources.IndexOfByPredicate([&](const auto& Source)
        { return Source.CatalogVariantId==BodyAttack.MotionVariantId; });
    }
    WarmOtherGrip();

    const FVector Root=GetOwner()->GetActorLocation();
    if (MotionWeaponId!=EquippedSword.InstanceId || !FMath::IsNearlyEqual(MotionPixelScale,Rig.PixelScale)
        || (bMotionCoreValid && (FVector::DistSquared(Root,MotionPreviousRoot)>FMath::Square(150.0)
            || GetOwner()->GetActorQuat().AngularDistance(MotionFinalPose.ActualRootWorld.GetRotation())>FMath::DegreesToRadians(120.0)
            || !GetOwner()->GetActorScale3D().Equals(MotionFinalPose.ActualRootWorld.GetScale3D()))))
    {
        ResetMotionCore();
        MotionWeaponId=EquippedSword.InstanceId;
        MotionPixelScale=Rig.PixelScale;
    }
    const FVector RootVelocity=bMotionCoreValid && Delta>UE_SMALL_NUMBER?(Root-MotionPreviousRoot)/Delta:FVector::ZeroVector;
    MotionPreviousRoot=Root;
    FCombatIntent Intent;
    Intent.AttackTarget=Root+FVector(AimOffset);
    // AimOffset is a world-axis offset; use pelvis as the sector origin rather than the feet.
    const FVector Pelvis=Mesh->GetComponentTransform().TransformPosition(
        Mesh->GetComponentSpaceTransforms().IsValidIndex(MotionRig.Pelvis)
            ?Mesh->GetComponentSpaceTransforms()[MotionRig.Pelvis].GetLocation():FVector::ZeroVector);
    const FVector GuardRay=Intent.AttackTarget-(UsesIndicatorDefense()?GetDefenseFrame().Centre:Pelvis);
    Intent.GuardDirectionWorld=GuardRay.SizeSquared()>FMath::Square(45.0)?GuardRay.GetSafeNormal():
        (bMotionCoreValid?MotionPlan.Intent.GuardDirectionWorld:GetOwner()->GetActorForwardVector());
    FVector GuardLocal=GetOwner()->GetActorQuat().UnrotateVector(Intent.GuardDirectionWorld);
    GuardLocal.X=FMath::Max(0.05,GuardLocal.X);
    GuardLocal.Y=FMath::Clamp(GuardLocal.Y,-0.6,0.6);
    GuardLocal.Z=FMath::Clamp(GuardLocal.Z,-1.0,1.0);
    Intent.GuardDirectionWorld=GetOwner()->GetActorQuat().RotateVector(GuardLocal.GetSafeNormal());
    Intent.StyleId=MotionCoreStyle.StyleId;
    Intent.WeaponInstanceId=EquippedSword.InstanceId;
    Intent.Phase=ECombatMotionPhase::Combat;
    Intent.SimulationTime=CombatTime();

    const FCombatWeaponDefinition Weapon=GetEquippedWeaponGeometry();
    if (MotionCoreStyle.bTwoHanded && !Weapon.bHasSecondaryGrip)
    {
        FailMotionCore(TEXT("Two-hand mode requires valid generated grip geometry for the equipped weapon."));
        return;
    }
    FCombatPhysicalContact Contact;
    if (MotionWeaponAdapter && MotionWeaponAdapter->ConsumeContact(Contact))
    {
        if (Contact.bWeaponClash) ReceiveMotionWeaponHit(Contact);
        if (Reaction.bDisarmed) return;
        // Contact from the completed physics step feeds the NEXT body target, never a hand IK target.
        // Ground support impulse alone must not veto an approach. A clash or an
        // impact opposing the committed ground direction can stop it; Mover handles capsule collision.
        if (Contact.bWeaponClash || FVector::DotProduct(Contact.Impulse,BodyAttack.ApproachDisplacement.GetSafeNormal())<-100)
            CancelInertiaMovement();
        MotionContactReaction=(MotionContactReaction+Contact.Impulse/2000.0).GetClampedToMaxSize(1.0);
        if (Intent.SimulationTime-MotionContactPublishTime>=0.08)
        {
            Contact.Serial=PhysicalContact.Serial+1;
            PhysicalContact=Contact;
            MotionContactPublishTime=Intent.SimulationTime;
            OnPhysicalWeaponContact.Broadcast(PhysicalContact);
            GetOwner()->ForceNetUpdate();
        }
    }
    ProcessBufferedBodyAttack();
    if (BodyAttack.bPrepared && MotionSnapshotAttackId!=BodyAttack.Id)
    {
        CaptureAttackStart();
        MotionSnapshotAttackId=BodyAttack.Id;
    }
    const auto& Debug=MotionCoreStyle.DebugStages;
    if (Debug.Inertia>0) UpdateWeaponInertia(Delta,RootVelocity); else { CancelInertiaMovement(); MotionInertiaReaction=FVector::ZeroVector; }
    if (bLibraryWasGuard!=bCombatActive)
    {
        bLibraryWasGuard=bCombatActive;
        if (GetOwner()->HasAuthority() && !BodyAttack.bPrepared) ServerLibraryAction_Implementation(bCombatActive?TEXT("Equip"):TEXT("Holster"),GetOwner()->GetActorForwardVector());
    }
    FCombatBodyDrive Drive=EvaluateBodyAttack(Intent);
    if (Debug.GuardLibrary>0) EvaluateLibraryGuard(Intent,Drive);
    if (Debug.ActionLibrary>0) EvaluateLibraryAction(Intent,Drive);
    // Carry the actual upper-body pose across abrupt cancellations and replicated phase changes.
    const bool bWasAttack=PreviousPresentedPhase==ECombatMotionPhase::Aiming || PreviousPresentedPhase==ECombatMotionPhase::Attacking || PreviousPresentedPhase==ECombatMotionPhase::Staggered;
    if (bWasAttack && (Intent.Phase==ECombatMotionPhase::Combat || Intent.Phase==ECombatMotionPhase::Recovery))
    { CaptureAttackStart(); ReturnBlendAt=Intent.SimulationTime; }
    if (Intent.Phase==ECombatMotionPhase::Aiming || Intent.Phase==ECombatMotionPhase::Attacking) ReturnBlendAt=-1;
    if (ReturnBlendAt>=0)
    {
        const float T=FMath::Clamp(float((Intent.SimulationTime-ReturnBlendAt)/0.3),0.0f,1.0f);
        Drive.StartPoseBlend=FMath::Max(Drive.StartPoseBlend,1-T*T*(3-2*T));
    }
    OnRep_MotionReaction();
    if (MotionReaction.Serial>0 && MotionReaction.bInterruptsAttack && Intent.SimulationTime<MotionReaction.GuardAt+0.2)
    {
        Drive.StartPoseBlend=1-FMath::Clamp(float((Intent.SimulationTime-MotionReaction.GuardAt)/0.2),0.0f,1.0f);
        if (Intent.SimulationTime<MotionReaction.GuardAt) Intent.Phase=ECombatMotionPhase::Staggered;
        Drive.FollowThrough=FMath::Max(Drive.FollowThrough,Drive.StartPoseBlend*0.9f);
    }
    PreviousPresentedPhase=Intent.Phase;
    if (Debug.Guidance>0) BlendLibraryGuidance(Drive,Intent);
    const float AttackStage=Intent.Phase==ECombatMotionPhase::Aiming?Debug.WindUp:(Intent.AttackType==EDirectionalAttack::Light?Debug.Light:(Intent.AttackType==EDirectionalAttack::Heavy?Debug.Heavy:Debug.Thrust));
    Drive.Weight*=AttackStage; Drive.WorkingPointWeight*=AttackStage;
    Drive.StartPoseBlend*=Debug.EntryPoseBlend;
    LowerBodyAlpha=FMath::FInterpTo(LowerBodyAlpha,Drive.LegMotionWeight,Delta,8);
    Drive.LegMotionWeight=LowerBodyAlpha;
    const float AnimationWeight=bDebugAnimationGuidance?DebugAnimationWeight:0.f;
    Drive.MotionWeight*=AnimationWeight; Drive.LegMotionWeight*=AnimationWeight;
    PlanCombatFeet(Delta,RootVelocity,Drive);
    if (Drive.bUseFootTargets && DebugFeetWeight<1)
    {
        for (int32 Side=0;Side<2;++Side)
        {
            const int32 Foot=MotionRig.Feet[Side];
            if (MotionRig.Names.IsValidIndex(Foot))
            {
                const FTransform Live=Mesh->GetBoneTransform(Foot);
                FTransform Blended; Blended.Blend(Live,Drive.FootTargets[Side],DebugFeetWeight);
                Drive.FootTargets[Side]=Blended;
            }
        }
        Drive.PelvisOffset*=DebugFeetWeight;
        if (DebugFeetWeight<=0) {Drive.bUseFootTargets=false;Drive.bRecoverStandingPosture=false;}
    }
    MotionSwingAxis=Drive.SwingAxis;
    Drive.WeaponLoad=1+(Weapon.WeaponWeight-1)*FMath::Clamp(MotionCoreStyle.WeaponWeightParticipation,0.0f,2.0f);
    Drive.ReactionWorld=(MotionContactReaction+MotionInertiaReaction).GetClampedToMaxSize(1.0);
    MotionContactReaction*=FMath::Exp(-8*FMath::Max(Delta,0.0f));
    FCombatFinalPose Solved;
    FCombatMotionPlan Plan;
    FCombatBodySolverDiagnostics Diagnostics;
    FCombatBodySolverState NextState=MotionSolverState;
    FCombatStyleDefinition PoseStyle=MotionCoreStyle;
    PoseStyle.bTwoHanded=MotionCoreStyle.bTwoHanded && bCombatActive && !IsLibraryActionPlaying();
    bool bSolved=CombatBodySolver::SolveMotion(MotionRig,Mesh->GetComponentSpaceTransforms(),
        Mesh->GetComponentTransform(),GetOwner()->GetActorTransform(),PoseStyle,Weapon,Intent,Drive,Alpha*DebugBodyWeight,Delta,
        NextState,Plan,Solved,Diagnostics);
    if (!bSolved && MotionCoreStyle.bTwoHanded && !MotionFinalPose.bHasSecondaryGrip)
    {
        // Entering two-hand mode has no previous paired pose to project from. Establish
        // a body-led neutral seed once, instead of alternating inventory/combat ownership.
        FCombatIntent SeedIntent=Intent;
        SeedIntent.GuardDirectionWorld=GetOwner()->GetActorForwardVector();
        FCombatBodyDrive SeedDrive;
        SeedDrive.Direction=SeedIntent.GuardDirectionWorld;
        SeedDrive.WeaponLoad=Drive.WeaponLoad;
        SeedDrive.bUseFootTargets=Drive.bUseFootTargets;
        SeedDrive.FootTargets[0]=Drive.FootTargets[0]; SeedDrive.FootTargets[1]=Drive.FootTargets[1];
        FCombatBodySolverState SeedState=MotionSolverState;
        bSolved=CombatBodySolver::SolveMotion(MotionRig,Mesh->GetComponentSpaceTransforms(),
            Mesh->GetComponentTransform(),GetOwner()->GetActorTransform(),PoseStyle,Weapon,
            SeedIntent,SeedDrive,Alpha*DebugBodyWeight,Delta,SeedState,Plan,Solved,Diagnostics);
        if (bSolved) { NextState=MoveTemp(SeedState); Plan.Intent=Intent; Diagnostics.bPoseLimited=true; }
    }
    const bool bRequestedSolved=bSolved;
    if (Debug.ContinuityFallback>0 && !bSolved && bMotionCoreValid)
    {
        Solved=FCombatFinalPose();
        Solved.SimulationTime=Intent.SimulationTime;
        bSolved=CombatBodySolver::ProjectContinuousPose(MotionRig,MotionCoreStyle,Weapon,
            MotionPlannedPose,GetOwner()->GetActorTransform(),Solved,Diagnostics);
        if (bSolved)
        {
            if (!bRequestedSolved) NextState=MotionSolverState; // Accepted spring evolution must not freeze behind the continuity projection.
            Plan=MotionPlan; Plan.Intent=Intent;
            Solved.SimulationTime=Intent.SimulationTime;
        }
    }
    bool bReactionAnimation=false;
    if (bSolved)
    {
        MotionPlannedPose=Solved;
        bReactionAnimation=Debug.HitAnimation>0 && BlendHitAnimation(Solved,Intent.SimulationTime);
    }
    if (bDebugBodyPhysics && !CombatBodySolver::UsesPhysicsAssetBiomechanics() && bSolved && MotionReaction.Serial>0 && Intent.SimulationTime<MotionReaction.Start+0.9 && (!MotionReaction.bWeapon || MotionBodyAdapter))
    {
        if (!MotionBodyAdapter)
        {
            MotionBodyAdapter=NewObject<UCombatDragonBodyAdapter>(GetOwner());
            GetOwner()->AddInstanceComponent(MotionBodyAdapter); MotionBodyAdapter->RegisterComponent();
        }
        if (MotionBodyAdapter->Initialize(*Mesh,Solved))
        {
            MotionBodyAdapter->PrepareTargets(Solved);
            if (MotionPhysicsReaction!=MotionReaction.Serial)
            {
                MotionPhysicsReaction=MotionReaction.Serial;
                if (!MotionReaction.bWeapon && Intent.SimulationTime<MotionReaction.Start+0.3)
                    MotionBodyAdapter->Hit(MotionReaction.Bone,MotionReaction.Point,MotionReaction.Impulse,
                        MotionReaction.Strength,MotionReaction.Start);
            }
            const FCombatFinalPose BeforeReaction=Solved;
            MotionBodyAdapter->Solve(MotionRig,Solved,Intent.SimulationTime);
            const FCombatFinalPose FullReaction=Solved;
            bSolved=bReactionAnimation || CombatBodySolver::CloseGripPair(MotionRig,MotionCoreStyle,Weapon,Solved,Diagnostics);
            // A contact cannot turn a valid two-hand plan into a Motion Core reset.
            // Project the physical offset onto the feasible grip/body envelope. The
            // Chaos impulse remains active; never stretch arms or move the weapon here.
            for (float ResponseFraction=0.5f; !bSolved && ResponseFraction>=0.125f; ResponseFraction*=0.5f)
            {
                Solved=BeforeReaction;
                for (int32 Bone=0; Bone<MotionRig.Names.Num(); ++Bone)
                {
                    const int32 Parent=MotionRig.Parents[Bone];
                    if (Parent==INDEX_NONE) continue;
                    const FName Name=MotionRig.Names[Bone],ParentName=MotionRig.Names[Parent];
                    FTransform Local=BeforeReaction.BonesWorld.FindChecked(Name).GetRelativeTransform(
                        BeforeReaction.BonesWorld.FindChecked(ParentName));
                    const FTransform PhysicalLocal=FullReaction.BonesWorld.FindChecked(Name).GetRelativeTransform(
                        FullReaction.BonesWorld.FindChecked(ParentName));
                    Local.SetRotation(FQuat::Slerp(Local.GetRotation(),PhysicalLocal.GetRotation(),ResponseFraction).GetNormalized());
                    const double Length=Local.GetLocation().Size();
                    const FVector Blended=FMath::Lerp(Local.GetLocation(),PhysicalLocal.GetLocation(),ResponseFraction);
                    Local.SetLocation(Blended.GetSafeNormal(UE_SMALL_NUMBER,Local.GetLocation().GetSafeNormal())*Length);
                    Solved.BonesWorld[Name]=Local*Solved.BonesWorld.FindChecked(ParentName);
                }
                bSolved=bReactionAnimation || CombatBodySolver::CloseGripPair(MotionRig,MotionCoreStyle,Weapon,Solved,Diagnostics);
            }
            if (!bSolved) { Solved=BeforeReaction; bSolved=true; }
        }
    }
    if (!CombatBodySolver::UsesPhysicsAssetBiomechanics() && MotionBodyAdapter && Intent.SimulationTime>=MotionReaction.Start+0.9) MotionBodyAdapter->Stop();
    if (bSolved && !bReactionAnimation && CombatBodySolver::UsesPhysicsAssetBiomechanics())
        ApplyCombatLook(Solved,Intent,Drive,Delta); // A target before physics, never a competing post-physics bone writer.
    if (bDebugBodyPhysics && bSolved && CombatBodySolver::UsesPhysicsAssetBiomechanics())
    {
        if (!MotionBodyAdapter)
        {
            MotionBodyAdapter=NewObject<UCombatDragonBodyAdapter>(GetOwner());
            GetOwner()->AddInstanceComponent(MotionBodyAdapter); MotionBodyAdapter->RegisterComponent();
        }
        bSolved=MotionBodyAdapter->Initialize(*Mesh,Solved);
        if (bSolved)
        {
            if (!bReactionAnimation && Debug.AnatomyPass>0) bSolved=CombatBodySolver::ConstrainAnatomy(MotionRig,Solved);
            MotionBodyAdapter->PrepareTargets(Solved);
            if (bReactionAnimation) Solved.bHasSecondaryGrip=false;
            bSolved=bSolved && MotionBodyAdapter->DrivePhysicsAsset(MotionRig,Solved,Intent.SimulationTime);
            if (bSolved && !bReactionAnimation && PoseStyle.bTwoHanded)
                bSolved=CombatBodySolver::CloseGripPair(MotionRig,PoseStyle,Weapon,Solved,Diagnostics);
            if (MotionReaction.Serial>0 && MotionPhysicsReaction!=MotionReaction.Serial)
            {
                MotionPhysicsReaction=MotionReaction.Serial;
                if (!MotionReaction.bWeapon && Intent.SimulationTime<MotionReaction.Start+0.3)
                    MotionBodyAdapter->Hit(MotionReaction.Bone,MotionReaction.Point,MotionReaction.Impulse,MotionReaction.Strength,MotionReaction.Start);
            }
        }
    }
    FTransform WeaponWorld;
    const bool bGripClosed=bSolved && PoseStyle.bTwoHanded && !bReactionAnimation && (Debug.GripTransition<=0 || BlendGripTransition(Solved,Intent.SimulationTime));
    if (bSolved && !bReactionAnimation && !CombatBodySolver::UsesPhysicsAssetBiomechanics()) ApplyCombatLook(Solved,Intent,Drive,Delta);
    if (bSolved && !CombatBodySolver::UsesPhysicsAssetBiomechanics() && !bReactionAnimation && Debug.AnatomyPass>0) bSolved=CombatBodySolver::ConstrainAnatomy(MotionRig,Solved);
    // Do not rate-reject the whole skeleton here. One fast joint could reject
    // every candidate and hold the previous frame indefinitely, including legs.
    // Source transitions blend upstream; the final anatomy pass clamps locally.
    FCombatWeaponSample Sample;
    FString Failure;
    if (!bSolved) Failure=FString::Printf(TEXT("Body solve rejected pose: live bones=%d, rig bones=%d, mesh=%s."),
        Mesh->GetComponentSpaceTransforms().Num(),MotionRig.Names.Num(),*GetNameSafe(Mesh->GetSkeletalMeshAsset()));
    else if (!Weapon.WeaponInstanceId.IsValid()) Failure=TEXT("Equipped weapon has no valid instance ID.");
    // Body-derived target goes to one Dragon physical handle. Contact samples use its actual result.
    bool bValid=bSolved && Weapon.WeaponInstanceId.IsValid()
        && CombatGripAdapter::TryBuildWeaponTarget(Solved,Weapon,MotionCoreStyle.bTwoHanded && bGripClosed && Solved.bHasSecondaryGrip,WeaponWorld);
    if (!bValid && Failure.IsEmpty()) Failure=FString::Printf(
        TEXT("Invalid weapon grip: primary=%d finite=%d normalized=%d unitScale=%d paired=%d secondary=%d."),
        int32(Solved.bHasPrimaryGrip),int32(!Solved.PrimaryGripWorld.ContainsNaN()),
        int32(Solved.PrimaryGripWorld.GetRotation().IsNormalized()),
        int32(Solved.PrimaryGripWorld.GetScale3D().Equals(FVector::OneVector,KINDA_SMALL_NUMBER)),
        int32(MotionCoreStyle.bTwoHanded && bGripClosed),int32(Solved.bHasSecondaryGrip));
    if (bValid && (!bCombatActive || !bDebugWeaponPhysics))
    {
        // Blending out a held pose must not leave an invisible live weapon after throw/unequip.
        if (MotionWeaponAdapter) MotionWeaponAdapter->Stop();
        if (AProceduralSword* Sword=ActiveSword())
            bValid=Sword->SetActorTransform(WeaponWorld,false,nullptr,ETeleportType::TeleportPhysics);
        bValid=bValid && CombatGripAdapter::TryMeasureWeapon(WeaponWorld,Weapon,Intent.SimulationTime,
            ++MotionStep,MotionEpoch,0,false,false,nullptr,Sample);
    }
    else if (bValid)
    {
        if (!MotionWeaponAdapter)
        {
            MotionWeaponAdapter=NewObject<UCombatDragonWeaponAdapter>(GetOwner());
            GetOwner()->AddInstanceComponent(MotionWeaponAdapter);
            MotionWeaponAdapter->RegisterComponent();
        }
        FTransform ActualWeapon;
        const bool bNewAdapter=!MotionWeaponAdapter->ReadWeapon(ActualWeapon);
        if (bNewAdapter)
        {
            bValid=MotionWeaponAdapter->Start(*Mesh,Weapon,WeaponWorld);
            bValid=bValid && MotionWeaponAdapter->ReadWeapon(ActualWeapon);
        }
        // PostPhysics result belongs to the grip submitted last frame, including its attack identity.
        if (bValid)
        {
            bValid=CombatGripAdapter::TryMeasureWeapon(ActualWeapon,Weapon,Intent.SimulationTime,++MotionStep,MotionEpoch,
                MotionSubmittedAttackId,!bNewAdapter && bMotionSubmittedAttackActive,true,
                bMotionCoreValid && !bNewAdapter?&MotionWeaponSample:nullptr,Sample);
            if (AProceduralSword* Sword=ActiveSword())
                bValid= bValid && Sword->SetActorTransform(ActualWeapon,false,nullptr,ETeleportType::TeleportPhysics);
            MotionWeaponAdapter->DriveGrip(Solved.PrimaryGripWorld,Intent.AttackId,
                Drive.FollowThrough*(1-Drive.RecoveryAlpha),MotionCoreStyle.FollowThroughDamping);
            Sample.AttackType=MotionSubmittedAttackType;
            MotionSubmittedAttackType=Intent.AttackType;
            MotionSubmittedAttackId=Intent.AttackId;
            Sample.StrikePhase=MotionSubmittedStrikePhase;
            MotionSubmittedStrikePhase=0;
            bMotionSubmittedAttackActive=Intent.Phase==ECombatMotionPhase::Attacking;
            if (bMotionSubmittedAttackActive && MotionRig.Sources.IsValidIndex(Drive.MotionSourceIndex))
            {
                const auto& Windows=MotionRig.Sources[Drive.MotionSourceIndex].StrikeWindows;
                if (!Windows.IsEmpty())
                {
                    bMotionSubmittedAttackActive=false;
                    for (int32 I=0; I<Windows.Num(); ++I)
                    {
                        if (Drive.MotionTime>=Windows[I].X) MotionSubmittedStrikePhase=I;
                        if (Drive.MotionTime>=Windows[I].X && Drive.MotionTime<=Windows[I].Y)
                            bMotionSubmittedAttackActive=true;
                    }
                }
            }
        }
    }
    if (!bValid)
    {
        FailMotionCore(Failure.IsEmpty()?TEXT("Weapon application or final sample failed."):Failure);
        return;
    }
    Solved.SimulationStep=MotionStep;
    Solved.Epoch=MotionEpoch;
    MotionSolverState=NextState;
    Plan.PrimaryGripGoal=Solved.PrimaryGripWorld.GetRelativeTransform(Solved.ActualRootWorld);
    if (Solved.bHasSecondaryGrip) Plan.SecondaryGripGoal=Solved.SecondaryGripWorld.GetRelativeTransform(Solved.ActualRootWorld);
    Plan.Movement.AttackId=BodyAttack.Id;
    Plan.Movement.StartTime=BodyAttack.StrikeAt;
    Plan.Movement.GroundDisplacement=BodyAttack.ApproachDisplacement;
    Plan.Movement.MaximumDistance=BodyAttack.ApproachDisplacement.Size();
    Plan.Movement.DurationSeconds=BodyAttack.ApproachSeconds;
    MotionPlan=MoveTemp(Plan);
    MotionFinalPose=MoveTemp(Solved);
    MotionDiagnostics=Diagnostics;
    MotionWeaponSample=Sample;
    MotionAttackQuality=FCombatAttackQuality();
    if (Sample.AttackId==BodyAttack.Id && Sample.bAttackActive && !Sample.bDiscontinuity)
    {
        const bool bThrust=BodyAttack.Type==EDirectionalAttack::Pierce;
        const FVector Working=bThrust?Sample.BladeTip:FMath::Lerp(Sample.BladeBase,Sample.BladeTip,0.75);
        const FVector Closest=bThrust?Sample.BladeTip:FMath::ClosestPointOnSegment(BodyAttack.Target,
            FMath::Lerp(Sample.BladeBase,Sample.BladeTip,0.55),Sample.BladeTip);
        MotionAttackQuality.bValid=true;
        MotionAttackQuality.bApproximateGeometry=Weapon.bApproximateGeometry;
        MotionAttackQuality.AttackId=static_cast<int32>(Sample.AttackId);
        MotionAttackQuality.WorkingRegionTargetErrorCm=FVector::Distance(Closest,BodyAttack.Target);
        MotionAttackQuality.WorkingPointPathErrorCm=Drive.WorkingPointWeight>0?
            FVector::Distance(Working,Drive.WorkingPointGoal):0;
        MotionAttackQuality.WorkingPointSpeedCmPerSecond=(Sample.LinearVelocity+
            FVector::CrossProduct(Sample.AngularVelocity,Working-Sample.WeaponWorld.GetLocation())).Size();
        MotionAttackQuality.MovementFulfillment=Drive.MovementFulfillment;
    }
    bMotionCoreValid=true;
    bMotionFailureLogged=false;
    MotionCoreFailure.Reset();
    // This exact map supplies rendered bones AND grip/socket queries, also on dedicated server.
    PoseBridge->PublishCombatPose(MotionFinalPose.BonesWorld);
    if (!MotionCoreStyle.bTwoHanded && IsLocal() && Hands && IsValid(Hands->GetItem(1)))
    {
        FTransform Left;
        if (PoseBridge->TryGetFinalBoneTransform(TEXT("hand_l"),Left))
        {
            const FName Socket=MotionCoreStyle.SecondaryGripSocket.IsNone()?Hands->GetItem(1)->HandSocket(1):MotionCoreStyle.SecondaryGripSocket;
            const FTransform Local=Mesh->DoesSocketExist(Socket)?
                Mesh->GetSocketTransform(Socket).GetRelativeTransform(Mesh->GetSocketTransform(TEXT("hand_l"))):FTransform(FQuat::Identity,Hands->HandItemOffset);
            Hands->GetItem(1)->SetHeldTransform(Local*Left);
        }
    }
    if (Hands) Hands->UpdateBackItems();
}

void UDirectionalCombatComponent::OnRep_PhysicalContact()
{
    if (!bUseMotionCoreV3 || PhysicalContact.WeaponId!=EquippedSword.InstanceId) return;
    MotionContactReaction=(MotionContactReaction+PhysicalContact.Impulse/2000.0).GetClampedToMaxSize(1.0);
    OnPhysicalWeaponContact.Broadcast(PhysicalContact);
}

