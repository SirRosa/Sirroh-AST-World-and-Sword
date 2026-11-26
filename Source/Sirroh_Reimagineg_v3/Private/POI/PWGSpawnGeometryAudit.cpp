#include "Procedural_World_Generator/PWGv2Generator.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Engine/LevelStreamingDynamic.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "POI/POIShaft.h"
#include "ProceduralMeshComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWGSpawnGeometryAudit,"Sirroh.PWG.SpawnGeometryAudit",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

bool FPWGSpawnGeometryAudit::RunTest(const FString&)
{
    // Audit both fresh seeds and copies of actual saves; never touch player save slots.
    TArray<FString> Existing;
    IFileManager::Get().FindFiles(Existing,*(FPaths::ProjectSavedDir()/TEXT("SaveGames/PWGv2_*.sav")),true,false);
    Existing.Sort();
    TArray<FString> Slots;
    for (const auto& File:Existing)
    {
        const FString Slot=FPaths::GetBaseFilename(File);
        if (!Slot.StartsWith(TEXT("PWGv2_SpawnAudit_")) && !Slot.Contains(TEXT("Probe"))) Slots.Add(Slot);
        if (Slots.Num()>=3) break;
    }
    Slots.Add(TEXT("")); Slots.Add(TEXT(""));
    int32 Case=0;
    for (const FString& Original:Slots)
    {
        const FString Name=TEXT("SpawnAudit_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
        const FString Slot=TEXT("PWGv2_")+Name;
        if (!Original.IsEmpty())
        {
            auto* Save=UGameplayStatics::LoadGameFromSlot(Original,0);
            if (!Save || !UGameplayStatics::SaveGameToSlot(Save,Slot,0)) { AddWarning(TEXT("Cannot copy fixture ")+Original); continue; }
        }
        const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
        UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
        if (!World) { UGameplayStatics::DeleteGameInSlot(Slot,0); AddError(TEXT("No test world")); continue; }
        GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
        World->SetGameInstance(NewObject<UGameInstance>(GEngine));
        auto* G=World->SpawnActor<APWGv2Generator>();
        G->WorldName=Name; G->SeedOverride=Case++%2==0?123:847;
        if (TestTrue(TEXT("Probe world generated or loaded"),G->GenerateWorld(NAME_None)))
        {
            G->EnsureLocationLoaded(FVector(-1,0,0));
            for (TActorIterator<APOIShaft> It(World);It;++It)
                while (It->HasPendingBuild()) It->ProcessPendingBuild();
            World->FlushLevelStreaming(EFlushLevelStreamingType::Full);
            const auto* D=G->GetDebugWorld();
            const FVector Spawn=G->GetWorldCenterSurfaceLocation();
            AddInfo(FString::Printf(TEXT("SPAWN_AUDIT case=%s seed=%d surface=%.1f"),Original.IsEmpty()?TEXT("NEW"):*Original,D->Seed,Spawn.Z));
            for (const auto& P:D->Portals) if (FMath::Abs(P.X)<6000)
                AddInfo(FString::Printf(TEXT("SPAWN_PORTAL x=%.1f width=%.1f layers=%d->%d top=%.1f bottom=%.1f kit=%d"),P.X,P.Width,P.UpperLayer,P.LowerLayer,P.TopZ,P.BottomZ,P.bKitShaft));
            for (int32 I=0;I<D->AuthoredPOIs.Num()&&I<D->AuthoredPOIPositions.Num();++I)
            {
                const auto& R=D->AuthoredPOIs[I]; const FVector P=D->AuthoredPOIPositions[I];
                if (FMath::Abs(P.X)<6000)
                    AddInfo(FString::Printf(TEXT("SPAWN_POI x=%.1f z=%.1f width=%.1f h=%.1f layer=%d level=%s"),P.X,P.Z,R.Width,R.SectionHeight,R.UpperLayer,*R.SectionLevel.ToString()));
            }
            TArray<UProceduralMeshComponent*> Meshes;
            G->GetComponents<UProceduralMeshComponent>(Meshes);
            int32 UnderCount=0;
            for (auto* M:Meshes)
            {
                const FProcMeshSection* S2=M->GetProcMeshSection(2);
                // Surface has water at section 1 and terrain starting at section 3.
                if (!S2 || S2->ProcVertexBuffer.IsEmpty()) continue;
                ++UnderCount;
                for (int32 S=0;S<M->GetNumSections();++S)
                {
                    const FProcMeshSection* Section=M->GetProcMeshSection(S);
                    if (!Section) continue;
                    int32 N=0,Above=0; double MaxExcess=-1e9; FVector Worst=FVector::ZeroVector;
                    for (const auto& V:Section->ProcVertexBuffer)
                    {
                        const FVector P=M->GetComponentTransform().TransformPosition(V.Position);
                        if (FMath::Abs(P.X-Spawn.X)>4000) continue;
                        // Compare with the playable central lane, not the deliberately lowered visual foreground.
                        const float Surface=G->GetSurfaceHeightAt(P.X,Spawn.Y);
                        if (Surface<=-BIG_NUMBER/2) continue;
                        const double Excess=P.Z-Surface; ++N;
                        if (Excess>50) ++Above;
                        if (Excess>MaxExcess) { MaxExcess=Excess; Worst=P; }
                    }
                    if (N>0 && (S==1 || S==17))
                        TestTrue(FString::Printf(TEXT("First-layer floor/roof and near shell do not protrude (section %d)"),S),Above==0);
                    if (N>0 && Above>0)
                        AddInfo(FString::Printf(TEXT("SPAWN_OVERGROUND section=%d above=%d/%d max=%.1f x=%.1f y=%.1f z=%.1f"),S,Above,N,MaxExcess,Worst.X,Worst.Y,Worst.Z));
                }
            }
            AddInfo(FString::Printf(TEXT("SPAWN_MESHES underground=%d total=%d"),UnderCount,Meshes.Num()));
            for (TActorIterator<APOIShaft> It(World);It;++It)
            {
                auto* A=*It;
                if (FMath::Abs(A->GetActorLocation().X-Spawn.X)>6000) continue;
                AddInfo(FString::Printf(TEXT("SPAWN_SHAFT %s x=%.1f z=%.1f height=%.1f layer=%d levels=%d"),*A->GetName(),A->GetActorLocation().X,A->GetActorLocation().Z,A->Height,A->Recipe.UpperLayer,A->GeneratedLevels.Num()));
                for (ULevelStreamingDynamic* Stream:A->GeneratedLevels) if (Stream&&Stream->GetLoadedLevel())
                {
                    for (AActor* Actor:Stream->GetLoadedLevel()->Actors)
                    {
                        if (!IsValid(Actor)||!Actor->GetRootComponent()) continue;
                        FVector Origin,Extent; Actor->GetActorBounds(false,Origin,Extent);
                        if (Extent.IsNearlyZero()||FMath::Abs(Origin.X-Spawn.X)>4000) continue;
                        const float Surface=G->GetSurfaceHeightAt(Origin.X,Spawn.Y);
                        if (Surface>-BIG_NUMBER/2 && Origin.Z+Extent.Z>Surface+50)
                            AddInfo(FString::Printf(TEXT("SPAWN_STREAM_ACTOR class=%s level=%s x=%.1f zmin=%.1f zmax=%.1f surface=%.1f"),*Actor->GetClass()->GetName(),*Stream->GetWorldAssetPackageName(),Origin.X,Origin.Z-Extent.Z,Origin.Z+Extent.Z,Surface));
                    }
                }
            }
        }
        GEngine->DestroyWorldContext(World); World->DestroyWorld(false);
        UGameplayStatics::DeleteGameInSlot(Slot,0);
    }
    return true;
}
#endif
