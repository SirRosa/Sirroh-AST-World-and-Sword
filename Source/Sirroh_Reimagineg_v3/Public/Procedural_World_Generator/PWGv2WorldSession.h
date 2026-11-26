#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PWGv2WorldSession.generated.h"

/** Survives map travel. Character and world selection are independent. */
UCLASS()
class SIRROH_REIMAGINEG_V3_API UPWGv2WorldSession : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintReadOnly) FString SelectedWorld;
    UPROPERTY(BlueprintReadOnly) int32 RequestedSeed = 0;
    UPROPERTY(BlueprintReadOnly) FGuid CharacterId;
    UPROPERTY(BlueprintReadOnly) FString LastError;
};
