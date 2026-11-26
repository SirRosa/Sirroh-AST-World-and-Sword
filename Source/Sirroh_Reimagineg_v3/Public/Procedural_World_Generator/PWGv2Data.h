#pragma once
#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameFramework/SaveGame.h"
#include "Items/SirrohNativeItemSave.h"
#include "POI/POITerrain.h"
#include "POI/POIShaft.h"
#include "PWGv2Data.generated.h"

class UTexture2D;

UENUM(BlueprintType)
enum class EPWGTerrainLane : uint8
{
	Front,
	Middle,
	Back
};



UENUM(BlueprintType)
enum class EPWGv2EntranceStyle : uint8 { None, Drop, Switchback };

UENUM(BlueprintType)
enum class EPWGTreeShape : uint8
{
    Conical, Pyramidal, Columnar, Ovoid, Spherical, Spreading,
    Vase, Umbrella, Weeping, Tiered, Irregular, MultiStem
};

USTRUCT(BlueprintType)
struct FPWGTreeShapeWeight
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EPWGTreeShape Shape = EPWGTreeShape::Ovoid;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) float Weight = 1;
};

USTRUCT(BlueprintType)
struct FPWGv2Preference
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Weight = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector2D HeightMetres = FVector2D(0,30);
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Moisture = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Vegetation = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Flatness = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float DistanceToWaterMetres = 100;
    /** Nonzero floor keeps an otherwise unlikely biome/species possible. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MinimumSuitability = 0.03f;
};

USTRUCT(BlueprintType)
struct FPWGv2AtlasPart
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<UTexture2D> Texture;
    /** Normalized rectangle: left, top, right, bottom. No assumed master layout. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector4 UV = FVector4(0,0,1,1);
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Columns = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Rows = 1;
};

USTRUCT(BlueprintType)
struct FPWGv2TerrainTextureRow : public FTableRowBase
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<UTexture2D> Texture;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FLinearColor Tint = FLinearColor::White;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) float Brightness = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="25")) float TilingWorldSize = 400;
};

USTRUCT(BlueprintType)
struct FPWGv2BiomeRow : public FTableRowBase
{
    GENERATED_BODY()
    FPWGv2BiomeRow()
    {
        for (int32 I=0;I<12;++I) { FPWGTreeShapeWeight W; W.Shape=static_cast<EPWGTreeShape>(I); W.Weight=0; TreeShapes.Add(W); }
    }
    /** Optional biome-specific distribution; takes priority over region defaults. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trees") TArray<FPWGTreeShapeWeight> TreeShapes;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FPWGv2Preference Preference;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FPWGv2AtlasPart Terrain;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ForegroundTexture = TEXT("Grass_Jungle_1");
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName BackgroundTexture = TEXT("Grass_Jungle_1");
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FLinearColor LandscapeTint = FLinearColor::White;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FPWGv2AtlasPart Grass;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FPWGv2AtlasPart Bush;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FPWGv2AtlasPart Rock;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FPWGv2AtlasPart Overhang;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FPWGv2AtlasPart GroundPlant;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FPWGv2AtlasPart TreePlant;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FLinearColor Tint = FLinearColor::White;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MinLengthMetres = 60;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float BlendMetres = 15;
};

USTRUCT(BlueprintType)
struct FPWGv2TreeRow : public FTableRowBase
{
    GENERATED_BODY()
    FPWGv2TreeRow()
    {
        for (int32 I=0;I<12;++I) { FPWGTreeShapeWeight W; W.Shape=static_cast<EPWGTreeShape>(I); W.Weight=0; Shapes.Add(W); }
    }
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FPWGv2Preference Preference;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FPWGv2AtlasPart Trunk;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FPWGv2AtlasPart Crown;
    /** Zero weights use the region/biome distribution. Species and shape are independent. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shape") TArray<FPWGTreeShapeWeight> Shapes;
    /** Trunk contains vertical square cells; cell zero is the terminal cap. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Segments", meta=(ClampMin="2", ClampMax="32")) int32 BarkCells = 8;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Segments", meta=(ClampMin="64", ClampMax="512")) int32 SegmentBudget = 256;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Segments", meta=(ClampMin="0.05", ClampMax="0.5")) float MinimumSizeRatio = 0.18f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Segments", meta=(ClampMin="0.01", ClampMax="5")) float DepthStep = 0.15f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wind", meta=(ClampMin="0", ClampMax="20")) float WindBendDegrees = 4;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Damage", meta=(ClampMin="1")) float BaseSegmentHealth = 100;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Damage", meta=(ClampMin="1", ClampMax="20")) float DurabilityMultiplier = 4;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Damage", meta=(ClampMin="1", ClampMax="10")) float GroundImpactResistance = 2.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crown", meta=(ClampMin="32", ClampMax="300")) int32 CrownBudget = 180;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Damage", meta=(ClampMin="50")) float ImpactBreakSpeed = 380;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Damage", meta=(ClampMin="1", ClampMax="20")) float SleepDelay = 3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector2D HeightRange = FVector2D(400,800);
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float TrunkWidthRatio = 0.125f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float CrownWidthRatio = 0.6f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float FruitChance = 0.15f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1", ClampMax="12")) int32 MinCrowns = 3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1", ClampMax="12")) int32 MaxCrowns = 6;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="2")) float CrownDepthSpacing = 8;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="0.4")) float CrownSpread = 0.24f;
};

USTRUCT(BlueprintType)
struct FPWGv2RegionRow : public FTableRowBase
{
    GENERATED_BODY()
    FPWGv2RegionRow()
    {
        for (int32 I=0;I<12;++I) { FPWGTreeShapeWeight W; W.Shape=static_cast<EPWGTreeShape>(I); W.Weight=0; TreeShapes.Add(W); }
    }
    /** Empty allows every biome. Used when planning a new island. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> AllowedBiomes;
    /** Zero weights select built-in regional defaults. Overrides may repeat a shape across regions. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trees") TArray<FPWGTreeShapeWeight> TreeShapes;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trees", meta=(ClampMin="0", ClampMax="1")) float TreesOnPathChance = 0.25f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float LengthMetres = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 MinBiomes = 2;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 MaxBiomes = 3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MinHeightMetres = 3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxHeightMetres = 12;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float NoiseWavelengthMetres = 70;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float GrassPerSquareMetre = 3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float TreesPer100Metres = 10;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float BushesPer100Metres = 15;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float RocksPer100Metres = 8;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Moisture = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float FruitMultiplier = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float WindStrength = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float CloudMultiplier = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 MinEntrances = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 MaxEntrances = 2;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EPWGv2EntranceStyle EntranceStyle = EPWGv2EntranceStyle::Switchback;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bDirectToCaves = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FLinearColor PreviewColor = FLinearColor(0.22f,0.4f,0.13f);
};

USTRUCT(BlueprintType)
struct FPWGv2WorldRow : public FTableRowBase
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float SeaLevel = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float BiomeLengthMetres = 256;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float OceanLengthMetres = 100;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float CoastLengthMetres = 60;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float ChunkLengthMetres = 32;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float SampleSpacing = 100;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float LaneSpacing = 100;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float ForegroundDepth = 1000;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float BackgroundDepth = 3000;
    /** Global depth grading, shared by all opaque/masked scene objects. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Depth Atmosphere") bool bDepthAtmosphere = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Depth Atmosphere", meta=(ClampMin="0", ClampMax="1")) float DepthFogStrength = 0.82f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Depth Atmosphere", meta=(ClampMin="0", ClampMax="1")) float DepthDesaturation = 0.55f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Depth Atmosphere", meta=(ClampMin="0", ClampMax="1")) float DepthForegroundDarkness = 0.985f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Depth Atmosphere", meta=(ClampMin="50")) float DepthNeutralWidth = 85.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Depth Atmosphere", meta=(ClampMin="0", ClampMax="1")) float DepthGroundMist = 0.14f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Depth Atmosphere", meta=(ClampMin="20")) float DepthMistHeight = 220.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Depth Atmosphere", meta=(ClampMin="0", ClampMax="1")) float DepthPixelBands = 0.35f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Depth Atmosphere") FLinearColor DepthDayFogColor = FLinearColor(0.48f,0.58f,0.65f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Depth Atmosphere") FLinearColor DepthNightFogColor = FLinearColor(0.055f,0.085f,0.15f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float RegionBlendMetres = 30;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxPlayableSlope = 0.35f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float GrassLandscapeDepth = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 GrassCullDistance = 12000;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 LoadRadius = 2;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 RetainRadius = 3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bEnableUnderground = true;
    /** Cut away nearby foreground; never remove potentially visible grass from the saved plan. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float CutawayRadius = 2400;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float UndergroundForegroundOpacity = 0.08f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bSkipPermanentlyHiddenGrass = false;
};

USTRUCT(BlueprintType)
struct FPWGv2UndergroundRow : public FTableRowBase
{
    GENERATED_BODY()
    /** 1=Underground, 2=Caves, 3=Dungeons, 4=Catacombs. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Layer = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float FloorBelowSeaMetres = 4;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float ClearHeightMetres = 5;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float RoomLengthMetres = 12;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float FloorVariationMetres = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bRectangular = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float RocksPer100Metres = 5;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FPWGv2AtlasPart Atlas;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="5")) float SwordsPer100Metres = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName WallTexture = TEXT("Dirt_200");
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FLinearColor WallTint = FLinearColor::White;
};

USTRUCT()
struct FPWGv2RegionSpan
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) FName Id;
    UPROPERTY(SaveGame) float StartX = 0;
    UPROPERTY(SaveGame) float EndX = 0;
    UPROPERTY(SaveGame) FPWGv2RegionRow Definition;
};
USTRUCT()
struct FPWGv2BiomeSpan
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) FName Id;
    UPROPERTY(SaveGame) float StartX = 0;
    UPROPERTY(SaveGame) float EndX = 0;
    UPROPERTY(SaveGame) FPWGv2BiomeRow Definition;
};
USTRUCT()
struct FPWGv2Portal
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) float X = 0;
    UPROPERTY(SaveGame) float Width = 1200;
    UPROPERTY(SaveGame) int32 UpperLayer = 0;
    UPROPERTY(SaveGame) int32 LowerLayer = 1;
    UPROPERTY(SaveGame) EPWGv2EntranceStyle Style = EPWGv2EntranceStyle::Switchback;
    UPROPERTY(SaveGame) bool bKitShaft=false;
    UPROPERTY(SaveGame) FPOIShaftRow ShaftRecipe;
    UPROPERTY(SaveGame) TArray<FPOIShaftRow> ShaftModules;
    UPROPERTY(SaveGame) int32 ShaftSeed=1;
    UPROPERTY(SaveGame) bool bPOIFirst=false;
    UPROPERTY(SaveGame) float TopZ=0;
    UPROPERTY(SaveGame) float BottomZ=0;
    UPROPERTY(SaveGame) TArray<FPOIShaftRow> ShaftSectionPlan;
};
USTRUCT()
struct FPWGv2Visit
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) TArray<FSirrohNativeItemSave> NativeInventory;
    UPROPERTY(SaveGame) FVector Position = FVector::ZeroVector;
    UPROPERTY(SaveGame) int32 Lane = 1;
};
USTRUCT()
struct FPWGTreeSegmentSave
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) FVector A = FVector::ZeroVector;
    UPROPERTY(SaveGame) FVector B = FVector::ZeroVector;
    UPROPERTY(SaveGame) FVector VelocityA = FVector::ZeroVector;
    UPROPERTY(SaveGame) FVector VelocityB = FVector::ZeroVector;
    UPROPERTY(SaveGame) float Health = 0;
    UPROPERTY(SaveGame) bool Broken = false;
    UPROPERTY(SaveGame) bool Sleeping = false;
};
USTRUCT()
struct FPWGTreeSave
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) int32 DamageRevision = 0;
    UPROPERTY(SaveGame) int32 GroundBreakCount = 0;
    UPROPERTY(SaveGame) int32 Seed = 0;
    UPROPERTY(SaveGame) EPWGTreeShape Shape = EPWGTreeShape::Ovoid;
    UPROPERTY(SaveGame) float Height = 600;
    UPROPERTY(SaveGame) FPWGv2TreeRow Definition;
    UPROPERTY(SaveGame) TArray<FPWGTreeSegmentSave> Segments;
};

/** A horizontal authored room chain, reserved before terrain is built. */
USTRUCT()
struct FPWGv2RoomChain
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) int32 Layer=1;
    UPROPERTY(SaveGame) float StartX=0;
    UPROPERTY(SaveGame) float EndX=0;
    UPROPERTY(SaveGame) float FloorZ=0;
    UPROPERTY(SaveGame) float ClearHeight=400;
    UPROPERTY(SaveGame) int32 FirstPiece=0;
    UPROPERTY(SaveGame) int32 PieceCount=0;
};

