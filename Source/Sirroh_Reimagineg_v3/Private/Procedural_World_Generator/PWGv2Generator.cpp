#include "Procedural_World_Generator/PWGv2Generator.h"
#include "Misc/ScopeExit.h"
#include "POI/POIArtPool.h"
#include "POI/POIVisibility.h"
#include "Procedural_World_Generator/PWGProceduralTree.h"
#include "PWGv2Mesh.h"
#include "PWG/PWGEnvironmentV2.h"
#include "PWG/PWGPlayerController.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/PackageName.h"
#include "Components/SceneComponent.h"
#include "PhysicsEngine/BodySetup.h"

DEFINE_LOG_CATEGORY_STATIC(LogPWGv2, Log, All);

APWGv2Generator::APWGv2Generator()
{
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("WorldRoot")));
    GetRootComponent()->SetMobility(EComponentMobility::Static);
    PrimaryActorTick.bCanEverTick=true;
    PrimaryActorTick.bStartWithTickEnabled=false;
}

FString APWGv2Generator::SlotName() const
{
    // World name is an identifier, never a filesystem path.
    return TEXT("PWGv2_")+WorldName;
}

bool APWGv2Generator::GenerateWorld(FName)
{
    SetActorTickEnabled(false);
    PendingSurfaceCollision.Reset(); PendingUndergroundCollision.Reset();
    // A reload of this generator must not copy a visual override from the saved plan.
    const FPWGVisualProfile PreviousVisualProfile=IsValid(EnvironmentV2)?EnvironmentV2->VisualProfile:FPWGVisualProfile();
    if (IsValid(EnvironmentV2)) { EnvironmentV2->Destroy(); EnvironmentV2=nullptr; }
    for (auto& Pair:Chunks) if (Pair.Value) Pair.Value->DestroyComponent();
    for (auto& Pair:GrassChunks) if (Pair.Value) Pair.Value->DestroyComponent();
    for (auto& Pair:UndergroundChunks) if (Pair.Value) Pair.Value->DestroyComponent();
    for (auto& Pair:DecorationChunks)
    {
        for (UHierarchicalInstancedStaticMeshComponent* Component:Pair.Value.Components) if (Component) Component->DestroyComponent();
        for (UProceduralMeshComponent* Mask:Pair.Value.VisualMasks) if (Mask) Mask->DestroyComponent();
        for (AActor* Actor:Pair.Value.Actors) if (Actor && Actor->GetOwner()==this) Actor->Destroy();
    }
    UndergroundChunks.Reset(); DecorationChunks.Reset(); AtlasMaterials.Reset(); bVisitReady=false; bVisualMaskPassEnabled=false;
    Chunks.Reset(); GrassChunks.Reset(); WorldData=nullptr;
    bLoadedFromSave=false; bWorldSaved=false; bUndergroundView=false;
    FParse::Value(FCommandLine::Get(),TEXT("PWGWorld="),WorldName);
    FParse::Value(FCommandLine::Get(),TEXT("PWGSeed="),SeedOverride);
    bool bValidName=!WorldName.IsEmpty() && WorldName.Len()<=64;
    for (TCHAR C:WorldName) bValidName &= FChar::IsAlnum(C)||C==TEXT('_')||C==TEXT('-');
    if (!bValidName)
    {
        UE_LOG(LogPWGv2,Error,TEXT("Invalid world name. Use 1-64 letters, digits, underscores or hyphens."));
        return false;
    }
    if (!WorldTable) WorldTable=LoadObject<UDataTable>(nullptr,TEXT("/Game/Systems/Procedural_World_Generator/PWG_v2/DT_PWGv2_World.DT_PWGv2_World"));
    if (!RegionsTable) RegionsTable=LoadObject<UDataTable>(nullptr,TEXT("/Game/Systems/Procedural_World_Generator/PWG_v2/DT_PWGv2_Regions.DT_PWGv2_Regions"));
    if (WorldTable)
    {
        if (WorldTable->GetRowStruct()!=FPWGv2WorldRow::StaticStruct()) return false;
        const auto* Row=WorldTable->FindRow<FPWGv2WorldRow>(WorldSettingsRow,TEXT("PWG v2"));
        if (!Row) return false;
        Settings=*Row;
    }
    if (!BiomesTable) BiomesTable=LoadObject<UDataTable>(nullptr,TEXT("/Game/Systems/Procedural_World_Generator/PWG_v2/DT_PWGv2_Biomes.DT_PWGv2_Biomes"));
    if (!TreesTable) TreesTable=LoadObject<UDataTable>(nullptr,TEXT("/Game/Systems/Procedural_World_Generator/PWG_v2/DT_PWGv2_Trees.DT_PWGv2_Trees"));
    if (!TerrainTexturesTable) TerrainTexturesTable=LoadObject<UDataTable>(nullptr,TEXT("/Game/Systems/Procedural_World_Generator/PWG_v2/DT_PWGv2_TerrainTextures.DT_PWGv2_TerrainTextures"));
    if (TerrainTexturesTable && TerrainTexturesTable->GetRowStruct()!=FPWGv2TerrainTextureRow::StaticStruct()) return false;
    if (!UndergroundTable) UndergroundTable=LoadObject<UDataTable>(nullptr,TEXT("/Game/Systems/Procedural_World_Generator/PWG_v2/DT_PWGv2_Underground.DT_PWGv2_Underground"));
    if ((BiomesTable && BiomesTable->GetRowStruct()!=FPWGv2BiomeRow::StaticStruct()) ||
        (TreesTable && TreesTable->GetRowStruct()!=FPWGv2TreeRow::StaticStruct()) ||
        (UndergroundTable && UndergroundTable->GetRowStruct()!=FPWGv2UndergroundRow::StaticStruct())) return false;
    if (RegionsTable && RegionsTable->GetRowStruct()!=FPWGv2RegionRow::StaticStruct()) return false;
    Settings.SampleSpacing=FMath::Clamp(Settings.SampleSpacing,50.f,200.f);
    Settings.ChunkLengthMetres=FMath::Clamp(Settings.ChunkLengthMetres,8.f,128.f);
    Settings.LaneSpacing=FMath::Clamp(Settings.LaneSpacing,100.f,300.f);
    Settings.ForegroundDepth=FMath::Clamp(Settings.ForegroundDepth,500.f,10000.f);
    Settings.BackgroundDepth=FMath::Clamp(Settings.BackgroundDepth,500.f,20000.f);
    Settings.CoastLengthMetres=FMath::Clamp(Settings.CoastLengthMetres,20.f,500.f);
    Settings.OceanLengthMetres=FMath::Clamp(Settings.OceanLengthMetres,10.f,1000.f);
    Settings.MaxPlayableSlope=FMath::Clamp(Settings.MaxPlayableSlope,0.05f,0.5f);
    Settings.RegionBlendMetres=FMath::Clamp(Settings.RegionBlendMetres,1.f,100.f);
    Settings.LoadRadius=FMath::Clamp(Settings.LoadRadius,1,4);
    Settings.RetainRadius=FMath::Clamp(Settings.RetainRadius,Settings.LoadRadius,6);
    if (UGameplayStatics::DoesSaveGameExist(SlotName(),0))
    {
        WorldData=Cast<UPWGv2WorldSave>(UGameplayStatics::LoadGameFromSlot(SlotName(),0));
        if (!WorldData || (WorldData->Version!=2 || WorldData->AtlasLayoutRevision!=1 || WorldData->Biomes.IsEmpty()) || WorldData->Regions.IsEmpty() || WorldData->Heights.Num()<2 ||
            WorldData->Settings.SampleSpacing<50 || WorldData->MaxX<=WorldData->MinX ||
            WorldData->Heights.Num()>1000000 || WorldData->Settings.ChunkLengthMetres<8)
        {
            WorldData=nullptr;
            UE_LOG(LogPWGv2,Error,TEXT("Invalid/incompatible world save. Preserved existing file; select a different PWGWorld."));
            return false;
        }
        // Render distances can be tuned without changing the saved terrain.
        FString SavedStampError;
        if (!FPOITerrainStamp::ValidateAndSort(WorldData->POITerrainStamps,SavedStampError))
        {
            UE_LOG(LogPWGv2,Error,TEXT("Invalid saved POI terrain: %s"),*SavedStampError); WorldData=nullptr; return false;
        }
        const auto Rendering=Settings;
        Settings=WorldData->Settings;
        Settings.GrassLandscapeDepth=Rendering.GrassLandscapeDepth;
        Settings.GrassCullDistance=Rendering.GrassCullDistance;
        Settings.LoadRadius=Rendering.LoadRadius; Settings.RetainRadius=Rendering.RetainRadius;
        bLoadedFromSave=true; bWorldSaved=true;
    }
    else
    {
        WorldData=NewObject<UPWGv2WorldSave>(this);
        WorldData->WorldId=FGuid::NewGuid();
        WorldData->Seed=SeedOverride!=0?SeedOverride:static_cast<int32>(WorldData->WorldId.A & 0x7fffffff);
        WorldData->Settings=Settings;
        WorldData->POITerrainStamps=InitialPOITerrainStamps;
        FString StampError;
        if (!FPOITerrainStamp::ValidateAndSort(WorldData->POITerrainStamps,StampError))
        {
            UE_LOG(LogPWGv2,Error,TEXT("%s"),*StampError); WorldData=nullptr; return false;
        }
        MakeProfile();
        if (WorldData->Heights.IsEmpty()) { WorldData=nullptr; return false; }
        MakeExtendedPlan(); WorldData->AtlasLayoutRevision=1;
        bWorldSaved=SaveWorld();
    }
    // Materialize all twelve weights so the existing debug editor can edit even disabled shapes.
    for (auto& Region:WorldData->Regions)
        if (!Region.Definition.TreeShapes.ContainsByPredicate([](const FPWGTreeShapeWeight& W){return W.Weight>0 && FMath::IsFinite(W.Weight);}))
            Region.Definition.TreeShapes=APWGProceduralTree::DefaultShapeWeights(Region.Id,NAME_None);
    // Pay texture/material IO once at world loading, not when sprinting across chunk boundaries.
    ResidentAssets.Reset();
    auto Retain=[&](FString Path)
    {
        if (Path.IsEmpty()) return;
        if (!Path.Contains(TEXT("."))) Path+=TEXT(".")+FPackageName::GetShortName(Path);
        if (UObject* Asset=LoadObject<UObject>(nullptr,*Path)) ResidentAssets.AddUnique(Asset);
    };
    auto RetainTheme=[&](UPOIArtTheme* Theme)
    {
        if (Theme)
        {
            ResidentAssets.AddUnique(Theme);
            Retain(Theme->Material.ToString());
            for (const UPOIArtPool* Pool:{Theme->Backgrounds.Get(),Theme->Platforms.Get()})
                if (Pool) for (const auto& Pair:Pool->Images) { Retain(Pair.Side.ToString()); Retain(Pair.Top.ToString()); }
        }
    };
    for (int32 Layer=0;Layer<4;++Layer) RetainTheme(UPOIArtTheme::ForLayer(Layer));
    for (const auto& Row:WorldData->AuthoredPOIs) if (!Row.ArtTheme.IsNull()) RetainTheme(Row.ArtTheme.LoadSynchronous());
    Retain(TEXT("/Game/POIKit/M_KitImageFit.M_KitImageFit"));
    for (const auto& B:WorldData->Biomes)
        for (const auto* Part:{&B.Definition.Terrain,&B.Definition.Grass,&B.Definition.Bush,&B.Definition.Rock,&B.Definition.Overhang,&B.Definition.GroundPlant,&B.Definition.TreePlant}) Retain(Part->Texture.ToString());
    for (const auto& T:WorldData->TreeDefinitions) { Retain(T.Trunk.Texture.ToString()); Retain(T.Crown.Texture.ToString()); }
    for (const auto& L:WorldData->Layers) Retain(L.Atlas.Texture.ToString());
    if (TerrainTexturesTable) for (const auto& Entry:TerrainTexturesTable->GetRowMap()) Retain(reinterpret_cast<const FPWGv2TerrainTextureRow*>(Entry.Value)->Texture.ToString());
    for (const TCHAR* Name:{TEXT("Sprite"),TEXT("Tiled"),TEXT("Terrain"),TEXT("TerrainCover"),TEXT("Landscape"),TEXT("LandscapeCover")})
        Retain(FString(TEXT("/Game/Systems/Procedural_World_Generator/PWG_v2/M_PWGv2_"))+Name);
    // Rendering extent follows the configured camera, including worlds with old snapshots.
    const auto* CameraController=Cast<APWGPlayerController>(UGameplayStatics::GetPlayerController(this,0));
    const float CameraDistance=CameraController?CameraController->SidescrollerCameraDistance:1840.f;
    const float ViewWidth=CameraController?CameraController->SidescrollerOrthoWidth:1520.f;
    int32 ViewX=0,ViewY=0;
    if (CameraController) CameraController->GetViewportSize(ViewX,ViewY);
    const float ViewHeight=ViewX>0 && ViewY>0?ViewWidth*float(ViewY)/ViewX:ViewWidth;
    Settings.ForegroundDepth=FMath::Max(Settings.ForegroundDepth,CameraDistance+Settings.LaneSpacing+600.f);
    ForegroundDrop=FMath::Max(1200.f,ViewHeight*1.25f);
    WorldSeed=WorldData->Seed;
    auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Systems/Procedural_World_Generator/PWG_v2/M_PWGv2_Preview.M_PWGv2_Preview"));
    if (!Base) { UE_LOG(LogPWGv2,Error,TEXT("Missing PWG v2 assets. Run Content/Python/pwg_v2_create_assets.py.")); WorldData=nullptr; return false; }
    PreviewMaterial=UMaterialInstanceDynamic::Create(Base,this);
    PreviewMaterial->SetVectorParameterValue(TEXT("Tint"),FLinearColor::White);
    WaterMaterial=UMaterialInstanceDynamic::Create(Base,this);
    WaterMaterial->SetVectorParameterValue(TEXT("Tint"),FLinearColor(0.04f,0.3f,0.85f));
    GrassMaterial=UMaterialInstanceDynamic::Create(Base,this);
    GrassMaterial->SetVectorParameterValue(TEXT("Tint"),FLinearColor(0.18f,0.45f,0.08f));
    CardMesh=GetOrCreateGrassCardMesh();
    RockMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    auto* ForegroundBase=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Systems/Procedural_World_Generator/PWG_v2/M_PWGv2_Foreground.M_PWGv2_Foreground"));
    ForegroundMaterial=UMaterialInstanceDynamic::Create(ForegroundBase?ForegroundBase:Base,this);
    ForegroundMaterial->SetScalarParameterValue(TEXT("Opacity"),1);
    EnvironmentV2=GetWorld()->SpawnActor<APWGEnvironmentV2>();
    if (EnvironmentV2)
    {
        EnvironmentV2->SetOwner(this);
        EnvironmentV2->bEnableTrees=false; EnvironmentV2->bEnableRocks=false;
        EnvironmentV2->LandscapeClose.Depth=Settings.BackgroundDepth+1000;
        EnvironmentV2->LandscapeMid.Depth=Settings.BackgroundDepth+2500;
        EnvironmentV2->LandscapeFar.Depth=Settings.BackgroundDepth+4000;
        EnvironmentV2->SkyDepth=Settings.BackgroundDepth+6000;
        EnvironmentV2->VisualProfile=PreviousVisualProfile;
        EnvironmentV2->Initialize(this,WorldSeed);
    }
    StreamAt(0);
    SetActorTickEnabled(true);
    UE_LOG(LogPWGv2,Display,TEXT("World '%s', seed %d, %d regions, %.0fm, %s, saved=%d"),*WorldName,WorldSeed,WorldData->Regions.Num(),(WorldData->MaxX-WorldData->MinX)/100.f,bLoadedFromSave?TEXT("loaded"):TEXT("generated"),bWorldSaved);
    return true;
}

