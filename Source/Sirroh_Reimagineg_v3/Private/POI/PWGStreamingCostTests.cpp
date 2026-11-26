#include "Procedural_World_Generator/PWGv2Generator.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "POI/POIShaft.h"
#include "EngineUtils.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWGStreamingCostTest,"Sirroh.PWG.StreamingCost",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPWGStreamingCostTest::RunTest(const FString& Parameters)
{
    if(!UGameplayStatics::DoesSaveGameExist(TEXT("PWGv2_CodexStreamingProbe"),0))
    { AddWarning(TEXT("Optional streaming fixture absent; copy a world save to PWGv2_CodexStreamingProbe.sav to run this benchmark."));return true; }
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->SetGameInstance(NewObject<UGameInstance>(GEngine));
    auto* G=World->SpawnActor<APWGv2Generator>();G->WorldName=TEXT("CodexStreamingProbe");
    if(TestTrue(TEXT("Copied world loads"),G->GenerateWorld(NAME_None)))
    {
        G->BeginDebugSession();
        const float Size=G->Settings.ChunkLengthMetres*100;
        for(int32 Index:{1,2,3,4,5,4,3,2})
        {
            const double Start=FPlatformTime::Seconds();
            G->StreamAt(Index*Size+Size*.5f,false);
            AddInfo(FString::Printf(TEXT("STREAM_SAMPLE center=%d ms=%.2f chunks=%d"),Index,(FPlatformTime::Seconds()-Start)*1000,G->Chunks.Num()));
            // Complete all queued work: the benchmark must account for deferred construction too.
            for(TActorIterator<APOIShaft> It(World);It;++It)
            {
                int32 Frames=0; double Worst=0;
                while(It->HasPendingBuild() && Frames<600)
                {
                    const double FrameStart=FPlatformTime::Seconds();
                    It->ProcessPendingBuild();
                    Worst=FMath::Max(Worst,(FPlatformTime::Seconds()-FrameStart)*1000); ++Frames;
                }
                TestFalse(TEXT("Queued shaft finishes"),It->HasPendingBuild());
                if(Frames>0)
                {
                    TestTrue(TEXT("Deferred shaft produced blocks"),It->GeneratedBlocks.Num()>0);
                    AddInfo(FString::Printf(TEXT("SHAFT_DEFERRED frames=%d worst_ms=%.2f blocks=%d"),Frames,Worst,It->GeneratedBlocks.Num()));
                }
            }
        }
    }
    GEngine->DestroyWorldContext(World);World->DestroyWorld(false);return true;
}
#endif
