#include "Combat/CombatBodySolver.h"

#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "DragonIK_Library.h"
#include "Combat/AnimNotify_CombatPoseHint.h"
#include "Combat/CombatGripAdapter.h"

namespace
{
    float MarkerTime(const UAnimSequence& Animation, FName Name)
    {
        float Found=-1;
        for (const FAnimNotifyEvent& Event:Animation.Notifies)
        {
            FName EventName=Event.NotifyName;
            if (const auto* Hint=Cast<UAnimNotify_CombatPoseHint>(Event.Notify)) EventName=FName(*Hint->GetNotifyName());
            if (EventName==Name)
            {
                // Ambiguous duplicate markers disable this hint instead of silently choosing one.
                if (Found>=0) return -1;
                Found=Event.GetTime();
            }
        }
        return Found;
    }

    double Limit(double Value, double Maximum)
    {
        return FMath::Clamp(Value, -FMath::Clamp(Maximum, 0.0, 180.0), FMath::Clamp(Maximum, 0.0, 180.0));
    }

    bool DescendsFrom(const FCombatBodyRig& Rig, int32 Bone, int32 Ancestor)
    {
        for (int32 I=Bone; I!=INDEX_NONE; I=Rig.Parents[I]) if (I==Ancestor) return true;
        return false;
    }

    void RotateChain(const FCombatBodyRig& Rig, TArray<FTransform>& Pose, int32 Root, const FQuat& Rotation)
    {
        const FVector Pivot=Pose[Root].GetLocation();
        for (int32 I=Root; I<Pose.Num(); ++I) if (DescendsFrom(Rig,I,Root))
        {
            Pose[I].SetLocation(Pivot+Rotation.RotateVector(Pose[I].GetLocation()-Pivot));
            Pose[I].SetRotation((Rotation*Pose[I].GetRotation()).GetNormalized());
        }
    }

    FQuat InWorld(const FQuat& Frame, const FQuat& Rotation)
    {
        return (Frame*Rotation*Frame.Inverse()).GetNormalized();
    }

    bool RestoreFoot(const FCombatBodyRig& Rig, TArray<FTransform>& Pose, int32 Side,
        const FTransform& Target, const FTransform& RootWorld, bool& bClamped)
    {
        const int32 A=Rig.Hips[Side], B=Rig.Knees[Side], C=Rig.Feet[Side];
        const FTransform OldA=Pose[A], OldB=Pose[B], OldC=Pose[C];
        const double L1=FVector::Distance(OldA.GetLocation(),OldB.GetLocation());
        const double L2=FVector::Distance(OldB.GetLocation(),OldC.GetLocation());
        if (L1<1 || L2<1) return false;
        FVector Ray=Target.GetLocation()-OldA.GetLocation();
        // Keep a finite knee bend. Near full extension the IK bend plane becomes
        // singular and tiny terrain/target changes can flip the leg between frames.
        const double Minimum=FMath::Max(FMath::Abs(L1-L2)+0.01,(L1+L2)*0.05);
        const double Maximum=FMath::Max(Minimum,(L1+L2)*0.98);
        const double Reach=FMath::Clamp(Ray.Size(),Minimum,Maximum);
        bClamped|=!FMath::IsNearlyEqual(Reach,Ray.Size(),0.1);
        const FVector Goal=OldA.GetLocation()+Ray.GetSafeNormal(UE_SMALL_NUMBER,-RootWorld.GetUnitAxis(EAxis::Z))*Reach;
        const FVector LegAxis=(OldC.GetLocation()-OldA.GetLocation()).GetSafeNormal(UE_SMALL_NUMBER,-RootWorld.GetUnitAxis(EAxis::Z));
        FVector Bend=FVector::VectorPlaneProject(OldB.GetLocation()-OldA.GetLocation(),LegAxis);
        if (Bend.SizeSquared()<1) Bend=FVector::VectorPlaneProject(RootWorld.GetUnitAxis(EAxis::X),LegAxis);
        const FVector Pole=OldA.GetLocation()+Bend.GetSafeNormal(UE_SMALL_NUMBER,RootWorld.GetUnitAxis(EAxis::X))*(L1+L2);
        UDragonIK_Library::Solve_Modified_Direct_TwoBoneIK_3(FTransform::Identity,0,
            Pose[A],Pose[B],Pose[C],Pole,Goal,L1,L2,false,1.0f,1.0f,0.5f,false);
        Pose[C].SetRotation(Target.GetRotation());
        for (int32 I=0; I<Pose.Num(); ++I)
        {
            if (I==A || I==B || I==C) continue;
            int32 Parent=Rig.Parents[I];
            while (Parent!=INDEX_NONE && Parent!=A && Parent!=B && Parent!=C) Parent=Rig.Parents[Parent];
            if (Parent!=INDEX_NONE)
                Pose[I]=Pose[I].GetRelativeTransform(Parent==A?OldA:Parent==B?OldB:OldC)*Pose[Parent];
        }
        return true;
    }
}

