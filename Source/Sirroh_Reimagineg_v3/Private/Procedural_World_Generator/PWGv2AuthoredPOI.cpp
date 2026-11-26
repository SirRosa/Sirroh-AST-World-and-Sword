#include "Procedural_World_Generator/PWGv2Generator.h"
#include "POI/POIArtPool.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Engine/World.h"

void APWGv2Generator::PlanAuthoredPOIs()
{
    WorldData->AuthoredPOIs.Reset(); WorldData->AuthoredPOIPositions.Reset();
    WorldData->RoomChains.Reset();
    // Snapshot the catalog at world creation, just like shaft recipes.
    for (int32 Layer:{0,1,3})
    {
        if (Layer && !Settings.bEnableUnderground) continue;
        auto* Table=LoadObject<UDataTable>(nullptr,Layer==0?
            TEXT("/Game/POIKit/Catalog/DT_UserSurfacePOIs.DT_UserSurfacePOIs"):
            (Layer==1?TEXT("/Game/POIKit/Catalog/DT_UserUndergroundPOIs.DT_UserUndergroundPOIs"):
            TEXT("/Game/POIKit/Catalog/DT_UserDungeonPOIs.DT_UserDungeonPOIs")));
        if (!Table || Table->GetRowStruct()!=FPOIShaftRow::StaticStruct()) continue;
        TArray<FPOIShaftRow> Pool;
        auto Names=Table->GetRowNames(); Names.Sort(FNameLexicalLess());
        for (FName Name:Names) if (auto* Row=Table->FindRow<FPOIShaftRow>(Name,TEXT("POI plan")))
        { FString Error; if (Row->Weight>0 && !Row->SectionLevel.IsNull() && Row->Validate(Error)) Pool.Add(*Row); }
        if (Pool.IsEmpty()) continue;
        if (Layer==1) { PlanRoomChains(Pool,Layer); continue; }
        FRandomStream Random(HashCombineFast(WorldData->Seed,0xA071+Layer));
        const float Start=WorldData->Regions[0].StartX+1500, End=WorldData->Regions.Last().EndX-1500;
        // One attempt every 80 metres; reject shafts, water and insufficient room clearance.
        for (float X=Start+Random.FRand()*2000;X<End;X+=8000)
        {
            float Total=0; for (const auto& Row:Pool) Total+=Row.Weight;
            float Draw=Random.FRand()*Total;
            FPOIShaftRow Row=Pool.Last();
            for (const auto& Candidate:Pool) { Draw-=Candidate.Weight; if (Draw<=0) {Row=Candidate; break;} }
            bool Blocked=false;
            for (const auto& Portal:WorldData->Portals)
                if (FMath::Abs(X-Portal.X)<Row.Width*.5+Portal.Width*.5+500) {Blocked=true; break;}
            if (Blocked) continue;
            const float Z=Layer==0?Height(X):LayerFloor(Layer,X);
            if (Layer==0 && Z<Settings.SeaLevel+100) continue;
            if (Layer)
            {
                for (float Sample:{X-Row.Width*.5f,X,X+Row.Width*.5f})
                    if (LayerCeiling(Layer,Sample)-Z<Row.SectionHeight+50 || FMath::Abs(LayerFloor(Layer,Sample)-Z)>10) Blocked=true;
                if (Blocked) continue;
            }
            Row.UpperLayer=Layer;
            WorldData->AuthoredPOIs.Add(Row); WorldData->AuthoredPOIPositions.Add(FVector(X,0,Z));
            if (!Layer)
            {
                FPOITerrainStamp Stamp;
                Stamp.Id=FName(*FString::Printf(TEXT("AuthoredPOI_%d"),WorldData->AuthoredPOIs.Num()));
                Stamp.Center=FVector2D(X,0); Stamp.HalfExtent=FVector2D(Row.Width*.5+50,200);
                Stamp.Height=Z; Stamp.BlendDistance=400; Stamp.Priority=100;
                WorldData->POITerrainStamps.Add(Stamp);
            }
        }
    }
    FString Error; FPOITerrainStamp::ValidateAndSort(WorldData->POITerrainStamps,Error);
}

bool APWGv2Generator::IsRoomChain(int32 Layer,float X,float Margin) const
{
    for (const auto& Chain:WorldData->RoomChains)
        if (Chain.Layer==Layer && X>=Chain.StartX-Margin && X<=Chain.EndX+Margin) return true;
    return false;
}