bool APWGv2Generator::SaveWorld()
{
    if (IsDebugSession()) return false;
    CaptureVisit();
    for (const auto& Batch:DecorationChunks) for (AActor* Actor:Batch.Value.Actors)
        if (auto* Tree=Cast<APWGProceduralTree>(Actor)) Tree->CaptureState();
    bWorldSaved=WorldData && UGameplayStatics::SaveGameToSlot(WorldData,SlotName(),0);
    if (!bWorldSaved) UE_LOG(LogPWGv2,Error,TEXT("Could not save world '%s'."),*WorldName);
    return bWorldSaved;
}

void APWGv2Generator::MakeProfile()
{
    TArray<FName> Names;
    if (RegionsTable) Names=RegionsTable->GetRowNames();
    Names.Sort(FNameLexicalLess());
    if (Names.IsEmpty()) Names={TEXT("Meadow")};
    FRandomStream Random(WorldData->Seed);
    // Camera faces +Y: screen-right is -X. Store spans in ascending world X.
    const TArray<FName> Order={TEXT("Desert"),TEXT("Rocklands"),TEXT("Mountains"),TEXT("Meadow"),TEXT("Swamp"),TEXT("Tundra")};
    Names.Sort([&](FName A,FName B) { const int32 IA=Order.Find(A),IB=Order.Find(B); return (IA<0?MAX_int32:IA)<(IB<0?MAX_int32:IB); });
    float Cursor=0;
    for (FName Name:Names)
    {
        FPWGv2RegionSpan Span; Span.Id=Name;
        if (RegionsTable) Span.Definition=*RegionsTable->FindRow<FPWGv2RegionRow>(Name,TEXT("PWG v2 region"));
        const int32 MinBiomes=FMath::Clamp(Span.Definition.MinBiomes,2,3);
        const int32 Count=Random.RandRange(MinBiomes,FMath::Clamp(Span.Definition.MaxBiomes,MinBiomes,3));
        Span.Definition.LengthMetres=Count*FMath::Clamp(Settings.BiomeLengthMetres,64.f,2000.f);
        Span.Definition.NoiseWavelengthMetres=FMath::Max(20.f,Span.Definition.NoiseWavelengthMetres);
        Span.Definition.MinHeightMetres=FMath::Clamp(Span.Definition.MinHeightMetres,0.f,500.f);
        Span.Definition.MaxHeightMetres=FMath::Clamp(Span.Definition.MaxHeightMetres,Span.Definition.MinHeightMetres,500.f);
        Span.StartX=Cursor; Cursor+=Span.Definition.LengthMetres*100; Span.EndX=Cursor;
        WorldData->Regions.Add(Span);
    }
    if (Cursor/Settings.SampleSpacing>500000) { UE_LOG(LogPWGv2,Error,TEXT("Island profile exceeds first-stage size budget.")); return; }
    for (auto& Span:WorldData->Regions) { Span.StartX-=Cursor/2; Span.EndX-=Cursor/2; }
    WorldData->MinX=-Cursor/2-100*(Settings.CoastLengthMetres+Settings.OceanLengthMetres);
    const int32 Count=FMath::CeilToInt((-2*WorldData->MinX)/Settings.SampleSpacing)+1;
    WorldData->MaxX=WorldData->MinX+(Count-1)*Settings.SampleSpacing;
    auto Eval=[&](const FPWGv2RegionSpan& Span,float X)
    {
        const float N=FMath::PerlinNoise1D(X/(Span.Definition.NoiseWavelengthMetres*100)+Random.GetInitialSeed()%8192);
        return Settings.SeaLevel+100*FMath::Lerp(Span.Definition.MinHeightMetres,Span.Definition.MaxHeightMetres,FMath::Clamp(0.5f+0.5f*N,0.f,1.f));
    };
    for (int32 I=0;I<Count;++I)
    {
        const float X=WorldData->MinX+I*Settings.SampleSpacing;
        const auto& Span=RegionAt(X);
        float Z=Eval(Span,X);
        for (int32 R=1;R<WorldData->Regions.Num();++R)
        {
            const float Edge=WorldData->Regions[R].StartX, Half=Settings.RegionBlendMetres*50;
            if (FMath::Abs(X-Edge)<Half)
            {
                const float A=FMath::SmoothStep(0.f,1.f,(X-Edge+Half)/(2*Half));
                Z=FMath::Lerp(Eval(WorldData->Regions[R-1],X),Eval(WorldData->Regions[R],X),A);
                break;
            }
        }
        const float Shore=FMath::Max(0.f,FMath::Abs(X)-Cursor/2);
        Z=FMath::Lerp(Z,Settings.SeaLevel-600,FMath::SmoothStep(0.f,Settings.CoastLengthMetres*100,Shore));
        WorldData->Heights.Add(Z);
    }
    // A safe, flat start independent of the shuffled region at the origin.
    const int32 Centre=FMath::RoundToInt(-WorldData->MinX/Settings.SampleSpacing);
    const float SpawnZ=FMath::Max(Settings.SeaLevel+250,WorldData->Heights[Centre]);
    for (int32 I=0;I<Count;++I)
        if (FMath::Abs(WorldData->MinX+I*Settings.SampleSpacing)<800) WorldData->Heights[I]=SpawnZ;
    const float Step=Settings.MaxPlayableSlope*Settings.SampleSpacing;
    // Lower excessive peaks instead of creating unwalkable region seams.
    for (int32 I=1;I<Count;++I) WorldData->Heights[I]=FMath::Min(WorldData->Heights[I],WorldData->Heights[I-1]+Step);
    for (int32 I=Count-2;I>=0;--I) WorldData->Heights[I]=FMath::Min(WorldData->Heights[I],WorldData->Heights[I+1]+Step);
}

