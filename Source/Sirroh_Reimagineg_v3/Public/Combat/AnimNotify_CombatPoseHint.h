#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_CombatPoseHint.generated.h"

UENUM(BlueprintType)
enum class ECombatPoseHintMarker : uint8 { Base, Wind_Up, Attack_Destination, Finish };

/** Metadata only: sampled at rig binding, never dispatches combat gameplay. */
UCLASS(meta=(DisplayName="Combat Pose Hint"))
class SIRROH_REIMAGINEG_V3_API UAnimNotify_CombatPoseHint : public UAnimNotify
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat") ECombatPoseHintMarker Marker = ECombatPoseHintMarker::Base;
    virtual FString GetNotifyName_Implementation() const override;
};
