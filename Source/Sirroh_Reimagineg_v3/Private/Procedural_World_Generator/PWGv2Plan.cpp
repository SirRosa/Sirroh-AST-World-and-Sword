#include "Items/SirrohASTInventoryComponent.h"
#include "Procedural_World_Generator/PWGv2Generator.h"
#include "Procedural_World_Generator/PWGProceduralTree.h"
#include "Procedural_World_Generator/PWGv2WorldSession.h"
#include "PWG/PWGEnvironmentV2.h"
#include "Placeholder/PlaceholderBerryWorld.h"
#include "Sword/SwordWorldSubsystem.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"

float APWGv2Generator::PreferenceScore(const FPWGv2Preference& P,float X) const
{
    const auto& R=RegionAt(X).Definition;
    const float H=(Height(X)-Settings.SeaLevel)/100;
    const float HError=FMath::Max(0.f,FMath::Max(static_cast<float>(P.HeightMetres.X)-H,H-static_cast<float>(P.HeightMetres.Y)))/20;
    const float Vegetation=FMath::Clamp(R.GrassPerSquareMetre/6,0.f,1.f);
    const float Flatness=1-FMath::Clamp(FMath::Abs(Height(X+100)-Height(X-100))/200,0.f,1.f);
    const float Coast=FMath::Min(FMath::Abs(X-WorldData->Regions[0].StartX),FMath::Abs(X-WorldData->Regions.Last().EndX))/100;
    const float WaterDistance=H<2?0:Coast;
    const float Error=HError+FMath::Abs(R.Moisture-P.Moisture)+FMath::Abs(Vegetation-P.Vegetation)+0.5f*FMath::Abs(Flatness-P.Flatness)+FMath::Min(2.f,FMath::Abs(WaterDistance-P.DistanceToWaterMetres)/150);
    return FMath::Max(0.f,P.Weight)*FMath::Max(FMath::Clamp(P.MinimumSuitability,0.001f,1.f),FMath::Exp(-Error*1.5f));
}

