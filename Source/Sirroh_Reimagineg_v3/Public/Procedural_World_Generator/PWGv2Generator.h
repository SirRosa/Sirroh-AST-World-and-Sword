#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "Procedural_World_Generator/PWGv2Data.h"
#include "PWGv2Generator.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UStaticMesh;
class APWGEnvironmentV2;
struct FPWGVisualProfile;

USTRUCT()
struct FPWGv2DecorationBatch
{
    GENERATED_BODY()
    UPROPERTY(Transient) TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> Components;
    TArray<UHierarchicalInstancedStaticMeshComponent*> GrassGroups; // Owned by Components.
    FBox GrassBounds = FBox(ForceInit);
    bool bGrassVisible = true;
    TArray<TWeakObjectPtr<class APWGProceduralTree>> Trees;
    TArray<FBox> UndergroundSectionBounds;
    // Custom-depth-only geometry; original render sections and collision stay authoritative.
    UPROPERTY(Transient) TArray<TObjectPtr<UProceduralMeshComponent>> VisualMasks;
    bool bVisualMasksBuilt=false;
    TMap<int32,int32> DepthPrototypePlans; // appended section -> near(0), mid(1), backing(2)
    TSet<int32> DepthPrototypeReplacedSections;
    UPROPERTY(Transient) TArray<TObjectPtr<AActor>> Actors;
};

