#include "Combat/CombatBodySolver.h"
#include "Combat/CombatGripAdapter.h"
#include "DragonIK_Library.h"

namespace
{
    bool Descendant(const FCombatBodyRig& Rig,int32 Bone,int32 Parent)
    {
        for (int32 I=Bone; I!=INDEX_NONE; I=Rig.Parents[I]) if (I==Parent) return true;
        return false;
    }

    void Rotate(const FCombatBodyRig& Rig,TArray<FTransform>& Pose,int32 Bone,const FQuat& Delta)
    {
        const FVector Pivot=Pose[Bone].GetLocation();
        for (int32 I=Bone; I<Pose.Num(); ++I) if (Descendant(Rig,I,Bone))
        {
            Pose[I].SetLocation(Pivot+Delta.RotateVector(Pose[I].GetLocation()-Pivot));
            Pose[I].SetRotation((Delta*Pose[I].GetRotation()).GetNormalized());
        }
    }

    FQuat Cone(const FQuat& Rotation,double Degrees)
    {
        FQuat Delta=Rotation.GetNormalized();
        if (Delta.W<0) Delta=Delta*-1;
        return FQuat::Slerp(FQuat::Identity,Delta,FMath::Min(1.0,
            FMath::DegreesToRadians(FMath::Max(0.0,Degrees))/FMath::Max(Delta.GetAngle(),UE_SMALL_NUMBER))).GetNormalized();
    }

    FTransform MeanFrame(const FTransform& A,const FTransform& B)
    {
        return FTransform(FQuat::Slerp(A.GetRotation(),B.GetRotation(),0.5).GetNormalized(),
            (A.GetLocation()+B.GetLocation())*0.5);
    }

    struct FArm
    {
        int32 Clavicle, Shoulder, Elbow, Hand, GripBone;
        FTransform GripInHand;
        FTransform WeaponGrip;
        FVector PoleOffset;
    };

    bool ElbowClear(const FCombatBodyRig& Rig,const TArray<FTransform>& Pose,int32 Elbow)
    {
        const FVector Right=Pose[Rig.Shoulder].GetLocation(),Left=Pose[Rig.SupportShoulder].GetLocation();
        const FVector Chest=(Right+Left)*0.5;
        const FVector Centre=FMath::ClosestPointOnSegment(Pose[Elbow].GetLocation(),Pose[Rig.Pelvis].GetLocation(),Chest);
        // Conservative torso + elbow clearance derived from this rig, not mannequin cm.
        const double Radius=FVector::Distance(Right,Left)*0.30;
        if (FVector::DistSquared(Pose[Elbow].GetLocation(),Centre)<Radius*Radius) return false;
        const int32 Head=Rig.Names.IndexOfByKey(FName(TEXT("head")));
        if (Head!=INDEX_NONE)
        {
            const FVector Neck=FMath::ClosestPointOnSegment(Pose[Elbow].GetLocation(),Chest,Pose[Head].GetLocation());
            const double NeckRadius=FVector::Distance(Right,Left)*0.23;
            if (FVector::DistSquared(Pose[Elbow].GetLocation(),Neck)<NeckRadius*NeckRadius) return false;
        }
        return true;
    }

