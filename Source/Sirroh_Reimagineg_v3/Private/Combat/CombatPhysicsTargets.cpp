#include "Combat/CombatBodySolver.h"
#include "HAL/IConsoleManager.h"
namespace
{
    TAutoConsoleVariable<int32> CVarPhysicsBody(TEXT("Combat.PhysicsAssetBody"),1,
        TEXT("Use authored pose targets and native PhysicsAsset joints; 0 restores legacy IK for comparison."));
}
bool CombatBodySolver::UsesPhysicsAssetBiomechanics() { return CVarPhysicsBody.GetValueOnGameThread()!=0; }
bool CombatBodySolver::BuildPhysicsTargets(const FCombatBodyRig& Rig,const TArray<FTransform>& Live,
    const FTransform& MeshWorld,const FTransform& Root,const FCombatIntent& Intent,const FCombatBodyDrive& Drive,
    float Alpha,FCombatBodySolverState& State,FCombatFinalPose& Out)
{
    if (Live.Num()!=Rig.Names.Num() || Rig.ReferenceLocalPose.Num()!=Live.Num()) return false;
    const bool Authored=Drive.AuthoredLocalPose.Num()==Live.Num();
    TArray<FTransform> Component; Component.SetNum(Live.Num());
    for (int32 I=0;I<Live.Num();++I)
    {
        const int32 P=Rig.Parents[I];
        FTransform Local=P==INDEX_NONE?Live[I]:Live[I].GetRelativeTransform(Live[P]);
        FTransform Target=Authored?Drive.AuthoredLocalPose[I]:Rig.ReferenceLocalPose[I];
        if (!Authored && Rig.StyleLocalRotations.IsValidIndex(I)) Target.SetRotation(Rig.StyleLocalRotations[I]);
        bool Lower=I==Rig.Pelvis;
        for (int32 A=I;A!=INDEX_NONE;A=Rig.Parents[A]) if (A==Rig.Hips[0] || A==Rig.Hips[1]) { Lower=true; break; }
        const float Weight=FMath::Clamp(Alpha*(Lower?Drive.LegMotionWeight:1.f),0.f,1.f);
        if (P!=INDEX_NONE)
        {
            Local.Blend(Local,Target,Weight);
            if ((!Lower || Drive.LegMotionWeight>0) && State.AttackStartRotations.IsValidIndex(I) && Drive.StartPoseBlend>0)
                Local.SetRotation(FQuat::Slerp(Local.GetRotation(),State.AttackStartRotations[I],
                    FMath::Clamp(Drive.StartPoseBlend,0.f,1.f)).GetNormalized());
        }
        if (Local.ContainsNaN()) return false;
        Local.NormalizeRotation();
        Component[I]=Local*(P==INDEX_NONE?FTransform::Identity:Component[P]);
        Out.BonesWorld.Add(Rig.Names[I],Component[I]*MeshWorld);
    }
    // These are animation targets, not a second anatomical limiter. Chaos owns feasibility.
    if (Authored && Drive.bTrajectoryPlan && !Rig.Spine.IsEmpty())
    {
        const int32 PivotBone=Rig.Spine[0];
        const FVector Pivot=Out.BonesWorld.FindChecked(Rig.Names[PivotBone]).GetLocation();
        const FQuat Rotation=Root.GetRotation()*Drive.TrajectoryCorrection.Quaternion()*Root.GetRotation().Inverse();
        for (int32 I=0;I<Rig.Names.Num();++I)
            for (int32 A=I;A!=INDEX_NONE;A=Rig.Parents[A]) if (A==PivotBone)
            {
                auto& T=Out.BonesWorld.FindChecked(Rig.Names[I]);
                T.SetLocation(Pivot+Rotation.RotateVector(T.GetLocation()-Pivot));
                T.SetRotation((Rotation*T.GetRotation()).GetNormalized()); break;
            }
    }
    Out.ActualRootWorld=Root; Out.SimulationTime=Intent.SimulationTime;
    Out.PrimaryGripWorld=Rig.GripLocal*Out.BonesWorld.FindChecked(Rig.Names[Rig.GripBone]);
    Out.PrimaryGripWorld.SetScale3D(FVector::OneVector);
    Out.PrimaryGripWorld.NormalizeRotation();
    Out.bHasPrimaryGrip=!Out.PrimaryGripWorld.ContainsNaN();
    if (!Out.bHasPrimaryGrip) return false;
    Out.bHasSecondaryGrip=false;
    if (Rig.SupportGripBone!=INDEX_NONE)
    {
        Out.SecondaryGripWorld=Rig.SupportGripLocal*Out.BonesWorld.FindChecked(Rig.Names[Rig.SupportGripBone]);
        Out.SecondaryGripWorld.SetScale3D(FVector::OneVector);
        Out.SecondaryGripWorld.NormalizeRotation();
        Out.bHasSecondaryGrip=!Out.SecondaryGripWorld.ContainsNaN();
        if (!Out.bHasSecondaryGrip) return false;
    }
    return true;
}
