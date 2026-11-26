#include "PWG/PWGGenerator.h"
#include "Placeholder/PlaceholderBerry.h"
#include "Placeholder/PlaceholderBerryWorld.h"
#include "Sword/SwordWorldSubsystem.h"

#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/MovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "MeshDescriptionBuilder.h"
#include "StaticMeshAttributes.h"
#include "ProceduralMeshComponent.h"
#include "PWG/PWGLedgeActor.h"
#include "PWG/PWGEnvironmentV2.h"
#include "PixelFormat.h"

DEFINE_LOG_CATEGORY_STATIC(LogPWG, Log, All);

namespace
{
	// Terrain uses Rock=4, Ground=5; the fruit atlas uses Ground=4, Rock=5.
	int32 FruitAtlasColumn(int32 Mini) { return Mini==4?5:(Mini==5?4:FMath::Clamp(Mini,0,7)); }
	constexpr int32 MasterSquareCellColumns = 32; // 4096 / 128
	constexpr int32 MasterSquareCellRows = 32;
	constexpr int32 MasterWideCellColumns = 16;   // 4096 / 256
	constexpr int32 MasterWideCellRows = 32;      // 4096 / 128
	constexpr int32 SurfaceStyleCount = 8;

	int32 MiniBiomeRuleToSurfaceStyle(const int32 RuleIndex)
	{
		// Rules are Mini01..Mini06, while atlas style ids insert Rock and Ground
		// between Mini04 and Mini05.
		return RuleIndex < 4 ? RuleIndex : RuleIndex + 2;
	}

	int32 CompactTerrainStyleForSurface(const int32 SurfaceStyle)
	{
		// Runtime ids are MB1,MB2,MB3,MB4,Rock,Ground,MB5,MB6.
		// The bottom atlas row is MB6,DeepRock,MB5.
		if (SurfaceStyle == static_cast<int32>(EPWGSurfaceStyle::MiniBiome05)) return 8;
		if (SurfaceStyle == static_cast<int32>(EPWGSurfaceStyle::MiniBiome06)) return 6;
		return FMath::Clamp(SurfaceStyle, 0, 5);
	}

	int32 MasterGrassFrame(const int32 SurfaceStyle, const int32 Variant)
	{
		const int32 Style = FMath::Clamp(SurfaceStyle, 0, SurfaceStyleCount - 1);
		const int32 Local = FMath::Clamp(Variant, 0, 7);
		const int32 BaseColumn = 16 + (Style % 2) * 4;
		const int32 BaseRow = (Style / 2) * 2;
		return (BaseRow + Local / 4) * MasterSquareCellColumns + BaseColumn + Local % 4;
	}

	int32 MasterWideFrame(const int32 SectionBaseColumn, const int32 SurfaceStyle, const int32 Variant)
	{
		const int32 Style = FMath::Clamp(SurfaceStyle, 0, SurfaceStyleCount - 1);
		const int32 Local = FMath::Clamp(Variant, 0, 3);
		const int32 BaseColumn = SectionBaseColumn + (Style % 2) * 2;
		const int32 BaseRow = 8 + (Style / 2) * 2;
		return (BaseRow + Local / 2) * MasterWideCellColumns + BaseColumn + Local % 2;
	}

	void EnableVegetationFillChannel(UPrimitiveComponent* Component)
	{
		if (Component)
		{
			// Channel 0 receives the sun/moon. Channel 1 receives the shadowless
			// camera-facing fill created by Environment V2.
			Component->SetLightingChannels(true, true, false);
		}
	}

	/**
	 * Cached, normalised height of the first opaque pixel in every texture column.
	 * Values are measured from the bottom of the image: 0 = bottom, 1 = top.
	 */
	const TArray<float>* GetLandscapeSilhouette(UTexture2D* Texture, const float AlphaThreshold)
	{
		if (!Texture)
		{
			return nullptr;
		}

		static TMap<FString, TArray<float>> Cache;
		static TSet<FString> FailedTextures;
		const int32 ThresholdByte = FMath::Clamp(FMath::RoundToInt(AlphaThreshold * 255.0f), 0, 255);
		const FString CacheKey = FString::Printf(TEXT("%s|%d"), *Texture->GetPathName(), ThresholdByte);
		if (TArray<float>* Existing = Cache.Find(CacheKey))
		{
			return Existing;
		}
		if (FailedTextures.Contains(CacheKey))
		{
			return nullptr;
		}

		FTexturePlatformData* PlatformData = Texture->GetPlatformData();
		if (!PlatformData || PlatformData->Mips.IsEmpty())
		{
			FailedTextures.Add(CacheKey);
			return nullptr;
		}

		FTexture2DMipMap& Mip = PlatformData->Mips[0];
		const int32 Width = Mip.SizeX;
		const int32 Height = Mip.SizeY;
		const EPixelFormat Format = static_cast<EPixelFormat>(PlatformData->PixelFormat);
		const bool bRGBA8 = Format == PF_B8G8R8A8 || Format == PF_R8G8B8A8;
		if (!bRGBA8 || Width <= 0 || Height <= 0 || Mip.BulkData.GetBulkDataSize() < static_cast<int64>(Width) * Height * 4)
		{
			if (!FailedTextures.Contains(CacheKey))
			{
				UE_LOG(LogPWG, Warning,
					TEXT("Landscape silhouette cannot read %s. Use an uncompressed BGRA8/RGBA8 texture (Never Stream recommended)."),
					*Texture->GetPathName());
			}
			FailedTextures.Add(CacheKey);
			return nullptr;
		}

		const uint8* Pixels = static_cast<const uint8*>(Mip.BulkData.LockReadOnly());
		if (!Pixels)
		{
			FailedTextures.Add(CacheKey);
			return nullptr;
		}

		TArray<float> Profile;
		Profile.SetNumUninitialized(Width);
		for (int32 X = 0; X < Width; ++X)
		{
			int32 SurfaceY = Height - 1;
			for (int32 Y = 0; Y < Height; ++Y)
			{
				const uint8 Alpha = Pixels[(static_cast<int64>(Y) * Width + X) * 4 + 3];
				if (Alpha >= ThresholdByte)
				{
					SurfaceY = Y;
					break;
				}
			}
			Profile[X] = 1.0f - static_cast<float>(SurfaceY) / FMath::Max(1, Height - 1);
		}
		Mip.BulkData.Unlock();
		return &Cache.Add(CacheKey, MoveTemp(Profile));
	}
}

APWG_Generator::APWG_Generator()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	TerrainMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Terrain"));
	TerrainMesh->SetupAttachment(Root);
	TerrainMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Render geometry contains atlas facades and backing layers which movement
	// never needs. Cooking only this small surface mesh is substantially cheaper.
	TerrainCollisionMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TerrainCollision"));
	TerrainCollisionMesh->SetupAttachment(Root);
	TerrainCollisionMesh->bUseAsyncCooking = false;
	TerrainCollisionMesh->bUseComplexAsSimpleCollision = false;
	TerrainCollisionMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	TerrainCollisionMesh->SetCollisionObjectType(ECC_WorldStatic);
	TerrainCollisionMesh->ComponentTags.AddUnique(TEXT("CombatTerrain"));
	TerrainCollisionMesh->SetCollisionResponseToAllChannels(ECR_Block);
	TerrainCollisionMesh->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Ignore);
	TerrainCollisionMesh->SetVisibility(false, true);
	TerrainCollisionMesh->SetHiddenInGame(true, true);

	DecorationMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Decorations"));
	DecorationMesh->SetupAttachment(Root);
	DecorationMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DecorationMesh->SetCastShadow(false);

	auto CreateVisualMesh = [this, Root](const FName Name)
	{
		UProceduralMeshComponent* Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(Name);
		Mesh->SetupAttachment(Root);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCastShadow(false);
		Mesh->SetVisibleInRayTracing(false);
		Mesh->SetAffectDistanceFieldLighting(false);
		Mesh->SetAffectDynamicIndirectLighting(false);
		return Mesh;
	};
	// UNUSED_BROKEN legacy components. Kept hidden only for serialized compatibility.
	EnvironmentMesh = CreateVisualMesh(TEXT("Environment_UNUSED_BROKEN"));
	SkyMesh = CreateVisualMesh(TEXT("Sky_UNUSED_BROKEN"));
	SunMesh = CreateVisualMesh(TEXT("Sun_UNUSED_BROKEN"));
	MoonMesh = CreateVisualMesh(TEXT("Moon_UNUSED_BROKEN"));
	CloudMesh = CreateVisualMesh(TEXT("Clouds_UNUSED_BROKEN"));
	TerrainSceneryMesh = CreateVisualMesh(TEXT("TerrainScenery_UNUSED_BROKEN"));
	VegetationMesh = CreateVisualMesh(TEXT("Vegetation_UNUSED_BROKEN"));
	EnvironmentMesh->SetVisibility(false, true);
	SkyMesh->SetVisibility(false, true);
	SunMesh->SetVisibility(false, true);
	MoonMesh->SetVisibility(false, true);
	CloudMesh->SetVisibility(false, true);
	TerrainSceneryMesh->SetVisibility(false, true);
	VegetationMesh->SetVisibility(false, true);
	TreeTrunkMesh = CreateVisualMesh(TEXT("ProceduralTreeTrunks"));
	EnableVegetationFillChannel(TreeTrunkMesh);
	EnvironmentMesh->TranslucencySortPriority = 100;
	SkyMesh->TranslucencySortPriority = -1000;
	SunMesh->TranslucencySortPriority = -900;
	MoonMesh->TranslucencySortPriority = -900;
	CloudMesh->TranslucencySortPriority = -800;
	TerrainSceneryMesh->TranslucencySortPriority = 50;
	VegetationMesh->TranslucencySortPriority = -50;
	TreeTrunkMesh->TranslucencySortPriority = 25;
	for (int32 Layer = 0; Layer < 3; ++Layer)
	{
		UProceduralMeshComponent* LayerMesh = CreateVisualMesh(*FString::Printf(TEXT("BackgroundLayer%d_UNUSED_BROKEN"), Layer));
		LayerMesh->TranslucencySortPriority = -100 - Layer * 100;
		BackgroundLayerMeshes.Add(LayerMesh);
		LayerMesh->SetVisibility(false, true);
		UProceduralMeshComponent* SpriteLayerMesh = CreateVisualMesh(*FString::Printf(TEXT("BackgroundSprites%d_UNUSED_BROKEN"), Layer));
		// Draw immediately before that layer's landscape. Its opaque lower portion
		// then hides sprite roots, while crowns remain visible through alpha.
		SpriteLayerMesh->TranslucencySortPriority = LayerMesh->TranslucencySortPriority - 1;
		BackgroundSpriteLayerMeshes.Add(SpriteLayerMesh);
		SpriteLayerMesh->SetVisibility(false, true);
	}
}

const FBiomeData* APWG_Generator::GetActiveBiomeDataForEnvironment() const
{
	if (!BiomeTable || ActiveBiomeRow.IsNone()) return nullptr;
	return BiomeTable->FindRow<FBiomeData>(ActiveBiomeRow, TEXT("Environment V2 master atlas"), false);
}

void APWG_Generator::ClearPlaceholderBerries()
{
	for (APlaceholderBerry* Berry : PlaceholderBerries)
		if (IsValid(Berry) && Berry->GetOwner()==this) Berry->Destroy();
	PlaceholderBerries.Reset();
}

void APWG_Generator::SpawnPlaceholderFruit(const FVector& LocalPosition,float LocalLaneY,const FString& Id,int32 Frame)
{
	if (GetWorld()->GetSubsystem<UPlaceholderBerryWorld>()->Collected.Contains(Id)) return;
	const FTransform Transform(FRotator::ZeroRotator,GetActorTransform().TransformPosition(LocalPosition));
	APlaceholderBerry* Berry=GetWorld()->SpawnActorDeferred<APlaceholderBerry>(APlaceholderBerry::StaticClass(),Transform,this,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Berry) return;
	Berry->PersistentId=Id;
	Berry->AtlasFrame=Frame;
	Berry->Size=BerryWorldSize;
	Berry->LaneY=GetActorTransform().TransformPosition(FVector(LocalPosition.X,LocalLaneY,LocalPosition.Z)).Y;
	UGameplayStatics::FinishSpawningActor(Berry,Transform);
	PlaceholderBerries.Add(Berry);
}

void APWG_Generator::EndPlay(const EEndPlayReason::Type Reason)
{
	ClearSwordScenery();
	ClearPlaceholderBerries();
	Super::EndPlay(Reason);
}

void APWG_Generator::BeginPlay()
{
	Super::BeginPlay();
	if (bGenerateOnBeginPlay)
	{
		GenerateWorld(DefaultBiomeRow);
	}
}

bool APWG_Generator::GenerateWorld(const FName BiomeRowName)
{
	ClearGeneratedWorld();
	if (!BiomeTable)
	{
		UE_LOG(LogPWG, Error, TEXT("GenerateWorld failed: BiomeTable is not assigned on %s."), *GetName());
		return false;
	}

	const FBiomeData* Biome = FindBiomeData(BiomeRowName, TEXT("PWG GenerateWorld"));
	if (!Biome)
	{
		UE_LOG(LogPWG, Error, TEXT("GenerateWorld failed: biome row '%s' does not exist."), *BiomeRowName.ToString());
		return false;
	}

	if (bEnableChunkStreaming && !bStreamingChild)
	{
		ActiveBiomeRow = BiomeRowName;
		ActiveWorldSeed = SeedOverride != 0 ? SeedOverride : Biome->Seed;
		ActiveTileSize = FMath::Max(1.0f, Biome->TileSize);
		StreamingChunkLength = FMath::Max(1, Biome->ChunkLengthInTiles) * ActiveTileSize;
		ChunkHalfDepth = ActiveTileSize * 1.5f;
		bWorldGenerated = true;
		for (int32 Index = -FMath::Max(1, PreloadRadiusInChunks); Index <= FMath::Max(1, PreloadRadiusInChunks); ++Index)
		{
			if (!EnsureChunkLoaded(Index))
			{
				ClearGeneratedWorld();
				return false;
			}
		}
		SpawnEnvironmentV2();
		SetActorTickEnabled(true);
		UE_LOG(LogPWG, Display, TEXT("Started chunk streaming for biome '%s' with chunks [%d..%d]."),
			*BiomeRowName.ToString(), -PreloadRadiusInChunks, PreloadRadiusInChunks);
		return true;
	}

	return GenerateSingleChunk(BiomeRowName, *Biome);
}

bool APWG_Generator::GenerateSingleChunk(const FName BiomeRowName, const FBiomeData& Biome)
{
	ActiveBiomeRow = BiomeRowName;
	UE_LOG(LogPWG, Display,
		TEXT("Chunk %d reads row '%s' only from %s | BlockOverhang Enabled=%d VerticalOffset=%.1f | Landscape Scroll=(%.4f, %.4f, %.4f) Movement=(%.2f, %.2f, %.2f)"),
		StreamingChunkIndex, *BiomeRowName.ToString(), *GetPathNameSafe(BiomeTable),
		Biome.BlockOverhang.bEnabled ? 1 : 0, Biome.BlockOverhang.VerticalOffset,
		Biome.LandscapeClose.ScrollSpeed, Biome.LandscapeMid.ScrollSpeed, Biome.LandscapeFar.ScrollSpeed,
		Biome.LandscapeClose.CameraFollowFactor, Biome.LandscapeMid.CameraFollowFactor,
		Biome.LandscapeFar.CameraFollowFactor);

	ActiveTileSize = FMath::Max(1.0f, Biome.TileSize);
	ChunkHalfDepth = ActiveTileSize * 1.5f;
	const int32 ColumnCount = FMath::Max(1, Biome.ChunkLengthInTiles);
	ChunkStartLocalX = -0.5f * ColumnCount * ActiveTileSize;
	if (ActiveWorldSeed == 0) ActiveWorldSeed = SeedOverride != 0 ? SeedOverride : Biome.Seed;
	const int32 ChunkSeed = HashCombineFast(static_cast<uint32>(ActiveWorldSeed), GetTypeHash(StreamingChunkIndex));
	FRandomStream Random(ChunkSeed);

	BuildSurfaceHeights(Biome, Random);
	BuildAtlasColumnPath(Biome, Random);
	BuildTerrainMesh(Biome, Random);
	BuildGrass(Biome, Random);
	BuildBushes(Biome, Random);
	BuildRocks(Biome);
	BuildBlockOverhangs(Biome, Random);
	BuildTrees(Biome);
	BuildDecorations(Biome, Random);
	// Legacy BuildChunkEnvironment is intentionally disconnected (UNUSED_BROKEN).
	BuildTraversalLedges(Biome);
	SpawnBiomeObjects(Biome, Random);
	bWorldGenerated = true;
	if (!bStreamingChild && !bEnableChunkStreaming) SpawnEnvironmentV2();
	if (!bStreamingChild && (Biome.Grass.bEnabled || Biome.Trees.bEnabled)) SetActorTickEnabled(true);

	UE_LOG(LogPWG, Display, TEXT("Generated biome '%s': %d x 3 tiles, seed %d, %d ledges."),
		*BiomeRowName.ToString(), ColumnCount, Random.GetInitialSeed(), SpawnedLedges.Num());
	return true;
}

void APWG_Generator::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bWorldGenerated || bStreamingChild) return;
	UpdateGrassMaterialParameters();
	for (const TPair<int32, TObjectPtr<APWG_Generator>>& Pair : LoadedChunks)
	{
		if (IsValid(Pair.Value)) Pair.Value->UpdateGrassMaterialParameters();
	}
	if (!bEnableChunkStreaming) return;
	StreamingAccumulator += DeltaSeconds;
	if (StreamingAccumulator >= FMath::Max(0.05f, StreamingUpdateInterval))
	{
		StreamingAccumulator = 0.0f;
		UpdateChunkStreaming();
	}
}

void APWG_Generator::SpawnEnvironmentV2()
{
	if (bStreamingChild || !GetWorld())
	{
		UE_LOG(LogPWG, Warning, TEXT("Environment V2 spawn skipped: child=%d world=%s"),
			bStreamingChild ? 1 : 0, *GetNameSafe(GetWorld()));
		return;
	}
	if (IsValid(EnvironmentV2)) EnvironmentV2->Destroy();
	TSubclassOf<APWGEnvironmentV2> SpawnClass = EnvironmentV2Class;
	// No editor assignment is required. If the optional project Blueprint child
	// exists, use it automatically so its Details defaults become the single
	// place for artistic tuning. A generator Blueprint saved before Environment
	// V2 existed can contain the native base class as an inherited default; that
	// must not silently bypass BP_PWGEnvironmentV2 and all of its Details values.
	if (!SpawnClass || SpawnClass == APWGEnvironmentV2::StaticClass())
	{
		if (UClass* EnvironmentBlueprintClass = LoadClass<APWGEnvironmentV2>(nullptr,
			TEXT("/Game/Systems/Sprite2Bone/Biomes/BP_PWGEnvironmentV2.BP_PWGEnvironmentV2_C"));
			EnvironmentBlueprintClass)
		{
			SpawnClass = EnvironmentBlueprintClass;
		}
	}
	if (!SpawnClass) SpawnClass = APWGEnvironmentV2::StaticClass();
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	// Environment cards are authored directly in centimetres from the active
	// camera view. They must never inherit an editor scale from the generator.
	const FTransform EnvironmentTransform(FRotator::ZeroRotator, GetActorLocation(), FVector::OneVector);
	EnvironmentV2 = GetWorld()->SpawnActor<APWGEnvironmentV2>(SpawnClass, EnvironmentTransform, Params);
	if (EnvironmentV2)
	{
		UE_LOG(LogPWG, Display, TEXT("Environment V2 spawned automatically: %s (%s)."),
			*GetNameSafe(EnvironmentV2), *GetNameSafe(SpawnClass.Get()));
		EnvironmentV2->Initialize(this, ActiveWorldSeed);
	}
	else
	{
		UE_LOG(LogPWG, Error, TEXT("Environment V2 actor failed to spawn."));
	}
}

int32 APWG_Generator::GetChunkIndexAtWorldX(const float WorldX) const
{
	if (StreamingChunkLength <= KINDA_SMALL_NUMBER) return 0;
	const FVector Local = GetActorTransform().InverseTransformPosition(FVector(WorldX, GetActorLocation().Y, GetActorLocation().Z));
	return FMath::FloorToInt((Local.X + StreamingChunkLength * 0.5f) / StreamingChunkLength);
}

APWG_Generator* APWG_Generator::EnsureChunkLoaded(const int32 ChunkIndex)
{
	if (TObjectPtr<APWG_Generator>* Existing = LoadedChunks.Find(ChunkIndex))
	{
		return IsValid(*Existing) ? Existing->Get() : nullptr;
	}
	UWorld* World = GetWorld();
	if (!World || ActiveBiomeRow.IsNone() || StreamingChunkLength <= 0.0f) return nullptr;

	const FVector ChunkLocation = GetActorTransform().TransformPosition(FVector(ChunkIndex * StreamingChunkLength, 0.0f, 0.0f));
	const FTransform ChunkTransform(GetActorRotation(), ChunkLocation, GetActorScale3D());
	APWG_Generator* Chunk = World->SpawnActorDeferred<APWG_Generator>(GetClass(), ChunkTransform, this, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Chunk) return nullptr;
	Chunk->bStreamingChild = true;
	Chunk->bEnableChunkStreaming = false;
	Chunk->bGenerateOnBeginPlay = false;
	Chunk->BiomeTable = BiomeTable;
	Chunk->DefaultBiomeRow = ActiveBiomeRow;
	Chunk->SeedOverride = ActiveWorldSeed;
	Chunk->StreamingBiomeRows = StreamingBiomeRows;
	Chunk->SharedSkyPanoramaTexture = SharedSkyPanoramaTexture;
	Chunk->SharedSkyHeight = SharedSkyHeight;
	Chunk->SkyParallaxFactor = SkyParallaxFactor;
	Chunk->SharedSkyScrollSpeed = SharedSkyScrollSpeed;
	Chunk->SharedSkyDepth = SharedSkyDepth;
	Chunk->SharedSkyVerticalOffset = SharedSkyVerticalOffset;
	Chunk->LandscapeVerticalFramingOffset = LandscapeVerticalFramingOffset;
	Chunk->ChunksPerBiome = ChunksPerBiome;
	Chunk->BiomeTransitionWidthInTiles = BiomeTransitionWidthInTiles;
	Chunk->bEnableSurfaceBushes = bEnableSurfaceBushes;
	Chunk->bEnableSurfaceRocks = bEnableSurfaceRocks;
	Chunk->RocksPer100Metres = RocksPer100Metres;
	Chunk->RockBaseHeight = RockBaseHeight;
	Chunk->RockEmbedHeightFraction = RockEmbedHeightFraction;
	Chunk->bEnableSwordSpawns = bEnableSwordSpawns;
	Chunk->bEnablePlaceholderBerries = bEnablePlaceholderBerries;
	Chunk->BerryBushChance = BerryBushChance;
	Chunk->BerriesPerBush = BerriesPerBush;
	Chunk->BerryWorldSize = BerryWorldSize;
	Chunk->TreeFruitChance = TreeFruitChance;
	Chunk->MaxFruitsPerTree = MaxFruitsPerTree;
	Chunk->BushAtlas = BushAtlas;
	Chunk->BushMaterial = BushMaterial;
	Chunk->BushAtlasColumns = BushAtlasColumns;
	Chunk->BushAtlasRows = BushAtlasRows;
	Chunk->BushesPer100Metres = BushesPer100Metres;
	Chunk->BushBaseSize = BushBaseSize;
	Chunk->BushScaleRange = BushScaleRange;
	Chunk->BushFrontChance = BushFrontChance;
	Chunk->BushBackDepthRange = BushBackDepthRange;
	Chunk->BushFrontDepthRange = BushFrontDepthRange;
	Chunk->BushRootBurial = BushRootBurial;
	Chunk->BushWindInfluence = BushWindInfluence;
	Chunk->BushStartCullDistance = BushStartCullDistance;
	Chunk->BushEndCullDistance = BushEndCullDistance;
	Chunk->BushTint = BushTint;
	Chunk->BushBrightness = BushBrightness;
	Chunk->ActiveWorldSeed = ActiveWorldSeed;
	Chunk->StreamingChunkIndex = ChunkIndex;
	Chunk->GlobalColumnOffset = static_cast<int64>(ChunkIndex)
		* FMath::Max(1, FMath::RoundToInt(StreamingChunkLength / ActiveTileSize));
	UGameplayStatics::FinishSpawningActor(Chunk, ChunkTransform);
	const FName ChunkBiomeRow = GetBiomeRowForChunk(ChunkIndex);
	if (!Chunk->GenerateWorld(ChunkBiomeRow))
	{
		Chunk->Destroy();
		return nullptr;
	}
	LoadedChunks.Add(ChunkIndex, Chunk);
	return Chunk;
}

FName APWG_Generator::GetBiomeRowForChunk(const int32 ChunkIndex) const
{
	if (StreamingBiomeRows.IsEmpty()) return ActiveBiomeRow;
	const int64 DistanceFromOrigin = FMath::Abs(static_cast<int64>(ChunkIndex));
	const int64 Region = DistanceFromOrigin / FMath::Max(1, ChunksPerBiome);
	return StreamingBiomeRows[Region % StreamingBiomeRows.Num()];
}

const FBiomeData* APWG_Generator::FindBiomeData(const FName RowName, const TCHAR* Context) const
{
	if (BiomeTable)
	{
		if (const FBiomeData* Row = BiomeTable->FindRow<FBiomeData>(RowName, Context, false)) return Row;
	}
	return nullptr;
}

void APWG_Generator::UpdateChunkStreaming()
{
	UWorld* World = GetWorld();
	APawn* Pawn = World ? UGameplayStatics::GetPlayerPawn(World, 0) : nullptr;
	if (!Pawn) return;
	const int32 Current = GetChunkIndexAtWorldX(Pawn->GetActorLocation().X);
	const int32 Preload = FMath::Max(1, PreloadRadiusInChunks);
	for (int32 Index = Current - Preload; Index <= Current + Preload; ++Index) EnsureChunkLoaded(Index);

	const int32 Retain = FMath::Max(Preload, RetentionRadiusInChunks);
	TArray<int32> ToRemove;
	for (const TPair<int32, TObjectPtr<APWG_Generator>>& Pair : LoadedChunks)
	{
		if (FMath::Abs(Pair.Key - Current) > Retain) ToRemove.Add(Pair.Key);
	}
	for (const int32 Index : ToRemove)
	{
		if (APWG_Generator* Chunk = LoadedChunks.FindRef(Index)) Chunk->Destroy();
		LoadedChunks.Remove(Index);
	}
}

