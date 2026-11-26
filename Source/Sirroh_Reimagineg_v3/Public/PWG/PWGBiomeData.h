#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "PWGBiomeData.generated.h"

class AActor;
class UMaterialInterface;
class UTexture2D;

/**
 * Stable surface-style ids shared by terrain generation and the 3072x3072 biome master atlas.
 * Do not reorder: the numeric value is the atlas section index.
 */
UENUM(BlueprintType)
enum class EPWGSurfaceStyle : uint8
{
	MiniBiome01 = 0,
	MiniBiome02 = 1,
	MiniBiome03 = 2,
	MiniBiome04 = 3,
	Rock = 4,
	Ground = 5,
	MiniBiome05 = 6,
	MiniBiome06 = 7
};

/** Weighted occurrence and patch-size rule for one of MiniBiome01..MiniBiome06. */
USTRUCT(BlueprintType)
struct FPWGMiniBiomeGenerationRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mini Biome")
	bool bEnabled = true;

	/** Relative chance when the generator starts a new surface patch. Zero disables the mini-biome. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mini Biome", meta=(ClampMin="0.0", ClampMax="20.0"))
	float Weight = 1.0f;

	/** Multiplies TerrainStylePatchSizeInTiles for the randomly selected patch. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mini Biome")
	FVector2D PatchLengthMultiplierRange = FVector2D(0.75f, 1.25f);
};

/** Generation controls for one optional playable terrain lane. Middle uses the existing Terrain fields. */
USTRUCT(BlueprintType)
struct FPWGTerrainLaneSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain Lane")
	bool bEnabled = true;

	/** Average height relative to the Middle lane. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain Lane|Shape")
	float HeightOffsetInTiles = 0.0f;

	/** How much of Middle's large-scale silhouette is inherited. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain Lane|Shape", meta=(ClampMin="0.0", ClampMax="1.0"))
	float MiddleShapeInfluence = 0.35f;

	/** Strength of this lane's independent height noise. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain Lane|Shape", meta=(ClampMin="0.0", ClampMax="3.0"))
	float HeightVariationMultiplier = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain Lane|Shape", meta=(ClampMin="0.05", ClampMax="4.0"))
	float NoiseFrequencyMultiplier = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain Lane|Shape")
	int32 NoiseSeedOffset = 193;

	/** Distance between deterministic connection zones. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain Lane|Connections", meta=(ClampMin="12", ClampMax="512"))
	int32 ConnectionSpacingInTiles = 72;

	/** Length on which this lane exactly meets Middle. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain Lane|Connections", meta=(ClampMin="1", ClampMax="64"))
	int32 ConnectionWidthInTiles = 5;

	/** Smooth approach on each side of the shared section. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain Lane|Connections", meta=(ClampMin="1", ClampMax="64"))
	int32 ConnectionBlendInTiles = 9;

	/** Multiplies grass, bushes and tree density assigned to this lane. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain Lane|Vegetation", meta=(ClampMin="0.0", ClampMax="4.0"))
	float VegetationDensityMultiplier = 1.0f;
};

USTRUCT(BlueprintType)
struct FPWGGrassDepthLayer
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Grass")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Grass", meta=(ClampMin="0.0", ClampMax="4.0"))
	float DensityMultiplier = 1.0f;

	/** Local Y range in centimetres. Negative values are closer to the camera. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Grass")
	FVector2D DepthRange = FVector2D(-40.0f, 40.0f);
};

USTRUCT(BlueprintType)
struct FPWGGrassSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Grass")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Grass")
	TSoftObjectPtr<UMaterialInterface> Material;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Grass|Atlas")
	TSoftObjectPtr<UTexture2D> FallbackAtlas = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(
		TEXT("/Game/Systems/Sprite2Bone/Biomes/Meadow/Meadow_Grass/Grass_Meadows.Grass_Meadows")));

	/** Optional atlas overrides for MiniBiome1..4. Empty entries use FallbackAtlas. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Grass|Atlas")
	TArray<TSoftObjectPtr<UTexture2D>> MiniBiomeAtlases;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Grass|Atlas", meta=(ClampMin="1", ClampMax="32"))
	int32 AtlasColumns = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Grass|Atlas", meta=(ClampMin="1", ClampMax="32"))
	int32 AtlasRows = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Grass|Placement", meta=(ClampMin="0.0", ClampMax="64.0"))
	float InstancesPerTile = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Grass|Placement")
	FVector2D WidthRange = FVector2D(45.0f, 95.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Grass|Placement")
	FVector2D HeightRange = FVector2D(45.0f, 110.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Grass|Placement")
	float SurfaceOffset = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Grass|Placement")
	bool bGenerateOnMiniBiome1 = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Grass|Placement")
	bool bGenerateOnMiniBiome2 = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Grass|Placement")
	bool bGenerateOnMiniBiome3 = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Grass|Placement")
	bool bGenerateOnMiniBiome4 = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Grass|Placement")
	bool bGenerateOnRockSurface = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Grass|Placement")
	bool bGenerateOnGroundSurface = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Grass|Placement")
	bool bGenerateOnMiniBiome5 = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Grass|Placement")
	bool bGenerateOnMiniBiome6 = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Grass|Depth Layers")
	FPWGGrassDepthLayer BehindLayer;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Grass|Depth Layers")
	FPWGGrassDepthLayer PlayerLayer;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Grass|Depth Layers")
	FPWGGrassDepthLayer FrontLayer;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Grass|Culling", meta=(ClampMin="0", Units="cm"))
	int32 StartCullDistance = 2000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Grass|Culling", meta=(ClampMin="100", Units="cm"))
	int32 EndCullDistance = 3200;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Grass|Rendering")
	bool bCastShadows = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Grass|Rendering")
	FLinearColor Tint = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Grass|Rendering", meta=(ClampMin="0.0"))
	float Brightness = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Grass|Interaction", meta=(ClampMin="0.0", Units="cm"))
	float InteractionRadius = 110.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Grass|Interaction", meta=(ClampMin="0.0"))
	float InteractionStrength = 55.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Grass|Rendering", meta=(ClampMin="0.0", Units="cm"))
	float PixelWorldStep = 1.0f;

	FPWGGrassSettings()
	{
		BehindLayer.DepthRange = FVector2D(45.0f, 140.0f);
		BehindLayer.DensityMultiplier = 0.8f;
		PlayerLayer.DepthRange = FVector2D(-40.0f, 40.0f);
		PlayerLayer.DensityMultiplier = 1.0f;
		FrontLayer.DepthRange = FVector2D(-140.0f, -55.0f);
		FrontLayer.DensityMultiplier = 0.65f;
	}
};

/** One atlas-driven, non-colliding overhang card for every eligible surface tile. */
USTRUCT(BlueprintType)
struct FPWGBlockOverhangSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Block Overhang")
	bool bEnabled = true;

	/** Optional override. When empty, the biome Grass material is reused. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Block Overhang")
	TSoftObjectPtr<UMaterialInterface> Material = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(
		TEXT("/Game/Systems/Sprite2Bone/Materials/M_PWG_Overhang.M_PWG_Overhang")));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Block Overhang|Atlas")
	TSoftObjectPtr<UTexture2D> FallbackAtlas = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(
		TEXT("/Game/Systems/Sprite2Bone/Biomes/Meadow/Meadow_Overhang/Meadows_Grass_Overhang_Atlas.Meadows_Grass_Overhang_Atlas")));

	/** Optional MiniBiome1..4 overrides. Empty entries use FallbackAtlas. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Block Overhang|Atlas")
	TArray<TSoftObjectPtr<UTexture2D>> MiniBiomeAtlases;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Block Overhang|Atlas", meta=(ClampMin="1", ClampMax="32"))
	int32 AtlasColumns = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Block Overhang|Atlas", meta=(ClampMin="1", ClampMax="32"))
	int32 AtlasRows = 8;

	/** Probability per eligible surface tile. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Block Overhang|Placement", meta=(ClampMin="0.0", ClampMax="1.0"))
	float ChancePerTile = 1.0f;

	/** Width relative to the 100 cm tile. Values above 1 deliberately overlap neighbours. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Block Overhang|Placement", meta=(ClampMin="1.0", ClampMax="2.0"))
	float WidthInTiles = 1.16f;

	/** Uniform multiplier. The source 2:1 aspect ratio is always preserved. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Block Overhang|Placement", meta=(ClampMin="0.1", ClampMax="4.0"))
	float UniformScale = 1.0f;

	/** Additional offset from the centred placement. Negative values move the card below the tile edge. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Block Overhang|Placement", meta=(Units="cm"))
	float VerticalOffset = -5.0f;

	/** Negative Y is toward the camera. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Block Overhang|Placement", meta=(ClampMin="0.0", Units="cm"))
	float DistanceInFrontOfTerrain = 2.0f;

	/**
	 * Tiny deterministic depth separation between neighbouring cards. This keeps the
	 * intentional horizontal overlap while preventing coplanar depth flicker.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Block Overhang|Placement", meta=(ClampMin="0.0", ClampMax="1.0", Units="cm"))
	float OverlapDepthStep = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Block Overhang|Placement")
	bool bGenerateOnMiniBiome1 = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Block Overhang|Placement")
	bool bGenerateOnMiniBiome2 = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Block Overhang|Placement")
	bool bGenerateOnMiniBiome3 = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Block Overhang|Placement")
	bool bGenerateOnMiniBiome4 = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Block Overhang|Placement")
	bool bGenerateOnRockSurface = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Block Overhang|Placement")
	bool bGenerateOnGroundSurface = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Block Overhang|Placement")
	bool bGenerateOnMiniBiome5 = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Block Overhang|Placement")
	bool bGenerateOnMiniBiome6 = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Block Overhang|Culling", meta=(ClampMin="0", Units="cm"))
	int32 StartCullDistance = 2000;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Block Overhang|Culling", meta=(ClampMin="100", Units="cm"))
	int32 EndCullDistance = 3400;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Block Overhang|Rendering")
	FLinearColor Tint = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Block Overhang|Rendering", meta=(ClampMin="0.0"))
	float Brightness = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Block Overhang|Rendering")
	bool bCastShadows = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Block Overhang|Rendering", meta=(ClampMin="0.0", Units="cm"))
	float PixelWorldStep = 1.0f;

	/** Wind strength relative to biome grass. 0.5 means half of the grass sway. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Block Overhang|Wind", meta=(ClampMin="0.0", ClampMax="2.0"))
	float WindStrengthMultiplier = 0.5f;
};

USTRUCT(BlueprintType)
struct FPWGBiomeWindSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Wind", meta=(ClampMin="0.0"))
	float MinimumStrength = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Wind", meta=(ClampMin="0.0"))
	float MaximumStrength = 14.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Wind", meta=(ClampMin="5.0", Units="s"))
	float StrengthCycleSeconds = 24.0f;

	/** Time required for one very slow 360-degree turn around the biome. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Wind", meta=(ClampMin="10.0", Units="s"))
	float DirectionCycleSeconds = 180.0f;
};

/** One depth batch of procedural trees. One HISM is created per atlas and enabled layer. */
USTRUCT(BlueprintType)
struct FPWGTreeDepthLayer
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees", meta=(ClampMin="0.0", ClampMax="4.0"))
	float DensityMultiplier = 1.0f;

	/** Local Y range in centimetres. Positive values are behind the player. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees")
	FVector2D DepthRange = FVector2D(80.0f, 145.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees", meta=(ClampMin="0.1", ClampMax="4.0"))
	float ScaleMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Culling", meta=(ClampMin="0", Units="cm"))
	int32 StartCullDistance = 6000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Culling", meta=(ClampMin="100", Units="cm"))
	int32 EndCullDistance = 10000;
};

/** Atlas and density override for one surface style. */
USTRUCT(BlueprintType)
struct FPWGTreeMiniBiomeSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees")
	bool bEnabled = true;

	/** Empty means FallbackAtlas. Every override must use the same atlas layout. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees")
	TSoftObjectPtr<UTexture2D> AtlasOverride;

	/** Average number of trees along 100 metres of this mini-biome. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees", meta=(ClampMin="0.0", ClampMax="100.0"))
	float TreesPer100Metres = 3.0f;
};

/** Configurable placement for one category of modular tree parts. */
USTRUCT(BlueprintType)
struct FPWGTreePartRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Parts", meta=(ClampMin="0"))
	int32 FirstFrame = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Parts", meta=(ClampMin="1", ClampMax="64"))
	int32 FrameCount = 4;

	/** Inclusive number of parts added to each tree. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Parts")
	FIntPoint CountRange = FIntPoint(1, 1);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Parts")
	FVector2D WidthFractionRange = FVector2D(0.18f, 0.28f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Parts")
	FVector2D HeightFractionRange = FVector2D(0.72f, 0.95f);

	/** Random X offset as a fraction of whole-tree height. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Parts")
	FVector2D HorizontalOffsetFractionRange = FVector2D(-0.02f, 0.02f);

	/** Bottom attachment height as a fraction of whole-tree height. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Parts")
	FVector2D VerticalOffsetFractionRange = FVector2D(0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Parts")
	FVector2D WindInfluenceRange = FVector2D(0.01f, 0.05f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Parts")
	bool bAllowHorizontalMirror = true;
};

USTRUCT(BlueprintType)
struct FPWGTreeSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees")
	TSoftObjectPtr<UMaterialInterface> Material;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Atlas")
	TSoftObjectPtr<UTexture2D> FallbackAtlas = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(
		TEXT("/Game/Systems/Sprite2Bone/Biomes/Meadow/Meadow_Procedural_Trees/PWG_Tree_FallbackAtlas_01.PWG_Tree_FallbackAtlas_01")));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Atlas", meta=(ClampMin="1", ClampMax="32"))
	int32 AtlasColumns = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Atlas", meta=(ClampMin="1", ClampMax="32"))
	int32 AtlasRows = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Placement", meta=(ClampMin="100.0", Units="cm"))
	float MinimumSpacing = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Placement", meta=(ClampMin="50.0", Units="cm"))
	FVector2D TreeHeightRange = FVector2D(380.0f, 680.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Placement")
	float SurfaceOffset = 0.5f;

	/** The first seven atlas cells are seamless bark used by rotated tapered segments. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Procedural Skeleton", meta=(ClampMin="0"))
	int32 TrunkFirstFrame = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Procedural Skeleton", meta=(ClampMin="1", ClampMax="16"))
	int32 TrunkFrameCount = 7;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Procedural Skeleton", meta=(ClampMin="0"))
	int32 JunctionFrame = 6;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Procedural Skeleton", meta=(ClampMin="0"))
	int32 EndCapFrame = 7;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Procedural Skeleton", meta=(ClampMin="0"))
	int32 LeavesFirstFrame = 8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Procedural Skeleton", meta=(ClampMin="1", ClampMax="16"))
	int32 LeavesFrameCount = 8;

	/** Visible bark width inside every 64 px trunk cell. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Procedural Skeleton", meta=(ClampMin="1.0", ClampMax="64.0"))
	float TrunkTextureWidthPixels = 55.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Procedural Skeleton", meta=(ClampMin="1.0", ClampMax="64.0"))
	float JunctionBaseWidthPixels = 30.0f;

	/** A cap is emitted only when the terminal branch is no wider than this in atlas-pixel space. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Procedural Skeleton", meta=(ClampMin="1.0", ClampMax="64.0"))
	float EndCapMaximumWidthPixels = 32.0f;

	/** Absolute atlas-pixel centres of the two attachment zones in JunctionFrame. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Procedural Skeleton")
	FVector2D JunctionLeftAttachmentPixel = FVector2D(142.0f, 70.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Procedural Skeleton")
	FVector2D JunctionRightAttachmentPixel = FVector2D(184.0f, 68.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Procedural Skeleton", meta=(ClampMin="20.0", Units="cm"))
	FVector2D SegmentLengthRange = FVector2D(65.0f, 105.0f);

	/** Small overlap along the branch axis that hides wedge-shaped gaps between bent segments. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Procedural Skeleton", meta=(ClampMin="0.0", ClampMax="10.0", Units="cm"))
	float SegmentJoinOverlap = 2.0f;

	/** Stable sub-centimetre depth order for overlapping bark segments. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Procedural Skeleton", meta=(ClampMin="0.0", ClampMax="0.5", Units="cm"))
	float SegmentJoinDepthStep = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Procedural Skeleton", meta=(ClampMin="1.0", Units="cm"))
	FVector2D BaseWidthRange = FVector2D(42.0f, 78.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Procedural Skeleton", meta=(ClampMin="1.0", Units="cm"))
	FVector2D TipWidthRange = FVector2D(8.0f, 18.0f);

	/** Random direction change applied to every new trunk/branch segment. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Procedural Skeleton")
	FVector2D TurnDegreesRange = FVector2D(-9.0f, 9.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Procedural Skeleton", meta=(ClampMin="0.0", ClampMax="60.0"))
	float MaximumLeanDegrees = 28.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Procedural Skeleton", meta=(ClampMin="0.0", ClampMax="1.0"))
	float BranchStartHeightFraction = 0.20f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Procedural Skeleton", meta=(ClampMin="0.0", ClampMax="1.0"))
	float BranchChancePerSegment = 0.42f;

	/** Guaranteed side branches on the main trunk; prevents pole-shaped trees. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Procedural Skeleton", meta=(ClampMin="0", ClampMax="8"))
	int32 MinimumPrimaryBranches = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Procedural Skeleton", meta=(ClampMin="0", ClampMax="12"))
	int32 MaximumPrimaryBranches = 7;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Procedural Skeleton", meta=(ClampMin="0", ClampMax="3"))
	int32 MaximumBranchDepth = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Procedural Skeleton")
	FVector2D BranchAngleRange = FVector2D(28.0f, 55.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Procedural Skeleton")
	FVector2D BranchLengthFractionRange = FVector2D(0.16f, 0.36f);

	/** Moves every child branch slightly towards the camera to give overlapping trunks a stable draw order. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Procedural Skeleton", meta=(ClampMin="0.0", ClampMax="5.0", Units="cm"))
	float BranchOverlapDepthOffset = 0.35f;

	/** Hard safety budget for geometry complexity of one tree. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Procedural Skeleton", meta=(ClampMin="4", ClampMax="256"))
	int32 MaximumSegmentsPerTree = 56;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Procedural Crown", meta=(ClampMin="1", ClampMax="32"))
	FIntPoint LeafCardsPerTipRange = FIntPoint(1, 2);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Procedural Crown", meta=(ClampMin="10.0", Units="cm"))
	FVector2D LeafCardSizeRange = FVector2D(110.0f, 230.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Procedural Crown", meta=(ClampMin="0.0", Units="cm"))
	FVector2D CrownSpreadRange = FVector2D(45.0f, 125.0f);

	/** Maximum horizontal offset as a fraction of leaf-card size; keeps the branch tip inside its crown. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Procedural Crown", meta=(ClampMin="0.0", ClampMax="0.5"))
	float CrownMaximumHorizontalOffsetFraction = 0.38f;

	/** Random vertical movement around the attached crown position, relative to leaf-card size. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Procedural Crown", meta=(ClampMin="0.0", ClampMax="0.3"))
	float CrownVerticalJitterFraction = 0.10f;

	/** Subtle per-card shade variation. Values are clamped to a safe non-overexposed range in C++. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Procedural Crown")
	FVector2D LeafBrightnessVariation = FVector2D(0.70f, 1.25f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Mini Biomes")
	FPWGTreeMiniBiomeSettings MiniBiome1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Mini Biomes")
	FPWGTreeMiniBiomeSettings MiniBiome2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Mini Biomes")
	FPWGTreeMiniBiomeSettings MiniBiome3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Mini Biomes")
	FPWGTreeMiniBiomeSettings MiniBiome4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Mini Biomes")
	FPWGTreeMiniBiomeSettings RockSurface;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Mini Biomes")
	FPWGTreeMiniBiomeSettings GroundSurface;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Mini Biomes")
	FPWGTreeMiniBiomeSettings MiniBiome5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Mini Biomes")
	FPWGTreeMiniBiomeSettings MiniBiome6;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Depth Layers")
	FPWGTreeDepthLayer BackLayer;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Depth Layers")
	FPWGTreeDepthLayer MiddleLayer;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Depth Layers")
	FPWGTreeDepthLayer FrontLayer;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Legacy Parts", meta=(AdvancedDisplay))
	FPWGTreePartRule Trunk;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Legacy Parts", meta=(AdvancedDisplay))
	FPWGTreePartRule LargeCrown;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Legacy Parts", meta=(AdvancedDisplay))
	FPWGTreePartRule SmallCrowns;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Legacy Parts", meta=(AdvancedDisplay))
	FPWGTreePartRule Accents;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Rendering")
	bool bCastShadows = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Rendering")
	FLinearColor Tint = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trees|Rendering", meta=(ClampMin="0.0"))
	float Brightness = 1.0f;

	FPWGTreeSettings()
	{
		MiniBiome1.TreesPer100Metres = 3.0f;
		MiniBiome2.TreesPer100Metres = 14.0f;
		MiniBiome3.TreesPer100Metres = 4.0f;
		MiniBiome4.TreesPer100Metres = 2.0f;
		RockSurface.bEnabled = false;
		RockSurface.TreesPer100Metres = 0.0f;
		GroundSurface.bEnabled = false;
		GroundSurface.TreesPer100Metres = 0.0f;
		MiniBiome5.TreesPer100Metres = 2.0f;
		MiniBiome6.TreesPer100Metres = 1.0f;

		BackLayer.DepthRange = FVector2D(80.0f, 145.0f);
		BackLayer.StartCullDistance = 2200;
		BackLayer.EndCullDistance = 3400;
		MiddleLayer.DepthRange = FVector2D(-35.0f,35.0f);
		MiddleLayer.DensityMultiplier = 0.35f;
		MiddleLayer.StartCullDistance = 2100;
		MiddleLayer.EndCullDistance = 3300;
		FrontLayer.DepthRange = FVector2D(-145.0f, -80.0f);
		FrontLayer.DensityMultiplier = 0.12f;
		FrontLayer.ScaleMultiplier = 1.12f;
		FrontLayer.StartCullDistance = 1900;
		FrontLayer.EndCullDistance = 3100;

		Trunk.FirstFrame = 0;
		Trunk.FrameCount = 4;
		LargeCrown.FirstFrame = 4;
		LargeCrown.FrameCount = 4;
		LargeCrown.WidthFractionRange = FVector2D(0.58f, 0.82f);
		LargeCrown.HeightFractionRange = FVector2D(0.45f, 0.68f);
		LargeCrown.HorizontalOffsetFractionRange = FVector2D(-0.06f, 0.06f);
		LargeCrown.VerticalOffsetFractionRange = FVector2D(0.38f, 0.52f);
		LargeCrown.WindInfluenceRange = FVector2D(0.45f, 0.72f);

		SmallCrowns.FirstFrame = 8;
		SmallCrowns.FrameCount = 4;
		SmallCrowns.CountRange = FIntPoint(0, 3);
		SmallCrowns.WidthFractionRange = FVector2D(0.25f, 0.48f);
		SmallCrowns.HeightFractionRange = FVector2D(0.24f, 0.46f);
		SmallCrowns.HorizontalOffsetFractionRange = FVector2D(-0.30f, 0.30f);
		SmallCrowns.VerticalOffsetFractionRange = FVector2D(0.46f, 0.80f);
		SmallCrowns.WindInfluenceRange = FVector2D(0.62f, 1.0f);

		Accents.FirstFrame = 12;
		Accents.FrameCount = 4;
		Accents.CountRange = FIntPoint(0, 2);
		Accents.WidthFractionRange = FVector2D(0.26f, 0.50f);
		Accents.HeightFractionRange = FVector2D(0.22f, 0.48f);
		Accents.HorizontalOffsetFractionRange = FVector2D(-0.18f, 0.18f);
		Accents.VerticalOffsetFractionRange = FVector2D(0.0f, 0.48f);
		Accents.WindInfluenceRange = FVector2D(0.08f, 0.28f);
	}
};

USTRUCT(BlueprintType)
struct FPWGObjectSpawnRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn")
	TSoftClassPtr<AActor> ActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn", meta=(ClampMin="0.0", ClampMax="1.0"))
	float ChancePerSurfaceTile = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn", meta=(ClampMin="0"))
	int32 MinimumSpacingInTiles = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn")
	FVector SpawnOffset = FVector::ZeroVector;
};

/** One seamless, alpha-capable panorama behind the playable terrain. */
USTRUCT(BlueprintType)
struct FPWGBackgroundLayer
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Background")
	TSoftObjectPtr<UTexture2D> Texture;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Background")
	FLinearColor Tint = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Background", meta=(ClampMin="0.0"))
	float Brightness = 1.0f;

	/** 0 keeps original colour, 1 fully applies DistanceFadeColor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Background|Atmosphere", meta=(ClampMin="0.0", ClampMax="1.0"))
	float DistanceFadeAmount = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Background|Atmosphere")
	FLinearColor DistanceFadeColor = FLinearColor(0.34f, 0.36f, 0.48f, 1.0f);

	/** Distance behind the rear edge of the playable terrain. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Background")
	float DistanceBehindTerrain = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Background")
	float VerticalOffset = 300.0f;

	/** Height of one repeated landscape image in world units (100 units = 1 metre). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Background", meta=(ClampMin="10.0"))
	float WorldHeight = 900.0f;

	/**
	 * Width of one repeated landscape image in world units. Set to 0 to derive
	 * it automatically from WorldHeight and the source texture aspect ratio.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Background", meta=(ClampMin="0.0"))
	float WorldWidth = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Background")
	float ScrollSpeed = 0.0f;

	/** Apparent horizontal movement: 0 stays almost fixed on screen, 1 moves like foreground terrain. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Background|Parallax", meta=(ClampMin="0.0", ClampMax="0.95"))
	float CameraFollowFactor = 0.65f;

	/** Alpha at or above this value is treated as the visible landscape silhouette. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Background|Sprite Placement", meta=(ClampMin="0.0", ClampMax="1.0"))
	float SilhouetteAlphaThreshold = 0.08f;

	/** Pushes the base of scenery this many source-texture pixels below the landscape silhouette. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Background|Sprite Placement", meta=(ClampMin="0.0", ClampMax="64.0"))
	float SceneryBurialPixels = 8.0f;

	/** Used only when CPU alpha data cannot be read. 0 is quad bottom, 1 is quad top. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Background|Sprite Placement", meta=(ClampMin="0.0", ClampMax="1.0"))
	float FallbackSilhouetteHeight = 0.55f;
};

USTRUCT(BlueprintType)
struct FPWGSurfaceOverhangSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Surface Overhang")
	TSoftObjectPtr<UTexture2D> Texture;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Surface Overhang")
	FLinearColor Tint = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Surface Overhang", meta=(ClampMin="0.0"))
	float Brightness = 1.0f;

	/** Height controls uniform scale; texture width is preserved from the PNG aspect ratio. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Surface Overhang", meta=(ClampMin="1.0"))
	float WorldHeight = 45.0f;

	/** Additional uniform scale. Aspect ratio is always preserved. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Surface Overhang", meta=(ClampMin="0.01", UIMin="0.1", UIMax="10.0"))
	float UniformScale = 1.0f;

	/** Vertical adjustment relative to the generated surface. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Surface Overhang")
	float VerticalOffset = 0.0f;

	/** Normalised beginning of this overhang on the chunk: 0 = left edge, 1 = right edge. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Surface Overhang|Coverage", meta=(ClampMin="0.0", ClampMax="1.0"))
	float StartFraction = 0.0f;

	/** Normalised end of this overhang on the chunk. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Surface Overhang|Coverage", meta=(ClampMin="0.0", ClampMax="1.0"))
	float EndFraction = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Surface Overhang", meta=(ClampMin="0.0"))
	float DistanceInFrontOfTerrain = 2.0f;
};

/** Random non-colliding sprite placed on one of the configured background layers. */
USTRUCT(BlueprintType)
struct FPWGScenerySpriteRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scenery")
	TSoftObjectPtr<UTexture2D> Texture;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scenery", meta=(ClampMin="0.0", ClampMax="1.0"))
	float ChancePerSurfaceTile = 0.02f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scenery")
	FVector2D MinimumSize = FVector2D(150.0f, 250.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scenery")
	FVector2D MaximumSize = FVector2D(300.0f, 500.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scenery")
	float HeightOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scenery", meta=(ClampMin="0", ClampMax="2"))
	int32 MinimumLayer = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scenery", meta=(ClampMin="0", ClampMax="2"))
	int32 MaximumLayer = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scenery")
	FLinearColor Tint = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scenery", meta=(ClampMin="0.0"))
	float Brightness = 1.0f;

	/** Enable for generated 8x5 _38VT atlases; suffix detection also enables it automatically. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scenery|Atlas")
	bool bDirectional38Atlas = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scenery|Atlas", meta=(ClampMin="0", ClampMax="37"))
	int32 FixedFrameIndex = 0;

	/** Match the fog/tint of the landscape layer on which this sprite is placed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scenery|Atmosphere")
	bool bInheritLandscapeAtmosphere = true;
};

/** Lightweight batched sprites placed around the playable three-tile terrain strip. */
USTRUCT(BlueprintType)
struct FPWGTerrainSpriteRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain Scenery")
	TSoftObjectPtr<UTexture2D> Texture;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain Scenery", meta=(ClampMin="0.0", ClampMax="1.0"))
	float ChancePerSurfaceTile = 0.03f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain Scenery")
	FVector2D MinimumSize = FVector2D(80.0f, 100.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain Scenery")
	FVector2D MaximumSize = FVector2D(180.0f, 300.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain Scenery")
	float MinimumDepthOffset = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain Scenery")
	float MaximumDepthOffset = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain Scenery")
	float SurfaceBurialDepth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain Scenery")
	float HeightOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain Scenery")
	FLinearColor Tint = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain Scenery", meta=(ClampMin="0.0"))
	float Brightness = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain Scenery|Atlas")
	bool bDirectional38Atlas = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain Scenery|Atlas", meta=(ClampMin="0", ClampMax="37"))
	int32 FixedFrameIndex = 0;
};

/** Low repeating panorama whose lower edge follows the generated surface. */
USTRUCT(BlueprintType)
struct FPWGVegetationLayer
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vegetation")
	TSoftObjectPtr<UTexture2D> Texture;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vegetation")
	FLinearColor Tint = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vegetation", meta=(ClampMin="0.0"))
	float Brightness = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vegetation")
	float DistanceBehindTerrain = 350.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vegetation")
	float VerticalOffset = -35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vegetation", meta=(ClampMin="10.0"))
	float WorldHeight = 300.0f;

	/** Width of one repeat. 0 preserves the texture aspect ratio from WorldHeight. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vegetation", meta=(ClampMin="0.0"))
	float WorldWidth = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vegetation|Atmosphere", meta=(ClampMin="0.0", ClampMax="1.0"))
	float DistanceFadeAmount = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vegetation|Atmosphere")
	FLinearColor DistanceFadeColor = FLinearColor(0.34f, 0.36f, 0.48f, 1.0f);
};

USTRUCT(BlueprintType)
struct FPWGCloudSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Clouds")
	TSoftObjectPtr<UTexture2D> Texture;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Clouds", meta=(ClampMin="0", ClampMax="64"))
	int32 Count = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Clouds")
	FVector2D MinimumSize = FVector2D(350.0f, 140.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Clouds")
	FVector2D MaximumSize = FVector2D(750.0f, 300.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Clouds")
	FVector2D HeightRange = FVector2D(450.0f, 1200.0f);

	/** World units per second. Typical gentle values are 5-20. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Clouds")
	FVector2D SpeedRange = FVector2D(5.0f, 18.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Clouds")
	float DepthBehindTerrain = 2300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Clouds")
	FLinearColor Tint = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Clouds", meta=(ClampMin="0.0"))
	float Brightness = 1.0f;
};

/** One 6x6 autotile family inside the 12x12 Terrerian atlas. */
USTRUCT(BlueprintType)
struct FPWGTerrainAtlasStyle
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Atlas")
	FName StyleName = TEXT("Terrain");

	// Index of the upper-left tile of this style's 6x6 block.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Atlas", meta=(ClampMin="0"))
	int32 BaseTileIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Atlas", meta=(ClampMin="0.0"))
	float Weight = 1.0f;

	FPWGTerrainAtlasStyle() = default;
	FPWGTerrainAtlasStyle(const FName InName, const int32 InBaseTileIndex, const float InWeight)
		: StyleName(InName), BaseTileIndex(InBaseTileIndex), Weight(InWeight)
	{
	}
};

USTRUCT(BlueprintType)
struct FBiomeData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="World", meta=(ClampMin="1"))
	int32 ChunkLengthInTiles = 256;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="World", meta=(ClampMin="1.0"))
	float TileSize = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="World", meta=(ClampMin="1"))
	int32 MinimumUndergroundDepth = 12;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain")
	int32 BaseSurfaceHeightInTiles = 14;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain", meta=(ClampMin="0"))
	int32 HeightVariationInTiles = 7;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain", meta=(ClampMin="0.0001"))
	float NoiseFrequency = 0.035f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain", meta=(ClampMin="1"))
	int32 MaximumStepInTiles = 3;

	// Maximum rise over one horizontal tile. 0.35 means 35 cm per 100 cm.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain|Smooth Surface", meta=(ClampMin="0.01", ClampMax="1.0"))
	float MaximumSurfaceSlope = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain|Smooth Surface", meta=(ClampMin="1", ClampMax="16"))
	int32 SurfaceSubdivisionsPerTile = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain|Smooth Surface", meta=(ClampMin="0", ClampMax="8"))
	int32 SurfaceSmoothingPasses = 3;

	/** Width of one pseudo-pixel terrain segment. 20 cm gives five segments per metre. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain|Pseudo Pixel", meta=(ClampMin="5.0", ClampMax="100.0", Units="cm"))
	float HorizontalSurfaceStep = 10.0f;

	/** Smallest vertical change of the generated surface. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain|Pseudo Pixel", meta=(ClampMin="5.0", ClampMax="100.0", Units="cm"))
	float VerticalSurfaceStep = 10.0f;

	/**
	 * Controls how long successive height levels are held. 1 follows the
	 * 1,2,2,3,3,3,3,4... pattern; 0 disables extra plateaus. Negative values
	 * reverse the bias, producing narrower high ridges for mountain biomes.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain|Pseudo Pixel", meta=(ClampMin="-2.0", ClampMax="2.0"))
	float PseudoPixelPlateauSmoothing = 1.0f;

	/** Maximum number of vertical steps allowed between adjacent surface segments. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain|Pseudo Pixel", meta=(ClampMin="1", ClampMax="16"))
	int32 MaximumVerticalStepsPerSegment = 1;

	/** Radius used to smooth only the invisible walking collision; visuals remain pixel-stepped. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain|Pseudo Pixel", meta=(ClampMin="0.0", ClampMax="200.0", Units="cm"))
	float WalkingCollisionSmoothingRadius = 50.0f;

	/** Minimum terrain-height blend at both ends of a biome region. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain|Biome Transition", meta=(ClampMin="0", ClampMax="128"))
	int32 BiomeEdgeSmoothingDistanceInTiles = 32;

	/** Maximum average rise/run used only while blending two main biomes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain|Biome Transition", meta=(ClampMin="0.02", ClampMax="0.5"))
	float BiomeTransitionMaximumSlope = 0.12f;

	/** Optional playable lane nearest the camera. Disable to return to two/one-layer terrain. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain|Playable Lanes")
	FPWGTerrainLaneSettings FrontLane;

	/** Optional playable lane behind Middle. Disable independently from Front. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain|Playable Lanes")
	FPWGTerrainLaneSettings BackLane;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain")
	int32 Seed = 1337;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Grass")
	FPWGGrassSettings Grass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Block Overhang")
	FPWGBlockOverhangSettings BlockOverhang;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Wind")
	FPWGBiomeWindSettings Wind;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Trees")
	FPWGTreeSettings Trees;

	/**
	 * One 4096x4096 texture shared by this main biome's procedural trees, grass,
	 * clouds, bushes, block overhangs and its Close/Mid/Far landscapes.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Atlas|Biome Master")
	TSoftObjectPtr<UTexture2D> BiomeMasterAtlas = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(
		TEXT("/Game/Systems/Sprite2Bone/Biomes/Meadow/Meadows_MasterAtlas.Meadows_MasterAtlas")));

	/** Enables all master-atlas regions. Missing texture falls back to the old per-system assets. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Atlas|Biome Master")
	bool bUseBiomeMasterAtlas = true;

	/** Rules 0..5 correspond to MiniBiome01..MiniBiome06. Weight zero or Disabled excludes a type. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Atlas|Biome Master", meta=(TitleProperty="Weight"))
	TArray<FPWGMiniBiomeGenerationRule> MiniBiomeGenerationRules;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Atlas")
	TSoftObjectPtr<UMaterialInterface> AtlasMaterial;

	/** Stylized colour at the exposed terrain surface. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain|Stylized Lighting")
	FLinearColor TerrainSurfaceLightColor = FLinearColor(1.0f, 0.88f, 0.42f, 1.0f);

	/** Stylized colour reached deep underground. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain|Stylized Lighting")
	FLinearColor TerrainDeepLightColor = FLinearColor(0.025f, 0.045f, 0.16f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain|Stylized Lighting", meta=(ClampMin="0.0", Units="cm"))
	float TerrainGradientStartDepth = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain|Stylized Lighting", meta=(ClampMin="1.0", Units="cm"))
	float TerrainGradientFullDepth = 1200.0f;

	/** Maximum depth encoded into the visual mesh vertex data. Increase only for exceptionally deep visible cuts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain|Visual Layers", meta=(ClampMin="100.0", Units="cm"))
	float TerrainVisualDepthRange = 3200.0f;

	/** Size of one stable pseudo-pixel used to roughen the MiniBiome/Ground/Rock boundaries. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain|Visual Layers", meta=(ClampMin="1.0", Units="cm"))
	float TerrainLayerNoisePixelSize = 12.5f;

	/** Maximum vertical displacement of the visual material boundaries. Does not affect collision. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain|Visual Layers", meta=(ClampMin="0.0", Units="cm"))
	float TerrainLayerNoiseStrength = 38.0f;

	/** Small softness around a noisy boundary. Low values retain a crisp pixel-art edge. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain|Visual Layers", meta=(ClampMin="0.5", Units="cm"))
	float TerrainLayerBlendWidth = 6.0f;

	/** Depth over which the original surface autotile is retained before switching to seamless centre tiles. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain|Visual Layers", meta=(ClampMin="1.0", Units="cm"))
	float TerrainSurfaceDetailDepth = 100.0f;

	/** 9x9 atlas made from nine independent 3x3 blocks of 80 px tiles. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Atlas|Compact Blocks")
	TSoftObjectPtr<UTexture2D> CompactBlockAtlasTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(
		TEXT("/Game/Systems/Sprite2Bone/Biomes/Meadow/Meadow_Blocks/Meadow_Blocks_Texture.Meadow_Blocks_Texture")));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Atlas|Compact Blocks")
	bool bUseCompactBlockAtlas = true;

	/** Mini-biome material depth in one-metre underground blocks. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Atlas|Compact Blocks", meta=(ClampMin="1", ClampMax="64"))
	int32 CompactBiomeDepthInTiles = 12;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Atlas|Compact Blocks", meta=(ClampMin="1", ClampMax="16"))
	int32 CompactGroundDepthMin = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Atlas|Compact Blocks", meta=(ClampMin="1", ClampMax="16"))
	int32 CompactGroundDepthMax = 6;

	/** Rock thickness below Ground before the new DeepRock texture takes over. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Atlas|Compact Blocks", meta=(ClampMin="1", ClampMax="128"))
	int32 CompactRockDepthInTiles = 16;

	/** Ground columns inserted between successive surface mini-biomes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Atlas|Compact Blocks", meta=(ClampMin="1", ClampMax="16"))
	int32 CompactSurfaceGroundGapMin = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Atlas|Compact Blocks", meta=(ClampMin="1", ClampMax="16"))
	int32 CompactSurfaceGroundGapMax = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Atlas", meta=(ClampMin="1"))
	int32 AtlasColumns = 12;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Atlas", meta=(ClampMin="1"))
	int32 AtlasRows = 12;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Atlas")
	TArray<int32> SurfaceTileIndices = { 0 };

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Atlas")
	TArray<int32> UndergroundTileIndices = { 1 };

	// Terrerian.png contains four 6x6 autotile families in a 12x12 atlas.
	// The generator chooses coherent patches, then resolves top/bottom/left/right/interior tiles.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Atlas|Terrerian")
	TArray<FPWGTerrainAtlasStyle> TerrainStyles = {
		FPWGTerrainAtlasStyle(TEXT("Cegla"), 0, 1.0f),
		FPWGTerrainAtlasStyle(TEXT("Zwir"), 6, 1.0f),
		FPWGTerrainAtlasStyle(TEXT("ZielonaTrawa"), 72, 1.0f),
		FPWGTerrainAtlasStyle(TEXT("SuchaTrawa"), 78, 1.0f)
	};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Atlas|Terrerian", meta=(ClampMin="2.0"))
	float TerrainStylePatchSizeInTiles = 28.0f;

	/** Pure MiniBiome2 columns kept at both chunk edges so different main-biome atlases can meet safely. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Atlas|Universal Grid", meta=(ClampMin="4", ClampMax="32"))
	int32 MiniBiome2BoundaryWidth = 6;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Caves")
	bool bGenerateCaves = true;

	/**
	 * Enables the old per-voxel Perlin prototype. It can create isolated square
	 * openings and is intentionally off until caves are generated as connected
	 * regions with explicit entrances and walls.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Caves|Experimental")
	bool bEnableExperimentalVoxelCarving = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Caves", meta=(ClampMin="0.001"))
	float CaveNoiseFrequency = 0.085f;

	// Perlin noise is in [-1,1]. Larger values create fewer caves.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Caves", meta=(ClampMin="-1.0", ClampMax="1.0"))
	float CaveNoiseThreshold = 0.48f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Caves", meta=(ClampMin="1"))
	int32 CaveMinimumDepthBelowSurface = 3;

	/** Draws an opaque textured wall behind excavated cells instead of exposing the sky panorama. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Caves")
	bool bRenderCaveBackWalls = true;

	/** Additional depth behind the first one-metre cave layer. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Caves", meta=(ClampMin="0.0", ClampMax="50.0"))
	float CaveBackWallInset = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Decoration")
	TArray<int32> DecorationTileIndices;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Decoration", meta=(ClampMin="0.0", ClampMax="1.0"))
	float DecorationChancePerTile = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Decoration", meta=(ClampMin="0.01"))
	float DecorationWidthInTiles = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Decoration", meta=(ClampMin="0.01"))
	float DecorationHeightInTiles = 0.6f;

	// Negative Y is the camera-facing side in the default sidescroller setup.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Decoration")
	float DecorationDepthOffsetInTiles = -1.51f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn")
	TArray<FPWGObjectSpawnRule> ObjectSpawnRules;

	// ---------- 2D environment ----------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Materials")
	TSoftObjectPtr<UMaterialInterface> EnvironmentMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Materials")
	TSoftObjectPtr<UMaterialInterface> EnvironmentVirtualTextureMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Sky")
	TSoftObjectPtr<UTexture2D> SkyPanoramaTexture;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Sky")
	FLinearColor SkyTint = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Sky", meta=(ClampMin="1000.0"))
	FVector2D SkySize = FVector2D(7000.0f, 4000.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Sky")
	float SkyDepth = 2500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Sky")
	float SkyScrollSpeed = 0.003f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Celestial")
	TSoftObjectPtr<UTexture2D> SunTexture;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Celestial")
	TSoftObjectPtr<UTexture2D> MoonTexture;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Celestial")
	FVector2D SunSize = FVector2D(450.0f, 450.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Celestial")
	FVector2D MoonSize = FVector2D(380.0f, 380.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Celestial", meta=(ClampMin="10.0"))
	float DayCycleDurationSeconds = 240.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Celestial")
	float CelestialOrbitRadius = 2200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Celestial")
	float CelestialVerticalOffset = 500.0f;

	/** Horizontal/vertical orbit kept inside the normal side-scroller view. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Celestial", meta=(ClampMin="0.0"))
	FVector2D CelestialOrbitSize = FVector2D(700.0f, 420.0f);

	/** Manual X/Z correction relative to the player and camera view. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Celestial")
	FVector2D CelestialScreenOffset = FVector2D(0.0f, 350.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Landscape")
	FPWGBackgroundLayer LandscapeClose;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Landscape")
	FPWGBackgroundLayer LandscapeMid;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Landscape")
	FPWGBackgroundLayer LandscapeFar;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Vegetation")
	FPWGVegetationLayer LandscapeVegetation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Background", meta=(TitleProperty="Texture"))
	TArray<FPWGScenerySpriteRule> BackgroundSpriteRules;

	/** Grass/pebbles scattered without collision between background layers. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Background", meta=(TitleProperty="Texture"))
	TArray<FPWGScenerySpriteRule> BackgroundDetailRules;

	/** Batched quads about one metre behind the rear edge of PWG terrain. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Terrain Scenery", meta=(TitleProperty="Texture"))
	TArray<FPWGTerrainSpriteRule> TerrainRearSpriteRules;

	/** Batched quads randomly placed through the three-metre playable depth strip. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Terrain Scenery", meta=(TitleProperty="Texture"))
	TArray<FPWGTerrainSpriteRule> TerrainDepthSpriteRules;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Clouds")
	FPWGCloudSettings Clouds;

	/** Multiple cloud textures/rules. When non-empty, replaces the legacy Clouds entry. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Clouds", meta=(TitleProperty="Texture"))
	TArray<FPWGCloudSettings> CloudRules;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Surface Overhang")
	FPWGSurfaceOverhangSettings SurfaceOverhang;

	/**
	 * Multiple independently scaled overhangs. Each entry can cover a different
	 * normalised section of the generated chunk. When non-empty, this replaces
	 * the legacy single SurfaceOverhang field above.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Surface Overhang", meta=(TitleProperty="Texture"))
	TArray<FPWGSurfaceOverhangSettings> SurfaceOverhangs;

	FBiomeData()
	{
		MiniBiomeGenerationRules.SetNum(6);
		// MiniBiome05 is uncommon. MiniBiome06 is intentionally tiny and very rare.
		MiniBiomeGenerationRules[4].Weight = 0.35f;
		MiniBiomeGenerationRules[4].PatchLengthMultiplierRange = FVector2D(0.45f, 0.80f);
		MiniBiomeGenerationRules[5].Weight = 0.06f;
		MiniBiomeGenerationRules[5].PatchLengthMultiplierRange = FVector2D(0.12f, 0.28f);

		FrontLane.HeightOffsetInTiles = -0.8f;
		FrontLane.MiddleShapeInfluence = 0.45f;
		FrontLane.HeightVariationMultiplier = 0.30f;
		FrontLane.NoiseFrequencyMultiplier = 0.45f;
		FrontLane.NoiseSeedOffset = 193;
		FrontLane.ConnectionSpacingInTiles = 68;
		FrontLane.ConnectionWidthInTiles = 5;
		FrontLane.ConnectionBlendInTiles = 10;

		BackLane.HeightOffsetInTiles = 0.35f;
		BackLane.MiddleShapeInfluence = 0.30f;
		BackLane.HeightVariationMultiplier = 0.75f;
		BackLane.NoiseFrequencyMultiplier = 0.70f;
		BackLane.NoiseSeedOffset = 947;
		BackLane.ConnectionSpacingInTiles = 83;
		BackLane.ConnectionWidthInTiles = 5;
		BackLane.ConnectionBlendInTiles = 11;
		LandscapeClose.DistanceBehindTerrain = 450.0f;
		LandscapeClose.VerticalOffset = -250.0f;
		LandscapeClose.WorldHeight = 650.0f;
		LandscapeClose.DistanceFadeAmount = 0.0f;
		LandscapeClose.CameraFollowFactor = 0.65f;
		LandscapeMid.DistanceBehindTerrain = 1100.0f;
		LandscapeMid.VerticalOffset = -100.0f;
		LandscapeMid.WorldHeight = 800.0f;
		LandscapeMid.DistanceFadeAmount = 0.22f;
		LandscapeMid.CameraFollowFactor = 0.35f;
		LandscapeFar.DistanceBehindTerrain = 2200.0f;
		LandscapeFar.VerticalOffset = 50.0f;
		LandscapeFar.WorldHeight = 1000.0f;
		LandscapeFar.DistanceFadeAmount = 0.48f;
		LandscapeFar.CameraFollowFactor = 0.15f;
	}

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Traversal", meta=(ClampMin="0.0"))
	float BackLedgeInset = 55.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Traversal", meta=(ClampMin="1.0"))
	float LedgeDetectionThickness = 24.0f;
};
