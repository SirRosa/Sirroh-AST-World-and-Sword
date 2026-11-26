#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "POIWorkshopGameMode.generated.h"

/** Real Sirroh AST/GASP pawn and existing sidescroller controller, without generating a world/menu. */
UCLASS()
class SIRROH_REIMAGINEG_V3_API APOIWorkshopGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    APOIWorkshopGameMode();
    virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;
    virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer,const FTransform& SpawnTransform) override;
};
