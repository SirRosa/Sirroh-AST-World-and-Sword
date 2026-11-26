#include "PWG/PWGGenerator.h"
#include "Sword/SwordWorldSubsystem.h"
#include "Sword/ProceduralSword.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"

void APWG_Generator::ClearSwordScenery()
{
    if (auto* State=GetWorld()->GetSubsystem<USwordWorldSubsystem>()) State->RemoveSurfaces(this);
    for (AProceduralSword* Sword:SpawnedSwords) if (IsValid(Sword) && Sword->GetOwner()==this) Sword->Destroy();
    SpawnedSwords.Reset();
    if (IsValid(RockInstances)) RockInstances->DestroyComponent();
    RockInstances=nullptr;
    bStarterSwordCreated=false;
}
void APWG_Generator::AddRock(const FBiomeData& Biome,const FVector& LocalPosition,EPWGTerrainLane Lane,int32 Mini,int32 Variant,float Scale,const FString& Id,int32 Seed,bool bForceSword)
{
    UTexture2D* Atlas=Biome.BiomeMasterAtlas.LoadSynchronous();
    if (!Atlas) return;
    if (!RockInstances)
    {
        UStaticMesh* Card=GetOrCreateGrassCardMesh();
        auto* Parent=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Systems/Sprite2Bone/Materials/M_PWG_Grass.M_PWG_Grass"));
        if (!Card || !Parent) return;
        RockInstances=NewObject<UHierarchicalInstancedStaticMeshComponent>(this,TEXT("PWG_SurfaceRocks"));
        RockInstances->SetupAttachment(RootComponent);
        RockInstances->SetStaticMesh(Card);
        RockInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        RockInstances->SetCanEverAffectNavigation(false);
        RockInstances->SetCastShadow(false);
        RockInstances->SetLightingChannels(true,true,false);
        RockInstances->bEnableDensityScaling=false;
        RockInstances->SetCullDistances(0,0); // Chunk streaming owns the lifetime, including sword-bearing rocks.
        RockInstances->SetNumCustomDataFloats(4);
        RockInstances->RegisterComponent();
        auto* MID=UMaterialInstanceDynamic::Create(Parent,this);
        MID->SetTextureParameterValue(TEXT("GrassAtlas"),Atlas);
        MID->SetScalarParameterValue(TEXT("AtlasColumns"),16);
        MID->SetScalarParameterValue(TEXT("AtlasRows"),32);
        MID->SetScalarParameterValue(TEXT("GrassBrightness"),1);
        MID->SetVectorParameterValue(TEXT("GrassTint"),FLinearColor::White);
        MID->SetScalarParameterValue(TEXT("PixelWorldStep"),Biome.Grass.PixelWorldStep);
        RockInstances->SetMaterial(0,MID);
        GrassMaterials.Add(MID);
    }
    Mini=FMath::Clamp(Mini,0,7); Variant=FMath::Clamp(Variant,0,3);
    const float Height=FMath::Max(1.0f,RockBaseHeight*Scale);
    const FTransform Transform(FRotator::ZeroRotator,LocalPosition,FVector(Height*2/100,1,Height/100));
    const int32 Index=RockInstances->AddInstance(Transform);
    // 3072/256=12, origin row=0. Each surface style owns a 2x2 group.
    const int32 Frame=((Mini/2)*2+Variant/2)*16+12+(Mini%2)*2+Variant%2;
    RockInstances->SetCustomDataValue(Index,0,Frame,false);
    RockInstances->SetCustomDataValue(Index,1,0,false);
    RockInstances->SetCustomDataValue(Index,2,0,false); // no wind
    RockInstances->SetCustomDataValue(Index,3,1,true);
    const FVector Centre=GetActorTransform().TransformPosition(LocalPosition+FVector(0,0,Height*0.25f));
    const float WorldLane=GetLaneWorldY(Lane);
    auto* State=GetWorld()->GetSubsystem<USwordWorldSubsystem>();
    State->RegisterSurface(this,Centre-FVector(Height*0.3f,0,0),Centre+FVector(Height*0.3f,0,0),Height*0.25f,WorldLane);
    if (Variant!=0) return;
    const FSwordSpawnRule Rule=State->RuleFor(Mini);
    FRandomStream Random(Seed);
    if (!bForceSword && (!bEnableSwordSpawns || !Rule.bEnabled || Random.FRand()>Rule.SwordChancePerSmallRock)) return;
    FVector Contact=GetActorTransform().TransformPosition(LocalPosition+FVector(0,1,Height*RockEmbedHeightFraction));
    if (auto* Sword=State->SpawnSword(Contact,WorldLane,Id,Mini,Seed,this))
    {
        Sword->Embed(Contact,FVector(0,0,-1));
        SpawnedSwords.Add(Sword);
    }
}
void APWG_Generator::BuildRocks(const FBiomeData& Biome)
{
    if (!bEnableSurfaceRocks || RocksPer100Metres<=0 || Biome.BiomeMasterAtlas.IsNull()) return;
    FRandomStream Random(static_cast<int32>(HashCombineFast(GetTypeHash(ActiveWorldSeed),GetTypeHash(StreamingChunkIndex))) ^ 0xB017u);
    const EPWGTerrainLane Lanes[]={EPWGTerrainLane::Front,EPWGTerrainLane::Middle,EPWGTerrainLane::Back};
    for (int32 Tile=0;Tile<SurfaceHeightTiles.Num();++Tile)
    {
        const int32 Mini=ColumnMiniBiomes.IsValidIndex(Tile)?ColumnMiniBiomes[Tile]:5;
        for (auto Lane:Lanes)
        {
            if (!IsLaneEnabledLocal(Lane)) continue;
            const float Density=Lane==EPWGTerrainLane::Front?Biome.FrontLane.VegetationDensityMultiplier:
                (Lane==EPWGTerrainLane::Back?Biome.BackLane.VegetationDensityMultiplier:1.0f);
            if (Random.FRand()>FMath::Clamp(RocksPer100Metres*ActiveTileSize*Density/10000.0f,0.0f,1.0f)) continue;
            const float X=ChunkStartLocalX+(Tile+Random.FRand())*ActiveTileSize;
            const float Y=GetLaneCenterYLocal(Lane)+Random.FRandRange(-25,25);
            const FVector Position(X,Y,GetLaneSurfaceHeightAtLocalX(X,Lane)-3);
            const float Roll=Random.FRand();
            const int32 Variant=Roll<0.45f?0:(Roll<0.75f?1:(Roll<0.93f?2:3));
            const float Sizes[]={0.65f,0.9f,1.2f,1.0f};
            const FString Id=FString::Printf(TEXT("rock-sword:%d:%d:%d:%d"),ActiveWorldSeed,StreamingChunkIndex,Tile,static_cast<int32>(Lane));
            const float Scale=Sizes[Variant]*Random.FRandRange(0.85f,1.15f);
            const int32 SwordSeed=Random.RandRange(1,MAX_int32);
            AddRock(Biome,Position,Lane,Mini,Variant,Scale,Id,SwordSeed);
        }
    }
}
void APWG_Generator::EnsureStarterSwordNear(const FVector& PlayerLocation)
{
    if (bStarterSwordCreated) return;
    const FBiomeData* Biome=FindBiomeData(ActiveBiomeRow,TEXT("Starter sword"));
    if (!Biome) return;
    const float Lane=GetLaneWorldY(EPWGTerrainLane::Middle);
    const float Surface=GetSurfaceHeightAt(PlayerLocation.X,Lane);
    FVector Point(PlayerLocation.X+140,Lane,0);
    double Best=DBL_MAX;
    for (float Offset:{140.0f,-140.0f,220.0f,-220.0f})
    {
        const float Z=GetSurfaceHeightAt(PlayerLocation.X+Offset,Lane);
        if (!FMath::IsFinite(Z) || Z<=-BIG_NUMBER*0.5f) continue;
        const double Score=FMath::Abs(Z-Surface)+FMath::Abs(Offset)*0.05f;
        if (Score<Best) { Best=Score; Point=FVector(PlayerLocation.X+Offset,Lane,Z-3); }
    }
    if (Best==DBL_MAX) return;
    // Resolve the actual streamed chunk so the guaranteed sword follows that mini-biome's rule.
    APWG_Generator* Source=this;
    if (bEnableChunkStreaming) if (auto* Chunk=LoadedChunks.Find(GetChunkIndexAtWorldX(Point.X))) Source=Chunk->Get();
    const FVector SourceLocal=Source->GetActorTransform().InverseTransformPosition(Point);
    const int32 Tile=FMath::FloorToInt((SourceLocal.X-Source->ChunkStartLocalX)/Source->ActiveTileSize);
    const int32 Mini=Source->ColumnMiniBiomes.IsValidIndex(Tile)?Source->ColumnMiniBiomes[Tile]:0;
    const FVector Local=GetActorTransform().InverseTransformPosition(Point);
    AddRock(*Biome,Local,EPWGTerrainLane::Middle,Mini,0,0.65f,FString::Printf(TEXT("starter-sword:%d"),ActiveWorldSeed),ActiveWorldSeed^0x570A,true);
    bStarterSwordCreated=true;
}
