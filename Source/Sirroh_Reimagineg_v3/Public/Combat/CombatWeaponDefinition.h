#pragma once

#include "CoreMinimal.h"
#include "CombatWeaponDefinition.generated.h"

/** Anchors use actual actor-local coordinates (the current sword blade points along -Z).
 * Anchor rotation defines the grip frame, independently of the render card orientation.
 * No camera projection and no hard-coded left/right hand socket names. */
USTRUCT(BlueprintType)
struct FCombatWeaponDefinition
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 GeometryVersion = 1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FGuid WeaponInstanceId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float WeaponAngle = 1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float WeaponReach = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float WeaponPower = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float WeaponWeight = 1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FTransform PrimaryGripLocal;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FTransform SecondaryGripLocal;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bHasSecondaryGrip = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float HandleLength = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FVector BladeBaseLocal = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FVector BladeTipLocal = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float BladeRadius = 3;
    /** True until a geometry generator supplies precise blade/handle dimensions. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bApproximateGeometry = true;
};