bool CombatBodySolver::BindRig(USkeletalMeshComponent& Mesh, const FCombatStyleDefinition& Style,
    FCombatBodyRig& OutRig, FString& OutError)
{
    OutRig=FCombatBodyRig();
    OutError.Reset();
    const auto& Limits=Style.Anatomy;
    const float Values[]={Style.BasePoseTime,Style.GuardResponseSpeed,Style.AttackResponseSpeed,Style.CounterLoadDegrees,
        Style.MinimumPreparationSeconds,Style.ChargeStartSeconds,Style.MaximumChargeSeconds,Style.ChargedExtraLoadDegrees,
        Style.WeaponWeightParticipation,Style.LightHints.PoseWeight,Style.HeavyHints.PoseWeight,Style.PierceHints.PoseWeight,Style.PelvisParticipation,
        Style.TorsoParticipation,Style.ShoulderParticipation,Style.GuardInterceptionWeight,
        Limits.PelvisYawDegrees,Limits.TorsoYawDegrees,Limits.TorsoLeanDegrees,Limits.ClavicleDegrees,
        Limits.ShoulderYawDegrees,Limits.ShoulderPitchDegrees,Limits.WristSwingDegrees,
        Limits.MinimumArmExtension,Limits.MaximumArmExtension,Limits.AttackWristSwingDegrees,
        Style.MaximumInertiaPullCm,Style.FollowThroughDamping};
    for (float Value:Values) if (!FMath::IsFinite(Value) || Value<0)
    {
        OutError=TEXT("Style tuning must be finite and nonnegative.");
        return false;
    }
    if (Style.ElbowPoleOffset.ContainsNaN() || Limits.MinimumArmExtension>=Limits.MaximumArmExtension)
    {
        OutError=TEXT("Invalid elbow pole or arm extension interval.");
        return false;
    }
    if (Style.DominantHand!=ECombatHand::Right)
    {
        OutError=TEXT("This rig binding requires a right dominant hand; left dominance must be explicitly calibrated.");
        return false;
    }
    auto* Asset=Mesh.GetSkeletalMeshAsset();
    auto* Base=Style.BasePose.LoadSynchronous();
    OutRig.Catalog=UCombatMotionCatalog::GetProfile(Style);
    const auto Library=OutRig.Catalog?UCombatMotionCatalog::RuntimeSources(*OutRig.Catalog):LibrarySources(Style);
    float GuardPriority=-FLT_MAX;
    for (const auto& Entry:Library)
        if (Entry.Action==TEXT("Guard") && Entry.Priority>GuardPriority)
            if (auto* GuardBase=Entry.Animation.LoadSynchronous()) { Base=GuardBase; GuardPriority=Entry.Priority; }
    if (!Asset || !Base || !Base->GetSkeleton())
    {
        OutError=TEXT("A skeletal mesh and one valid Combat Motion Style BasePose are required.");
        return false;
    }
    const float BaseMarker=Style.bReadAnimationHints?MarkerTime(*Base,TEXT("Base")):-1;
    const float SampleTime=BaseMarker>=0?BaseMarker:Style.BasePoseTime;
    const FReferenceSkeleton& Ref=Asset->GetRefSkeleton();
    const FReferenceSkeleton& Source=Base->GetSkeleton()->GetReferenceSkeleton();
    FCombatBodyRig Rig;
    Rig.ReferenceLocalPose=Ref.GetRefBonePose();
    Rig.Pelvis=Ref.FindBoneIndex(TEXT("pelvis"));
    Rig.Clavicle=Ref.FindBoneIndex(TEXT("clavicle_r"));
    Rig.Shoulder=Ref.FindBoneIndex(TEXT("upperarm_r"));
    Rig.Elbow=Ref.FindBoneIndex(TEXT("lowerarm_r"));
    Rig.Hand=Ref.FindBoneIndex(TEXT("hand_r"));
    if (Style.bTwoHanded)
    {
        Rig.SupportClavicle=Ref.FindBoneIndex(TEXT("clavicle_l"));
        Rig.SupportShoulder=Ref.FindBoneIndex(TEXT("upperarm_l"));
        Rig.SupportElbow=Ref.FindBoneIndex(TEXT("lowerarm_l"));
        Rig.SupportHand=Ref.FindBoneIndex(TEXT("hand_l"));
        if (Rig.Clavicle==INDEX_NONE || Rig.SupportClavicle==INDEX_NONE || Rig.SupportShoulder==INDEX_NONE
            || Rig.SupportElbow==INDEX_NONE || Rig.SupportHand==INDEX_NONE
            || Ref.GetParentIndex(Rig.SupportShoulder)!=Rig.SupportClavicle
            || Ref.GetParentIndex(Rig.SupportElbow)!=Rig.SupportShoulder
            || Ref.GetParentIndex(Rig.SupportHand)!=Rig.SupportElbow
            || Ref.GetParentIndex(Rig.SupportClavicle)!=Ref.GetParentIndex(Rig.Clavicle))
        {
            OutError=TEXT("Two-hand style requires both clavicle/upperarm/lowerarm/hand chains on the same chest.");
            return false;
        }
    }
    for (int32 Side=0; Side<2; ++Side)
    {
        Rig.Hips[Side]=Ref.FindBoneIndex(Side==0?TEXT("thigh_r"):TEXT("thigh_l"));
        Rig.Knees[Side]=Ref.FindBoneIndex(Side==0?TEXT("calf_r"):TEXT("calf_l"));
        Rig.Feet[Side]=Ref.FindBoneIndex(Side==0?TEXT("foot_r"):TEXT("foot_l"));
    }
    if (Rig.Pelvis==INDEX_NONE || Rig.Clavicle==INDEX_NONE || Rig.Shoulder==INDEX_NONE
        || Rig.Elbow==INDEX_NONE || Rig.Hand==INDEX_NONE)
    {
        OutError=TEXT("Missing pelvis/clavicle_r/upperarm_r/lowerarm_r/hand_r bone chain.");
        return false;
    }
    if (Ref.GetParentIndex(Rig.Shoulder)!=Rig.Clavicle || Ref.GetParentIndex(Rig.Elbow)!=Rig.Shoulder
        || Ref.GetParentIndex(Rig.Hand)!=Rig.Elbow)
    {
        OutError=TEXT("Unsupported arm hierarchy; phase-2 requires a direct clavicle/arm/forearm/hand chain.");
        return false;
    }
    for (int32 Side=0; Side<2; ++Side)
    {
        if (Rig.Hips[Side]==INDEX_NONE || Rig.Knees[Side]==INDEX_NONE || Rig.Feet[Side]==INDEX_NONE
            || Ref.GetParentIndex(Rig.Hips[Side])!=Rig.Pelvis
            || Ref.GetParentIndex(Rig.Knees[Side])!=Rig.Hips[Side]
            || Ref.GetParentIndex(Rig.Feet[Side])!=Rig.Knees[Side])
        {
            OutError=TEXT("Unsupported thigh/calf/foot hierarchy for stance preservation.");
            return false;
        }
    }
    int32 Spine=Ref.GetParentIndex(Rig.Clavicle);
    while (Spine!=INDEX_NONE && Spine!=Rig.Pelvis)
    {
        Rig.Spine.Insert(Spine,0);
        Spine=Ref.GetParentIndex(Spine);
    }
    if (Spine!=Rig.Pelvis || Rig.Spine.IsEmpty())
    {
        OutError=TEXT("Clavicle must descend from pelvis through a spine chain.");
        return false;
    }
    for (int32 I=0; I<Ref.GetNum(); ++I)
    {
        Rig.Names.Add(Ref.GetBoneName(I));
        Rig.Parents.Add(Ref.GetParentIndex(I));
        Rig.StyleBones.Add(I==Rig.Pelvis || DescendsFrom(Rig,I,Rig.Spine[0]));
        FQuat Rotation=Ref.GetRefBonePose()[I].GetRotation();
        if (!Rig.Spine.Contains(I))
        {
            const int32 SourceIndex=Source.FindBoneIndex(Ref.GetBoneName(I));
            if (SourceIndex==INDEX_NONE && (I==Rig.Pelvis || I==Rig.Clavicle || I==Rig.Shoulder
                || I==Rig.Elbow || I==Rig.Hand || (Style.bTwoHanded && (I==Rig.SupportClavicle
                    || I==Rig.SupportShoulder || I==Rig.SupportElbow || I==Rig.SupportHand))))
            {
                OutError=FString::Printf(TEXT("Base pose skeleton lacks upper-body bone %s."),*Ref.GetBoneName(I).ToString());
                return false;
            }
            if (SourceIndex!=INDEX_NONE)
            {
                FTransform Sample;
                Base->GetBoneTransform(Sample,FSkeletonPoseBoneIndex(SourceIndex),
                    FAnimExtractContext(static_cast<double>(FMath::Clamp(SampleTime,0.0f,static_cast<float>(Base->GetPlayLength())))),false);
                Rotation=(Sample.GetRotation()*Source.GetRefBonePose()[SourceIndex].GetRotation().Inverse()*Rotation).GetNormalized();
                if (Rotation.ContainsNaN()) { OutError=TEXT("Base pose contains an invalid rotation."); return false; }
            }
            else Rig.StyleBones[I]=false; // Additional twist/accessory bones retain local locomotion pose.
        }
        Rig.StyleLocalRotations.Add(Rotation);
    }
    // Transfer cumulative spine rotation across normalized chain length. Source and
    // target need not have the same numbered spine bones (UEFN vs UE5 skeletons).
    const int32 SourcePelvis=Source.FindBoneIndex(TEXT("pelvis"));
    const int32 SourceClavicle=Source.FindBoneIndex(TEXT("clavicle_r"));
    TArray<int32> SourceSpine;
    int32 SourceParent=SourceClavicle==INDEX_NONE?INDEX_NONE:Source.GetParentIndex(SourceClavicle);
    while (SourceParent!=INDEX_NONE && SourceParent!=SourcePelvis)
    {
        SourceSpine.Insert(SourceParent,0);
        SourceParent=Source.GetParentIndex(SourceParent);
    }
    if (SourcePelvis==INDEX_NONE || SourceParent!=SourcePelvis || SourceSpine.IsEmpty())
    {
        OutError=TEXT("Base pose needs a pelvis-to-clavicle spine chain for rotation retargeting.");
        return false;
    }
    TArray<FQuat> SourceDeltas;
    SourceDeltas.Add(FQuat::Identity);
    FQuat SourceReference=FQuat::Identity, SourcePosed=FQuat::Identity;
    for (int32 Bone:SourceSpine)
    {
        FTransform Sample;
        Base->GetBoneTransform(Sample,FSkeletonPoseBoneIndex(Bone),
            FAnimExtractContext(static_cast<double>(FMath::Clamp(SampleTime,0.0f,static_cast<float>(Base->GetPlayLength())))),false);
        SourceReference=(SourceReference*Source.GetRefBonePose()[Bone].GetRotation()).GetNormalized();
        SourcePosed=(SourcePosed*Sample.GetRotation()).GetNormalized();
        const FQuat Delta=(SourcePosed*SourceReference.Inverse()).GetNormalized();
        if (Delta.ContainsNaN()) { OutError=TEXT("Base spine pose contains an invalid rotation."); return false; }
        SourceDeltas.Add(Delta);
    }
    FQuat TargetReference=FQuat::Identity, PreviousTarget=FQuat::Identity;
    for (int32 I=0; I<Rig.Spine.Num(); ++I)
    {
        const int32 Bone=Rig.Spine[I];
        const double Position=static_cast<double>(I+1)*SourceSpine.Num()/Rig.Spine.Num();
        const int32 Lower=FMath::FloorToInt(Position);
        const int32 Upper=FMath::Min(Lower+1,SourceSpine.Num());
        const FQuat Delta=FQuat::Slerp(SourceDeltas[Lower],SourceDeltas[Upper],Position-Lower).GetNormalized();
        TargetReference=(TargetReference*Ref.GetRefBonePose()[Bone].GetRotation()).GetNormalized();
        const FQuat Target=(Delta*TargetReference).GetNormalized();
        Rig.StyleLocalRotations[Bone]=(PreviousTarget.Inverse()*Target).GetNormalized();
        PreviousTarget=Target;
    }
    // Socket calibration is rig-local, read once. It never comes from the current weapon.
    if (Style.PrimaryGripSocket.IsNone())
    {
        if (Style.PrimaryGripInHand.ContainsNaN() || !Style.PrimaryGripInHand.GetRotation().IsNormalized()
            || !Style.PrimaryGripInHand.GetScale3D().Equals(FVector::OneVector))
        {
            OutError=TEXT("PrimaryGripInHand must be a valid rigid, unit-scale hand-local transform.");
            return false;
        }
        Rig.GripBone=Rig.Hand;
        Rig.GripLocal=Style.PrimaryGripInHand;
    }
    else
    {
        const USkeletalMeshSocket* Socket=Mesh.GetSocketByName(Style.PrimaryGripSocket);
        Rig.GripBone=Ref.FindBoneIndex(Socket?Socket->BoneName:Style.PrimaryGripSocket);
        if (Rig.GripBone==INDEX_NONE || !DescendsFrom(Rig,Rig.GripBone,Rig.Hand))
        {
            OutError=FString::Printf(TEXT("Grip '%s' on mesh '%s' must resolve to hand_r or a descendant bone/socket. Set None to use PrimaryGripInHand."),
                *Style.PrimaryGripSocket.ToString(),*Asset->GetName());
            return false;
        }
        Rig.GripLocal=Socket?FTransform(Socket->RelativeRotation,Socket->RelativeLocation,FVector::OneVector):FTransform::Identity;
    }
    if (Style.bTwoHanded)
    {
        if (Style.SecondaryGripSocket.IsNone())
        {
            Rig.SupportGripBone=Rig.SupportHand;
            Rig.SupportGripLocal=Style.SecondaryGripInHand;
        }
        else
        {
            const USkeletalMeshSocket* Socket=Mesh.GetSocketByName(Style.SecondaryGripSocket);
            Rig.SupportGripBone=Ref.FindBoneIndex(Socket?Socket->BoneName:Style.SecondaryGripSocket);
            Rig.SupportGripLocal=Socket?FTransform(Socket->RelativeRotation,Socket->RelativeLocation,FVector::OneVector):FTransform::Identity;
        }
        if (Rig.SupportGripBone==INDEX_NONE || !DescendsFrom(Rig,Rig.SupportGripBone,Rig.SupportHand)
            || Rig.SupportGripLocal.ContainsNaN() || !Rig.SupportGripLocal.GetRotation().IsNormalized()
            || !Rig.SupportGripLocal.GetScale3D().Equals(FVector::OneVector))
        {
            OutError=TEXT("Two-hand support grip must be a rigid hand_l descendant socket or explicit SecondaryGripInHand.");
            return false;
        }
    }
    TArray<FTransform> Reference=Ref.GetRefBonePose();
    for (int32 I=0; I<Reference.Num(); ++I)
        if (Rig.Parents[I]!=INDEX_NONE) Reference[I]=Reference[I]*Reference[Rig.Parents[I]];
    Rig.StandingPelvisHeight=FMath::Abs((Reference[Rig.Pelvis].GetLocation()-
        (Reference[Rig.Feet[0]].GetLocation()+Reference[Rig.Feet[1]].GetLocation())*0.5).Z);
    const FQuat ReferenceClavicle=(Reference[Rig.Clavicle]*Mesh.GetComponentTransform()).GetRotation();
    Rig.ClavicleAnatomicalFrame=(ReferenceClavicle.Inverse()*Mesh.GetOwner()->GetActorQuat()).GetNormalized();
    if (Style.bReadAnimationHints)
    {
        const FCombatAnimationHints* Sources[]={&Style.LightHints,&Style.HeavyHints,&Style.PierceHints};
        for (int32 Family=0; Family<3; ++Family)
        {
            const auto& Input=*Sources[Family];
            UAnimSequence* Animation=Input.Animation.IsNull()?Base:Input.Animation.LoadSynchronous();
            if (!Animation) continue;
            auto& Hints=Rig.Hints[Family];
            Hints.Weight=FMath::Clamp(Input.PoseWeight,0.0f,0.5f);
            Hints.WindTime=MarkerTime(*Animation,TEXT("Wind_Up"));
            Hints.DestinationTime=MarkerTime(*Animation,TEXT("Attack_Destination"));
            Hints.FinishTime=MarkerTime(*Animation,TEXT("Finish"));
            const float Times[]={Hints.WindTime,Hints.DestinationTime,Hints.FinishTime};
            TArray<FQuat>* Poses[]={&Hints.WindUp,&Hints.Destination,&Hints.Finish};
            for (int32 Marker=0; Marker<3; ++Marker) if (Times[Marker]>=0)
            {
                FCombatStyleDefinition Sampling=Style;
                Sampling.bReadAnimationHints=false; // sample once; never recurse into hint discovery
                Sampling.MotionLibrary.Reset();
        Sampling.MotionSources.Reset();
                Sampling.ThrustMotionSources.Reset();
                Sampling.BasePose=Animation;
                Sampling.BasePoseTime=Times[Marker];
                FCombatBodyRig Sampled; FString Error;
                if (BindRig(Mesh,Sampling,Sampled,Error)) *Poses[Marker]=MoveTemp(Sampled.StyleLocalRotations);
                else UE_LOG(LogTemp,Warning,TEXT("Combat hint %s ignored: %s"),*Animation->GetName(),*Error);
            }
        }
    }
    if (!Style.MotionLibrary.IsNull()) Rig.Sources=Library;
    else
    {
    for (int32 I=0; I<FMath::Min(10,Style.MotionSources.Num()); ++I) Rig.Sources.Add(Style.MotionSources[I]);
    for (int32 I=0; I<FMath::Min(10,Style.ThrustMotionSources.Num()); ++I)
    {
        auto ThrustSource=Style.ThrustMotionSources[I];
        ThrustSource.bCuts=false; ThrustSource.bThrusts=true; ThrustSource.NativeFamily=EDirectionalAttack::Pierce;
        Rig.Sources.Add(ThrustSource);
    }
    }
    Rig.Motions.SetNum(Rig.Sources.Num());
    for (int32 Index=0; Index<Rig.Motions.Num(); ++Index)
    {
        const auto& Input=Rig.Sources[Index];
        UAnimSequence* Animation=Input.Animation.LoadSynchronous();
        if (!Animation || (Animation->IsValidAdditive() && Input.Action!=TEXT("GuardAim") && Input.Action!=TEXT("LookAim")) || !FMath::IsFinite(Input.PoseWeight)
            || !FMath::IsFinite(Input.AdaptationDegrees) || !FMath::IsFinite(Input.LoadTime)
            || !FMath::IsFinite(Input.ContactTime) || !FMath::IsFinite(Input.FinishTime)
            || Input.NeutralTarget.ContainsNaN() || Input.NeutralTarget.SizeSquared()<1
            || Input.LoadTime<=0 || Input.ContactTime<=Input.LoadTime
            || Input.FinishTime<=Input.ContactTime || Input.FinishTime>1
            || Animation->GetPlayLength()<=0)
        {
            UE_LOG(LogTemp,Warning,TEXT("Combat motion source %d ignored: invalid sequence or phase interval."),Index);
            continue;
        }
        FCombatSampledMotion Motion;
        Motion.Length=Animation->GetPlayLength();
        Motion.Sequence.Reset(Animation);
        Motion.bSameSkeleton=Animation->GetSkeleton()==Asset->GetSkeleton();
        Motion.bAdditive=Animation->IsValidAdditive();
        for (FName Name:Rig.Names) Motion.SourceBones.Add(Animation->GetSkeleton()->GetReferenceSkeleton().FindBoneIndex(Name));
        const int32 Count=FMath::Clamp(FMath::CeilToInt(Motion.Length*30)+1,2,121);
        FCombatStyleDefinition Sampling=Style;
        Sampling.bReadAnimationHints=false;
        Sampling.MotionLibrary.Reset();
        Sampling.MotionSources.Reset();
        Sampling.ThrustMotionSources.Reset();
        Sampling.BasePose=Animation;
        for (int32 Frame=0; Frame<Count; ++Frame)
        {
            Sampling.BasePoseTime=Motion.Length*Frame/(Count-1);
            FCombatMotionFrame Key;
            if (Motion.bSameSkeleton)
            {
                for (int32 Bone=0; Bone<Rig.Names.Num(); ++Bone)
                {
                    FTransform Sample;
                    Animation->GetBoneTransform(Sample,FSkeletonPoseBoneIndex(Motion.SourceBones[Bone]),FAnimExtractContext(double(Sampling.BasePoseTime)),false);
                    Key.LocalRotations.Add(Sample.GetRotation());
                }
            }
            else
            {
                FCombatBodyRig Sampled; FString Error;
                if (!BindRig(Mesh,Sampling,Sampled,Error)) { Motion.Frames.Reset(); break; }
                Key.LocalRotations=MoveTemp(Sampled.StyleLocalRotations);
            }
            const auto& SourceRef=Animation->GetSkeleton()->GetReferenceSkeleton();
            const int32 PelvisIndex=SourceRef.FindBoneIndex(TEXT("pelvis"));
            FTransform PelvisSample;
            Animation->GetBoneTransform(PelvisSample,FSkeletonPoseBoneIndex(PelvisIndex),
                FAnimExtractContext(double(Sampling.BasePoseTime)),false);
            Key.PelvisTranslation=PelvisSample.GetLocation();
            FTransform RootSample;
            Animation->GetBoneTransform(RootSample,FSkeletonPoseBoneIndex(0),FAnimExtractContext(double(Sampling.BasePoseTime)),false);
            Key.RootTranslation=RootSample.GetLocation();
            if (Key.PelvisTranslation.ContainsNaN()) { Motion.Frames.Reset(); break; }
            TArray<FTransform> Component=Ref.GetRefBonePose();
            for (int32 Bone=0; Bone<Component.Num(); ++Bone)
            {
                // Exclude source root travel/rotation. Gameplay motion belongs to Mover.
                if (Rig.Parents[Bone]!=INDEX_NONE) Component[Bone].SetRotation(Key.LocalRotations[Bone]);
                if (Rig.Parents[Bone]!=INDEX_NONE) Component[Bone]*=Component[Rig.Parents[Bone]];
            }
            for (int32 Side=0; Side<2; ++Side) Key.Feet[Side]=Component[Rig.Feet[Side]].GetLocation();
            Key.Grip=Rig.GripLocal*Component[Rig.GripBone];
            Motion.Frames.Add(MoveTemp(Key));
        }
        Rig.Motions[Index]=MoveTemp(Motion);
    }
    OutRig=MoveTemp(Rig);
    return true;
}

