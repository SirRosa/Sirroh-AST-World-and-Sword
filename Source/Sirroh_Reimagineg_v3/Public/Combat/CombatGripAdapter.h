#pragma once

#include "Combat/CombatMotionTypes.h"
#include "Combat/CombatWeaponDefinition.h"

/** Stateless boundary after body solving. No cursor, IK, actor movement or legacy trajectory access. */
namespace CombatGripAdapter
{
    /** Reject incompatible solved grip pairs instead of pulling the support arm toward the weapon.
     * Rigid unit-scale frames only; generated dimensions already include weapon pixel scale. */
    SIRROH_REIMAGINEG_V3_API bool TryBuildWeaponTarget(const FCombatFinalPose& Pose,
        const FCombatWeaponDefinition& Weapon, bool bTwoHanded, FTransform& OutWeaponWorld,
        float GripToleranceCm = 2, float GripToleranceDegrees = 10);

    /** Measurement only. Previous samples may not bridge teleport/equipment/epoch boundaries. */
    SIRROH_REIMAGINEG_V3_API bool TryMeasureWeapon(const FTransform& ActualWeaponWorld,
        const FCombatWeaponDefinition& Weapon, double Time, uint32 Step, uint32 Epoch,
        uint32 AttackId, bool bAttackActive, bool bContactEnabled,
        const FCombatWeaponSample* Previous, FCombatWeaponSample& OutSample);
}