void APWG_Generator::BuildSurfaceHeights(const FBiomeData& Biome, FRandomStream& Random)
{
	const int32 Count = FMath::Max(1, Biome.ChunkLengthInTiles);
	SurfaceHeightTiles.SetNumUninitialized(Count);
	SmoothSurfaceHeights.SetNumUninitialized(Count);
	ActiveSurfaceSegmentsPerTile = FMath::Clamp(
		FMath::RoundToInt(ActiveTileSize / FMath::Max(5.0f, Biome.HorizontalSurfaceStep)), 1, 20);
	ActiveSurfaceSegmentWidth = ActiveTileSize / ActiveSurfaceSegmentsPerTile;
	int32 Passes = FMath::Clamp(Biome.SurfaceSmoothingPasses, 0, 8);
	// Every chunk must filter the shared height samples identically or two
	// different biome rows could disagree on their common boundary.
	if (BiomeTable)
	{
		for (const FName RowName : StreamingBiomeRows)
		{
			if (const FBiomeData* Row = FindBiomeData(RowName, TEXT("PWG shared smoothing")))
			{
				Passes = FMath::Max(Passes, FMath::Clamp(Row->SurfaceSmoothingPasses, 0, 8));
			}
		}
	}
	const int32 Padding = Passes + 1;
	TArray<float> Samples;
	Samples.SetNumUninitialized(Count + Padding * 2);
	for (int32 Index = -Padding; Index < Count + Padding; ++Index)
	{
		Samples[Index + Padding] = EvaluateSmoothedGlobalHeight(Biome, GlobalColumnOffset + Index);
	}
	for (int32 Pass = 0; Pass < Passes; ++Pass)
	{
		TArray<float> Filtered = Samples;
		for (int32 Index = 1; Index < Samples.Num() - 1; ++Index)
		{
			Filtered[Index] = Samples[Index - 1] * 0.25f + Samples[Index] * 0.5f + Samples[Index + 1] * 0.25f;
		}
		Samples = MoveTemp(Filtered);
	}
	for (int32 Index = 0; Index < Count; ++Index) SmoothSurfaceHeights[Index] = Samples[Index + Padding];

	const float VerticalStep = FMath::Max(5.0f, Biome.VerticalSurfaceStep);
	const float BaseHeight = Biome.BaseSurfaceHeightInTiles * ActiveTileSize;
	const int32 MaximumRelativeLevel = FMath::Max(1,
		FMath::CeilToInt(Biome.HeightVariationInTiles * ActiveTileSize / VerticalStep));
	const int32 SegmentCount = Count * ActiveSurfaceSegmentsPerTile;
	SteppedSurfaceHeights.SetNumUninitialized(SegmentCount);

	auto SampleFilteredHeight = [&](const float CentreCoordinate)
	{
		if (CentreCoordinate <= 0.0f)
		{
			const float Alpha = FMath::Clamp(CentreCoordinate + 1.0f, 0.0f, 1.0f);
			return FMath::Lerp(Samples[Padding - 1], Samples[Padding], FMath::SmoothStep(0.0f, 1.0f, Alpha));
		}
		if (CentreCoordinate >= Count - 1)
		{
			const float Alpha = FMath::Clamp(CentreCoordinate - (Count - 1), 0.0f, 1.0f);
			return FMath::Lerp(Samples[Padding + Count - 1], Samples[Padding + Count],
				FMath::SmoothStep(0.0f, 1.0f, Alpha));
		}
		const int32 Left = FMath::FloorToInt(CentreCoordinate);
		const float Alpha = CentreCoordinate - Left;
		return FMath::Lerp(Samples[Padding + Left], Samples[Padding + Left + 1],
			FMath::SmoothStep(0.0f, 1.0f, Alpha));
	};

	TArray<float> Targets;
	Targets.SetNumUninitialized(SegmentCount);
	for (int32 Segment = 0; Segment < SegmentCount; ++Segment)
	{
		const float CentreCoordinate = (Segment + 0.5f) / ActiveSurfaceSegmentsPerTile - 0.5f;
		Targets[Segment] = FMath::RoundToFloat(SampleFilteredHeight(CentreCoordinate) / VerticalStep) * VerticalStep;
	}

	float CurrentHeight = Targets[0];
	int32 HoldRemaining = 0;
	const float PlateauStrength = FMath::Abs(Biome.PseudoPixelPlateauSmoothing);
	for (int32 Segment = 0; Segment < SegmentCount; ++Segment)
	{
		if (HoldRemaining <= 0 && !FMath::IsNearlyEqual(CurrentHeight, Targets[Segment], 0.1f))
		{
			const int32 DesiredSteps = FMath::RoundToInt((Targets[Segment] - CurrentHeight) / VerticalStep);
			const int32 AppliedSteps = FMath::Clamp(DesiredSteps,
				-FMath::Max(1, Biome.MaximumVerticalStepsPerSegment),
				FMath::Max(1, Biome.MaximumVerticalStepsPerSegment));
			CurrentHeight += AppliedSteps * VerticalStep;

			int32 RelativeLevel = FMath::Abs(FMath::RoundToInt((CurrentHeight - BaseHeight) / VerticalStep));
			if (Biome.PseudoPixelPlateauSmoothing < 0.0f)
			{
				RelativeLevel = FMath::Max(1, MaximumRelativeLevel - RelativeLevel + 1);
			}
			const int32 PatternLength = RelativeLevel > 0
				? 1 + FMath::FloorToInt(FMath::Log2(static_cast<float>(RelativeLevel))) : 1;
			const int32 HoldLength = 1 + FMath::RoundToInt((PatternLength - 1) * PlateauStrength);
			HoldRemaining = FMath::Max(0, HoldLength - 1);
		}
		else if (HoldRemaining > 0)
		{
			--HoldRemaining;
		}
		SteppedSurfaceHeights[Segment] = CurrentHeight;
	}

	// The plateau walk is stateful. Without anchoring its far edge, a steep
	// biome transition could leave this chunk several metres behind its target,
	// while the next chunk restarted directly at its own target -- creating a
	// vertical wall. Anchor the last sample and propagate only the minimum
	// required correction backwards at the configured step rate.
	SteppedSurfaceHeights.Last() = Targets.Last();
	const float MaximumSegmentChange = VerticalStep * FMath::Max(1, Biome.MaximumVerticalStepsPerSegment);
	for (int32 Segment = SegmentCount - 2; Segment >= 0; --Segment)
	{
		SteppedSurfaceHeights[Segment] = FMath::Clamp(SteppedSurfaceHeights[Segment],
			SteppedSurfaceHeights[Segment + 1] - MaximumSegmentChange,
			SteppedSurfaceHeights[Segment + 1] + MaximumSegmentChange);
		SteppedSurfaceHeights[Segment] = FMath::RoundToFloat(SteppedSurfaceHeights[Segment] / VerticalStep) * VerticalStep;
	}

	const float PreviousLastCentre = -0.5f - 0.5f / ActiveSurfaceSegmentsPerTile;
	const float NextFirstCentre = Count - 0.5f + 0.5f / ActiveSurfaceSegmentsPerTile;
	LeftGhostSurfaceHeight = FMath::RoundToFloat(SampleFilteredHeight(PreviousLastCentre) / VerticalStep) * VerticalStep;
	RightGhostSurfaceHeight = FMath::RoundToFloat(SampleFilteredHeight(NextFirstCentre) / VerticalStep) * VerticalStep;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		float MaximumHeight = -BIG_NUMBER;
		for (int32 Sub = 0; Sub < ActiveSurfaceSegmentsPerTile; ++Sub)
		{
			MaximumHeight = FMath::Max(MaximumHeight,
				SteppedSurfaceHeights[Index * ActiveSurfaceSegmentsPerTile + Sub]);
		}
		SurfaceHeightTiles[Index] = FMath::CeilToInt(MaximumHeight / ActiveTileSize);
	}

	bFrontLaneEnabled = Biome.FrontLane.bEnabled;
	bBackLaneEnabled = Biome.BackLane.bEnabled;
	FrontSteppedSurfaceHeights.Reset();
	BackSteppedSurfaceHeights.Reset();
	FrontSurfaceHeightTiles.Reset();
	BackSurfaceHeightTiles.Reset();
	FrontConnectionAlpha.Reset();
	BackConnectionAlpha.Reset();

	auto BuildOptionalLane = [&](const FPWGTerrainLaneSettings& Settings, const EPWGTerrainLane Lane,
		TArray<float>& OutHeights, TArray<int32>& OutTiles, TArray<float>& OutConnections)
	{
		OutHeights.SetNumUninitialized(SegmentCount);
		OutConnections.SetNumUninitialized(SegmentCount);
		FRandomStream LaneSeed((ActiveWorldSeed != 0 ? ActiveWorldSeed : Biome.Seed) + Settings.NoiseSeedOffset);
		const float Phase = LaneSeed.FRandRange(-10000.0f,10000.0f);
		const float MiddleBase = Biome.BaseSurfaceHeightInTiles*ActiveTileSize;
		for (int32 Segment=0;Segment<SegmentCount;++Segment)
		{
			const double GlobalTile = static_cast<double>(GlobalColumnOffset)
				+ (static_cast<double>(Segment)+0.5)/ActiveSurfaceSegmentsPerTile;
			const float Sample = static_cast<float>((GlobalTile+Phase)*Biome.NoiseFrequency
				*FMath::Max(0.05f,Settings.NoiseFrequencyMultiplier));
			const float Noise = (FMath::PerlinNoise1D(Sample)
				+0.45f*FMath::PerlinNoise1D(Sample*2.17f+31.7f))/1.45f;
			float Contact = EvaluateLaneConnectionAlpha(Settings,Lane,GlobalTile);
			// Main-biome boundaries always converge, preventing mismatched lane offsets
			// from creating a wall between two streaming regions.
			const int32 Tile = Segment/ActiveSurfaceSegmentsPerTile;
			const int32 EdgeBlend = FMath::Max(1,Biome.BiomeEdgeSmoothingDistanceInTiles);
			if (Tile<EdgeBlend && GetBiomeRowForChunk(StreamingChunkIndex-1)!=ActiveBiomeRow)
				Contact=FMath::Max(Contact,1.0f-static_cast<float>(Tile)/EdgeBlend);
			if (Tile>=Count-EdgeBlend && GetBiomeRowForChunk(StreamingChunkIndex+1)!=ActiveBiomeRow)
				Contact=FMath::Max(Contact,1.0f-static_cast<float>(Count-1-Tile)/EdgeBlend);
			const float Independent = MiddleBase + Settings.HeightOffsetInTiles*ActiveTileSize
				+ (SteppedSurfaceHeights[Segment]-MiddleBase)*Settings.MiddleShapeInfluence
				+ Noise*Biome.HeightVariationInTiles*ActiveTileSize*Settings.HeightVariationMultiplier;
			OutHeights[Segment]=FMath::RoundToFloat(FMath::Lerp(Independent,SteppedSurfaceHeights[Segment],Contact)/VerticalStep)*VerticalStep;
			OutConnections[Segment]=Contact;
		}
		OutTiles.SetNumUninitialized(Count);
		for (int32 Tile=0;Tile<Count;++Tile)
		{
			float MaxHeight=-BIG_NUMBER;
			for (int32 Sub=0;Sub<ActiveSurfaceSegmentsPerTile;++Sub)
				MaxHeight=FMath::Max(MaxHeight,OutHeights[Tile*ActiveSurfaceSegmentsPerTile+Sub]);
			OutTiles[Tile]=FMath::CeilToInt(MaxHeight/ActiveTileSize);
		}
	};
	if (bFrontLaneEnabled)
		BuildOptionalLane(Biome.FrontLane,EPWGTerrainLane::Front,FrontSteppedSurfaceHeights,FrontSurfaceHeightTiles,FrontConnectionAlpha);
	if (bBackLaneEnabled)
		BuildOptionalLane(Biome.BackLane,EPWGTerrainLane::Back,BackSteppedSurfaceHeights,BackSurfaceHeightTiles,BackConnectionAlpha);

	FrontLeftGhostSurfaceHeight = bFrontLaneEnabled ? FrontSteppedSurfaceHeights[0] : LeftGhostSurfaceHeight;
	FrontRightGhostSurfaceHeight = bFrontLaneEnabled ? FrontSteppedSurfaceHeights.Last() : RightGhostSurfaceHeight;
	BackLeftGhostSurfaceHeight = bBackLaneEnabled ? BackSteppedSurfaceHeights[0] : LeftGhostSurfaceHeight;
	BackRightGhostSurfaceHeight = bBackLaneEnabled ? BackSteppedSurfaceHeights.Last() : RightGhostSurfaceHeight;

	int32 MinimumHeight = SurfaceHeightTiles[0];
	for (const int32 Height : SurfaceHeightTiles) MinimumHeight=FMath::Min(MinimumHeight,Height);
	for (const int32 Height : FrontSurfaceHeightTiles) MinimumHeight=FMath::Min(MinimumHeight,Height);
	for (const int32 Height : BackSurfaceHeightTiles) MinimumHeight=FMath::Min(MinimumHeight,Height);
	BottomLocalZ = (MinimumHeight - FMath::Max(1, Biome.MinimumUndergroundDepth)) * ActiveTileSize;
}

float APWG_Generator::EvaluateSmoothedGlobalHeight(const FBiomeData& Biome, const int64 GlobalColumn) const
{
	if (!BiomeTable || StreamingBiomeRows.Num() < 2
		|| (BiomeTransitionWidthInTiles <= 0 && Biome.BiomeEdgeSmoothingDistanceInTiles <= 0))
	{
		return EvaluateBiomeHeight(Biome, GlobalColumn);
	}

	const int64 ColumnsPerChunk = FMath::Max<int64>(1, Biome.ChunkLengthInTiles);
	const int64 OwnerChunk64 = GlobalColumn >= 0
		? GlobalColumn / ColumnsPerChunk
		: -(((-GlobalColumn - 1) / ColumnsPerChunk) + 1);
	const int32 OwnerChunk = static_cast<int32>(FMath::Clamp<int64>(OwnerChunk64, MIN_int32, MAX_int32));
	const FName CurrentRow = GetBiomeRowForChunk(OwnerChunk);
	const FBiomeData* CurrentBiome = FindBiomeData(CurrentRow, TEXT("PWG biome height"));
	if (!CurrentBiome) CurrentBiome = &Biome;

	// Search actual biome-region boundaries, rather than only this chunk's two
	// edges. A gentle transition is allowed to extend through several chunks.
	const double Position = static_cast<double>(GlobalColumn) + 0.5;
	const int32 SearchRadius = FMath::Max(2, FMath::Max(1, ChunksPerBiome) + 2);
	double NearestDistance = TNumericLimits<double>::Max();
	const FBiomeData* BlendLeft = nullptr;
	const FBiomeData* BlendRight = nullptr;
	double BlendBoundary = 0.0;
	double BlendWidth = 0.0;
	for (int32 BoundaryChunk = OwnerChunk - SearchRadius; BoundaryChunk <= OwnerChunk + SearchRadius + 1; ++BoundaryChunk)
	{
		const FName LeftRow = GetBiomeRowForChunk(BoundaryChunk - 1);
		const FName RightRow = GetBiomeRowForChunk(BoundaryChunk);
		if (LeftRow == RightRow) continue;
		const FBiomeData* LeftBiome = FindBiomeData(LeftRow, TEXT("PWG region transition left"));
		const FBiomeData* RightBiome = FindBiomeData(RightRow, TEXT("PWG region transition right"));
		if (!LeftBiome || !RightBiome) continue;

		const int64 BoundaryColumn = static_cast<int64>(BoundaryChunk) * ColumnsPerChunk;
		const double Boundary = static_cast<double>(BoundaryColumn);
		const float DifferenceInTiles = FMath::Abs(
			EvaluateBiomeHeight(*LeftBiome, BoundaryColumn)
			- EvaluateBiomeHeight(*RightBiome, BoundaryColumn)) / FMath::Max(1.0f, ActiveTileSize);
		const double AllowedSlope = FMath::Max(0.02f,
			FMath::Min(LeftBiome->BiomeTransitionMaximumSlope, RightBiome->BiomeTransitionMaximumSlope));
		const double RequiredWidth = 0.75 * DifferenceInTiles / AllowedSlope;
		const double RequestedWidth = FMath::Max3<double>(BiomeTransitionWidthInTiles,
			LeftBiome->BiomeEdgeSmoothingDistanceInTiles, RightBiome->BiomeEdgeSmoothingDistanceInTiles);
		// Keep neighbouring transitions from crossing the centre of their regions.
		const double MaximumWidth = ColumnsPerChunk * FMath::Max(1, ChunksPerBiome) * 0.49;
		const double Width = FMath::Min(FMath::Max(RequestedWidth, RequiredWidth), MaximumWidth);
		const double Distance = FMath::Abs(Position - Boundary);
		if (Width > 0.0 && Distance < Width && Distance < NearestDistance)
		{
			NearestDistance = Distance;
			BlendLeft = LeftBiome;
			BlendRight = RightBiome;
			BlendBoundary = Boundary;
			BlendWidth = Width;
		}
	}
	if (BlendLeft && BlendRight)
	{
		const float Alpha = FMath::SmoothStep(0.0f, 1.0f,
			static_cast<float>((Position - BlendBoundary + BlendWidth) / (2.0 * BlendWidth)));
		return FMath::Lerp(EvaluateBiomeHeight(*BlendLeft, GlobalColumn),
			EvaluateBiomeHeight(*BlendRight, GlobalColumn), Alpha);
	}
	return EvaluateBiomeHeight(*CurrentBiome, GlobalColumn);
}

float APWG_Generator::EvaluateBiomeHeight(const FBiomeData& Biome, const int64 GlobalColumn) const
{
	FRandomStream PhaseRandom(ActiveWorldSeed != 0 ? ActiveWorldSeed : Biome.Seed);
	const float Phase = PhaseRandom.FRandRange(-10000.0f, 10000.0f);
	const float Sample = (static_cast<double>(GlobalColumn) + Phase) * Biome.NoiseFrequency;
	const float Noise = (FMath::PerlinNoise1D(Sample)
		+ 0.45f * FMath::PerlinNoise1D(Sample * 2.17f + 31.7f)) / 1.45f;
	return (Biome.BaseSurfaceHeightInTiles + Noise * Biome.HeightVariationInTiles) * ActiveTileSize;
}

float APWG_Generator::EvaluateLaneConnectionAlpha(const FPWGTerrainLaneSettings& Settings,
	const EPWGTerrainLane Lane, const double GlobalTileCoordinate) const
{
	const double Spacing=FMath::Max(12,Settings.ConnectionSpacingInTiles);
	const uint32 Hash=HashCombineFast(GetTypeHash(ActiveWorldSeed),
		HashCombineFast(GetTypeHash(Settings.NoiseSeedOffset),GetTypeHash(static_cast<uint8>(Lane))));
	const double Offset=static_cast<double>(Hash%FMath::Max(1,Settings.ConnectionSpacingInTiles));
	double Phase=FMath::Fmod(GlobalTileCoordinate+Offset,Spacing);
	if (Phase<0.0) Phase+=Spacing;
	const double Distance=FMath::Min(Phase,Spacing-Phase);
	const float HalfShared=FMath::Max(0.5f,Settings.ConnectionWidthInTiles*0.5f);
	if (Distance<=HalfShared) return 1.0f;
	const float Blend=FMath::Max(1.0f,static_cast<float>(Settings.ConnectionBlendInTiles));
	return 1.0f-FMath::SmoothStep(0.0f,1.0f,FMath::Clamp(static_cast<float>(Distance-HalfShared)/Blend,0.0f,1.0f));
}

void APWG_Generator::BuildAtlasColumnPath(const FBiomeData& Biome, FRandomStream& Random)
{
	const int32 Count = SurfaceHeightTiles.Num();
	ColumnMiniBiomes.Init(1, Count);
	ColumnBlendFrom.Init(-1, Count);
	ColumnBlendTo.Init(-1, Count);
	ColumnBlendStep.Init(-1, Count);
	ColumnAtlasX.Init(9, Count); // BIOME2 Surface_2 on the top edge.
	ColumnAtlasY.Init(0, Count);
	ColumnAtlasDirection.Init(0, Count);
	if (Count <= 0) return;

	if (Biome.bUseCompactBlockAtlas)
	{
		// Six optional mini-biomes. No one type is forced into a chunk. Ground is
		// the universal separator and also makes neighbouring streamed chunks meet
		// without reinstating the old mandatory MiniBiome02 boundary.
		struct FWeightedMini
		{
			int32 RuleIndex = 0;
			float Weight = 1.0f;
			FVector2D PatchMultiplier = FVector2D(0.75f, 1.25f);
		};
		TArray<FWeightedMini> Candidates;
		for (int32 RuleIndex = 0; RuleIndex < 6; ++RuleIndex)
		{
			FPWGMiniBiomeGenerationRule Rule;
			if (Biome.MiniBiomeGenerationRules.IsValidIndex(RuleIndex))
			{
				Rule = Biome.MiniBiomeGenerationRules[RuleIndex];
			}
			else if (RuleIndex == 4)
			{
				Rule.Weight = 0.35f;
				Rule.PatchLengthMultiplierRange = FVector2D(0.45f, 0.80f);
			}
			else if (RuleIndex == 5)
			{
				Rule.Weight = 0.06f;
				Rule.PatchLengthMultiplierRange = FVector2D(0.12f, 0.28f);
			}
			if (Rule.bEnabled && Rule.Weight > 0.0f)
			{
				Candidates.Add({ RuleIndex, Rule.Weight, Rule.PatchLengthMultiplierRange });
			}
		}
		if (Candidates.IsEmpty())
		{
			Candidates.Add({ 1, 1.0f, FVector2D(0.75f, 1.25f) });
		}
		auto PickCandidate = [&]() -> const FWeightedMini&
		{
			float TotalWeight = 0.0f;
			for (const FWeightedMini& Candidate : Candidates) TotalWeight += Candidate.Weight;
			float Roll = Random.FRandRange(0.0f, FMath::Max(KINDA_SMALL_NUMBER, TotalWeight));
			for (const FWeightedMini& Candidate : Candidates)
			{
				Roll -= Candidate.Weight;
				if (Roll <= 0.0f) return Candidate;
			}
			return Candidates.Last();
		};

		ColumnMiniBiomes.Init(static_cast<uint8>(EPWGSurfaceStyle::Ground), Count);
		const int32 EdgeGround = FMath::Clamp(
			FMath::Min(Biome.CompactSurfaceGroundGapMin, Biome.CompactSurfaceGroundGapMax),
			1, FMath::Max(1, Count / 8));
		int32 Position = EdgeGround;
		const int32 End = FMath::Max(Position, Count - EdgeGround);
		int32 PreviousRule = INDEX_NONE;
		while (Position < End)
		{
			const FWeightedMini* Selected = &PickCandidate();
			if (Candidates.Num() > 1 && Selected->RuleIndex == PreviousRule)
			{
				for (int32 Retry = 0; Retry < 3 && Selected->RuleIndex == PreviousRule; ++Retry)
				{
					Selected = &PickCandidate();
				}
			}
			const float PatchMultiplier = Random.FRandRange(
				FMath::Min(Selected->PatchMultiplier.X, Selected->PatchMultiplier.Y),
				FMath::Max(Selected->PatchMultiplier.X, Selected->PatchMultiplier.Y));
			const int32 MinimumPatchLength = Selected->RuleIndex == 5 ? 2
				: (Selected->RuleIndex == 4 ? 4 : 6);
			const int32 PatchLength = FMath::Max(MinimumPatchLength, FMath::RoundToInt(
				Biome.TerrainStylePatchSizeInTiles * FMath::Max(0.05f, PatchMultiplier)));
			const uint8 SurfaceStyle = static_cast<uint8>(MiniBiomeRuleToSurfaceStyle(Selected->RuleIndex));
			for (int32 I = 0; I < PatchLength && Position < End; ++I)
			{
				ColumnMiniBiomes[Position++] = SurfaceStyle;
			}
			PreviousRule = Selected->RuleIndex;
			if (Position >= End) break;
			const int32 Gap = Random.RandRange(
				FMath::Min(Biome.CompactSurfaceGroundGapMin, Biome.CompactSurfaceGroundGapMax),
				FMath::Max(Biome.CompactSurfaceGroundGapMin, Biome.CompactSurfaceGroundGapMax));
			for (int32 I = 0; I < Gap && Position < End; ++I)
			{
				ColumnMiniBiomes[Position++] = static_cast<uint8>(EPWGSurfaceStyle::Ground);
			}
		}
		return;
	}

	const int32 Boundary = FMath::Clamp(Biome.MiniBiome2BoundaryWidth, 4, FMath::Max(4, Count / 3));
	const int32 Start = FMath::Min(Boundary, Count);
	const int32 End = FMath::Max(Start, Count - Boundary);
	// Clockwise outer ring. Consecutive entries are always orthogonal neighbours in the CSV grid.
	TArray<FIntPoint> Perimeter;
	for (int32 X = 0; X < 12; ++X) Perimeter.Emplace(X, 0);
	for (int32 Y = 1; Y < 12; ++Y) Perimeter.Emplace(11, Y);
	for (int32 X = 10; X >= 0; --X) Perimeter.Emplace(X, 11);
	for (int32 Y = 10; Y >= 1; --Y) Perimeter.Emplace(0, Y);
	constexpr int32 TargetCursor = 9;
	int32 Cursor = TargetCursor;
	int32 TravelDirection = Random.RandBool() ? 1 : -1;
	int32 BlendExitCooldown = 0;

	auto IsBlendCoordinate = [](const FIntPoint P)
	{
		return (P.Y == 0 && P.X >= 4 && P.X <= 7)
			|| (P.X == 11 && P.Y >= 4 && P.Y <= 7)
			|| (P.Y == 11 && P.X >= 4 && P.X <= 7)
			|| (P.X == 0 && P.Y >= 4 && P.Y <= 7);
	};
	auto MiniBiomeAt = [](const FIntPoint P)
	{
		if (P.Y == 0) return P.X <= 5 ? 0 : 1;
		if (P.X == 11) return P.Y <= 5 ? 1 : 3;
		if (P.Y == 11) return P.X <= 5 ? 2 : 3;
		return P.Y <= 5 ? 0 : 2;
	};
	auto WriteColumn = [&](const int32 Column, const int32 Direction)
	{
		const FIntPoint P = Perimeter[Cursor];
		ColumnAtlasX[Column] = static_cast<uint8>(P.X);
		ColumnAtlasY[Column] = static_cast<uint8>(P.Y);
		ColumnAtlasDirection[Column] = static_cast<int8>(Direction);
		ColumnMiniBiomes[Column] = static_cast<uint8>(MiniBiomeAt(P));
		if (!IsBlendCoordinate(P) || Direction == 0) return;
		int32 BackCursor = Cursor;
		int32 BlendStep = -1;
		do
		{
			BackCursor = (BackCursor - Direction + Perimeter.Num()) % Perimeter.Num();
			++BlendStep;
		} while (IsBlendCoordinate(Perimeter[BackCursor]));
		const int32 From = MiniBiomeAt(Perimeter[BackCursor]);
		int32 ProbeCursor = Cursor;
		while (IsBlendCoordinate(Perimeter[ProbeCursor]))
		{
			ProbeCursor = (ProbeCursor + Direction + Perimeter.Num()) % Perimeter.Num();
		}
		const int32 To = MiniBiomeAt(Perimeter[ProbeCursor]);
		ColumnBlendFrom[Column] = static_cast<int8>(From);
		ColumnBlendTo[Column] = static_cast<int8>(To);
		ColumnBlendStep[Column] = static_cast<int8>(BlendStep);
	};

	for (int32 Column = 0; Column < Start; ++Column) WriteColumn(Column, 0);
	int32 Position = Start;
	const int32 ProceduralEnd = FMath::Max(Position, End - (Perimeter.Num() - 1));
	while (Position < ProceduralEnd)
	{
		const bool bInsideBlend = IsBlendCoordinate(Perimeter[Cursor]);
		int32 Step = 0;
		if (bInsideBlend)
		{
			Step = TravelDirection; // Blend never repeats and never reverses midway.
		}
		else
		{
			const int32 CurrentMiniBiome = MiniBiomeAt(Perimeter[Cursor]);
			const float MiniBiomeWeight = Biome.TerrainStyles.IsValidIndex(CurrentMiniBiome)
				? FMath::Max(0.1f, Biome.TerrainStyles[CurrentMiniBiome].Weight) : 1.0f;
			const float MoveChance = FMath::Clamp(6.0f /
				FMath::Max(6.0f, Biome.TerrainStylePatchSizeInTiles * MiniBiomeWeight), 0.08f, 0.55f);
			if (Random.FRand() < MoveChance)
			{
				if (BlendExitCooldown <= 0 && Random.FRand() < 0.28f) TravelDirection *= -1;
				Step = TravelDirection;
			}
		}
		if (BlendExitCooldown > 0) --BlendExitCooldown;
		if (Step != 0)
		{
			const int32 NextCursor = (Cursor + Step + Perimeter.Num()) % Perimeter.Num();
			if (bInsideBlend && !IsBlendCoordinate(Perimeter[NextCursor])) BlendExitCooldown = 4;
			Cursor = NextCursor;
		}
		WriteColumn(Position++, Step == 0 ? TravelDirection : Step);
	}

	// Continue in the existing direction until BIOME2. This deliberately reserves a
	// complete perimeter, so returning to the chunk boundary never reverses a blend.
	while (Position < End && Cursor != TargetCursor)
	{
		Cursor = (Cursor + TravelDirection + Perimeter.Num()) % Perimeter.Num();
		WriteColumn(Position++, TravelDirection);
	}
	Cursor = TargetCursor;
	while (Position < Count) WriteColumn(Position++, 0);
}

void APWG_Generator::AddQuad(FMeshBuffers& B, const FVector& A, const FVector& Bp,
	const FVector& C, const FVector& D, const FVector& Normal, const FVector& Tangent,
	const int32 AtlasIndex, const FBiomeData& Biome, const float TileU0, const float TileU1,
	const uint8 QuarterTurns, const bool bMirrorU, const float TileV0, const float TileV1) const
{
	const int32 Base = B.Vertices.Num();
	B.Vertices.Append({ A, Bp, C, D });
	B.Triangles.Append({ Base, Base + 1, Base + 2, Base, Base + 2, Base + 3 });
	const FVector QuadVertices[4] = { A, Bp, C, D };
	const float QuadCentreX = (A.X + Bp.X + C.X + D.X) * 0.25f;
	const int32 QuadColumn = FMath::Clamp(
		FMath::FloorToInt((QuadCentreX - ChunkStartLocalX) / FMath::Max(1.0f, ActiveTileSize)),
		0, FMath::Max(0, ColumnMiniBiomes.Num() - 1));
	const float EncodedSurfaceStyle = ColumnMiniBiomes.IsValidIndex(QuadColumn)
		? static_cast<float>(CompactTerrainStyleForSurface(ColumnMiniBiomes[QuadColumn])) / 8.0f : 0.125f;
	for (int32 I = 0; I < 4; ++I)
	{
		B.Normals.Add(Normal);
		EPWGTerrainLane Lane = EPWGTerrainLane::Middle;
		float BestLaneDistance = FMath::Abs(QuadVertices[I].Y - GetLaneCenterYLocal(Lane));
		if (bFrontLaneEnabled && FMath::Abs(QuadVertices[I].Y - GetLaneCenterYLocal(EPWGTerrainLane::Front)) < BestLaneDistance)
		{
			Lane = EPWGTerrainLane::Front;
			BestLaneDistance = FMath::Abs(QuadVertices[I].Y - GetLaneCenterYLocal(Lane));
		}
		if (bBackLaneEnabled && FMath::Abs(QuadVertices[I].Y - GetLaneCenterYLocal(EPWGTerrainLane::Back)) < BestLaneDistance)
		{
			Lane = EPWGTerrainLane::Back;
		}
		const float SurfaceZ = GetLaneSurfaceHeightAtLocalX(QuadVertices[I].X, Lane);
		const float Depth = FMath::Max(0.0f, SurfaceZ - QuadVertices[I].Z);
		if (Biome.bUseCompactBlockAtlas)
		{
			// R is continuous surface-relative depth and B is the surface mini-biome style.
			// The shader uses these values to draw contour-following visual layers; collision stays block based.
			const float EncodedDepth = FMath::Clamp(Depth / FMath::Max(100.0f, Biome.TerrainVisualDepthRange), 0.0f, 1.0f);
			B.Colors.Add(FLinearColor(EncodedDepth, 0.0f, EncodedSurfaceStyle, 1.0f).ToFColor(false));
		}
		else
		{
			const float Start = FMath::Max(0.0f, Biome.TerrainGradientStartDepth);
			const float End = FMath::Max(Start + 1.0f, Biome.TerrainGradientFullDepth);
			const float Gradient = FMath::SmoothStep(0.0f, 1.0f,
				FMath::Clamp((Depth - Start) / (End - Start), 0.0f, 1.0f));
			B.Colors.Add(FMath::Lerp(Biome.TerrainSurfaceLightColor, Biome.TerrainDeepLightColor, Gradient).ToFColor(true));
		}
		B.Tangents.Add(FProcMeshTangent(Tangent, false));
	}

	const int32 Columns = Biome.bUseCompactBlockAtlas ? 9 : 12;
	const int32 Rows = Biome.bUseCompactBlockAtlas ? 9 : 12;
	const int32 Tile = FMath::Clamp(AtlasIndex, 0, Columns * Rows - 1);
	const int32 Column = Tile % Columns;
	const int32 Row = Tile / Columns;
	const float LocalU0 = FMath::Clamp(TileU0, 0.0f, 1.0f);
	const float LocalU1 = FMath::Clamp(TileU1, 0.0f, 1.0f);
	const float LocalV0 = FMath::Clamp(TileV0, 0.0f, 1.0f);
	const float LocalV1 = FMath::Clamp(TileV1, 0.0f, 1.0f);
	FVector2D LocalUVs[4] = {
		FVector2D(LocalU0, LocalV1), FVector2D(LocalU1, LocalV1),
		FVector2D(LocalU1, LocalV0), FVector2D(LocalU0, LocalV0)
	};
	for (FVector2D& UV : LocalUVs)
	{
		if (bMirrorU) UV.X = 1.0f - UV.X;
		switch (QuarterTurns & 3)
		{
		case 1: UV = FVector2D(UV.Y, 1.0f - UV.X); break;
		case 2: UV = FVector2D(1.0f - UV.X, 1.0f - UV.Y); break;
		case 3: UV = FVector2D(1.0f - UV.Y, UV.X); break;
		default: break;
		}
		UV.X = (Column + UV.X) / Columns;
		UV.Y = (Row + UV.Y) / Rows;
		B.UVs.Add(UV);
	}
}

void APWG_Generator::ApplyStylizedTerrainLighting(const FLinearColor& Tint, const float Brightness)
{
	const float SafeBrightness = FMath::Max(0.0f, Brightness);
	auto ApplyToChunk = [&](APWG_Generator* Chunk)
	{
		if (!IsValid(Chunk) || !Chunk->TerrainMaterialInstance) return;
		Chunk->TerrainMaterialInstance->SetVectorParameterValue(TEXT("WorldLightTint"), Tint);
		Chunk->TerrainMaterialInstance->SetScalarParameterValue(TEXT("WorldLightBrightness"), SafeBrightness);
	};
	ApplyToChunk(this);
	for (const TPair<int32, TObjectPtr<APWG_Generator>>& Pair : LoadedChunks)
	{
		ApplyToChunk(Pair.Value);
	}
}

int32 APWG_Generator::PickTile(const TArray<int32>& Pool, FRandomStream& Random) const
{
	return Pool.IsEmpty() ? 0 : Pool[Random.RandRange(0, Pool.Num() - 1)];
}