    // Project one arm onto its feasible set, preserving the source elbow plane and bone lengths.
    // The outer solve changes BOTH arms and their shared body/frame to close the residual.
    bool ProjectArm(const FCombatBodyRig& Rig,const FCombatStyleDefinition& Style,
        const TArray<FTransform>& Source,TArray<FTransform>& Pose,const FArm& Arm,
        const FTransform& Frame,const FQuat& BodyFrame)
    {
        const FTransform GoalGrip=Arm.WeaponGrip*Frame;
        const FTransform Goal=Arm.GripInHand.Inverse()*GoalGrip;
        const auto& Limits=Style.Anatomy;
        const FVector Pivot=Pose[Arm.Clavicle].GetLocation();
        const FVector From=(Pose[Arm.Hand].GetLocation()-Pivot).GetSafeNormal();
        const FVector To=(Goal.GetLocation()-Pivot).GetSafeNormal();
        if (From.IsNearlyZero() || To.IsNearlyZero()) return false;
        Rotate(Rig,Pose,Arm.Clavicle,Cone(FQuat::FindBetweenNormals(From,To),Limits.ClavicleDegrees));

        const FTransform OldA=Pose[Arm.Shoulder],OldB=Pose[Arm.Elbow],OldC=Pose[Arm.Hand];
        const double L1=FVector::Distance(OldA.GetLocation(),OldB.GetLocation());
        const double L2=FVector::Distance(OldB.GetLocation(),OldC.GetLocation());
        if (L1<1 || L2<1) return false;
        const FVector SourceRay=(OldC.GetLocation()-OldA.GetLocation()).GetSafeNormal();
        const FVector Requested=Goal.GetLocation()-OldA.GetLocation();
        // Limits apply to correction of the already planned biomechanical pose. An overhead
        // source is not forced into a world-forward cone, nor given permission to stretch.
        const FQuat DirectionDelta=Cone(FQuat::FindBetweenNormals(SourceRay,
            Requested.GetSafeNormal(UE_SMALL_NUMBER,SourceRay)),
            FMath::Min(Limits.ShoulderYawDegrees,Limits.ShoulderPitchDegrees));
        const double Minimum=FMath::Max(FMath::Abs(L1-L2)+0.01,(L1+L2)*Limits.MinimumArmExtension);
        const double Maximum=(L1+L2)*FMath::Min(0.98f,Limits.MaximumArmExtension);
        if (Minimum>=Maximum) return false;
        const FVector End=OldA.GetLocation()+DirectionDelta.RotateVector(SourceRay)*
            FMath::Clamp(Requested.Size(),Minimum,Maximum);
        FVector Pole=FVector::VectorPlaneProject(OldB.GetLocation()-OldA.GetLocation(),SourceRay);
        if (Pole.SizeSquared()<0.01) Pole=FVector::VectorPlaneProject(BodyFrame.RotateVector(Arm.PoleOffset),SourceRay);
        if (Pole.IsNearlyZero()) return false;
        Pole=OldA.GetLocation()+DirectionDelta.RotateVector(Pole.GetSafeNormal())*(L1+L2);
        UDragonIK_Library::Solve_Modified_Direct_TwoBoneIK_3(FTransform::Identity,0,
            Pose[Arm.Shoulder],Pose[Arm.Elbow],Pose[Arm.Hand],Pole,End,L1,L2,false,1,1,0.5f,false);
        if (!ElbowClear(Rig,Pose,Arm.Elbow))
        {
            // Reorient the bend plane around the same shoulder/hand ray. Do not pull
            // a hand off the grip or translate an elbow through the chest.
            FVector Outward=BodyFrame.GetAxisY()*(Arm.Shoulder==Rig.Shoulder?1.0:-1.0)+BodyFrame.GetAxisX()*0.35;
            Outward=FVector::VectorPlaneProject(Outward,(End-OldA.GetLocation()).GetSafeNormal()).GetSafeNormal();
            if (Outward.IsNearlyZero()) return false;
            Pose[Arm.Shoulder]=OldA; Pose[Arm.Elbow]=OldB; Pose[Arm.Hand]=OldC;
            UDragonIK_Library::Solve_Modified_Direct_TwoBoneIK_3(FTransform::Identity,0,
                Pose[Arm.Shoulder],Pose[Arm.Elbow],Pose[Arm.Hand],OldA.GetLocation()+Outward*(L1+L2),
                End,L1,L2,false,1,1,0.5f,false);
            if (!ElbowClear(Rig,Pose,Arm.Elbow)) return false;
        }

        // Bound the wrist relative to its forearm, not relative to a stale world orientation.
        const FQuat SourceWrist=(Source[Arm.Elbow].GetRotation().Inverse()*Source[Arm.Hand].GetRotation()).GetNormalized();
        const FQuat BaseWrist=(Pose[Arm.Elbow].GetRotation()*SourceWrist).GetNormalized();
        const FVector TwistAxis=BaseWrist.UnrotateVector((Pose[Arm.Hand].GetLocation()-Pose[Arm.Elbow].GetLocation()).GetSafeNormal());
        const FQuat LocalDelta=(BaseWrist.Inverse()*Goal.GetRotation()).GetNormalized();
        FQuat Swing,Twist;
        LocalDelta.ToSwingTwist(TwistAxis,Swing,Twist);
        Pose[Arm.Hand].SetRotation((BaseWrist*Cone(Swing,Limits.AttackWristSwingDegrees)*
            Cone(Twist,Limits.WristTwistDegrees)).GetNormalized());

        // Auxiliary grip, twist and finger branches follow their nearest solved ancestor.
        for (int32 I=0; I<Pose.Num(); ++I)
        {
            if (I==Arm.Shoulder || I==Arm.Elbow || I==Arm.Hand) continue;
            int32 Parent=Rig.Parents[I];
            while (Parent!=INDEX_NONE && Parent!=Arm.Shoulder && Parent!=Arm.Elbow && Parent!=Arm.Hand) Parent=Rig.Parents[Parent];
            if (Parent!=INDEX_NONE)
                Pose[I]=Pose[I].GetRelativeTransform(Parent==Arm.Shoulder?OldA:Parent==Arm.Elbow?OldB:OldC)*Pose[Parent];
        }
        return !Pose[Arm.Hand].ContainsNaN();
    }
}

