#include "Combat/DirectionalCombatComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

void UDirectionalCombatComponent::BlendLibraryGuidance(FCombatBodyDrive& Drive,const FCombatIntent& Intent)
{
    if (Drive.AuthoredLocalPose.Num()!=MotionRig.Names.Num()) return;
    // A phase change within one clip is not a new pose transition. In particular,
    // releasing a charge must not restart blending and suppress the authored swing.
    if (GuidanceSource!=Drive.MotionSourceIndex || GuidanceAttack!=Intent.AttackId || GuidanceAction!=LibraryActionSerial)
    {
        GuidanceSource=Drive.MotionSourceIndex; GuidanceAttack=Intent.AttackId;
        GuidancePhase=Intent.Phase; GuidanceAction=LibraryActionSerial;
        GuidanceBlendAt=Intent.SimulationTime; GuidanceOffsets.Reset();
        GuidanceDuration=0.2; GuidancePelvisOffset=FVector::ZeroVector;
        for (int32 I=0; I<MotionRig.Names.Num(); ++I)
        {
            const int32 P=MotionRig.Parents[I];
            FQuat Current=Drive.AuthoredLocalPose[I].GetRotation();
            if (bMotionCoreValid && P!=INDEX_NONE)
            {
                const auto* Bone=MotionPlannedPose.BonesWorld.Find(MotionRig.Names[I]);
                const auto* Parent=MotionPlannedPose.BonesWorld.Find(MotionRig.Names[P]);
                if (Bone && Parent)
                {
                    const FTransform Local=Bone->GetRelativeTransform(*Parent);
                    Current=Local.GetRotation();
                    if (I==MotionRig.Pelvis) GuidancePelvisOffset=(Local.GetLocation()-Drive.AuthoredLocalPose[I].GetLocation()).GetClampedToMaxSize(40);
                }
            }
            else if (bMotionCoreValid && P==INDEX_NONE)
            {
                const auto* Bone=MotionPlannedPose.BonesWorld.Find(MotionRig.Names[I]);
                if (Bone) Current=Bone->GetRelativeTransform(Mesh->GetRelativeTransform()*MotionPlannedPose.ActualRootWorld).GetRotation();
            }
            GuidanceOffsets.Add((Current*Drive.AuthoredLocalPose[I].GetRotation().Inverse()).GetNormalized());
            const FString Name=MotionRig.Names[I].ToString();
            const double Rate=I==MotionRig.Pelvis?270:Name.StartsWith(TEXT("spine"))?330:Name.StartsWith(TEXT("clavicle"))?510:900;
            GuidanceDuration=FMath::Max(GuidanceDuration,1.5*FMath::RadiansToDegrees(GuidanceOffsets.Last().AngularDistance(FQuat::Identity))/Rate);
        }
    }
    // Do not truncate the duration calculated from joint travel. Large reversals
    // need more braking time than small changes, even when attack input is immediate.
    const double Duration=FMath::Clamp(GuidanceDuration,0.2,1.0);
    const double T=FMath::Clamp((Intent.SimulationTime-GuidanceBlendAt)/Duration,0.,1.);
    const double Weight=1-T*T*(3-2*T);
    if (GuidanceOffsets.Num()!=Drive.AuthoredLocalPose.Num()) return;
    // One transition of the whole source, before feet, body constraints and grip
    // closure. Never smooth the rendered weapon independently of its hands.
    Drive.StartPoseBlend=0;
    for (int32 I=0; I<Drive.AuthoredLocalPose.Num(); ++I)
    {
        auto& Local=Drive.AuthoredLocalPose[I];
        if (I==MotionRig.Pelvis) Local.AddToTranslation(GuidancePelvisOffset*Weight);
        Local.SetRotation((FQuat::Slerp(FQuat::Identity,GuidanceOffsets[I],Weight)*Local.GetRotation()).GetNormalized());
        Drive.AuthoredComponentPose[I]=Local*(MotionRig.Parents[I]==INDEX_NONE?FTransform::Identity:Drive.AuthoredComponentPose[MotionRig.Parents[I]]);
    }
}