void APWGv2Generator::MakeExtendedPlan()
{
    WorldData->Biomes.Reset(); WorldData->TreeDefinitions.Reset(); WorldData->Layers.Reset(); WorldData->Portals.Reset();
    WorldData->RoomChains.Reset(); WorldData->AuthoredPOIs.Reset(); WorldData->AuthoredPOIPositions.Reset();
    TArray<FName> Names=BiomesTable?BiomesTable->GetRowNames():TArray<FName>(); Names.Sort(FNameLexicalLess());
    FRandomStream Random(HashCombineFast(WorldData->Seed,0xB10AE));
    for (const auto& Region:WorldData->Regions)
    {
        const int32 Count=FMath::Clamp(FMath::RoundToInt((Region.EndX-Region.StartX)/(100*FMath::Clamp(Settings.BiomeLengthMetres,64.f,2000.f))),2,3);
        for (int32 I=0;I<Count;++I)
        {
            FPWGv2BiomeSpan Span; Span.StartX=FMath::Lerp(Region.StartX,Region.EndX,float(I)/Count); Span.EndX=FMath::Lerp(Region.StartX,Region.EndX,float(I+1)/Count);
            TArray<float> Weights; float Total=0;
            for (FName Name:Names)
            {
                const auto* Row=BiomesTable->FindRow<FPWGv2BiomeRow>(Name,TEXT("Biome plan"));
                const bool Repeated=Names.Num()>1 && !WorldData->Biomes.IsEmpty() && WorldData->Biomes.Last().Id==Name;
                const bool Allowed=Region.Definition.AllowedBiomes.IsEmpty() || Region.Definition.AllowedBiomes.Contains(Name);
                const float W=Row && Allowed && !Repeated && Row->MinLengthMetres*100<=Span.EndX-Span.StartX?PreferenceScore(Row->Preference,(Span.StartX+Span.EndX)/2):0;
                Weights.Add(W); Total+=W;
            }
            if (Total<=0)
            {
                // If two biomes cannot fit, retry the entire region as one span.
                if (Count==2 && I==0)
                {
                    Span.EndX=Region.EndX;
                    for (int32 N=0;N<Names.Num();++N)
                    {
                        const auto* Row=BiomesTable->FindRow<FPWGv2BiomeRow>(Names[N],TEXT("Biome plan"));
                        const bool Allowed=Region.Definition.AllowedBiomes.IsEmpty() || Region.Definition.AllowedBiomes.Contains(Names[N]);
                        Weights[N]=Row && Allowed && Row->MinLengthMetres*100<=Span.EndX-Span.StartX?PreferenceScore(Row->Preference,(Span.StartX+Span.EndX)/2):0; Total+=Weights[N];
                    }
                }
            }
            Span.Id=TEXT("Fallback"); Span.Definition.Tint=Region.Definition.PreviewColor;
            if (Total>0)
            {
                float Roll=Random.FRand()*Total;
                for (int32 N=0;N<Names.Num();++N) if (Weights[N]>0 && (Roll-=Weights[N])<=0)
                { Span.Id=Names[N]; Span.Definition=*BiomesTable->FindRow<FPWGv2BiomeRow>(Names[N],TEXT("Biome plan")); break; }
            }
            WorldData->Biomes.Add(Span);
            if (Span.EndX==Region.EndX) break;
        }
    }
    if (TreesTable)
    {
        auto Trees=TreesTable->GetRowNames(); Trees.Sort(FNameLexicalLess());
        for (FName Name:Trees) WorldData->TreeDefinitions.Add(*TreesTable->FindRow<FPWGv2TreeRow>(Name,TEXT("Tree plan")));
    }
    for (int32 Layer=1;Layer<=4;++Layer)
    {
        FPWGv2UndergroundRow Row; Row.Layer=Layer; Row.FloorBelowSeaMetres=Layer==1?4:Layer*20-20;
        Row.ClearHeightMetres=Layer==2?8:5; Row.bRectangular=Layer>=3;
        if (UndergroundTable)
            for (FName Name:UndergroundTable->GetRowNames())
                if (const auto* Candidate=UndergroundTable->FindRow<FPWGv2UndergroundRow>(Name,TEXT("Underground plan")); Candidate->Layer==Layer) { Row=*Candidate; break; }
        Row.ClearHeightMetres=FMath::Clamp(Row.ClearHeightMetres,2.5f,15.f);
        Row.RoomLengthMetres=FMath::Clamp(Row.RoomLengthMetres,6.f,60.f);
        Row.FloorVariationMetres=FMath::Clamp(Row.FloorVariationMetres,0.f,2.f);
        const float MinDepth=Layer==1?3:WorldData->Layers.Last().FloorBelowSeaMetres+Row.ClearHeightMetres+5;
        Row.FloorBelowSeaMetres=FMath::Clamp(Row.FloorBelowSeaMetres,MinDepth,MinDepth+100);
        WorldData->Layers.Add(Row);
    }
    if (!Settings.bEnableUnderground) { PlanAuthoredPOIs(); return; }
    auto AddPortal=[&](float X,int32 Upper,int32 Lower,EPWGv2EntranceStyle Style)
    {
        FPWGv2Portal P; P.X=FMath::GridSnap(X,Settings.SampleSpacing); P.Width=600;
        P.UpperLayer=Upper; P.LowerLayer=Lower; P.Style=Style; WorldData->Portals.Add(P);
    };
    FRandomStream Entrances(HashCombineFast(WorldData->Seed,0xCA7E));
    for (const auto& R:WorldData->Regions)
    {
        if (R.Definition.EntranceStyle==EPWGv2EntranceStyle::None) continue;
        const int32 Capacity=FMath::Max(1,FMath::FloorToInt((R.EndX-R.StartX)/2500));
        const int32 Min=FMath::Clamp(R.Definition.MinEntrances,1,Capacity);
        const int32 Count=Entrances.RandRange(Min,FMath::Clamp(R.Definition.MaxEntrances,Min,Capacity));
        for (int32 I=0;I<Count;++I)
        {
            float X=FMath::Lerp(R.StartX+1000,R.EndX-1000,float(I+1)/(Count+1));
            if (FMath::Abs(X)<1500) X=X<0?-1800:1800;
            X=FMath::Clamp(X,R.StartX+500,R.EndX-500);
            const int32 Target=(!bUseKitShafts && R.Definition.bDirectToCaves)?2:1;
            AddPortal(X,0,Target,R.Definition.EntranceStyle);
            if (Target==1)
            {
                // The lower exit must be a floor, not another shaft at the same X.
                const float Direction=X<(R.StartX+R.EndX)*.5f?1.f:-1.f;
                const float NextX=FMath::Clamp(X+Direction*1800.f,R.StartX+400.f,R.EndX-400.f);
                if (FMath::Abs(NextX-X)>=900.f) AddPortal(NextX,1,2,EPWGv2EntranceStyle::Switchback);
            }
        }
    }
    // Weighted candidates from every biome, then a global quota of 2..3 distinct entrances.
    TArray<float> Candidates; for (const auto& B:WorldData->Biomes) Candidates.Add((B.StartX+B.EndX)/2);
    for (int32 I=Candidates.Num()-1;I>0;--I) Candidates.Swap(I,Entrances.RandRange(0,I));
    const int32 Count=Entrances.RandRange(2,3);
    if (Candidates.Num()<Count)
    {
        Candidates.Reset(); for (int32 I=0;I<Count;++I) Candidates.Add(FMath::Lerp(WorldData->Regions[0].StartX+800,WorldData->Regions.Last().EndX-800,float(I+1)/(Count+1)));
    }
    for (int32 I=0;I<Count;++I) AddPortal(Candidates[I],2,3,EPWGv2EntranceStyle::Switchback);
    for (int32 I=0;I<2;++I) AddPortal(FMath::Lerp(WorldData->Regions[0].StartX,WorldData->Regions.Last().EndX,float(I+1)/3),3,4,EPWGv2EntranceStyle::Switchback);
    if (bUseKitShafts)
    {
        UDataTable* Table=ShaftVariants;
        if (!Table) Table=LoadObject<UDataTable>(nullptr,TEXT("/Game/POIKit/DT_ShaftModules.DT_ShaftModules"));
        if (!Table) Table=LoadObject<UDataTable>(nullptr,TEXT("/Game/POIKit/DT_ShaftVariants.DT_ShaftVariants"));
        TArray<FPOIShaftRow> Recipes;
        if (Table && Table->GetRowStruct()==FPOIShaftRow::StaticStruct())
        {
            TArray<FName> ShaftNames=Table->GetRowNames(); ShaftNames.Sort([](FName A,FName B){return A.LexicalLess(B);});
            for (FName Name:ShaftNames) if (const auto* Row=Table->FindRow<FPOIShaftRow>(Name,TEXT("Shaft variants")))
            { FString Error; if (Row->Weight>0 && Row->Validate(Error)) Recipes.Add(*Row); }
        }
        // The editor's Save POI menu registers modules here. No generator setup required.
        if (auto* UserTable=LoadObject<UDataTable>(nullptr,TEXT("/Game/POIKit/Catalog/DT_UserShaftModules.DT_UserShaftModules")))
        {
            if (UserTable->GetRowStruct()==FPOIShaftRow::StaticStruct())
            {
                auto UserNames=UserTable->GetRowNames(); UserNames.Sort([](FName A,FName B){return A.LexicalLess(B);});
                for (FName Name:UserNames)
                    if (const auto* Row=UserTable->FindRow<FPOIShaftRow>(Name,TEXT("User POI")))
                    { FString Error; if (Row->Weight>0 && Row->Validate(Error)) Recipes.Add(*Row); }
            }
        }
        FRandomStream ShaftRandom(HashCombineFast(WorldData->Seed,0x5AAF7));
        for (auto& Portal:WorldData->Portals)
        {
            if (Portal.LowerLayer!=Portal.UpperLayer+1) continue;
            TArray<int32> Matches; float Total=0;
            for (int32 I=0;I<Recipes.Num();++I) if (Recipes[I].UpperLayer==Portal.UpperLayer && Recipes[I].Role==EPOIShaftRole::RepeatedSection) { Matches.Add(I); Total+=Recipes[I].Weight; }
            // Native fallback keeps all four transitions usable even without authored content.
            FPOIShaftRow Recipe; Recipe.UpperLayer=Portal.UpperLayer;
            if (!Matches.IsEmpty())
            {
                float Draw=ShaftRandom.FRand()*Total;
                for (int32 I:Matches) { Recipe=Recipes[I]; Draw-=Recipe.Weight; if (Draw<=0) break; }
            }
            if (Matches.IsEmpty())
            {
                TArray<int32> EntranceCandidates;
                float EntranceWeight=0;
                for (int32 I=0;I<Recipes.Num();++I)
                    if (Recipes[I].UpperLayer==Portal.UpperLayer && Recipes[I].Role==EPOIShaftRole::Entrance)
                    { EntranceCandidates.Add(I); EntranceWeight+=Recipes[I].Weight; }
                float Draw=ShaftRandom.FRand()*EntranceWeight;
                for (int32 I:EntranceCandidates)
                {
                    Recipe=Recipes[I]; Draw-=Recipe.Weight;
                    if (Draw<=0) break;
                }
                Recipe.Role=EPOIShaftRole::RepeatedSection;
                Recipe.bUseCustomSection=false;
                Recipe.SectionLevel.Reset();
                Recipe.Pieces.Reset(); Recipe.NativePieces.Reset();
            }
            for (const auto& Module:Recipes)
                if (Module.UpperLayer==Portal.UpperLayer && Module.Role!=EPOIShaftRole::RepeatedSection &&
                    FMath::IsNearlyEqual(Module.Width,Recipe.Width)) Portal.ShaftModules.Add(Module);
            Portal.ShaftSeed=ShaftRandom.RandRange(1,MAX_int32-1);
            Portal.bKitShaft=true; Portal.ShaftRecipe=Recipe; Portal.Width=Recipe.Width;
        }
    }
    PlanShaftTerrain();
    PlanAuthoredPOIs();
}