const FPWGv2RegionSpan& APWGv2Generator::RegionAt(float X) const
{
    for (const auto& Span:WorldData->Regions) if (X<Span.EndX) return Span;
    return WorldData->Regions.Last();
}

float APWGv2Generator::Height(float X,float Y) const
{
    if (!WorldData || X<WorldData->MinX || X>WorldData->MaxX) return -BIG_NUMBER;
    const float P=(X-WorldData->MinX)/Settings.SampleSpacing;
    const int32 I=FMath::Clamp(FMath::FloorToInt(P),0,WorldData->Heights.Num()-2);
    const float Base=FMath::Lerp(WorldData->Heights[I],WorldData->Heights[I+1],FMath::Clamp(P-I,0.f,1.f));
    // Each lane retains a flat 1m cross-section; the gaps interpolate its gentle X profile.
    const float Gap=FMath::Max(0.f,Settings.LaneSpacing-100.f);
    const float Amplitude=FMath::Min(18.f,Gap*0.1f)*FMath::Clamp((Base-Settings.SeaLevel)/100.f,0.f,1.f)*FMath::Clamp((FMath::Abs(X)-800.f)/800.f,0.f,1.f);
    auto LaneOffset=[&](int32 Lane) { return Amplitude*FMath::Sin(X/1800.f+Lane*2.1f+WorldData->Seed%71); };
    float Offset=LaneOffset(0);
    if (Gap>0 && Y<=-50) Offset=FMath::Lerp(LaneOffset(-1),LaneOffset(0),FMath::SmoothStep(-Settings.LaneSpacing+50.f,-50.f,Y));
    else if (Gap>0 && Y>=50) Offset=FMath::Lerp(LaneOffset(0),LaneOffset(1),FMath::SmoothStep(50.f,Settings.LaneSpacing-50.f,Y));
    // Independent 2D relief, smoothly joined to the playable bank (zero derivative).
    const float FrontEdge=-Settings.LaneSpacing-50;
    if (Y<FrontEdge)
    {
        const float D=FMath::Clamp((FrontEdge-Y)/FMath::Max(1.f,Settings.ForegroundDepth+FrontEdge),0.f,1.f);
        const float Ease=D*D*(3-2*D);
        const float Detail=FMath::PerlinNoise2D(FVector2D(X/1000.f+WorldData->Seed%4096,Y/700.f));
        return FPOITerrainStamp::Apply(WorldData->POITerrainStamps,X,Y,Base+LaneOffset(-1)-Ease*ForegroundDrop+Ease*(1-Ease)*100.f*Detail);
    }
    const float BackEdge=Settings.LaneSpacing+50;
    if (Y<=BackEdge) return FPOITerrainStamp::Apply(WorldData->POITerrainStamps,X,Y,Base+Offset);
    const float D=FMath::Clamp((Y-BackEdge)/FMath::Max(1.f,Settings.BackgroundDepth-BackEdge),0.f,1.f);
    const auto Region=EcologyAt(X);
    const float Relief=FMath::Clamp((Region.MaxHeightMetres-Region.MinHeightMetres)*35.f,60.f,900.f);
    const float Seed=WorldData->Seed%4096;
    const float Hills=FMath::PerlinNoise2D(FVector2D(X/2100.f+Seed,Y/1300.f+31));
    const float Detail=FMath::PerlinNoise2D(FVector2D(X/700.f-Seed,Y/550.f));
    return FPOITerrainStamp::Apply(WorldData->POITerrainStamps,X,Y,Base+Offset+D*D*(3-2*D)*Relief*(0.25f+Hills+0.2f*Detail));
}

