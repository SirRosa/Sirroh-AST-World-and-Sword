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
#include "POI/POIKitBlock.h"
#include "Components/BoxComponent.h"
#include "POIImageLighting.h"
#include "Engine/Texture2D.h"
#include "Components/StaticMeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "Materials/Material.h"
#include "Algo/BinarySearch.h"
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
            // Test the real parent shaders on both generated procedural walls and kit cards.
            auto IsLit=[](UMaterialInterface* M,const TCHAR* Name)
            { return M && M->GetMaterial() && M->GetMaterial()->GetName()==Name && M->GetMaterial()->GetShadingModels().HasShadingModel(MSM_DefaultLit); };
            int32 LitCaves=0,LitShafts=0,LitProjected=0,LitCards=0;
            for (const auto& Pair:G->UndergroundChunks) if (auto* M=Pair.Value.Get())
                for (int32 S=0;S<M->GetNumSections();++S)
                {
                    const auto* Part=M->GetProcMeshSection(S); if (!Part || Part->ProcVertexBuffer.IsEmpty()) continue;
                    if (S>=5 && S<=8) { ++LitCaves; TestTrue(TEXT("Cave interior is Default Lit"),IsLit(M->GetMaterial(S),TEXT("M_PWGv2_BackgroundLit"))); }
                    else if (S==9 || (S>=13 && S<=16)) { ++LitShafts; TestTrue(TEXT("Shaft lining is Default Lit"),IsLit(M->GetMaterial(S),TEXT("M_PWGv2_BackgroundLit"))); }
                    else if (S>=G->SolidCutawaySectionBase+4) { ++LitProjected; TestTrue(TEXT("Projected POI background is Default Lit"),IsLit(M->GetMaterial(S),TEXT("M_KitImageFit_LitBackground"))); }
                }
            // Direct card path is also exercised when this seed has only projected room backgrounds.
            auto* Card=World->SpawnActor<APOIKitBlock>();
            if (Card) { Card->Settings.Kind=EPOIKitKind::Background; Card->RefreshAppearance(); }
            TestTrue(TEXT("Background probe spawned"),Card!=nullptr);
            for (TActorIterator<APOIKitBlock> It(World);It;++It)
            {
                if (It->Settings.Kind!=EPOIKitKind::Background) continue;
                TArray<UStaticMeshComponent*> Visuals; It->GetComponents<UStaticMeshComponent>(Visuals);
                for (auto* V:Visuals) if (V && V->GetFName()==FName(TEXT("ImageOrBlock")))
                { ++LitCards; TestTrue(TEXT("Kit background card is Default Lit"),IsLit(V->GetMaterial(0),TEXT("M_KitImageFit_LitBackground"))); }
            }
            if (Card)
            {
                // Probe actual PNG face assignment, including optional caps and native mesh overrides.
                auto* Image=LoadObject<UTexture2D>(nullptr,TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture"));
                auto* OriginalPNGMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/POIKit/M_KitImageFit.M_KitImageFit"));
                TestTrue(TEXT("PNG probe assets exist"),Image && OriginalPNGMaterial);
                Card->Settings.Kind=EPOIKitKind::AST;
                Card->Settings.Image=Image; Card->Settings.TopImage=Image; Card->Settings.Material=OriginalPNGMaterial;
                Card->RefreshAppearance();
                TArray<UStaticMeshComponent*> Faces; Card->GetComponents<UStaticMeshComponent>(Faces);
                int32 LitFaces=0;
                for (auto* Face:Faces) if (Face && (Face->GetFName()==FName(TEXT("ImageOrBlock")) || Face->GetFName()==FName(TEXT("TopImage")) || Face->GetFName()==FName(TEXT("BottomImage"))))
                {
                    ++LitFaces;
                    TestTrue(TEXT("Platform PNG face is Default Lit"),IsLit(Face->GetMaterial(0),TEXT("M_KitImageFit_LitBackground")));
                }
                TestEqual(TEXT("All three platform image faces tested"),LitFaces,3);
                if (Image && OriginalPNGMaterial && Faces.Num()>0)
                {
                    auto* Face=Faces[0];
                    auto* MID=UMaterialInstanceDynamic::Create(OriginalPNGMaterial,Face);
                    const FLinearColor ExpectedTint(.2f,.4f,.6f,1.f),ExpectedRect(.1f,.2f,.7f,.6f);
                    MID->SetTextureParameterValue(TEXT("ImageTexture"),Image);
                    MID->SetVectorParameterValue(TEXT("Tint"),ExpectedTint);
                    MID->SetVectorParameterValue(TEXT("UVRect"),ExpectedRect);
                    Face->SetMaterial(0,MID);
                    TestEqual(TEXT("Native PNG slot converted once"),POIImageLighting::ApplyToActor(Card),1);
                    auto* Converted=Face->GetMaterial(0);
                    UTexture* ActualImage=nullptr; FLinearColor ActualTint,ActualRect;
                    TestTrue(TEXT("Native PNG receives scene lighting"),IsLit(Converted,TEXT("M_KitImageFit_LitBackground")));
                    TestTrue(TEXT("Native PNG texture preserved"),Converted && Converted->GetTextureParameterValue(FMaterialParameterInfo(FName(TEXT("ImageTexture"))),ActualImage) && ActualImage==Image);
                    TestTrue(TEXT("Native PNG tint preserved"),Converted && Converted->GetVectorParameterValue(FMaterialParameterInfo(FName(TEXT("Tint"))),ActualTint) && ActualTint.Equals(ExpectedTint));
                    TestTrue(TEXT("Native PNG UV rect preserved"),Converted && Converted->GetVectorParameterValue(FMaterialParameterInfo(FName(TEXT("UVRect"))),ActualRect) && ActualRect.Equals(ExpectedRect));
                    TestEqual(TEXT("Already lit native PNG not reallocated"),POIImageLighting::ApplyToActor(Card),0);
                }
                Card->Destroy();
            }
            TestTrue(TEXT("Audit contains lit cave and kit backgrounds"),LitCaves>0 && LitCards>0);
            if(G->UsesUnifiedBackground() && G->BackgroundMode!=EPOIBackgroundMode::Enclosed)
                TestEqual(TEXT("Natural default has no projected rectangular POI art"),LitProjected,0);
            FPOIShaftRow ModeProbe; ModeProbe.UpperLayer=1;
            ModeProbe.SectionLevel=TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/POIKit/TestMode.TestMode")));
            const FName ModeKey(*ModeProbe.SectionLevel.ToString());
            G->BackgroundModesBySection.Add(ModeKey,EPOIBackgroundMode::Architectural);
            TestTrue(TEXT("Architectural mode resolved from authored path"),G->ResolveBackgroundMode(ModeProbe)==EPOIBackgroundMode::Architectural);
            G->BackgroundModesBySection.Add(ModeKey,EPOIBackgroundMode::Enclosed);
            TestTrue(TEXT("Enclosed mode opt-in resolved without modifying save row"),G->ResolveBackgroundMode(ModeProbe)==EPOIBackgroundMode::Enclosed);
            G->BackgroundModesBySection.Remove(ModeKey);
            // Exercise the actual fallback card: rendering policy must never disable
            // its solid body, and switching to Enclosed restores the authored face.
            auto* PolicyProbe=World->SpawnActor<APOIShaft>();
            if(PolicyProbe)
            {
                PolicyProbe->BackgroundMode=EPOIBackgroundMode::Natural;
                PolicyProbe->Height=300.f; PolicyProbe->Recipe.UpperLayer=1;
                PolicyProbe->BuildShaft();
                int32 Checked=0;
                for(APOIKitBlock* Block:PolicyProbe->GeneratedBlocks)
                    if(Block && Block->Settings.Kind==EPOIKitKind::Background)
                    {
                        ++Checked;
                        const auto Collision=Block->GetPhysicalBody()->GetCollisionEnabled();
                        TArray<UStaticMeshComponent*> Faces;Block->GetVisualMaskMeshes(Faces);
                        TestTrue(TEXT("Natural card body is retained"),Collision!=ECollisionEnabled::NoCollision);
                        for(auto* Face:Faces) TestTrue(TEXT("Natural card face is hidden"),Face->bHiddenInGame!=0);
                        PolicyProbe->BackgroundMode=EPOIBackgroundMode::Enclosed;
                        PolicyProbe->SetBackgroundAuditSuppressed(true);
                        PolicyProbe->SetBackgroundAuditSuppressed(false);
                        for(auto* Face:Faces) TestFalse(TEXT("Enclosed restores card render flag"),Face->bHiddenInGame!=0);
                        TestEqual(TEXT("Background policy preserves body collision"),Block->GetPhysicalBody()->GetCollisionEnabled(),Collision);
                    }
                TestTrue(TEXT("Background policy exercised a real kit card"),Checked>0);
                PolicyProbe->Destroy();
            }
            else AddError(TEXT("Background policy probe could not spawn"));
            AddInfo(FString::Printf(TEXT("LIT_BACKGROUNDS cave=%d shaft=%d projected=%d cards=%d"),LitCaves,LitShafts,LitProjected,LitCards));
            // Check actual foreground geometry, not merely component visibility.
            int32 MaskQuads=0,OpenSamples=0,ShaftSamples=0,SolidSamples=0;
            int32 ShadeMin=255,ShadeMax=0,BlackVertices=0;
            TSet<uint8> ShadeLevels;
            for (const auto& Pair:G->UndergroundChunks)
            {
                auto* Mesh=Pair.Value.Get(); if (!Mesh) continue;
                TArray<float> Edges;
                // Index quads by horizontal strip rather than rescanning the refined mask for every probe.
                TMap<float,TArray<FVector4>> Strips;
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
                        ShadeLevels.Add(V.Color.R); if (V.Color.R==0) ++BlackVertices;
                        TestTrue(TEXT("Cutaway shading is neutral"),V.Color.R==V.Color.G && V.Color.G==V.Color.B);
                    }
                    for (int32 V=0;V+3<Part->ProcVertexBuffer.Num();V+=4)
                    {
                        const auto& A=Part->ProcVertexBuffer[V].Position;
                        const auto& B=Part->ProcVertexBuffer[V+1].Position;
                        TestTrue(TEXT("Mask is outside the playable lanes"),A.Y < -G->Settings.LaneSpacing-100.f);
                        Edges.AddUnique(A.X); Edges.AddUnique(B.X); ++MaskQuads;
                        const auto& C=Part->ProcVertexBuffer[V+2].Position;
                        const auto& E=Part->ProcVertexBuffer[V+3].Position;
                        Strips.FindOrAdd(float(A.X)).Add(FVector4(A.Z,B.Z,E.Z,C.Z));
                        TestTrue(TEXT("Cutaway horizontal shading spacing <=20cm"),B.X-A.X<=20.01);
                    }
                }
                Edges.Sort();
                auto Covered=[&](float X,float Z)
                {
                    const int32 I=Algo::UpperBound(Edges,X)-1;
                    if (I<0 || I+1>=Edges.Num()) return false;
                    const auto* Strip=Strips.Find(Edges[I]); if (!Strip) return false;
                    const double T=(X-Edges[I])/(Edges[I+1]-Edges[I]);
                    for (const auto& Q:*Strip)
                        if (Z>=FMath::Lerp(Q.X,Q.Y,T)-.01 && Z<=FMath::Lerp(Q.Z,Q.W,T)+.01) return true;
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
            TestTrue(TEXT("Mask reaches exact black beyond the gradient"),ShadeMin==0 && BlackVertices>0);
            TestTrue(TEXT("Mask has multiple distinct intermediate gradient levels"),ShadeLevels.Num()>=6);
            TestTrue(TEXT("Mask edge multiplier is approximately 85 percent of normal wall"),FMath::Abs(ShadeMax-int32(Edge.ToFColor(false).R))<=2 || FMath::Abs(ShadeMax-int32(Edge.ToFColor(true).R))<=2);
            AddInfo(FString::Printf(TEXT("SOLID_SHADE vertex_min=%d vertex_max=%d levels=%d black_vertices=%d"),ShadeMin,ShadeMax,ShadeLevels.Num(),BlackVertices));
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
                                TestTrue(TEXT("Solid earth switches material without changing visibility"),Mesh->GetMaterial(S)==G->WallMaterial(S-G->EarthSectionBase+1,false,false,false,bUnderground));
                                if (bUnderground) TestTrue(TEXT("Exposed earth backing is lit underground"),IsLit(Mesh->GetMaterial(S),TEXT("M_PWGv2_BackgroundLit")));
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