void UDirectionalCombatComponent::ApplyCombatLook(FCombatFinalPose& FinalPose,const FCombatIntent& Intent,const FCombatBodyDrive& Drive,float Delta)
{
    if (!bDebugLookSolver) { LookWeight=0; return; }
    const int32 Head=MotionRig.Names.IndexOfByKey(FName(TEXT("head")));
    if (Head==INDEX_NONE || !FinalPose.BonesWorld.Contains(TEXT("head"))) return;
    const FQuat ActorFrame=GetOwner()->GetActorQuat();
    const FVector Ray=Intent.AttackTarget-FinalPose.BonesWorld.FindChecked(TEXT("head")).GetLocation();
    const FVector Local=ActorFrame.UnrotateVector(Ray.GetSafeNormal());
    const bool ActionBlocks=IsLibraryActionPlaying() && MotionRig.Sources.IsValidIndex(LibraryActionSource)
        && (MotionRig.Sources[LibraryActionSource].Action==TEXT("Roll") || MotionRig.Sources[LibraryActionSource].Action==TEXT("Dodge"));
    const bool Turning=Drive.AuthoredLocalPose.Num()>0 && Drive.AuthoredLocalPose[0].GetRotation().AngularDistance(
        MotionRig.ReferenceLocalPose[0].GetRotation())>FMath::DegreesToRadians(60.);
    const float Goal=bCombatActive && Local.X>-0.15 && Ray.SizeSquared()>900 && !ActionBlocks && !Turning?DebugLookWeight:0.f;
    LookWeight=FMath::FInterpTo(LookWeight,Goal,Delta,7);
    const float Yaw=FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(Local.Y,Local.X)),-65.,65.);
    const float Pitch=FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(Local.Z,Local.Size2D())),-35.,35.);
    const FVector Desired=ActorFrame.RotateVector(FRotator(Pitch,Yaw,0).Vector());
    SmoothedLookDirection=SmoothedLookDirection.IsNearlyZero()?Desired:FMath::VInterpTo(SmoothedLookDirection,Desired,Delta,10).GetSafeNormal();
    TArray<FTransform> Ref=MotionRig.ReferenceLocalPose;
    for (int32 I=0; I<Ref.Num(); ++I) if (MotionRig.Parents[I]!=INDEX_NONE) Ref[I]*=Ref[MotionRig.Parents[I]];
    // Calibrate forward from the reference skeleton; head bone X is not assumed to be gaze.
    const FQuat ReferenceWorld=(Ref[Head]*Mesh->GetComponentTransform()).GetRotation();
    const FVector BoneForward=ReferenceWorld.UnrotateVector(ActorFrame.GetAxisX());
    for (int32 Joint=0; Joint<MotionRig.Names.Num(); ++Joint)
    {
        const FString Name=MotionRig.Names[Joint].ToString();
        if (Name!=TEXT("head") && Name!=TEXT("neck_01")) continue;
        const FVector Current=FinalPose.BonesWorld.FindChecked(TEXT("head")).GetRotation().RotateVector(BoneForward);
        FQuat Correction=FQuat::FindBetweenNormals(Current,SmoothedLookDirection);
        const double Angle=Correction.AngularDistance(FQuat::Identity);
        const double Limit=FMath::DegreesToRadians(Name==TEXT("head")?35.:20.);
        Correction=FQuat::Slerp(FQuat::Identity,Correction,LookWeight*FMath::Min(1.,Limit/FMath::Max(Angle,UE_SMALL_NUMBER))).GetNormalized();
        const FVector Pivot=FinalPose.BonesWorld.FindChecked(MotionRig.Names[Joint]).GetLocation();
        for (int32 I=Joint; I<MotionRig.Names.Num(); ++I)
        {
            bool Child=false;
            for (int32 P=I; P!=INDEX_NONE; P=MotionRig.Parents[P]) if (P==Joint) { Child=true; break; }
            if (!Child) continue;
            auto& Bone=FinalPose.BonesWorld.FindChecked(MotionRig.Names[I]);
            Bone.SetLocation(Pivot+Correction.RotateVector(Bone.GetLocation()-Pivot));
            Bone.SetRotation((Correction*Bone.GetRotation()).GetNormalized());
        }
    }
}