float APWGv2Generator::GetSurfaceHeightAt(float X,float Y) const
{
    if (!WorldData) return -BIG_NUMBER;
    const FVector P=GetActorTransform().InverseTransformPosition(FVector(X,Y,GetActorLocation().Z));
    if (P.Y < -Settings.ForegroundDepth || P.Y>Settings.BackgroundDepth) return -BIG_NUMBER;
    const float Z=Height(P.X,P.Y);
    return Z<=-BIG_NUMBER/2?Z:GetActorTransform().TransformPosition(FVector(P.X,P.Y,Z)).Z;
}
float APWGv2Generator::GetLaneWorldY(EPWGTerrainLane Lane) const
{
    return GetActorLocation().Y+(static_cast<int32>(Lane)-1)*Settings.LaneSpacing;
}
float APWGv2Generator::GetSurfaceHeightForLane(float X,EPWGTerrainLane Lane) const
{
    return GetSurfaceHeightAt(X,GetLaneWorldY(Lane));
}
bool APWGv2Generator::FindLaneSwitchDestination(const FVector& P,int32 Direction,EPWGTerrainLane& Lane,FVector& Destination) const
{
    if (!WorldData || Direction==0 || GetSurfaceHeightAt(P.X,P.Y)<=-BIG_NUMBER/2) return false;
    const int32 Current=FMath::Clamp(FMath::RoundToInt((P.Y-GetActorLocation().Y)/Settings.LaneSpacing)+1,0,2);
    const int32 Target=Current+(Direction>0?1:-1);
    if (Target<0 || Target>(LayerAt(P)>0?1:2)) return false;
    Lane=static_cast<EPWGTerrainLane>(Target); Destination=P; Destination.Y=GetLaneWorldY(Lane);
    return true;
}