APWG_Generator::FAtlasTileChoice APWG_Generator::PickUniversalTerrainTile(
	const FBiomeData& Biome, const int32 X, const ETerrainTileRole TileRole,
	FRandomStream& Random, const int32 DepthBelowSurface, const bool bAllowCompactBlend) const
{
	FAtlasTileChoice Result;
	if (Biome.bUseCompactBlockAtlas)
	{
		// Atlas groups: MB1, MB2, MB3 / MB4, Rock, Ground /
		// MB6, DeepRock, MB5. DeepRock is an underground-only style.
		constexpr int32 RockStyle = 4;
		constexpr int32 GroundStyle = 5;
		constexpr int32 DeepRockStyle = 7;
		auto StyleAtDepth = [&](const int32 CellX, const int32 Depth) -> int32
		{
			if (Depth < 0) return INDEX_NONE;
			const int32 SafeX = FMath::Clamp(CellX, 0, FMath::Max(0, ColumnMiniBiomes.Num() - 1));
			const int32 SurfaceStyle = ColumnMiniBiomes.IsValidIndex(SafeX)
				? CompactTerrainStyleForSurface(ColumnMiniBiomes[SafeX]) : 1;
			if (Depth < FMath::Max(1, Biome.CompactBiomeDepthInTiles)) return SurfaceStyle;
			const uint32 ColumnHash = HashCombineFast(GetTypeHash(GlobalColumnOffset + SafeX),
				static_cast<uint32>(ActiveWorldSeed != 0 ? ActiveWorldSeed : Biome.Seed));
			const int32 GroundMin = FMath::Max(1, FMath::Min(Biome.CompactGroundDepthMin, Biome.CompactGroundDepthMax));
			const int32 GroundMax = FMath::Max(GroundMin, FMath::Max(Biome.CompactGroundDepthMin, Biome.CompactGroundDepthMax));
			const int32 GroundDepth = GroundMin + static_cast<int32>(ColumnHash % static_cast<uint32>(GroundMax - GroundMin + 1));
			const int32 GroundEnd = Biome.CompactBiomeDepthInTiles + GroundDepth;
			if (Depth < GroundEnd) return GroundStyle;
			return Depth < GroundEnd + FMath::Max(1, Biome.CompactRockDepthInTiles)
				? RockStyle : DeepRockStyle;
		};
		auto Priority = [](const int32 Style)
		{
			// MB1 > MB2 > MB3 > MB4 > MB5 > MB6 > Ground > Rock > DeepRock.
			switch (Style)
			{
			case 0: return 0;
			case 1: return 1;
			case 2: return 2;
			case 3: return 3;
			case 8: return 4;
			case 6: return 5;
			case GroundStyle: return 6;
			case RockStyle: return 7;
			default: return 8;
			}
		};
		auto Centre = [](const int32 Style)
		{
			const int32 SafeStyle = FMath::Clamp(Style, 0, 8);
			return FIntPoint((SafeStyle % 3) * 3 + 1, (SafeStyle / 3) * 3 + 1);
		};

		const int32 SafeDepth = FMath::Max(0, DepthBelowSurface);
		const int32 CurrentStyle = StyleAtDepth(X, SafeDepth);
		const int32 SafeCurrentX = FMath::Clamp(X, 0, FMath::Max(0, SurfaceHeightTiles.Num() - 1));
		const int32 ReferenceCellZ = SurfaceHeightTiles.IsValidIndex(SafeCurrentX)
			? SurfaceHeightTiles[SafeCurrentX] - SafeDepth : -SafeDepth;
		const bool bFollowSurfacePath = SafeDepth == 0
			&& (TileRole == ETerrainTileRole::Top || TileRole == ETerrainTileRole::SurfaceToGround);
		auto NeighbourStyleAt = [&](const int32 DX, const int32 DY) -> int32
		{
			const int32 NeighbourX = FMath::Clamp(X + DX, 0, FMath::Max(0, SurfaceHeightTiles.Num() - 1));
			if (bFollowSurfacePath)
			{
				if (DY < 0) return INDEX_NONE;
				return StyleAtDepth(NeighbourX, DY);
			}
			// Compare cells at the same absolute terrain Z. Depth-from-surface is
			// different in neighbouring columns whenever the ground is sloped.
			const int32 NeighbourWorldZ = ReferenceCellZ - DY;
			const int32 NeighbourDepth = SurfaceHeightTiles.IsValidIndex(NeighbourX)
				? SurfaceHeightTiles[NeighbourX] - NeighbourWorldZ : SafeDepth + DY;
			return StyleAtDepth(NeighbourX, NeighbourDepth);
		};
		if (!bAllowCompactBlend)
		{
			const FIntPoint Tile = Centre(CurrentStyle);
			Result.Index = Tile.Y * 9 + Tile.X;
			return Result;
		}
		int32 DominantStyle = INDEX_NONE;
		bool bHasCardinalCandidate = false;
		int32 BestPriority = MAX_int32;
		for (int32 DY = -1; DY <= 1; ++DY)
		{
			for (int32 DX = -1; DX <= 1; ++DX)
			{
				if ((DX == 0 && DY == 0) || SafeDepth + DY < 0) continue;
				const int32 NeighbourStyle = NeighbourStyleAt(DX, DY);
				if (NeighbourStyle == INDEX_NONE) continue;
				const int32 NeighbourPriority = Priority(NeighbourStyle);
				if (NeighbourPriority >= Priority(CurrentStyle)) continue;
				const bool bCardinal = DX == 0 || DY == 0;
				if ((bCardinal && !bHasCardinalCandidate)
					|| (bCardinal == bHasCardinalCandidate && NeighbourPriority < BestPriority))
				{
					DominantStyle = NeighbourStyle;
					BestPriority = NeighbourPriority;
					bHasCardinalCandidate = bCardinal;
				}
			}
		}
		if (DominantStyle == INDEX_NONE)
		{
			const FIntPoint Tile = Centre(CurrentStyle);
			Result.Index = Tile.Y * 9 + Tile.X;
			return Result;
		}
		// Combine every touching direction of the winning material. A straight
		// border resolves to a side/bottom tile; a corner resolves to one of the
		// four diagonal tiles. Thus all eight atlas directions are reachable.
		FIntPoint DirectionSum = FIntPoint::ZeroValue;
		for (int32 DY = -1; DY <= 1; ++DY)
		{
			for (int32 DX = -1; DX <= 1; ++DX)
			{
				if ((DX == 0 && DY == 0) || SafeDepth + DY < 0) continue;
				if (NeighbourStyleAt(DX, DY) == DominantStyle)
				{
					const int32 Weight = (DX == 0 || DY == 0) ? 3 : 1;
					DirectionSum.X += DX * Weight;
					DirectionSum.Y += DY * Weight;
				}
			}
		}
		FIntPoint BlendOffset(-FMath::Sign(DirectionSum.X), -FMath::Sign(DirectionSum.Y));
		if (BlendOffset == FIntPoint::ZeroValue)
		{
			// Symmetric neighbourhood: prefer the direct edge that selected the material.
			for (int32 DY = -1; DY <= 1 && BlendOffset == FIntPoint::ZeroValue; ++DY)
			{
				for (int32 DX = -1; DX <= 1; ++DX)
				{
					if ((DX == 0) == (DY == 0) && !(DX == 0 && DY == 0)) continue;
					if (NeighbourStyleAt(DX, DY) == DominantStyle)
					{
						BlendOffset = FIntPoint(-DX, -DY);
						break;
					}
				}
			}
		}
		const FIntPoint Tile = Centre(DominantStyle) + BlendOffset;
		Result.Index = FMath::Clamp(Tile.Y, 0, 8) * 9 + FMath::Clamp(Tile.X, 0, 8);
		return Result;
	}
	const int32 Mini = ColumnMiniBiomes.IsValidIndex(X) ? FMath::Clamp<int32>(ColumnMiniBiomes[X], 0, 3) : 1;
	auto Set = [&](const int32 Column, const int32 Row, const int32 Rotation = 0, const bool bMirror = false)
	{
		Result.Index = FMath::Clamp(Row, 0, 11) * 12 + FMath::Clamp(Column, 0, 11);
		Result.QuarterTurns = static_cast<uint8>(Rotation & 3);
		Result.bMirrorU = bMirror;
	};
	auto RotationFromTo = [](const int32 NativeDirection, const int32 TargetDirection)
	{
		return (TargetDirection - NativeDirection + 4) & 3;
	};
	if (ColumnAtlasX.IsValidIndex(X) && ColumnAtlasY.IsValidIndex(X)
		&& (TileRole == ETerrainTileRole::Top || TileRole == ETerrainTileRole::SurfaceToGround
			|| TileRole == ETerrainTileRole::Interior))
	{
		const FIntPoint Outer(ColumnAtlasX[X], ColumnAtlasY[X]);
		const int32 NativeDirection = Outer.Y == 0 ? 0 : Outer.X == 11 ? 1 : Outer.Y == 11 ? 2 : 3;
		int32 Inset = 0;
		if (TileRole == ETerrainTileRole::SurfaceToGround) Inset = 1;
		else if (TileRole == ETerrainTileRole::Interior)
		{
			const int32 Boundary = FMath::Clamp(Biome.MiniBiome2BoundaryWidth, 4,
				FMath::Max(4, ColumnAtlasX.Num() / 3));
			const bool bProtectedBiome2Boundary = X < Boundary || X >= ColumnAtlasX.Num() - Boundary;
			Inset = FMath::Clamp(DepthBelowSurface, 2, bProtectedBiome2Boundary ? 3 : 4);
		}
		const int32 Column = FMath::Clamp(Outer.X, Inset, 11 - Inset);
		const int32 Row = FMath::Clamp(Outer.Y, Inset, 11 - Inset);
		Set(Column, Row, RotationFromTo(NativeDirection, 0),
			ColumnAtlasDirection.IsValidIndex(X) && ColumnAtlasDirection[X] < 0);
		return Result;
	}
	const int32 NativeSurfaceDirection = Mini < 2 ? 0 : 2;
	const int32 NativeSideDirection = (Mini == 0 || Mini == 2) ? 3 : 1;
	if (TileRole == ETerrainTileRole::Top || TileRole == ETerrainTileRole::Bottom)
	{
		const int32 Column = (Mini == 0 || Mini == 2) ? Random.RandRange(1, 3) : Random.RandRange(8, 10);
		const int32 Row = Mini < 2 ? 0 : 11;
		const int32 Target = TileRole == ETerrainTileRole::Top ? 0 : 2;
		Set(Column, Row, RotationFromTo(NativeSurfaceDirection, Target));
		return Result;
	}

	if (TileRole == ETerrainTileRole::Left || TileRole == ETerrainTileRole::Right)
	{
		const int32 Column = NativeSideDirection == 3 ? 0 : 11;
		const int32 Row = Mini < 2 ? Random.RandRange(1, 3) : Random.RandRange(8, 10);
		const int32 Target = TileRole == ETerrainTileRole::Left ? 3 : 1;
		Set(Column, Row, RotationFromTo(NativeSideDirection, Target));
		return Result;
	}

	const int32 Column = NativeSideDirection == 3 ? 0 : 11;
	const int32 Row = NativeSurfaceDirection == 0 ? 0 : 11;
	int32 TargetA = 0;
	int32 TargetB = 3;
	if (TileRole == ETerrainTileRole::TopRight) { TargetA = 0; TargetB = 1; }
	else if (TileRole == ETerrainTileRole::BottomLeft) { TargetA = 2; TargetB = 3; }
	else if (TileRole == ETerrainTileRole::BottomRight) { TargetA = 2; TargetB = 1; }
	for (int32 Rotation = 0; Rotation < 4; ++Rotation)
	{
		const int32 A = (NativeSurfaceDirection + Rotation) & 3;
		const int32 B = (NativeSideDirection + Rotation) & 3;
		if ((A == TargetA && B == TargetB) || (A == TargetB && B == TargetA))
		{
			Set(Column, Row, Rotation);
			return Result;
		}
	}
	Set(Column, Row);
	return Result;
}

bool APWG_Generator::IsSolidCell(const int32 X, const int32 Z) const
{
	if (X < 0 || X >= SurfaceHeightTiles.Num() || Z < BottomTileIndex || Z >= BottomTileIndex + SolidGridHeight)
	{
		return false;
	}
	return SolidCells.IsValidIndex(X * SolidGridHeight + (Z - BottomTileIndex))
		&& SolidCells[X * SolidGridHeight + (Z - BottomTileIndex)] != 0;
}

float APWG_Generator::GetLocalSurfaceHeightByIndex(const int32 Index) const
{
	if (!SurfaceHeightTiles.IsValidIndex(Index) || SteppedSurfaceHeights.IsEmpty()) return BottomLocalZ;
	const int32 CentreSegment = Index * ActiveSurfaceSegmentsPerTile + ActiveSurfaceSegmentsPerTile / 2;
	return SteppedSurfaceHeights[FMath::Clamp(CentreSegment, 0, SteppedSurfaceHeights.Num() - 1)];
}

float APWG_Generator::GetSmoothSurfaceHeightAtLocalX(const float LocalX) const
{
	if (SteppedSurfaceHeights.IsEmpty()) return BottomLocalZ;
	const float RelativeX = LocalX - ChunkStartLocalX;
	if (RelativeX < 0.0f) return LeftGhostSurfaceHeight;
	const int32 Segment = FMath::FloorToInt(RelativeX / FMath::Max(1.0f, ActiveSurfaceSegmentWidth));
	return SteppedSurfaceHeights[FMath::Clamp(Segment, 0, SteppedSurfaceHeights.Num() - 1)];
}

bool APWG_Generator::IsLaneEnabledLocal(const EPWGTerrainLane Lane) const
{
	return Lane==EPWGTerrainLane::Middle
		|| (Lane==EPWGTerrainLane::Front && bFrontLaneEnabled)
		|| (Lane==EPWGTerrainLane::Back && bBackLaneEnabled);
}

float APWG_Generator::GetLaneCenterYLocal(const EPWGTerrainLane Lane) const
{
	if (Lane==EPWGTerrainLane::Front) return -ActiveTileSize;
	if (Lane==EPWGTerrainLane::Back) return ActiveTileSize;
	return 0.0f;
}

const TArray<float>& APWG_Generator::GetLaneSteppedHeights(const EPWGTerrainLane Lane) const
{
	if (Lane==EPWGTerrainLane::Front && bFrontLaneEnabled) return FrontSteppedSurfaceHeights;
	if (Lane==EPWGTerrainLane::Back && bBackLaneEnabled) return BackSteppedSurfaceHeights;
	return SteppedSurfaceHeights;
}

float APWG_Generator::GetLaneSurfaceHeightAtLocalX(const float LocalX, const EPWGTerrainLane Lane) const
{
	const TArray<float>& Heights=GetLaneSteppedHeights(Lane);
	if (Heights.IsEmpty()) return BottomLocalZ;
	const float RelativeX=LocalX-ChunkStartLocalX;
	if (RelativeX<0.0f)
	{
		if (Lane==EPWGTerrainLane::Front) return FrontLeftGhostSurfaceHeight;
		if (Lane==EPWGTerrainLane::Back) return BackLeftGhostSurfaceHeight;
		return LeftGhostSurfaceHeight;
	}
	const int32 Segment=FMath::FloorToInt(RelativeX/FMath::Max(1.0f,ActiveSurfaceSegmentWidth));
	return Heights[FMath::Clamp(Segment,0,Heights.Num()-1)];
}

void APWG_Generator::BuildSmoothSurfaceShell(FMeshBuffers& Mesh, FMeshBuffers& BackingMesh,
	const FBiomeData& Biome, FRandomStream& Random)
{
	const int32 Subdivisions = ActiveSurfaceSegmentsPerTile;
	const float SegmentWidth = ActiveSurfaceSegmentWidth;
	const float YMin = -ChunkHalfDepth;
	const float YMax = ChunkHalfDepth;
	const float BackingInset = FMath::Max(0.5f, ActiveTileSize * 0.01f);
	TArray<FAtlasTileChoice> SurfaceChoices;
	TArray<FAtlasTileChoice> FacadeChoices;
	TArray<FAtlasTileChoice> BackingChoices;
	SurfaceChoices.SetNum(SurfaceHeightTiles.Num());
	FacadeChoices.SetNum(SurfaceHeightTiles.Num());
	BackingChoices.SetNum(SurfaceHeightTiles.Num());
	for (int32 X = 0; X < SurfaceHeightTiles.Num(); ++X)
	{
		SurfaceChoices[X] = PickUniversalTerrainTile(Biome, X, ETerrainTileRole::Top, Random);
		FacadeChoices[X] = PickUniversalTerrainTile(Biome, X, ETerrainTileRole::SurfaceToGround, Random);
		// Blend tiles contain alpha and must sit over the complete current-style
		// centre tile. Reusing the blend as its own backing caused square holes.
		BackingChoices[X] = PickUniversalTerrainTile(Biome, X, ETerrainTileRole::Interior, Random, 2, false);
	}
	TArray<float> FacadeBaseHeights;
	FacadeBaseHeights.SetNumUninitialized(SurfaceHeightTiles.Num());
	for (int32 X = 0; X < SurfaceHeightTiles.Num(); ++X)
	{
		float MinimumHeight = BIG_NUMBER;
		for (int32 Sub = 0; Sub < Subdivisions; ++Sub)
		{
			MinimumHeight = FMath::Min(MinimumHeight, SteppedSurfaceHeights[X * Subdivisions + Sub]);
		}
		FacadeBaseHeights[X] = FMath::FloorToFloat(MinimumHeight / ActiveTileSize) * ActiveTileSize;
	}

	for (int32 Segment = 0; Segment < SteppedSurfaceHeights.Num(); ++Segment)
	{
		const int32 X = Segment / Subdivisions;
		int32 RunEnd = Segment + 1;
		while (RunEnd < SteppedSurfaceHeights.Num()
			&& RunEnd / Subdivisions == X
			&& FMath::IsNearlyEqual(SteppedSurfaceHeights[RunEnd], SteppedSurfaceHeights[Segment], 0.1f))
		{
			++RunEnd;
		}
		const float X0 = ChunkStartLocalX + Segment * SegmentWidth;
		const float X1 = ChunkStartLocalX + RunEnd * SegmentWidth;
		const float SurfaceZ = SteppedSurfaceHeights[Segment];
		const float FacadeBaseZ = FacadeBaseHeights[X];
		const int32 LocalSegment = Segment % Subdivisions;
		const float TileU0 = static_cast<float>(LocalSegment) / Subdivisions;
		const float TileU1 = static_cast<float>(RunEnd - X * Subdivisions) / Subdivisions;
		const FAtlasTileChoice& SurfaceTile = SurfaceChoices[X];
		const FAtlasTileChoice& FacadeTile = FacadeChoices[X];
		const FAtlasTileChoice& BackingTile = BackingChoices[X];
		const FVector SurfaceTangent = FVector::XAxisVector;
		const FVector SurfaceNormal = FVector::UpVector;

		for (int32 Y = 0; Y < 3; ++Y)
		{
			const float Y0 = YMin + Y * ActiveTileSize;
			const float Y1 = Y0 + ActiveTileSize;
			AddQuad(BackingMesh, FVector(X0,Y0,SurfaceZ-BackingInset), FVector(X1,Y0,SurfaceZ-BackingInset),
				FVector(X1,Y1,SurfaceZ-BackingInset), FVector(X0,Y1,SurfaceZ-BackingInset),
				SurfaceNormal, SurfaceTangent, BackingTile.Index, Biome, TileU0, TileU1,
				BackingTile.QuarterTurns, BackingTile.bMirrorU);
			AddQuad(Mesh, FVector(X0,Y0,SurfaceZ), FVector(X1,Y0,SurfaceZ), FVector(X1,Y1,SurfaceZ), FVector(X0,Y1,SurfaceZ),
				SurfaceNormal, SurfaceTangent, SurfaceTile.Index, Biome, TileU0, TileU1,
				SurfaceTile.QuarterTurns, SurfaceTile.bMirrorU);
		}
		if (SurfaceZ > FacadeBaseZ + KINDA_SMALL_NUMBER)
		{
			AddQuad(BackingMesh, FVector(X0,YMin+BackingInset,FacadeBaseZ), FVector(X1,YMin+BackingInset,FacadeBaseZ),
				FVector(X1,YMin+BackingInset,SurfaceZ), FVector(X0,YMin+BackingInset,SurfaceZ),
				-FVector::YAxisVector, FVector::XAxisVector, BackingTile.Index, Biome, TileU0, TileU1,
				BackingTile.QuarterTurns, BackingTile.bMirrorU);
			AddQuad(Mesh, FVector(X0,YMin,FacadeBaseZ), FVector(X1,YMin,FacadeBaseZ), FVector(X1,YMin,SurfaceZ), FVector(X0,YMin,SurfaceZ),
				-FVector::YAxisVector, FVector::XAxisVector, FacadeTile.Index, Biome, TileU0, TileU1,
				FacadeTile.QuarterTurns, FacadeTile.bMirrorU);
			AddQuad(BackingMesh, FVector(X1,YMax-BackingInset,FacadeBaseZ), FVector(X0,YMax-BackingInset,FacadeBaseZ),
				FVector(X0,YMax-BackingInset,SurfaceZ), FVector(X1,YMax-BackingInset,SurfaceZ),
				FVector::YAxisVector, -FVector::XAxisVector, BackingTile.Index, Biome, TileU0, TileU1,
				BackingTile.QuarterTurns, BackingTile.bMirrorU);
			AddQuad(Mesh, FVector(X1,YMax,FacadeBaseZ), FVector(X0,YMax,FacadeBaseZ), FVector(X0,YMax,SurfaceZ), FVector(X1,YMax,SurfaceZ),
				FVector::YAxisVector, -FVector::XAxisVector, FacadeTile.Index, Biome, TileU0, TileU1,
				FacadeTile.QuarterTurns, FacadeTile.bMirrorU);
		}

		if (RunEnd <= SteppedSurfaceHeights.Num())
		{
			const float NextZ = RunEnd < SteppedSurfaceHeights.Num()
				? SteppedSurfaceHeights[RunEnd] : RightGhostSurfaceHeight;
			if (!FMath::IsNearlyEqual(SurfaceZ, NextZ, 0.1f))
			{
				const bool bHighOnLeft = SurfaceZ > NextZ;
				const float LowZ = FMath::Min(SurfaceZ, NextZ);
				const float HighZ = FMath::Max(SurfaceZ, NextZ);
				const FVector Normal = bHighOnLeft ? FVector::XAxisVector : -FVector::XAxisVector;
				const FAtlasTileChoice SideTile = PickUniversalTerrainTile(Biome, X,
					bHighOnLeft ? ETerrainTileRole::Right : ETerrainTileRole::Left, Random);
				const FAtlasTileChoice SideBaseTile = PickUniversalTerrainTile(Biome, X,
					bHighOnLeft ? ETerrainTileRole::Right : ETerrainTileRole::Left, Random, 0, false);
				const float BackingX = X1 - Normal.X * BackingInset;
				for (int32 Y = 0; Y < 3; ++Y)
				{
					const float Y0 = YMin + Y * ActiveTileSize;
					const float Y1 = Y0 + ActiveTileSize;
					if (bHighOnLeft)
					{
						AddQuad(BackingMesh, FVector(BackingX,Y0,LowZ), FVector(BackingX,Y1,LowZ),
							FVector(BackingX,Y1,HighZ), FVector(BackingX,Y0,HighZ), Normal, FVector::YAxisVector,
							SideBaseTile.Index, Biome);
						AddQuad(Mesh, FVector(X1,Y0,LowZ), FVector(X1,Y1,LowZ), FVector(X1,Y1,HighZ), FVector(X1,Y0,HighZ),
							Normal, FVector::YAxisVector, SideTile.Index, Biome, 0.0f, 1.0f, SideTile.QuarterTurns, SideTile.bMirrorU);
					}
					else
					{
						AddQuad(BackingMesh, FVector(BackingX,Y1,LowZ), FVector(BackingX,Y0,LowZ),
							FVector(BackingX,Y0,HighZ), FVector(BackingX,Y1,HighZ), Normal, -FVector::YAxisVector,
							SideBaseTile.Index, Biome);
						AddQuad(Mesh, FVector(X1,Y1,LowZ), FVector(X1,Y0,LowZ), FVector(X1,Y0,HighZ), FVector(X1,Y1,HighZ),
							Normal, -FVector::YAxisVector, SideTile.Index, Biome, 0.0f, 1.0f, SideTile.QuarterTurns, SideTile.bMirrorU);
					}
				}
			}
		}
		Segment = RunEnd - 1;
	}
}

void APWG_Generator::BuildLayeredTerrainMesh(const FBiomeData& Biome, FRandomStream& Random)
{
	FMeshBuffers Mesh,Backing;
	const float T=ActiveTileSize;
	const float Inset=FMath::Max(0.5f,T*0.01f);
	TArray<EPWGTerrainLane> Lanes;
	if (bFrontLaneEnabled) Lanes.Add(EPWGTerrainLane::Front);
	Lanes.Add(EPWGTerrainLane::Middle);
	if (bBackLaneEnabled) Lanes.Add(EPWGTerrainLane::Back);
	const int32 SegmentCount=SteppedSurfaceHeights.Num();

	// Top ribbons, collision ribbons and X-facing cliffs for every playable lane.
	for(const EPWGTerrainLane Lane:Lanes)
	{
		const TArray<float>& Heights=GetLaneSteppedHeights(Lane);
		const float Y0=GetLaneCenterYLocal(Lane)-T*0.5f;
		const float Y1=Y0+T;
		for(int32 S=0;S<SegmentCount;++S)
		{
			const int32 X=S/ActiveSurfaceSegmentsPerTile;
			const float X0=ChunkStartLocalX+S*ActiveSurfaceSegmentWidth;
			const float X1=X0+ActiveSurfaceSegmentWidth;
			const float U0=static_cast<float>(S%ActiveSurfaceSegmentsPerTile)/ActiveSurfaceSegmentsPerTile;
			const float U1=static_cast<float>(S%ActiveSurfaceSegmentsPerTile+1)/ActiveSurfaceSegmentsPerTile;
			const FAtlasTileChoice Top=PickUniversalTerrainTile(Biome,X,ETerrainTileRole::Top,Random);
			const FAtlasTileChoice Base=PickUniversalTerrainTile(Biome,X,ETerrainTileRole::Interior,Random,2,false);
			AddQuad(Backing,FVector(X0,Y0,Heights[S]-Inset),FVector(X1,Y0,Heights[S]-Inset),
				FVector(X1,Y1,Heights[S]-Inset),FVector(X0,Y1,Heights[S]-Inset),FVector::UpVector,FVector::XAxisVector,
				Base.Index,Biome,U0,U1,Base.QuarterTurns,Base.bMirrorU);
			AddQuad(Mesh,FVector(X0,Y0,Heights[S]),FVector(X1,Y0,Heights[S]),FVector(X1,Y1,Heights[S]),
				FVector(X0,Y1,Heights[S]),FVector::UpVector,FVector::XAxisVector,Top.Index,Biome,U0,U1,
				Top.QuarterTurns,Top.bMirrorU);

			const float Next=S+1<SegmentCount?Heights[S+1]:Heights[S];
			if(FMath::Abs(Next-Heights[S])>=100.0f-KINDA_SMALL_NUMBER)
			{
				const float Low=FMath::Min(Heights[S],Next),High=FMath::Max(Heights[S],Next);
				const bool HighLeft=Heights[S]>Next;
				const FAtlasTileChoice Side=PickUniversalTerrainTile(Biome,X,
					HighLeft?ETerrainTileRole::Right:ETerrainTileRole::Left,Random);
				if(HighLeft)
				{
					AddQuad(Mesh,FVector(X1,Y0,Low),FVector(X1,Y1,Low),FVector(X1,Y1,High),FVector(X1,Y0,High),
						FVector::XAxisVector,FVector::YAxisVector,Side.Index,Biome,0,1,Side.QuarterTurns,Side.bMirrorU);
				}
				else
				{
					AddQuad(Mesh,FVector(X1,Y1,Low),FVector(X1,Y0,Low),FVector(X1,Y0,High),FVector(X1,Y1,High),
						-FVector::XAxisVector,-FVector::YAxisVector,Side.Index,Biome,0,1,Side.QuarterTurns,Side.bMirrorU);
				}
			}
		}
	}

	// Walls between adjacent lanes. They disappear naturally where connection masks make both heights equal.
	for(int32 Pair=0;Pair+1<Lanes.Num();++Pair)
	{
		const EPWGTerrainLane FrontLane=Lanes[Pair],RearLane=Lanes[Pair+1];
		const TArray<float>& A=GetLaneSteppedHeights(FrontLane);
		const TArray<float>& B=GetLaneSteppedHeights(RearLane);
		const float Y=(GetLaneCenterYLocal(FrontLane)+GetLaneCenterYLocal(RearLane))*0.5f;
		for(int32 S=0;S<SegmentCount;++S)
		{
			if(FMath::IsNearlyEqual(A[S],B[S],0.1f)) continue;
			const int32 X=S/ActiveSurfaceSegmentsPerTile;
			const float X0=ChunkStartLocalX+S*ActiveSurfaceSegmentWidth;
			const float X1=X0+ActiveSurfaceSegmentWidth;
			const float U0=static_cast<float>(S%ActiveSurfaceSegmentsPerTile)/ActiveSurfaceSegmentsPerTile;
			const float U1=static_cast<float>(S%ActiveSurfaceSegmentsPerTile+1)/ActiveSurfaceSegmentsPerTile;
			const float Low=FMath::Min(A[S],B[S]),High=FMath::Max(A[S],B[S]);
			const bool RearHigher=B[S]>A[S];
			const FVector Normal=RearHigher?-FVector::YAxisVector:FVector::YAxisVector;
			const FAtlasTileChoice Side=PickUniversalTerrainTile(Biome,X,ETerrainTileRole::SurfaceToGround,Random);
			const FAtlasTileChoice SideBase=PickUniversalTerrainTile(
				Biome,X,ETerrainTileRole::SurfaceToGround,Random,0,false);
			// A lane can differ by several metres while one surface segment is only
			// a fraction of a tile wide. Repeating metre-high slices and retaining
			// the segment's U range prevents one atlas cell being stretched over the
			// complete wall. The opaque base also fills alpha in blend cells.
			for(float SliceLow=Low;SliceLow<High-KINDA_SMALL_NUMBER;SliceLow+=T)
			{
				const float SliceHigh=FMath::Min(SliceLow+T,High);
				const float VSpan=(SliceHigh-SliceLow)/T;
				const float BackY=Y-Normal.Y*Inset;
				if(RearHigher)
				{
					AddQuad(Backing,FVector(X0,BackY,SliceLow),FVector(X1,BackY,SliceLow),
						FVector(X1,BackY,SliceHigh),FVector(X0,BackY,SliceHigh),Normal,FVector::XAxisVector,
						SideBase.Index,Biome,U0,U1,SideBase.QuarterTurns,SideBase.bMirrorU,0.0f,VSpan);
					AddQuad(Mesh,FVector(X0,Y,SliceLow),FVector(X1,Y,SliceLow),FVector(X1,Y,SliceHigh),
						FVector(X0,Y,SliceHigh),Normal,FVector::XAxisVector,Side.Index,Biome,U0,U1,
						Side.QuarterTurns,Side.bMirrorU,0.0f,VSpan);
				}
				else
				{
					AddQuad(Backing,FVector(X1,BackY,SliceLow),FVector(X0,BackY,SliceLow),
						FVector(X0,BackY,SliceHigh),FVector(X1,BackY,SliceHigh),Normal,-FVector::XAxisVector,
						SideBase.Index,Biome,U0,U1,SideBase.QuarterTurns,SideBase.bMirrorU,0.0f,VSpan);
					AddQuad(Mesh,FVector(X1,Y,SliceLow),FVector(X0,Y,SliceLow),FVector(X0,Y,SliceHigh),
						FVector(X1,Y,SliceHigh),Normal,-FVector::XAxisVector,Side.Index,Biome,U0,U1,
						Side.QuarterTurns,Side.bMirrorU,0.0f,VSpan);
				}
			}
			// Inter-lane facades remain visual only; no invisible collision wall.

		}
	}

	// Only the closest active lane needs the deep camera-facing facade.
	const EPWGTerrainLane VisibleLane=Lanes[0];
	const TArray<float>& Visible=GetLaneSteppedHeights(VisibleLane);
	const float FrontY=GetLaneCenterYLocal(VisibleLane)-T*0.5f;
	for(int32 X=0;X<SurfaceHeightTiles.Num();++X)
	{
		float MinTop=BIG_NUMBER;
		for(int32 Sub=0;Sub<ActiveSurfaceSegmentsPerTile;++Sub)
			MinTop=FMath::Min(MinTop,Visible[X*ActiveSurfaceSegmentsPerTile+Sub]);
		const int32 FullTop=FMath::FloorToInt(MinTop/T);
		for(int32 Z=FMath::RoundToInt(BottomLocalZ/T);Z<FullTop;++Z)
		{
			const int32 Depth=FMath::Max(1,FMath::CeilToInt(MinTop/T)-Z);
			const FAtlasTileChoice Tile=PickUniversalTerrainTile(Biome,X,ETerrainTileRole::Interior,Random,Depth);
			const FAtlasTileChoice Base=PickUniversalTerrainTile(Biome,X,ETerrainTileRole::Interior,Random,Depth,false);
			const float X0=ChunkStartLocalX+X*T,X1=X0+T,Z0=Z*T,Z1=Z0+T;
			AddQuad(Backing,FVector(X0,FrontY+Inset,Z0),FVector(X1,FrontY+Inset,Z0),FVector(X1,FrontY+Inset,Z1),
				FVector(X0,FrontY+Inset,Z1),-FVector::YAxisVector,FVector::XAxisVector,Base.Index,Biome);
			AddQuad(Mesh,FVector(X0,FrontY,Z0),FVector(X1,FrontY,Z0),FVector(X1,FrontY,Z1),FVector(X0,FrontY,Z1),
				-FVector::YAxisVector,FVector::XAxisVector,Tile.Index,Biome,0,1,Tile.QuarterTurns,Tile.bMirrorU);
		}
		const float BaseZ=FullTop*T;
		const FAtlasTileChoice Face=PickUniversalTerrainTile(Biome,X,ETerrainTileRole::SurfaceToGround,Random);
		const FAtlasTileChoice FaceBase=PickUniversalTerrainTile(
			Biome,X,ETerrainTileRole::SurfaceToGround,Random,0,false);
		for(int32 Sub=0;Sub<ActiveSurfaceSegmentsPerTile;++Sub)
		{
			const int32 S=X*ActiveSurfaceSegmentsPerTile+Sub;
			if(Visible[S]<=BaseZ+KINDA_SMALL_NUMBER) continue;
			const float X0=ChunkStartLocalX+S*ActiveSurfaceSegmentWidth,X1=X0+ActiveSurfaceSegmentWidth;
			const float U0=static_cast<float>(Sub)/ActiveSurfaceSegmentsPerTile,U1=static_cast<float>(Sub+1)/ActiveSurfaceSegmentsPerTile;
			for(float SliceLow=BaseZ;SliceLow<Visible[S]-KINDA_SMALL_NUMBER;SliceLow+=T)
			{
				const float SliceHigh=FMath::Min(SliceLow+T,Visible[S]);
				const float VSpan=(SliceHigh-SliceLow)/T;
				AddQuad(Backing,FVector(X0,FrontY+Inset,SliceLow),FVector(X1,FrontY+Inset,SliceLow),
					FVector(X1,FrontY+Inset,SliceHigh),FVector(X0,FrontY+Inset,SliceHigh),-FVector::YAxisVector,
					FVector::XAxisVector,FaceBase.Index,Biome,U0,U1,FaceBase.QuarterTurns,FaceBase.bMirrorU,0.0f,VSpan);
				AddQuad(Mesh,FVector(X0,FrontY,SliceLow),FVector(X1,FrontY,SliceLow),FVector(X1,FrontY,SliceHigh),
					FVector(X0,FrontY,SliceHigh),-FVector::YAxisVector,FVector::XAxisVector,Face.Index,Biome,U0,U1,
					Face.QuarterTurns,Face.bMirrorU,0.0f,VSpan);
			}
		}
	}

	TerrainMesh->CreateMeshSection(0,Mesh.Vertices,Mesh.Triangles,Mesh.Normals,Mesh.UVs,Mesh.Colors,Mesh.Tangents,false);
	UMaterialInterface* TerrainMaterial=Biome.AtlasMaterial.LoadSynchronous();
	if(TerrainMaterial&&Biome.bUseCompactBlockAtlas)
	{
		if(UTexture2D* Texture=Biome.CompactBlockAtlasTexture.LoadSynchronous())
		{
			UMaterialInstanceDynamic* MID=UMaterialInstanceDynamic::Create(TerrainMaterial,this);
			MID->SetTextureParameterValue(TEXT("TerrainAtlas"),Texture);
			MID->SetVectorParameterValue(TEXT("WorldLightTint"),FLinearColor::White);
			MID->SetScalarParameterValue(TEXT("WorldLightBrightness"),1.0f);
			MID->SetVectorParameterValue(TEXT("SurfaceLightColor"),Biome.TerrainSurfaceLightColor);
			MID->SetVectorParameterValue(TEXT("DeepLightColor"),Biome.TerrainDeepLightColor);
			MID->SetScalarParameterValue(TEXT("GradientStartDepth"),FMath::Max(0.0f,Biome.TerrainGradientStartDepth));
			MID->SetScalarParameterValue(TEXT("GradientFullDepth"),FMath::Max(Biome.TerrainGradientStartDepth+1.0f,Biome.TerrainGradientFullDepth));
			MID->SetScalarParameterValue(TEXT("VisualDepthRange"),FMath::Max(100.0f,Biome.TerrainVisualDepthRange));
			MID->SetScalarParameterValue(TEXT("BiomeLayerDepth"),FMath::Max(1,Biome.CompactBiomeDepthInTiles)*ActiveTileSize);
			MID->SetScalarParameterValue(TEXT("GroundLayerDepth"),0.5f*(FMath::Max(1,Biome.CompactGroundDepthMin)+FMath::Max(1,Biome.CompactGroundDepthMax))*ActiveTileSize);
			MID->SetScalarParameterValue(TEXT("RockLayerDepth"),FMath::Max(1,Biome.CompactRockDepthInTiles)*ActiveTileSize);
			MID->SetScalarParameterValue(TEXT("LayerNoisePixelSize"),FMath::Max(1.0f,Biome.TerrainLayerNoisePixelSize));
			MID->SetScalarParameterValue(TEXT("LayerNoiseStrength"),FMath::Max(0.0f,Biome.TerrainLayerNoiseStrength));
			MID->SetScalarParameterValue(TEXT("LayerBlendWidth"),FMath::Max(0.5f,Biome.TerrainLayerBlendWidth));
			MID->SetScalarParameterValue(TEXT("SurfaceDetailDepth"),FMath::Max(1.0f,Biome.TerrainSurfaceDetailDepth));
			TerrainMaterialInstance=MID;
			TerrainMaterial=MID;
		}
	}
	TerrainMesh->SetMaterial(0,TerrainMaterial);
	BuildSolidTerrainCollision();
	TerrainCollisionMesh->SetVisibility(false,true);
	TerrainCollisionMesh->SetHiddenInGame(true,true);
	if(!Backing.Vertices.IsEmpty())
	{
		DecorationMesh->CreateMeshSection(1,Backing.Vertices,Backing.Triangles,Backing.Normals,Backing.UVs,
			Backing.Colors,Backing.Tangents,false);
		DecorationMesh->SetMaterial(1,TerrainMaterial);
	}
}

