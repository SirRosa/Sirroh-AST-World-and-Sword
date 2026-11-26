#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "POIModule.generated.h"

/** V1 uses left-to-right modules on X/Z, with a single continuous lane on Y.
 * Entry and exit are at X=0 and X=WidthCells, Y=0, Z=Entry/ExitFloor.
 * Authors guarantee a traversable path between them; the planner checks topology. */
USTRUCT(BlueprintType)
struct SIRROH_REIMAGINEG_V3_API FPOIModuleRow : public FTableRowBase
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Module") FName ModuleId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Module") FName Environment = TEXT("Dungeon");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Module") FName Category = TEXT("Corridor");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dimensions", meta=(ClampMin="100")) float CellWidth = 400;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dimensions", meta=(ClampMin="100")) float FloorHeight = 300;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dimensions", meta=(ClampMin="100")) float LaneWidth = 300;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Module", meta=(ClampMin="1", ClampMax="32")) int32 WidthCells = 2;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Module", meta=(ClampMin="1", ClampMax="16")) int32 HeightCells = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Connections", meta=(ClampMin="0")) int32 EntryFloor = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Connections", meta=(ClampMin="0")) int32 ExitFloor = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Connections") FName EntryType = TEXT("Walk");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Connections") FName ExitType = TEXT("Walk");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Selection", meta=(ClampMin="0")) float Weight = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Selection", meta=(ClampMin="0")) int32 MinimumBaseFloor = 0;
    /** Source map pivot is at the lower left of the module, centered on its lane. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Content") TSoftObjectPtr<UWorld> Level;
};

USTRUCT(BlueprintType)
struct SIRROH_REIMAGINEG_V3_API FPOIPlanSettings
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plan") int32 Seed = 12345;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plan") FName Environment = TEXT("Dungeon");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plan", meta=(ClampMin="1", ClampMax="64")) int32 ModuleCount = 8;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plan", meta=(ClampMin="1", ClampMax="16")) int32 Floors = 3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plan", meta=(ClampMin="0")) int32 StartFloor = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plan", meta=(ClampMin="0")) int32 EndFloor = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plan") bool bRequireTopFloor = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plan") FName BoundaryType = TEXT("Walk");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plan", meta=(ClampMin="100", ClampMax="100000")) int32 SearchBudget = 10000;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dimensions", meta=(ClampMin="100")) float CellWidth = 400;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dimensions", meta=(ClampMin="100")) float FloorHeight = 300;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dimensions", meta=(ClampMin="100")) float LaneWidth = 300;
};

USTRUCT(BlueprintType)
struct SIRROH_REIMAGINEG_V3_API FPOIPlacement
{
    GENERATED_BODY()
    /** Snapshot, not an index into an editable catalog. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Plan") FPOIModuleRow Module;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Plan") FIntPoint Cell = FIntPoint::ZeroValue;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Plan") FName InstanceId;
};

UCLASS()
class SIRROH_REIMAGINEG_V3_API UPOIPlanner : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    static bool ValidateSettings(const FPOIPlanSettings& Settings, FString& Error);
    UFUNCTION(BlueprintCallable, Category="POI") static bool ValidateCatalog(const TArray<FPOIModuleRow>& Modules, FString& Error);
    UFUNCTION(BlueprintCallable, Category="POI") static bool BuildPlan(const TArray<FPOIModuleRow>& Modules, const FPOIPlanSettings& Settings, TArray<FPOIPlacement>& Plan, FString& Error);
    UFUNCTION(BlueprintCallable, Category="POI") static TArray<FPOIModuleRow> ExampleCatalog();
};