bool CombatBodySolver::SampleMotion(const FCombatBodyRig& Rig, int32 Index, float Time,
    TArray<FTransform>& Local, TArray<FTransform>& Component)
{
    if (!Rig.Motions.IsValidIndex(Index) || !Rig.Motions[Index].IsValid()) return false;
    const auto& Motion=Rig.Motions[Index];
    const UAnimSequence* Sequence=Motion.Sequence.Get();
    if (!Sequence) return false;
    const float Seconds=FMath::Clamp(Time,0.0f,1.0f)*Motion.Length;
    const auto& Source=Sequence->GetSkeleton()->GetReferenceSkeleton();
    const double Position=FMath::Clamp(double(Time),0.0,1.0)*(Motion.Frames.Num()-1);
    const int32 A=FMath::FloorToInt(Position), B=FMath::Min(A+1,Motion.Frames.Num()-1);
    Local=Rig.ReferenceLocalPose;
    Component.SetNum(Local.Num());
    for (int32 Bone=0; Bone<Local.Num(); ++Bone)
    {
        const int32 SourceBone=Motion.SourceBones[Bone];
        // The mapped spine may contain target bones absent from the source rig.
        // Apply the cumulative-chain mapping to every target spine segment.
        if (!Motion.bSameSkeleton && Rig.Spine.Contains(Bone))
            Local[Bone].SetRotation(FQuat::Slerp(Motion.Frames[A].LocalRotations[Bone],
                Motion.Frames[B].LocalRotations[Bone],Position-A).GetNormalized());
        if (SourceBone!=INDEX_NONE)
        {
            FTransform Sample;
            Sequence->GetBoneTransform(Sample,FSkeletonPoseBoneIndex(SourceBone),FAnimExtractContext(double(Seconds)),false);
            if (Motion.bSameSkeleton) Local[Bone]=Sample;
            else
            {
                // Cross-skeleton source retains target proportions; the existing cumulative
                // spine mapping is necessary when the rigs have different spine counts.
                Local[Bone].SetRotation(Rig.Spine.Contains(Bone)?FQuat::Slerp(
                    Motion.Frames[A].LocalRotations[Bone],Motion.Frames[B].LocalRotations[Bone],Position-A).GetNormalized():
                    (Sample.GetRotation()*Source.GetRefBonePose()[SourceBone].GetRotation().Inverse()*Local[Bone].GetRotation()).GetNormalized());
                if (Bone==Rig.Pelvis) Local[Bone].AddToTranslation(Sample.GetLocation()-Source.GetRefBonePose()[SourceBone].GetLocation());
            }
        }
        // Horizontal root translation must be executed by Mover, never hidden in the mesh.
        if (Rig.Parents[Bone]==INDEX_NONE) Local[Bone].SetTranslation(Rig.ReferenceLocalPose[Bone].GetTranslation());
        if (Local[Bone].ContainsNaN()) return false;
        Component[Bone]=Local[Bone]*(Rig.Parents[Bone]==INDEX_NONE?FTransform::Identity:Component[Rig.Parents[Bone]]);
    }
    return true;
}

