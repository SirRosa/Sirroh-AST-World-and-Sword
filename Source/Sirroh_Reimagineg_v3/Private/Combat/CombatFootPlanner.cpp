#include "Combat/DirectionalCombatComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "MoverComponent.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "Combat/CombatTrainingNPC.h"
#include "Sword/ProceduralSword.h"

void UDirectionalCombatComponent::PlanCombatFeet(float Delta, const FVector& RootVelocity, FCombatBodyDrive& Drive)
{
    if (!bDebugFeetSolver) { Drive.bUseFootTargets=false; Drive.bRecoverStandingPosture=false; bMotionFeetReady=false; bMotionHoldingSupport=false; return; }
    auto* Mover=GetOwner()->FindComponentByClass<UMoverComponent>();
    const bool bGrounded=Mover?Mover->HasGameplayTag(Mover_IsOnGround,false):Cast<ACombatTrainingNPC>(GetOwner())!=nullptr;
    const auto* CharacterMover=Cast<UCharacterMoverComponent>(Mover);
    Drive.bRecoverStandingPosture=bGrounded && (!CharacterMover || !CharacterMover->IsCrouching());
    if (!bGrounded || !bCombatActive)
    { Drive.bRecoverStandingPosture=false; bMotionFeetReady=false; MotionSwingFoot=INDEX_NONE; return; }
    if ((BodyAttack.bPrepared && BodyAttack.bLayeredAnimation && Drive.MotionWeight>0)
        || (IsLibraryActionPlaying() && Drive.LegMotionWeight<0.05f && RootVelocity.Size2D()>8))
    { Drive.bUseFootTargets=false; Drive.bRecoverStandingPosture=false; bMotionFeetReady=false; bMotionHoldingSupport=false; return; }
    const bool bHitRecovery=MotionReaction.Serial>0 && !MotionReaction.bWeapon && CombatTime()<MotionReaction.Start+0.9;
    const bool bFinished=(!IsLibraryActionPlaying() && GetCombatPhase()!=EDirectionalCombatPhase::Combat && BodyAttack.bReleased && CombatTime()>=BodyAttack.StrikeAt+BodyAttack.Duration) ||
        (Drive.MotionSourceIndex!=INDEX_NONE && Drive.SupportMotionTime>=MotionRig.Sources[Drive.MotionSourceIndex].FinishTime);
    const bool bGuard=GetCombatPhase()==EDirectionalCombatPhase::Combat && !IsLibraryActionPlaying();
    if (!bHitRecovery && Drive.LegMotionWeight<=0 && (bGuard || bFinished) && (!bMotionFeetReady || RootVelocity.Size2D()>8))
    { Drive.bRecoverStandingPosture=false; bMotionFeetReady=false; bMotionHoldingSupport=false; MotionSwingFoot=INDEX_NONE; return; }
    const bool bFirstSupport=!bMotionFeetReady;
    const auto* Capsule=GetOwner()->FindComponentByClass<UCapsuleComponent>();
    const double SupportZ=Capsule?Capsule->GetComponentLocation().Z-Capsule->GetScaledCapsuleHalfHeight():
        GetOwner()->GetActorLocation().Z-90;
    const FVector Up=FVector::UpVector;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(CombatFootGround),false,GetOwner());
    if (ActiveSword()) Params.AddIgnoredActor(ActiveSword());
    FCollisionObjectQueryParams Objects;
    Objects.AddObjectTypesToQuery(ECC_WorldStatic);
    Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
    Objects.AddObjectTypesToQuery(ECC_GameTraceChannel5);
    const auto Ground=[&](const FTransform& Foot, float Sole, FTransform& Out)
    {
        FHitResult Hit;
        FVector Start=Foot.GetLocation(), End=Start;
        Start.Z=FMath::Max(Start.Z+40,SupportZ+60); End.Z=SupportZ-85;
        FCollisionQueryParams TerrainParams=Params;
        bool bTerrain=false;
        for (int32 Attempt=0; Attempt<16; ++Attempt)
        {
            if (!GetWorld()->LineTraceSingleByObjectType(Hit,Start,End,Objects,TerrainParams)) break;
            UPrimitiveComponent* Surface=Hit.GetComponent();
            if (!Surface) break;
            if (Surface->ComponentHasTag(TEXT("CombatTerrain"))) { bTerrain=true; break; }
            TerrainParams.AddIgnoredComponent(Surface); // vegetation/ledges/other proxies cannot support a foot
        }
        if (!bTerrain || FVector::DotProduct(Hit.ImpactNormal,Up)<0.65) return false;
        Out=Foot;
        Out.SetLocation(Hit.ImpactPoint+Up*Sole);
        Out.SetRotation((FQuat::FindBetweenNormals(Up,Hit.ImpactNormal)*Foot.GetRotation()).GetNormalized());
        return true;
    };
    if (!bHitRecovery && bMotionFeetReady && (bGuard || bFinished) && RootVelocity.Size2D()<=8
        && MotionSupportFacing.AngularDistance(GetOwner()->GetActorQuat())<FMath::DegreesToRadians(35.0))
    {
        // Keep the landing stance. Recovery changes the upper body, not planted foot positions.
        if (!bMotionHoldingSupport)
            for (int32 Side=0; Side<2; ++Side)
                if (const FTransform* Foot=MotionFinalPose.BonesWorld.Find(MotionRig.Names[MotionRig.Feet[Side]]))
                    MotionFeet[Side]=*Foot;
        bMotionHoldingSupport=true;
        Drive.bUseFootTargets=true; Drive.bRecoverStandingPosture=false;
        for (int32 Side=0; Side<2; ++Side)
        {
            FTransform Floor;
            if (Ground(MotionFeet[Side],2,Floor))
                MotionFeet[Side].SetLocation(FVector(MotionFeet[Side].GetLocation().X,MotionFeet[Side].GetLocation().Y,
                    FMath::Max(MotionFeet[Side].GetLocation().Z,Floor.GetLocation().Z)));
            Drive.FootTargets[Side]=MotionFeet[Side];
        }
        return;
    }
    if (!bFinished && Drive.LegMotionWeight>0 && Drive.AuthoredComponentPose.Num()==MotionRig.Names.Num() && !bHitRecovery)
    {
        // The clip owns the entire step, including planted intervals, lift, travel and
        // landing. Terrain only adds the difference from the capsule support plane.
        bMotionHoldingSupport=false; MotionSupportFacing=GetOwner()->GetActorQuat();
        Drive.bRecoverStandingPosture=false;
        Drive.bUseFootTargets=true;
        MotionSwingFoot=INDEX_NONE;
        double Penetration=0;
        for (int32 Side=0; Side<2; ++Side)
        {
            FTransform Authored=Drive.AuthoredComponentPose[MotionRig.Feet[Side]]*Mesh->GetComponentTransform();
            const FTransform Live=Mesh->GetComponentSpaceTransforms()[MotionRig.Feet[Side]]*Mesh->GetComponentTransform();
            Authored.Blend(Live,Authored,Drive.LegMotionWeight);
            FTransform Floor;
            Drive.FootTargets[Side]=Authored;
            if (Ground(Authored,0,Floor))
            {
                const double HeightCorrection=FMath::Clamp(Floor.GetLocation().Z-SupportZ,-35.0,35.0);
                Drive.FootTargets[Side].AddToTranslation(Up*HeightCorrection);
                Penetration=FMath::Max(Penetration,Floor.GetLocation().Z+2-Drive.FootTargets[Side].GetLocation().Z);
                // Preserve foot roll/toe-off; add only terrain tilt.
                Drive.FootTargets[Side].SetRotation(Floor.GetRotation());
            }
            FQuat DeltaRotation=(Drive.FootTargets[Side].GetRotation()*Live.GetRotation().Inverse()).GetNormalized();
            if (DeltaRotation.W<0) DeltaRotation=DeltaRotation*-1;
            const double RotationAngle=DeltaRotation.GetAngle();
            Drive.FootTargets[Side].SetRotation((FQuat::Slerp(FQuat::Identity,DeltaRotation,
                FMath::Min(1.0,FMath::DegreesToRadians(55.0)/FMath::Max(RotationAngle,UE_SMALL_NUMBER)))*Live.GetRotation()).GetNormalized());
            MotionFeet[Side]=Drive.FootTargets[Side];
        }
        bMotionFeetReady=true;
        // Lift the whole authored body together; do not fold the knees to hide penetration.
        const FVector Lift=Up*FMath::Clamp(Penetration,0.0,35.0);
        Drive.PelvisOffset=Lift;
        for (int32 Side=0; Side<2; ++Side)
        { Drive.FootTargets[Side].AddToTranslation(Lift); MotionFeet[Side]=Drive.FootTargets[Side]; }
        return;
    }
    FTransform Desired[2];
    for (int32 Side=0; Side<2; ++Side)
    {
        const FTransform Live=Mesh->GetComponentSpaceTransforms()[MotionRig.Feet[Side]]*Mesh->GetComponentTransform();
        if (!bMotionFeetReady)
        {
            FTransform Floor;
            if (!Ground(Live,0,Floor)) { bMotionFeetReady=false; MotionSwingFoot=INDEX_NONE; return; }
            MotionFootSole[Side]=FMath::Clamp(float(Live.GetLocation().Z-Floor.GetLocation().Z),2.0f,12.0f);
        }
        if (!Ground(Live,MotionFootSole[Side],Desired[Side]))
        { bMotionFeetReady=false; MotionSwingFoot=INDEX_NONE; return; }
    }
    if (!bMotionFeetReady)
    {
        MotionFeet[0]=Desired[0]; MotionFeet[1]=Desired[1];
        if (bHitRecovery && bMotionCoreValid)
            for (int32 Side=0; Side<2; ++Side)
                if (const auto* Previous=MotionFinalPose.BonesWorld.Find(MotionRig.Names[MotionRig.Feet[Side]]))
                {
                    FTransform Planted;
                    if (Ground(*Previous,MotionFootSole[Side],Planted)) MotionFeet[Side]=Planted;
                }
        MotionSwingFoot=INDEX_NONE; bMotionFeetReady=true;
    }
    FVector GroundVelocity=FVector::VectorPlaneProject(RootVelocity,Up);
    if (bHitRecovery && bFirstSupport && GroundVelocity.SizeSquared()<100)
        GroundVelocity=FVector::VectorPlaneProject(MotionReaction.Impulse,Up).GetSafeNormal()*120;
    // Only actual, collision-resolved root movement advances the step, never requested distance.
    if (MotionSwingFoot==INDEX_NONE)
    {
        const double Right=FVector::Dist2D(MotionFeet[0].GetLocation(),Desired[0].GetLocation());
        const double Left=FVector::Dist2D(MotionFeet[1].GetLocation(),Desired[1].GetLocation());
        if (FMath::Max(Right,Left)>(bHitRecovery?2:8) || (bHitRecovery && bFirstSupport))
        {
            int32 Side=Right>=Left?0:1;
            if (!bHitRecovery && Drive.Weight>0 && FMath::Abs(Right-Left)<8)
            {
                if (FMath::Abs(Drive.AuthoredFootLift[0]-Drive.AuthoredFootLift[1])>2)
                    Side=Drive.AuthoredFootLift[0]>Drive.AuthoredFootLift[1]?0:1;
                else if (MotionCoreStyle.PreferredSupportFoot==ECombatSupportFoot::Left) Side=0;
                else if (MotionCoreStyle.PreferredSupportFoot==ECombatSupportFoot::Right) Side=1;
            }
            FTransform Candidate=Desired[Side];
            Candidate.AddToTranslation((GroundVelocity*(bHitRecovery?0.16:0.12)).GetClampedToMaxSize(40));
            FTransform Landing;
            if (Ground(Candidate,MotionFootSole[Side],Landing))
            {
                MotionSwingFoot=Side; MotionFootProgress=0;
                MotionStepStart=MotionFeet[Side]; MotionStepEnd=Landing;
            }
        }
    }
    if (MotionSwingFoot!=INDEX_NONE)
    {
        // Reconcile the anticipated landing with actual movement every frame. If Mover
        // stops at a wall, the foot must not finish the previously predicted full stride.
        FTransform Candidate=Desired[MotionSwingFoot], Landing;
        Candidate.AddToTranslation((GroundVelocity*0.12).GetClampedToMaxSize(40));
        if (Ground(Candidate,MotionFootSole[MotionSwingFoot],Landing))
            MotionStepEnd.Blend(MotionStepEnd,Landing,FMath::Clamp(Delta*18.0f,0.0f,1.0f));
        const float StepSeconds=FMath::Clamp((bHitRecovery?28.0f:35.0f)/FMath::Max(1.0f,float(GroundVelocity.Size())),0.10f,0.22f);
        MotionFootProgress=FMath::Min(1.0f,MotionFootProgress+FMath::Clamp(Delta,0.0f,0.1f)/StepSeconds);
        const float T=MotionFootProgress, Blend=T*T*(3-2*T);
        MotionFeet[MotionSwingFoot].Blend(MotionStepStart,MotionStepEnd,Blend);
        const float Lift=bHitRecovery?14:FMath::Max(8.0f,Drive.AuthoredFootLift[MotionSwingFoot]);
        MotionFeet[MotionSwingFoot].AddToTranslation(Up*(FMath::Sin(PI*T)*Lift));
        if (T>=1) MotionSwingFoot=INDEX_NONE;
    }
    for (int32 Side=0; Side<2; ++Side)
    {
        // Release support before an unreachable planted leg can drag pelvis underground.
        if (Side!=MotionSwingFoot && FVector::Dist2D(MotionFeet[Side].GetLocation(),Desired[Side].GetLocation())>50)
            MotionFeet[Side]=Desired[Side];
        Drive.FootTargets[Side]=MotionFeet[Side];
    }
    if (MotionSwingFoot==INDEX_NONE && FVector::Dist2D(MotionFeet[0].GetLocation(),Desired[0].GetLocation())<8
        && FVector::Dist2D(MotionFeet[1].GetLocation(),Desired[1].GetLocation())<8)
        MotionSupportFacing=GetOwner()->GetActorQuat();
    Drive.bUseFootTargets=true;
    if (MotionSwingFoot!=INDEX_NONE)
    {
        const int32 Support=1-MotionSwingFoot;
        FVector Shift=MotionFeet[Support].GetLocation()-GetOwner()->GetActorLocation();
        Shift.Z=0;
        Drive.PelvisOffset=Shift.GetClampedToMaxSize(bHitRecovery?7:3)+Up*(FMath::Sin(PI*MotionFootProgress)*(bHitRecovery?-3.0:1.5));
    }
    if (!bHitRecovery && Drive.MotionWeight>0)
    {
        const FVector Authored=FVector::VectorPlaneProject(Drive.AuthoredPelvisOffset,Up).GetClampedToMaxSize(6)*Drive.MotionWeight;
        Drive.PelvisOffset+=Authored;
        const FVector Support=FMath::ClosestPointOnSegment(GetOwner()->GetActorLocation()+Drive.PelvisOffset,
            MotionFeet[0].GetLocation(),MotionFeet[1].GetLocation());
        const FVector Error=FVector::VectorPlaneProject(GetOwner()->GetActorLocation()+Drive.PelvisOffset-Support,Up);
        Drive.PelvisOffset-=Error-Error.GetClampedToMaxSize(10);
        Drive.PelvisOffset=Drive.PelvisOffset.GetClampedToMaxSize(10);
    }
}