void APWG_Generator::BuildTerrainMesh(const FBiomeData& Biome, FRandomStream& Random)
{
	if (bFrontLaneEnabled || bBackLaneEnabled)
	{
		BuildLayeredTerrainMesh(Biome,Random);
		return;
	}
	FMeshBuffers Mesh;
	FMeshBuffers BackingMesh;
	const int32 Count = SurfaceHeightTiles.Num();
	const float T = ActiveTileSize;
	const float YMin = -ChunkHalfDepth;
	const float YMax = ChunkHalfDepth;

	BottomTileIndex = FMath::RoundToInt(BottomLocalZ / T);
	int32 MaximumSurface = SurfaceHeightTiles[0];
	for (const int32 Height : SurfaceHeightTiles) MaximumSurface = FMath::Max(MaximumSurface, Height);
	SolidGridHeight = FMath::Max(1, MaximumSurface - BottomTileIndex);
	SolidCells.Init(0, Count * SolidGridHeight);
	CellStyleBases.Init(INDEX_NONE, Count * SolidGridHeight);

	for (int32 X = 0; X < Count; ++X)
	{
		for (int32 Z = BottomTileIndex; Z < SurfaceHeightTiles[X]; ++Z)
		{
			bool bSolid = true;
			const int32 Depth = SurfaceHeightTiles[X] - Z;
			if (Biome.bGenerateCaves && Biome.bEnableExperimentalVoxelCarving
				&& Depth > FMath::Max(1, Biome.CaveMinimumDepthBelowSurface)
				&& Z > BottomTileIndex + 1 && X > 1 && X < Count - 2)
			{
				const float Seed = static_cast<float>(Random.GetInitialSeed());
				const FVector2D P((GlobalColumnOffset + X + Seed * 0.013f) * Biome.CaveNoiseFrequency,
					(Z - Seed * 0.007f) * Biome.CaveNoiseFrequency);
				const float CaveNoise = (FMath::PerlinNoise2D(P) + 0.45f * FMath::PerlinNoise2D(P * 2.11f + FVector2D(17.0f, 31.0f))) / 1.45f;
				bSolid = CaveNoise <= Biome.CaveNoiseThreshold;
			}
			const int32 GridIndex = X * SolidGridHeight + (Z - BottomTileIndex);
			SolidCells[GridIndex] = bSolid ? 1 : 0;
			CellStyleBases[GridIndex] = ColumnMiniBiomes.IsValidIndex(X) ? ColumnMiniBiomes[X] : 1;
		}
	}

	for (int32 X = 0; X < Count; ++X)
	{
		const float X0 = ChunkStartLocalX + X * T;
		const float X1 = X0 + T;
		for (int32 Z = BottomTileIndex; Z < SurfaceHeightTiles[X]; ++Z)
		{
			const float Z0 = Z * T;
			const float Z1 = Z0 + T;
			const int32 GridIndex = X * SolidGridHeight + (Z - BottomTileIndex);
			if (!IsSolidCell(X, Z))
			{
				if (Biome.bRenderCaveBackWalls || Biome.bGenerateCaves)
				{
					const int32 Depth = SurfaceHeightTiles[X] - Z;
					const FAtlasTileChoice CaveTile = PickUniversalTerrainTile(
						Biome, X, ETerrainTileRole::Interior, Random, Depth);
					// Keep the back wall one block behind the visible facade. At the old rear edge
					// an angled camera could look past the same X/Z cell and expose the panorama.
					const float BackWallY = FMath::Min(YMax - 1.0f,
						YMin + T + FMath::Clamp(Biome.CaveBackWallInset, 0.0f, T * 0.5f));
					AddQuad(BackingMesh, FVector(X0,BackWallY,Z0), FVector(X1,BackWallY,Z0),
						FVector(X1,BackWallY,Z1), FVector(X0,BackWallY,Z1),
						-FVector::YAxisVector, FVector::XAxisVector, CaveTile.Index, Biome,
						0.0f, 1.0f, CaveTile.QuarterTurns, CaveTile.bMirrorU);
				}
				continue;
			}
			const bool bLeft = IsSolidCell(X - 1, Z);
			const bool bRight = IsSolidCell(X + 1, Z);
			const bool bUp = IsSolidCell(X, Z + 1);
			const bool bDown = IsSolidCell(X, Z - 1);

			ETerrainTileRole FacadeRole = ETerrainTileRole::Interior;
			if (!bUp && !bLeft) FacadeRole = ETerrainTileRole::TopLeft;
			else if (!bUp && !bRight) FacadeRole = ETerrainTileRole::TopRight;
			else if (!bDown && !bLeft) FacadeRole = ETerrainTileRole::BottomLeft;
			else if (!bDown && !bRight) FacadeRole = ETerrainTileRole::BottomRight;
			else if (!bUp) FacadeRole = ETerrainTileRole::Top;
			else if (!bDown) FacadeRole = ETerrainTileRole::Bottom;
			else if (!bLeft) FacadeRole = ETerrainTileRole::Left;
			else if (!bRight) FacadeRole = ETerrainTileRole::Right;

			// The exposed upper surface is emitted later as one continuous,
			// subdivided shell. Voxel tops would reintroduce stair collision.
			if (!bDown)
			{
				const FAtlasTileChoice Tile = PickUniversalTerrainTile(Biome, X, ETerrainTileRole::Bottom, Random);
				for (int32 Y = 0; Y < 3; ++Y)
				{
					const float Y0 = YMin + Y * T;
					const float Y1 = Y0 + T;
					AddQuad(Mesh, FVector(X0,Y1,Z0), FVector(X1,Y1,Z0), FVector(X1,Y0,Z0), FVector(X0,Y0,Z0),
						FVector::DownVector, FVector::XAxisVector, Tile.Index, Biome, 0.0f, 1.0f,
						Tile.QuarterTurns, Tile.bMirrorU);
				}
			}
			const bool bLeftIsSurfaceGap = X > 0 && Z >= SurfaceHeightTiles[X - 1];
			if (!bLeft && !bLeftIsSurfaceGap && !(bStreamingChild && X == 0))
			{
				const FAtlasTileChoice Tile = PickUniversalTerrainTile(Biome, X, ETerrainTileRole::Left, Random);
				const float SideTopZ = (!bUp && X == 0) ? GetSmoothSurfaceHeightAtLocalX(X0) : Z1;
				for (int32 Y = 0; Y < 3; ++Y)
				{
					const float Y0 = YMin + Y * T;
					const float Y1 = Y0 + T;
					AddQuad(Mesh, FVector(X0,Y1,Z0), FVector(X0,Y0,Z0), FVector(X0,Y0,SideTopZ), FVector(X0,Y1,SideTopZ),
						-FVector::XAxisVector, -FVector::YAxisVector, Tile.Index, Biome, 0.0f, 1.0f,
						Tile.QuarterTurns, Tile.bMirrorU);
				}
			}
			const bool bRightIsSurfaceGap = X + 1 < Count && Z >= SurfaceHeightTiles[X + 1];
			if (!bRight && !bRightIsSurfaceGap && !(bStreamingChild && X == Count - 1))
			{
				const FAtlasTileChoice Tile = PickUniversalTerrainTile(Biome, X, ETerrainTileRole::Right, Random);
				const float SideTopZ = (!bUp && X == Count - 1) ? GetSmoothSurfaceHeightAtLocalX(X1) : Z1;
				for (int32 Y = 0; Y < 3; ++Y)
				{
					const float Y0 = YMin + Y * T;
					const float Y1 = Y0 + T;
					AddQuad(Mesh, FVector(X1,Y0,Z0), FVector(X1,Y1,Z0), FVector(X1,Y1,SideTopZ), FVector(X1,Y0,SideTopZ),
						FVector::XAxisVector, FVector::YAxisVector, Tile.Index, Biome, 0.0f, 1.0f,
						Tile.QuarterTurns, Tile.bMirrorU);
				}
			}

			float MinimumSurfaceInColumn = BIG_NUMBER;
			for (int32 Sub = 0; Sub < ActiveSurfaceSegmentsPerTile; ++Sub)
			{
				const int32 SegmentIndex = X * ActiveSurfaceSegmentsPerTile + Sub;
				if (SteppedSurfaceHeights.IsValidIndex(SegmentIndex))
				{
					MinimumSurfaceInColumn = FMath::Min(MinimumSurfaceInColumn, SteppedSurfaceHeights[SegmentIndex]);
				}
			}
			const int32 SurfaceShellBaseTile = FMath::FloorToInt(MinimumSurfaceInColumn / T);
			if (Z < SurfaceShellBaseTile)
			{
				const ETerrainTileRole VisualRole = FacadeRole == ETerrainTileRole::Top
					? ETerrainTileRole::SurfaceToGround : FacadeRole;
				const int32 Depth = SurfaceHeightTiles[X] - Z;
				const FAtlasTileChoice FacadeTile = PickUniversalTerrainTile(Biome, X, VisualRole, Random, Depth);
				const FAtlasTileChoice BackingTile = PickUniversalTerrainTile(
					Biome, X, ETerrainTileRole::Interior, Random, FMath::Max(2, Depth), false);
				const float BackingInset = FMath::Max(0.5f, T * 0.01f);
				AddQuad(BackingMesh, FVector(X0,YMin+BackingInset,Z0), FVector(X1,YMin+BackingInset,Z0),
					FVector(X1,YMin+BackingInset,Z1), FVector(X0,YMin+BackingInset,Z1),
					-FVector::YAxisVector, FVector::XAxisVector, BackingTile.Index, Biome, 0.0f, 1.0f,
					BackingTile.QuarterTurns, BackingTile.bMirrorU);
				AddQuad(Mesh, FVector(X0,YMin,Z0), FVector(X1,YMin,Z0), FVector(X1,YMin,Z1), FVector(X0,YMin,Z1),
					-FVector::YAxisVector, FVector::XAxisVector, FacadeTile.Index, Biome, 0.0f, 1.0f,
					FacadeTile.QuarterTurns, FacadeTile.bMirrorU);
				AddQuad(BackingMesh, FVector(X1,YMax-BackingInset,Z0), FVector(X0,YMax-BackingInset,Z0),
					FVector(X0,YMax-BackingInset,Z1), FVector(X1,YMax-BackingInset,Z1),
					FVector::YAxisVector, -FVector::XAxisVector, BackingTile.Index, Biome, 0.0f, 1.0f,
					BackingTile.QuarterTurns, BackingTile.bMirrorU);
				AddQuad(Mesh, FVector(X1,YMax,Z0), FVector(X0,YMax,Z0), FVector(X0,YMax,Z1), FVector(X1,YMax,Z1),
					FVector::YAxisVector, -FVector::XAxisVector, FacadeTile.Index, Biome, 0.0f, 1.0f,
					FacadeTile.QuarterTurns, FacadeTile.bMirrorU);
			}
		}
	}
	BuildSmoothSurfaceShell(Mesh, BackingMesh, Biome, Random);

	TerrainMesh->CreateMeshSection(0, Mesh.Vertices, Mesh.Triangles, Mesh.Normals, Mesh.UVs, Mesh.Colors, Mesh.Tangents, false);
	UMaterialInterface* TerrainMaterial = Biome.AtlasMaterial.LoadSynchronous();
	if (TerrainMaterial && Biome.bUseCompactBlockAtlas)
	{
		if (UTexture2D* CompactTexture = Biome.CompactBlockAtlasTexture.LoadSynchronous())
		{
			UMaterialInstanceDynamic* CompactMID = UMaterialInstanceDynamic::Create(TerrainMaterial, this);
			CompactMID->SetTextureParameterValue(TEXT("TerrainAtlas"), CompactTexture);
			CompactMID->SetVectorParameterValue(TEXT("WorldLightTint"),FLinearColor::White);
			CompactMID->SetScalarParameterValue(TEXT("WorldLightBrightness"),1.0f);
			CompactMID->SetVectorParameterValue(TEXT("SurfaceLightColor"),Biome.TerrainSurfaceLightColor);
			CompactMID->SetVectorParameterValue(TEXT("DeepLightColor"),Biome.TerrainDeepLightColor);
			CompactMID->SetScalarParameterValue(TEXT("GradientStartDepth"),FMath::Max(0.0f,Biome.TerrainGradientStartDepth));
			CompactMID->SetScalarParameterValue(TEXT("GradientFullDepth"),FMath::Max(Biome.TerrainGradientStartDepth+1.0f,Biome.TerrainGradientFullDepth));
			CompactMID->SetScalarParameterValue(TEXT("VisualDepthRange"),FMath::Max(100.0f,Biome.TerrainVisualDepthRange));
			CompactMID->SetScalarParameterValue(TEXT("BiomeLayerDepth"),FMath::Max(1,Biome.CompactBiomeDepthInTiles)*ActiveTileSize);
			CompactMID->SetScalarParameterValue(TEXT("GroundLayerDepth"),0.5f*(FMath::Max(1,Biome.CompactGroundDepthMin)+FMath::Max(1,Biome.CompactGroundDepthMax))*ActiveTileSize);
			CompactMID->SetScalarParameterValue(TEXT("RockLayerDepth"),FMath::Max(1,Biome.CompactRockDepthInTiles)*ActiveTileSize);
			CompactMID->SetScalarParameterValue(TEXT("LayerNoisePixelSize"),FMath::Max(1.0f,Biome.TerrainLayerNoisePixelSize));
			CompactMID->SetScalarParameterValue(TEXT("LayerNoiseStrength"),FMath::Max(0.0f,Biome.TerrainLayerNoiseStrength));
			CompactMID->SetScalarParameterValue(TEXT("LayerBlendWidth"),FMath::Max(0.5f,Biome.TerrainLayerBlendWidth));
			CompactMID->SetScalarParameterValue(TEXT("SurfaceDetailDepth"),FMath::Max(1.0f,Biome.TerrainSurfaceDetailDepth));
			TerrainMaterialInstance=CompactMID;
			TerrainMaterial = CompactMID;
		}
	}
	TerrainMesh->SetMaterial(0, TerrainMaterial);

	BuildSolidTerrainCollision();
	TerrainCollisionMesh->SetVisibility(false, true);
	TerrainCollisionMesh->SetHiddenInGame(true, true);
	if (!BackingMesh.Vertices.IsEmpty())
	{
		DecorationMesh->CreateMeshSection(1, BackingMesh.Vertices, BackingMesh.Triangles,
			BackingMesh.Normals, BackingMesh.UVs, BackingMesh.Colors, BackingMesh.Tangents, false);
		DecorationMesh->SetMaterial(1, TerrainMaterial);
	}
}

void APWG_Generator::BuildDecorations(const FBiomeData& Biome, FRandomStream& Random)
{
	if (Biome.DecorationTileIndices.IsEmpty() || Biome.DecorationChancePerTile <= 0.0f)
	{
		return;
	}

	FMeshBuffers Mesh;
	const float T = ActiveTileSize;
	const float Y = Biome.DecorationDepthOffsetInTiles * T;
	for (int32 X = 0; X < SurfaceHeightTiles.Num(); ++X)
	{
		if (Random.FRand() > Biome.DecorationChancePerTile) continue;
		const float CenterX = ChunkStartLocalX + (X + 0.5f) * T;
		const float HalfWidth = Biome.DecorationWidthInTiles * T * 0.5f;
		const float Z0 = GetLocalSurfaceHeightByIndex(X) + 1.0f;
		const float Z1 = Z0 + Biome.DecorationHeightInTiles * T;
		AddQuad(Mesh, FVector(CenterX-HalfWidth,Y,Z0), FVector(CenterX+HalfWidth,Y,Z0),
			FVector(CenterX+HalfWidth,Y,Z1), FVector(CenterX-HalfWidth,Y,Z1),
			-FVector::YAxisVector, FVector::XAxisVector, PickTile(Biome.DecorationTileIndices, Random), Biome);
	}
	if (!Mesh.Vertices.IsEmpty())
	{
		DecorationMesh->CreateMeshSection(0, Mesh.Vertices, Mesh.Triangles, Mesh.Normals, Mesh.UVs, Mesh.Colors, Mesh.Tangents, false);
		UMaterialInterface* DecorationMaterial = Biome.AtlasMaterial.LoadSynchronous();
		if (DecorationMaterial && Biome.bUseCompactBlockAtlas)
		{
			if (UTexture2D* CompactTexture = Biome.CompactBlockAtlasTexture.LoadSynchronous())
			{
				UMaterialInstanceDynamic* CompactMID = UMaterialInstanceDynamic::Create(DecorationMaterial, this);
				CompactMID->SetTextureParameterValue(TEXT("TerrainAtlas"), CompactTexture);
				DecorationMaterial = CompactMID;
			}
		}
		DecorationMesh->SetMaterial(0, DecorationMaterial);
	}
}

UStaticMesh* APWG_Generator::GetOrCreateGrassCardMesh()
{
	if (GrassCardMesh) return GrassCardMesh;
	static TWeakObjectPtr<UStaticMesh> SharedGrassCard;
	if (SharedGrassCard.IsValid())
	{
		GrassCardMesh = SharedGrassCard.Get();
		return GrassCardMesh;
	}

	FMeshDescription Description;
	FStaticMeshAttributes Attributes(Description);
	Attributes.Register();
	FMeshDescriptionBuilder Builder;
	Builder.SetMeshDescription(&Description);
	Builder.SetNumUVLayers(1);
	const FPolygonGroupID Group = Builder.AppendPolygonGroup(TEXT("Grass"));
	constexpr int32 VerticalSegments = 4;
	FVertexID Vertices[VerticalSegments + 1][2];
	for (int32 Row = 0; Row <= VerticalSegments; ++Row)
	{
		const float Alpha = static_cast<float>(Row) / VerticalSegments;
		Vertices[Row][0] = Builder.AppendVertex(FVector(-50.0f, 0.0f, Alpha * 100.0f));
		Vertices[Row][1] = Builder.AppendVertex(FVector( 50.0f, 0.0f, Alpha * 100.0f));
	}
	auto MakeInstance = [&](const FVertexID Vertex, const FVector2D UV)
	{
		const FVertexInstanceID Instance = Builder.AppendInstance(Vertex);
		Builder.SetInstanceUV(Instance, UV);
		Builder.SetInstanceTangentSpace(Instance, -FVector::YAxisVector, FVector::XAxisVector, 1.0f);
		Builder.SetInstanceColor(Instance, FVector4f(1.0f));
		return Instance;
	};
	for (int32 Row = 0; Row < VerticalSegments; ++Row)
	{
		const float V0 = 1.0f - static_cast<float>(Row) / VerticalSegments;
		const float V1 = 1.0f - static_cast<float>(Row + 1) / VerticalSegments;
		const FVertexInstanceID LB = MakeInstance(Vertices[Row][0], FVector2D(0.0f, V0));
		const FVertexInstanceID RB = MakeInstance(Vertices[Row][1], FVector2D(1.0f, V0));
		const FVertexInstanceID RT = MakeInstance(Vertices[Row + 1][1], FVector2D(1.0f, V1));
		Builder.AppendTriangle(LB, RB, RT, Group);
		const FVertexInstanceID LB2 = MakeInstance(Vertices[Row][0], FVector2D(0.0f, V0));
		const FVertexInstanceID RT2 = MakeInstance(Vertices[Row + 1][1], FVector2D(1.0f, V1));
		const FVertexInstanceID LT = MakeInstance(Vertices[Row + 1][0], FVector2D(0.0f, V1));
		Builder.AppendTriangle(LB2, RT2, LT, Group);
	}

	UStaticMesh* Mesh = NewObject<UStaticMesh>(GetTransientPackage(), TEXT("PWG_RuntimeGrassCard"), RF_Transient);
	Mesh->GetStaticMaterials().Add(FStaticMaterial(nullptr, TEXT("Grass")));
	UStaticMesh::FBuildMeshDescriptionsParams Params;
	Params.bMarkPackageDirty = false;
	Params.bBuildSimpleCollision = false;
	Params.bFastBuild = true;
	Params.bAllowCpuAccess = false;
	TArray<const FMeshDescription*> Descriptions;
	Descriptions.Add(&Description);
	if (!Mesh->BuildFromMeshDescriptions(Descriptions, Params)) return nullptr;
	SharedGrassCard = Mesh;
	GrassCardMesh = Mesh;
	return GrassCardMesh;
}

void APWG_Generator::BuildGrass(const FBiomeData& Biome, FRandomStream& Random)
{
	for (UHierarchicalInstancedStaticMeshComponent* Component : GrassInstanceComponents)
	{
		if (IsValid(Component)) Component->DestroyComponent();
	}
	GrassInstanceComponents.Reset();
	GrassMaterials.Reset();
	if (!Biome.Grass.bEnabled || Biome.Grass.InstancesPerTile <= 0.0f) return;
	UStaticMesh* CardMesh = GetOrCreateGrassCardMesh();
	UTexture2D* MasterAtlas = Biome.bUseBiomeMasterAtlas ? Biome.BiomeMasterAtlas.LoadSynchronous() : nullptr;
	const bool bUsingMasterAtlas = MasterAtlas != nullptr;
	UTexture2D* FallbackAtlas = Biome.Grass.FallbackAtlas.LoadSynchronous();
	if (!FallbackAtlas)
	{
		FallbackAtlas = LoadObject<UTexture2D>(nullptr,
			TEXT("/Game/Systems/Sprite2Bone/Biomes/Meadow/Meadow_Grass/Grass_Meadows.Grass_Meadows"));
	}
	if (!CardMesh || (!MasterAtlas && !FallbackAtlas))
	{
		UE_LOG(LogPWG, Warning, TEXT("Grass disabled for %s: runtime card or fallback atlas is unavailable."),
			*ActiveBiomeRow.ToString());
		return;
	}

	struct FGrassBatch
	{
		int32 Layer = 0;
		TObjectPtr<UTexture2D> Atlas;
		TArray<FTransform> Transforms;
		TArray<FVector4f> CustomData;
	};
	TArray<FGrassBatch> Batches;
	auto IsMiniEnabled = [&](const int32 Mini)
	{
		switch (Mini)
		{
		case 0: return Biome.Grass.bGenerateOnMiniBiome1;
		case 1: return Biome.Grass.bGenerateOnMiniBiome2;
		case 2: return Biome.Grass.bGenerateOnMiniBiome3;
		case 3: return Biome.Grass.bGenerateOnMiniBiome4;
		case 4: return Biome.Grass.bGenerateOnRockSurface;
		case 5: return Biome.Grass.bGenerateOnGroundSurface;
		case 6: return Biome.Grass.bGenerateOnMiniBiome5;
		case 7: return Biome.Grass.bGenerateOnMiniBiome6;
		default: return false;
		}
	};
	auto AtlasForMini = [&](const int32 Mini)
	{
		if (MasterAtlas) return MasterAtlas;
		UTexture2D* Atlas = Biome.Grass.MiniBiomeAtlases.IsValidIndex(Mini)
			? Biome.Grass.MiniBiomeAtlases[Mini].LoadSynchronous() : nullptr;
		return Atlas ? Atlas : FallbackAtlas;
	};
	auto FindBatch = [&](const int32 Layer, UTexture2D* Atlas) -> FGrassBatch&
	{
		for (FGrassBatch& Batch : Batches)
		{
			if (Batch.Layer == Layer && Batch.Atlas == Atlas) return Batch;
		}
		FGrassBatch& NewBatch = Batches.AddDefaulted_GetRef();
		NewBatch.Layer = Layer;
		NewBatch.Atlas = Atlas;
		return NewBatch;
	};

	const FPWGGrassDepthLayer* GrassDepthLayers[] = {
		&Biome.Grass.BehindLayer, &Biome.Grass.PlayerLayer, &Biome.Grass.FrontLayer
	};
	const EPWGTerrainLane GrassLanes[] = {
		EPWGTerrainLane::Back, EPWGTerrainLane::Middle, EPWGTerrainLane::Front
	};
	const int32 FrameCount = FMath::Max(1, Biome.Grass.AtlasColumns * Biome.Grass.AtlasRows);
	for (int32 X = 0; X < SurfaceHeightTiles.Num(); ++X)
	{
		const int32 Mini = ColumnMiniBiomes.IsValidIndex(X) ? ColumnMiniBiomes[X] : 5;
		if (!IsMiniEnabled(Mini)) continue;
		UTexture2D* Atlas = AtlasForMini(Mini);
		for (int32 LayerIndex = 0; LayerIndex < UE_ARRAY_COUNT(GrassDepthLayers); ++LayerIndex)
		{
			const FPWGGrassDepthLayer& Layer = *GrassDepthLayers[LayerIndex];
			const EPWGTerrainLane TerrainLane=GrassLanes[LayerIndex];
			if (!IsLaneEnabledLocal(TerrainLane)||!Layer.bEnabled||Layer.DensityMultiplier<=0.0f) continue;
			const float LaneDensity=TerrainLane==EPWGTerrainLane::Front?Biome.FrontLane.VegetationDensityMultiplier
				:(TerrainLane==EPWGTerrainLane::Back?Biome.BackLane.VegetationDensityMultiplier:1.0f);
			const float DesiredCount = Biome.Grass.InstancesPerTile * Layer.DensityMultiplier * LaneDensity;
			int32 Count = FMath::FloorToInt(DesiredCount);
			if (Random.FRand() < DesiredCount - Count) ++Count;
			FGrassBatch& Batch = FindBatch(LayerIndex, Atlas);
			for (int32 Instance = 0; Instance < Count; ++Instance)
			{
				const float LocalX = ChunkStartLocalX + (X + Random.FRand()) * ActiveTileSize;
				const float LocalY = Random.FRandRange(FMath::Min(Layer.DepthRange.X, Layer.DepthRange.Y),
					FMath::Max(Layer.DepthRange.X, Layer.DepthRange.Y));
				const float LocalZ = GetLaneSurfaceHeightAtLocalX(LocalX,TerrainLane) + Biome.Grass.SurfaceOffset;
				const float Width = Random.FRandRange(FMath::Min(Biome.Grass.WidthRange.X, Biome.Grass.WidthRange.Y),
					FMath::Max(Biome.Grass.WidthRange.X, Biome.Grass.WidthRange.Y));
				const float Height = Random.FRandRange(FMath::Min(Biome.Grass.HeightRange.X, Biome.Grass.HeightRange.Y),
					FMath::Max(Biome.Grass.HeightRange.X, Biome.Grass.HeightRange.Y));
				Batch.Transforms.Emplace(FRotator::ZeroRotator, FVector(LocalX, LocalY, LocalZ),
					FVector(Width / 100.0f, 1.0f, Height / 100.0f));
				const int32 Frame = bUsingMasterAtlas
					? MasterGrassFrame(Mini, Random.RandRange(0, 7))
					: Random.RandRange(0, FrameCount - 1);
				Batch.CustomData.Emplace(static_cast<float>(Frame),
					Random.FRand(), Random.FRandRange(0.8f, 1.2f), Random.FRandRange(0.92f, 1.08f));
			}
		}
	}

	UMaterialInterface* GrassParent = Biome.Grass.Material.LoadSynchronous();
	for (int32 BatchIndex = 0; BatchIndex < Batches.Num(); ++BatchIndex)
	{
		FGrassBatch& Batch = Batches[BatchIndex];
		if (Batch.Transforms.IsEmpty()) continue;
		const FName Name = MakeUniqueObjectName(this, UHierarchicalInstancedStaticMeshComponent::StaticClass(),
			*FString::Printf(TEXT("Grass_L%d_B%d"), Batch.Layer, BatchIndex));
		UHierarchicalInstancedStaticMeshComponent* HISM = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, Name);
		HISM->SetupAttachment(RootComponent);
		HISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		EnableVegetationFillChannel(HISM);
		HISM->SetCanEverAffectNavigation(false);
		HISM->SetCastShadow(Biome.Grass.bCastShadows);
		HISM->SetReceivesDecals(false);
		HISM->bEnableDensityScaling = true;
		HISM->SetVisibleInRayTracing(false);
		HISM->SetEvaluateWorldPositionOffsetInRayTracing(false);
		HISM->SetAffectDistanceFieldLighting(false);
		HISM->SetAffectDynamicIndirectLighting(false);
		HISM->SetCullDistances(FMath::Max(0, Biome.Grass.StartCullDistance),
			FMath::Max(Biome.Grass.StartCullDistance + 1, Biome.Grass.EndCullDistance));
		HISM->SetNumCustomDataFloats(4);
		HISM->SetStaticMesh(CardMesh);
		HISM->RegisterComponent();
		HISM->AddInstances(Batch.Transforms, false, false, false);
		for (int32 InstanceIndex = 0; InstanceIndex < Batch.CustomData.Num(); ++InstanceIndex)
		{
			const FVector4f& Data = Batch.CustomData[InstanceIndex];
			HISM->SetCustomDataValue(InstanceIndex, 0, Data.X, false);
			HISM->SetCustomDataValue(InstanceIndex, 1, Data.Y, false);
			HISM->SetCustomDataValue(InstanceIndex, 2, Data.Z, false);
			HISM->SetCustomDataValue(InstanceIndex, 3, Data.W, false);
		}
		HISM->BuildTreeIfOutdated(true, true);
		UMaterialInstanceDynamic* MID = GrassParent ? UMaterialInstanceDynamic::Create(GrassParent, this) : nullptr;
		if (MID)
		{
			MID->SetTextureParameterValue(TEXT("GrassAtlas"), Batch.Atlas);
			MID->SetScalarParameterValue(TEXT("AtlasColumns"), bUsingMasterAtlas
				? MasterSquareCellColumns : FMath::Max(1, Biome.Grass.AtlasColumns));
			MID->SetScalarParameterValue(TEXT("AtlasRows"), bUsingMasterAtlas
				? MasterSquareCellRows : FMath::Max(1, Biome.Grass.AtlasRows));
			MID->SetVectorParameterValue(TEXT("GrassTint"), Biome.Grass.Tint);
			MID->SetScalarParameterValue(TEXT("GrassBrightness"), Biome.Grass.Brightness);
			MID->SetScalarParameterValue(TEXT("PixelWorldStep"), Biome.Grass.PixelWorldStep);
			HISM->SetMaterial(0, MID);
			GrassMaterials.Add(MID);
		}
		else if (BatchIndex == 0)
		{
			UE_LOG(LogPWG, Warning, TEXT("Grass instances were generated for %s, but Grass.Material is not assigned yet."),
				*ActiveBiomeRow.ToString());
		}
		GrassInstanceComponents.Add(HISM);
	}
	UpdateGrassMaterialParameters();
}

