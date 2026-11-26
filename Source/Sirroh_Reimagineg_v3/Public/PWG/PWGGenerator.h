#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "PWG/PWGBiomeData.h"
#include "Procedural_World_Generator/PWGv2Data.h"
#include "PWGGenerator.generated.h"

class APWGLedgeActor;
class APlaceholderBerry;
class AProceduralSword;
class APWGEnvironmentV2;
class UDataTable;
class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UProceduralMeshComponent;
class USceneComponent;
class UTexture2D;
class UStaticMesh;

UCLASS(BlueprintType)
class SIRROH_REIMAGINEG_V3_API APWG_Generator : public AActor
{
	GENERATED_BODY()

public:
	APWG_Generator();

	UFUNCTION(BlueprintCallable, Category="PWG")
	virtual bool GenerateWorld(FName BiomeRowName);
	void BuildSolidTerrainCollision();

	UFUNCTION(BlueprintPure, Category="PWG")
	virtual float GetSurfaceHeightAt(float X, float Y) const;
    virtual float GetCameraGroundHeight(const FVector& Location) const { return GetSurfaceHeightAt(Location.X,Location.Y); }

	UFUNCTION(BlueprintPure, Category="PWG")
	FVector GetWorldCenterSurfaceLocation() const;

	UFUNCTION(BlueprintPure, Category="PWG|Terrain Lanes")
	virtual float GetSurfaceHeightForLane(float X, EPWGTerrainLane Lane) const;

	UFUNCTION(BlueprintPure, Category="PWG|Terrain Lanes")
	virtual float GetLaneWorldY(EPWGTerrainLane Lane) const;

	UFUNCTION(BlueprintPure, Category="PWG|Terrain Lanes")
	virtual bool IsTerrainLaneEnabled(EPWGTerrainLane Lane) const;

	/** Returns a safe destination only inside a generated lane-connection zone. Direction: -1 camera/front, +1 back. */
	UFUNCTION(BlueprintCallable, Category="PWG|Terrain Lanes")
	virtual bool FindLaneSwitchDestination(const FVector& WorldLocation, int32 Direction,
		EPWGTerrainLane& OutLane, FVector& OutWorldLocation) const;