bool CombatBodySolver::CloseGripPair(const FCombatBodyRig& Rig,const FCombatStyleDefinition& Style,
    const FCombatWeaponDefinition& Weapon,FCombatFinalPose& Pose,FCombatBodySolverDiagnostics& Diagnostics,const FVector* PreviousElbows)
{
    if (Style.DebugStages.GripPair<=0) { Pose.bHasSecondaryGrip=false; return true; }
    if (!Style.bTwoHanded) return true;
    if (!Weapon.bHasSecondaryGrip || Rig.SupportGripBone==INDEX_NONE || Rig.Spine.IsEmpty()
        || Weapon.PrimaryGripLocal.ContainsNaN() || Weapon.SecondaryGripLocal.ContainsNaN()
        || !Weapon.PrimaryGripLocal.GetScale3D().Equals(FVector::OneVector)
        || !Weapon.SecondaryGripLocal.GetScale3D().Equals(FVector::OneVector)
        || FVector::DistSquared(Weapon.PrimaryGripLocal.GetLocation(),Weapon.SecondaryGripLocal.GetLocation())<1) return false;
    TArray<FTransform> Source;
    Source.Reserve(Rig.Names.Num());
    for (FName Name:Rig.Names)
    {
        const FTransform* Bone=Pose.BonesWorld.Find(Name);
        if (!Bone || Bone->ContainsNaN()) return false;
        Source.Add(*Bone);
    }
    const FTransform GripOffsets[2]={Rig.GripLocal,Rig.SupportGripLocal};
    const FArm Arms[2]={
        {Rig.Clavicle,Rig.Shoulder,Rig.Elbow,Rig.Hand,Rig.GripBone,
            (Rig.GripLocal*Source[Rig.GripBone]).GetRelativeTransform(Source[Rig.Hand]),
            Weapon.PrimaryGripLocal,Style.ElbowPoleOffset},
        {Rig.SupportClavicle,Rig.SupportShoulder,Rig.SupportElbow,Rig.SupportHand,Rig.SupportGripBone,
            (Rig.SupportGripLocal*Source[Rig.SupportGripBone]).GetRelativeTransform(Source[Rig.SupportHand]),
            Weapon.SecondaryGripLocal,Style.ElbowPoleOffset*FVector(1,-1,1)}};
    const auto ImpliedFrame=[&](const TArray<FTransform>& Bones,int32 Side)
    {
        FTransform Grip=GripOffsets[Side]*Bones[Arms[Side].GripBone];
        Grip.SetScale3D(FVector::OneVector);
        return Arms[Side].WeaponGrip.Inverse()*Grip;
    };
    const FTransform OriginalFrames[2]={ImpliedFrame(Source,0),ImpliedFrame(Source,1)};
    // Preserve a compatible authored pose exactly; even a numerically equivalent IK
    // pass can change elbow roll/twist. Terrain and contact have already been applied.
    if (OriginalFrames[0].Equals(OriginalFrames[1],0.0001))
    {
        bool bWithinReach=true;
        for (int32 Side=0; Side<2; ++Side)
        {
            const auto& A=Arms[Side];
            const double Length=FVector::Distance(Source[A.Shoulder].GetLocation(),Source[A.Elbow].GetLocation())+
                FVector::Distance(Source[A.Elbow].GetLocation(),Source[A.Hand].GetLocation());
            const double Extension=FVector::Distance(Source[A.Shoulder].GetLocation(),Source[A.Hand].GetLocation())/FMath::Max(1.0,Length);
            bWithinReach&=Length>2 && Extension>=Style.Anatomy.MinimumArmExtension && Extension<=Style.Anatomy.MaximumArmExtension;
            (Side==0?Diagnostics.PrimaryArmExtension:Diagnostics.SecondaryArmExtension)=Extension;
        }
        if (bWithinReach && ElbowClear(Rig,Source,Rig.Elbow) && ElbowClear(Rig,Source,Rig.SupportElbow))
        {
            Pose.PrimaryGripWorld=Rig.GripLocal*Source[Rig.GripBone];
            Pose.SecondaryGripWorld=Rig.SupportGripLocal*Source[Rig.SupportGripBone];
            Pose.PrimaryGripWorld.SetScale3D(FVector::OneVector);
            Pose.SecondaryGripWorld.SetScale3D(FVector::OneVector);
            Pose.bHasPrimaryGrip=true; Pose.bHasSecondaryGrip=true;
            return true;
        }
    }
    const FTransform Preference=MeanFrame(OriginalFrames[0],OriginalFrames[1]);
    const FQuat BodyFrame=Pose.ActualRootWorld.GetRotation();
    TArray<FTransform> Best;
    double BestCost=TNumericLimits<double>::Max();
    double BestPositionError=0,BestAngleError=0,BestCorrection=0;

    // No movement of capsule or support feet here. Candidate chest rotations carry both
    // shoulders. They are evaluated from the SAME source so budgets cannot accumulate.
    const double PitchBudget=FMath::Min(12.0,double(Style.Anatomy.TorsoLeanDegrees));
    const double YawBudget=FMath::Min(12.0,double(Style.Anatomy.TorsoYawDegrees));
    const FRotator BodyCandidates[]={FRotator::ZeroRotator,FRotator(PitchBudget,0,0),
        FRotator(-PitchBudget,0,0),FRotator(0,YawBudget,0),FRotator(0,-YawBudget,0)};
    for (const FRotator& Body:BodyCandidates)
    {
        TArray<FTransform> BodyPose=Source;
        const FQuat Part=FQuat::Slerp(FQuat::Identity,Body.Quaternion(),1.0/Rig.Spine.Num()).GetNormalized();
        for (int32 Bone:Rig.Spine) Rotate(Rig,BodyPose,Bone,(BodyFrame*Part*BodyFrame.Inverse()).GetNormalized());
        FTransform Frame=Preference;
        for (int32 Iteration=0; Iteration<16; ++Iteration)
        {
            TArray<FTransform> Candidate=BodyPose;
            if (!ProjectArm(Rig,Style,BodyPose,Candidate,Arms[0],Frame,BodyFrame)
                || !ProjectArm(Rig,Style,BodyPose,Candidate,Arms[1],Frame,BodyFrame)) break;
            const FTransform A=ImpliedFrame(Candidate,0),B=ImpliedFrame(Candidate,1);
            const double PositionError=FVector::Distance(A.GetLocation(),B.GetLocation());
            const double AngleError=FMath::RadiansToDegrees(A.GetRotation().AngularDistance(B.GetRotation()));
            double Deviation=0,Correction=0;
            for (const FArm& Arm:Arms)
            {
                const double Distance=FVector::Distance(Source[Arm.Hand].GetLocation(),Candidate[Arm.Hand].GetLocation());
                Deviation+=Distance*Distance;
                Correction=FMath::Max(Correction,Distance);
            }
            // Closure is a constraint, not a soft target that can be outvoted by accuracy.
            // Both contacts are much tighter than the adapter's final rejection threshold.
            if (PositionError<=0.25 && AngleError<=1
                && ElbowClear(Rig,Candidate,Rig.Elbow) && ElbowClear(Rig,Candidate,Rig.SupportElbow))
            {
                double Cost=Deviation+4*(Body.Pitch*Body.Pitch+Body.Yaw*Body.Yaw);
                if (PreviousElbows)
                    for (int32 Side=0;Side<2;++Side)
                        Cost+=0.35*FVector::DistSquared(Pose.ActualRootWorld.InverseTransformPosition(Candidate[Arms[Side].Elbow].GetLocation()),PreviousElbows[Side]);
                if (Cost<BestCost)
                {
                    BestCost=Cost; Best=MoveTemp(Candidate);
                    BestPositionError=PositionError; BestAngleError=AngleError; BestCorrection=Correction;
                }
                break;
            }
            const FTransform Projected=MeanFrame(A,B);
            if (Frame.Equals(Projected,0.00001)) break;
            Frame=Projected;
        }
        if (BestCost<0.0001) break;
    }
    if (Best.IsEmpty()) return false;
    FCombatFinalPose Result=Pose;
    Result.PrimaryGripWorld=Rig.GripLocal*Best[Rig.GripBone];
    Result.SecondaryGripWorld=Rig.SupportGripLocal*Best[Rig.SupportGripBone];
    Result.PrimaryGripWorld.SetScale3D(FVector::OneVector);
    Result.SecondaryGripWorld.SetScale3D(FVector::OneVector);
    Result.bHasPrimaryGrip=true; Result.bHasSecondaryGrip=true;
    FTransform Frame;
    if (!CombatGripAdapter::TryBuildWeaponTarget(Result,Weapon,true,Frame)) return false;
    for (int32 I=0; I<Best.Num(); ++I)
    {
        if (Best[I].ContainsNaN()) return false;
        Result.BonesWorld[Rig.Names[I]]=Best[I];
    }
    Diagnostics.GripClosureErrorCm=BestPositionError;
    Diagnostics.GripClosureErrorDegrees=BestAngleError;
    Diagnostics.GripCorrectionCm=BestCorrection;
    for (int32 Side=0; Side<2; ++Side)
    {
        const auto& A=Arms[Side];
        const double Length=FVector::Distance(Best[A.Shoulder].GetLocation(),Best[A.Elbow].GetLocation())+
            FVector::Distance(Best[A.Elbow].GetLocation(),Best[A.Hand].GetLocation());
        const double Extension=FVector::Distance(Best[A.Shoulder].GetLocation(),Best[A.Hand].GetLocation())/Length;
        (Side==0?Diagnostics.PrimaryArmExtension:Diagnostics.SecondaryArmExtension)=Extension;
    }
    Pose=MoveTemp(Result);
    return true;
}

