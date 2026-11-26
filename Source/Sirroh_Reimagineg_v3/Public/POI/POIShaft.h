#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/DataTable.h"
#include "POI/POIKitBlock.h"
#include "Components/SplineComponent.h"
#include "POIShaft.generated.h"

/** Original AST actor with its authored spline and editable Blueprint settings. */
USTRUCT(BlueprintType)
struct SIRROH_REIMAGINEG_V3_API FPOIShaftNativePiece
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftClassPtr<AActor> ActorClass;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FTransform Transform;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FSplinePoint> Points;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bClosedLoop=false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TMap<FName,FString> Settings;
};

USTRUCT(BlueprintType)
struct SIRROH_REIMAGINEG_V3_API FPOIShaftPiece
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Section") FTransform Transform;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Section") FPOIKitSettings Block;
};

UENUM(BlueprintType)
enum class EPOIShaftRole : uint8 { RepeatedSection, Entrance, Middle, Exit };

USTRUCT(BlueprintType)
struct SIRROH_REIMAGINEG_V3_API FPOIShaftRow : public FTableRowBase
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Selection") EPOIShaftRole Role=EPOIShaftRole::RepeatedSection;
    /** 0 surface -> underground, 1 underground -> caves, 2 caves -> dungeon, 3 dungeon -> catacombs. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Selection",meta=(ClampMin="0",ClampMax="3")) int32 UpperLayer=0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Selection",meta=(ClampMin="0")) float Weight=1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shaft",meta=(ClampMin="400",ClampMax="1200")) float Width=600;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shaft",meta=(ClampMin="75",ClampMax="200")) float StepRise=150;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shaft") EPOIKitKind Climbing=EPOIKitKind::Both;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shaft") EPOIKitCollision Collision=EPOIKitCollision::OneWay;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shaft") TSoftObjectPtr<UTexture2D> PlatformImage;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shaft") TSoftObjectPtr<UTexture2D> SideImage;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shaft") TSoftObjectPtr<UTexture2D> BackImage;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Custom Section") bool bUseCustomSection=false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Custom Section",meta=(ClampMin="150",ClampMax="3000")) float SectionHeight=300;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Custom Section") TArray<FPOIShaftPiece> Pieces;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Custom Section") TArray<FPOIShaftNativePiece> NativePieces;
    /** Complete authored map, preserving arbitrary actors and references between them. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Custom Section") TSoftObjectPtr<UWorld> SectionLevel;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Selection") FName Category;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Selection") TSoftObjectPtr<class UPOIArtTheme> ArtTheme;
    bool Validate(FString& Error) const;
};

/** Editor workshop and runtime shaft; origin = middle of lower exit, Z=lower floor.
 * Build creates editable attached kit blocks. Export Section captures attached kit blocks.
 * PWG uses a saved row snapshot, with unscaled authored sections repeated up the shaft. */
UCLASS(Blueprintable)
class SIRROH_REIMAGINEG_V3_API APOIShaft : public AActor
{
    GENERATED_BODY()
public:
    APOIShaft();
    virtual void Tick(float DeltaSeconds) override;
    /** Runtime underground fragments project their background art onto the tunnel mesh. */
    bool bProjectBackground=false;
    // PWG enables this only for distant, prefetched chunks. Initial/nearby builds stay immediate.
    bool bIncrementalBuild=false;
    bool HasPendingBuild() const { return !PendingBlocks.IsEmpty(); }
    void ProcessPendingBuild();
    // Independent of distance dormancy: visual suppression must never pause simulation.
    void SetLayerRenderHidden(bool bHideLayer);
    bool IsLayerRenderHidden() const { return bLayerRenderHidden; }
    void SetVisualMaskEnabled(bool bEnabled);
    void LogVisualMaskCoverage() const;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shaft") FPOIShaftRow Recipe;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shaft",meta=(ClampMin="150",ClampMax="30000")) float Height=900;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shaft") float BottomExitClearance=300;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shaft") bool bBuildBoundaryWalls=true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Assembly") bool bAssembleModules=false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Assembly") TObjectPtr<UDataTable> ModuleTable;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Assembly") int32 Seed=1;
    UPROPERTY() TArray<FPOIShaftRow> SavedModules;
    UPROPERTY() TArray<FPOIShaftRow> PlannedSections;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Assembly") TArray<EPOIShaftRole> BuiltRoles;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Export") TObjectPtr<UDataTable> TargetTable;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Export") FName RowName=TEXT("MyShaft");
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Shaft") FString Status;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Shaft") TArray<TObjectPtr<APOIKitBlock>> GeneratedBlocks;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Shaft") TArray<TObjectPtr<AActor>> GeneratedNativeActors;
    UPROPERTY(Transient) TArray<TObjectPtr<class ULevelStreamingDynamic>> GeneratedLevels;
    UFUNCTION(CallInEditor, BlueprintCallable, Category="Shaft") void BuildShaft();
    UFUNCTION(CallInEditor, BlueprintCallable, Category="Shaft") void ClearGenerated();
    UFUNCTION(CallInEditor, Category="Shaft") void ExportSection();
protected:
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    struct FPendingBlock { FPOIKitSettings Settings; FTransform Local; bool Boundary=false; };
    TArray<FPendingBlock> PendingBlocks;
    bool bProcessingPending=false;
    struct FPausedActor
    {
        TWeakObjectPtr<AActor> Actor;
        bool Tick=false, Hidden=false;
        TArray<TWeakObjectPtr<UActorComponent>> TickingComponents;
    };
    TArray<FPausedActor> PausedActors;
    TSet<TWeakObjectPtr<AActor>> PausedSet;
    bool bDormant=false;
    bool bLayerRenderHidden=false;
    bool bVisualMaskEnabled=false;
    struct FVisualStencilState { TWeakObjectPtr<UPrimitiveComponent> Mesh; int32 OriginalStencil=0; bool bOriginalEnabled=false; int32 AppliedStencil=0; };
    TArray<FVisualStencilState> VisualStencilStates;
    void ApplyVisualMaskToActor(AActor* Actor);
    void RestoreVisualStencils();
    TSet<TWeakObjectPtr<AActor>> LayerHiddenActors;
    void HideLayerActor(AActor* Actor);
    void RefreshLayerRenderVisibility();
    int32 DormantActorCount=-1;
    void UpdateDormancy();
    UFUNCTION() void ApplyLoadedArt();
    UPROPERTY(Transient) TSet<TObjectPtr<ULevelStreamingDynamic>> DecoratedLevels;
    FRandomStream ArtRandom;
    void AddSection(const FPOIShaftRow& Section, float Offset);
    APOIKitBlock* AddBlock(const FPOIKitSettings& Settings,const FTransform& Local,bool Boundary=false);
};
