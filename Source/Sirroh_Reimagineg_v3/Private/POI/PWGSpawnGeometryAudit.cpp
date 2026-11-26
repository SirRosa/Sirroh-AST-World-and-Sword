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
#include "Materials/Material.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Procedural_World_Generator/PWGProceduralTree.h"

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
            // Check actual foreground geometry, not merely component visibility.
            int32 MaskQuads=0,OpenSamples=0,ShaftSamples=0,SolidSamples=0;
            int32 ShadeMin=255,ShadeMax=0;
            for (const auto& Pair:G->UndergroundChunks)
            {
                auto* Mesh=Pair.Value.Get(); if (!Mesh) continue;
                TArray<float> Edges;
                for (int32 L=1;L<=4;++L)
                {
                    const int32 S=G->SolidCutawaySectionBase+L-1;
                    const auto* Part=Mesh->GetProcMeshSection(S);
                    if (!Part || Part->ProcVertexBuffer.IsEmpty()) continue;
                    TestFalse(TEXT("Solid cutaway has no section collision"),Part->bEnableCollision);
                    auto* MaskMaterial=Mesh->GetMaterial(S);
                    TestTrue(TEXT("Solid cutaway uses dedicated gradient material"),MaskMaterial && MaskMaterial==G->WallMaterial(L,false,false,true) && MaskMaterial!=G->WallMaterial(L));
                    TestTrue(TEXT("Solid cutaway parent shader is not the ordinary tiled shader or fallback"),MaskMaterial && MaskMaterial->GetMaterial() && MaskMaterial->GetMaterial()->GetName()==TEXT("M_PWGv2_SolidCutaway"));
                    for (const auto& V:Part->ProcVertexBuffer)
                    {
                        ShadeMin=FMath::Min(ShadeMin,int32(V.Color.R)); ShadeMax=FMath::Max(ShadeMax,int32(V.Color.R));
                        TestTrue(TEXT("Cutaway shading is neutral"),V.Color.R==V.Color.G && V.Color.G==V.Color.B);
                    }
                    for (int32 V=0;V+3<Part->ProcVertexBuffer.Num();V+=4)
                    {
                        const auto& A=Part->ProcVertexBuffer[V].Position;
                        const auto& B=Part->ProcVertexBuffer[V+1].Position;
                        TestTrue(TEXT("Mask is outside the playable lanes"),A.Y < -G->Settings.LaneSpacing-100.f);
                        Edges.AddUnique(A.X); Edges.AddUnique(B.X); ++MaskQuads;
                    }
                }
                Edges.Sort();
                auto Covered=[&](float X,float Z)
                {
                    for (int32 L=1;L<=4;++L)
                    {
                        const auto* Part=Mesh->GetProcMeshSection(G->SolidCutawaySectionBase+L-1);
                        if (!Part) continue;
                        for (int32 V=0;V+3<Part->ProcVertexBuffer.Num();V+=4)
                        {
                            const auto& A=Part->ProcVertexBuffer[V].Position; const auto& B=Part->ProcVertexBuffer[V+1].Position;
                            const auto& C=Part->ProcVertexBuffer[V+2].Position; const auto& E=Part->ProcVertexBuffer[V+3].Position;
                            if (X<=A.X || X>=B.X) continue;
                            const float T=(X-A.X)/(B.X-A.X);
                            // Shared material seams are covered by the adjoining quads as well.
                            if (Z>=FMath::Lerp(A.Z,B.Z,T)-.01 && Z<=FMath::Lerp(E.Z,C.Z,T)+.01) return true;
                        }
                    }
                    return false;
                };
                for (int32 I=0;I+1<Edges.Num();++I)
                {
                    const float A=Edges[I],B=Edges[I+1],X=(A+B)*.5f;
                    auto Floor=[&](int32 L){return (G->LayerFloor(L,A)+G->LayerFloor(L,B))*.5f;};
                    auto Roof=[&](int32 L){return (G->LayerCeiling(L,A)+G->LayerCeiling(L,B))*.5f;};
                    TestTrue(TEXT("Sky is masked above underground"),Covered(X,Floor(0)+5000.f));
                    TestTrue(TEXT("Void below deepest layer is masked"),Covered(X,Floor(4)-5000.f));
                    SolidSamples+=2;
                    for (int32 L=1;L<=4;++L)
                    {
                        if (L==1 && G->RegionAt(X).Definition.EntranceStyle==EPWGv2EntranceStyle::None) continue;
                        TestFalse(TEXT("Room interior remains open"),Covered(X,(Floor(L)+Roof(L))*.5f)); ++OpenSamples;
                        if (L<=1) continue;
                        const float Z=(Roof(L)+Floor(L-1))*.5f;
                        bool bPassage=false;
                        for (const auto& P:D->Portals)
                            if (P.UpperLayer<L && P.LowerLayer>=L && FMath::Abs(X-P.X)<P.Width*.5f) bPassage=true;
                        // Test ordinary inter-floor gaps, away from missing first-layer regions.
                        if (!bPassage && Floor(L-1)>Roof(L)+20.f && !(L==2 && G->RegionAt(X).Definition.EntranceStyle==EPWGv2EntranceStyle::None))
                        { TestTrue(TEXT("Solid gap between rooms is masked"),Covered(X,Z)); ++SolidSamples; }
                    }
                    for (const auto& P:D->Portals)
                        if (FMath::Abs(X-P.X)<P.Width*.5f && P.UpperLayer>=0 && P.LowerLayer<=4 && P.UpperLayer<P.LowerLayer)
                        {
                            const float Low=Floor(P.LowerLayer),High=Floor(P.UpperLayer);
                            if (High>Low+20.f) { TestFalse(TEXT("Shaft interior remains open"),Covered(X,(Low+High)*.5f)); ++ShaftSamples; }
                        }
                }
            }
            TestTrue(TEXT("Audit contains actual cutaway quads and rooms"),MaskQuads>0 && OpenSamples>0 && SolidSamples>0);
            // Accept either linear or sRGB vertex conversion; verify genuine gradient and darker baseline.
            const FLinearColor Edge(.85f,.85f,.85f);
            TestTrue(TEXT("Mask has distance shading"),ShadeMax-ShadeMin>30 && ShadeMin>0);
            TestTrue(TEXT("Mask edge multiplier is approximately 85 percent of normal wall"),FMath::Abs(ShadeMax-int32(Edge.ToFColor(false).R))<=2 || FMath::Abs(ShadeMax-int32(Edge.ToFColor(true).R))<=2);
            AddInfo(FString::Printf(TEXT("SOLID_SHADE vertex_min=%d vertex_max=%d"),ShadeMin,ShadeMax));
            AddInfo(FString::Printf(TEXT("SOLID_CUTAWAY quads=%d room_samples=%d shaft_samples=%d solid_samples=%d"),MaskQuads,OpenSamples,ShaftSamples,SolidSamples));
            // Exercise both domain changes and the no-frustum (debug/perspective) path.
            auto CheckVisibility=[&](bool bUnderground)
            {
                G->UpdateGameplayView(FTransform::Identity,0.f,16.f/9.f);
                int32 Surface=0, SurfaceSections=0, Cave=0, Earth=0, Props=0;
                for (const auto& Pair:G->Chunks) if (auto* Mesh=Pair.Value.Get())
                {
                    ++Surface;
                    TestTrue(TEXT("Surface component remains visible and registered"),Mesh->IsVisible() && Mesh->IsRegistered());
                    for (int32 S=0;S<Mesh->GetNumSections();++S)
                        if (const auto* Part=Mesh->GetProcMeshSection(S); Part && !Part->ProcVertexBuffer.IsEmpty())
                        {
                            if (S>=3) ++SurfaceSections;
                            TestTrue(TEXT("Actual terrain/water draw section visibility"),Mesh->IsMeshSectionVisible(S)!=bUnderground);
                        }
                }
                for (const auto& Pair:G->DecorationChunks)
                {
                    const auto& Batch=Pair.Value;
                    for (UHierarchicalInstancedStaticMeshComponent* C:Batch.Components) if (IsValid(C))
                    {
                        if (C->ComponentHasTag(TEXT("PWGSurfaceVisual"))) { ++Props; TestTrue(TEXT("Surface vegetation/rocks layer visibility"),C->IsVisible()!=bUnderground); }
                        if (C->ComponentHasTag(TEXT("PWGUndergroundVisual"))) TestTrue(TEXT("Cave rocks layer visibility"),C->IsVisible()==bUnderground);
                    }
                    for (const auto& Tree:Batch.Trees) if (Tree.IsValid()) TestTrue(TEXT("Tree layer visibility"),Tree->GetRootComponent()->IsVisible()!=bUnderground);
                    for (AActor* Actor:Batch.Actors) if (IsValid(Actor))
                    {
                        if (auto* POI=Cast<APOIShaft>(Actor))
                        {
                            if (POI->ActorHasTag(TEXT("PWGSurfacePOI"))) TestTrue(TEXT("Surface POI render mask"),POI->IsLayerRenderHidden()==bUnderground);
                            if (POI->ActorHasTag(TEXT("PWGUndergroundPOI"))) TestTrue(TEXT("Underground POI render mask"),POI->IsLayerRenderHidden()!=bUnderground);
                        }
                    }
                    if (auto* Mesh=G->UndergroundChunks.FindRef(Pair.Key).Get())
                        for (int32 S=0;S<Batch.UndergroundSectionBounds.Num();++S) if (Batch.UndergroundSectionBounds[S].IsValid)
                        {
                            ++Cave;
                            if (G->IsEarthSection(S))
                            {
                                ++Earth;
                                TestTrue(TEXT("Solid earth remains visible in both domains"),Mesh->IsMeshSectionVisible(S));
                            }
                            else if (G->IsSolidCutawaySection(S)) TestTrue(TEXT("Solid foreground only underground"),Mesh->IsMeshSectionVisible(S)==bUnderground);
                            else TestTrue(TEXT("Cave section layer visibility"),Mesh->IsMeshSectionVisible(S)==(bUnderground || G->IsSurfaceVisibleUndergroundSection(S)));
                        }
                }
                TestTrue(TEXT("Visibility audit has actual terrain draw sections"),Surface>0 && SurfaceSections>0);
                TestTrue(TEXT("Visibility audit has solid earth backing"),Earth>0);
                TestTrue(TEXT("Visibility audit has cave geometry"),Cave>0);
                AddInfo(FString::Printf(TEXT("LAYER_VISIBILITY underground=%d surface=%d terrain_sections=%d cave_sections=%d solid_earth=%d props=%d"),bUnderground,Surface,SurfaceSections,Cave,Earth,Props));
            };
            TMap<UProceduralMeshComponent*,ECollisionEnabled::Type> CollisionBefore;
            for (auto* Mesh:Meshes) CollisionBefore.Add(Mesh,Mesh->GetCollisionEnabled());
            G->UpdateCutaway(Spawn+FVector(0,0,100)); CheckVisibility(false);
            G->UpdateCutaway(Spawn-FVector(0,0,5000)); CheckVisibility(true);
            for (const auto& Pair:CollisionBefore) TestTrue(TEXT("Visibility never changes collision"),Pair.Key->GetCollisionEnabled()==Pair.Value);
            // A newly built chunk inherits the active layer, even before the next camera update.
            const float Size=G->Settings.ChunkLengthMetres*100;
            const int32 Fresh=G->Settings.LoadRadius+2;
            if (Fresh*Size<D->MaxX && !G->Chunks.Contains(Fresh))
            {
                G->BuildChunk(Fresh);
                if (auto* Surface=G->Chunks.FindRef(Fresh).Get())
                {
                    for (int32 S=0;S<Surface->GetNumSections();++S)
                        if (const auto* Part=Surface->GetProcMeshSection(S); Part && !Part->ProcVertexBuffer.IsEmpty())
                            TestFalse(TEXT("Newly streamed terrain draw section hidden underground"),Surface->IsMeshSectionVisible(S));
                }
                else AddError(TEXT("Fresh chunk did not build"));
                CheckVisibility(true);
            }
            G->UpdateCutaway(Spawn+FVector(0,0,100)); CheckVisibility(false);
        }
        GEngine->DestroyWorldContext(World); World->DestroyWorld(false);
        UGameplayStatics::DeleteGameInSlot(Slot,0);
    }
    return true;
}
#endif
