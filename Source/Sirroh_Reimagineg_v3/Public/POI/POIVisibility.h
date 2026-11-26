#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "POIVisibility.generated.h"
/** Screen-facing occlusion curtain driven by bounded 2D sight traces, not scene captures. */
UCLASS()
class SIRROH_REIMAGINEG_V3_API APOIVisibility : public AActor
{
    GENERATED_BODY()
public:
    APOIVisibility();
    virtual void Tick(float DeltaSeconds) override;
private:
    UPROPERTY() TObjectPtr<class UProceduralMeshComponent> Fog;
};