bool APWGv2Generator::DebugRoomChainDestination(int32 Index,float CapsuleHalfHeight,FVector& Out)
{
    if (!WorldData || !WorldData->RoomChains.IsValidIndex(Index)) return false;
    const auto& Chain=WorldData->RoomChains[Index];
    const float X=Chain.StartX+100;
    StreamAt(X,true);
    GetWorld()->FlushLevelStreaming(EFlushLevelStreamingType::Full);
    Out=GetActorTransform().TransformPosition(FVector(X,0,Chain.FloorZ+CapsuleHalfHeight+5));
    return true;
}

void APWGv2Generator::PlanRoomChains(const TArray<FPOIShaftRow>& Pool,int32 Layer)
{
    if (WorldData->Regions.IsEmpty() || WorldData->Layers.Num()!=4) return;
    TArray<int32> Entrances;
    for (int32 I=0;I<Pool.Num();++I)
        if (Pool[I].Category.ToString().StartsWith(TEXT("Entrance")) && Pool[I].Weight>0) Entrances.Add(I);
    if (Entrances.IsEmpty()) return;
    FRandomStream Random(HashCombineFast(WorldData->Seed,0xC011+Layer));
    auto Pick=[&](const TArray<int32>& Options,int32 Previous)->int32
    {
        float Total=0;
        for (int32 I:Options) if (Options.Num()==1 || I!=Previous) Total+=Pool[I].Weight;
        float Draw=Random.FRand()*Total;
        for (int32 I:Options) if (Options.Num()==1 || I!=Previous)
        { Draw-=Pool[I].Weight; if (Draw<=0) return I; }
        return Options.Last();
    };
    const float End=WorldData->Regions.Last().EndX-1500;
    for (float Start=WorldData->Regions[0].StartX+1500;Start<End;Start+=2000)
    {
        const int32 First=Pick(Entrances,INDEX_NONE);
        const float Height=Pool[First].SectionHeight;
        TMap<FName,TArray<int32>> Groups;
        TArray<int32> CompatibleEntrances;
        for (int32 I=0;I<Pool.Num();++I)
        {
            if (Pool[I].Weight<=0 || !FMath::IsNearlyEqual(Pool[I].SectionHeight,Height,.1f)) continue;
            if (Entrances.Contains(I)) CompatibleEntrances.Add(I);
            else if (!Pool[I].Category.IsNone()) Groups.FindOrAdd(Pool[I].Category).Add(I);
        }
        TArray<FName> Categories; Groups.GetKeys(Categories); Categories.Sort(FNameLexicalLess());
        if (Categories.IsEmpty()) continue;
        // Shuffle category bags: variety is independent of how many variants each category has.
        TArray<int32> Sequence; Sequence.Add(First);
        TArray<FName> Bag;
        const int32 InteriorCount=Random.RandRange(4,7);
        for (int32 N=0;N<InteriorCount;++N)
        {
            if (Bag.IsEmpty())
            {
                Bag=Categories;
                for (int32 K=Bag.Num()-1;K>0;--K) Bag.Swap(K,Random.RandRange(0,K));
            }
            const FName Category=Bag.Pop();
            Sequence.Add(Pick(Groups[Category],Sequence.Last()));
        }
        Sequence.Add(Pick(CompatibleEntrances,First));
        float Width=0; for (int32 I:Sequence) Width+=Pool[I].Width;
        if (Start+Width>End) continue;
        bool Blocked=false;
        for (const auto& P:WorldData->Portals)
            if ((P.UpperLayer<=Layer && P.LowerLayer>=Layer) && P.X+P.Width*.5f>Start-1400 && P.X-P.Width*.5f<Start+Width+1400) Blocked=true;
        if (Blocked) continue;
        float Floor=LayerFloor(Layer,Start+Width*.5f);
        for (float X=Start-800;X<=Start+Width+800;X+=100)
        {
            Floor=FMath::Min(Floor,LayerFloor(Layer-1,X)-Height-150);
            if (Layer==1 && RegionAt(X).Definition.EntranceStyle==EPWGv2EntranceStyle::None) Blocked=true;
        }
        if (Layer<4) for (float X=Start-800;X<=Start+Width+800;X+=100)
            if (Floor<LayerCeiling(Layer+1,X)+150) Blocked=true;
        if (Blocked) continue;
        FPWGv2RoomChain Chain; Chain.Layer=Layer; Chain.StartX=Start; Chain.EndX=Start+Width;
        Chain.FloorZ=Floor; Chain.ClearHeight=Height; Chain.FirstPiece=WorldData->AuthoredPOIs.Num(); Chain.PieceCount=Sequence.Num();
        float Cursor=Start;
        for (int32 I:Sequence)
        {
            auto Row=Pool[I]; Row.UpperLayer=Layer;
            WorldData->AuthoredPOIs.Add(Row);
            WorldData->AuthoredPOIPositions.Add(FVector(Cursor+Row.Width*.5f,0,Floor));
            Cursor+=Row.Width;
        }
        WorldData->RoomChains.Add(Chain);
        Start+=Width+2000;
    }
    UE_LOG(LogTemp,Display,TEXT("POI room chains: layer %d, total chains %d, total fragments %d"),Layer,WorldData->RoomChains.Num(),WorldData->AuthoredPOIs.Num());
}