void APWG_Generator::BuildBushes(const FBiomeData& Biome, FRandomStream& Random)
{
	ClearPlaceholderBerries();
	FRandomStream BerryRandom(HashCombineFast(GetTypeHash(ActiveWorldSeed),GetTypeHash(StreamingChunkIndex)) ^ 0xBE771E5u);
	for (UHierarchicalInstancedStaticMeshComponent* Component : BushInstanceComponents)
	{
		if (IsValid(Component)) Component->DestroyComponent();
	}
	BushInstanceComponents.Reset();
	if (!bEnableSurfaceBushes || BushesPer100Metres <= 0.0f) return;

	UStaticMesh* CardMesh = GetOrCreateGrassCardMesh();
	UTexture2D* MasterAtlas = Biome.bUseBiomeMasterAtlas ? Biome.BiomeMasterAtlas.LoadSynchronous() : nullptr;
	const bool bUsingMasterAtlas = MasterAtlas != nullptr;
	UTexture2D* Atlas = MasterAtlas ? MasterAtlas : BushAtlas.LoadSynchronous();
	UMaterialInterface* ParentMaterial = BushMaterial.LoadSynchronous();
	if (!ParentMaterial) ParentMaterial = Biome.Grass.Material.LoadSynchronous();
	if (!CardMesh || !Atlas || !ParentMaterial)
	{
		UE_LOG(LogPWG, Warning, TEXT("PWG bushes disabled for %s: card, atlas or material is unavailable."),
			*ActiveBiomeRow.ToString());
		return;
	}

	TArray<FTransform> Transforms;
	TArray<FVector4f> CustomData;
	const float DesiredPerTile = FMath::Max(0.0f, BushesPer100Metres) * ActiveTileSize / 10000.0f;
	const int32 FrameCount = FMath::Max(1, BushAtlasColumns * BushAtlasRows);
	const EPWGTerrainLane BushLanes[]={EPWGTerrainLane::Front,EPWGTerrainLane::Middle,EPWGTerrainLane::Back};
	for (int32 Tile = 0; Tile < SurfaceHeightTiles.Num(); ++Tile)
	{
		const int32 Mini = ColumnMiniBiomes.IsValidIndex(Tile) ? ColumnMiniBiomes[Tile] : 5;
		if (Mini < 0 || Mini >= SurfaceStyleCount) continue;
		// Old bush sheets contain no Rock/Ground/MB5/MB6 sections.
		if (!bUsingMasterAtlas && Mini > 3) continue;
		for(const EPWGTerrainLane TerrainLane:BushLanes)
		{
			if(!IsLaneEnabledLocal(TerrainLane)) continue;
			const float LaneDensity=TerrainLane==EPWGTerrainLane::Front?Biome.FrontLane.VegetationDensityMultiplier
				:(TerrainLane==EPWGTerrainLane::Back?Biome.BackLane.VegetationDensityMultiplier:1.0f);
			const float LaneDesired=DesiredPerTile*LaneDensity;
			int32 Count=FMath::FloorToInt(LaneDesired);
			if(Random.FRand()<LaneDesired-Count) ++Count;
			for(int32 Instance=0;Instance<Count;++Instance)
			{
				const float LocalX=ChunkStartLocalX+(Tile+Random.FRand())*ActiveTileSize;
				const float LocalY=GetLaneCenterYLocal(TerrainLane)+Random.FRandRange(-ActiveTileSize*0.38f,ActiveTileSize*0.38f);
				const float Scale=Random.FRandRange(FMath::Min(BushScaleRange.X,BushScaleRange.Y),FMath::Max(BushScaleRange.X,BushScaleRange.Y));
				const float Height=FMath::Max(1.0f,BushBaseSize)*FMath::Max(0.01f,Scale);
				const float Width=Height*2.0f;
				const float LocalZ=GetLaneSurfaceHeightAtLocalX(LocalX,TerrainLane)-FMath::Max(0.0f,BushRootBurial);
				Transforms.Emplace(FRotator::ZeroRotator,FVector(LocalX,LocalY,LocalZ),FVector(Width/100.0f,1.0f,Height/100.0f));
				const int32 Frame = bUsingMasterAtlas
					? MasterWideFrame(4, Mini, Random.RandRange(0, 3))
					: Random.RandRange(0, FrameCount - 1);
				const bool bFruitBush=bEnablePlaceholderBerries && BerryRandom.FRand()<BerryBushChance;
				CustomData.Emplace(static_cast<float>(Frame),Random.FRand(),
					FMath::Max(0.0f,BushWindInfluence)*Random.FRandRange(0.90f,1.10f),Random.FRandRange(0.94f,1.06f));
				// Fruit cards stay attached visually; only non-fruiting bushes sway in this prototype.
				if (bFruitBush) CustomData.Last().Z=0.0f;
				if (bFruitBush)
				{
					for (int32 Fruit=0;Fruit<FMath::Clamp(BerriesPerBush,1,8);++Fruit)
					{
						const FString Id=FString::Printf(TEXT("%d:%d:%d:%d:%d:%d"),ActiveWorldSeed,StreamingChunkIndex,Tile,static_cast<int32>(TerrainLane),Instance,Fruit);
						const float OffsetX=Width*((Fruit+0.5f)/FMath::Clamp(BerriesPerBush,1,8)-0.5f)*0.55f;
						const float OffsetZ=Height*BerryRandom.FRandRange(0.40f,0.72f);
						const int32 FruitFrame=BerryRandom.RandRange(4,7)*8+FruitAtlasColumn(Mini);
						if (GetWorld()->GetSubsystem<UPlaceholderBerryWorld>()->Collected.Contains(Id)) continue;
						const FVector Position=GetActorTransform().TransformPosition(FVector(LocalX+OffsetX,LocalY-3.0f,LocalZ+OffsetZ));
						const FTransform Transform(FRotator::ZeroRotator,Position);
						APlaceholderBerry* Berry=GetWorld()->SpawnActorDeferred<APlaceholderBerry>(APlaceholderBerry::StaticClass(),Transform,this,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
						if (Berry)
						{
							Berry->PersistentId=Id;
							Berry->Size=BerryWorldSize;
							Berry->AtlasFrame=FruitFrame;
							Berry->LaneY=GetActorTransform().TransformPosition(FVector(LocalX,GetLaneCenterYLocal(TerrainLane),LocalZ)).Y;
							UGameplayStatics::FinishSpawningActor(Berry,Transform);
							PlaceholderBerries.Add(Berry);
						}
					}
				}
			}
		}
	}
	if (Transforms.IsEmpty()) return;

	UHierarchicalInstancedStaticMeshComponent* HISM = NewObject<UHierarchicalInstancedStaticMeshComponent>(
		this,MakeUniqueObjectName(this,UHierarchicalInstancedStaticMeshComponent::StaticClass(),TEXT("PWG_SurfaceBushes")));
	HISM->SetupAttachment(RootComponent);
	HISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EnableVegetationFillChannel(HISM);
	HISM->SetCanEverAffectNavigation(false);
	HISM->SetCastShadow(false);
	HISM->SetReceivesDecals(false);
	HISM->bEnableDensityScaling = true;
	HISM->SetVisibleInRayTracing(false);
	HISM->SetEvaluateWorldPositionOffsetInRayTracing(false);
	HISM->SetAffectDistanceFieldLighting(false);
	HISM->SetAffectDynamicIndirectLighting(false);
	HISM->SetCullDistances(FMath::Max(0,BushStartCullDistance),
		FMath::Max(BushStartCullDistance+1,BushEndCullDistance));
	HISM->SetNumCustomDataFloats(4);
	HISM->SetStaticMesh(CardMesh);
	HISM->RegisterComponent();
	HISM->AddInstances(Transforms,false,false,false);
	for (int32 Index=0;Index<CustomData.Num();++Index)
	{
		HISM->SetCustomDataValue(Index,0,CustomData[Index].X,false);
		HISM->SetCustomDataValue(Index,1,CustomData[Index].Y,false);
		HISM->SetCustomDataValue(Index,2,CustomData[Index].Z,false);
		HISM->SetCustomDataValue(Index,3,CustomData[Index].W,false);
	}
	HISM->BuildTreeIfOutdated(true,true);
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(ParentMaterial,this);
	MID->SetTextureParameterValue(TEXT("GrassAtlas"),Atlas);
	MID->SetScalarParameterValue(TEXT("AtlasColumns"),bUsingMasterAtlas?MasterWideCellColumns:FMath::Max(1,BushAtlasColumns));
	MID->SetScalarParameterValue(TEXT("AtlasRows"),bUsingMasterAtlas?MasterWideCellRows:FMath::Max(1,BushAtlasRows));
	MID->SetVectorParameterValue(TEXT("GrassTint"),BushTint);
	MID->SetScalarParameterValue(TEXT("GrassBrightness"),BushBrightness);
	MID->SetScalarParameterValue(TEXT("PixelWorldStep"),Biome.Grass.PixelWorldStep);
	HISM->SetMaterial(0,MID);
	GrassMaterials.Add(MID);
	BushInstanceComponents.Add(HISM);
	UpdateGrassMaterialParameters();
}

void APWG_Generator::BuildBlockOverhangs(const FBiomeData& Biome, FRandomStream& Random)
{
	for (UHierarchicalInstancedStaticMeshComponent* Component : BlockOverhangInstanceComponents)
	{
		if (IsValid(Component)) Component->DestroyComponent();
	}
	BlockOverhangInstanceComponents.Reset();
	const FPWGBlockOverhangSettings& Settings = Biome.BlockOverhang;
	if (!Settings.bEnabled || Settings.ChancePerTile <= 0.0f) return;

	UStaticMesh* CardMesh = GetOrCreateGrassCardMesh();
	UTexture2D* MasterAtlas = Biome.bUseBiomeMasterAtlas ? Biome.BiomeMasterAtlas.LoadSynchronous() : nullptr;
	const bool bUsingMasterAtlas = MasterAtlas != nullptr;
	UTexture2D* FallbackAtlas = Settings.FallbackAtlas.LoadSynchronous();
	if (!FallbackAtlas)
	{
		FallbackAtlas = LoadObject<UTexture2D>(nullptr,
			TEXT("/Game/Systems/Sprite2Bone/Biomes/Meadow/Meadow_Overhang/Meadows_Grass_Overhang_Atlas.Meadows_Grass_Overhang_Atlas"));
	}
	UMaterialInterface* ParentMaterial = Settings.Material.LoadSynchronous();
	if (!ParentMaterial)
	{
		ParentMaterial = LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Game/Systems/Sprite2Bone/Materials/M_PWG_Overhang.M_PWG_Overhang"));
	}
	if (!ParentMaterial) ParentMaterial = Biome.Grass.Material.LoadSynchronous();
	if (!CardMesh || (!MasterAtlas && !FallbackAtlas) || !ParentMaterial)
	{
		UE_LOG(LogPWG, Warning, TEXT("Block overhang disabled for %s: card, atlas or material is unavailable."),
			*ActiveBiomeRow.ToString());
		return;
	}

	struct FOverhangBatch
	{
		TObjectPtr<UTexture2D> Atlas;
		TArray<FTransform> Transforms;
		TArray<FVector4f> CustomData;
	};
	TArray<FOverhangBatch> Batches;
	auto IsMiniEnabled = [&](const int32 Mini)
	{
		switch (Mini)
		{
		case 0: return Settings.bGenerateOnMiniBiome1;
		case 1: return Settings.bGenerateOnMiniBiome2;
		case 2: return Settings.bGenerateOnMiniBiome3;
		case 3: return Settings.bGenerateOnMiniBiome4;
		case 4: return Settings.bGenerateOnRockSurface;
		case 5: return Settings.bGenerateOnGroundSurface;
		case 6: return Settings.bGenerateOnMiniBiome5;
		case 7: return Settings.bGenerateOnMiniBiome6;
		default: return false;
		}
	};
	auto AtlasForMini = [&](const int32 Mini)
	{
		if (MasterAtlas) return MasterAtlas;
		UTexture2D* Atlas = Settings.MiniBiomeAtlases.IsValidIndex(Mini)
			? Settings.MiniBiomeAtlases[Mini].LoadSynchronous() : nullptr;
		return Atlas ? Atlas : FallbackAtlas;
	};
	auto FindBatch = [&](UTexture2D* Atlas) -> FOverhangBatch&
	{
		for (FOverhangBatch& Batch : Batches)
		{
			if (Batch.Atlas == Atlas) return Batch;
		}
		FOverhangBatch& NewBatch = Batches.AddDefaulted_GetRef();
		NewBatch.Atlas = Atlas;
		return NewBatch;
	};

	const int32 FrameCount = FMath::Max(1, Settings.AtlasColumns * Settings.AtlasRows);
	const float Width = ActiveTileSize * FMath::Max(1.0f, Settings.WidthInTiles)
		* FMath::Max(0.1f, Settings.UniformScale);
	const float Height = Width * 0.5f; // Both the old 128x64 and master 256x128 cells are 2:1.
	const EPWGTerrainLane OverhangLanes[]={EPWGTerrainLane::Front,EPWGTerrainLane::Middle,EPWGTerrainLane::Back};
	for (int32 Tile = 0; Tile < SurfaceHeightTiles.Num(); ++Tile)
	{
		const int32 Mini = ColumnMiniBiomes.IsValidIndex(Tile) ? ColumnMiniBiomes[Tile] : 5;
		if (!IsMiniEnabled(Mini)) continue;
		for(const EPWGTerrainLane TerrainLane:OverhangLanes)
		{
			if(!IsLaneEnabledLocal(TerrainLane)) continue;
			const float Density=TerrainLane==EPWGTerrainLane::Front?Biome.FrontLane.VegetationDensityMultiplier
				:(TerrainLane==EPWGTerrainLane::Back?Biome.BackLane.VegetationDensityMultiplier:1.0f);
			if(Random.FRand()>FMath::Clamp(Settings.ChancePerTile*Density,0.0f,1.0f)) continue;
			const float X0=ChunkStartLocalX+Tile*ActiveTileSize;
			const float X1=X0+ActiveTileSize;
			const float Z0=GetLaneSurfaceHeightAtLocalX(X0,TerrainLane);
			const float Z1=GetLaneSurfaceHeightAtLocalX(X1,TerrainLane);
			const float SlopeDegrees=FMath::RadiansToDegrees(FMath::Atan2(Z1-Z0,ActiveTileSize));
			const float BaseLocalY=GetLaneCenterYLocal(TerrainLane)-ActiveTileSize*0.5f
				-FMath::Max(0.0f,Settings.DistanceInFrontOfTerrain);
			// Adjacent overhangs intentionally overlap in X. Give each neighbour a stable,
			// sub-centimetre depth rank so their masked pixels never fight in the depth buffer.
			// Eight ranks are sufficient because cards wider than that cannot overlap each other.
			const int32 DepthRank=FMath::Abs((GlobalColumnOffset+Tile)%8);
			const float LocalY=BaseLocalY-DepthRank*FMath::Max(0.0f,Settings.OverlapDepthStep);
			FOverhangBatch& Batch=FindBatch(AtlasForMini(Mini));
			Batch.Transforms.Emplace(FRotator(SlopeDegrees,0.0f,0.0f),
				FVector((X0+X1)*0.5f,LocalY,(Z0+Z1)*0.5f-Height*0.5f+Settings.VerticalOffset),
				FVector(Width/100.0f,1.0f,Height/100.0f));
			const int32 Frame = bUsingMasterAtlas
				? MasterWideFrame(8, Mini, Random.RandRange(0, 3))
				: Random.RandRange(0, FrameCount - 1);
			Batch.CustomData.Emplace(static_cast<float>(Frame),
				Random.FRand(),Random.FRandRange(0.9f,1.1f),1.0f);
		}
	}

	for (int32 BatchIndex = 0; BatchIndex < Batches.Num(); ++BatchIndex)
	{
		FOverhangBatch& Batch = Batches[BatchIndex];
		if (Batch.Transforms.IsEmpty()) continue;
		const FName Name = MakeUniqueObjectName(this, UHierarchicalInstancedStaticMeshComponent::StaticClass(),
			*FString::Printf(TEXT("BlockOverhang_%d"), BatchIndex));
		UHierarchicalInstancedStaticMeshComponent* HISM = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, Name);
		HISM->SetupAttachment(RootComponent);
		HISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		EnableVegetationFillChannel(HISM);
		HISM->SetCanEverAffectNavigation(false);
		HISM->SetCastShadow(Settings.bCastShadows);
		HISM->SetReceivesDecals(false);
		HISM->bEnableDensityScaling = true;
		HISM->SetVisibleInRayTracing(false);
		HISM->SetEvaluateWorldPositionOffsetInRayTracing(false);
		HISM->SetAffectDistanceFieldLighting(false);
		HISM->SetAffectDynamicIndirectLighting(false);
		HISM->TranslucencySortPriority = 60;
		HISM->SetCullDistances(FMath::Max(0, Settings.StartCullDistance),
			FMath::Max(Settings.StartCullDistance + 1, Settings.EndCullDistance));
		HISM->SetNumCustomDataFloats(4);
		HISM->SetStaticMesh(CardMesh);
		HISM->RegisterComponent();
		HISM->AddInstances(Batch.Transforms, false, false, false);
		for (int32 InstanceIndex = 0; InstanceIndex < Batch.CustomData.Num(); ++InstanceIndex)
		{
			const FVector4f& Data = Batch.CustomData[InstanceIndex];
			HISM->SetCustomDataValue(InstanceIndex, 0, Data.X, false);
			HISM->SetCustomDataValue(InstanceIndex, 1, Data.Y, false);
			HISM->SetCustomDataValue(InstanceIndex, 2, Data.Z, false);
			HISM->SetCustomDataValue(InstanceIndex, 3, Data.W, false);
		}
		HISM->BuildTreeIfOutdated(true, true);
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(ParentMaterial, this);
		MID->SetTextureParameterValue(TEXT("GrassAtlas"), Batch.Atlas);
		MID->SetScalarParameterValue(TEXT("AtlasColumns"), bUsingMasterAtlas
			? MasterWideCellColumns : FMath::Max(1, Settings.AtlasColumns));
		MID->SetScalarParameterValue(TEXT("AtlasRows"), bUsingMasterAtlas
			? MasterWideCellRows : FMath::Max(1, Settings.AtlasRows));
		MID->SetVectorParameterValue(TEXT("GrassTint"), Settings.Tint);
		MID->SetScalarParameterValue(TEXT("GrassBrightness"), Settings.Brightness);
		MID->SetScalarParameterValue(TEXT("PixelWorldStep"), Settings.PixelWorldStep);
		HISM->SetMaterial(0, MID);
		BlockOverhangMaterials.Add(MID);
		BlockOverhangInstanceComponents.Add(HISM);
	}
}

