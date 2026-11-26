#include "Procedural_World_Generator/PWGv2Generator.h"
#include "Procedural_World_Generator/PWGProceduralTree.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/Texture2D.h"
#include "Misc/PackageName.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Placeholder/PlaceholderBerry.h"
#include "Placeholder/PlaceholderBerryWorld.h"
#include "Kismet/GameplayStatics.h"
#include "POI/POIShaft.h"

namespace
{
FPWGv2AtlasPart Frame(const FPWGv2AtlasPart& Part,int32 Index)
{
    FPWGv2AtlasPart Result=Part;
    const int32 Cols=FMath::Max(1,Part.Columns),Rows=FMath::Max(1,Part.Rows);
    Index=FMath::Clamp(Index,0,Cols*Rows-1);
    const float W=(Part.UV.Z-Part.UV.X)/Cols,H=(Part.UV.W-Part.UV.Y)/Rows;
    Result.UV=FVector4(Part.UV.X+(Index%Cols)*W,Part.UV.Y+(Index/Cols)*H,Part.UV.X+(Index%Cols+1)*W,Part.UV.Y+(Index/Cols+1)*H);
    Result.Columns=Result.Rows=1; return Result;
}
void SetAtlas(UMaterialInstanceDynamic* M,const FPWGv2AtlasPart& Part,const FString& Suffix)
{
    FString ObjectPath=Part.Texture.ToString();
    // DT imports can contain a package path; soft loading requires Package.Asset.
    if (!ObjectPath.IsEmpty() && !ObjectPath.Contains(TEXT(".")))
        ObjectPath+=TEXT(".")+FPackageName::GetShortName(ObjectPath);
    UTexture2D* T=ObjectPath.IsEmpty()?nullptr:LoadObject<UTexture2D>(nullptr,*ObjectPath);
    if (!T && !Part.Texture.IsNull()) UE_LOG(LogTemp,Error,TEXT("PWG v2: cannot load atlas %s"),*Part.Texture.ToString());
    if (!T) T=LoadObject<UTexture2D>(nullptr,TEXT("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture"));
    M->SetTextureParameterValue(*FString(TEXT("Atlas")+Suffix),T);
    const auto& R=Part.UV;
    M->SetVectorParameterValue(*FString(TEXT("UVScale")+Suffix),FLinearColor(FMath::Max(0.0001,double(R.Z-R.X)),FMath::Max(0.0001,double(R.W-R.Y)),0,0));
    M->SetVectorParameterValue(*FString(TEXT("UVOffset")+Suffix),FLinearColor(R.X,R.Y,0,0));
}
}
UMaterialInstanceDynamic* APWGv2Generator::AtlasMaterial(const FPWGv2AtlasPart& Part,FLinearColor Tint,bool Tiled,bool TreeLighting,bool LitBackground)
{
    const FString Key=Part.Texture.ToString()+Part.UV.ToString()+Tint.ToString()+(LitBackground?TEXT("BackgroundLit"):TreeLighting?TEXT("TreeLit"):(Tiled?TEXT("Tiled"):TEXT("Sprite")));
    if (auto* Found=AtlasMaterials.Find(Key)) return *Found;
    auto* Base=LoadObject<UMaterialInterface>(nullptr,LitBackground?TEXT("/Game/Systems/Procedural_World_Generator/PWG_v2/M_PWGv2_BackgroundLit.M_PWGv2_BackgroundLit"):TreeLighting?TEXT("/Game/Systems/Procedural_World_Generator/PWG_v2/M_PWGv2_TreeLit.M_PWGv2_TreeLit"):Tiled?TEXT("/Game/Systems/Procedural_World_Generator/PWG_v2/M_PWGv2_Tiled.M_PWGv2_Tiled"):TEXT("/Game/Systems/Procedural_World_Generator/PWG_v2/M_PWGv2_Sprite.M_PWGv2_Sprite"));
    auto* M=UMaterialInstanceDynamic::Create(Base?Base:PreviewMaterial,this);
    SetAtlas(M,Part,TEXT("")); M->SetVectorParameterValue(TEXT("Tint"),Tint);
    AtlasMaterials.Add(Key,M); return M;
}
UMaterialInstanceDynamic* APWGv2Generator::TerrainMaterialAt(float X,bool Cover,bool Entrance,int32 Tile,int32 Landscape)
{
    int32 A,B; float Alpha; BiomeMix(X,A,B,Alpha);
    if (!WorldData->Biomes.IsValidIndex(A)) return PreviewMaterial;
    const FString Key=FString::Printf(TEXT("Terrain:%d:%d:%d:%d:%d:%d"),A,B,Cover,Entrance,Tile,Landscape);
    if (auto* Found=AtlasMaterials.Find(Key)) return *Found;
    auto* Base=LoadObject<UMaterialInterface>(nullptr,Cover?TEXT("/Game/Systems/Procedural_World_Generator/PWG_v2/M_PWGv2_TerrainCover.M_PWGv2_TerrainCover"):TEXT("/Game/Systems/Procedural_World_Generator/PWG_v2/M_PWGv2_Terrain.M_PWGv2_Terrain"));
    if (Landscape) Base=LoadObject<UMaterialInterface>(nullptr,Cover?TEXT("/Game/Systems/Procedural_World_Generator/PWG_v2/M_PWGv2_LandscapeCover.M_PWGv2_LandscapeCover"):TEXT("/Game/Systems/Procedural_World_Generator/PWG_v2/M_PWGv2_Landscape.M_PWGv2_Landscape"));
    if (!Base) return PreviewMaterial;
    auto* M=UMaterialInstanceDynamic::Create(Base,this);
    SetAtlas(M,WorldData->Biomes[A].Definition.Terrain,TEXT("A"));
    auto Part=WorldData->Biomes[B].Definition.Terrain;
    // Terrain stores BASE; neighbouring atlas cells are directional transition tiles.
    const float W=Part.UV.Z-Part.UV.X,H=Part.UV.W-Part.UV.Y;
    Part.UV+=FVector4((Tile%3-1)*W,(Tile/3-1)*H,(Tile%3-1)*W,(Tile/3-1)*H);
    SetAtlas(M,Part,TEXT("B"));
    M->SetScalarParameterValue(TEXT("OutsideOpacity"),Entrance?0.12f:1.f);
    M->SetScalarParameterValue(TEXT("Opacity"),Entrance?0.12f:1.f);
    M->SetVectorParameterValue(TEXT("TintA"),WorldData->Biomes[A].Definition.Tint);
    M->SetVectorParameterValue(TEXT("TintB"),WorldData->Biomes[B].Definition.Tint);
    if (Landscape && TerrainTexturesTable)
    {
        for (int32 Side=0;Side<2;++Side)
        {
            const auto& Biome=WorldData->Biomes[Side?B:A].Definition;
            const FName Name=Landscape==1?Biome.ForegroundTexture:Biome.BackgroundTexture;
            if (const auto* Row=TerrainTexturesTable->FindRow<FPWGv2TerrainTextureRow>(Name,TEXT("Landscape"),false))
            {
                FPWGv2AtlasPart Texture; Texture.Texture=Row->Texture;
                const FString Suffix=Side?TEXT("B"):TEXT("A");
                SetAtlas(M,Texture,Suffix);
                M->SetScalarParameterValue(*FString(TEXT("UVRepeat")+Suffix),100.f/FMath::Max(25.f,Row->TilingWorldSize));
                M->SetVectorParameterValue(*FString(TEXT("Tint")+Suffix),Row->Tint*Biome.LandscapeTint*Biome.Tint*Row->Brightness);
            }
        }
    }
    AtlasMaterials.Add(Key,M); return M;
}

