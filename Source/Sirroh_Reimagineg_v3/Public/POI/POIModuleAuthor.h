#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "POI/POIModule.h"
#include "POIModuleAuthor.generated.h"

class UBoxComponent;
class UArrowComponent;

/** Place exactly one at the map origin. Bounds and connection arrows are editor helpers.
 * Export updates a row in the selected DataTable; save that asset afterwards. */
UCLASS(Blueprintable)
class SIRROH_REIMAGINEG_V3_API APOIModuleAuthor : public AActor
{
    GENERATED_BODY()
public:
    APOIModuleAuthor();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="POI") FPOIModuleRow Definition;
    UPROPERTY(EditAnywhere, Category="POI") TObjectPtr<UDataTable> TargetCatalog;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="POI") FString Status;
    UFUNCTION(CallInEditor, Category="POI") void ValidateModule();
    UFUNCTION(CallInEditor, Category="POI") void ExportDefinition();
    virtual void OnConstruction(const FTransform& Transform) override;
private:
    UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Bounds;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UArrowComponent> Entry;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UArrowComponent> Exit;
};