void APWG_Generator::BuildTrees(const FBiomeData& Biome)
{
	for (UHierarchicalInstancedStaticMeshComponent* Component : TreeInstanceComponents)
	{
		if (IsValid(Component)) Component->DestroyComponent();
	}
	TreeInstanceComponents.Reset();
	TreeMaterials.Reset();
	TreeTrunkMesh->ClearAllMeshSections();
	if (!Biome.Trees.bEnabled || Biome.Trees.MinimumSpacing <= 0.0f) return;

	UStaticMesh* LeafCardMesh = GetOrCreateGrassCardMesh();
	UTexture2D* MasterAtlas = Biome.bUseBiomeMasterAtlas ? Biome.BiomeMasterAtlas.LoadSynchronous() : nullptr;
	const bool bUsingMasterAtlas = MasterAtlas != nullptr;
	UTexture2D* FallbackAtlas = Biome.Trees.FallbackAtlas.LoadSynchronous();
	if (!FallbackAtlas)
	{
		FallbackAtlas = LoadObject<UTexture2D>(nullptr,
			TEXT("/Game/Systems/Sprite2Bone/Biomes/Meadow/Meadow_Procedural_Trees/PWG_Tree_FallbackAtlas_01.PWG_Tree_FallbackAtlas_01"));
	}
	if (!LeafCardMesh || (!MasterAtlas && !FallbackAtlas))
	{
		UE_LOG(LogPWG, Warning, TEXT("Procedural trees disabled for %s: card or fallback atlas is unavailable."),
			*ActiveBiomeRow.ToString());
		return;
	}

	struct FProceduralTreeBatch
	{
		int32 Layer = 0;
		TObjectPtr<UTexture2D> Atlas;
		FMeshBuffers Bark;
		TArray<FTransform> Leaves;
		TArray<FVector4f> LeafData;
		TArray<float> LeafWindInfluence;
	};
	TArray<FProceduralTreeBatch> Batches;
	const FPWGTreeMiniBiomeSettings* MiniSettings[] = {
		&Biome.Trees.MiniBiome1, &Biome.Trees.MiniBiome2,
		&Biome.Trees.MiniBiome3, &Biome.Trees.MiniBiome4,
		&Biome.Trees.RockSurface, &Biome.Trees.GroundSurface,
		&Biome.Trees.MiniBiome5, &Biome.Trees.MiniBiome6
	};
	UTexture2D* MiniAtlases[UE_ARRAY_COUNT(MiniSettings)] = {};
	for (int32 Mini = 0; Mini < UE_ARRAY_COUNT(MiniSettings); ++Mini)
	{
		MiniAtlases[Mini] = MasterAtlas ? MasterAtlas : MiniSettings[Mini]->AtlasOverride.LoadSynchronous();
		if (!MiniAtlases[Mini]) MiniAtlases[Mini] = FallbackAtlas;
	}
	const FPWGTreeDepthLayer* DepthLayers[] = { &Biome.Trees.BackLayer, &Biome.Trees.MiddleLayer, &Biome.Trees.FrontLayer };
	const EPWGTerrainLane TreeLanes[] = { EPWGTerrainLane::Back, EPWGTerrainLane::Middle, EPWGTerrainLane::Front };
	const int32 AtlasColumns = bUsingMasterAtlas ? MasterSquareCellColumns : FMath::Max(1, Biome.Trees.AtlasColumns);
	const int32 AtlasRows = bUsingMasterAtlas ? MasterSquareCellRows : FMath::Max(1, Biome.Trees.AtlasRows);
	const int32 AtlasFrameCount = AtlasColumns * AtlasRows;
	const float TreeCellPixels = bUsingMasterAtlas ? 128.0f : 64.0f;

	auto FindBatch = [&](const int32 Layer, UTexture2D* Atlas) -> FProceduralTreeBatch&
	{
		for (FProceduralTreeBatch& Batch : Batches)
		{
			if (Batch.Layer == Layer && Batch.Atlas == Atlas) return Batch;
		}
		FProceduralTreeBatch& Batch = Batches.AddDefaulted_GetRef();
		Batch.Layer = Layer;
		Batch.Atlas = Atlas;
		return Batch;
	};

	auto AppendAtlasQuad = [&](FMeshBuffers& Mesh, const FVector& LB, const FVector& RB,
		const FVector& RT, const FVector& LT, const int32 RawFrame, const FVector& Tangent)
	{
		const int32 Frame = FMath::Clamp(RawFrame, 0, AtlasFrameCount - 1);
		const float CellX = static_cast<float>(Frame % AtlasColumns);
		const float CellY = static_cast<float>(Frame / AtlasColumns);
		const int32 Base = Mesh.Vertices.Num();
		Mesh.Vertices.Append({ LB, RB, RT, LT });
		Mesh.Triangles.Append({ Base, Base + 1, Base + 2, Base, Base + 2, Base + 3 });
		Mesh.UVs.Append({ FVector2D(CellX, CellY + 1.0f), FVector2D(CellX + 1.0f, CellY + 1.0f),
			FVector2D(CellX + 1.0f, CellY), FVector2D(CellX, CellY) });
		for (int32 Vertex = 0; Vertex < 4; ++Vertex)
		{
			Mesh.Normals.Add(-FVector::YAxisVector);
			Mesh.Colors.Add(FColor::White);
			Mesh.Tangents.Add(FProcMeshTangent(Tangent.GetSafeNormal(), false));
		}
	};

	float SwordSurfaceLaneY=0;
	auto AppendTaperedSegment = [&](FMeshBuffers& Mesh, const FVector& Start, const FVector& End,
		const float StartVisibleWidth, const float EndVisibleWidth, const int32 Frame, const int32 DepthRank)
	{
		const FVector Direction = (End - Start).GetSafeNormal();
		if (Direction.IsNearlyZero()) return;
		GetWorld()->GetSubsystem<USwordWorldSubsystem>()->RegisterSurface(this,
			GetActorTransform().TransformPosition(Start),GetActorTransform().TransformPosition(End),
			FMath::Max(StartVisibleWidth,EndVisibleWidth)*0.5f,SwordSurfaceLaneY);
		const FVector Right(Direction.Z, 0.0f, -Direction.X);
		const float JoinOverlap = FMath::Clamp(Biome.Trees.SegmentJoinOverlap, 0.0f, 10.0f);
		// Never wrap the rank: long secondary branches can cross segments emitted much
		// earlier, and recycling a plane after 16 pieces reintroduced depth fighting.
		const float StableDepthOffset = FMath::Max(0.0f, Biome.Trees.SegmentJoinDepthStep) * DepthRank;
		FVector VisibleStart = Start - Direction * JoinOverlap;
		FVector VisibleEnd = End + Direction * JoinOverlap * 0.25f;
		VisibleStart.Y -= StableDepthOffset;
		VisibleEnd.Y -= StableDepthOffset;
		const float TextureWidth = FMath::Max(1.0f, Biome.Trees.TrunkTextureWidthPixels
			* (bUsingMasterAtlas ? 2.0f : 1.0f));
		const float StartCardHalf = FMath::Max(0.5f, StartVisibleWidth) * TreeCellPixels * 0.5f / TextureWidth;
		const float EndCardHalf = FMath::Max(0.5f, EndVisibleWidth) * TreeCellPixels * 0.5f / TextureWidth;
		AppendAtlasQuad(Mesh, VisibleStart - Right * StartCardHalf, VisibleStart + Right * StartCardHalf,
			VisibleEnd + Right * EndCardHalf, VisibleEnd - Right * EndCardHalf, Frame, Right);
	};

	auto AppendOrientedCard = [&](FMeshBuffers& Mesh, const FVector& Base, const FVector& Direction,
		const float CardWidth, const float CardHeight, const int32 Frame)
	{
		const FVector Axis = Direction.GetSafeNormal();
		const FVector Right(Axis.Z, 0.0f, -Axis.X);
		const FVector Top = Base + Axis * CardHeight;
		AppendAtlasQuad(Mesh, Base - Right * CardWidth * 0.5f, Base + Right * CardWidth * 0.5f,
			Top + Right * CardWidth * 0.5f, Top - Right * CardWidth * 0.5f, Frame, Right);
	};

	const float Spacing = FMath::Max(100.0f, Biome.Trees.MinimumSpacing);
	const double GlobalStart = static_cast<double>(GlobalColumnOffset) * ActiveTileSize;
	const double GlobalEnd = GlobalStart + static_cast<double>(SurfaceHeightTiles.Num()) * ActiveTileSize;
	const int64 FirstSlot = FMath::FloorToInt64(GlobalStart / Spacing) - 1;
	const int64 LastSlot = FMath::CeilToInt64(GlobalEnd / Spacing) + 1;
	int32 GeneratedTrees = 0;
	int32 GeneratedSegments = 0;
	int32 GeneratedLeafCards = 0;

	for (int32 LayerIndex = 0; LayerIndex < UE_ARRAY_COUNT(DepthLayers); ++LayerIndex)
	{
		const FPWGTreeDepthLayer& Layer = *DepthLayers[LayerIndex];
		const EPWGTerrainLane TerrainLane=TreeLanes[LayerIndex];
		SwordSurfaceLaneY=GetLaneWorldY(TerrainLane);
		if (!IsLaneEnabledLocal(TerrainLane)||!Layer.bEnabled||Layer.DensityMultiplier<=0.0f) continue;
		const float LaneDensity=TerrainLane==EPWGTerrainLane::Front?Biome.FrontLane.VegetationDensityMultiplier
			:(TerrainLane==EPWGTerrainLane::Back?Biome.BackLane.VegetationDensityMultiplier:1.0f);

		for (int64 Slot = FirstSlot; Slot <= LastSlot; ++Slot)
		{
			const uint32 SlotHash = HashCombineFast(GetTypeHash(ActiveWorldSeed), GetTypeHash(Slot));
			FRandomStream TreeRandom(static_cast<int32>(HashCombineFast(SlotHash, GetTypeHash(LayerIndex))));
			const double CandidateGlobalX = (static_cast<double>(Slot) + 0.5) * Spacing
				+ TreeRandom.FRandRange(-0.34f * Spacing, 0.34f * Spacing);
			if (CandidateGlobalX < GlobalStart || CandidateGlobalX >= GlobalEnd) continue;

			const float LocalX = ChunkStartLocalX + static_cast<float>(CandidateGlobalX - GlobalStart);
			const int32 Column = FMath::FloorToInt((LocalX - ChunkStartLocalX) / ActiveTileSize);
			const int32 Mini = ColumnMiniBiomes.IsValidIndex(Column) ? ColumnMiniBiomes[Column] : 5;
			if (Mini < 0 || Mini >= UE_ARRAY_COUNT(MiniSettings)) continue;
			const FPWGTreeMiniBiomeSettings& MiniRule = *MiniSettings[Mini];
			if (!MiniRule.bEnabled || MiniRule.TreesPer100Metres <= 0.0f) continue;
			const float SpawnProbability = FMath::Clamp(
				MiniRule.TreesPer100Metres * Layer.DensityMultiplier * LaneDensity * Spacing / 10000.0f, 0.0f, 1.0f);
			if (TreeRandom.FRand() > SpawnProbability) continue;

			FProceduralTreeBatch& Batch = FindBatch(LayerIndex, MiniAtlases[Mini]);
			const float LayerScale = FMath::Max(0.1f, Layer.ScaleMultiplier);
			const float TreeHeight = TreeRandom.FRandRange(
				FMath::Min(Biome.Trees.TreeHeightRange.X, Biome.Trees.TreeHeightRange.Y),
				FMath::Max(Biome.Trees.TreeHeightRange.X, Biome.Trees.TreeHeightRange.Y)) * LayerScale;
			const float BaseWidth = TreeRandom.FRandRange(
				FMath::Min(Biome.Trees.BaseWidthRange.X, Biome.Trees.BaseWidthRange.Y),
				FMath::Max(Biome.Trees.BaseWidthRange.X, Biome.Trees.BaseWidthRange.Y)) * LayerScale;
			const float TipWidth = TreeRandom.FRandRange(
				FMath::Min(Biome.Trees.TipWidthRange.X, Biome.Trees.TipWidthRange.Y),
				FMath::Max(Biome.Trees.TipWidthRange.X, Biome.Trees.TipWidthRange.Y)) * LayerScale;
			const float LocalY = TreeRandom.FRandRange(FMath::Min(Layer.DepthRange.X, Layer.DepthRange.Y),
				FMath::Max(Layer.DepthRange.X, Layer.DepthRange.Y));
			const FVector Root(LocalX, LocalY,
				GetLaneSurfaceHeightAtLocalX(LocalX,TerrainLane) + Biome.Trees.SurfaceOffset);
			const float TreePhase = TreeRandom.FRand();
			int32 SegmentBudget = FMath::Max(4, Biome.Trees.MaximumSegmentsPerTree);
			int32 ForkCount = 0;
			int32 MainBranchCount = 0;
			int32 TerminalCount = 0;
			int32 LeafHierarchyOrder = 0;
			int32 BarkHierarchyOrder = 0;
			const bool bNestCrownAllowed = bUsingMasterAtlas && TreeRandom.FRand() < 0.08f;
			const bool bFruitCrownAllowed = bUsingMasterAtlas && TreeRandom.FRand() < 0.14f;
			bool bNestCrownPlaced = false;
			bool bFruitCrownPlaced = false;
			FRandomStream FruitRandom(static_cast<int32>(HashCombineFast(SlotHash,GetTypeHash(LayerIndex))) ^ 0xF817u);
			const bool bFruitTree=bEnablePlaceholderBerries && FruitRandom.FRand()<TreeFruitChance;
			int32 FruitCount=0;

			auto AddCrown = [&](const FVector& Tip, const FVector& BranchDirection, const int32 BranchDepth)
			{
				const int32 MinCards = FMath::Max(1, FMath::Min(Biome.Trees.LeafCardsPerTipRange.X,
					Biome.Trees.LeafCardsPerTipRange.Y));
				const int32 MaxCards = FMath::Max(MinCards, FMath::Max(Biome.Trees.LeafCardsPerTipRange.X,
					Biome.Trees.LeafCardsPerTipRange.Y));
				// Master-atlas rows have semantic roles and are not interchangeable:
				// 0 dense, 1 medium, 2 sparse, 3 nest, 4 dry, 5 fruit, 6 left, 7 right.
				// A special crown can replace one suitable side-branch crown, once per tree.
				const bool bSpecialCandidate = bUsingMasterAtlas && BranchDepth > 0;
				const bool bUseNest = bSpecialCandidate && bNestCrownAllowed && !bNestCrownPlaced;
				const bool bUseFruit = bSpecialCandidate && !bUseNest && bFruitCrownAllowed
					&& !bFruitCrownPlaced;
				if (bUseNest) bNestCrownPlaced = true;
				if (bUseFruit) bFruitCrownPlaced = true;
				const int32 Cards = (bUseNest || bUseFruit) ? 1 : TreeRandom.RandRange(MinCards, MaxCards);
				const float Spread = TreeRandom.FRandRange(FMath::Min(Biome.Trees.CrownSpreadRange.X,
					Biome.Trees.CrownSpreadRange.Y), FMath::Max(Biome.Trees.CrownSpreadRange.X,
					Biome.Trees.CrownSpreadRange.Y)) * LayerScale;
				for (int32 Card = 0; Card < Cards; ++Card)
				{
					// Nest and fruit cells are authored at their native 128 px size. They must
					// never inherit the large random crown scale used by ordinary foliage.
					const float Size = (bUseNest || bUseFruit)
						? TreeCellPixels * LayerScale
						: TreeRandom.FRandRange(FMath::Min(Biome.Trees.LeafCardSizeRange.X,
							Biome.Trees.LeafCardSizeRange.Y), FMath::Max(Biome.Trees.LeafCardSizeRange.X,
							Biome.Trees.LeafCardSizeRange.Y)) * LayerScale;
					// The branch tip must stay inside the leaf card. Previously Spread could be
					// wider than the card itself and the positive Z offset could place an entire
					// clump above its terminal branch, producing visibly airborne leaves.
					const float HorizontalOffsetLimit = FMath::Min(Spread,
						Size * FMath::Clamp(Biome.Trees.CrownMaximumHorizontalOffsetFraction, 0.0f, 0.5f));
					const float VerticalJitter = Size * FMath::Clamp(
						Biome.Trees.CrownVerticalJitterFraction, 0.0f, 0.3f);
					// Camera is on negative Y. The first emitted leaf card stays closest and every
					// following card is placed slightly farther away, creating a stable depth order.
					const float HorizontalOffset = (bUseNest || bUseFruit) ? 0.0f
						: TreeRandom.FRandRange(-HorizontalOffsetLimit, HorizontalOffsetLimit);
					const float VerticalOffset = -Size * 0.45f + ((bUseNest || bUseFruit) ? 0.0f
						: TreeRandom.FRandRange(-VerticalJitter, VerticalJitter));
					// Bark segments use their own small negative-Y hierarchy. Put the entire
					// leaf stack farther towards the camera than the maximum possible bark
					// offset, while retaining a deterministic first-card-on-top order.
					const float LeafFrontDistance = 2.0f
						+ FMath::Max(0.0f, Biome.Trees.SegmentJoinDepthStep)
							* FMath::Max(4, Biome.Trees.MaximumSegmentsPerTree)
						+ FMath::Max(0.0f, Biome.Trees.BranchOverlapDepthOffset)
							* (FMath::Max(0, Biome.Trees.MaximumBranchDepth) + 1);
					const FVector Offset(HorizontalOffset,
						-LeafFrontDistance + FMath::Min(0.5f, LeafHierarchyOrder * 0.01f), VerticalOffset);
					++LeafHierarchyOrder;
					// Preserve the seeded random sequence but avoid a negative instance scale.
					// Mirroring that way reverses the lit card's handedness and produced isolated
					// leaf clumps with completely different lighting.
					TreeRandom.RandRange(0, 1);
					Batch.Leaves.Emplace(FRotator::ZeroRotator, Tip + Offset,
						FVector(Size / 100.0f, 1.0f,
							TreeRandom.FRandRange(0.82f, 1.18f) * Size / 100.0f));
					const int32 FirstLeaf = FMath::Clamp(Biome.Trees.LeavesFirstFrame, 0, AtlasFrameCount - 1);
					const int32 LastLeaf = FMath::Clamp(FirstLeaf + FMath::Max(1, Biome.Trees.LeavesFrameCount) - 1,
						FirstLeaf, AtlasFrameCount - 1);
					const FVector2D BrightnessRange = Biome.Trees.LeafBrightnessVariation.GetMax() > 0.0f
						? Biome.Trees.LeafBrightnessVariation : FVector2D(0.70f, 1.25f);
					const float LeafBrightnessMin = FMath::Clamp(FMath::Min(
						BrightnessRange.X, BrightnessRange.Y), 0.55f, 1.30f);
					const float LeafBrightnessMax = FMath::Clamp(FMath::Max(
						BrightnessRange.X, BrightnessRange.Y),
						LeafBrightnessMin, 1.30f);
					int32 LeafFrame = TreeRandom.RandRange(FirstLeaf, LastLeaf);
					if (bUsingMasterAtlas)
					{
						int32 CrownRow = 1; // medium is the neutral fallback
						if (bUseNest) CrownRow = 3;
						else if (bUseFruit) CrownRow = 5;
						else if (Card == 0 && BranchDepth > 0 && FMath::Abs(BranchDirection.X) > 0.12f)
						{
							// A branch growing left receives the left-facing side crown, and vice versa.
							CrownRow = BranchDirection.X < 0.0f ? 6 : 7;
						}
						else
						{
							const float VariantRoll = TreeRandom.FRand();
							CrownRow = VariantRoll < 0.42f ? 0
								: (VariantRoll < 0.76f ? 1 : (VariantRoll < 0.94f ? 2 : 4));
						}
						LeafFrame = CrownRow * MasterSquareCellColumns + 8 + Mini;
					}
					Batch.LeafData.Emplace(static_cast<float>(LeafFrame),
						FMath::Frac(TreePhase + TerminalCount * 0.113f + Card * 0.047f),
						TreeRandom.FRandRange(0.90f, 1.10f),
						TreeRandom.FRandRange(LeafBrightnessMin, LeafBrightnessMax));
					Batch.LeafWindInfluence.Add(TreeRandom.FRandRange(0.55f, 1.0f));
					if (bFruitTree && !bUseNest && FruitCount<FMath::Clamp(MaxFruitsPerTree,1,20))
					{
						Batch.LeafWindInfluence.Last()=0.0f;
						for (int32 Fruit=0;Fruit<2 && FruitCount<FMath::Clamp(MaxFruitsPerTree,1,20);++Fruit,++FruitCount)
						{
							const FString Id=FString::Printf(TEXT("tree:%d:%lld:%d:%d"),ActiveWorldSeed,Slot,LayerIndex,FruitCount);
							const FVector P=Batch.Leaves.Last().TransformPosition(FVector(Fruit==0?-15:15,-3,50));
							SpawnPlaceholderFruit(P,GetLaneCenterYLocal(TerrainLane),Id,FruitRandom.RandRange(0,3)*8+FruitAtlasColumn(Mini));
						}
					}
					++GeneratedLeafCards;
				}
				++TerminalCount;
			};

			TFunction<void(FVector, float, float, float, float, int32)> GrowPath;
			GrowPath = [&](FVector Current, float AngleDegrees, const float TotalLength,
				const float StartWidth, const float EndWidth, const int32 Depth)
			{
				float Travelled = 0.0f;
				float LastSegmentLength = 64.0f;
				FVector LastDirection(0.0f, 0.0f, 1.0f);
				while (Travelled < TotalLength - 1.0f && SegmentBudget > 0)
				{
					LastSegmentLength = FMath::Min(TotalLength - Travelled, TreeRandom.FRandRange(
						FMath::Min(Biome.Trees.SegmentLengthRange.X, Biome.Trees.SegmentLengthRange.Y),
						FMath::Max(Biome.Trees.SegmentLengthRange.X, Biome.Trees.SegmentLengthRange.Y)) * LayerScale);
					const float Turn = TreeRandom.FRandRange(FMath::Min(Biome.Trees.TurnDegreesRange.X,
						Biome.Trees.TurnDegreesRange.Y), FMath::Max(Biome.Trees.TurnDegreesRange.X,
						Biome.Trees.TurnDegreesRange.Y));
					AngleDegrees += Turn * (Depth == 0 ? 1.0f : 1.35f);
					const float AngleLimit = Depth == 0 ? FMath::Max(1.0f, Biome.Trees.MaximumLeanDegrees) : 78.0f;
					AngleDegrees = FMath::Clamp(AngleDegrees, -AngleLimit, AngleLimit);
					const float Radians = FMath::DegreesToRadians(AngleDegrees);
					LastDirection = FVector(FMath::Sin(Radians), 0.0f, FMath::Cos(Radians));
					const FVector Next = Current + LastDirection * LastSegmentLength;
					const float Alpha0 = Travelled / FMath::Max(1.0f, TotalLength);
					const float Alpha1 = (Travelled + LastSegmentLength) / FMath::Max(1.0f, TotalLength);
					const float Width0 = FMath::Lerp(StartWidth, EndWidth, FMath::Pow(Alpha0, 0.72f));
					const float Width1 = FMath::Lerp(StartWidth, EndWidth, FMath::Pow(Alpha1, 0.72f));
					const int32 FirstTrunk = FMath::Clamp(Biome.Trees.TrunkFirstFrame, 0, AtlasFrameCount - 1);
					const int32 LastTrunk = FMath::Clamp(FirstTrunk + FMath::Max(1, Biome.Trees.TrunkFrameCount) - 1,
						FirstTrunk, AtlasFrameCount - 1);
					const int32 TrunkFrame = bUsingMasterAtlas
						? TreeRandom.RandRange(1, 7) * MasterSquareCellColumns + Mini
						: TreeRandom.RandRange(FirstTrunk, LastTrunk);
					AppendTaperedSegment(Batch.Bark, Current, Next, Width0, Width1,
						TrunkFrame, BarkHierarchyOrder++);
					--SegmentBudget;
					++GeneratedSegments;
					Travelled += LastSegmentLength;
					Current = Next;

					const float PathProgress = Travelled / FMath::Max(1.0f, TotalLength);
					const bool bMayFork = Depth < FMath::Max(0, Biome.Trees.MaximumBranchDepth)
						&& ForkCount < FMath::Max(0, Biome.Trees.MaximumPrimaryBranches)
						&& PathProgress >= Biome.Trees.BranchStartHeightFraction
						&& Travelled < TotalLength * 0.88f && SegmentBudget > 2;
					const int32 RequiredMainBranches = FMath::Min(
						FMath::Max(0, Biome.Trees.MinimumPrimaryBranches),
						FMath::Max(0, Biome.Trees.MaximumPrimaryBranches));
					const float NextRequiredProgress = RequiredMainBranches > 0
						? FMath::Lerp(FMath::Max(Biome.Trees.BranchStartHeightFraction, 0.28f), 0.72f,
							static_cast<float>(MainBranchCount) / RequiredMainBranches)
						: 1.0f;
					const bool bForceMainFork = Depth == 0 && MainBranchCount < RequiredMainBranches
						&& PathProgress >= NextRequiredProgress;
					if (!bMayFork || (!bForceMainFork && TreeRandom.FRand() > Biome.Trees.BranchChancePerSegment)) continue;

					// A split is made from two ordinary tapered trunks. Their wide bases overlap at
					// the same fork point, removing the need for a dedicated junction sprite.
					// The child is moved a fraction towards the camera to avoid coplanar flicker
					// and to give every recursive branch a deterministic visual hierarchy.
					const bool bMainUsesRight = AngleDegrees >= 0.0f ? true : TreeRandom.RandRange(0, 1) != 0;
					FVector BranchStart = Current;
					BranchStart.Y -= FMath::Max(0.0f, Biome.Trees.BranchOverlapDepthOffset) * (Depth + 1);
					const float SideSign = bMainUsesRight ? -1.0f : 1.0f;
					const float SideAngle = AngleDegrees + SideSign * TreeRandom.FRandRange(
						FMath::Min(Biome.Trees.BranchAngleRange.X, Biome.Trees.BranchAngleRange.Y),
						FMath::Max(Biome.Trees.BranchAngleRange.X, Biome.Trees.BranchAngleRange.Y));
					const float SideLength = TreeHeight * TreeRandom.FRandRange(
						FMath::Min(Biome.Trees.BranchLengthFractionRange.X, Biome.Trees.BranchLengthFractionRange.Y),
						FMath::Max(Biome.Trees.BranchLengthFractionRange.X, Biome.Trees.BranchLengthFractionRange.Y))
						* FMath::Pow(0.72f, Depth);
					++ForkCount;
					if (Depth == 0) ++MainBranchCount;
					GrowPath(BranchStart, SideAngle, SideLength, Width1 * TreeRandom.FRandRange(0.42f, 0.62f),
						FMath::Max(3.0f, EndWidth * 0.65f), Depth + 1);
					AngleDegrees += (bMainUsesRight ? 1.0f : -1.0f) * TreeRandom.FRandRange(2.0f, 7.0f);
				}

				const float TerminalPixels = EndWidth / FMath::Max(0.01f, LastSegmentLength / TreeCellPixels);
				const float EndCapMaximumWidthPixels = Biome.Trees.EndCapMaximumWidthPixels
					* (bUsingMasterAtlas ? 2.0f : 1.0f);
				if (TerminalPixels <= EndCapMaximumWidthPixels)
				{
					const float CapWidth = EndWidth * TreeCellPixels / FMath::Max(1.0f, EndCapMaximumWidthPixels);
					const int32 EndCapFrame = bUsingMasterAtlas ? Mini : Biome.Trees.EndCapFrame;
					AppendOrientedCard(Batch.Bark, Current, LastDirection, CapWidth, CapWidth,
						EndCapFrame);
				}
				AddCrown(Current, LastDirection, Depth);
			};

			GrowPath(Root, TreeRandom.FRandRange(-4.0f, 4.0f), TreeHeight, BaseWidth, TipWidth, 0);
			++GeneratedTrees;
		}
	}

	UMaterialInterface* TreeParent = Biome.Trees.Material.LoadSynchronous();
	TreeTrunkMesh->SetCullDistance(FMath::Max3(Biome.Trees.BackLayer.EndCullDistance,
		Biome.Trees.MiddleLayer.EndCullDistance,Biome.Trees.FrontLayer.EndCullDistance));
	int32 BarkSection = 0;
	for (int32 BatchIndex = 0; BatchIndex < Batches.Num(); ++BatchIndex)
	{
		FProceduralTreeBatch& Batch = Batches[BatchIndex];
		UMaterialInstanceDynamic* MID = TreeParent ? UMaterialInstanceDynamic::Create(TreeParent, this) : nullptr;
		if (MID)
		{
			MID->SetTextureParameterValue(TEXT("TreeAtlas"), Batch.Atlas);
			MID->SetScalarParameterValue(TEXT("AtlasColumns"), AtlasColumns);
			MID->SetScalarParameterValue(TEXT("AtlasRows"), AtlasRows);
			MID->SetVectorParameterValue(TEXT("TreeTint"), Biome.Trees.Tint);
			MID->SetScalarParameterValue(TEXT("TreeBrightness"), Biome.Trees.Brightness);
			TreeMaterials.Add(MID);
		}
		else if (BatchIndex == 0)
		{
			UE_LOG(LogPWG, Warning, TEXT("Procedural trees were generated for %s, but Trees.Material is not assigned."),
				*ActiveBiomeRow.ToString());
		}

		if (!Batch.Bark.Vertices.IsEmpty())
		{
			TreeTrunkMesh->CreateMeshSection(BarkSection, Batch.Bark.Vertices, Batch.Bark.Triangles,
				Batch.Bark.Normals, Batch.Bark.UVs, Batch.Bark.Colors, Batch.Bark.Tangents, false);
			TreeTrunkMesh->SetMaterial(BarkSection++, MID ? static_cast<UMaterialInterface*>(MID) : TreeParent);
		}
		if (Batch.Leaves.IsEmpty()) continue;

		const FName Name = MakeUniqueObjectName(this, UHierarchicalInstancedStaticMeshComponent::StaticClass(),
			*FString::Printf(TEXT("TreeLeaves_L%d_B%d"), Batch.Layer, BatchIndex));
		UHierarchicalInstancedStaticMeshComponent* HISM = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, Name);
		HISM->SetupAttachment(RootComponent);
		HISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		EnableVegetationFillChannel(HISM);
		HISM->SetCanEverAffectNavigation(false);
		HISM->SetCastShadow(Biome.Trees.bCastShadows);
		HISM->SetReceivesDecals(false);
		HISM->bEnableDensityScaling = true;
		HISM->SetVisibleInRayTracing(false);
		HISM->SetEvaluateWorldPositionOffsetInRayTracing(false);
		HISM->SetAffectDistanceFieldLighting(false);
		HISM->SetAffectDynamicIndirectLighting(false);
		const FPWGTreeDepthLayer& Layer = *DepthLayers[Batch.Layer];
		HISM->SetCullDistances(FMath::Max(0, Layer.StartCullDistance),
			FMath::Max(Layer.StartCullDistance + 1, Layer.EndCullDistance));
		HISM->SetNumCustomDataFloats(5);
		HISM->SetStaticMesh(LeafCardMesh);
		HISM->RegisterComponent();
		HISM->AddInstances(Batch.Leaves, false, false, false);
		for (int32 InstanceIndex = 0; InstanceIndex < Batch.LeafData.Num(); ++InstanceIndex)
		{
			const FVector4f& Data = Batch.LeafData[InstanceIndex];
			HISM->SetCustomDataValue(InstanceIndex, 0, Data.X, false);
			HISM->SetCustomDataValue(InstanceIndex, 1, Data.Y, false);
			HISM->SetCustomDataValue(InstanceIndex, 2, Data.Z, false);
			HISM->SetCustomDataValue(InstanceIndex, 3, Data.W, false);
			HISM->SetCustomDataValue(InstanceIndex, 4, Batch.LeafWindInfluence[InstanceIndex], false);
		}
		HISM->BuildTreeIfOutdated(true, true);
		HISM->SetMaterial(0, MID ? static_cast<UMaterialInterface*>(MID) : TreeParent);
		TreeInstanceComponents.Add(HISM);
	}
	UE_LOG(LogPWG, Display, TEXT("Generated %d procedural trees (%d bark segments, %d leaf cards, %d leaf batches) for %s."),
		GeneratedTrees, GeneratedSegments, GeneratedLeafCards, TreeInstanceComponents.Num(), *ActiveBiomeRow.ToString());
	UpdateGrassMaterialParameters();
}

void APWG_Generator::BuildLegacyTrees(const FBiomeData& Biome)
{
	for (UHierarchicalInstancedStaticMeshComponent* Component : TreeInstanceComponents)
	{
		if (IsValid(Component)) Component->DestroyComponent();
	}
	TreeInstanceComponents.Reset();
	TreeMaterials.Reset();
	if (!Biome.Trees.bEnabled || Biome.Trees.MinimumSpacing <= 0.0f) return;

	UStaticMesh* CardMesh = GetOrCreateGrassCardMesh();
	UTexture2D* FallbackAtlas = Biome.Trees.FallbackAtlas.LoadSynchronous();
	if (!FallbackAtlas)
	{
		FallbackAtlas = LoadObject<UTexture2D>(nullptr,
			TEXT("/Game/Systems/Sprite2Bone/Biomes/Meadow/Meadow_Procedural_Trees/Birch_Atlas.Birch_Atlas"));
	}
	if (!CardMesh || !FallbackAtlas)
	{
		UE_LOG(LogPWG, Warning, TEXT("Trees disabled for %s: runtime card or fallback atlas is unavailable."),
			*ActiveBiomeRow.ToString());
		return;
	}

	struct FTreeBatch
	{
		int32 Layer = 0;
		TObjectPtr<UTexture2D> Atlas;
		TArray<FTransform> Transforms;
		TArray<FVector4f> CustomDataA;
		TArray<float> WindInfluences;
	};
	TArray<FTreeBatch> Batches;
	const FPWGTreeMiniBiomeSettings* MiniSettings[] = {
		&Biome.Trees.MiniBiome1, &Biome.Trees.MiniBiome2,
		&Biome.Trees.MiniBiome3, &Biome.Trees.MiniBiome4
	};
	UTexture2D* MiniAtlases[UE_ARRAY_COUNT(MiniSettings)] = {};
	for (int32 Mini = 0; Mini < UE_ARRAY_COUNT(MiniSettings); ++Mini)
	{
		MiniAtlases[Mini] = MiniSettings[Mini]->AtlasOverride.LoadSynchronous();
		if (!MiniAtlases[Mini]) MiniAtlases[Mini] = FallbackAtlas;
	}
	const FPWGTreeDepthLayer* DepthLayers[] = { &Biome.Trees.BackLayer, &Biome.Trees.MiddleLayer, &Biome.Trees.FrontLayer };
	const EPWGTerrainLane TreeLanes[] = { EPWGTerrainLane::Back, EPWGTerrainLane::Middle, EPWGTerrainLane::Front };
	const int32 AtlasFrameCount = FMath::Max(1, Biome.Trees.AtlasColumns * Biome.Trees.AtlasRows);

	auto FindBatch = [&](const int32 Layer, UTexture2D* Atlas) -> FTreeBatch&
	{
		for (FTreeBatch& Batch : Batches)
		{
			if (Batch.Layer == Layer && Batch.Atlas == Atlas) return Batch;
		}
		FTreeBatch& Batch = Batches.AddDefaulted_GetRef();
		Batch.Layer = Layer;
		Batch.Atlas = Atlas;
		return Batch;
	};

	const float Spacing = FMath::Max(100.0f, Biome.Trees.MinimumSpacing);
	const double GlobalStart = static_cast<double>(GlobalColumnOffset) * ActiveTileSize;
	const double GlobalEnd = GlobalStart + static_cast<double>(SurfaceHeightTiles.Num()) * ActiveTileSize;
	const int64 FirstSlot = FMath::FloorToInt64(GlobalStart / Spacing) - 1;
	const int64 LastSlot = FMath::CeilToInt64(GlobalEnd / Spacing) + 1;

	for (int32 LayerIndex = 0; LayerIndex < UE_ARRAY_COUNT(DepthLayers); ++LayerIndex)
	{
		const FPWGTreeDepthLayer& Layer = *DepthLayers[LayerIndex];
		const EPWGTerrainLane TerrainLane=TreeLanes[LayerIndex];
		if (!IsLaneEnabledLocal(TerrainLane)||!Layer.bEnabled||Layer.DensityMultiplier<=0.0f) continue;

		for (int64 Slot = FirstSlot; Slot <= LastSlot; ++Slot)
		{
			const uint32 SlotHash = HashCombineFast(GetTypeHash(ActiveWorldSeed), GetTypeHash(Slot));
			FRandomStream TreeRandom(static_cast<int32>(HashCombineFast(SlotHash, GetTypeHash(LayerIndex))));
			const double CandidateGlobalX = (static_cast<double>(Slot) + 0.5) * Spacing
				+ TreeRandom.FRandRange(-0.34f * Spacing, 0.34f * Spacing);
			if (CandidateGlobalX < GlobalStart || CandidateGlobalX >= GlobalEnd) continue;

			const float LocalX = ChunkStartLocalX + static_cast<float>(CandidateGlobalX - GlobalStart);
			const int32 Column = FMath::FloorToInt((LocalX - ChunkStartLocalX) / ActiveTileSize);
			const int32 Mini = ColumnMiniBiomes.IsValidIndex(Column) ? ColumnMiniBiomes[Column] : 5;
			if (Mini < 0 || Mini >= UE_ARRAY_COUNT(MiniSettings)) continue; // Ground and Rock never receive trees.
			const FPWGTreeMiniBiomeSettings& MiniRule = *MiniSettings[Mini];
			if (!MiniRule.bEnabled || MiniRule.TreesPer100Metres <= 0.0f) continue;

			const float SpawnProbability = FMath::Clamp(
				MiniRule.TreesPer100Metres * Layer.DensityMultiplier * Spacing / 10000.0f, 0.0f, 1.0f);
			if (TreeRandom.FRand() > SpawnProbability) continue;

			UTexture2D* Atlas = MiniAtlases[Mini];
			FTreeBatch& Batch = FindBatch(LayerIndex, Atlas);
			const float LayerScale = FMath::Max(0.1f, Layer.ScaleMultiplier);
			const float TreeHeight = TreeRandom.FRandRange(
				FMath::Min(Biome.Trees.TreeHeightRange.X, Biome.Trees.TreeHeightRange.Y),
				FMath::Max(Biome.Trees.TreeHeightRange.X, Biome.Trees.TreeHeightRange.Y)) * LayerScale;
			const float LocalY = TreeRandom.FRandRange(
				FMath::Min(Layer.DepthRange.X, Layer.DepthRange.Y),
				FMath::Max(Layer.DepthRange.X, Layer.DepthRange.Y));
			const float SurfaceZ = GetLaneSurfaceHeightAtLocalX(LocalX,TerrainLane) + Biome.Trees.SurfaceOffset;
			const float TreePhase = TreeRandom.FRand();

			FRandomStream FruitRandom(static_cast<int32>(HashCombineFast(GetTypeHash(ActiveWorldSeed),
				HashCombineFast(GetTypeHash(Slot),GetTypeHash(LayerIndex)))) ^ 0xF817u);
			const bool bFruitTree=bEnablePlaceholderBerries && FruitRandom.FRand()<TreeFruitChance;
			int32 FruitCount=0;
			auto AddParts = [&](const FPWGTreePartRule& Rule, const float DepthOffset)
			{
				const int32 MinCount = FMath::Max(0, FMath::Min(Rule.CountRange.X, Rule.CountRange.Y));
				const int32 MaxCount = FMath::Max(MinCount, FMath::Max(Rule.CountRange.X, Rule.CountRange.Y));
				const int32 Count = TreeRandom.RandRange(MinCount, MaxCount);
				for (int32 PartIndex = 0; PartIndex < Count; ++PartIndex)
				{
					const int32 FirstFrame = FMath::Clamp(Rule.FirstFrame, 0, AtlasFrameCount - 1);
					const int32 LastFrame = FMath::Clamp(FirstFrame + FMath::Max(1, Rule.FrameCount) - 1,
						FirstFrame, AtlasFrameCount - 1);
					const float Width = TreeHeight * TreeRandom.FRandRange(
						FMath::Min(Rule.WidthFractionRange.X, Rule.WidthFractionRange.Y),
						FMath::Max(Rule.WidthFractionRange.X, Rule.WidthFractionRange.Y));
					const float Height = TreeHeight * TreeRandom.FRandRange(
						FMath::Min(Rule.HeightFractionRange.X, Rule.HeightFractionRange.Y),
						FMath::Max(Rule.HeightFractionRange.X, Rule.HeightFractionRange.Y));
					const float OffsetX = TreeHeight * TreeRandom.FRandRange(
						FMath::Min(Rule.HorizontalOffsetFractionRange.X, Rule.HorizontalOffsetFractionRange.Y),
						FMath::Max(Rule.HorizontalOffsetFractionRange.X, Rule.HorizontalOffsetFractionRange.Y));
					const float OffsetZ = TreeHeight * TreeRandom.FRandRange(
						FMath::Min(Rule.VerticalOffsetFractionRange.X, Rule.VerticalOffsetFractionRange.Y),
						FMath::Max(Rule.VerticalOffsetFractionRange.X, Rule.VerticalOffsetFractionRange.Y));
					const bool bMirror = Rule.bAllowHorizontalMirror && TreeRandom.RandRange(0, 1) != 0;
					const FVector Scale((bMirror ? -1.0f : 1.0f) * FMath::Max(1.0f, Width) / 100.0f,
						1.0f, FMath::Max(1.0f, Height) / 100.0f);
					Batch.Transforms.Emplace(FRotator::ZeroRotator,
						FVector(LocalX + OffsetX, LocalY + DepthOffset, SurfaceZ + OffsetZ), Scale);
					if (&Rule==&Biome.Trees.Trunk)
					{
						const FVector Base(LocalX+OffsetX,LocalY+DepthOffset,SurfaceZ+OffsetZ);
						GetWorld()->GetSubsystem<USwordWorldSubsystem>()->RegisterSurface(this,
							GetActorTransform().TransformPosition(Base),GetActorTransform().TransformPosition(Base+FVector(0,0,Height)),
							Width*0.15f,GetLaneWorldY(TerrainLane));
					}
					Batch.CustomDataA.Emplace(static_cast<float>(TreeRandom.RandRange(FirstFrame, LastFrame)),
						FMath::Frac(TreePhase + PartIndex * 0.071f), TreeRandom.FRandRange(0.82f, 1.18f),
						TreeRandom.FRandRange(0.90f, 1.10f));
					Batch.WindInfluences.Add(TreeRandom.FRandRange(
						FMath::Min(Rule.WindInfluenceRange.X, Rule.WindInfluenceRange.Y),
						FMath::Max(Rule.WindInfluenceRange.X, Rule.WindInfluenceRange.Y)));
					if (bFruitTree && (&Rule==&Biome.Trees.LargeCrown || &Rule==&Biome.Trees.SmallCrowns)
						&& FruitCount<FMath::Clamp(MaxFruitsPerTree,1,20))
					{
						Batch.WindInfluences.Last()=0.0f;
						for (int32 Fruit=0;Fruit<2 && FruitCount<FMath::Clamp(MaxFruitsPerTree,1,20);++Fruit,++FruitCount)
						{
							const FString Id=FString::Printf(TEXT("legacy-tree:%d:%lld:%d:%d"),ActiveWorldSeed,Slot,LayerIndex,FruitCount);
							SpawnPlaceholderFruit(Batch.Transforms.Last().TransformPosition(FVector(Fruit==0?-15:15,-3,50)),
								GetLaneCenterYLocal(TerrainLane),Id,FruitRandom.RandRange(0,3)*8+FruitAtlasColumn(Mini));
						}
					}
				}
			};

			// A tiny depth separation prevents masked cards from z-fighting where parts overlap.
			AddParts(Biome.Trees.Trunk, 0.0f);
			AddParts(Biome.Trees.Accents, -0.35f);
			AddParts(Biome.Trees.LargeCrown, -0.7f);
			AddParts(Biome.Trees.SmallCrowns, -1.05f);
		}
	}

	UMaterialInterface* TreeParent = Biome.Trees.Material.LoadSynchronous();
	for (int32 BatchIndex = 0; BatchIndex < Batches.Num(); ++BatchIndex)
	{
		FTreeBatch& Batch = Batches[BatchIndex];
		if (Batch.Transforms.IsEmpty()) continue;
		const FName Name = MakeUniqueObjectName(this, UHierarchicalInstancedStaticMeshComponent::StaticClass(),
			*FString::Printf(TEXT("Trees_L%d_B%d"), Batch.Layer, BatchIndex));
		UHierarchicalInstancedStaticMeshComponent* HISM = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, Name);
		HISM->SetupAttachment(RootComponent);
		HISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		EnableVegetationFillChannel(HISM);
		HISM->SetCanEverAffectNavigation(false);
		HISM->SetCastShadow(Biome.Trees.bCastShadows);
		HISM->SetReceivesDecals(false);
		const FPWGTreeDepthLayer& Layer = *DepthLayers[Batch.Layer];
		HISM->SetCullDistances(FMath::Max(0, Layer.StartCullDistance),
			FMath::Max(Layer.StartCullDistance + 1, Layer.EndCullDistance));
		HISM->SetNumCustomDataFloats(5);
		HISM->SetStaticMesh(CardMesh);
		HISM->RegisterComponent();
		HISM->AddInstances(Batch.Transforms, false, false, false);
		for (int32 InstanceIndex = 0; InstanceIndex < Batch.CustomDataA.Num(); ++InstanceIndex)
		{
			const FVector4f& Data = Batch.CustomDataA[InstanceIndex];
			HISM->SetCustomDataValue(InstanceIndex, 0, Data.X, false);
			HISM->SetCustomDataValue(InstanceIndex, 1, Data.Y, false);
			HISM->SetCustomDataValue(InstanceIndex, 2, Data.Z, false);
			HISM->SetCustomDataValue(InstanceIndex, 3, Data.W, false);
			HISM->SetCustomDataValue(InstanceIndex, 4, Batch.WindInfluences[InstanceIndex], false);
		}
		HISM->BuildTreeIfOutdated(true, true);

		UMaterialInstanceDynamic* MID = TreeParent ? UMaterialInstanceDynamic::Create(TreeParent, this) : nullptr;
		if (MID)
		{
			MID->SetTextureParameterValue(TEXT("TreeAtlas"), Batch.Atlas);
			MID->SetScalarParameterValue(TEXT("AtlasColumns"), FMath::Max(1, Biome.Trees.AtlasColumns));
			MID->SetScalarParameterValue(TEXT("AtlasRows"), FMath::Max(1, Biome.Trees.AtlasRows));
			MID->SetVectorParameterValue(TEXT("TreeTint"), Biome.Trees.Tint);
			MID->SetScalarParameterValue(TEXT("TreeBrightness"), Biome.Trees.Brightness);
			HISM->SetMaterial(0, MID);
			TreeMaterials.Add(MID);
		}
		else if (BatchIndex == 0)
		{
			UE_LOG(LogPWG, Warning, TEXT("Tree parts were generated for %s, but Trees.Material is not assigned yet."),
				*ActiveBiomeRow.ToString());
		}
		TreeInstanceComponents.Add(HISM);
	}
	UE_LOG(LogPWG, Verbose, TEXT("Generated %d tree-part batches for biome %s."),
		TreeInstanceComponents.Num(), *ActiveBiomeRow.ToString());
	UpdateGrassMaterialParameters();
}