void APWGv2Generator::BuildAuthoredPOIs(int32 Index,float Start,float End)
{
    for (int32 I=0;I<WorldData->AuthoredPOIs.Num() && I<WorldData->AuthoredPOIPositions.Num();++I)
    {
        const FVector Local=WorldData->AuthoredPOIPositions[I];
        if (Local.X<Start || Local.X>=End) continue;
        FActorSpawnParameters Params; Params.Owner=this;
        auto* POI=GetWorld()->SpawnActor<APOIShaft>(APOIShaft::StaticClass(),GetActorTransform().TransformPosition(Local),FRotator::ZeroRotator,Params);
        if (!POI) continue;
        POI->Recipe=WorldData->AuthoredPOIs[I]; POI->Height=POI->Recipe.SectionHeight;
        POI->Tags.Add(POI->Recipe.UpperLayer==0?TEXT("PWGSurfacePOI"):TEXT("PWGUndergroundPOI"));
        POI->bProjectBackground=HasProjectedPOIBackground(I);
        POI->BackgroundMode=ResolveBackgroundMode(POI->Recipe);
        POI->SetBackgroundAuditSuppressed(IsBackgroundAuditNativeSuppressed());
        POI->Seed=HashCombineFast(WorldData->Seed,I+0xA071); POI->bBuildBoundaryWalls=false;
        POI->BuildShaft(); DecorationChunks.FindOrAdd(Index).Actors.Add(POI);
    }
}

bool APWGv2Generator::HasProjectedPOIBackground(int32 I) const
{
    if(!WorldData || !WorldData->AuthoredPOIs.IsValidIndex(I)) return false;
    const auto& Row=WorldData->AuthoredPOIs[I];
    if(Row.UpperLayer<=0 || ResolveBackgroundMode(Row)!=EPOIBackgroundMode::Enclosed || Row.ArtTheme.IsNull()) return false;
    auto* Theme=Row.ArtTheme.LoadSynchronous();
    if(!Theme || !Theme->Backgrounds || Theme->Backgrounds->Images.IsEmpty()) return false;
    FRandomStream Random(HashCombineFast(WorldData->Seed,I+0xB671));
    const auto& Pair=Theme->Backgrounds->Images[Random.RandRange(0,Theme->Backgrounds->Images.Num()-1)];
    return Pair.Side.LoadSynchronous()!=nullptr &&
        (LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/POIKit/M_KitImageFit_LitBackground.M_KitImageFit_LitBackground")) || Theme->Material.LoadSynchronous());
}

bool APWGv2Generator::UsesUnifiedBackground() const
{
#if !UE_BUILD_SHIPPING
    // Only an explicit QA launch can reproduce the previous geometry for isolation.
    return !FParse::Param(FCommandLine::Get(),TEXT("PWGBackgroundAuditLegacy"));
#else
    return true;
#endif
}
EPOIBackgroundMode APWGv2Generator::ResolveBackgroundMode(const FPOIShaftRow& Row) const
{
    if(!UsesUnifiedBackground() || Row.UpperLayer<=0) return EPOIBackgroundMode::Enclosed;
#if !UE_BUILD_SHIPPING
    if(FParse::Param(FCommandLine::Get(),TEXT("PWGBackgroundAuditEnclosed"))) return EPOIBackgroundMode::Enclosed;
#endif
    if(!Row.SectionLevel.IsNull())
        if(const auto* Mode=BackgroundModesBySection.Find(FName(*Row.SectionLevel.ToString()))) return *Mode;
    return BackgroundMode;
}