void APWGv2Generator::Tick(float DeltaSeconds)
{
    // Deliberately do not run the legacy generator tick.
    UpdateTime+=DeltaSeconds;
    if (UpdateTime<0.2f || !WorldData) return;
    UpdateTime=0;
    if (const APawn* Pawn=UGameplayStatics::GetPlayerPawn(this,0))
    {
        UpdateCutaway(Pawn->GetActorLocation());
        StreamAt(GetWorld()->GetTimeSeconds()<DebugTeleportPinUntil?DebugTeleportPinX:GetActorTransform().InverseTransformPosition(Pawn->GetActorLocation()).X,false);
        // The camera-facing ray curtain hid unrelated lanes and decoration.
        // Do not spawn it. Region/portal visibility is a separate rendering pass.
        SaveTime+=0.2f;
        if (SaveTime>=10) { SaveTime=0; SaveWorld(); }
    }
}

void APWGv2Generator::StreamAt(float X,bool Immediate)
{
    const float Size=Settings.ChunkLengthMetres*100;
    const int32 Centre=FMath::FloorToInt(X/Size);
    TArray<int32> Pending;
    for (int32 I=Centre-Settings.LoadRadius;I<=Centre+Settings.LoadRadius;++I)
        if (!Chunks.Contains(I) && I*Size<WorldData->MaxX && (I+1)*Size>WorldData->MinX) Pending.Add(I);
    Pending.Sort([Centre](int32 A,int32 B) { return FMath::Abs(A-Centre)<FMath::Abs(B-Centre); });
    const int32 Budget=Immediate?Pending.Num():FMath::Min(1,Pending.Num());
    for (int32 N=0;N<Budget;++N) BuildChunk(Pending[N],!Immediate && FMath::Abs(Pending[N]-Centre)>1);
    // Ensure explicit arrivals and the neighbouring chunks have usable collision.
    // GetBodySetup may still be the empty pre-cook body while async cooking runs.
    auto FinishNearbyCollision=[Centre,Immediate](auto& PendingBodies,auto& Meshes)
    {
        for (auto It=PendingBodies.CreateIterator();It;++It)
        {
            auto* Mesh=Meshes.FindRef(It.Key()).Get();
            if (!Mesh) { It.RemoveCurrent(); continue; }
            const bool Ready=Mesh->GetBodySetup()->AggGeom.ConvexElems.Num()==It.Value().Num();
            if (!Ready && (Immediate || FMath::Abs(It.Key()-Centre)<=1))
            {
                Mesh->bUseAsyncCooking=false;
                Mesh->SetCollisionConvexMeshes(It.Value());
            }
            else if (!Ready) continue;
            It.RemoveCurrent();
        }
    };
    FinishNearbyCollision(PendingSurfaceCollision,Chunks);
    FinishNearbyCollision(PendingUndergroundCollision,UndergroundChunks);
    TArray<int32> Remove;
    for (const auto& Pair:Chunks) if (FMath::Abs(Pair.Key-Centre)>Settings.RetainRadius) Remove.Add(Pair.Key);
    for (int32 I:Remove)
    {
        PendingSurfaceCollision.Remove(I); PendingUndergroundCollision.Remove(I);
        Chunks[I]->DestroyComponent(); Chunks.Remove(I);
        if (auto* Grass=GrassChunks.FindRef(I).Get()) Grass->DestroyComponent();
        GrassChunks.Remove(I);
        if (auto* Under=UndergroundChunks.FindRef(I).Get()) Under->DestroyComponent();
        UndergroundChunks.Remove(I);
        if (auto* Batch=DecorationChunks.Find(I))
        {
            for (UHierarchicalInstancedStaticMeshComponent* Component:Batch->Components) if (Component) Component->DestroyComponent();
            for (UProceduralMeshComponent* Mask:Batch->VisualMasks) if (Mask) Mask->DestroyComponent();
            for (AActor* Actor:Batch->Actors) if (Actor && Actor->GetOwner()==this) Actor->Destroy();
        }
        DecorationChunks.Remove(I);
    }
}



