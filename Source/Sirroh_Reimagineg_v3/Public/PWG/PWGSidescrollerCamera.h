#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraActor.h"
#include "PWGSidescrollerCamera.generated.h"

class APWGv2Generator;

/** Perspective 2.5D camera: mostly side-on, with a small right/up viewing angle. */
UCLASS(Blueprintable)
class SIRROH_REIMAGINEG_V3_API APWGSidescrollerCamera : public ACameraActor
{
	GENERATED_BODY()

public:
	APWGSidescrollerCamera();
	virtual void Tick(float DeltaSeconds) override;

	void SetFollowTarget(AActor* NewTarget, bool bSnapImmediately = true);
	void SetGroundProvider(APWGv2Generator* NewGroundProvider);
	FVector GetScreenRightMovementDirection() const;
	void AddScrollZoom(float Steps);

	/** Camera distance from the character. 1840 is 15% farther than the old setup. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PWG|Camera", meta=(ClampMin="100.0", Units="cm"))
	float CameraDistance = 1840.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PWG|Camera", meta=(Units="cm"))
	float CameraHeight = 277.5f;

	/** Horizontal angle away from a direct side view. Keep 0 for Kingdom-style side view. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PWG|Camera", meta=(ClampMin="-30.0", ClampMax="30.0", Units="deg"))
	float CameraAngle = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PWG|Camera")
	FVector FocusOffset = FVector(0.0f, 0.0f, 277.5f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PWG|Camera", meta=(ClampMin="0.0"))
	float FollowLagSpeed = 7.0f;

	/** Smooths only genuine terrain-height changes. Player jumps never move the camera vertically. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PWG|Camera", meta=(ClampMin="0.0"))
	float GroundHeightLagSpeed = 4.0f;

private:
	float NormalOrthoWidth = -1;
	float RequestedZoom = 0.f;
	float SmoothedZoom = 0.f;
	float CombatAlpha = 0;
	UPROPERTY(Transient)
	TObjectPtr<AActor> FollowTarget;

	UPROPERTY(Transient)
	TObjectPtr<APWGv2Generator> GroundProvider;

	float SmoothedGroundHeight = 0.0f;
	bool bHasGroundHeight = false;

	void UpdateCamera(float DeltaSeconds, bool bSnap);
};
