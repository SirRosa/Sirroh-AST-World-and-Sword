#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "PlaceholderBerryWorld.generated.h"

class APlaceholderBerry;

/** Session-only state survives terrain chunk eviction, but never leaks into another world. */
UCLASS()
class SIRROH_REIMAGINEG_V3_API UPlaceholderBerryWorld : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    TSet<FString> Collected;
    TArray<TWeakObjectPtr<APlaceholderBerry>> Berries;
};