UCLASS()
class SIRROH_REIMAGINEG_V3_API UPWGv2WorldSave : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY(SaveGame) int32 Version = 2;
    UPROPERTY(SaveGame) int32 AtlasLayoutRevision = 0;
    UPROPERTY(SaveGame) FGuid WorldId;
    UPROPERTY(SaveGame) int32 Seed = 0;
    UPROPERTY(SaveGame) FPWGv2WorldRow Settings;
    UPROPERTY(SaveGame) TArray<FPWGv2RegionSpan> Regions;
    UPROPERTY(SaveGame) float MinX = 0;
    UPROPERTY(SaveGame) float MaxX = 0;
    UPROPERTY(SaveGame) TArray<float> Heights;
    UPROPERTY(SaveGame) TArray<FPOITerrainStamp> POITerrainStamps;
    UPROPERTY(SaveGame) TArray<FPWGv2BiomeSpan> Biomes;
    UPROPERTY(SaveGame) TArray<FPWGv2TreeRow> TreeDefinitions;
    UPROPERTY(SaveGame) TArray<FPWGv2UndergroundRow> Layers;
    UPROPERTY(SaveGame) TArray<FPWGv2Portal> Portals;
    UPROPERTY(SaveGame) TArray<FPOIShaftRow> AuthoredPOIs;
    UPROPERTY(SaveGame) TArray<FVector> AuthoredPOIPositions;
    UPROPERTY(SaveGame) TArray<FPWGv2RoomChain> RoomChains;
    UPROPERTY(SaveGame) TMap<FGuid,FPWGv2Visit> Visits;
    UPROPERTY(SaveGame) TSet<FName> RemovedObjects;
    UPROPERTY(SaveGame) TMap<FName,FPWGTreeSave> DamagedTrees;
};