void APWGv2Generator::BiomeMix(float X,int32& A,int32& B,float& Alpha) const
{
    A=0; B=0; Alpha=0;
    if (!WorldData || WorldData->Biomes.IsEmpty()) return;
    while (A+1<WorldData->Biomes.Num() && X>WorldData->Biomes[A].EndX) ++A;
    B=A;
    for (int32 N=FMath::Max(0,A-1);N<=A && N+1<WorldData->Biomes.Num();++N)
    {
        const auto& Left=WorldData->Biomes[N]; const auto& Right=WorldData->Biomes[N+1];
        const float Half=FMath::Max(1.f,FMath::Min3(Left.Definition.BlendMetres*50,(Left.EndX-Left.StartX)*0.25f,(Right.EndX-Right.StartX)*0.25f));
        if (FMath::Abs(X-Left.EndX)<=Half)
        { A=N; B=N+1; Alpha=FMath::SmoothStep(0.f,1.f,(X-Left.EndX+Half)/(2*Half)); return; }
    }
}

void APWGv2Generator::CaptureVisit()
{
    if (!WorldData || !bVisitReady) return;
    if (const auto* Berries=GetWorld()->GetSubsystem<UPlaceholderBerryWorld>())
        for (const auto& Id:Berries->Collected) if (Id.StartsWith(WorldData->WorldId.ToString()+TEXT(":"))) WorldData->RemovedObjects.Add(FName(*Id));
    if (const auto* Swords=GetWorld()->GetSubsystem<USwordWorldSubsystem>())
        for (const auto& Id:Swords->RemovedFromSpawn) if (Id.StartsWith(WorldData->WorldId.ToString()+TEXT(":"))) WorldData->RemovedObjects.Add(FName(*Id));
    const auto* Session=GetGameInstance()->GetSubsystem<UPWGv2WorldSession>();
    const APawn* Pawn=UGameplayStatics::GetPlayerPawn(this,0);
    if (!Session || !Session->CharacterId.IsValid() || !Pawn) return;
    FPWGv2Visit Visit=WorldData->Visits.FindRef(Session->CharacterId); Visit.Position=Pawn->GetActorLocation();
    if(const auto* Inventory=Pawn->FindComponentByClass<USirrohASTInventoryComponent>();Inventory && Inventory->IsNativeInventoryReady())
        Visit.NativeInventory=Inventory->CaptureNativeItems();
    Visit.Lane=FMath::Clamp(FMath::RoundToInt((Visit.Position.Y-GetActorLocation().Y)/Settings.LaneSpacing)+1,0,2);
    WorldData->Visits.Add(Session->CharacterId,Visit);
}
bool APWGv2Generator::GetSavedArrival(FVector& Position,int32& Lane)
{
    bVisitReady=true;
    const auto* Session=GetGameInstance()->GetSubsystem<UPWGv2WorldSession>();
    const auto* Visit=Session && WorldData?WorldData->Visits.Find(Session->CharacterId):nullptr;
    if (!Visit) return false;
    const FVector Local=GetActorTransform().InverseTransformPosition(Visit->Position);
    if (!FMath::IsFinite(Local.X)||!FMath::IsFinite(Local.Z)||Local.X<WorldData->Regions[0].StartX||Local.X>WorldData->Regions.Last().EndX) return false;
    Lane=FMath::Clamp(Visit->Lane,0,LayerAt(Visit->Position)>0?1:2);
    // Returning to a world places the character on a stable floor, never midway through a fall.
    int32 Layer=LayerAt(Visit->Position);
    if (Layer==1 && RegionAt(Local.X).Definition.EntranceStyle==EPWGv2EntranceStyle::None) Layer=2;
    if (Layer>=3) Lane=0;
    if (IsOpening(Layer,Local.X,(Lane-1)*Settings.LaneSpacing)) Lane=0;
    const float Y=(Lane-1)*Settings.LaneSpacing;
    Position=GetActorTransform().TransformPosition(FVector(Local.X,Y,Layer==0?Height(Local.X,Y):LayerFloor(Layer,Local.X)));
    StreamAt(Local.X);
    // Room modules can occupy the old flat floor. Resume on their actual top, not inside a solid block.
    if (Layer>0)
    {
        FHitResult Hit; FCollisionQueryParams Query(SCENE_QUERY_STAT(PWGArrival),false,this);
        const FVector From=GetActorTransform().TransformPosition(FVector(Local.X,Y,LayerCeiling(Layer,Local.X)-190));
        const FVector To=Position-FVector(0,0,100);
        // Terrain components belong to this actor, so do not ignore the generator itself.
        Query.ClearIgnoredActors();
        if (GetWorld()->LineTraceSingleByObjectType(Hit,From,To,FCollisionObjectQueryParams(ECC_WorldStatic),Query) && !Hit.bStartPenetrating && Hit.ImpactNormal.Z>0.5f)
            Position=Hit.ImpactPoint;
    }
    return true;
}
void APWGv2Generator::EndPlay(const EEndPlayReason::Type Reason)
{
    if (WorldData && bVisitReady) { CaptureVisit(); SaveWorld(); }
    for (const auto& Batch:DecorationChunks) for (AActor* Actor:Batch.Value.Actors)
        if (auto* Tree=Cast<APWGProceduralTree>(Actor); IsValid(Tree)) Tree->Destroy();
    if (IsValid(EnvironmentV2)) EnvironmentV2->Destroy();
    Super::EndPlay(Reason);
}
bool APWGv2Generator::MarkObjectRemoved(FName Id)
{
    if (!WorldData || Id.IsNone()) return false;
    WorldData->RemovedObjects.Add(Id); return SaveWorld();
}