bool CombatBodySolver::SolveMotion(const FCombatBodyRig& Rig, const TArray<FTransform>& LiveComponentPose,
    const FTransform& MeshWorld, const FTransform& ActualRootWorld, const FCombatStyleDefinition& Style,
    const FCombatWeaponDefinition& Weapon, const FCombatIntent& Intent, const FCombatBodyDrive& Drive,
    float BlendAlpha, float DeltaSeconds, FCombatBodySolverState& State,
    FCombatMotionPlan& OutPlan, FCombatFinalPose& OutPose, FCombatBodySolverDiagnostics& OutDiagnostics)
{
    // PhysicsAsset constrains the solved targets; it must not bypass directional arm/grip IK.
    const bool bProtectedAttack=Style.bPreserveAuthoredAttackPose && Drive.Weight>0
        && Drive.MotionWeight>0 && Style.DebugStages.AuthoredPose>0
        && Drive.AuthoredLocalPose.Num()==Rig.Names.Num()
        && Rig.Motions.IsValidIndex(Drive.MotionSourceIndex) && Rig.Motions[Drive.MotionSourceIndex].IsValid();
    if (bProtectedAttack || Style.DebugStages.MotionFit<=0 || Drive.WorkingPointWeight<=0 || Drive.WorkingPointGoal.ContainsNaN())
    {
        State.MotionFit*=FMath::Exp(-10*FMath::Max(0.0f,DeltaSeconds));
        if (!SolveGuard(Rig,LiveComponentPose,MeshWorld,ActualRootWorld,Style,Intent,Drive,
            BlendAlpha,DeltaSeconds,State,OutPlan,OutPose,OutDiagnostics)
            || !CloseGripPair(Rig,Style,Weapon,OutPose,OutDiagnostics,State.bHasGripElbows?State.GripElbows:nullptr)) return false;
        if (Style.bTwoHanded)
        {
            State.GripElbows[0]=ActualRootWorld.InverseTransformPosition(OutPose.BonesWorld.FindChecked(Rig.Names[Rig.Elbow]).GetLocation());
            State.GripElbows[1]=ActualRootWorld.InverseTransformPosition(OutPose.BonesWorld.FindChecked(Rig.Names[Rig.SupportElbow]).GetLocation());
            State.bHasGripElbows=true;
        }
        OutPlan.PrimaryGripGoal=OutPose.PrimaryGripWorld.GetRelativeTransform(ActualRootWorld);
        if (Style.bTwoHanded) OutPlan.SecondaryGripGoal=OutPose.SecondaryGripWorld.GetRelativeTransform(ActualRootWorld);
        return true;
    }
    const FCombatBodySolverState Initial=State;
    const FVector Previous=Initial.MotionFit;
    FVector Best=Previous;
    double BestCost=TNumericLimits<double>::Max();
    const auto Evaluate=[&](const FVector& Parameters)
    {
        FCombatBodyDrive Candidate=Drive;
        const FQuat Frame=ActualRootWorld.GetRotation();
        const FQuat Turn=(Frame*FRotator(Parameters.Y,Parameters.X,0).Quaternion()*Frame.Inverse()).GetNormalized();
        Candidate.Direction=Turn.RotateVector(Drive.Direction);
        Candidate.CoreRotation+=FRotator(Parameters.Y*0.3,Parameters.X*0.5,0);
        Candidate.Extension=FMath::Clamp(Drive.Extension+float(Parameters.Z),
            Style.Anatomy.MinimumArmExtension,Style.Anatomy.MaximumArmExtension);
        FCombatBodySolverState Next=Initial;
        FCombatMotionPlan Plan; FCombatFinalPose Pose; FCombatBodySolverDiagnostics Diagnostics;
        if (!SolveGuard(Rig,LiveComponentPose,MeshWorld,ActualRootWorld,Style,Intent,Candidate,
            BlendAlpha,DeltaSeconds,Next,Plan,Pose,Diagnostics)) return;
        if (OutPose.BonesWorld.IsEmpty()) OutPose=Pose;
        if (!CloseGripPair(Rig,Style,Weapon,Pose,Diagnostics,Initial.bHasGripElbows?Initial.GripElbows:nullptr)) return;
        Plan.PrimaryGripGoal=Pose.PrimaryGripWorld.GetRelativeTransform(ActualRootWorld);
        if (Style.bTwoHanded) Plan.SecondaryGripGoal=Pose.SecondaryGripWorld.GetRelativeTransform(ActualRootWorld);
        FTransform Result;
        if (!CombatGripAdapter::TryBuildWeaponTarget(Pose,Weapon,Style.bTwoHanded,Result)) return;
        const FVector Base=Result.TransformPosition(Weapon.BladeBaseLocal);
        const FVector Tip=Result.TransformPosition(Weapon.BladeTipLocal);
        const bool bThrust=Intent.AttackType==EDirectionalAttack::Pierce;
        const FVector Working=bThrust?Tip:FMath::Lerp(Base,Tip,0.75);
        const double Error=FVector::DistSquared(Working,Drive.WorkingPointGoal);
        const double Alignment=FVector::DotProduct((Tip-Base).GetSafeNormal(),Drive.StrikeDirection);
        // During a cut the blade should sweep across the travel direction; during a
        // thrust its tip should lead. Score the actual blade, never a hand-only proxy.
        const double CrossTravel=FVector::DotProduct((Tip-Base).GetSafeNormal(),Drive.WorkingTravelDirection);
        // Pose deviation and temporal change compete with accuracy. Misses remain misses;
        // we never enlarge anatomy, relocate the actor, or snap the physical weapon.
        const FVector Change=Parameters-Previous;
        const double Cost=Error*Drive.WorkingPointWeight+
            Drive.WorkingPointWeight*(bThrust?400*FMath::Square(1-Alignment):100*CrossTravel*CrossTravel)
            +0.4*(Parameters.X*Parameters.X+Parameters.Y*Parameters.Y)+1200*Parameters.Z*Parameters.Z
            +2*(Change.X*Change.X+Change.Y*Change.Y)+2000*Change.Z*Change.Z
            +(Diagnostics.bDirectionClamped?100:0)+(Diagnostics.bFootReachClamped?200:0)
            +Diagnostics.GripCorrectionCm*Diagnostics.GripCorrectionCm;
        if (Cost>=BestCost) return;
        BestCost=Cost; Best=Parameters; Next.MotionFit=Best;
        if (Style.bTwoHanded)
        {
            Next.GripElbows[0]=ActualRootWorld.InverseTransformPosition(Pose.BonesWorld.FindChecked(Rig.Names[Rig.Elbow]).GetLocation());
            Next.GripElbows[1]=ActualRootWorld.InverseTransformPosition(Pose.BonesWorld.FindChecked(Rig.Names[Rig.SupportElbow]).GetLocation());
            Next.bHasGripElbows=true;
        }
        Diagnostics.WorkingPointErrorCm=FMath::Sqrt(Error);
        State=MoveTemp(Next); OutPlan=MoveTemp(Plan); OutPose=MoveTemp(Pose); OutDiagnostics=Diagnostics;
    };
    Evaluate(Best);
    // Seven whole-body candidates; every candidate starts from the same simulation
    // state, so searching cannot advance springs or interpolation seven times.
    const double AngularStep=FMath::Min(3.0,FMath::Max(0.0,double(DeltaSeconds))*90);
    const double ReachStep=FMath::Min(0.04,FMath::Max(0.0,double(DeltaSeconds))*1.2);
    for (int32 Dimension=0; Dimension<3; ++Dimension)
    {
        const FVector Centre=Best;
        for (int32 Sign=-1; Sign<=1; Sign+=2)
        {
            FVector Candidate=Centre;
            const double Budget=Dimension==2?0.15:25;
            Candidate[Dimension]=FMath::Clamp(Candidate[Dimension]+Sign*(Dimension==2?ReachStep:AngularStep),-Budget,Budget);
            Evaluate(Candidate);
        }
    }
    return BestCost<TNumericLimits<double>::Max();
}

