#include "POI/POIModule.h"
#include "POI/POIGenerator.h"
#include "POI/POITerrain.h"
#include "POI/POIInteractiveProp.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Algo/Reverse.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPOIPlannerTest,"Sirroh.POI.Planner",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPOIPlannerTest::RunTest(const FString& Parameters)
{
    auto Catalog=UPOIPlanner::ExampleCatalog(); FPOIPlanSettings S; FString Error;
    for (int32 Seed=0;Seed<100;++Seed)
    {
        S.Seed=Seed; TArray<FPOIPlacement> Plan,Repeat;
        if (!TestTrue(TEXT("Feasible plan succeeds"),UPOIPlanner::BuildPlan(Catalog,S,Plan,Error))) return false;
        TestEqual(TEXT("Requested module count"),Plan.Num(),S.ModuleCount);
        Algo::Reverse(Catalog);
        TestTrue(TEXT("Reordered catalog succeeds"),UPOIPlanner::BuildPlan(Catalog,S,Repeat,Error));
        int32 X=0,Floor=S.StartFloor; bool Top=false;
        for (int32 I=0;I<Plan.Num();++I)
        {
            const auto& P=Plan[I];
            TestEqual(TEXT("No gaps or overlap in X"),P.Cell.X,X);
            TestEqual(TEXT("Entry connects to prior exit"),P.Cell.Y+P.Module.EntryFloor,Floor);
            TestTrue(TEXT("Inside room height"),P.Cell.Y>=0 && P.Cell.Y+P.Module.HeightCells<=S.Floors);
            if (Repeat.IsValidIndex(I)) { TestEqual(TEXT("Stable selection"),P.Module.ModuleId,Repeat[I].Module.ModuleId); TestEqual(TEXT("Stable position"),P.Cell,Repeat[I].Cell); }
            X+=P.Module.WidthCells; Floor=P.Cell.Y+P.Module.ExitFloor; Top|=Floor==S.Floors-1;
        }
        TestEqual(TEXT("Exit floor reached"),Floor,S.EndFloor); TestTrue(TEXT("Top floor reached"),Top);
    }
    TArray<FPOIPlacement> Plan;
    S.ModuleCount=1; TestFalse(TEXT("Impossible height rejected"),UPOIPlanner::BuildPlan(Catalog,S,Plan,Error)); TestTrue(TEXT("No partial plan"),Plan.IsEmpty());
    S.Floors=1; TestTrue(TEXT("Single-floor corridor"),UPOIPlanner::BuildPlan(Catalog,S,Plan,Error));
    Catalog[0].Weight=-1; TestFalse(TEXT("Negative weight rejected"),UPOIPlanner::BuildPlan(Catalog,S,Plan,Error));
    Catalog=UPOIPlanner::ExampleCatalog(); const FPOIModuleRow Duplicate=Catalog[0]; Catalog.Add(Duplicate); TestFalse(TEXT("Duplicate IDs rejected"),UPOIPlanner::ValidateCatalog(Catalog,Error));
    Catalog=UPOIPlanner::ExampleCatalog(); S=FPOIPlanSettings(); S.SearchBudget=1;
    TestFalse(TEXT("Budget terminates search"),UPOIPlanner::BuildPlan(Catalog,S,Plan,Error));
    Catalog=UPOIPlanner::ExampleCatalog(); for (auto& M:Catalog) M.EntryType=TEXT("Incompatible");
    TestFalse(TEXT("Incompatible connections rejected"),UPOIPlanner::BuildPlan(Catalog,S,Plan,Error));
    Catalog=UPOIPlanner::ExampleCatalog(); S=FPOIPlanSettings();
    TestTrue(TEXT("Build layout for persistence"),UPOIPlanner::BuildPlan(Catalog,S,Plan,Error));
    auto* Save=NewObject<UPOILayoutSave>(); Save->Settings=S; Save->Placements=Plan;
    TArray<uint8> Bytes;
    TestTrue(TEXT("Serialize layout"),UGameplayStatics::SaveGameToMemory(Save,Bytes));
    auto* Loaded=Cast<UPOILayoutSave>(UGameplayStatics::LoadGameFromMemory(Bytes));
    if (TestNotNull(TEXT("Deserialize layout"),Loaded))
    {
        TestEqual(TEXT("Snapshot size survives"),Loaded->Placements.Num(),Plan.Num());
        for (int32 I=0;I<Plan.Num() && Loaded->Placements.IsValidIndex(I);++I)
        {
            TestEqual(TEXT("Snapshot ID survives"),Loaded->Placements[I].Module.ModuleId,Plan[I].Module.ModuleId);
            TestEqual(TEXT("Snapshot geometry survives"),Loaded->Placements[I].Cell,Plan[I].Cell);
            TestEqual(TEXT("Stable instance ID survives"),Loaded->Placements[I].InstanceId,Plan[I].InstanceId);
        }
    }
    FPOITerrainStamp Stamp; Stamp.Id=TEXT("Foundation"); Stamp.Height=200;
    TArray<FPOITerrainStamp> Stamps{Stamp};
    TestTrue(TEXT("Valid terrain stamp"),FPOITerrainStamp::ValidateAndSort(Stamps,Error));
    TestEqual(TEXT("Foundation height"),FPOITerrainStamp::Apply(Stamps,0,0,100),200.f);
    TestEqual(TEXT("Unchanged outside falloff"),FPOITerrainStamp::Apply(Stamps,800,0,100),100.f);
    TestEqual(TEXT("Smooth halfway blend"),FPOITerrainStamp::Apply(Stamps,600,0,100),150.f);
    Stamp.Id=TEXT("Pit"); Stamp.Priority=1; Stamp.Operation=EPOITerrainOperation::Offset; Stamp.Height=-50; Stamps.Insert(Stamp,0);
    TestTrue(TEXT("Sort stamps"),FPOITerrainStamp::ValidateAndSort(Stamps,Error));
    TestEqual(TEXT("Priority composition"),FPOITerrainStamp::Apply(Stamps,0,0,100),150.f);
    Stamps.Add(Stamp); TestFalse(TEXT("Duplicate stamp IDs rejected"),FPOITerrainStamp::ValidateAndSort(Stamps,Error));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPOIObjectTest,"Sirroh.POI.ObjectPersistence",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPOIObjectTest::RunTest(const FString& Parameters)
{
    const UWorld::InitializationValues Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    if (!TestNotNull(TEXT("Test world"),World)) return false;
    World->InitializeActorsForPlay(FURL());
    auto* Door=World->SpawnActor<APOIInteractiveProp>(); auto* Lever=World->SpawnActor<APOIInteractiveProp>(); auto* Box=World->SpawnActor<APOIInteractiveProp>();
    Door->SetActorLocation(FVector(100,0,0)); Door->PersistentState->LocalId=TEXT("Door");
    Lever->Kind=EPOIInteractionKind::Lever; Lever->Targets.Add(Door); Lever->PersistentState->LocalId=TEXT("Lever");
    Box->Kind=EPOIInteractionKind::Container; Box->InitialLoot={TEXT("Torch"),TEXT("Brick")}; Box->PersistentState->LocalId=TEXT("Chest");
    Door->DispatchBeginPlay(); Lever->DispatchBeginPlay(); Box->DispatchBeginPlay();
    TestTrue(TEXT("Actor BeginPlay dispatched"),Door->HasActorBegunPlay());
    TestTrue(TEXT("Lever can be activated nearby"),Lever->Interact(Box));
    TestTrue(TEXT("Lever opens linked door"),Door->PersistentState->bActive);
    TestTrue(TEXT("Open door clears collision"),!Door->GetActorEnableCollision());
    const auto DoorState=Door->PersistentState->Capture(FTransform::Identity);
    Door->SetActivated(false); Door->PersistentState->Restore(DoorState,FTransform::Identity);
    TestTrue(TEXT("Door state restored"),Door->PersistentState->bActive && !Door->GetActorEnableCollision());
    TestTrue(TEXT("Container opens nearby"),Box->Interact(Lever));
    TestTrue(TEXT("Take existing token"),Box->TakeLoot(Lever,TEXT("Torch")));
    TestFalse(TEXT("No duplicate token pickup"),Box->TakeLoot(Lever,TEXT("Torch")));
    auto* Save=NewObject<UPOILayoutSave>(); Save->ObjectStates.Add(TEXT("Module_000/Chest"),Box->PersistentState->Capture(FTransform::Identity));
    TArray<uint8> Bytes; UGameplayStatics::SaveGameToMemory(Save,Bytes);
    auto* Loaded=Cast<UPOILayoutSave>(UGameplayStatics::LoadGameFromMemory(Bytes));
    if (TestNotNull(TEXT("Object state save"),Loaded))
    {
        const auto* State=Loaded->ObjectStates.Find(TEXT("Module_000/Chest"));
        if (TestNotNull(TEXT("Object namespaced key"),State)) { Box->PersistentState->Loot.Reset(); Box->PersistentState->Restore(*State,FTransform::Identity); TestEqual(TEXT("Only remaining loot restored"),Box->PersistentState->Loot.Num(),1); }
    }
    Lever->SetActorLocation(FVector(10000,0,0)); TestFalse(TEXT("Range enforced"),Box->Interact(Lever));
    auto* Generator=World->SpawnActor<APOIGenerator>(); Generator->GeneratePreview();
    TestEqual(TEXT("Native preview generated"),Generator->Plan.Num(),8);
    World->DestroyWorld(false);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPOIStreamingTest,"Sirroh.POI.StreamingPersistence",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPOIStreamingTest::RunTest(const FString& Parameters)
{
    auto* Catalog=LoadObject<UDataTable>(nullptr,TEXT("/Game/POIPrototype/DT_POIExamples.DT_POIExamples"));
    if (!TestNotNull(TEXT("Demo catalog exists"),Catalog)) return false;
    const UWorld::InitializationValues Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL()); World->GetWorldSettings()->NotifyBeginPlay();
    auto* PC=World->SpawnActor<APlayerController>(); auto* Pawn=World->SpawnActor<ADefaultPawn>(); PC->Possess(Pawn);
    auto* Generator=World->SpawnActor<APOIGenerator>();
    Generator->bUseExampleCatalog=false; Generator->Catalog=Catalog; Generator->Settings.ModuleCount=1; Generator->Settings.Floors=1;
    Generator->GeneratePreview();
    World->FlushLevelStreaming(EFlushLevelStreamingType::Full); static_cast<AActor*>(Generator)->Tick(0.25f);
    auto FindDoor=[&]()->APOIInteractiveProp*
    {
        for (TActorIterator<APOIInteractiveProp> It(World);It;++It) if (It->Kind==EPOIInteractionKind::Door) return *It;
        return nullptr;
    };
    auto* Door=FindDoor();
    if (TestNotNull(TEXT("Source map streamed in"),Door))
    {
        TestTrue(TEXT("Streamed actor began play"),Door->HasActorBegunPlay()); Door->SetActivated(true);
        Pawn->SetActorLocation(FVector(100000,0,0)); static_cast<AActor*>(Generator)->Tick(0.25f); World->FlushLevelStreaming(EFlushLevelStreamingType::Full);
        TestNull(TEXT("Source map unloaded"),FindDoor());
        Pawn->SetActorLocation(FVector(0,0,120)); static_cast<AActor*>(Generator)->Tick(0.25f); World->FlushLevelStreaming(EFlushLevelStreamingType::Full); static_cast<AActor*>(Generator)->Tick(0.25f);
        Door=FindDoor();
        if (TestNotNull(TEXT("Source map reloaded"),Door)) TestTrue(TEXT("Open state survived streaming"),Door->PersistentState->bActive && !Door->GetActorEnableCollision());
    }
    World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
    return true;
}
#endif