UMaterialInstanceDynamic* APWGv2Generator::WallMaterial(int32 Layer,bool Cover,bool Entrance,bool SolidCutaway,bool LitBackground)
{
    const auto& L=WorldData->Layers[Layer-1];
    const auto* Row=TerrainTexturesTable?TerrainTexturesTable->FindRow<FPWGv2TerrainTextureRow>(L.WallTexture,TEXT("Cave wall"),false):nullptr;
    FPWGv2AtlasPart Texture=L.Atlas;
    FLinearColor Tint=L.WallTint;
    if (Row) { Texture=FPWGv2AtlasPart(); Texture.Texture=Row->Texture; Tint*=Row->Tint*Row->Brightness; }
    if (SolidCutaway)
    {
        const FString Key=FString::Printf(TEXT("SolidCutaway:%d"),Layer);
        if (auto* Found=AtlasMaterials.Find(Key)) return *Found;
        auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Systems/Procedural_World_Generator/PWG_v2/M_PWGv2_SolidCutaway.M_PWGv2_SolidCutaway"));
        if (!Base)
        {
            UE_LOG(LogTemp,Error,TEXT("PWG: missing M_PWGv2_SolidCutaway; run Source/Tools/create_pwg_cutaway_material.py"));
            return AtlasMaterial(Texture,Tint*FLinearColor(.85f,.85f,.85f,1.f),true);
        }
        auto* M=UMaterialInstanceDynamic::Create(Base,this);
        SetAtlas(M,Texture,TEXT("")); M->SetVectorParameterValue(TEXT("Tint"),Tint);
        AtlasMaterials.Add(Key,M); return M;
    }
    if (!Cover) return AtlasMaterial(Texture,Tint,true,false,LitBackground);
    const FString Key=FString::Printf(TEXT("TunnelCover:%d:%d"),Layer,int32(Entrance));
    if (auto* Found=AtlasMaterials.Find(Key)) return *Found;
    auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Systems/Procedural_World_Generator/PWG_v2/M_PWGv2_LandscapeCover.M_PWGv2_LandscapeCover"));
    if (!Base) return AtlasMaterial(Texture,Tint,true);
    auto* M=UMaterialInstanceDynamic::Create(Base,this);
    SetAtlas(M,Texture,TEXT("A")); SetAtlas(M,Texture,TEXT("B"));
    M->SetVectorParameterValue(TEXT("TintA"),Tint); M->SetVectorParameterValue(TEXT("TintB"),Tint);
    M->SetScalarParameterValue(TEXT("OutsideOpacity"),Entrance?0.12f:1.f);
    M->SetScalarParameterValue(TEXT("Opacity"),1.f);
    AtlasMaterials.Add(Key,M); return M;
}