void APWG_Generator::UpdateGrassMaterialParameters()
{
	if ((GrassMaterials.IsEmpty() && BlockOverhangMaterials.IsEmpty() && TreeMaterials.IsEmpty()) || !GetWorld()) return;
	const FBiomeData* Biome = FindBiomeData(ActiveBiomeRow, TEXT("PWG grass update"));
	if (!Biome) return;
	const float Time = GetWorld()->GetTimeSeconds();
	const float DirectionPeriod = FMath::Max(10.0f, Biome->Wind.DirectionCycleSeconds);
	const float DirectionPhase = static_cast<float>(GetTypeHash(ActiveBiomeRow) % 1000) / 1000.0f * UE_TWO_PI;
	const float Angle = Time / DirectionPeriod * UE_TWO_PI + DirectionPhase;
	const FVector WindDirection(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
	const float StrengthMin = FMath::Min(Biome->Wind.MinimumStrength, Biome->Wind.MaximumStrength);
	const float StrengthMax = FMath::Max(Biome->Wind.MinimumStrength, Biome->Wind.MaximumStrength);
	const float StrengthWave = 0.5f + 0.5f * FMath::Sin(Time / FMath::Max(5.0f,
		Biome->Wind.StrengthCycleSeconds) * UE_TWO_PI + DirectionPhase * 0.37f);
	const float WindStrength = FMath::Lerp(StrengthMin, StrengthMax, StrengthWave);
	APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	const FVector InteractionPosition = Pawn ? Pawn->GetActorLocation() : FVector(0.0f, 0.0f, -BIG_NUMBER);
	const FVector InteractionVelocity = Pawn ? Pawn->GetVelocity() : FVector::ZeroVector;
	for (UMaterialInstanceDynamic* MID : GrassMaterials)
	{
		if (!MID) continue;
		MID->SetVectorParameterValue(TEXT("WindDirection"), WindDirection);
		MID->SetScalarParameterValue(TEXT("WindStrength"), WindStrength);
		MID->SetVectorParameterValue(TEXT("InteractionPosition"), InteractionPosition);
		MID->SetVectorParameterValue(TEXT("InteractionVelocity"), InteractionVelocity);
		MID->SetScalarParameterValue(TEXT("InteractionRadius"), Biome->Grass.InteractionRadius);
		MID->SetScalarParameterValue(TEXT("InteractionStrength"), Biome->Grass.InteractionStrength);
		MID->SetScalarParameterValue(TEXT("PixelWorldStep"), Biome->Grass.PixelWorldStep);
	}
	for (UMaterialInstanceDynamic* MID : TreeMaterials)
	{
		if (!MID) continue;
		// Tree cards live in the X/Z presentation plane. Removing the Y component
		// preserves wind sway without allowing WPO to reorder overlapping leaves in depth.
		MID->SetVectorParameterValue(TEXT("WindDirection"), FVector(WindDirection.X, 0.0f, 0.0f));
		MID->SetScalarParameterValue(TEXT("WindStrength"), WindStrength);
	}
	for (UMaterialInstanceDynamic* MID : BlockOverhangMaterials)
	{
		if (!MID) continue;
		// Overhang may sway only across the visible X/Z plane. Depth movement causes
		// the card to pass through the terrain facade and become visibly clipped.
		MID->SetVectorParameterValue(TEXT("WindDirection"), FVector(WindDirection.X, 0.0f, 0.0f));
		MID->SetScalarParameterValue(TEXT("WindStrength"),
			WindStrength * FMath::Max(0.0f, Biome->BlockOverhang.WindStrengthMultiplier));
		MID->SetVectorParameterValue(TEXT("InteractionVelocity"), FVector::ZeroVector);
		MID->SetScalarParameterValue(TEXT("InteractionRadius"), 0.0f);
		MID->SetScalarParameterValue(TEXT("InteractionStrength"), 0.0f);
	}
}

void APWG_Generator::AddTexturedQuad(FMeshBuffers& B, const FVector& A, const FVector& Bp,
	const FVector& C, const FVector& D, const FVector2D& UV0, const FVector2D& UV1) const
{
	const int32 Base = B.Vertices.Num();
	B.Vertices.Append({ A, Bp, C, D });
	B.Triangles.Append({ Base, Base + 1, Base + 2, Base, Base + 2, Base + 3 });
	for (int32 Index = 0; Index < 4; ++Index)
	{
		B.Normals.Add(-FVector::YAxisVector);
		B.Colors.Add(FColor::White);
		B.Tangents.Add(FProcMeshTangent(FVector::XAxisVector, false));
	}
	B.UVs.Append({ FVector2D(UV0.X, UV1.Y), FVector2D(UV1.X, UV1.Y),
		FVector2D(UV1.X, UV0.Y), FVector2D(UV0.X, UV0.Y) });
}

UMaterialInstanceDynamic* APWG_Generator::CreateEnvironmentSection_UNUSED_BROKEN(UProceduralMeshComponent* Mesh,
	const int32 SectionIndex, const FMeshBuffers& Buffers, const FBiomeData& Biome, UTexture2D* Texture,
	const FLinearColor& Tint, const float Brightness, const bool bVirtualTexture, const float ScrollSpeed)
{
	if (!Mesh || !Texture || Buffers.Vertices.IsEmpty()) return nullptr;
	UMaterialInterface* Parent = (bVirtualTexture
		? Biome.EnvironmentVirtualTextureMaterial.LoadSynchronous()
		: Biome.EnvironmentMaterial.LoadSynchronous());
	// Environment materials are shared infrastructure. Allow later biomes to
	// inherit them from the first configured biome instead of duplicating refs.
	if (!Parent && !StreamingBiomeRows.IsEmpty())
	{
		if (const FBiomeData* SharedDefaults = FindBiomeData(StreamingBiomeRows[0], TEXT("PWG shared environment material")))
		{
			Parent = bVirtualTexture
				? SharedDefaults->EnvironmentVirtualTextureMaterial.LoadSynchronous()
				: SharedDefaults->EnvironmentMaterial.LoadSynchronous();
		}
	}
	if (!Parent)
	{
		UE_LOG(LogPWG, Warning, TEXT("Cannot render PWG environment texture %s: environment material is not assigned in biome %s."),
			*Texture->GetName(), *ActiveBiomeRow.ToString());
		return nullptr;
	}
	Mesh->CreateMeshSection(SectionIndex, Buffers.Vertices, Buffers.Triangles, Buffers.Normals,
		Buffers.UVs, Buffers.Colors, Buffers.Tangents, false);
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Parent, this);
	MID->SetTextureParameterValue(TEXT("SpriteTexture"), Texture);
	MID->SetVectorParameterValue(TEXT("SpriteTint"), Tint);
	MID->SetScalarParameterValue(TEXT("SpriteBrightness"), FMath::Max(0.0f, Brightness));
	MID->SetScalarParameterValue(TEXT("ScrollSpeed"), ScrollSpeed);
	Mesh->SetMaterial(SectionIndex, MID);
	EnvironmentMaterials.Add(MID);
	return MID;
}

void APWG_Generator::BuildChunkEnvironment_UNUSED_BROKEN(const FBiomeData& Biome, FRandomStream& Random)
{
	bHasLandscapeParallaxAnchor = false;
	EnvironmentMesh->ClearAllMeshSections();
	TerrainSceneryMesh->ClearAllMeshSections();
	VegetationMesh->ClearAllMeshSections();
	for (UProceduralMeshComponent* LayerMesh : BackgroundLayerMeshes)
	{
		if (LayerMesh)
		{
			LayerMesh->SetRelativeLocation(FVector::ZeroVector);
			LayerMesh->ClearAllMeshSections();
		}
	}
	for (UProceduralMeshComponent* SpriteMesh : BackgroundSpriteLayerMeshes)
	{
		if (SpriteMesh)
		{
			SpriteMesh->SetRelativeLocation(FVector::ZeroVector);
			SpriteMesh->ClearAllMeshSections();
		}
	}
	int32 Section = 0;
	LandscapeMaterials.SetNum(3);
	LandscapeRepeatWidths.Init(1.0f, 3);
	const float ChunkWidth = SurfaceHeightTiles.Num() * ActiveTileSize;
	const float X0 = ChunkStartLocalX;

	const FPWGBackgroundLayer* LandscapeLayers[] = {
		&Biome.LandscapeClose, &Biome.LandscapeMid, &Biome.LandscapeFar
	};
	UTexture2D* LandscapeTextures[3] = { nullptr, nullptr, nullptr };
	const TArray<float>* LandscapeSilhouettes[3] = { nullptr, nullptr, nullptr };
	for (int32 LayerIndex = 0; LayerIndex < UE_ARRAY_COUNT(LandscapeLayers); ++LayerIndex)
	{
		LandscapeTextures[LayerIndex] = LandscapeLayers[LayerIndex]->Texture.LoadSynchronous();
		LandscapeSilhouettes[LayerIndex] = GetLandscapeSilhouette(
			LandscapeTextures[LayerIndex], LandscapeLayers[LayerIndex]->SilhouetteAlphaThreshold);
	}
	for (int32 LayerIndex = 0; LayerIndex < UE_ARRAY_COUNT(LandscapeLayers); ++LayerIndex)
	{
		const FPWGBackgroundLayer& Layer = *LandscapeLayers[LayerIndex];
		UTexture2D* Texture = LandscapeTextures[LayerIndex];
		if (!Texture) continue;
		const float Y = ChunkHalfDepth + Layer.DistanceBehindTerrain;
		const float CentreZ = Biome.BaseSurfaceHeightInTiles * ActiveTileSize
			+ Layer.VerticalOffset + LandscapeVerticalFramingOffset;
		FMeshBuffers Mesh;
		const float Aspect = static_cast<float>(FMath::Max(1, Texture->GetSizeX())) / FMath::Max(1, Texture->GetSizeY());
		const float Height = FMath::Max(10.0f, Layer.WorldHeight);
		const float RepeatWidth = Layer.WorldWidth > 0.0f
			? FMath::Max(10.0f, Layer.WorldWidth)
			: FMath::Max(10.0f, Height * Aspect);
		const double GlobalChunkStart = static_cast<double>(GlobalColumnOffset) * ActiveTileSize;
		const double FirstRepeat = FMath::FloorToDouble(GlobalChunkStart / RepeatWidth) * RepeatWidth;
		for (double GlobalX = FirstRepeat; GlobalX < GlobalChunkStart + ChunkWidth; GlobalX += RepeatWidth)
		{
			const float RepeatX0 = X0 + static_cast<float>(GlobalX - GlobalChunkStart);
			const float RepeatX1 = RepeatX0 + RepeatWidth;
			AddTexturedQuad(Mesh, FVector(RepeatX0,Y,CentreZ-Height*0.5f), FVector(RepeatX1,Y,CentreZ-Height*0.5f),
				FVector(RepeatX1,Y,CentreZ+Height*0.5f), FVector(RepeatX0,Y,CentreZ+Height*0.5f),
				FVector2D(0.0f,0.0f), FVector2D(1.0f,1.0f));
		}
		UProceduralMeshComponent* LayerMesh = BackgroundLayerMeshes.IsValidIndex(LayerIndex)
			? BackgroundLayerMeshes[LayerIndex].Get() : EnvironmentMesh.Get();
		FLinearColor LayerTint = FMath::Lerp(Layer.Tint, Layer.DistanceFadeColor,
			FMath::Clamp(Layer.DistanceFadeAmount, 0.0f, 1.0f));
		LayerTint.A = Layer.Tint.A;
		LandscapeRepeatWidths[LayerIndex] = RepeatWidth;
		LandscapeMaterials[LayerIndex] = CreateEnvironmentSection_UNUSED_BROKEN(LayerMesh, 0, Mesh, Biome, Texture, LayerTint,
			Layer.Brightness, Texture->GetName().EndsWith(TEXT("_38VT"), ESearchCase::IgnoreCase), Layer.ScrollSpeed);
	}

	auto GetSceneryBaseZ = [&](const int32 LayerIndex, const float LocalX)
	{
		const FPWGBackgroundLayer& Layer = *LandscapeLayers[LayerIndex];
		UTexture2D* LandscapeTexture = LandscapeTextures[LayerIndex];
		const float Height = FMath::Max(10.0f, Layer.WorldHeight);
		float SilhouetteHeight = FMath::Clamp(Layer.FallbackSilhouetteHeight, 0.0f, 1.0f);
		if (LandscapeTexture && LandscapeSilhouettes[LayerIndex] && !LandscapeSilhouettes[LayerIndex]->IsEmpty())
		{
			const float Aspect = static_cast<float>(FMath::Max(1, LandscapeTexture->GetSizeX())) /
				FMath::Max(1, LandscapeTexture->GetSizeY());
			const double RepeatWidth = Layer.WorldWidth > 0.0f
				? FMath::Max(10.0, static_cast<double>(Layer.WorldWidth))
				: FMath::Max(10.0, static_cast<double>(Height * Aspect));
			const double GlobalX = static_cast<double>(GlobalColumnOffset) * ActiveTileSize +
				static_cast<double>(LocalX - ChunkStartLocalX);
			double WrappedX = FMath::Fmod(GlobalX, RepeatWidth);
			if (WrappedX < 0.0) WrappedX += RepeatWidth;
			const float U = static_cast<float>(WrappedX / RepeatWidth);
			const int32 PixelX = FMath::Clamp(FMath::FloorToInt(U * LandscapeSilhouettes[LayerIndex]->Num()),
				0, LandscapeSilhouettes[LayerIndex]->Num() - 1);
			SilhouetteHeight = (*LandscapeSilhouettes[LayerIndex])[PixelX];
		}
		const float CentreZ = Biome.BaseSurfaceHeightInTiles * ActiveTileSize
			+ Layer.VerticalOffset + LandscapeVerticalFramingOffset;
		const float PixelWorldHeight = LandscapeTexture
			? Height / FMath::Max(1, LandscapeTexture->GetSizeY()) : 0.0f;
		return CentreZ - Height * 0.5f + SilhouetteHeight * Height
			- Layer.SceneryBurialPixels * PixelWorldHeight;
	};

	int32 SpriteSections[3] = { 0, 0, 0 };
	auto AddSceneryRules = [&](const TArray<FPWGScenerySpriteRule>& Rules)
	{
		for (const FPWGScenerySpriteRule& Rule : Rules)
		{
			UTexture2D* Texture = Rule.Texture.LoadSynchronous();
			if (!Texture || Rule.ChancePerSurfaceTile <= 0.0f) continue;
			FMeshBuffers LayerBuffers[3];
			for (int32 X = 0; X < SurfaceHeightTiles.Num(); ++X)
			{
				if (Random.FRand() > Rule.ChancePerSurfaceTile) continue;
				const int32 MinLayer = FMath::Clamp(FMath::Min(Rule.MinimumLayer, Rule.MaximumLayer), 0, 2);
				const int32 MaxLayer = FMath::Clamp(FMath::Max(Rule.MinimumLayer, Rule.MaximumLayer), MinLayer, 2);
				const int32 SelectedLayer = Random.RandRange(MinLayer, MaxLayer);
				const FPWGBackgroundLayer& Layer = *LandscapeLayers[SelectedLayer];
				const float Width = Random.FRandRange(FMath::Min(Rule.MinimumSize.X, Rule.MaximumSize.X), FMath::Max(Rule.MinimumSize.X, Rule.MaximumSize.X));
				const float Height = Random.FRandRange(FMath::Min(Rule.MinimumSize.Y, Rule.MaximumSize.Y), FMath::Max(Rule.MinimumSize.Y, Rule.MaximumSize.Y));
				const float CentreX = ChunkStartLocalX + (X + Random.FRand()) * ActiveTileSize;
				const float BaseZ = GetSceneryBaseZ(SelectedLayer, CentreX) + Rule.HeightOffset;
				// One centimetre behind the landscape keeps both at effectively identical
				// parallax while allowing its opaque pixels to cover the sprite root.
				const float Y = ChunkHalfDepth + Layer.DistanceBehindTerrain + 1.0f;
				const bool b38 = Rule.bDirectional38Atlas || Texture->GetName().EndsWith(TEXT("_38VT"), ESearchCase::IgnoreCase);
				FVector2D UV0(0.0f, 0.0f), UV1(1.0f, 1.0f);
				if (b38)
				{
					const int32 Frame = FMath::Clamp(Rule.FixedFrameIndex, 0, 37);
					constexpr float UStep = 400.0f / 4096.0f;
					constexpr float VStep = 400.0f / 2048.0f;
					UV0 = FVector2D((Frame % 8) * UStep, (Frame / 8) * VStep);
					UV1 = UV0 + FVector2D(UStep, VStep);
				}
				AddTexturedQuad(LayerBuffers[SelectedLayer], FVector(CentreX-Width*0.5f,Y,BaseZ), FVector(CentreX+Width*0.5f,Y,BaseZ),
					FVector(CentreX+Width*0.5f,Y,BaseZ+Height), FVector(CentreX-Width*0.5f,Y,BaseZ+Height), UV0, UV1);
			}
			const bool b38 = Rule.bDirectional38Atlas || Texture->GetName().EndsWith(TEXT("_38VT"), ESearchCase::IgnoreCase);
			for (int32 LayerIndex = 0; LayerIndex < 3; ++LayerIndex)
			{
				if (BackgroundSpriteLayerMeshes.IsValidIndex(LayerIndex))
				{
					const FPWGBackgroundLayer& Layer = *LandscapeLayers[LayerIndex];
					FLinearColor SpriteTint = Rule.Tint;
					float SpriteBrightness = Rule.Brightness;
					if (Rule.bInheritLandscapeAtmosphere)
					{
						FLinearColor Atmosphere = FMath::Lerp(Layer.Tint, Layer.DistanceFadeColor,
							FMath::Clamp(Layer.DistanceFadeAmount, 0.0f, 1.0f));
						Atmosphere.A = 1.0f;
						SpriteTint *= Atmosphere;
						SpriteBrightness *= Layer.Brightness;
					}
					CreateEnvironmentSection_UNUSED_BROKEN(BackgroundSpriteLayerMeshes[LayerIndex], SpriteSections[LayerIndex]++,
						LayerBuffers[LayerIndex], Biome, Texture, SpriteTint, SpriteBrightness, b38, 0.0f);
				}
			}
		}
	};
	AddSceneryRules(Biome.BackgroundSpriteRules);
	AddSceneryRules(Biome.BackgroundDetailRules);

	// Lightweight terrain scenery: all instances sharing a rule are batched into
	// one procedural-mesh section; no AActor is spawned per tree, bush or rock.
	int32 TerrainScenerySection = 0;
	auto AddTerrainScenery = [&](const TArray<FPWGTerrainSpriteRule>& Rules, const bool bInsideDepthStrip)
	{
		for (const FPWGTerrainSpriteRule& Rule : Rules)
		{
			UTexture2D* Texture = Rule.Texture.LoadSynchronous();
			if (!Texture || Rule.ChancePerSurfaceTile <= 0.0f) continue;
			FMeshBuffers Mesh;
			for (int32 X = 0; X < SurfaceHeightTiles.Num(); ++X)
			{
				if (Random.FRand() > Rule.ChancePerSurfaceTile) continue;
				const float Width = Random.FRandRange(FMath::Min(Rule.MinimumSize.X, Rule.MaximumSize.X),
					FMath::Max(Rule.MinimumSize.X, Rule.MaximumSize.X));
				const float Height = Random.FRandRange(FMath::Min(Rule.MinimumSize.Y, Rule.MaximumSize.Y),
					FMath::Max(Rule.MinimumSize.Y, Rule.MaximumSize.Y));
				const float CentreX = ChunkStartLocalX + (X + Random.FRand()) * ActiveTileSize;
				const float MinDepth = FMath::Min(Rule.MinimumDepthOffset, Rule.MaximumDepthOffset);
				const float MaxDepth = FMath::Max(Rule.MinimumDepthOffset, Rule.MaximumDepthOffset);
				const float Depth = Random.FRandRange(MinDepth, MaxDepth);
				const float Y = bInsideDepthStrip
					? FMath::Clamp(Depth, -ChunkHalfDepth + 1.0f, ChunkHalfDepth - 1.0f)
					: ChunkHalfDepth + Depth;
				const float BaseZ = GetSmoothSurfaceHeightAtLocalX(CentreX)
					- FMath::Max(0.0f, Rule.SurfaceBurialDepth) + Rule.HeightOffset;
				FVector2D UV0(0.0f, 0.0f), UV1(1.0f, 1.0f);
				const bool b38 = Rule.bDirectional38Atlas || Texture->GetName().EndsWith(TEXT("_38VT"), ESearchCase::IgnoreCase);
				if (b38)
				{
					const int32 Frame = FMath::Clamp(Rule.FixedFrameIndex, 0, 37);
					constexpr float UStep = 400.0f / 4096.0f;
					constexpr float VStep = 400.0f / 2048.0f;
					UV0 = FVector2D((Frame % 8) * UStep, (Frame / 8) * VStep);
					UV1 = UV0 + FVector2D(UStep, VStep);
				}
				AddTexturedQuad(Mesh, FVector(CentreX-Width*0.5f,Y,BaseZ), FVector(CentreX+Width*0.5f,Y,BaseZ),
					FVector(CentreX+Width*0.5f,Y,BaseZ+Height), FVector(CentreX-Width*0.5f,Y,BaseZ+Height), UV0, UV1);
			}
			const bool b38 = Rule.bDirectional38Atlas || Texture->GetName().EndsWith(TEXT("_38VT"), ESearchCase::IgnoreCase);
			CreateEnvironmentSection_UNUSED_BROKEN(TerrainSceneryMesh, TerrainScenerySection++, Mesh, Biome, Texture,
				Rule.Tint, Rule.Brightness, b38, 0.0f);
		}
	};
	AddTerrainScenery(Biome.TerrainRearSpriteRules, false);
	AddTerrainScenery(Biome.TerrainDepthSpriteRules, true);

	// Fourth, terrain-following landscape. Its lower and upper silhouette deform
	// together, preserving the panorama height instead of vertically stretching it.
	if (UTexture2D* Texture = Biome.LandscapeVegetation.Texture.LoadSynchronous())
	{
		const FPWGVegetationLayer& Layer = Biome.LandscapeVegetation;
		FMeshBuffers Mesh;
		const float Height = FMath::Max(10.0f, Layer.WorldHeight);
		const float Aspect = static_cast<float>(FMath::Max(1, Texture->GetSizeX())) / FMath::Max(1, Texture->GetSizeY());
		const float RepeatWidth = Layer.WorldWidth > 0.0f ? FMath::Max(10.0f, Layer.WorldWidth) : Height * Aspect;
		const float GlobalStart = static_cast<float>(GlobalColumnOffset) * ActiveTileSize;
		const float Y = ChunkHalfDepth + Layer.DistanceBehindTerrain;
		for (int32 Segment = 0; Segment < SteppedSurfaceHeights.Num(); ++Segment)
		{
			const float SX0 = ChunkStartLocalX + Segment * ActiveSurfaceSegmentWidth;
			const float SX1 = SX0 + ActiveSurfaceSegmentWidth;
			const float Z0 = GetSmoothSurfaceHeightAtLocalX(SX0) + Layer.VerticalOffset;
			const float Z1 = GetSmoothSurfaceHeightAtLocalX(SX1) + Layer.VerticalOffset;
			const float U0 = (GlobalStart + Segment * ActiveSurfaceSegmentWidth) / RepeatWidth;
			const float U1 = (GlobalStart + (Segment + 1) * ActiveSurfaceSegmentWidth) / RepeatWidth;
			AddTexturedQuad(Mesh, FVector(SX0,Y,Z0), FVector(SX1,Y,Z1), FVector(SX1,Y,Z1+Height),
				FVector(SX0,Y,Z0+Height), FVector2D(U0,0.0f), FVector2D(U1,1.0f));
		}
		FLinearColor Tint = FMath::Lerp(Layer.Tint, Layer.DistanceFadeColor,
			FMath::Clamp(Layer.DistanceFadeAmount, 0.0f, 1.0f));
		Tint.A = Layer.Tint.A;
		CreateEnvironmentSection_UNUSED_BROKEN(VegetationMesh, 0, Mesh, Biome, Texture, Tint, Layer.Brightness, false, 0.0f);
	}

	TArray<FPWGSurfaceOverhangSettings> OverhangSettings;
	if (!Biome.BlockOverhang.bEnabled && !Biome.SurfaceOverhangs.IsEmpty())
	{
		OverhangSettings = Biome.SurfaceOverhangs;
	}
	else if (!Biome.BlockOverhang.bEnabled && !Biome.SurfaceOverhang.Texture.IsNull())
	{
		OverhangSettings.Add(Biome.SurfaceOverhang);
	}

	for (const FPWGSurfaceOverhangSettings& Overhang : OverhangSettings)
	{
		UTexture2D* Texture = Overhang.Texture.LoadSynchronous();
		if (!Texture) continue;

		FMeshBuffers Mesh;
		const int32 Subdivisions = ActiveSurfaceSegmentsPerTile;
		const int32 SegmentCount = SurfaceHeightTiles.Num() * Subdivisions;
		const float SegmentWidth = ActiveSurfaceSegmentWidth;
		const float OverhangHeight = FMath::Max(1.0f, Overhang.WorldHeight * FMath::Max(0.01f, Overhang.UniformScale));
		const float HalfHeight = OverhangHeight * 0.5f;
		const float TextureAspect = static_cast<float>(FMath::Max(1, Texture->GetSizeX())) / FMath::Max(1, Texture->GetSizeY());
		const float RepeatWidth = FMath::Max(1.0f, OverhangHeight * TextureAspect);
		const float GlobalChunkStart = static_cast<float>(GlobalColumnOffset) * ActiveTileSize;
		const float Y = -ChunkHalfDepth - Overhang.DistanceInFrontOfTerrain;
		const float MinFraction = FMath::Clamp(FMath::Min(Overhang.StartFraction, Overhang.EndFraction), 0.0f, 1.0f);
		const float MaxFraction = FMath::Clamp(FMath::Max(Overhang.StartFraction, Overhang.EndFraction), 0.0f, 1.0f);
		const int32 FirstSegment = FMath::Clamp(FMath::FloorToInt(MinFraction * SegmentCount), 0, SegmentCount);
		const int32 LastSegment = FMath::Clamp(FMath::CeilToInt(MaxFraction * SegmentCount), FirstSegment, SegmentCount);
		for (int32 Segment = FirstSegment; Segment < LastSegment; ++Segment)
		{
			const float SX0 = ChunkStartLocalX + Segment * SegmentWidth;
			const float SX1 = SX0 + SegmentWidth;
			const float Z0 = GetSmoothSurfaceHeightAtLocalX(SX0) + Overhang.VerticalOffset;
			const float Z1 = GetSmoothSurfaceHeightAtLocalX(SX1) + Overhang.VerticalOffset;
			const float U0 = (GlobalChunkStart + Segment * SegmentWidth) / RepeatWidth;
			const float U1 = (GlobalChunkStart + (Segment + 1) * SegmentWidth) / RepeatWidth;
			AddTexturedQuad(Mesh, FVector(SX0,Y,Z0-HalfHeight), FVector(SX1,Y,Z1-HalfHeight),
				FVector(SX1,Y,Z1+HalfHeight), FVector(SX0,Y,Z0+HalfHeight), FVector2D(U0,0.0f), FVector2D(U1,1.0f));
		}
		CreateEnvironmentSection_UNUSED_BROKEN(EnvironmentMesh, Section++, Mesh, Biome, Texture, Overhang.Tint,
			Overhang.Brightness,
			Texture->GetName().EndsWith(TEXT("_38VT"), ESearchCase::IgnoreCase), 0.0f);
	}
}

