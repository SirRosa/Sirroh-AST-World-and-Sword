#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PWG/PWGTraversalProvider.h"
#include "PWGLedgeActor.generated.h"

class UBoxComponent;

UCLASS(NotBlueprintable, Transient)
class SIRROH_REIMAGINEG_V3_API APWGLedgeActor : public AActor, public IPWGTraversalProvider
{
	GENERATED_BODY()

public:
	APWGLedgeActor();

	void ConfigureLedge(const FVector& WallCenter, const FVector& WallExtent,
		const FTransform& InFrontLedge, const FTransform& InBackLedge);

	virtual bool GetPWGLedgeTransforms_Implementation(FTransform& FrontLedge, FTransform& BackLedge) const override;

private:
	UPROPERTY(VisibleAnywhere, Category="PWG|Traversal")
	TObjectPtr<UBoxComponent> DetectionBox;

	UPROPERTY(VisibleInstanceOnly, Category="PWG|Traversal")
	FTransform FrontLedgeTransform;

	UPROPERTY(VisibleInstanceOnly, Category="PWG|Traversal")
	FTransform BackLedgeTransform;
};