/** First playable PWG_v2 surface. Old PWG remains a separate implementation. */
UCLASS()
class SIRROH_REIMAGINEG_V3_API APWGv2Generator : public AActor
{
    GENERATED_BODY()
public:
    APWGv2Generator();
    // Debug edits are made on a transient copy; normal save files are never overwritten.
    UPWGv2WorldSave* BeginDebugSession();
    UPWGv2WorldSave* GetDebugWorld() const { return WorldData; }
    bool HasGlobalDepthAtmosphere() const;
    void SetVisualMaskPassEnabled(bool bEnabled);
    void SetDepthPrototypePresentation(bool bEnabled,int32 FormationCount,bool bNear,bool bMid,bool bBacking);
    void UpdateCaveReference(const FPWGVisualProfile& Profile,bool bAllowed,const FVector& View,float Width,float Height,const FVector& Pawn);
    void ResetCaveReference();
    bool IsCaveReferenceActive() const {return bCaveReferenceActive && bUndergroundView;}
    float GetCaveReferenceStart() const {return CaveReferenceStart;}
    float GetCaveReferenceDepth() const {return CaveReferenceDepth;}
    bool IsDepthPrototypeActive() const { return bUndergroundView && bDepthPrototypeActive && BackgroundAuditIsolation==0; }
    // Reserved stencil roles for this pass, independent of physical lane.
    static constexpr int32 VisualRear=201;
    static constexpr int32 VisualCutaway=202;
    static constexpr int32 VisualGameplay=203;
    static int32 VisualRoleForSection(int32 Section);
    bool IsVisualMaskPassEnabled() const { return bVisualMaskPassEnabled && bUndergroundView; }
    bool HasProjectedPOIBackground(int32 POIIndex) const;
    /** Authoring overrides keyed by the existing SectionLevel soft object path; no save schema change. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PWG v2|Background") EPOIBackgroundMode BackgroundMode=EPOIBackgroundMode::Natural;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PWG v2|Background") TMap<FName,EPOIBackgroundMode> BackgroundModesBySection;
    EPOIBackgroundMode ResolveBackgroundMode(const FPOIShaftRow& Row) const;
    bool UsesUnifiedBackground() const;
    void SetBackgroundAuditIsolation(int32 Isolation);
    bool IsBackgroundAuditSectionVisible(int32 Section) const;
    bool IsBackgroundAuditNativeSuppressed() const {return BackgroundAuditIsolation==1;}

    void DebugVisualCoverage(const FTransform& Camera,float Width,float Height) const;
    bool DebugVisualScene(int32 Scene,APawn* Pawn,FVector& Target,FVector& Focus,FString& Label);
    bool IsDebugSession() const { return DebugOriginalWorld != nullptr; }
    UPWGv2WorldSave* GetDebugEditWorld() const { return DebugWorkingWorld; }
    bool DebugRebuild(bool Restore, FString& Error, bool Replan=false);
    FString DebugLocation(const FVector& Position) const;
    bool DebugRoomChainDestination(int32 Index, float CapsuleHalfHeight, FVector& Out);
    TArray<int32> DebugAvailableLayers(FName Region,FName Biome,APawn* Pawn) const;
    bool DebugFindDestination(FName Region, FName Biome, int32 Layer, APawn* Pawn, FVector& Out, FString& Error);
    FVector GetWorldCenterSurfaceLocation() const { const auto P=GetActorLocation(); return FVector(P.X,P.Y,GetSurfaceHeightAt(P.X,P.Y)); }
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PWG v2") int32 SeedOverride=0;
    /** Applied only when creating a NEW named world; saved worlds keep their own stamps. Local PWG coordinates. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PWG v2|POI") TArray<FPOITerrainStamp> InitialPOITerrainStamps;
    /** New worlds only. Existing saves retain their portal snapshots. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PWG v2|POI") bool bUseKitShafts=true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PWG v2|POI") TObjectPtr<UDataTable> ShaftVariants;
    UFUNCTION(BlueprintCallable, Category="PWG v2")
    virtual bool GenerateWorld(FName UnusedBiome);
    UFUNCTION(BlueprintCallable, Category="PWG v2")
    virtual bool IsWorldGenerated() const { return WorldData != nullptr; }
    UFUNCTION(BlueprintCallable, Category="PWG v2")
    virtual float GetSurfaceHeightAt(float X, float Y) const;
    UFUNCTION(BlueprintCallable, Category="PWG v2")
    virtual float GetSurfaceHeightForLane(float X, EPWGTerrainLane Lane) const;
    UFUNCTION(BlueprintCallable, Category="PWG v2")
    virtual float GetLaneWorldY(EPWGTerrainLane Lane) const;
    UFUNCTION(BlueprintCallable, Category="PWG v2")
    virtual bool IsTerrainLaneEnabled(EPWGTerrainLane Lane) const { return true; }
    UFUNCTION(BlueprintCallable, Category="PWG v2")
    virtual bool FindLaneSwitchDestination(const FVector& Location, int32 Direction, EPWGTerrainLane& Lane, FVector& Destination) const;
    virtual void EnsureStarterSwordNear(const FVector& Location) {}

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PWG v2") TObjectPtr<UDataTable> RegionsTable;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PWG v2") TObjectPtr<UDataTable> WorldTable;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PWG v2") FName WorldSettingsRow = TEXT("Default");
    /** Different names create independent worlds; existing names load their saved profile. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PWG v2") FString WorldName = TEXT("FirstIsland");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PWG v2") FPWGv2WorldRow Settings;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="PWG v2") int32 WorldSeed = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="PWG v2") bool bLoadedFromSave = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="PWG v2") bool bWorldSaved = false;
    UFUNCTION(BlueprintCallable, Category="PWG v2") bool SaveWorld();
    UFUNCTION(BlueprintCallable, Category="PWG v2") bool MarkObjectRemoved(FName Id);
    virtual float GetCameraGroundHeight(const FVector& Location) const;
    void UpdateGameplayView(const FTransform& Camera, float ViewWidth, float Aspect);
    void RegisterTraversalVisual(UPrimitiveComponent* Component);
    int32 LayerAt(const FVector& Location) const;
    bool GetShaftLaneDestination(const FVector& Location,float Radius,float HalfHeight,FVector& Destination) const;
    bool GetSavedArrival(FVector& Position, int32& Lane);
    void EnsureLocationLoaded(const FVector& Position) { StreamAt(GetActorTransform().InverseTransformPosition(Position).X); }
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PWG v2") TObjectPtr<UDataTable> BiomesTable;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PWG v2") TObjectPtr<UDataTable> TreesTable;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PWG v2") TObjectPtr<UDataTable> UndergroundTable;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PWG v2") TObjectPtr<UDataTable> TerrainTexturesTable;
protected:
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY(Transient) TObjectPtr<class APOIVisibility> SightCurtain;
    friend class FPWGShaftTerrainTest;
    friend class FPWGStreamingCostTest;
    friend class FPWGRoomChainTest;
    friend class FPWGSpawnGeometryAudit;
    UPROPERTY(Transient) TObjectPtr<UPWGv2WorldSave> DebugOriginalWorld;
    UPROPERTY(Transient) TObjectPtr<UPWGv2WorldSave> DebugWorkingWorld;
    UPROPERTY(Transient) TArray<TObjectPtr<UDataTable>> DebugOriginalTables;
    FPWGv2WorldRow DebugOriginalSettings;
    double DebugTeleportPinUntil=0;
    float DebugTeleportPinX=0;
    UPROPERTY(Transient) TObjectPtr<APWGEnvironmentV2> EnvironmentV2;
    UStaticMesh* GetOrCreateGrassCardMesh();
    UPROPERTY(Transient) TObjectPtr<UPWGv2WorldSave> WorldData;
    UPROPERTY(Transient) TMap<int32, TObjectPtr<UProceduralMeshComponent>> Chunks;
    UPROPERTY(Transient) TMap<int32, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> GrassChunks;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> PreviewMaterial;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> WaterMaterial;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> GrassMaterial;
    UPROPERTY(Transient) TObjectPtr<UStaticMesh> PreviewGrassMesh;
    UPROPERTY(Transient) TMap<int32, TObjectPtr<UProceduralMeshComponent>> UndergroundChunks;
    UPROPERTY(Transient) TMap<int32, FPWGv2DecorationBatch> DecorationChunks;
    TArray<TWeakObjectPtr<UPrimitiveComponent>> TraversalVisuals;
    UPROPERTY(Transient) TMap<FString,TObjectPtr<UMaterialInstanceDynamic>> AtlasMaterials;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> ForegroundMaterial;
    UPROPERTY(Transient) TObjectPtr<UStaticMesh> CardMesh;
    UPROPERTY(Transient) TObjectPtr<UStaticMesh> RockMesh;
    UPROPERTY(Transient) TArray<TObjectPtr<UObject>> ResidentAssets;
    // Keep source hulls only until the asynchronous body is ready or promoted.
    TMap<int32,TArray<TArray<FVector>>> PendingSurfaceCollision;
    TMap<int32,TArray<TArray<FVector>>> PendingUndergroundCollision;
    float ForegroundDrop = 1200;
    float UpdateTime = 0;
    float SaveTime = 0;
    bool bVisitReady = false;
    bool bUndergroundView = false;
    void MakeExtendedPlan();
    float PreferenceScore(const FPWGv2Preference& Preference, float X) const;
    void BiomeMix(float X, int32& A, int32& B, float& Alpha) const;
    UMaterialInstanceDynamic* TerrainMaterialAt(float X, bool Cover=false, bool Entrance=false, int32 Tile=4, int32 Landscape=0);
    UMaterialInstanceDynamic* AtlasMaterial(const FPWGv2AtlasPart& Part, FLinearColor Tint, bool Tiled=false, bool TreeLighting=false, bool LitBackground=false);
    UMaterialInstanceDynamic* WallMaterial(int32 Layer,bool Cover=false,bool Entrance=false,bool SolidCutaway=false,bool LitBackground=false);
    void BuildDecorationsV2(int32 Index, float Start, float End);
    void PlanAuthoredPOIs();
    void PlanRoomChains(const TArray<FPOIShaftRow>& Pool, int32 Layer);
    bool IsRoomChain(int32 Layer, float X, float Margin=0) const;
    void PlanShaftTerrain();
    void BuildAuthoredPOIs(int32 Index, float Start, float End);
    void BuildUnderground(int32 Index, float Start, float End, bool AsyncCooking, const TArray<float>& SurfaceSamples);
    float LayerFloor(int32 Layer, float X) const;
    float LayerCeiling(int32 Layer, float X) const;
    bool IsOpening(int32 Layer, float X, float Y) const;
    void UpdateCutaway(const FVector& Player);
    void ApplyChunkLayerVisibility(int32 Index);
    void BuildVisualMasks(int32 Index);
    void SyncVisualMasks(int32 Index);
    bool bVisualMaskPassEnabled=false;
    bool bDepthPrototypeActive=false;
    UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> CaveReferenceMesh;
    UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> CaveReferenceMaterials;
    UPROPERTY(Transient) TArray<TObjectPtr<UInstancedStaticMeshComponent>> CaveReferenceInstances;
    UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> CaveReferenceInstanceMaterials;
    TArray<int32> CaveReferenceInstancePlans;
    bool bCaveReferenceHybrid=false, bCaveReferenceGeological=false;
    void ClearCaveReferenceInstances();
    FVector CaveReferenceAnchor=FVector::ZeroVector;
    float CaveReferenceWidth=0.f, CaveReferenceHeight=0.f, CaveReferenceHalfWidth=0.f;
    float CaveReferenceStart=0.f, CaveReferenceDepth=0.f;
    bool bCaveReferencePinned=false, bCaveReferenceActive=false;
    float CaveReferenceTextures[4]={-1.f,-1.f,-1.f,-1.f};
    float CaveReferenceFlatAlbedo=-1.f;
    int32 CaveReferenceLastDebug=-1;
    UMaterialInstanceDynamic* CreateCaveReferenceMaterial(int32 Layer);
    bool BuildCaveReference(const FVector& Pawn,float Depth,bool bHybrid,bool bGeological);
    bool SuppressCaveReferenceOriginal(const FPWGv2DecorationBatch& Batch,int32 Section) const;
    int32 DepthPrototypeFormationCount=2;
    bool bDepthPrototypeNear=true, bDepthPrototypeMid=true, bDepthPrototypeBacking=true;
    bool IsDepthPrototypeSectionVisible(const FPWGv2DecorationBatch& Batch,int32 Section) const;
    int32 BackgroundAuditIsolation=0;
    float BackgroundAuditFirstShaftX=MAX_flt;
    float BackgroundAuditFirstRoomX=MAX_flt;
    // Only entrance linings and the solid earth backing may render from the surface.
    // Earth is NOT cave geometry: its top edge meets the surface behind the playable lanes.
    static constexpr int32 EarthSectionBase=25;
    // Opaque camera-side rock/soil, separate from the far earth backing; underground only.
    static constexpr int32 SolidCutawaySectionBase=EarthSectionBase+4;
    static bool IsSolidCutawaySection(int32 Section) { return Section>=SolidCutawaySectionBase && Section<SolidCutawaySectionBase+4; }
    static bool IsEarthSection(int32 Section) { return Section>=EarthSectionBase && Section<EarthSectionBase+4; }
    static bool IsSurfaceVisibleUndergroundSection(int32 Section) { return IsEarthSection(Section) || Section==9 || Section==10 || Section==13 || Section==21; }
    void CaptureVisit();
    FString SlotName() const;
    void MakeProfile();
    FPWGv2RegionRow EcologyAt(float X) const;
    void StreamAt(float X, bool Immediate=true);
    void BuildChunk(int32 Index, bool AsyncCooking=false);
    float Height(float X, float Y = 0) const;
    const FPWGv2RegionSpan& RegionAt(float X) const;
};