void APWG_Generator::UpdateLandscapeParallax_UNUSED_BROKEN(const FVector& FollowLocation)
{
	const FBiomeData* Biome = FindBiomeData(ActiveBiomeRow, TEXT("PWG landscape parallax"));
	if (!Biome) return;
	if (!bHasLandscapeParallaxAnchor)
	{
		LandscapeParallaxAnchorViewLocation = FollowLocation;
		bHasLandscapeParallaxAnchor = true;
	}
	const FVector CameraDelta = FollowLocation - LandscapeParallaxAnchorViewLocation;
	const FPWGBackgroundLayer* LandscapeDefinitions[] = {
		&Biome->LandscapeClose, &Biome->LandscapeMid, &Biome->LandscapeFar
	};
	for (int32 LayerIndex = 0; LayerIndex < UE_ARRAY_COUNT(LandscapeDefinitions); ++LayerIndex)
	{
		if (!LandscapeMaterials.IsValidIndex(LayerIndex) || !LandscapeMaterials[LayerIndex]
			|| !LandscapeRepeatWidths.IsValidIndex(LayerIndex)) continue;
		const float MovementScale = FMath::Clamp(LandscapeDefinitions[LayerIndex]->CameraFollowFactor, 0.0f, 0.95f);
		const float CameraFollowAmount = 1.0f - MovementScale;
		// Move the complete landscape layer with the camera. This keeps its alpha
		// silhouette and every attached sprite/detail in the same coordinate space.
		const FVector ComponentOffset(CameraDelta.X * CameraFollowAmount, 0.0f,
			CameraDelta.Z * CameraFollowAmount);
		if (BackgroundLayerMeshes.IsValidIndex(LayerIndex) && BackgroundLayerMeshes[LayerIndex])
		{
			BackgroundLayerMeshes[LayerIndex]->SetRelativeLocation(ComponentOffset);
		}
		LandscapeMaterials[LayerIndex]->SetVectorParameterValue(
			TEXT("UVOffset"), FLinearColor::Black);
		LandscapeMaterials[LayerIndex]->SetScalarParameterValue(
			TEXT("ScrollSpeed"), LandscapeDefinitions[LayerIndex]->ScrollSpeed);
		if (BackgroundSpriteLayerMeshes.IsValidIndex(LayerIndex) && BackgroundSpriteLayerMeshes[LayerIndex])
		{
			BackgroundSpriteLayerMeshes[LayerIndex]->SetRelativeLocation(ComponentOffset);
		}
	}
}

void APWG_Generator::BuildGlobalEnvironment_UNUSED_BROKEN(const FBiomeData& Biome)
{
	SkyMesh->ClearAllMeshSections();
	SunMesh->ClearAllMeshSections();
	MoonMesh->ClearAllMeshSections();
	CloudMesh->ClearAllMeshSections();
	ActiveClouds.Reset();
	ActiveCloudRules.Reset();
	ActiveCloudTexture = nullptr;
	EnvironmentMaterials.Reset();
	LandscapeMaterials.Reset();
	LandscapeRepeatWidths.Reset();
	SkyMaterial = nullptr;
	ActiveSkyBiome = Biome;
	ActiveSkyBiomeRow = GetBiomeRowForChunk(GetChunkIndexAtWorldX(
		UGameplayStatics::GetPlayerPawn(this, 0) ? UGameplayStatics::GetPlayerPawn(this, 0)->GetActorLocation().X : GetActorLocation().X));

	auto BuildBillboard = [&](UProceduralMeshComponent* Mesh, UTexture2D* Texture, const FVector2D Size,
		const int32 Section, const float ScrollSpeed) -> UMaterialInstanceDynamic*
	{
		if (!Texture) return nullptr;
		FMeshBuffers Buffers;
		AddTexturedQuad(Buffers, FVector(-Size.X*0.5f,0.0f,-Size.Y*0.5f), FVector(Size.X*0.5f,0.0f,-Size.Y*0.5f),
			FVector(Size.X*0.5f,0.0f,Size.Y*0.5f), FVector(-Size.X*0.5f,0.0f,Size.Y*0.5f), FVector2D::ZeroVector, FVector2D(1.0f,1.0f));
		return CreateEnvironmentSection_UNUSED_BROKEN(Mesh, Section, Buffers, Biome, Texture, Biome.SkyTint, 1.0f, false, ScrollSpeed);
	};
	UTexture2D* SharedSky = SharedSkyPanoramaTexture.LoadSynchronous();
	if (!SharedSky && !StreamingBiomeRows.IsEmpty())
	{
		if (const FBiomeData* FirstBiome = FindBiomeData(StreamingBiomeRows[0], TEXT("PWG shared sky texture")))
		{
			SharedSky = FirstBiome->SkyPanoramaTexture.LoadSynchronous();
		}
	}
	if (!SharedSky)
	{
		SharedSky = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Systems/Sprite2Bone/Textures/Sky/15.15"));
	}
	const float SkyWidth = FMath::Max(StreamingChunkLength * (FMath::Max(1, PreloadRadiusInChunks) * 2 + 1), 1000.0f);
	SkyMaterial = BuildBillboard(SkyMesh, SharedSky, FVector2D(SkyWidth, SharedSkyHeight), 0, SharedSkyScrollSpeed);
	SkyMesh->SetHiddenInGame(false, true);
	SkyMesh->SetVisibility(true, true);
	SkyMesh->SetBoundsScale(10.0f);
	if (!SharedSky)
	{
		UE_LOG(LogPWG, Warning, TEXT("Shared Sky is invisible because neither SharedSkyPanoramaTexture nor the first biome SkyPanoramaTexture is assigned."));
	}
	BuildBillboard(SunMesh, Biome.SunTexture.LoadSynchronous(), Biome.SunSize, 0, 0.0f);
	BuildBillboard(MoonMesh, Biome.MoonTexture.LoadSynchronous(), Biome.MoonSize, 0, 0.0f);

	ActiveCloudRules = Biome.CloudRules;
	if (ActiveCloudRules.IsEmpty() && !Biome.Clouds.Texture.IsNull()) ActiveCloudRules.Add(Biome.Clouds);
	FRandomStream CloudRandom(HashCombineFast(static_cast<uint32>(ActiveWorldSeed), 0xC10D5u));
	for (int32 RuleIndex = 0; RuleIndex < ActiveCloudRules.Num(); ++RuleIndex)
	{
		const FPWGCloudSettings& Rule = ActiveCloudRules[RuleIndex];
		UTexture2D* CloudTexture = Rule.Texture.LoadSynchronous();
		if (!CloudTexture || Rule.Count <= 0) continue;
		if (!ActiveCloudTexture) ActiveCloudTexture = CloudTexture;
		FMeshBuffers CloudBuffers;
		for (int32 Index = 0; Index < Rule.Count; ++Index)
		{
			FCloudInstance& Cloud = ActiveClouds.AddDefaulted_GetRef();
			Cloud.RuleIndex = RuleIndex;
			Cloud.X = CloudRandom.FRandRange(-SkyWidth * 0.5f, SkyWidth * 0.5f);
			Cloud.Z = CloudRandom.FRandRange(FMath::Min(Rule.HeightRange.X, Rule.HeightRange.Y),
				FMath::Max(Rule.HeightRange.X, Rule.HeightRange.Y));
			Cloud.Speed = CloudRandom.FRandRange(FMath::Min(Rule.SpeedRange.X, Rule.SpeedRange.Y),
				FMath::Max(Rule.SpeedRange.X, Rule.SpeedRange.Y));
			Cloud.Size.X = CloudRandom.FRandRange(FMath::Min(Rule.MinimumSize.X, Rule.MaximumSize.X),
				FMath::Max(Rule.MinimumSize.X, Rule.MaximumSize.X));
			Cloud.Size.Y = CloudRandom.FRandRange(FMath::Min(Rule.MinimumSize.Y, Rule.MaximumSize.Y),
				FMath::Max(Rule.MinimumSize.Y, Rule.MaximumSize.Y));
			const float CloudY = ChunkHalfDepth + Rule.DepthBehindTerrain;
			AddTexturedQuad(CloudBuffers,
				FVector(Cloud.X-Cloud.Size.X*0.5f,CloudY,Cloud.Z-Cloud.Size.Y*0.5f),
				FVector(Cloud.X+Cloud.Size.X*0.5f,CloudY,Cloud.Z-Cloud.Size.Y*0.5f),
				FVector(Cloud.X+Cloud.Size.X*0.5f,CloudY,Cloud.Z+Cloud.Size.Y*0.5f),
				FVector(Cloud.X-Cloud.Size.X*0.5f,CloudY,Cloud.Z+Cloud.Size.Y*0.5f),
				FVector2D::ZeroVector, FVector2D(1.0f,1.0f));
		}
		CreateEnvironmentSection_UNUSED_BROKEN(CloudMesh, RuleIndex, CloudBuffers, Biome, CloudTexture,
			Rule.Tint, Rule.Brightness, false, 0.0f);
	}
}

void APWG_Generator::UpdateGlobalEnvironment_UNUSED_BROKEN(const float DeltaSeconds)
{
	APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Pawn) return;
	const int32 ChunkIndex = GetChunkIndexAtWorldX(Pawn->GetActorLocation().X);
	const FName BiomeRow = GetBiomeRowForChunk(ChunkIndex);
	if (BiomeRow != ActiveSkyBiomeRow)
	{
		if (const FBiomeData* Biome = FindBiomeData(BiomeRow, TEXT("PWG sky biome")))
		{
			BuildGlobalEnvironment_UNUSED_BROKEN(*Biome);
		}
	}

	DayCycleClock = FMath::Fmod(DayCycleClock + DeltaSeconds, FMath::Max(10.0f, ActiveSkyBiome.DayCycleDurationSeconds));
	const float Angle = DayCycleClock / FMath::Max(10.0f, ActiveSkyBiome.DayCycleDurationSeconds) * UE_TWO_PI;
	const FVector Follow = Pawn->GetActorLocation();
	SkyMesh->SetWorldLocation(FVector(Follow.X, GetActorLocation().Y + ChunkHalfDepth + SharedSkyDepth,
		Follow.Z + SharedSkyVerticalOffset));
	if (SkyMaterial && StreamingChunkLength > KINDA_SMALL_NUMBER)
	{
		const float SkyWidth = StreamingChunkLength * (FMath::Max(1, PreloadRadiusInChunks) * 2 + 1);
		const float ParallaxU = Follow.X / FMath::Max(1.0f, SkyWidth) * FMath::Clamp(SkyParallaxFactor, 0.0f, 0.25f);
		SkyMaterial->SetVectorParameterValue(TEXT("UVOffset"), FLinearColor(ParallaxU, 0.0f, 0.0f, 0.0f));
	}
	const float CelestialY = GetActorLocation().Y + ChunkHalfDepth + SharedSkyDepth - 100.0f;
	const FVector OrbitCentre(Follow.X + ActiveSkyBiome.CelestialScreenOffset.X, CelestialY,
		Follow.Z + ActiveSkyBiome.CelestialVerticalOffset + ActiveSkyBiome.CelestialScreenOffset.Y);
	const FVector SunOffset(FMath::Cos(Angle) * ActiveSkyBiome.CelestialOrbitSize.X, 0.0f,
		FMath::Sin(Angle) * ActiveSkyBiome.CelestialOrbitSize.Y);
	SunMesh->SetWorldLocation(OrbitCentre + SunOffset);
	MoonMesh->SetWorldLocation(OrbitCentre - SunOffset);
	SunMesh->SetVisibility(FMath::Sin(Angle) >= 0.0f, true);
	MoonMesh->SetVisibility(FMath::Sin(Angle) < 0.0f, true);

	if (!ActiveCloudRules.IsEmpty() && !ActiveClouds.IsEmpty())
	{
		const float SkyWidth = FMath::Max(StreamingChunkLength * (FMath::Max(1, PreloadRadiusInChunks) * 2 + 1), 1000.0f);
		TArray<FMeshBuffers> CloudBuffers;
		CloudBuffers.SetNum(ActiveCloudRules.Num());
		for (FCloudInstance& Cloud : ActiveClouds)
		{
			Cloud.X += Cloud.Speed * DeltaSeconds;
			if (Cloud.X - Cloud.Size.X * 0.5f > SkyWidth * 0.5f) Cloud.X = -SkyWidth * 0.5f - Cloud.Size.X * 0.5f;
			if (!CloudBuffers.IsValidIndex(Cloud.RuleIndex)) continue;
			const float CloudY = ChunkHalfDepth + ActiveCloudRules[Cloud.RuleIndex].DepthBehindTerrain;
			AddTexturedQuad(CloudBuffers[Cloud.RuleIndex],
				FVector(Cloud.X-Cloud.Size.X*0.5f,CloudY,Cloud.Z-Cloud.Size.Y*0.5f),
				FVector(Cloud.X+Cloud.Size.X*0.5f,CloudY,Cloud.Z-Cloud.Size.Y*0.5f),
				FVector(Cloud.X+Cloud.Size.X*0.5f,CloudY,Cloud.Z+Cloud.Size.Y*0.5f),
				FVector(Cloud.X-Cloud.Size.X*0.5f,CloudY,Cloud.Z+Cloud.Size.Y*0.5f),
				FVector2D::ZeroVector, FVector2D(1.0f,1.0f));
		}
		for (int32 RuleIndex = 0; RuleIndex < CloudBuffers.Num(); ++RuleIndex)
		{
			CloudMesh->SetWorldLocation(FVector(Follow.X, GetActorLocation().Y, Follow.Z));
			CloudMesh->UpdateMeshSection(RuleIndex, CloudBuffers[RuleIndex].Vertices, CloudBuffers[RuleIndex].Normals,
				CloudBuffers[RuleIndex].UVs, CloudBuffers[RuleIndex].Colors, CloudBuffers[RuleIndex].Tangents);
		}
	}
}

void APWG_Generator::BuildTraversalLedges(const FBiomeData& Biome)
{
	UWorld* World = GetWorld();
	if (!World) return;
	const EPWGTerrainLane Candidates[]={EPWGTerrainLane::Front,EPWGTerrainLane::Middle,EPWGTerrainLane::Back};
	for(const EPWGTerrainLane Lane:Candidates)
	{
		if(!IsLaneEnabledLocal(Lane)) continue;
		const TArray<float>& Heights=GetLaneSteppedHeights(Lane);
		for (int32 Segment = 1; Segment < Heights.Num(); ++Segment)
		{
			const float LeftZ=Heights[Segment-1],RightZ=Heights[Segment];
			const float Difference=FMath::Abs(LeftZ-RightZ);
			if(Difference<100.0f-KINDA_SMALL_NUMBER) continue;
			const float BoundaryLocalX=ChunkStartLocalX+Segment*ActiveSurfaceSegmentWidth;
			const bool bHighOnLeft=LeftZ>RightZ;
			const float HighZ=FMath::Max(LeftZ,RightZ),LowZ=FMath::Min(LeftZ,RightZ);
			const FVector TowardHighLocal=bHighOnLeft?-FVector::XAxisVector:FVector::XAxisVector;
			const FVector OutwardLocal=-TowardHighLocal;
			const float LaneY=GetLaneCenterYLocal(Lane);
			const FVector FrontLocal(BoundaryLocalX,LaneY,HighZ);
			const FVector BackLocal=FrontLocal+TowardHighLocal*Biome.BackLedgeInset;
			const FVector WorldFront=GetActorTransform().TransformPosition(FrontLocal);
			const FVector WorldBack=GetActorTransform().TransformPosition(BackLocal);
			const FRotator LedgeRotation=GetActorTransform().TransformVectorNoScale(OutwardLocal).GetSafeNormal().Rotation();
			const FVector WallCenter=GetActorTransform().TransformPosition(FVector(BoundaryLocalX,LaneY,(HighZ+LowZ)*0.5f));
			const FVector WallExtent(Biome.LedgeDetectionThickness*0.5f,ActiveTileSize*0.48f,Difference*0.5f);
			if(APWGLedgeActor* Ledge=World->SpawnActor<APWGLedgeActor>(APWGLedgeActor::StaticClass(),WallCenter,FRotator::ZeroRotator))
			{
				Ledge->AttachToActor(this,FAttachmentTransformRules::KeepWorldTransform);
				Ledge->ConfigureLedge(WallCenter,WallExtent,FTransform(LedgeRotation,WorldFront),FTransform(LedgeRotation,WorldBack));
				SpawnedLedges.Add(Ledge);
			}
		}
	}
}

void APWG_Generator::SpawnBiomeObjects(const FBiomeData& Biome, FRandomStream& Random)
{
	UWorld* World = GetWorld();
	if (!World) return;
	const int32 Center = SurfaceHeightTiles.Num() / 2;
	for (const FPWGObjectSpawnRule& Rule : Biome.ObjectSpawnRules)
	{
		UClass* Class = Rule.ActorClass.LoadSynchronous();
		if (!Class || Rule.ChancePerSurfaceTile <= 0.0f) continue;
		int32 LastSpawn = MIN_int32 / 2;
		for (int32 X = 0; X < SurfaceHeightTiles.Num(); ++X)
		{
			if (FMath::Abs(X - Center) <= 3 || X - LastSpawn < Rule.MinimumSpacingInTiles || Random.FRand() > Rule.ChancePerSurfaceTile) continue;
			const FVector LocalLocation(ChunkStartLocalX + (X + 0.5f) * ActiveTileSize, 0.0f, GetLocalSurfaceHeightByIndex(X));
			const FVector WorldLocation = GetActorTransform().TransformPosition(LocalLocation) + Rule.SpawnOffset;
			if (AActor* Spawned = World->SpawnActor<AActor>(Class, WorldLocation, GetActorRotation()))
			{
				SpawnedActors.Add(Spawned);
				LastSpawn = X;
			}
		}
	}
}

float APWG_Generator::GetSurfaceHeightAt(const float X, const float Y) const
{
	if (bEnableChunkStreaming && !bStreamingChild)
	{
		const int32 ChunkIndex = GetChunkIndexAtWorldX(X);
		if (const APWG_Generator* Chunk = LoadedChunks.FindRef(ChunkIndex))
		{
			return Chunk->GetSurfaceHeightAt(X, Y);
		}
		return -BIG_NUMBER;
	}
	if (!bWorldGenerated || SurfaceHeightTiles.IsEmpty()) return -BIG_NUMBER;
	const FVector Local = GetActorTransform().InverseTransformPosition(FVector(X, Y, GetActorLocation().Z));
	EPWGTerrainLane Lane=EPWGTerrainLane::Middle;
	float Best=FMath::Abs(Local.Y);
	if(bFrontLaneEnabled&&FMath::Abs(Local.Y+ActiveTileSize)<Best){Lane=EPWGTerrainLane::Front;Best=FMath::Abs(Local.Y+ActiveTileSize);}
	if(bBackLaneEnabled&&FMath::Abs(Local.Y-ActiveTileSize)<Best){Lane=EPWGTerrainLane::Back;Best=FMath::Abs(Local.Y-ActiveTileSize);}
	if (Best > ActiveTileSize*0.75f) return -BIG_NUMBER;
	const int32 Index = FMath::FloorToInt((Local.X - ChunkStartLocalX) / ActiveTileSize);
	if (!SurfaceHeightTiles.IsValidIndex(Index)) return -BIG_NUMBER;
	return GetActorTransform().TransformPosition(FVector(Local.X,Local.Y,GetLaneSurfaceHeightAtLocalX(Local.X,Lane))).Z;
}

float APWG_Generator::GetSurfaceHeightForLane(const float X, const EPWGTerrainLane Lane) const
{
	if(bEnableChunkStreaming&&!bStreamingChild)
	{
		if(const APWG_Generator* Chunk=LoadedChunks.FindRef(GetChunkIndexAtWorldX(X)))
			return Chunk->GetSurfaceHeightForLane(X,Lane);
		return -BIG_NUMBER;
	}
	if(!bWorldGenerated||!IsLaneEnabledLocal(Lane)) return -BIG_NUMBER;
	const FVector Local=GetActorTransform().InverseTransformPosition(FVector(X,GetActorLocation().Y,GetActorLocation().Z));
	return GetActorTransform().TransformPosition(FVector(Local.X,GetLaneCenterYLocal(Lane),
		GetLaneSurfaceHeightAtLocalX(Local.X,Lane))).Z;
}

float APWG_Generator::GetLaneWorldY(const EPWGTerrainLane Lane) const
{
	if(bEnableChunkStreaming&&!bStreamingChild)
	{
		if(const APWG_Generator* Chunk=LoadedChunks.FindRef(0)) return Chunk->GetLaneWorldY(Lane);
	}
	return GetActorTransform().TransformPosition(FVector(0.0f,GetLaneCenterYLocal(Lane),0.0f)).Y;
}

bool APWG_Generator::IsTerrainLaneEnabled(const EPWGTerrainLane Lane) const
{
	if(bEnableChunkStreaming&&!bStreamingChild)
	{
		if(const APWG_Generator* Chunk=LoadedChunks.FindRef(0)) return Chunk->IsTerrainLaneEnabled(Lane);
	}
	return IsLaneEnabledLocal(Lane);
}

bool APWG_Generator::FindLaneSwitchDestination(const FVector& WorldLocation, const int32 Direction,
	EPWGTerrainLane& OutLane, FVector& OutWorldLocation) const
{
	if(bEnableChunkStreaming&&!bStreamingChild)
	{
		if(const APWG_Generator* Chunk=LoadedChunks.FindRef(GetChunkIndexAtWorldX(WorldLocation.X)))
			return Chunk->FindLaneSwitchDestination(WorldLocation,Direction,OutLane,OutWorldLocation);
		return false;
	}
	const FVector Local=GetActorTransform().InverseTransformPosition(WorldLocation);
	const int32 Segment=FMath::FloorToInt((Local.X-ChunkStartLocalX)/FMath::Max(1.0f,ActiveSurfaceSegmentWidth));
	if(!SteppedSurfaceHeights.IsValidIndex(Segment)||Direction==0) return false;
	EPWGTerrainLane Current=EPWGTerrainLane::Middle;
	float Best=FMath::Abs(Local.Y);
	if(bFrontLaneEnabled&&FMath::Abs(Local.Y+ActiveTileSize)<Best){Current=EPWGTerrainLane::Front;Best=FMath::Abs(Local.Y+ActiveTileSize);}
	if(bBackLaneEnabled&&FMath::Abs(Local.Y-ActiveTileSize)<Best){Current=EPWGTerrainLane::Back;}
	const int32 CurrentIndex=static_cast<int32>(Current);
	const int32 TargetIndex=CurrentIndex+(Direction>0?1:-1);
	if(TargetIndex<static_cast<int32>(EPWGTerrainLane::Front)||TargetIndex>static_cast<int32>(EPWGTerrainLane::Back)) return false;
	const EPWGTerrainLane Target=static_cast<EPWGTerrainLane>(TargetIndex);
	if(!IsLaneEnabledLocal(Target)) return false;
	const TArray<float>& Connection=(Current==EPWGTerrainLane::Front||Target==EPWGTerrainLane::Front)
		?FrontConnectionAlpha:BackConnectionAlpha;
	// Change lanes only on the truly shared plateau. The surrounding blend is
	// visual/terrain smoothing; accepting it could move the capsule sideways
	// into a surface which is still a few centimetres higher or lower.
	if(!Connection.IsValidIndex(Segment)||Connection[Segment]<1.0f-KINDA_SMALL_NUMBER) return false;
	const float CurrentSurface=GetLaneSteppedHeights(Current)[Segment];
	const float TargetSurface=GetLaneSteppedHeights(Target)[Segment];
	if(!FMath::IsNearlyEqual(CurrentSurface,TargetSurface,0.5f)) return false;
	OutLane=Target;
	const FVector DestinationLocal(Local.X,GetLaneCenterYLocal(Target),Local.Z);
	OutWorldLocation=GetActorTransform().TransformPosition(DestinationLocal);
	return true;
}

FVector APWG_Generator::GetWorldCenterSurfaceLocation() const
{
	const FVector Center = GetActorLocation();
	return FVector(Center.X, Center.Y, GetSurfaceHeightAt(Center.X, Center.Y));
}

void APWG_Generator::ClearGeneratedWorld()
{
	ClearSwordScenery();
	ClearPlaceholderBerries();
	bWorldGenerated = false;
	SetActorTickEnabled(false);
	if (IsValid(EnvironmentV2))
	{
		EnvironmentV2->Destroy();
		EnvironmentV2 = nullptr;
	}
	TArray<TObjectPtr<APWG_Generator>> Chunks;
	LoadedChunks.GenerateValueArray(Chunks);
	LoadedChunks.Reset();
	for (APWG_Generator* Chunk : Chunks) if (IsValid(Chunk)) Chunk->Destroy();
	TerrainMesh->ClearAllMeshSections();
	TerrainCollisionMesh->ClearAllMeshSections();
	TerrainCollisionMesh->ClearCollisionConvexMeshes();
	DecorationMesh->ClearAllMeshSections();
	EnvironmentMesh->ClearAllMeshSections();
	for (UProceduralMeshComponent* LayerMesh : BackgroundLayerMeshes)
	{
		if (LayerMesh) LayerMesh->ClearAllMeshSections();
	}
	for (UProceduralMeshComponent* SpriteMesh : BackgroundSpriteLayerMeshes)
	{
		if (SpriteMesh) SpriteMesh->ClearAllMeshSections();
	}
	SkyMesh->ClearAllMeshSections();
	SunMesh->ClearAllMeshSections();
	MoonMesh->ClearAllMeshSections();
	CloudMesh->ClearAllMeshSections();
	TerrainSceneryMesh->ClearAllMeshSections();
	VegetationMesh->ClearAllMeshSections();
	ActiveClouds.Reset();
	ActiveCloudRules.Reset();
	ActiveCloudTexture = nullptr;
	EnvironmentMaterials.Reset();
	for (UHierarchicalInstancedStaticMeshComponent* Component : GrassInstanceComponents)
	{
		if (IsValid(Component)) Component->DestroyComponent();
	}
	GrassInstanceComponents.Reset();
	for (UHierarchicalInstancedStaticMeshComponent* Component : BushInstanceComponents)
	{
		if (IsValid(Component)) Component->DestroyComponent();
	}
	BushInstanceComponents.Reset();
	GrassMaterials.Reset();
	BlockOverhangMaterials.Reset();
	TerrainMaterialInstance = nullptr;
	for (UHierarchicalInstancedStaticMeshComponent* Component : BlockOverhangInstanceComponents)
	{
		if (IsValid(Component)) Component->DestroyComponent();
	}
	BlockOverhangInstanceComponents.Reset();
	for (UHierarchicalInstancedStaticMeshComponent* Component : TreeInstanceComponents)
	{
		if (IsValid(Component)) Component->DestroyComponent();
	}
	TreeInstanceComponents.Reset();
	TreeMaterials.Reset();
	SkyMaterial = nullptr;
	for (AActor* Actor : SpawnedActors) if (IsValid(Actor)) Actor->Destroy();
	for (APWGLedgeActor* Ledge : SpawnedLedges) if (IsValid(Ledge)) Ledge->Destroy();
	SpawnedActors.Reset();
	SpawnedLedges.Reset();
	SurfaceHeightTiles.Reset();
	SmoothSurfaceHeights.Reset();
	SteppedSurfaceHeights.Reset();
	FrontSurfaceHeightTiles.Reset();
	BackSurfaceHeightTiles.Reset();
	FrontSteppedSurfaceHeights.Reset();
	BackSteppedSurfaceHeights.Reset();
	FrontConnectionAlpha.Reset();
	BackConnectionAlpha.Reset();
	LeftGhostSurfaceHeight = 0.0f;
	RightGhostSurfaceHeight = 0.0f;
	FrontLeftGhostSurfaceHeight=FrontRightGhostSurfaceHeight=0.0f;
	BackLeftGhostSurfaceHeight=BackRightGhostSurfaceHeight=0.0f;
	bFrontLaneEnabled=false;
	bBackLaneEnabled=false;
	SolidCells.Reset();
	CellStyleBases.Reset();
	ColumnMiniBiomes.Reset();
	ColumnBlendFrom.Reset();
	ColumnBlendTo.Reset();
	ColumnBlendStep.Reset();
	ColumnAtlasX.Reset();
	ColumnAtlasY.Reset();
	ColumnAtlasDirection.Reset();
	SolidGridHeight = 0;
}

// One support surface for Chaos, Mover and IK. Merge flat runs to avoid a body per pixel.
void APWG_Generator::BuildSolidTerrainCollision()
{
    TArray<TArray<FVector>> Hulls;
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    const int32 Faces[]={0,2,1,0,3,2,4,5,6,4,6,7,0,1,5,0,5,4,
        1,2,6,1,6,5,2,3,7,2,7,6,3,0,4,3,4,7};
    for (EPWGTerrainLane Lane:{EPWGTerrainLane::Front,EPWGTerrainLane::Middle,EPWGTerrainLane::Back})
    {
        if (!IsLaneEnabledLocal(Lane)) continue;
        const auto& Heights=GetLaneSteppedHeights(Lane);
        for (int32 First=0;First<Heights.Num();)
        {
            int32 End=First+1;
            while (End<Heights.Num() && FMath::IsNearlyEqual(Heights[End],Heights[First],0.001f)) ++End;
            const double X0=ChunkStartLocalX+First*ActiveSurfaceSegmentWidth;
            const double X1=ChunkStartLocalX+End*ActiveSurfaceSegmentWidth;
            const double HalfDepth=(bFrontLaneEnabled || bBackLaneEnabled)?ActiveTileSize*0.5:ChunkHalfDepth;
            const double Y0=GetLaneCenterYLocal(Lane)-HalfDepth;
            const double Y1=Y0+2*HalfDepth;
            const double Top=Heights[First], Bottom=FMath::Min(double(BottomLocalZ),Top-100.0);
            TArray<FVector> Hull={FVector(X0,Y0,Bottom),FVector(X1,Y0,Bottom),FVector(X1,Y1,Bottom),FVector(X0,Y1,Bottom),
                FVector(X0,Y0,Top),FVector(X1,Y0,Top),FVector(X1,Y1,Top),FVector(X0,Y1,Top)};
            const int32 Base=Vertices.Num();
            Vertices.Append(Hull);
            for (int32 Index:Faces) Triangles.Add(Base+Index);
            Hulls.Add(MoveTemp(Hull)); First=End;
        }
    }
    // Reapply at generation time: existing Blueprint component defaults can override the constructor.
    TerrainCollisionMesh->bUseAsyncCooking=false;
    TerrainCollisionMesh->bUseComplexAsSimpleCollision=false;
    TerrainCollisionMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    TerrainCollisionMesh->SetCollisionObjectType(ECC_WorldStatic);
    TerrainCollisionMesh->SetCollisionResponseToAllChannels(ECR_Block);
    TerrainCollisionMesh->SetCollisionResponseToChannel(ECC_GameTraceChannel1,ECR_Ignore);
    TerrainCollisionMesh->CreateMeshSection(0,Vertices,Triangles,TArray<FVector>(),TArray<FVector2D>(),
        TArray<FColor>(),TArray<FProcMeshTangent>(),true);
    TerrainCollisionMesh->SetCollisionConvexMeshes(Hulls);
}