void APWGv2Generator::BuildDecorationsV2(int32 Index,float Start,float End)
{
    if (!CardMesh || WorldData->Biomes.IsEmpty()) return;
    auto& Saved=DecorationChunks.FindOrAdd(Index);
    struct FBatch { UStaticMesh* Mesh=nullptr; UMaterialInstanceDynamic* Material=nullptr; TArray<FTransform> Instances; bool Grass=false; };
    TMap<TPair<TPair<UMaterialInstanceDynamic*, UStaticMesh*>, bool>,FBatch> Batches;
    auto Add=[&](UStaticMesh* Mesh,UMaterialInstanceDynamic* Material,FVector P,float Width,float Height,bool IsGrass=false)
    {
        if (!Mesh || !Material) return;
        const TPair<TPair<UMaterialInstanceDynamic*, UStaticMesh*>, bool> Key(MakeTuple(Material,Mesh),IsGrass);
        auto& Batch=Batches.FindOrAdd(Key); Batch.Mesh=Mesh; Batch.Material=Material; Batch.Grass=IsGrass;
        Batch.Instances.Add(FTransform(FRotator::ZeroRotator,P,FVector(Width/100,Mesh==CardMesh?1.f:Width/100,Height/100)));
    };
    auto PickBiome=[&](float X,FRandomStream& R)->const FPWGv2BiomeRow&
    {
        int32 A,B; float Alpha; BiomeMix(X,A,B,Alpha);
        return WorldData->Biomes[R.FRand()<Alpha?B:A].Definition;
    };
    // A sprite can cover the opening even with its pivot on solid ground.
    // The cutaway exposes the foreground all the way to the back lip at Y=50.
    const auto CoversEntrance = [&](float X,float Y,float HalfWidth)
    {
        for (int32 I=0;I<WorldData->AuthoredPOIs.Num() && I<WorldData->AuthoredPOIPositions.Num();++I)
            if (WorldData->AuthoredPOIs[I].UpperLayer==0 && FMath::Abs(Y)<250 &&
                FMath::Abs(X-WorldData->AuthoredPOIPositions[I].X)<WorldData->AuthoredPOIs[I].Width*.5f+HalfWidth)
                return true;
        if (!Settings.bEnableUnderground || Y > 50.f) return false;
        for (const auto& Portal : WorldData->Portals)
            if (Portal.UpperLayer == 0 && FMath::Abs(X-Portal.X) < Portal.Width*.5f+HalfWidth)
                return true;
        return false;
    };
    const float Back=Settings.GrassLandscapeDepth>0?FMath::Min(Settings.BackgroundDepth,Settings.GrassLandscapeDepth):Settings.BackgroundDepth;
    TMap<TPair<const FPWGv2BiomeRow*, int32>, UMaterialInstanceDynamic*> GrassMaterials;
    auto ResolveGrassMaterial=[&](const FPWGv2BiomeRow& Biome,int32 FrameIndex)
    {
        auto& Material=GrassMaterials.FindOrAdd({&Biome,FrameIndex});
        if(!Material)Material=AtlasMaterial(Frame(Biome.Grass,FrameIndex),FLinearColor::White);
        return Material;
    };
    // Stable cell hashing: changing render depth never moves surviving instances.
    for (int32 CX=FMath::FloorToInt(Start/100);CX<=FMath::FloorToInt(End/100);++CX)
    {
    const auto Region=EcologyAt(CX*100.f);
    const float Density=FMath::Clamp(Region.GrassPerSquareMetre,0.f,12.f);
    for (int32 CY=FMath::FloorToInt(-Settings.ForegroundDepth/100);CY<=FMath::FloorToInt(Back/100);++CY)
    {
        FRandomStream R(HashCombineFast(HashCombineFast(WorldData->Seed,CX),CY)^0x675A55);
        const int32 Count=FMath::FloorToInt(Density)+(R.FRand()<FMath::Frac(Density)?1:0);
        for (int32 I=0;I<Count;++I)
        {
            const float X=(CX+R.FRand())*100,Y=(CY+R.FRand())*100;
            if (X<Start || X>=End || Y < -Settings.ForegroundDepth || Y>Back || CoversEntrance(X,Y,30.f)) continue;
            const float Z=Height(X,Y); if (Height(X)<=Settings.SeaLevel+2) continue;
            const auto& Biome=PickBiome(X,R); if (Biome.Grass.Texture.IsNull()) continue;
            // Conservative omission only below a permanently opaque bank; dynamic cutaway always disables it.
            if (Settings.bSkipPermanentlyHiddenGrass && !Settings.bEnableUnderground && Y>Settings.LaneSpacing+50 && Z+120<Height(X,-Settings.LaneSpacing-50)) continue;
            Add(CardMesh,ResolveGrassMaterial(Biome,R.RandRange(0,7)),FVector(X,Y,Z-2),R.FRandRange(35,60),R.FRandRange(35,75),true);
        }
    }
    }
    // Hang cards inside the cut, against the exposed back lip visible to the side camera.
    for (const auto& Portal : WorldData->Portals)
    {
        if (!Settings.bEnableUnderground || Portal.UpperLayer != 0 || Portal.Width <= 0) continue;
        const float Left = Portal.X - Portal.Width * 0.5f;
        if (Left >= End || Left + Portal.Width <= Start) continue;
        const int32 Count = FMath::CeilToInt(Portal.Width / 70.f);
        const float Width = Portal.Width / Count;
        for (int32 Tile = 0; Tile < Count; ++Tile)
        {
            const float X = Left + (Tile + 0.5f) * Width;
            if (X < Start || X >= End) continue;
            FRandomStream R(HashCombineFast(WorldData->Seed, FMath::RoundToInt(X)));
            const auto& Biome = PickBiome(X,R);
            if (Biome.Overhang.Texture.IsNull()) continue;
            const float RimY = 47.f - (Tile % 3) * 3.f;
            constexpr float CardHeight = 100.f;
            Add(CardMesh, AtlasMaterial(Frame(Biome.Overhang,R.RandRange(0,7)),FLinearColor::White),
                FVector(X,RimY,Height(X,50.f) - CardHeight + 22.f), Width * 1.55f,CardHeight);
        }
    }
    for (int32 Type=0;Type<3;++Type)
    {
        FRandomStream Random(HashCombineFast(WorldData->Seed,Index)^(0x7100+Type));
        const float MaxDensity=30;
        const int32 Candidates=FMath::CeilToInt((End-Start)*MaxDensity/10000);
        for (int32 I=0;I<Candidates;++I)
        {
            const float X=Random.FRandRange(Start,End); const auto Region=EcologyAt(X);
            const float Density=Type==0?Region.TreesPer100Metres:(Type==1?Region.BushesPer100Metres:Region.RocksPer100Metres);
            if (Random.FRand()>FMath::Clamp(Density/MaxDensity,0.f,1.f)) continue;
            const FString ObjectId=FString::Printf(TEXT("%s:%d:%d:%d"),*WorldData->WorldId.ToString(),Index,Type,I);
            if (WorldData->RemovedObjects.Contains(FName(*ObjectId))) continue;
            // Seeded path placement uses exact lane centres; trees never block the pawn.
            const bool Foreground=Random.FRand()<0.45f;
            float Y=Foreground?Random.FRandRange(-Settings.ForegroundDepth,-Settings.LaneSpacing-150):
                (Type==0?Random.FRandRange(Settings.LaneSpacing+100,Settings.BackgroundDepth):Random.FRandRange(-Settings.LaneSpacing,Settings.BackgroundDepth));
            if (Type==0 && Random.FRand()<FMath::Clamp(Region.TreesOnPathChance,0.f,1.f)) Y=Random.RandRange(-1,1)*Settings.LaneSpacing;
            const float Z=Height(X,Y); if (Height(X)<=Settings.SeaLevel+10 || IsOpening(0,X,Y)) continue;
            const auto& Biome=PickBiome(X,Random);
            if (Type==0 && !WorldData->TreeDefinitions.IsEmpty())
            {
                float Total=0; TArray<float> Weights;
                for (const auto& Tree:WorldData->TreeDefinitions) { const float W=PreferenceScore(Tree.Preference,X); Weights.Add(W); Total+=W; }
                if (Total<=0) continue;
                int32 Chosen=0; float Roll=Random.FRand()*Total;
                for (int32 N=0;N<Weights.Num();++N) if (Weights[N]>0 && (Roll-=Weights[N])<=0) { Chosen=N; break; }
                const FName TreeId(*ObjectId);
                const auto* TreeSave=WorldData->DamagedTrees.Find(TreeId);
                const auto& Tree=TreeSave?TreeSave->Definition:WorldData->TreeDefinitions[Chosen];
                const float H=Random.FRandRange(FMath::Clamp(float(Tree.HeightRange.GetMin()),200.f,1500.f),FMath::Clamp(float(Tree.HeightRange.GetMax()),200.f,1500.f));
                FRandomStream ShapeRandom(HashCombineFast(WorldData->Seed,GetTypeHash(ObjectId)));
                FName BiomeId;
                for (const auto& Span:WorldData->Biomes) if (&Span.Definition==&Biome) { BiomeId=Span.Id; break; }
                const auto HasWeights=[](const TArray<FPWGTreeShapeWeight>& W) { return W.ContainsByPredicate([](const FPWGTreeShapeWeight& V){return V.Weight>0 && FMath::IsFinite(V.Weight);}); };
                const auto& ShapeWeights=HasWeights(Tree.Shapes)?Tree.Shapes:(HasWeights(Biome.TreeShapes)?Biome.TreeShapes:Region.TreeShapes);
                const EPWGTreeShape Shape=APWGProceduralTree::SelectShape(ShapeWeights,RegionAt(X).Id,BiomeId,ShapeRandom);
                const FTransform Transform=FTransform(FVector(X,Y,Z))*GetActorTransform();
                auto* Actor=GetWorld()->SpawnActorDeferred<APWGProceduralTree>(APWGProceduralTree::StaticClass(),Transform,this,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
                if (Actor)
                {
                    UGameplayStatics::FinishSpawningActor(Actor,Transform);
                    Actor->Initialize(this,TreeId,Tree,Shape,ShapeRandom.GetCurrentSeed(),H,Region.WindStrength,
                        AtlasMaterial(Tree.Trunk,FLinearColor::White,false,true),AtlasMaterial(Tree.Crown,FLinearColor::White,false,true),
                        Biome.TreePlant.Texture.IsNull()?nullptr:AtlasMaterial(Frame(Biome.TreePlant,2),FLinearColor::White,false,true),Region.FruitMultiplier,TreeSave);
                    Saved.Actors.Add(Actor);
                    Saved.Trees.Add(Actor);
                }
            }
            else if (Type==1 && !Biome.Bush.Texture.IsNull() && !CoversEntrance(X,Y,65.f))
                Add(CardMesh,AtlasMaterial(Frame(Biome.Bush,Random.RandRange(0,3)),FLinearColor::White),FVector(X,Y,Z-5),130,100);
            else if (Type==2)
            {
                if (!Biome.Rock.Texture.IsNull()) Add(CardMesh,AtlasMaterial(Frame(Biome.Rock,Random.RandRange(0,3)),FLinearColor::White),FVector(X,Y,Z-8),100,80);
                else Add(RockMesh,PreviewMaterial,FVector(X,Y,Z+10),80,50);
            }
            // Interactive fruit uses existing pickup behavior with world-qualified persistent IDs.
            if (Type==1 && Random.FRand()<FMath::Clamp(Region.FruitMultiplier*0.3f,0.f,1.f))
            {
                const FString Id=ObjectId+TEXT(":fruit");
                const bool Collected=GetWorld()->GetSubsystem<UPlaceholderBerryWorld>()->Collected.Contains(Id) || WorldData->RemovedObjects.Contains(FName(*Id));
                if (!Biome.GroundPlant.Texture.IsNull() && !CoversEntrance(X,-Settings.LaneSpacing-2,47.5f)) Add(CardMesh,AtlasMaterial(Frame(Biome.GroundPlant,Collected?6:2),FLinearColor::White),FVector(X,-Settings.LaneSpacing-2,Height(X)-3),95,95);
                if (!GetWorld()->GetSubsystem<UPlaceholderBerryWorld>()->Collected.Contains(Id) && !WorldData->RemovedObjects.Contains(FName(*Id)))
                {
                    const float LaneY=-Settings.LaneSpacing;
                    const FTransform Spawn(GetActorTransform().TransformPosition(FVector(X,LaneY,Height(X,LaneY)+65)));
                    auto* Fruit=GetWorld()->SpawnActorDeferred<APlaceholderBerry>(APlaceholderBerry::StaticClass(),Spawn,this,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
                    if (Fruit) { Fruit->PersistentId=Id; Fruit->LaneY=GetActorLocation().Y+LaneY; Fruit->Tags.Add(TEXT("PWGSurfaceFruit")); UGameplayStatics::FinishSpawningActor(Fruit,Spawn); Saved.Actors.Add(Fruit); }
                }
            }
        }
    }
    for (auto& Pair:Batches)
    {
        auto& Batch=Pair.Value;
        auto* H=NewObject<UHierarchicalInstancedStaticMeshComponent>(this);
        H->ComponentTags.Add(TEXT("PWGSurfaceVisual"));
        H->SetupAttachment(GetRootComponent()); H->SetStaticMesh(Batch.Mesh); H->SetMaterial(0,Batch.Material);
        H->SetCollisionEnabled(ECollisionEnabled::NoCollision); H->SetCastShadow(false); H->SetCanEverAffectNavigation(false);
        const int32 Cull=Batch.Grass?FMath::Max(1000,Settings.GrassCullDistance):30000;
        H->SetCullDistances(FMath::Max(0,Cull-1000),Cull); H->RegisterComponent(); H->AddInstances(Batch.Instances,false,false);
        Saved.Components.Add(H);
        if (Batch.Grass)
        {
            Saved.GrassGroups.Add(H);
            // Instance render bounds can still be rebuilding asynchronously at this point.
            const FBox CardBounds = Batch.Mesh->GetBoundingBox();
            for (const FTransform& Instance : Batch.Instances)
                Saved.GrassBounds += CardBounds.TransformBy(Instance * H->GetComponentTransform());
        }
    }
}

void APWGv2Generator::ApplyChunkLayerVisibility(int32 Index)
{
    // Render state only: keep hulls, navigation, actors, simulation and streaming intact.
    if (auto* Surface=Chunks.FindRef(Index).Get())
    {
        // Keep the procedural component registered/visible; toggle actual draw sections,
        // not the component. This also restores the water and terrain after a layer switch.
        Surface->SetVisibility(true);
        for (int32 Section=0;Section<Surface->GetNumSections();++Section)
            if (const auto* Part=Surface->GetProcMeshSection(Section); Part && !Part->ProcVertexBuffer.IsEmpty())
                if (Surface->IsMeshSectionVisible(Section)==bUndergroundView)
                    Surface->SetMeshSectionVisible(Section,!bUndergroundView);
    }
    if (auto* Grass=GrassChunks.FindRef(Index).Get()) Grass->SetVisibility(!bUndergroundView);
    if (auto* Batch=DecorationChunks.Find(Index))
    {
        for (UHierarchicalInstancedStaticMeshComponent* Component:Batch->Components)
        {
            if (!IsValid(Component)) continue;
            if (Component->ComponentHasTag(TEXT("PWGSurfaceVisual"))) Component->SetVisibility(!bUndergroundView);
            else if (Component->ComponentHasTag(TEXT("PWGUndergroundVisual"))) Component->SetVisibility(bUndergroundView);
        }
        Batch->bGrassVisible=!bUndergroundView;
        for (const auto& WeakTree:Batch->Trees)
            if (auto* Tree=WeakTree.Get()) Tree->GetRootComponent()->SetVisibility(!bUndergroundView);
        for (AActor* Actor:Batch->Actors)
        {
            if (!IsValid(Actor) || Actor->GetOwner()!=this) continue;
            if (Actor->ActorHasTag(TEXT("PWGSurfaceFruit"))) Actor->SetActorHiddenInGame(bUndergroundView);
            if (auto* POI=Cast<APOIShaft>(Actor))
            {
                if (POI->ActorHasTag(TEXT("PWGSurfacePOI"))) POI->SetLayerRenderHidden(bUndergroundView);
                else if (POI->ActorHasTag(TEXT("PWGUndergroundPOI"))) POI->SetLayerRenderHidden(!bUndergroundView);
            }
        }
        if (auto* Mesh=UndergroundChunks.FindRef(Index).Get())
            for (int32 Section=0;Section<Batch->UndergroundSectionBounds.Num();++Section)
                if (Batch->UndergroundSectionBounds[Section].IsValid)
                {
                    const bool bVisible=bUndergroundView || IsSurfaceVisibleUndergroundSection(Section);
                    if (Mesh->IsMeshSectionVisible(Section)!=bVisible) Mesh->SetMeshSectionVisible(Section,bVisible);
                }
    }
}

void APWGv2Generator::RegisterTraversalVisual(UPrimitiveComponent* Component)
{
    TraversalVisuals.AddUnique(Component);
}

void APWGv2Generator::UpdateGameplayView(const FTransform& Camera, float ViewWidth, float Aspect)
{
    // Use the same padded frame for graphics at every depth, including long connecting shafts.
    const FVector Right = Camera.GetUnitAxis(EAxis::Y);
    const FVector Up = Camera.GetUnitAxis(EAxis::Z);
    constexpr float EdgeMargin = 100.f;
    const auto InView = [&](const FBox& Box)
    {
        const FVector Offset = Box.GetCenter() - Camera.GetLocation();
        const FVector Extent = Box.GetExtent();
        return ViewWidth <= 0.f || (
            FMath::Abs(Offset.Dot(Right)) <= ViewWidth * 0.5 + EdgeMargin + Extent.Dot(Right.GetAbs()) &&
            FMath::Abs(Offset.Dot(Up)) <= ViewWidth * 0.5 / Aspect + EdgeMargin + Extent.Dot(Up.GetAbs()));
    };
    for (int32 Index = TraversalVisuals.Num() - 1; Index >= 0; --Index)
    {
        auto* Mesh = TraversalVisuals[Index].Get();
        if (!Mesh)
        {
            TraversalVisuals.RemoveAtSwap(Index);
            continue;
        }
        // Keep nearby casters available while their shadow can still enter the frame.
        const bool bVisible = InView(Mesh->Bounds.GetBox().ExpandBy(300.f));
        if (Mesh->IsVisible() != bVisible) Mesh->SetVisibility(bVisible);
    }
    for (auto& Pair : DecorationChunks)
    {
        auto& Batch = Pair.Value;
        const bool bShowGrass = Batch.GrassBounds.IsValid && !bUndergroundView && InView(Batch.GrassBounds);
        if (Batch.bGrassVisible != bShowGrass)
        {
            Batch.bGrassVisible = bShowGrass;
            for (auto* Grass : Batch.GrassGroups) Grass->SetVisibility(Batch.bGrassVisible);
        }
        for (const auto& WeakTree : Batch.Trees)
        {
            if (auto* Tree = WeakTree.Get())
            {
                auto* Root = Tree->GetRootComponent();
                const bool bVisible = !bUndergroundView && InView(Root->Bounds.GetBox());
                if (Root->IsVisible() != bVisible) Root->SetVisibility(bVisible);
            }
        }
        if (auto* Mesh = UndergroundChunks.FindRef(Pair.Key).Get())
        {
            for (int32 Section = 0; Section < Batch.UndergroundSectionBounds.Num(); ++Section)
            {
                const FBox& Bounds = Batch.UndergroundSectionBounds[Section];
                if (!Bounds.IsValid) continue;
                const bool bVisible = (bUndergroundView || IsSurfaceVisibleUndergroundSection(Section)) && InView(Bounds);
                if (Mesh->IsMeshSectionVisible(Section) != bVisible)
                    Mesh->SetMeshSectionVisible(Section, bVisible);
            }
        }
    }
}