void APWGv2Generator::BuildChunk(int32 Index,bool AsyncCooking)
{
    const double ChunkStart=FPlatformTime::Seconds();
    ON_SCOPE_EXIT
    {
        const double Ms=(FPlatformTime::Seconds()-ChunkStart)*1000;
        if(Ms>16)UE_LOG(LogPWGv2,Warning,TEXT("PWG streaming chunk %d took %.1f ms (async collision=%d)"),Index,Ms,AsyncCooking);
    };
    const float Size=Settings.ChunkLengthMetres*100;
    const float Start=FMath::Max(WorldData->MinX,Index*Size),End=FMath::Min(WorldData->MaxX,(Index+1)*Size);
    auto* Mesh=NewObject<UProceduralMeshComponent>(this);
    Mesh->SetCanEverAffectNavigation(false);
    Mesh->SetupAttachment(GetRootComponent()); Mesh->SetMobility(EComponentMobility::Static); Mesh->RegisterComponent();
    Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Mesh->SetCollisionObjectType(ECC_WorldStatic); Mesh->SetCollisionResponseToAllChannels(ECR_Block);
    Mesh->SetCollisionResponseToChannel(ECC_GameTraceChannel1,ECR_Ignore);
    Mesh->ComponentTags.Add(TEXT("CombatTerrain"));
    Mesh->bUseComplexAsSimpleCollision=false; Mesh->bUseAsyncCooking=AsyncCooking; Mesh->SetCastShadow(false);
    TMap<int32,FPWGv2MeshBuffers> Surface;
    TMap<int32,float> MaterialX;
    TMap<int32,bool> CoverFlags,EntranceFlags;
    TMap<int32,int32> Tiles,LandscapeKinds;
    auto AddFace=[&](int32 A,int32 B,float X,bool Cover,bool Entrance,int32 Tile,int32 Landscape=0)->FPWGv2MeshBuffers&
    {
        const int32 Key=((((A*4096+B)*2+int32(Cover))*2+int32(Entrance))*9+Tile)*3+Landscape;
        LandscapeKinds.FindOrAdd(Key)=Landscape;
        MaterialX.FindOrAdd(Key)=X; CoverFlags.FindOrAdd(Key)=Cover; EntranceFlags.FindOrAdd(Key)=Entrance; Tiles.FindOrAdd(Key)=Tile;
        return Surface.FindOrAdd(Key);
    };
    FPWGv2MeshBuffers Water;
    TArray<TArray<FVector>> Hulls;
    TArray<float> Samples; Samples.Add(Start);
    for (float X=WorldData->MinX+(FMath::FloorToInt((Start-WorldData->MinX)/Settings.SampleSpacing)+1)*Settings.SampleSpacing; X<End; X+=Settings.SampleSpacing) Samples.Add(X);
    for (const auto& P:WorldData->Portals) if (P.UpperLayer==0)
        for (float Edge:{P.X-P.Width/2,P.X+P.Width/2}) if (Edge>Start && Edge<End) Samples.AddUnique(Edge);
    Samples.Add(End); Samples.Sort();
    TArray<float> Depths={-Settings.ForegroundDepth,-Settings.LaneSpacing-50,-50,50,Settings.LaneSpacing+50};
    Depths.AddUnique(-Settings.LaneSpacing+50); Depths.AddUnique(Settings.LaneSpacing-50); Depths.Sort();
    for (float Y=Settings.LaneSpacing+250;Y<Settings.BackgroundDepth;Y+=200) Depths.Add(Y);
    Depths.Add(Settings.BackgroundDepth);
    for (float Y=-Settings.LaneSpacing-250;Y>-Settings.ForegroundDepth;Y-=200) Depths.AddUnique(Y);
    Depths.Sort();
    const float Bottom=Settings.SeaLevel-(WorldData->Layers.Num()==4?WorldData->Layers.Last().FloorBelowSeaMetres*100+500:2500);
    for (int32 I=0;I<Samples.Num()-1;++I)
    {
        const float X0=Samples[I],X1=Samples[I+1],Mid=(X0+X1)/2;
        int32 A,B; float Mix; BiomeMix(Mid,A,B,Mix);
        const bool Mouth=IsOpening(0,Mid,0);
        
        int32 DummyA,DummyB; float Blend0,Blend1;
        BiomeMix(X0,DummyA,DummyB,Blend0); if (DummyA==DummyB) Blend0=DummyA==B?1:0;
        BiomeMix(X1,DummyA,DummyB,Blend1); if (DummyA==DummyB) Blend1=DummyA==B?1:0;
        if (A==B) Blend0=Blend1=0;
        for (int32 Y=0;Y<Depths.Num()-1;++Y)
        {
            const float Y0=Depths[Y],Y1=Depths[Y+1];
            if (IsOpening(0,Mid,(Y0+Y1)/2)) continue;
            const FVector V0(X0,Y0,Height(X0,Y0)),V1(X1,Y0,Height(X1,Y0)),V2(X1,Y1,Height(X1,Y1)),V3(X0,Y1,Height(X0,Y1));
            auto& TopBuffer=AddFace(A,B,Mid,Y1<=-Settings.LaneSpacing-50 || (Mouth && Y1<=-50),Mouth && Y1<=-50,4,Y1<=-Settings.LaneSpacing-50?1:(Y0>=Settings.LaneSpacing+50?2:0));
            TopBuffer.Quad(V0,V1,V2,V3,FLinearColor::White,Blend0,Blend1);
            // Close the underside of the playable bank where the foreground rolls away.
            // Use the exact surface vertices. Keep the shaft mouth open in the cutaway.
            if (Y0 == -Settings.LaneSpacing-50 && !Mouth)
            {
                auto& Bank = AddFace(A,B,Mid,true,false,4,1);
                const FVector Drop(0,0,100);
                Bank.Quad(V0-Drop,V1-Drop,V1,V0,FLinearColor::White,Blend0,Blend1);
            }
            if (Y0>=-Settings.LaneSpacing-50 && Y1<=Settings.LaneSpacing+50) Hulls.Add({V0,V1,V2,V3,V0-FVector(0,0,60),V1-FVector(0,0,60),V2-FVector(0,0,60),V3-FVector(0,0,60)});
        }
        // The foreground continues below and behind the camera; no visible vertical cut face.
    }
    // Finite ocean ends block movement rather than dropping the player out of the world.
    for (float X:{WorldData->MinX,WorldData->MaxX}) if (X>=Start && X<=End)
    {
        const float Z=Height(X);
        Hulls.Add({FVector(X-20,-Settings.LaneSpacing-50,Z-100),FVector(X+20,-Settings.LaneSpacing-50,Z-100),FVector(X+20,Settings.LaneSpacing+50,Z-100),FVector(X-20,Settings.LaneSpacing+50,Z-100),FVector(X-20,-Settings.LaneSpacing-50,Z+2000),FVector(X+20,-Settings.LaneSpacing-50,Z+2000),FVector(X+20,Settings.LaneSpacing+50,Z+2000),FVector(X-20,Settings.LaneSpacing+50,Z+2000)});
    }
    auto Wall=[&](float X0,float X1,float Y0,float Y1,float Z0,float Z1)
    {
        Hulls.Add({FVector(X0,Y0,Z0),FVector(X1,Y0,Z0),FVector(X1,Y1,Z0),FVector(X0,Y1,Z0),FVector(X0,Y0,Z1),FVector(X1,Y0,Z1),FVector(X1,Y1,Z1),FVector(X0,Y1,Z1)});
    };
    const float Top=FMath::Max(Height(Start),Height(End))+3000;
    // Guard radius is 54 cm; retain clearance beyond each metre-wide lane.
    Wall(Start,End,-Settings.LaneSpacing-95,-Settings.LaneSpacing-75,Bottom,Top);
    Wall(Start,End,Settings.LaneSpacing+75,Settings.LaneSpacing+95,Bottom,Top);
    int32 Section=3;
    for (auto& Pair:Surface) { Pair.Value.Submit(Mesh,Section); Mesh->SetMaterial(Section++,TerrainMaterialAt(MaterialX[Pair.Key],CoverFlags[Pair.Key],EntranceFlags[Pair.Key],Tiles[Pair.Key],LandscapeKinds[Pair.Key])); }
    for (int32 I=0;I<Samples.Num()-1;++I)
    {
        const float X0=Samples[I],X1=Samples[I+1];
        if (FMath::Min(Height(X0),Height(X1))>Settings.SeaLevel) continue;
        for (int32 J=0;J<Depths.Num()-1;++J)
        {
            auto WaterPoint=[&](float X,float Y) { return FVector(X,Y,Settings.SeaLevel); };
            Water.Quad(WaterPoint(X0,Depths[J]),WaterPoint(X1,Depths[J]),WaterPoint(X1,Depths[J+1]),WaterPoint(X0,Depths[J+1]),FLinearColor::White);
        }
    }
    Water.Submit(Mesh,1); Mesh->SetMaterial(1,WaterMaterial);
    // Section creation updates collision too: install hulls only once all sections exist.
    Mesh->SetCollisionConvexMeshes(Hulls);
    if (AsyncCooking) PendingSurfaceCollision.Add(Index,Hulls);
    // Foreground is grouped with atlas sections; section 1 remains reserved for water.
    Mesh->SetCanEverAffectNavigation(Hulls.Num()>0);
    Chunks.Add(Index,Mesh);
    const double TerrainDone=FPlatformTime::Seconds();
    BuildDecorationsV2(Index,Start,End);
    const double DecorationDone=FPlatformTime::Seconds();
    if (Settings.bEnableUnderground) BuildUnderground(Index,Start,End,AsyncCooking,Samples);
    const double UndergroundDone=FPlatformTime::Seconds();
    BuildAuthoredPOIs(Index,Start,End);
    ApplyChunkLayerVisibility(Index);
    const double POIDone=FPlatformTime::Seconds();
    if((POIDone-ChunkStart)*1000>16)
        UE_LOG(LogPWGv2,Display,TEXT("PWG chunk phases %d: terrain=%.1f vegetation=%.1f underground=%.1f POI=%.1f ms"),Index,
            (TerrainDone-ChunkStart)*1000,(DecorationDone-TerrainDone)*1000,(UndergroundDone-DecorationDone)*1000,(POIDone-UndergroundDone)*1000);
}



bool APWGv2Generator::HasGlobalDepthAtmosphere() const
{
    return IsValid(EnvironmentV2) && EnvironmentV2->IsDepthAtmosphereActive();
}
