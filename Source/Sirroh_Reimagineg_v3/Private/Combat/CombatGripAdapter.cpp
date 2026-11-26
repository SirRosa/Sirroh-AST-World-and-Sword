#include "Combat/CombatGripAdapter.h"

namespace
{
    bool IsRigidFrame(const FTransform& Frame)
    {
        return !Frame.ContainsNaN() && Frame.GetRotation().IsNormalized()
            && Frame.GetScale3D().Equals(FVector::OneVector, KINDA_SMALL_NUMBER);
    }
}

bool CombatGripAdapter::TryBuildWeaponTarget(const FCombatFinalPose& Pose,
    const FCombatWeaponDefinition& Weapon, bool bTwoHanded, FTransform& OutWeaponWorld,
    float GripToleranceCm, float GripToleranceDegrees)
{
    if (!Pose.bHasPrimaryGrip || !IsRigidFrame(Pose.PrimaryGripWorld)
        || !IsRigidFrame(Weapon.PrimaryGripLocal)) return false;
    const FTransform Target=Weapon.PrimaryGripLocal.Inverse()*Pose.PrimaryGripWorld;
    if (!IsRigidFrame(Target)) return false;
    if (bTwoHanded)
    {
        if (!Weapon.bHasSecondaryGrip || !Pose.bHasSecondaryGrip
            || !IsRigidFrame(Pose.SecondaryGripWorld) || !IsRigidFrame(Weapon.SecondaryGripLocal)
            || !FMath::IsFinite(GripToleranceCm) || !FMath::IsFinite(GripToleranceDegrees)) return false;
        const FTransform Secondary=Weapon.SecondaryGripLocal*Target;
        if (FVector::Distance(Secondary.GetLocation(),Pose.SecondaryGripWorld.GetLocation())>FMath::Max(0.0f,GripToleranceCm)
            || FMath::RadiansToDegrees(Secondary.GetRotation().AngularDistance(Pose.SecondaryGripWorld.GetRotation()))
                >FMath::Clamp(GripToleranceDegrees,0.0f,180.0f)) return false;
    }
    OutWeaponWorld=Target;
    return true;
}

bool CombatGripAdapter::TryMeasureWeapon(const FTransform& ActualWeaponWorld,
    const FCombatWeaponDefinition& Weapon, double Time, uint32 Step, uint32 Epoch,
    uint32 AttackId, bool bAttackActive, bool bContactEnabled,
    const FCombatWeaponSample* Previous, FCombatWeaponSample& OutSample)
{
    if (!IsRigidFrame(ActualWeaponWorld) || !IsRigidFrame(Weapon.PrimaryGripLocal)
        || !Weapon.WeaponInstanceId.IsValid() || !FMath::IsFinite(Time)
        || Weapon.BladeBaseLocal.ContainsNaN() || Weapon.BladeTipLocal.ContainsNaN()
        || !FMath::IsFinite(Weapon.BladeRadius) || Weapon.BladeRadius<=0) return false;
    FCombatWeaponSample Sample;
    Sample.SimulationTime=Time;
    Sample.SimulationStep=Step;
    Sample.Epoch=Epoch;
    Sample.AttackId=AttackId;
    Sample.WeaponInstanceId=Weapon.WeaponInstanceId;
    Sample.WeaponWorld=ActualWeaponWorld;
    Sample.PrimaryGripWorld=Weapon.PrimaryGripLocal*ActualWeaponWorld;
    Sample.BladeBase=ActualWeaponWorld.TransformPosition(Weapon.BladeBaseLocal);
    Sample.BladeTip=ActualWeaponWorld.TransformPosition(Weapon.BladeTipLocal);
    Sample.BladeRadius=Weapon.BladeRadius;
    Sample.bAttackActive=bAttackActive;
    Sample.bContactEnabled=bContactEnabled;
    if (Previous && Previous->WeaponInstanceId==Sample.WeaponInstanceId && Previous->Epoch==Epoch
        && Step>Previous->SimulationStep && FMath::IsFinite(Previous->SimulationTime)
        && Time>Previous->SimulationTime+UE_SMALL_NUMBER && IsRigidFrame(Previous->WeaponWorld))
    {
        const double Dt=Time-Previous->SimulationTime;
        Sample.LinearVelocity=(ActualWeaponWorld.GetLocation()-Previous->WeaponWorld.GetLocation())/Dt;
        FQuat Delta=(ActualWeaponWorld.GetRotation()*Previous->WeaponWorld.GetRotation().Inverse()).GetNormalized();
        if (Delta.W<0) Delta=Delta*-1.0;
        FVector Axis; double Angle;
        Delta.ToAxisAndAngle(Axis,Angle);
        Sample.AngularVelocity=Axis*(Angle/Dt);
        Sample.bDiscontinuity=false;
    }
    OutSample=Sample;
    return true;
}
