#pragma once
#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Combat/CombatAttackTypes.h"
#include "CombatPoseProfile.generated.h"

class UAnimSequence;

/** Pose/handling configuration only. Gameplay statistics are intentionally external. */
USTRUCT(BlueprintType)
struct FCombatPoseProfile : public FTableRowBase
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TSoftObjectPtr<UAnimSequence> CombatStyle;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float PoseTime=0;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float BlendSeconds=0.2f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float FollowSpeed=12;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float ArmExtension=0.78f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float MaxTorsoLeanDegrees=12;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float CombatMoveSpeed=1;
    // Attack poses are sampled once; procedural trajectories drive the weapon.
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TSoftObjectPtr<UAnimSequence> AttackLightPose;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TSoftObjectPtr<UAnimSequence> AttackHeavyPose;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TSoftObjectPtr<UAnimSequence> AttackPiercePose;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TSoftObjectPtr<UAnimSequence> BlockPose;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float BlockPoseTime=0;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FCombatContactSettings Contacts;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FCombatChargeSettings Charging;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float CombatSpeedMultiplier=0.65f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float CombatViewWidthMultiplier=0.7f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float CombatCameraBlendSeconds=0.3f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float StanceLowering=7;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FDirectionalAttackSettings LightAttack;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FDirectionalAttackSettings HeavyAttack;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FDirectionalAttackSettings PierceAttack;

    FCombatPoseProfile()
    {
        HeavyAttack.Duration=0.48f; HeavyAttack.Recovery=0.42f; HeavyAttack.FollowThroughDegrees=100;
        PierceAttack.Duration=0.22f; PierceAttack.Recovery=0.22f; PierceAttack.ReachFraction=0.94f;
    }
};
