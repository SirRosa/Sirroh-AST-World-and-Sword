#include "POI/POIShaft.h"
#include "POI/POIArtPool.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPOIPublishedTest,"Sirroh.POI.PublishedLevels",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPOIPublishedTest::RunTest(const FString& Parameters)
{
    auto* Theme=UPOIArtTheme::ForLayer(0);
    if (!TestNotNull(TEXT("Default theme exists"),Theme)) return false;
    TestEqual(TEXT("Ten background images"),Theme->Backgrounds?Theme->Backgrounds->Images.Num():0,10);
    TestEqual(TEXT("Ten platform pairs"),Theme->Platforms?Theme->Platforms->Images.Num():0,10);
    FRandomStream A(71),B(71); TSet<FSoftObjectPath> Choices;
    for (int32 I=0;I<64;++I)
    {
        FPOIKitSettings S,T; S.Kind=T.Kind=EPOIKitKind::AST;
        Theme->Apply(S,A);Theme->Apply(T,B);
        TestEqual(TEXT("Seed reproduces platform appearance"),S.Image.ToSoftObjectPath(),T.Image.ToSoftObjectPath());
        TestFalse(TEXT("Paired top is present"),S.TopImage.IsNull()); Choices.Add(S.Image.ToSoftObjectPath());
    }
    TestTrue(TEXT("Pool generates variety"),Choices.Num()>1);
    FPOIShaftRow Row; Row.bUseCustomSection=true;
    Row.SectionLevel=TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/POIKit/L_POI_Template.L_POI_Template")));
    FString Error; TestTrue(TEXT("Map section accepts arbitrary actors without legacy piece restrictions"),Row.Validate(Error));
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    auto* Shaft=World->SpawnActor<APOIShaft>();
    Shaft->Recipe=Row; Shaft->Height=300; Shaft->bBuildBoundaryWalls=false;
    Shaft->BuildShaft();
    TestEqual(TEXT("Map creates one streaming instance"),Shaft->GeneratedLevels.Num(),1);
    World->FlushLevelStreaming(EFlushLevelStreamingType::Full);
    if (!Shaft->GeneratedLevels.IsEmpty())
    {
        auto* Stream=Shaft->GeneratedLevels[0].Get();
        TestNotNull(TEXT("Streaming loads the authored level"),Stream->GetLoadedLevel());
        TestTrue(TEXT("Instance becomes visible"),Stream->IsLevelVisible());
        if (auto* Level=Stream->GetLoadedLevel())
        {
            int32 Count=0;
            for (AActor* Actor:Level->Actors) if (auto* Block=Cast<APOIKitBlock>(Actor))
            {
                ++Count;
                TestFalse(TEXT("Streamed blocks have an image"),Block->Settings.Image.IsNull());
            }
            TestTrue(TEXT("Authored blocks retained"),Count>10);
        }
    }
    Shaft->ClearGenerated(); World->FlushLevelStreaming(EFlushLevelStreamingType::Full);
    TestEqual(TEXT("Instance handles cleared on unload"),Shaft->GeneratedLevels.Num(),0);
    GEngine->DestroyWorldContext(World); World->DestroyWorld(false);
    return true;
}
#endif