	UFUNCTION(BlueprintPure, Category="PWG")
	virtual bool IsWorldGenerated() const { return bWorldGenerated; }
	virtual void EnsureStarterSwordNear(const FVector& PlayerLocation);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PWG|Rocks") bool bEnableSurfaceRocks=true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PWG|Rocks",meta=(ClampMin="0",ClampMax="100")) float RocksPer100Metres=4.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PWG|Rocks",meta=(ClampMin="1")) float RockBaseHeight=80.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PWG|Rocks",meta=(ClampMin="0.1",ClampMax="1")) float RockEmbedHeightFraction=0.55f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PWG|Swords") bool bEnableSwordSpawns=true;

	/** Applies the global day/night multiplier to every currently streamed terrain chunk. */
	void ApplyStylizedTerrainLighting(const FLinearColor& Tint, float Brightness);

	/** Read-only bridge used by the single Environment V2 actor for biome-owned atlas data. */
	const FBiomeData* GetActiveBiomeDataForEnvironment() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="PWG")
	TObjectPtr<UDataTable> BiomeTable;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="PWG")
	FName DefaultBiomeRow = TEXT("Meadow");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="PWG")
	bool bGenerateOnBeginPlay = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="PWG", meta=(ClampMin="0"))
	int32 SeedOverride = 0;

	/** Keeps the current chunk and its neighbours generated around the player. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="PWG|Streaming")
	bool bEnableChunkStreaming = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="PWG|Streaming", meta=(ClampMin="1", ClampMax="4"))
	int32 PreloadRadiusInChunks = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="PWG|Streaming", meta=(ClampMin="1", ClampMax="8"))
	int32 RetentionRadiusInChunks = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="PWG|Streaming", meta=(ClampMin="0.05", ClampMax="2.0"))
	float StreamingUpdateInterval = 0.25f;

	/** Biomes used in successive streaming regions. The first entry surrounds world origin. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="PWG|Streaming|Biomes")
	TArray<FName> StreamingBiomeRows = { TEXT("Meadow"), TEXT("Mountains") };

	/** New, isolated Environment implementation. The root generator creates exactly one instance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="PWG|Environment V2")
	TSubclassOf<APWGEnvironmentV2> EnvironmentV2Class;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="PWG|Environment V2")
	TObjectPtr<APWGEnvironmentV2> EnvironmentV2;

	/** UNUSED_BROKEN: retained only so existing serialized generator assets can load. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="PWG|Environment_UNUSED_BROKEN|Shared Sky")
	TSoftObjectPtr<UTexture2D> SharedSkyPanoramaTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(
		TEXT("/Game/Systems/Sprite2Bone/Textures/Sky/15.15")));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="PWG|Environment_UNUSED_BROKEN|Shared Sky", meta=(ClampMin="1000.0"))
	float SharedSkyHeight = 4000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="PWG|Environment_UNUSED_BROKEN|Shared Sky", meta=(ClampMin="0.0", ClampMax="0.25"))
	float SkyParallaxFactor = 0.02f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="PWG|Environment_UNUSED_BROKEN|Shared Sky", meta=(ClampMin="-0.01", ClampMax="0.01"))
	float SharedSkyScrollSpeed = 0.0001f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="PWG|Environment_UNUSED_BROKEN|Shared Sky")
	float SharedSkyDepth = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="PWG|Environment_UNUSED_BROKEN|Shared Sky")
	float SharedSkyVerticalOffset = 400.0f;

	/** Compensates landscape height for the low, ground-focused camera framing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="PWG|Environment_UNUSED_BROKEN|Landscape", meta=(Units="cm"))
	float LandscapeVerticalFramingOffset = -100.0f;

	/** Number of chunks occupied by one biome before advancing to the next row. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="PWG|Streaming|Biomes", meta=(ClampMin="1", ClampMax="32"))
	int32 ChunksPerBiome = 2;

	/** Terrain-height blend on both sides of a biome boundary. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="PWG|Streaming|Biomes", meta=(ClampMin="0", ClampMax="128"))
	int32 BiomeTransitionWidthInTiles = 24;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="PWG")
	TObjectPtr<UProceduralMeshComponent> TerrainMesh;

	/** Simplified invisible heightfield used only by movement and physics. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="PWG")
	TObjectPtr<UProceduralMeshComponent> TerrainCollisionMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="PWG")
	TObjectPtr<UProceduralMeshComponent> DecorationMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="PWG|Environment")
	TObjectPtr<UProceduralMeshComponent> EnvironmentMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="PWG|Environment")
	TArray<TObjectPtr<UProceduralMeshComponent>> BackgroundLayerMeshes;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="PWG|Environment")
	TArray<TObjectPtr<UProceduralMeshComponent>> BackgroundSpriteLayerMeshes;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="PWG|Environment")
	TObjectPtr<UProceduralMeshComponent> SkyMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="PWG|Environment")
	TObjectPtr<UProceduralMeshComponent> SunMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="PWG|Environment")
	TObjectPtr<UProceduralMeshComponent> MoonMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="PWG|Environment")
	TObjectPtr<UProceduralMeshComponent> CloudMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="PWG|Environment")
	TObjectPtr<UProceduralMeshComponent> TerrainSceneryMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="PWG|Environment")
	TObjectPtr<UProceduralMeshComponent> VegetationMesh;

	/** Runtime-created grass batches. Usually three components when all mini-biomes share one atlas. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="PWG|Grass")
	TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> GrassInstanceComponents;

	/** Lightweight, non-colliding bushes generated on the PWG surface (never on landscapes). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PWG|Bushes")
	bool bEnableSurfaceBushes = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PWG|Placeholder Berries")
	bool bEnablePlaceholderBerries = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PWG|Placeholder Berries", meta=(ClampMin="0", ClampMax="1"))
	float BerryBushChance = 0.65f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PWG|Placeholder Berries", meta=(ClampMin="1", ClampMax="8"))
	int32 BerriesPerBush = 3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PWG|Placeholder Berries", meta=(ClampMin="1"))
	float BerryWorldSize = 14.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PWG|Placeholder Berries", meta=(ClampMin="0", ClampMax="1"))
	float TreeFruitChance = 0.75f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PWG|Placeholder Berries", meta=(ClampMin="1", ClampMax="20"))
	int32 MaxFruitsPerTree = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PWG|Bushes")
	TSoftObjectPtr<UTexture2D> BushAtlas = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(
		TEXT("/Game/Systems/Sprite2Bone/Biomes/Meadow/Meadow_Bushes/Meadows_Bush_Atlas.Meadows_Bush_Atlas")));

	/** Uses the grass shader graph; the atlas is still independently assigned above. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PWG|Bushes")
	TSoftObjectPtr<UMaterialInterface> BushMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(
		TEXT("/Game/Systems/Sprite2Bone/Materials/M_PWG_Grass.M_PWG_Grass")));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PWG|Bushes|Atlas", meta=(ClampMin="1", ClampMax="32"))
	int32 BushAtlasColumns = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PWG|Bushes|Atlas", meta=(ClampMin="1", ClampMax="32"))
	int32 BushAtlasRows = 8;

	/** Average number of cards per 100 metres of terrain. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PWG|Bushes|Placement", meta=(ClampMin="0.0", ClampMax="200.0"))
	float BushesPer100Metres = 14.0f;

	/** Height of one bush before random ScaleRange is applied; width keeps the atlas frame's 2:1 ratio. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PWG|Bushes|Placement", meta=(ClampMin="1.0", Units="cm"))
	float BushBaseSize = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PWG|Bushes|Placement")
	FVector2D BushScaleRange = FVector2D(0.5f, 1.2f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PWG|Bushes|Placement", meta=(ClampMin="0.0", ClampMax="1.0"))
	float BushFrontChance = 0.35f;

	/** Positive Y is behind the player. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PWG|Bushes|Placement")
	FVector2D BushBackDepthRange = FVector2D(75.0f, 140.0f);

	/** Negative Y is in front of the player. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PWG|Bushes|Placement")
	FVector2D BushFrontDepthRange = FVector2D(-140.0f, -75.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PWG|Bushes|Placement", meta=(ClampMin="0.0", Units="cm"))
	float BushRootBurial = 8.0f;

	/** Multiplies the grass material's wind per bush; below 1 makes bushes heavier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PWG|Bushes|Wind", meta=(ClampMin="0.0", ClampMax="2.0"))
	float BushWindInfluence = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PWG|Bushes|Culling", meta=(ClampMin="0", Units="cm"))
	int32 BushStartCullDistance = 3500;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PWG|Bushes|Culling", meta=(ClampMin="100", Units="cm"))
	int32 BushEndCullDistance = 6500;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PWG|Bushes|Rendering")
	FLinearColor BushTint = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PWG|Bushes|Rendering", meta=(ClampMin="0.0"))
	float BushBrightness = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="PWG|Bushes")
	TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> BushInstanceComponents;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="PWG|Block Overhang")
	TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> BlockOverhangInstanceComponents;

	/** Runtime-created modular tree batches. Usually two components when every mini-biome shares an atlas. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="PWG|Trees")
	TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> TreeInstanceComponents;

	/** All static bark segments and junctions are merged into this component per chunk. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="PWG|Trees")
	TObjectPtr<UProceduralMeshComponent> TreeTrunkMesh;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	enum class ETerrainTileRole : uint8
	{
		Interior,
		Top,
		SurfaceToGround,
		Bottom,
		Left,
		Right,
		TopLeft,
		TopRight,
		BottomLeft,
		BottomRight
	};

	struct FMeshBuffers
	{
		TArray<FVector> Vertices;
		TArray<int32> Triangles;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
		TArray<FColor> Colors;
		TArray<FProcMeshTangent> Tangents;
	};

	struct FAtlasTileChoice
	{
		int32 Index = 0;
		uint8 QuarterTurns = 0;
		bool bMirrorU = false;
	};

	void BuildSurfaceHeights(const FBiomeData& Biome, FRandomStream& Random);
	void BuildAtlasColumnPath(const FBiomeData& Biome, FRandomStream& Random);
	void BuildTerrainMesh(const FBiomeData& Biome, FRandomStream& Random);
	void BuildLayeredTerrainMesh(const FBiomeData& Biome, FRandomStream& Random);
	void BuildDecorations(const FBiomeData& Biome, FRandomStream& Random);
	void BuildTraversalLedges(const FBiomeData& Biome);
	void SpawnBiomeObjects(const FBiomeData& Biome, FRandomStream& Random);
	void BuildChunkEnvironment_UNUSED_BROKEN(const FBiomeData& Biome, FRandomStream& Random);
	void BuildGrass(const FBiomeData& Biome, FRandomStream& Random);
	void BuildBushes(const FBiomeData& Biome, FRandomStream& Random);
	void BuildRocks(const FBiomeData& Biome);
	void AddRock(const FBiomeData& Biome,const FVector& LocalPosition,EPWGTerrainLane Lane,int32 Mini,int32 Variant,float Scale,const FString& Id,int32 Seed,bool bForceSword=false);
	void ClearSwordScenery();
	UPROPERTY(Transient) TObjectPtr<UHierarchicalInstancedStaticMeshComponent> RockInstances;
	UPROPERTY(Transient) TArray<TObjectPtr<AProceduralSword>> SpawnedSwords;
	bool bStarterSwordCreated=false;
	void ClearPlaceholderBerries();
	void SpawnPlaceholderFruit(const FVector& LocalPosition, float LocalLaneY, const FString& Id, int32 Frame);
	UPROPERTY(Transient)
	TArray<TObjectPtr<APlaceholderBerry>> PlaceholderBerries;
	void BuildBlockOverhangs(const FBiomeData& Biome, FRandomStream& Random);
	void BuildTrees(const FBiomeData& Biome);
	void BuildLegacyTrees(const FBiomeData& Biome);
	void UpdateGrassMaterialParameters();
	protected:
    UStaticMesh* GetOrCreateGrassCardMesh();
private:
	void SpawnEnvironmentV2();
	// UNUSED_BROKEN: legacy environment implementation. Never called.
	void BuildGlobalEnvironment_UNUSED_BROKEN(const FBiomeData& Biome);
	void UpdateGlobalEnvironment_UNUSED_BROKEN(float DeltaSeconds);
	void UpdateLandscapeParallax_UNUSED_BROKEN(const FVector& FollowLocation);
	void ClearGeneratedWorld();
	void AddQuad(FMeshBuffers& Buffers, const FVector& A, const FVector& B,
		const FVector& C, const FVector& D, const FVector& Normal,
		const FVector& Tangent, int32 AtlasIndex, const FBiomeData& Biome,
		float TileU0 = 0.0f, float TileU1 = 1.0f, uint8 QuarterTurns = 0,
		bool bMirrorU = false, float TileV0 = 0.0f, float TileV1 = 1.0f) const;
	int32 PickTile(const TArray<int32>& Pool, FRandomStream& Random) const;
	FAtlasTileChoice PickUniversalTerrainTile(const FBiomeData& Biome, int32 X,
		ETerrainTileRole TileRole, FRandomStream& Random, int32 DepthBelowSurface = 0,
		bool bAllowCompactBlend = true) const;
	bool IsSolidCell(int32 X, int32 Z) const;
	float GetLocalSurfaceHeightByIndex(int32 Index) const;
	float GetSmoothSurfaceHeightAtLocalX(float LocalX) const;
	float GetLaneSurfaceHeightAtLocalX(float LocalX, EPWGTerrainLane Lane) const;
	const TArray<float>& GetLaneSteppedHeights(EPWGTerrainLane Lane) const;
	bool IsLaneEnabledLocal(EPWGTerrainLane Lane) const;
	float GetLaneCenterYLocal(EPWGTerrainLane Lane) const;
	float EvaluateLaneConnectionAlpha(const FPWGTerrainLaneSettings& Settings,
		EPWGTerrainLane Lane, double GlobalTileCoordinate) const;
	void BuildSmoothSurfaceShell(FMeshBuffers& Mesh, FMeshBuffers& BackingMesh,
		const FBiomeData& Biome, FRandomStream& Random);
	void AddTexturedQuad(FMeshBuffers& Buffers, const FVector& A, const FVector& B,
		const FVector& C, const FVector& D, const FVector2D& UV0, const FVector2D& UV1) const;
	UMaterialInstanceDynamic* CreateEnvironmentSection_UNUSED_BROKEN(UProceduralMeshComponent* Mesh, int32 SectionIndex,
		const FMeshBuffers& Buffers, const FBiomeData& Biome, UTexture2D* Texture,
		const FLinearColor& Tint, float Brightness, bool bVirtualTexture, float ScrollSpeed);
	bool GenerateSingleChunk(FName BiomeRowName, const FBiomeData& Biome);
	APWG_Generator* EnsureChunkLoaded(int32 ChunkIndex);
	void UpdateChunkStreaming();
	int32 GetChunkIndexAtWorldX(float WorldX) const;
	float EvaluateSmoothedGlobalHeight(const FBiomeData& Biome, int64 GlobalColumn) const;
	float EvaluateBiomeHeight(const FBiomeData& Biome, int64 GlobalColumn) const;
	FName GetBiomeRowForChunk(int32 ChunkIndex) const;
	const FBiomeData* FindBiomeData(FName RowName, const TCHAR* Context) const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> SpawnedActors;

	UPROPERTY(Transient)
	TArray<TObjectPtr<APWGLedgeActor>> SpawnedLedges;

	TArray<int32> SurfaceHeightTiles;
	TArray<float> SmoothSurfaceHeights;
	TArray<float> SteppedSurfaceHeights;
	TArray<int32> FrontSurfaceHeightTiles;
	TArray<int32> BackSurfaceHeightTiles;
	TArray<float> FrontSteppedSurfaceHeights;
	TArray<float> BackSteppedSurfaceHeights;
	TArray<float> FrontConnectionAlpha;
	TArray<float> BackConnectionAlpha;
	TArray<uint8> SolidCells;
	TArray<int32> CellStyleBases;
	TArray<uint8> ColumnMiniBiomes;
	TArray<int8> ColumnBlendFrom;
	TArray<int8> ColumnBlendTo;
	TArray<int8> ColumnBlendStep;
	TArray<uint8> ColumnAtlasX;
	TArray<uint8> ColumnAtlasY;
	TArray<int8> ColumnAtlasDirection;
	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<APWG_Generator>> LoadedChunks;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> EnvironmentMaterials;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> LandscapeMaterials;
	TArray<float> LandscapeRepeatWidths;
	FVector LandscapeParallaxAnchorViewLocation = FVector::ZeroVector;
	bool bHasLandscapeParallaxAnchor = false;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> GrassMaterials;

	/** Overhang uses the grass shader but must never move along the depth axis. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> BlockOverhangMaterials;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> TreeMaterials;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> TerrainMaterialInstance;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> GrassCardMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SkyMaterial;

	FName ActiveBiomeRow = NAME_None;
	int32 ActiveWorldSeed = 0;
	int32 StreamingChunkIndex = 0;
	int64 GlobalColumnOffset = 0;
	float StreamingChunkLength = 0.0f;
	float StreamingAccumulator = 0.0f;
	float LeftGhostSurfaceHeight = 0.0f;
	float RightGhostSurfaceHeight = 0.0f;
	float FrontLeftGhostSurfaceHeight = 0.0f;
	float FrontRightGhostSurfaceHeight = 0.0f;
	float BackLeftGhostSurfaceHeight = 0.0f;
	float BackRightGhostSurfaceHeight = 0.0f;
	bool bFrontLaneEnabled = false;
	bool bBackLaneEnabled = false;
	float DayCycleClock = 0.0f;
	FName ActiveSkyBiomeRow = NAME_None;
	FBiomeData ActiveSkyBiome;
	bool bStreamingChild = false;
	int32 BottomTileIndex = 0;
	int32 SolidGridHeight = 0;
	float ActiveTileSize = 100.0f;
	float ActiveSurfaceSegmentWidth = 10.0f;
	int32 ActiveSurfaceSegmentsPerTile = 10;
	float ChunkStartLocalX = 0.0f;
	float ChunkHalfDepth = 150.0f;
	float BottomLocalZ = 0.0f;
	bool bWorldGenerated = false;

	struct FCloudInstance
	{
		int32 RuleIndex = 0;
		float X = 0.0f;
		float Z = 0.0f;
		float Speed = 0.0f;
		FVector2D Size = FVector2D::ZeroVector;
	};
	TArray<FCloudInstance> ActiveClouds;
	TArray<FPWGCloudSettings> ActiveCloudRules;
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> ActiveCloudTexture;
};


