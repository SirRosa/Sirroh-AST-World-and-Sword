#include "Procedural_World_Generator/PWGv2Generator.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWGRoomChainTest,"Sirroh.POI.RoomChains",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPWGRoomChainTest::RunTest(const FString& Parameters)
{
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    auto* G=World->SpawnActor<APWGv2Generator>(); G->WorldData=NewObject<UPWGv2WorldSave>(G);
    auto* D=G->WorldData.Get(); D->Seed=123; D->MinX=-50000; D->MaxX=50000;
    G->Settings.SampleSpacing=100; G->Settings.bEnableUnderground=true;
    D->Heights.Init(2000,1001);
    FPWGv2RegionSpan Region; Region.StartX=-50000; Region.EndX=50000; D->Regions.Add(Region);
    for (int32 I=1;I<=4;++I) { FPWGv2UndergroundRow L; L.Layer=I; L.FloorBelowSeaMetres=I*30; L.ClearHeightMetres=5; D->Layers.Add(L); }
    FPWGv2Portal Portal; Portal.X=0; Portal.Width=600; Portal.UpperLayer=0; Portal.LowerLayer=1; D->Portals.Add(Portal);
    TArray<FPOIShaftRow> Pool;
    for (const TCHAR* Category:{TEXT("Entrance_U"),TEXT("Pushable_U"),TEXT("Light_U"),TEXT("Empty_U"),TEXT("Parkour_U")})
        for (int32 V=0;V<10;++V)
        {
            FPOIShaftRow R; R.Category=Category; R.Width=800+V*20; R.SectionHeight=400;
            R.SectionLevel=TSoftObjectPtr<UWorld>(FSoftObjectPath(FString::Printf(TEXT("/Game/Test_%s_%d.Test_%s_%d"),Category,V,Category,V))); Pool.Add(R);
        }
    G->PlanRoomChains(Pool,1);
    TestTrue(TEXT("Chains generated on uneven terrain"),D->RoomChains.Num()>0);
    for (const auto& C:D->RoomChains)
    {
        TestTrue(TEXT("6..9 complete fragments"),C.PieceCount>=6 && C.PieceCount<=9);
        TestEqual(TEXT("Starts with entrance"),D->AuthoredPOIs[C.FirstPiece].Category,FName(TEXT("Entrance_U")));
        TestEqual(TEXT("Ends with entrance"),D->AuthoredPOIs[C.FirstPiece+C.PieceCount-1].Category,FName(TEXT("Entrance_U")));
        TSet<FName> Categories; float X=C.StartX;
        for (int32 I=C.FirstPiece;I<C.FirstPiece+C.PieceCount;++I)
        {
            const auto& R=D->AuthoredPOIs[I]; const FVector P=D->AuthoredPOIPositions[I];
            TestTrue(TEXT("No gaps or overlap at seams"),FMath::IsNearlyEqual(P.X-R.Width*.5f,X,.1f)); X+=R.Width;
            Categories.Add(R.Category);
        }
        TestEqual(TEXT("Every initial category represented"),Categories.Num(),5);
        TestTrue(TEXT("No shaft overlap, including terrain blend"),C.EndX<Portal.X-Portal.Width*.5f-1400 || C.StartX>Portal.X+Portal.Width*.5f+1400);
        for (float Sample=C.StartX;Sample<=C.EndX;Sample+=100)
        {
            TestEqual(TEXT("Floor follows POI"),G->LayerFloor(1,Sample),C.FloorZ);
            TestEqual(TEXT("Ceiling follows POI"),G->LayerCeiling(1,Sample),C.FloorZ+400);
        }
    }
    const auto Positions=D->AuthoredPOIPositions;
    TArray<uint8> Bytes; TestTrue(TEXT("Save chain plan"),UGameplayStatics::SaveGameToMemory(D,Bytes));
    auto* Restored=Cast<UPWGv2WorldSave>(UGameplayStatics::LoadGameFromMemory(Bytes));
    if (TestNotNull(TEXT("Reload chain plan"),Restored)) TestEqual(TEXT("Chain count retained"),Restored->RoomChains.Num(),D->RoomChains.Num());
    D->RoomChains.Reset(); D->AuthoredPOIs.Reset(); D->AuthoredPOIPositions.Reset(); G->PlanRoomChains(Pool,1);
    TestTrue(TEXT("Deterministic same seed"),Positions==D->AuthoredPOIPositions);
    // Exercise the same planning path used by Main Menu, with the actual shipped tables/catalog.
    const FString Root=TEXT("/Game/Systems/Procedural_World_Generator/PWG_v2/DT_PWGv2_");
    auto* Catalog=LoadObject<UDataTable>(nullptr,TEXT("/Game/POIKit/Catalog/DT_UserUndergroundPOIs.DT_UserUndergroundPOIs"));
    if (TestNotNull(TEXT("Published Underground catalog exists"),Catalog))
    {
        TMap<FName,int32> Counts;
        for (FName Name:Catalog->GetRowNames())
        {
            const auto* Row=Catalog->FindRow<FPOIShaftRow>(Name,TEXT("Catalog verification"));
            if (!Row) continue;
            Counts.FindOrAdd(Row->Category)++;
            TestTrue(TEXT("Published map exists on disk"),FPackageName::DoesPackageExist(Row->SectionLevel.ToSoftObjectPath().GetLongPackageName()));
        }
        for (const TCHAR* C:{TEXT("Entrance_U"),TEXT("Pushable_U"),TEXT("Light_U"),TEXT("Empty_U"),TEXT("Parkour_U")})
            TestTrue(FString::Printf(TEXT("Ten published variants for %s"),C),Counts.FindRef(FName(C))>=10);
    }
    auto Table=[&](const TCHAR* Name) { return LoadObject<UDataTable>(nullptr,*(Root+Name+TEXT(".DT_PWGv2_")+Name)); };
    G->WorldTable=Table(TEXT("World")); G->RegionsTable=Table(TEXT("Regions"));
    G->BiomesTable=Table(TEXT("Biomes")); G->TreesTable=Table(TEXT("Trees")); G->UndergroundTable=Table(TEXT("Underground"));
    if (G->WorldTable)
    if (const auto* Settings=G->WorldTable->FindRow<FPWGv2WorldRow>(G->WorldSettingsRow,TEXT("Room chain test")))
    {
        G->Settings=*Settings;
        for (int32 Seed:{123,847,2026})
        {
            G->WorldData=NewObject<UPWGv2WorldSave>(G); G->WorldData->Seed=Seed;
            G->MakeProfile(); G->MakeExtendedPlan();
            AddInfo(FString::Printf(TEXT("Main Menu plan seed %d: %d chains, %d fragments"),Seed,G->WorldData->RoomChains.Num(),G->WorldData->AuthoredPOIs.Num()));
            TestTrue(TEXT("Default world produces playable corridor chain candidates"),G->WorldData->RoomChains.Num()>0);
        }
    }
    GEngine->DestroyWorldContext(World); World->DestroyWorld(false); return true;
}
#endif