bool CombatBodySolver::ProjectContinuousPose(const FCombatBodyRig& Rig,const FCombatStyleDefinition& Style,
    const FCombatWeaponDefinition& Weapon,const FCombatFinalPose& Previous,const FTransform& RootWorld,
    FCombatFinalPose& Requested,FCombatBodySolverDiagnostics& Diagnostics)
{
    if (!Previous.bHasPrimaryGrip || Previous.BonesWorld.Num()!=Rig.Names.Num()) return false;
    const bool Pair=Style.bTwoHanded && (Requested.bHasSecondaryGrip || Previous.bHasSecondaryGrip);
    FCombatFinalPose Target=Requested;
    const bool bHasTarget=Target.BonesWorld.Num()==Rig.Names.Num();
    if (!bHasTarget)
    {
        Target=Previous;
        Target.SimulationTime=Requested.SimulationTime;
        for (auto& Bone:Target.BonesWorld)
            Bone.Value=Bone.Value.GetRelativeTransform(Previous.ActualRootWorld)*RootWorld;
    }
    Target.ActualRootWorld=RootWorld;
    // Backtracking is bounded and keeps the currently solved support/pelvis. At zero
    // the previous feasible upper body is transported by the actual root/pelvis, not
    // rendered at yesterday's world position. No reset, legacy pose or weapon teleport.
    // Try the unmodified pose first: safe authored motion retains its original clock.
    // Backtrack only discontinuities, with enough resolution not to freeze small joints.
    for (float Fraction:{1.f,0.5f,0.25f,0.125f,0.0625f,0.03125f,0.015625f,0.0078125f,0.f})
    {
        if (Fraction==0)
        {
            // Preserve ALL local transforms on failure, not target translations
            // combined with old rotations. That mixture can stretch limbs and fail
            // the very fallback intended to prevent alternating/reset poses.
            FCombatFinalPose Held=Previous;
            Held.ActualRootWorld=RootWorld;
            Held.SimulationTime=Target.SimulationTime;
            Held.bHasSecondaryGrip=Style.bTwoHanded && Previous.bHasSecondaryGrip;
            for (auto& Bone:Held.BonesWorld)
                Bone.Value=Bone.Value.GetRelativeTransform(Previous.ActualRootWorld)*RootWorld;
            if (!ConstrainAnatomy(Rig,Held)) return false;
            FTransform HeldFrame;
            if (!CombatGripAdapter::TryBuildWeaponTarget(Held,Weapon,Held.bHasSecondaryGrip,HeldFrame)) return false;
            Requested=MoveTemp(Held);
            Diagnostics.bPoseLimited=true;
            return true;
        }
        FCombatFinalPose Candidate=Target;
        if (!Rig.Names.IsEmpty())
        {
            const FTransform OldRoot=Previous.BonesWorld.FindChecked(Rig.Names[0]).GetRelativeTransform(Previous.ActualRootWorld);
            FTransform NewRoot=Target.BonesWorld.FindChecked(Rig.Names[0]).GetRelativeTransform(RootWorld);
            NewRoot.Blend(OldRoot,NewRoot,Fraction);
            Candidate.BonesWorld[Rig.Names[0]]=NewRoot*RootWorld;
        }
        for (int32 I=0; I<Rig.Names.Num(); ++I)
        {
            const int32 Parent=Rig.Parents[I];
            if (Parent==INDEX_NONE) continue;
            const FName Name=Rig.Names[I],ParentName=Rig.Names[Parent];
            const FTransform* Old=Previous.BonesWorld.Find(Name);
            const FTransform* OldParent=Previous.BonesWorld.Find(ParentName);
            const FTransform* New=Target.BonesWorld.Find(Name);
            const FTransform* NewParent=Target.BonesWorld.Find(ParentName);
            if (!Old || !OldParent || !New || !NewParent) return false;
            const FTransform OldLocal=Old->GetRelativeTransform(*OldParent);
            FTransform Local=New->GetRelativeTransform(*NewParent);
            Local.SetRotation(FQuat::Slerp(OldLocal.GetRotation(),Local.GetRotation(),Fraction).GetNormalized());
            // Only rotate bones. Translation interpolation could change limb lengths.
            Local.SetLocation(I==Rig.Pelvis?FMath::Lerp(OldLocal.GetLocation(),Local.GetLocation(),Fraction):Local.GetLocation());
            Candidate.BonesWorld[Name]=Local*Candidate.BonesWorld.FindChecked(ParentName);
        }
        Candidate.PrimaryGripWorld=Rig.GripLocal*Candidate.BonesWorld.FindChecked(Rig.Names[Rig.GripBone]);
        if (Pair) Candidate.SecondaryGripWorld=Rig.SupportGripLocal*Candidate.BonesWorld.FindChecked(Rig.Names[Rig.SupportGripBone]);
        Candidate.PrimaryGripWorld.SetScale3D(FVector::OneVector);
        Candidate.SecondaryGripWorld.SetScale3D(FVector::OneVector);
        Candidate.bHasPrimaryGrip=true; Candidate.bHasSecondaryGrip=Pair;
        FTransform Frame;
        if (Pair && Fraction>0 && !CombatGripAdapter::TryBuildWeaponTarget(Candidate,Weapon,true,Frame)
            && !CloseGripPair(Rig,Style,Weapon,Candidate,Diagnostics)) continue;
        if (!ConstrainAnatomy(Rig,Candidate)) continue;
        if (!CombatGripAdapter::TryBuildWeaponTarget(Candidate,Weapon,Pair,Frame)) continue;
        // Validate AFTER grip closure. A late IK pass must not defeat the rate limit.
        const double Dt=FMath::Clamp(Target.SimulationTime-Previous.SimulationTime,0.001,0.1);
        bool Continuous=true;
        for (int32 I=0; I<Rig.Names.Num(); ++I)
        {
            const int32 Parent=Rig.Parents[I];
            if (Parent==INDEX_NONE)
            {
                const FQuat Old=Previous.BonesWorld.FindChecked(Rig.Names[I]).GetRelativeTransform(Previous.ActualRootWorld).GetRotation();
                const FQuat Next=Candidate.BonesWorld.FindChecked(Rig.Names[I]).GetRelativeTransform(RootWorld).GetRotation();
                if (Old.AngularDistance(Next)>FMath::DegreesToRadians(360.)*Dt+0.001) { Continuous=false; break; }
                continue;
            }
            const FName Name=Rig.Names[I],ParentName=Rig.Names[Parent];
            const FTransform Old=Previous.BonesWorld.FindChecked(Name).GetRelativeTransform(Previous.BonesWorld.FindChecked(ParentName));
            const FTransform Next=Candidate.BonesWorld.FindChecked(Name).GetRelativeTransform(Candidate.BonesWorld.FindChecked(ParentName));
            const FString Bone=Name.ToString();
            const double Rate=I==Rig.Pelvis?270:Bone.StartsWith(TEXT("spine"))?180:Bone.StartsWith(TEXT("clavicle"))?360:
                Bone.StartsWith(TEXT("upperarm"))?720:Bone.StartsWith(TEXT("lowerarm"))?900:
                Bone.StartsWith(TEXT("hand"))?900:Bone.StartsWith(TEXT("neck")) || Bone==TEXT("head")?240:720;
            if (Old.GetRotation().AngularDistance(Next.GetRotation())>FMath::DegreesToRadians(Rate)*Dt+0.001
                || (I==Rig.Pelvis && FVector::Distance(Old.GetLocation(),Next.GetLocation())>180*Dt+0.01)) { Continuous=false; break; }
            // Local angular bounds alone multiply along a chain. Also bound the
            // resulting body-relative travel, excluding legitimate Mover translation.
            const FVector OldPosition=Previous.ActualRootWorld.InverseTransformPosition(Previous.BonesWorld.FindChecked(Name).GetLocation());
            const FVector NewPosition=RootWorld.InverseTransformPosition(Candidate.BonesWorld.FindChecked(Name).GetLocation());
            const double Speed=Rig.Spine.Contains(I)?450.0:1600.0;
            if (FVector::Distance(OldPosition,NewPosition)>Speed*Dt+0.01) { Continuous=false; break; }
        }
        if (Continuous && !Rig.Spine.IsEmpty())
        {
            const FName Chest=Rig.Names[Rig.Spine.Last()],Pelvis=Rig.Names[Rig.Pelvis];
            const FQuat OldTorso=Previous.BonesWorld.FindChecked(Chest).GetRelativeTransform(
                Previous.BonesWorld.FindChecked(Pelvis)).GetRotation();
            const FQuat NewTorso=Candidate.BonesWorld.FindChecked(Chest).GetRelativeTransform(
                Candidate.BonesWorld.FindChecked(Pelvis)).GetRotation();
            // Five individually safe spine joints must not grant five times the
            // angular speed to the chest, particularly during axial reversals.
            Continuous=OldTorso.AngularDistance(NewTorso)<=FMath::DegreesToRadians(240.0)*Dt+0.001;
        }
        if (!Continuous) continue;
        Requested=MoveTemp(Candidate);
        Diagnostics.bPoseLimited=false;
        for (int32 I=0; I<Rig.Names.Num(); ++I) if (Rig.StyleBones[I])
        {
            const auto& A=Requested.BonesWorld.FindChecked(Rig.Names[I]);
            const auto& B=Target.BonesWorld.FindChecked(Rig.Names[I]);
            Diagnostics.bPoseLimited|=A.GetRotation().AngularDistance(B.GetRotation())>FMath::DegreesToRadians(1.0)
                || FVector::DistSquared(A.GetLocation(),B.GetLocation())>0.0625;
        }
        return true;
    }
    return false;
}