FPWGv2RegionRow APWGv2Generator::EcologyAt(float X) const
{
    FPWGv2RegionRow Result=RegionAt(X).Definition;
    for (int32 I=1;I<WorldData->Regions.Num();++I)
    {
        const float Half=Settings.RegionBlendMetres*50,Edge=WorldData->Regions[I].StartX;
        if (FMath::Abs(X-Edge)>Half) continue;
        const auto& A=WorldData->Regions[I-1].Definition; const auto& B=WorldData->Regions[I].Definition;
        const float T=FMath::SmoothStep(0.f,1.f,(X-Edge+Half)/(2*Half));
        Result.MinHeightMetres=FMath::Lerp(A.MinHeightMetres,B.MinHeightMetres,T);
        Result.MaxHeightMetres=FMath::Lerp(A.MaxHeightMetres,B.MaxHeightMetres,T);
        Result.GrassPerSquareMetre=FMath::Lerp(A.GrassPerSquareMetre,B.GrassPerSquareMetre,T);
        Result.TreesPer100Metres=FMath::Lerp(A.TreesPer100Metres,B.TreesPer100Metres,T);
        Result.BushesPer100Metres=FMath::Lerp(A.BushesPer100Metres,B.BushesPer100Metres,T);
        Result.RocksPer100Metres=FMath::Lerp(A.RocksPer100Metres,B.RocksPer100Metres,T);
        Result.Moisture=FMath::Lerp(A.Moisture,B.Moisture,T);
        Result.FruitMultiplier=FMath::Lerp(A.FruitMultiplier,B.FruitMultiplier,T);
        Result.WindStrength=FMath::Lerp(A.WindStrength,B.WindStrength,T);
        Result.CloudMultiplier=FMath::Lerp(A.CloudMultiplier,B.CloudMultiplier,T);
        break;
    }
    return Result;
}
