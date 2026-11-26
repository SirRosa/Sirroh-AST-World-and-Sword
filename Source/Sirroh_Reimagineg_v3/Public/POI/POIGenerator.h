#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/SaveGame.h"
#include "POI/POIModule.h"
#include "POI/POIStateComponent.h"
#include "POIGenerator.generated.h"

class UInstancedStaticMeshComponent;
class ULevelStreamingDynamic;

UCLASS()
class SIRROH_REIMAGINEG_V3_API UPOILayoutSave : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY(SaveGame) int32 Version=1;
    UPROPERTY(SaveGame) FPOIPlanSettings Settings;
    UPROPERTY(SaveGame) TArray<FPOIPlacement> Placements;
    UPROPERTY(SaveGame) TMap<FString,FPOIObjectState> ObjectStates;
};

/** Place in an empty map and press Generate Preview. Native examples require no content assets.
 * Runtime map streaming is local/single-player in v1; not a network replication system. */
UCLASS(Blueprintable)
class SIRROH_REIMAGINEG_V3_API APOIGenerator : public AActor
{
    GENERATED_BODY()
public:
    APOIGenerator();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="POI") FPOIPlanSettings Settings;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="POI") TObjectPtr<UDataTable> Catalog;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="POI") bool bUseExampleCatalog=true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="POI") bool bGenerateOnBeginPlay=true;
    /** Opt-in only: E interacts, F5 saves, F9 loads. Leave disabled in the real AST map. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="POI|Demo") bool bEnableDemoInput=false;
    UFUNCTION(BlueprintCallable, Category="POI|Demo") void InteractNearby();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="POI|Streaming", meta=(ClampMin="1000")) float LoadDistance=6000;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="POI|Streaming", meta=(ClampMin="0")) float UnloadMargin=2000;
    /** Use a distinct slot per generated room/world. Only layout is saved in v1. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="POI|Save") FString LayoutSlot=TEXT("POI_Prototype_Layout");
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="POI") TArray<FPOIPlacement> Plan;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="POI") FString Status;
    UFUNCTION(CallInEditor, BlueprintCallable, Category="POI") void GeneratePreview();
    UFUNCTION(CallInEditor, BlueprintCallable, Category="POI") void ClearPreview();
    UFUNCTION(BlueprintCallable, Category="POI|Save") bool SaveLayout();
    UFUNCTION(BlueprintCallable, Category="POI|Save") bool LoadLayout();
    UFUNCTION(CallInEditor, Category="POI|Save") void SaveLayoutToSlot() { SaveLayout(); }
    UFUNCTION(CallInEditor, Category="POI|Save") void LoadLayoutFromSlot() { LoadLayout(); }
    void RecordRemovedObject(const FString& Key);
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void Tick(float DeltaSeconds) override;
private:
    UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> PreviewMesh;
    UPROPERTY(Transient) TMap<int32,TObjectPtr<ULevelStreamingDynamic>> LoadedMaps;
    TSet<int32> FailedMaps;
    TSet<int32> RestoredMaps;
    TSet<int32> PersistentMaps;
    UPROPERTY(Transient) TMap<FString,FPOIObjectState> ObjectStates;
    UPROPERTY() FPOIPlanSettings BuiltSettings;
    void RebuildGeometry();
    void UpdateStreaming();
    void ReleaseMaps();
    void CaptureMapState(int32 Index);
    UFUNCTION() void RestoreVisibleMapStates();
};
