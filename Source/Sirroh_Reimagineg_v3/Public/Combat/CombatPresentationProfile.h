#pragma once
#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "CombatPresentationProfile.generated.h"

class UAnimSequence;
class UPoseAsset;
class USoundBase;
class UNiagaraSystem;

/** Local coordinates: X=character forward, Y=depth away from camera, Z=up.
 * Grip offsets are fractions of arm length relative to the reference shoulder. */
USTRUCT(BlueprintType)
struct FCombatGuardProfile : public FTableRowBase
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FVector GuardGrip=FVector(0.6,0,-0.5);
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FVector GuardBlade=FVector(0,0,1);
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FVector ReadyGrip=FVector(-0.2,0.45,-0.2);
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FVector ReadyBlade=FVector(-0.5,1,0);
    /** PoseAsset overrides the corresponding single-frame AnimSequence fallback below.
     * An empty pose name selects the first pose stored in that asset. */
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Pose Assets") TSoftObjectPtr<UPoseAsset> GuardPoseAsset;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Pose Assets") FName GuardPoseName;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Pose Assets") TSoftObjectPtr<UPoseAsset> LightPoseAsset;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Pose Assets") FName LightPoseName;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Pose Assets") TSoftObjectPtr<UPoseAsset> HeavyPoseAsset;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Pose Assets") FName HeavyPoseName;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Pose Assets") TSoftObjectPtr<UPoseAsset> PiercePoseAsset;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Pose Assets") FName PiercePoseName;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TSoftObjectPtr<UAnimSequence> GuardPose;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin="0",Units="s",ToolTip="Fixed sample time in seconds, not a frame number. Ignored when GuardPoseAsset is assigned.")) float GuardPoseTime=0;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TSoftObjectPtr<UAnimSequence> LightPose;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin="0",Units="s")) float LightPoseTime=0;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TSoftObjectPtr<UAnimSequence> HeavyPose;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin="0",Units="s")) float HeavyPoseTime=0;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TSoftObjectPtr<UAnimSequence> PiercePose;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin="0",Units="s")) float PiercePoseTime=0;
};

USTRUCT(BlueprintType)
struct FCombatEffectProfile : public FTableRowBase
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TSoftObjectPtr<USoundBase> CollisionSound;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float Volume=1;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float Pitch=1;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TSoftObjectPtr<UNiagaraSystem> SwingTrail;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float TrailScale=1;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float TrailFadeSeconds=0.35f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TSoftObjectPtr<UNiagaraSystem> SwordCollisionParticles;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float CollisionScale=1;
};