bool CombatBodySolver::SolveGuard(const FCombatBodyRig& Rig, const TArray<FTransform>& LiveComponentPose,
    const FTransform& MeshWorld, const FTransform& ActualRootWorld, const FCombatStyleDefinition& Style,
    const FCombatIntent& Intent, const FCombatBodyDrive& Drive, float BlendAlpha, float DeltaSeconds, FCombatBodySolverState& State,
    FCombatMotionPlan& OutPlan, FCombatFinalPose& OutPose, FCombatBodySolverDiagnostics& OutDiagnostics)
{
    if (Rig.Names.IsEmpty() || LiveComponentPose.Num()!=Rig.Names.Num() || MeshWorld.ContainsNaN()
        || ActualRootWorld.ContainsNaN() || Intent.GuardDirectionWorld.ContainsNaN()
        || !FMath::IsFinite(DeltaSeconds) || !FMath::IsFinite(BlendAlpha)) return false;
    for (const FTransform& Bone:LiveComponentPose) if (Bone.ContainsNaN()) return false;
    const double Alpha=FMath::Clamp(static_cast<double>(BlendAlpha),0.0,1.0);
    const double AuthoredResponse=Drive.MotionWeight>0?FMath::Clamp(Drive.AuthoredWorkingVelocity.Size()/100.0+FMath::Sqrt(Drive.AuthoredWorkingAcceleration.Size())/15.0,1.0,3.0):1;
    const double Response=1-FMath::Exp(AuthoredResponse*-FMath::Clamp(FMath::Lerp(Style.GuardResponseSpeed,Style.AttackResponseSpeed,Drive.Weight)/FMath::Sqrt(FMath::Clamp(Drive.WeaponLoad,1.0f,4.0f)),0.1f,80.0f)*FMath::Clamp(DeltaSeconds,0.0f,0.1f));
    const auto& Limits=Style.Anatomy;
    const FQuat RootRotation=ActualRootWorld.GetRotation();
    const FCombatSampledMotion* Motion=Rig.Motions.IsValidIndex(Drive.MotionSourceIndex)
        && Rig.Motions[Drive.MotionSourceIndex].IsValid()?&Rig.Motions[Drive.MotionSourceIndex]:nullptr;
    const double MotionWeight=Motion?FMath::Clamp(double(Drive.MotionWeight),0.0,1.0):0;
    const bool bAuthored=MotionWeight>0 && Drive.AuthoredLocalPose.Num()==Rig.Names.Num();
    const double CorrectionAlpha=Alpha*(1-MotionWeight+MotionWeight*Drive.AdaptationWeight);
    auto Debug=Style.DebugStages;
    if(Style.bPreserveAuthoredAttackPose && bAuthored && Drive.Weight>0){
        // The authored attack already supplies these joint trajectories. Applying generic
        // guard/aim rotations on top was the conflict isolated by the user's debug settings.
        const float Procedural=1.f-FMath::Clamp(float(MotionWeight)*Debug.AuthoredPose,0.f,1.f);
        Debug.BasePose*=Procedural;Debug.AnimationHints*=Procedural;
        Debug.PelvisYaw*=Procedural;Debug.Torso*=Procedural;
        Debug.ShoulderAim*=Procedural;Debug.ElbowFlex*=Procedural;
    }
    const double SamplePosition=Motion?FMath::Clamp(double(Drive.MotionTime),0.0,1.0)*(Motion->Frames.Num()-1):0;
    const int32 SampleA=FMath::FloorToInt(SamplePosition);
    const int32 SampleB=Motion?FMath::Min(SampleA+1,Motion->Frames.Num()-1):0;
    const FVector Guard=Intent.GuardDirectionWorld.GetSafeNormal(UE_SMALL_NUMBER,ActualRootWorld.GetUnitAxis(EAxis::X));
    const FVector BodyDirection=FMath::Lerp(Guard,Drive.Direction,Drive.Weight).GetSafeNormal(UE_SMALL_NUMBER,Guard);
    const FVector LocalGuard=RootRotation.UnrotateVector(BodyDirection);
    const double Yaw=bAuthored && Drive.bTrajectoryPlan?Drive.TrajectoryCorrection.Yaw:FMath::RadiansToDegrees(FMath::Atan2(LocalGuard.Y,LocalGuard.X));
    const double Pitch=bAuthored && Drive.bTrajectoryPlan?Drive.TrajectoryCorrection.Pitch:FMath::RadiansToDegrees(FMath::Atan2(LocalGuard.Z,FVector2D(LocalGuard.X,LocalGuard.Y).Size()));
    // Plan proximal angular demand first. Residual demand will be clamped at the shoulder.
    State.PelvisYaw=FMath::Lerp(State.PelvisYaw,Limit(Yaw*FMath::Clamp(Style.PelvisParticipation,0.0f,1.0f)+Drive.CoreRotation.Yaw*0.4*Drive.Weight,Limits.PelvisYawDegrees),Response);
    State.TorsoYaw=FMath::Lerp(State.TorsoYaw,Limit(Yaw*FMath::Clamp(Style.TorsoParticipation,0.0f,1.0f)+Drive.CoreRotation.Yaw*0.6*Drive.Weight,Limits.TorsoYawDegrees),Response);
    State.TorsoPitch=FMath::Lerp(State.TorsoPitch,Limit(Pitch*FMath::Clamp(Style.TorsoParticipation,0.0f,1.0f)+Drive.CoreRotation.Pitch*Drive.Weight-FMath::Clamp(Drive.WeaponLoad-1,0.0f,5.0f)*1.5,Limits.TorsoLeanDegrees),Response);
    State.ClavicleYaw=FMath::Lerp(State.ClavicleYaw,Limit((Yaw-State.PelvisYaw-State.TorsoYaw)*0.2,Limits.ClavicleDegrees),Response);
    State.ClaviclePitch=FMath::Lerp(State.ClaviclePitch,Limit((Pitch-State.TorsoPitch)*0.2,Limits.ClavicleDegrees),Response);
    State.LoadResponse=FMath::VInterpTo(State.LoadResponse,Drive.ReactionWorld,DeltaSeconds,6);
    const FVector BodyLoad=RootRotation.UnrotateVector(State.LoadResponse);
    FCombatMotionPlan Plan;
    Plan.Intent=Intent;
    Plan.StrikeDirection=(Intent.AttackTarget-ActualRootWorld.GetLocation()).GetSafeNormal(UE_SMALL_NUMBER,Guard);
    if (Drive.Weight>0) Plan.StrikeDirection=Drive.StrikeDirection;
    Plan.CounterLoadDirection=-Plan.StrikeDirection;
    Plan.AttackPhaseAlpha=Drive.PhaseAlpha;
    Plan.Anatomy=Limits;
    Plan.PelvisDelta.SetRotation(FRotator(0,State.PelvisYaw*CorrectionAlpha*Debug.PelvisYaw,0).Quaternion());
    Plan.TorsoRotation=FRotator(State.TorsoPitch*CorrectionAlpha,State.TorsoYaw*CorrectionAlpha,0)*Debug.Torso;
    Plan.ClavicleRotation=FRotator(State.ClaviclePitch*CorrectionAlpha,State.ClavicleYaw*CorrectionAlpha,0)*Debug.Clavicles;
    Plan.TorsoRotation.Pitch=Limit(Plan.TorsoRotation.Pitch-BodyLoad.X*8*Alpha*Debug.Torso,Limits.TorsoLeanDegrees);
    Plan.TorsoRotation.Yaw=Limit(Plan.TorsoRotation.Yaw+BodyLoad.Y*8*Alpha*Debug.Torso,Limits.TorsoYawDegrees);
    Plan.ClavicleRotation.Yaw=Limit(Plan.ClavicleRotation.Yaw+BodyLoad.Y*4*Alpha*Debug.Clavicles,Limits.ClavicleDegrees);
    Plan.ShoulderParticipation=FMath::Clamp(Style.ShoulderParticipation,0.0f,1.0f);
    Plan.GuardInterceptionWeight=FMath::Clamp(Style.GuardInterceptionWeight,0.0f,1.0f)*Alpha*(1-Drive.Weight);
    // Root motion and persistent planting await the Mover integration; retain animated feet.
    TArray<FTransform> Pose;
    Pose.SetNum(LiveComponentPose.Num());
    for (int32 I=0; I<Pose.Num(); ++I)
    {
        const int32 Parent=Rig.Parents[I];
        FTransform Local=Parent==INDEX_NONE?LiveComponentPose[I]:LiveComponentPose[I].GetRelativeTransform(LiveComponentPose[Parent]);
        const bool bMotionLeg=Motion && Drive.bUseFootTargets &&
            (DescendsFrom(Rig,I,Rig.Hips[0]) || DescendsFrom(Rig,I,Rig.Hips[1]));
        if (bAuthored)
        {
            const bool bLower=I==Rig.Pelvis || DescendsFrom(Rig,I,Rig.Hips[0]) || DescendsFrom(Rig,I,Rig.Hips[1]);
            if (Rig.StyleBones[I] && (!bLower || Drive.LegMotionWeight>0))
                Local.SetRotation(FQuat::Slerp(Local.GetRotation(),Rig.StyleLocalRotations[I],Alpha*Debug.BasePose).GetNormalized());
            Local.Blend(Local,Drive.AuthoredLocalPose[I],Alpha*Debug.AuthoredPose*(bLower?Drive.LegMotionWeight:MotionWeight));
        }
        else if (Rig.StyleBones[I] || bMotionLeg)
        {
            FQuat StyleRotation=bMotionLeg?Local.GetRotation():Rig.StyleLocalRotations[I];
            const auto& Hints=Rig.Hints[FMath::Clamp(int32(Intent.AttackType),0,2)];
            if (MotionWeight>0)
            {
                const FQuat Authored=FQuat::Slerp(Motion->Frames[SampleA].LocalRotations[I],
                    Motion->Frames[SampleB].LocalRotations[I],SamplePosition-SampleA).GetNormalized();
                const bool bSupportBone=I==Rig.Pelvis || Rig.Spine.Contains(I) || bMotionLeg;
                const double SupportWeight=bSupportBone?FMath::Lerp(0.35,1.0,double(Drive.MovementFulfillment)):1;
                StyleRotation=FQuat::Slerp(StyleRotation,Authored,MotionWeight*SupportWeight*Debug.AuthoredPose).GetNormalized();
            }
            else if (Drive.Weight>0 && Hints.Weight>0)
            {
                const FQuat Wind=Hints.WindUp.IsValidIndex(I)?Hints.WindUp[I]:StyleRotation;
                const FQuat Destination=Hints.Destination.IsValidIndex(I)?Hints.Destination[I]:StyleRotation;
                const FQuat Finish=Hints.Finish.IsValidIndex(I)?Hints.Finish[I]:StyleRotation;
                FQuat Hint=FQuat::Slerp(Wind,Destination,Drive.PhaseAlpha).GetNormalized();
                Hint=FQuat::Slerp(Hint,Finish,Drive.RecoveryAlpha).GetNormalized();
                FQuat Delta=(Hint*StyleRotation.Inverse()).GetNormalized();
                // Authored guidance cannot replace directional intent or anatomical constraints.
                const double Angle=Delta.AngularDistance(FQuat::Identity);
                Delta=FQuat::Slerp(FQuat::Identity,Delta,FMath::Min(1.0,FMath::DegreesToRadians(15.0)/FMath::Max(Angle,UE_SMALL_NUMBER))).GetNormalized();
                StyleRotation=(FQuat::Slerp(FQuat::Identity,Delta,Hints.Weight*Drive.Weight*Debug.AnimationHints)*StyleRotation).GetNormalized();
            }
            Local.SetRotation(FQuat::Slerp(Local.GetRotation(),StyleRotation,Alpha*Debug.BasePose).GetNormalized());
        }
        Pose[I]=Local*(Parent==INDEX_NONE?MeshWorld:Pose[Parent]);
    }
    if (Drive.bUseFootTargets)
        for (int32 I=0; I<Pose.Num(); ++I) if (DescendsFrom(Rig,I,Rig.Pelvis)) Pose[I].AddToTranslation(Drive.PelvisOffset);
    if (bAuthored && CorrectionAlpha<=UE_SMALL_NUMBER && State.LoadResponse.IsNearlyZero(0.001) && Drive.StartPoseBlend<=UE_SMALL_NUMBER)
    {
        FCombatBodySolverDiagnostics Diagnostics;
        for (int32 Side=0; Side<2; ++Side)
            if (Debug.FootRestore>0 && Drive.bUseFootTargets && !Pose[Rig.Feet[Side]].Equals(Drive.FootTargets[Side],0.0001)
                && !RestoreFoot(Rig,Pose,Side,Drive.FootTargets[Side],ActualRootWorld,Diagnostics.bFootReachClamped)) return false;
        FCombatFinalPose Final;
        Final.SimulationTime=Intent.SimulationTime; Final.ActualRootWorld=ActualRootWorld;
        Final.PrimaryGripWorld=Rig.GripLocal*Pose[Rig.GripBone];
        Final.PrimaryGripWorld.SetScale3D(FVector::OneVector); Final.bHasPrimaryGrip=true;
        const double Length=FVector::Distance(Pose[Rig.Shoulder].GetLocation(),Pose[Rig.Elbow].GetLocation())+
            FVector::Distance(Pose[Rig.Elbow].GetLocation(),Pose[Rig.Hand].GetLocation());
        Diagnostics.PrimaryArmExtension=FVector::Distance(Pose[Rig.Shoulder].GetLocation(),Pose[Rig.Hand].GetLocation())/FMath::Max(1.0,Length);
        for (int32 Bone=0; Bone<Pose.Num(); ++Bone) Final.BonesWorld.Add(Rig.Names[Bone],Pose[Bone]);
        Plan.PrimaryGripGoal=Final.PrimaryGripWorld.GetRelativeTransform(ActualRootWorld);
        State.PelvisSpring=0; State.PelvisSpringVelocity=0;
        OutPlan=MoveTemp(Plan); OutPose=MoveTemp(Final); OutDiagnostics=Diagnostics;
        return true;
    }
    const FTransform FootTargets[2]={Drive.bUseFootTargets?Drive.FootTargets[0]:LiveComponentPose[Rig.Feet[0]]*MeshWorld,
        Drive.bUseFootTargets?Drive.FootTargets[1]:LiveComponentPose[Rig.Feet[1]]*MeshWorld};
    // Support height is independent of attack completion/weapon settling. Never accumulate
    // a low source pelvis into the next attack. Deliberate crouching/airborne poses opt out.
    const double ProceduralCompression=-FMath::Min(10.0, double(Drive.Weight*(1-Drive.FollowThrough)*(4+Drive.WeaponLoad*2)));
    const double TargetCompression=Drive.bRecoverStandingPosture?
        FMath::Lerp(ProceduralCompression,FMath::Clamp(Drive.AuthoredPelvisOffset.Z,-10.0,5.0),MotionWeight):0;
    double Remaining=FMath::Clamp(double(DeltaSeconds),0.0,0.1);
    while (Remaining>0)
    {
        const double Dt=FMath::Min(Remaining,1.0/120.0); Remaining-=Dt;
        State.PelvisSpringVelocity+=(180*(TargetCompression-State.PelvisSpring)-20*State.PelvisSpringVelocity)*Dt;
        State.PelvisSpring+=State.PelvisSpringVelocity*Dt;
    }
    if (Drive.bRecoverStandingPosture && Debug.StandingRecovery>0)
    {
        const double FeetZ=FMath::Min(FootTargets[0].GetLocation().Z,FootTargets[1].GetLocation().Z);
        const double Height=Rig.StandingPelvisHeight*FMath::Abs(MeshWorld.GetScale3D().Z);
        const double TargetZ=FeetZ+Height*0.96+State.PelvisSpring;
        const double Shift=(TargetZ-Pose[Rig.Pelvis].GetLocation().Z)*Alpha*Debug.StandingRecovery;
        for (int32 I=0; I<Pose.Num(); ++I)
            if (DescendsFrom(Rig,I,Rig.Pelvis)) Pose[I].AddToTranslation(FVector(0,0,Shift));
    }
    const FVector AuthoredArmRay=(Pose[Rig.Hand].GetLocation()-Pose[Rig.Shoulder].GetLocation()).GetSafeNormal();
    RotateChain(Rig,Pose,Rig.Pelvis,InWorld(RootRotation,Plan.PelvisDelta.GetRotation()));
    const FQuat SpinePart=FQuat::Slerp(FQuat::Identity,Plan.TorsoRotation.Quaternion(),1.0/Rig.Spine.Num()).GetNormalized();
    for (int32 Bone:Rig.Spine) RotateChain(Rig,Pose,Bone,InWorld(RootRotation,SpinePart));
    RotateChain(Rig,Pose,Rig.Clavicle,InWorld(RootRotation,Plan.ClavicleRotation.Quaternion()));
    if (Style.bTwoHanded) RotateChain(Rig,Pose,Rig.SupportClavicle,InWorld(RootRotation,Plan.ClavicleRotation.Quaternion()));

    if (Debug.CoreLimits>0 && MotionWeight>0 && CorrectionAlpha>0 && !bAuthored)
    {
        // Bound the composed source + procedural correction, not each layer separately.
        // Cones are relative to the validated style; authored motion gets no extra budget.
        TArray<int32,TInlineAllocator<10>> Core;
        Core.Add(Rig.Pelvis); Core.Append(Rig.Spine); Core.Add(Rig.Clavicle);
        for (int32 Bone:Core)
        {
            const int32 Parent=Rig.Parents[Bone];
            const FQuat ParentRotation=Parent==INDEX_NONE?MeshWorld.GetRotation():Pose[Parent].GetRotation();
            const FQuat Local=(ParentRotation.Inverse()*Pose[Bone].GetRotation()).GetNormalized();
            const FQuat Delta=(Local*Rig.StyleLocalRotations[Bone].Inverse()).GetNormalized();
            const double Degrees=Bone==Rig.Pelvis?Limits.PelvisYawDegrees:
                Bone==Rig.Clavicle?Limits.ClavicleDegrees:
                FMath::Sqrt(FMath::Square(Limits.TorsoYawDegrees)+FMath::Square(Limits.TorsoLeanDegrees))/Rig.Spine.Num();
            const double Angle=Delta.AngularDistance(FQuat::Identity);
            const double Fraction=FMath::Min(1.0,FMath::DegreesToRadians(FMath::Max(0.0,Degrees))/FMath::Max(Angle,UE_SMALL_NUMBER));
            const FQuat Desired=(ParentRotation*FQuat::Slerp(FQuat::Identity,Delta,Fraction)*Rig.StyleLocalRotations[Bone]).GetNormalized();
            RotateChain(Rig,Pose,Bone,FQuat::Slerp(FQuat::Identity,(Desired*Pose[Bone].GetRotation().Inverse()).GetNormalized(),CorrectionAlpha).GetNormalized());
        }
    }

    // A constrained shoulder rotation carries the entire already-bent arm. No hand target IK.
    const FQuat BodyFrame=(Pose[Rig.Clavicle].GetRotation()*Rig.ClavicleAnatomicalFrame).GetNormalized();
    const FVector Residual=BodyFrame.UnrotateVector(BodyDirection);
    const double RequestedYaw=FMath::RadiansToDegrees(FMath::Atan2(Residual.Y,Residual.X));
    const double RequestedPitch=FMath::RadiansToDegrees(FMath::Atan2(Residual.Z,FVector2D(Residual.X,Residual.Y).Size()));
    const double SafeYaw=Limit(RequestedYaw,FMath::Min(Limits.ShoulderYawDegrees,85.0f));
    const double SafePitch=Limit(RequestedPitch,FMath::Min(Limits.ShoulderPitchDegrees,85.0f));
    const double ArmResponse=1-FMath::Pow(1-Response,0.5+Plan.ShoulderParticipation);
    State.ShoulderYaw=FMath::Lerp(State.ShoulderYaw,SafeYaw,ArmResponse);
    State.ShoulderPitch=FMath::Lerp(State.ShoulderPitch,SafePitch,ArmResponse);
    const FVector ArmRay=(Pose[Rig.Hand].GetLocation()-Pose[Rig.Shoulder].GetLocation()).GetSafeNormal();
    const FVector ArmDirection=BodyFrame.RotateVector(FRotator(State.ShoulderPitch,State.ShoulderYaw,0).Vector());
    FQuat ShoulderDelta=FQuat::FindBetweenNormals(ArmRay,ArmDirection);
    if (bAuthored && Drive.Weight<=0 && !Drive.bTrajectoryPlan)
    {
        // Directional guard transports the authored arm shape. Neutral aim preserves the stance.
        const FQuat AimTurn=FQuat::FindBetweenNormals(ActualRootWorld.GetUnitAxis(EAxis::X),Guard);
        ShoulderDelta=FQuat::FindBetweenNormals(ArmRay,AimTurn.RotateVector(AuthoredArmRay));
        Plan.GuardInterceptionWeight*=FMath::Clamp(float(AimTurn.AngularDistance(FQuat::Identity)/HALF_PI),0.f,1.f);
    }
    if (MotionWeight>0)
    {
        // Keep the authored shoulder/elbow relationship. Directional adaptation is a
        // bounded residual; the hard chest-relative clamp still runs below.
        const double Angle=ShoulderDelta.AngularDistance(FQuat::Identity);
        const double Budget=FMath::DegreesToRadians(FMath::Clamp(double(Drive.MotionAdaptationDegrees),0.0,90.0));
        ShoulderDelta=FQuat::Slerp(FQuat::Identity,ShoulderDelta,
            FMath::Lerp(1.0,FMath::Min(1.0,Budget/FMath::Max(Angle,UE_SMALL_NUMBER)),MotionWeight)).GetNormalized();
    }
    const FQuat ArmTurn=FQuat::Slerp(FQuat::Identity,ShoulderDelta,CorrectionAlpha*Debug.ShoulderAim).GetNormalized();
    RotateChain(Rig,Pose,Rig.Shoulder,ArmTurn);
    if (Style.bTwoHanded) RotateChain(Rig,Pose,Rig.SupportShoulder,ArmTurn);

    FVector Upper=Pose[Rig.Elbow].GetLocation()-Pose[Rig.Shoulder].GetLocation();
    FVector Lower=Pose[Rig.Hand].GetLocation()-Pose[Rig.Elbow].GetLocation();
    const double L1=Upper.Size(),L2=Lower.Size();
    if (L1<1 || L2<1) return false;
    const double MaxReach=(L1+L2)*FMath::Clamp(Limits.MaximumArmExtension,0.2f,0.98f);
    const double MinReach=FMath::Max(FMath::Abs(L1-L2)+0.01,(L1+L2)*FMath::Clamp(Limits.MinimumArmExtension,0.01f,0.5f));
    const double MinFlex=FMath::Acos(FMath::Clamp((MaxReach*MaxReach-L1*L1-L2*L2)/(2*L1*L2),-1.0,1.0));
    const double MaxFlex=FMath::Acos(FMath::Clamp((MinReach*MinReach-L1*L1-L2*L2)/(2*L1*L2),-1.0,1.0));
    const double Flex=FMath::Acos(FMath::Clamp(FVector::DotProduct(Upper/L1,Lower/L2),-1.0,1.0));
    const double Reach=FMath::Clamp((L1+L2)*Drive.Extension,MinReach,MaxReach);
    const double AttackFlex=FMath::Acos(FMath::Clamp((Reach*Reach-L1*L1-L2*L2)/(2*L1*L2),-1.0,1.0));
    const double DesiredFlex=FMath::Clamp(FMath::Lerp(Flex,AttackFlex,double(Drive.Weight)*(1-MotionWeight*0.65)),MinFlex,FMath::Max(MinFlex,MaxFlex));
    FVector Hinge=FVector::CrossProduct(Upper,Lower).GetSafeNormal();
    if (Hinge.IsNearlyZero()) Hinge=FVector::CrossProduct(Upper,BodyFrame.RotateVector(Style.ElbowPoleOffset)).GetSafeNormal();
    if (Hinge.IsNearlyZero()) return false;
    RotateChain(Rig,Pose,Rig.Elbow,FQuat(Hinge,(DesiredFlex-Flex)*CorrectionAlpha*Debug.ElbowFlex));
    // Flex correction must not put the hand behind the permitted chest-relative sector.
    const FVector CorrectedRay=(Pose[Rig.Hand].GetLocation()-Pose[Rig.Shoulder].GetLocation()).GetSafeNormal();
    const FVector CorrectedLocal=BodyFrame.UnrotateVector(CorrectedRay);
    const double CorrectedYaw=FMath::RadiansToDegrees(FMath::Atan2(CorrectedLocal.Y,CorrectedLocal.X));
    const double CorrectedPitch=FMath::RadiansToDegrees(FMath::Atan2(CorrectedLocal.Z,FVector2D(CorrectedLocal.X,CorrectedLocal.Y).Size()));
    const FVector SafeRay=BodyFrame.RotateVector(FRotator(
        Limit(CorrectedPitch,FMath::Min(Limits.ShoulderPitchDegrees,85.0f)),
        Limit(CorrectedYaw,FMath::Min(Limits.ShoulderYawDegrees,85.0f)),0).Vector());
    RotateChain(Rig,Pose,Rig.Shoulder,FQuat::Slerp(FQuat::Identity,FQuat::FindBetweenNormals(CorrectedRay,SafeRay),CorrectionAlpha*Debug.ChestClamp).GetNormalized());

    // Wrist preference is evaluated only after the body and arm. Project the existing blade
    // axis into the interception plane; there is no universal 'horizontal sword' in 3D.
    const FTransform Grip=Rig.GripLocal*Pose[Rig.GripBone];
    FVector Travel=Intent.bHasIncomingTravelDirection?Intent.IncomingTravelDirectionWorld.GetSafeNormal():-Guard;
    if (Travel.IsNearlyZero() || Travel.ContainsNaN()) Travel=-Guard;
    const FVector CurrentAxis=Grip.GetUnitAxis(EAxis::X);
    FVector Preferred=FVector::VectorPlaneProject(CurrentAxis,Travel).GetSafeNormal();
    if (Preferred.IsNearlyZero()) Preferred=FVector::VectorPlaneProject(BodyFrame.GetAxisZ(),Travel).GetSafeNormal();
    if (Preferred.IsNearlyZero()) Preferred=FVector::VectorPlaneProject(BodyFrame.GetAxisY(),Travel).GetSafeNormal();
    const bool bAttacking=Drive.Weight>0;
    const bool bThrust=Intent.AttackType==EDirectionalAttack::Pierce && bAttacking;
    if (bAttacking)
    {
        // The wrist uncocks through the cut, then follows through. This is a bounded
        // anatomical rotation after torso/shoulder/elbow solving, never a weapon target.
        const float Uncock=FMath::Lerp(-0.55f,1.0f,Drive.PhaseAlpha);
        const FVector AttackAxis=bThrust?Drive.StrikeDirection:
            FQuat(Drive.SwingAxis,FMath::DegreesToRadians(55.0f)*Uncock).RotateVector(Drive.Direction);
        Preferred=FMath::Lerp(Preferred,AttackAxis,Drive.Weight).GetSafeNormal(UE_SMALL_NUMBER,CurrentAxis);
    }
    FQuat WristDelta=FQuat::FindBetweenNormals(CurrentAxis,Preferred);
    const float WristLimit=FMath::Lerp(FMath::Clamp(Limits.WristSwingDegrees,0.0f,45.0f),
        FMath::Clamp(Limits.AttackWristSwingDegrees,0.0f,90.0f),Drive.Weight);
    WristDelta=UDragonIK_Library::LimitSwing(WristDelta,CurrentAxis,WristLimit).GetNormalized();
    WristDelta=FQuat::Slerp(FQuat::Identity,WristDelta,
        FMath::Max(Plan.GuardInterceptionWeight,Drive.Weight*(1-MotionWeight*0.8))).GetNormalized();
    // Store in hand-local space so changing actor facing does not reuse a world-space correction.
    const FQuat HandRotation=Pose[Rig.Hand].GetRotation();
    const FQuat LocalCorrection=(HandRotation.Inverse()*WristDelta*HandRotation).GetNormalized();
    State.WristCorrection=FQuat::Slerp(State.WristCorrection,LocalCorrection,Response).GetNormalized();
    RotateChain(Rig,Pose,Rig.Hand,InWorld(HandRotation,FQuat::Slerp(FQuat::Identity,State.WristCorrection,CorrectionAlpha*Debug.Wrist*(Drive.bTrajectoryPlan?1-MotionWeight:1)).GetNormalized()));
    if (Drive.StartPoseBlend>0 && State.AttackStartRotations.Num()==Pose.Num())
    {
        const TArray<FTransform> Before=Pose;
        for (int32 I=0; I<Pose.Num(); ++I)
        {
            const int32 Parent=Rig.Parents[I];
            if (Parent==INDEX_NONE) continue;
            FTransform Local=Before[I].GetRelativeTransform(Before[Parent]);
            if (Rig.StyleBones[I]) Local.SetRotation(FQuat::Slerp(Local.GetRotation(),State.AttackStartRotations[I],Drive.StartPoseBlend).GetNormalized());
            Pose[I]=Local*Pose[Parent];
        }
    }
    FCombatBodySolverDiagnostics Diagnostics;
    Diagnostics.bDirectionClamped=!FMath::IsNearlyEqual(SafeYaw,RequestedYaw,0.1) || !FMath::IsNearlyEqual(SafePitch,RequestedPitch,0.1);
    for (int32 Side=0; Side<2; ++Side)
        if (Debug.FootRestore>0 && !RestoreFoot(Rig,Pose,Side,FootTargets[Side],ActualRootWorld,Diagnostics.bFootReachClamped)) return false;
    Diagnostics.PrimaryArmExtension=FVector::Distance(Pose[Rig.Hand].GetLocation(),Pose[Rig.Shoulder].GetLocation())/(L1+L2);
    FCombatFinalPose Final;
    Final.SimulationTime=Intent.SimulationTime;
    Final.ActualRootWorld=ActualRootWorld;
    Final.PrimaryGripWorld=Rig.GripLocal*Pose[Rig.GripBone];
    Final.PrimaryGripWorld.SetScale3D(FVector::OneVector);
    Final.bHasPrimaryGrip=true;
    for (int32 I=0; I<Pose.Num(); ++I)
    {
        if (Pose[I].ContainsNaN()) return false;
        Final.BonesWorld.Add(Rig.Names[I],Pose[I]);
    }
    Plan.PreferredBladeAxisWorld=Preferred;
    Plan.GuardCoveragePointWorld=Final.PrimaryGripWorld.GetLocation();
    Plan.PrimaryGripGoal=Final.PrimaryGripWorld.GetRelativeTransform(ActualRootWorld);
    OutPlan=MoveTemp(Plan);
    OutPose=MoveTemp(Final);
    OutDiagnostics=Diagnostics;
    return true;
}
