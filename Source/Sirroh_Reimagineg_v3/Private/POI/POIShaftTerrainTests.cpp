#include "Procedural_World_Generator/PWGv2Generator.h"
#include "POI/POIKitBlock.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWGShaftTerrainTest,"Sirroh.POI.ShaftTerrain",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPWGShaftTerrainTest::RunTest(const FString& Parameters)
{
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    auto* G=World->SpawnActor<APWGv2Generator>();
    G->WorldData=NewObject<UPWGv2WorldSave>(G);
    auto* D=G->WorldData.Get(); D->Seed=123; D->MinX=-10000; D->MaxX=10000;
    G->Settings.SampleSpacing=100; G->Settings.bEnableUnderground=true; G->Settings.LaneSpacing=150;
    D->Heights.Init(2000,201);
    FPWGv2RegionSpan Region; Region.StartX=-10000; Region.EndX=10000; D->Regions.Add(Region);
    for (int32 I=1;I<=4;++I) { FPWGv2UndergroundRow L; L.Layer=I; L.FloorBelowSeaMetres=I*20; L.ClearHeightMetres=5; D->Layers.Add(L); }
    for (int32 I=0;I<2;++I)
    {
        FPWGv2Portal P; P.X=I==0?-2000:0; P.Width=600; P.UpperLayer=I; P.LowerLayer=I+1;
        P.bKitShaft=true; P.ShaftSeed=77+I;
        for (auto SectionRole:{EPOIShaftRole::Entrance,EPOIShaftRole::Middle,EPOIShaftRole::Exit})
        {
            FPOIShaftRow Row; Row.Role=SectionRole; Row.bUseCustomSection=true; Row.SectionHeight=300;
            Row.SectionLevel=TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/POIKit/L_POI_Template.L_POI_Template")));
            P.ShaftModules.Add(Row);
        }
        D->Portals.Add(P);
    }
    G->PlanShaftTerrain();
    for (const auto& P:D->Portals)
    {
        TestTrue(TEXT("Randomized 4..6 complete modules"),P.ShaftSectionPlan.Num()>=4 && P.ShaftSectionPlan.Num()<=6);
        TestTrue(TEXT("Actual module height defines depth within 0.01 cm"),FMath::IsNearlyEqual(P.TopZ-P.BottomZ,P.ShaftSectionPlan.Num()*300.f,.01f));
        TestTrue(TEXT("Lower floor meets shaft bottom"),FMath::IsNearlyEqual(G->LayerFloor(P.LowerLayer,P.X),P.BottomZ));
        TestTrue(TEXT("Upper floor meets shaft top"),FMath::IsNearlyEqual(G->LayerFloor(P.UpperLayer,P.X),P.TopZ));
        for (float Edge:{P.X-299,P.X+299}) TestTrue(TEXT("Exit width rests on same floor"),FMath::IsNearlyEqual(G->LayerFloor(P.LowerLayer,Edge),P.BottomZ));
    }
    const auto& P=D->Portals[0];
    TestTrue(TEXT("Front lane mouth has no invisible floor"),G->IsOpening(0,P.X,-150));
    TestTrue(TEXT("Middle lane mouth is open"),G->IsOpening(0,P.X,0));
    TestFalse(TEXT("Back lane remains solid"),G->IsOpening(0,P.X,150));
    TestFalse(TEXT("Bottom exit is not another hole"),G->IsOpening(1,P.X,0));
    FVector Destination;
    TestTrue(TEXT("Front lane routed into shaft"),G->GetShaftLaneDestination(FVector(P.X,-150,P.TopZ+90),35,90,Destination));
    TestEqual(TEXT("Routing changes only depth"),Destination,FVector(P.X,0,P.TopZ+90));
    TestFalse(TEXT("No routing outside mouth"),G->GetShaftLaneDestination(FVector(P.X+400,-150,P.TopZ+90),35,90,Destination));
    TestFalse(TEXT("No routing high above mouth"),G->GetShaftLaneDestination(FVector(P.X,-150,P.TopZ+500),35,90,Destination));
    TArray<uint8> Bytes;
    TestTrue(TEXT("World snapshot serializes"),UGameplayStatics::SaveGameToMemory(D,Bytes));
    auto* Restored=Cast<UPWGv2WorldSave>(UGameplayStatics::LoadGameFromMemory(Bytes));
    if (TestNotNull(TEXT("Snapshot restored"),Restored))
    { TestEqual(TEXT("Planned height survives reload"),Restored->Portals[0].BottomZ,P.BottomZ); TestEqual(TEXT("Module plan survives reload"),Restored->Portals[0].ShaftSectionPlan.Num(),P.ShaftSectionPlan.Num()); }
    auto* Block=World->SpawnActor<APOIKitBlock>(); Block->Settings.Kind=EPOIKitKind::AST; Block->RefreshBlock();
    AActor* Grip=Block->GetASTActor(); const FVector Extent=Block->GetPhysicalBody()->GetUnscaledBoxExtent();
    Block->RefreshAppearance();
    TestEqual(TEXT("Art refresh retains original AST actor"),Block->GetASTActor(),Grip);
    TestEqual(TEXT("Art refresh retains physical collision"),Block->GetPhysicalBody()->GetUnscaledBoxExtent(),Extent);
    GEngine->DestroyWorldContext(World); World->DestroyWorld(false); return true;
}
#endif
