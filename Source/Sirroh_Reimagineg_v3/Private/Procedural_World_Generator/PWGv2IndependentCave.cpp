#include "PWGv2IndependentCave.h"
#include "Procedural_World_Generator/PWGv2Generator.h"
#include "PWG/PWGEnvironmentV2.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "HAL/PlatformTime.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"

FPWGDepthFormationProfile APWGv2Generator::ResolveDepthFormationProfile(int32 Layer,float LocalX) const
{
    FName Biome=NAME_None;
    if(WorldData)for(const auto& Span:WorldData->Biomes)
        if(LocalX>=Span.StartX && LocalX<Span.EndX){Biome=Span.Id;break;}
    return FPWGDepthFormationProfile::Resolve(DepthFormationProfiles,Layer,Biome);
}

void APWGv2Generator::ClearCaveReferenceInstances()
{
    for(const auto& C:CaveReferenceInstances)if(IsValid(C))C->DestroyComponent();
    CaveReferenceInstances.Reset();CaveReferenceInstanceMaterials.Reset();CaveReferenceInstancePlans.Reset();
}

void APWGv2Generator::ResetCaveReference()
{
    const bool bWasActive=bCaveReferenceActive;
    bCaveReferenceActive=false; bCaveReferencePinned=false; bCaveReferenceUsesProfiles=false;
    if(IsValid(CaveReferenceMesh)) CaveReferenceMesh->DestroyComponent();
    CaveReferenceMesh=nullptr; CaveReferenceMaterials.Reset(); ClearCaveReferenceInstances();
    for(float& V:CaveReferenceTextures) V=-1.f;
    CaveReferenceFlatAlbedo=-1.f; CaveReferenceLastDebug=-1;
    CaveReferenceDepth=0.f;
    if(bWasActive) for(const auto& Pair:UndergroundChunks) ApplyChunkLayerVisibility(Pair.Key);
}

bool APWGv2Generator::BuildCaveReference(const FVector& Pawn,float Depth,bool bHybrid)
{
    if(!WorldData || !bCaveReferencePinned) return false;
    const int32 Layer=LayerAt(Pawn);
    if(Layer<=0 || !WorldData->Layers.IsValidIndex(Layer-1)) return false;
    const double Beg=FPlatformTime::Seconds();
    static const TCHAR* MeshPaths[]={
        TEXT("/AST_InventorySystem/NiagaraExamples/StaticMesh/S_Rock_shopk.S_Rock_shopk"),
        TEXT("/AST_InventorySystem/Effects/Meshes/Rocks/Debris_Chunk1.Debris_Chunk1"),
        TEXT("/AST_InventorySystem/Effects/Meshes/Rocks/Debris_Chunk2.Debris_Chunk2"),
        TEXT("/AST_InventorySystem/Effects/Meshes/Rocks/Debris_Chunk3.Debris_Chunk3"),
        TEXT("/AST_InventorySystem/Effects/Meshes/Rocks/ultkedqjw_LOD7.ultkedqjw_LOD7")};
    static const TCHAR* TexturePaths[]={
        nullptr,
        TEXT("/AST_InventorySystem/Effects/Textures/Rocks/Debris_Chunk1_BaseColor.Debris_Chunk1_BaseColor"),
        TEXT("/AST_InventorySystem/Effects/Textures/Rocks/Debris_Chunk2_BaseColor.Debris_Chunk2_BaseColor"),
        TEXT("/AST_InventorySystem/Effects/Textures/Rocks/Debris_Chunk3_BaseColor.Debris_Chunk3_BaseColor"),
        TEXT("/AST_InventorySystem/Effects/Textures/Rocks/ultkedqjw_4K_Albedo.ultkedqjw_4K_Albedo")};
    UStaticMesh* Assets[5]={};
    if(bHybrid)for(int32 K=0;K<5;++K)
    {
        Assets[K]=LoadObject<UStaticMesh>(nullptr,MeshPaths[K]);
        if(!Assets[K] || Assets[K]->GetBoundingBox().GetExtent().GetMin()<.001)
        {UE_LOG(LogTemp,Error,TEXT("VF_CAVE_HYBRID missing/degenerate %s"),MeshPaths[K]);return false;}
    }
    ClearCaveReferenceInstances();
    if(bCaveReferenceHybrid!=bHybrid){CaveReferenceMaterials.Reset();bCaveReferenceHybrid=bHybrid;}
    for(float& V:CaveReferenceTextures)V=-1.f;
    CaveReferenceFlatAlbedo=-1.f; CaveReferenceLastDebug=-1;
    // Materials are created once per reference, not per rock. Asset failure leaves Stage 1 visible.
    if(CaveReferenceMaterials.Num()!=4)
    {
        CaveReferenceMaterials.Reset();
        for(int32 I=0;I<4;++I)
        {
            auto* M=CreateCaveReferenceMaterial(Layer,I);
            if(!M){CaveReferenceMaterials.Reset();return false;}
            CaveReferenceMaterials.Add(M);
        }
    }
    const float Floor=LayerFloor(Layer,CaveReferenceAnchor.X);
    const float Roof=LayerCeiling(Layer,CaveReferenceAnchor.X);
    const int32 Seed=int32(HashCombineFast(uint32(WorldData->Seed),uint32(FMath::FloorToInt(CaveReferenceAnchor.X/100.f))));
    CaveReferenceStart=FMath::Max(125.f,Settings.LaneSpacing+65.f);
    CaveReferenceDepth=Depth;
    FPWGv2MeshBuffers B[4]; TArray<PWGIndependentCave::FHybridInstance> Instances;
    PWGIndependentCave::FRecipe Recipe;
    if(bCaveReferenceUsesProfiles)
    {
        Recipe.Scale=CaveReferenceResolved.Scale;Recipe.Density=CaveReferenceResolved.Density;
        Recipe.Families=uint32(CaveReferenceResolved.FamilyMask);Recipe.PlanCount=CaveReferenceResolved.FormationPlans;
        Recipe.Composition=int32(CaveReferenceResolved.Composition);
        UE_LOG(LogTemp,Display,TEXT("VF_CAVE_PROFILE layer=%d biome=%s depth=%.1f plans=%d familymask=%d scale=%.2f density=%.2f"),
            Layer,*CaveReferenceResolved.Biome.ToString(),Depth,Recipe.PlanCount,CaveReferenceResolved.FamilyMask,Recipe.Scale,Recipe.Density);
    }
    PWGIndependentCave::Build(B,Seed,CaveReferenceAnchor.X,Floor,Roof,CaveReferenceWidth,
        CaveReferenceAnchor.Z,CaveReferenceHeight,CaveReferenceHalfWidth,CaveReferenceStart,Depth,Recipe,bHybrid?&Instances:nullptr);
    if(!IsValid(CaveReferenceMesh))
    {
        CaveReferenceMesh=NewObject<UProceduralMeshComponent>(this);
        CaveReferenceMesh->SetupAttachment(GetRootComponent());
        CaveReferenceMesh->SetMobility(EComponentMobility::Movable);
        CaveReferenceMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        CaveReferenceMesh->SetCanEverAffectNavigation(false);
        CaveReferenceMesh->SetCastShadow(false);
        CaveReferenceMesh->SetRenderCustomDepth(true);
        CaveReferenceMesh->SetCustomDepthStencilValue(VisualRear);
        CaveReferenceMesh->ComponentTags.Add(TEXT("PWGCaveReference"));
        CaveReferenceMesh->RegisterComponent();
    }
    int32 Vertices=0,Triangles=0;
    for(int32 I=0;I<4;++I)
    {
        if(B[I].V.IsEmpty())CaveReferenceMesh->ClearMeshSection(I);
        else PWGIndependentCave::Submit(B[I],CaveReferenceMesh,I);
        CaveReferenceMesh->SetMaterial(I,CaveReferenceMaterials[I]);
        Vertices+=B[I].V.Num();Triangles+=B[I].T.Num()/3;
    }
    UInstancedStaticMeshComponent* Groups[3][5]={};
    int32 Added=0;
    for(const auto& I:Instances)
    {
        auto*& C=Groups[I.Plan][I.Mesh];
        if(!C)
        {
            auto* M=CreateCaveReferenceMaterial(Layer);
            if(!M){ClearCaveReferenceInstances();return false;}
            if(TexturePaths[I.Mesh])
            {
                if(auto* T=LoadObject<UTexture2D>(nullptr,TexturePaths[I.Mesh])) M->SetTextureParameterValue(TEXT("RockAlbedo"),T);
                // Debris textures lack a verified matching normal map.
                M->SetScalarParameterValue(TEXT("CaveNormalScale"),0.f);
            }
            C=NewObject<UInstancedStaticMeshComponent>(this);
            C->SetupAttachment(GetRootComponent());C->SetMobility(EComponentMobility::Movable);
            C->SetStaticMesh(Assets[I.Mesh]);C->SetMaterial(0,M);
            C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetCanEverAffectNavigation(false);
            C->SetCastShadow(false);C->SetRenderCustomDepth(true);C->SetCustomDepthStencilValue(VisualRear);
            C->ComponentTags.Add(TEXT("PWGCaveReference"));C->RegisterComponent();
            CaveReferenceInstances.Add(C);CaveReferenceInstanceMaterials.Add(M);CaveReferenceInstancePlans.Add(I.Plan);
        }
        const FBox Box=Assets[I.Mesh]->GetBoundingBox();
        const FVector Ext=Box.GetExtent();
        const FQuat Q(FVector::YAxisVector,FMath::DegreesToRadians(I.Angle));
        const FVector Rot=Q.RotateVector(FVector::XAxisVector).GetAbs()*I.Extent.X+
            Q.RotateVector(FVector::YAxisVector).GetAbs()*I.Extent.Y+Q.RotateVector(FVector::ZAxisVector).GetAbs()*I.Extent.Z;
        const double Fit=FMath::Min3(I.Extent.X/FMath::Max(Rot.X,.001),I.Extent.Y/FMath::Max(Rot.Y,.001),I.Extent.Z/FMath::Max(Rot.Z,.001));
        const FVector Scale=I.Extent/Ext*Fit;
        const FVector P=I.Centre-Q.RotateVector(Box.GetCenter()*Scale);
        C->AddInstance(FTransform(Q,P,Scale),false);++Added;
    }
    UE_LOG(LogTemp,Display,TEXT("VF_CAVE_HYBRID enabled=%d instances=%d batches=%d"),bHybrid,Added,CaveReferenceInstances.Num());
    UE_LOG(LogTemp,Display,TEXT("VF_CAVE_REFERENCE seed=%d layer=%d depth_cm=%.0f floor=%.1f roof=%.1f width=%.1f vertices=%d triangles=%d ms=%.3f"),
        Seed,Layer,Depth,Floor,Roof,CaveReferenceWidth,Vertices,Triangles,(FPlatformTime::Seconds()-Beg)*1000.);
    return true;
}

bool APWGv2Generator::SuppressCaveReferenceOriginal(const FPWGv2DecorationBatch& Batch,int32 Section) const
{
    if(!IsCaveReferenceActive() || !Batch.UndergroundSectionBounds.IsValidIndex(Section)) return false;
    if(!IsEarthSection(Section) && !(Section>=5 && Section<=9) && !(Section>=13 && Section<=16)) return false;
    const FBox& Box=Batch.UndergroundSectionBounds[Section];
    if(!Box.IsValid) return false;
    const FVector LocalCentre=GetActorTransform().InverseTransformPosition(Box.GetCenter());
    // Only chunks intersecting the bounded reference aperture are suppressed.
    // Projected Enclosed POI art, cutaway, physics and gameplay sections are never changed.
    return FMath::Abs(LocalCentre.X-CaveReferenceAnchor.X)<=CaveReferenceWidth*.85+Settings.ChunkLengthMetres*100.f;
}

void APWGv2Generator::UpdateCaveReference(const FPWGVisualProfile& Profile,bool bAllowed,
    const FVector& View,float Width,float Height,const FVector& Pawn)
{
    if(!Profile.bCaveReference)
    {
        if(bCaveReferencePinned || IsValid(CaveReferenceMesh)) ResetCaveReference();
        return;
    }
    if(bCaveReferencePinned && bCaveReferenceUsesProfiles!=Profile.bUseDepthFormationProfiles) ResetCaveReference();
    const FVector LocalView=GetActorTransform().InverseTransformPosition(View);
    if(bAllowed && !bCaveReferencePinned && bUndergroundView && Width>100.f && Height>100.f)
    {
        CaveReferenceAnchor=FVector(LocalView.X,0,LocalView.Z);
        CaveReferenceWidth=Width; CaveReferenceHeight=Height;
        CaveReferenceHalfWidth=Width*.95f+2.f*Settings.ChunkLengthMetres*100.f;
        bCaveReferenceUsesProfiles=Profile.bUseDepthFormationProfiles;
        if(bCaveReferenceUsesProfiles)CaveReferenceResolved=ResolveDepthFormationProfile(LayerAt(Pawn),CaveReferenceAnchor.X);
        bCaveReferencePinned=true;
    }
    bool bWanted=bAllowed && bUndergroundView && bCaveReferencePinned && BackgroundAuditIsolation==0
        && FMath::Abs(LocalView.X-CaveReferenceAnchor.X)<=CaveReferenceWidth*.22
        && FMath::Abs(LocalView.Z-CaveReferenceAnchor.Z)<=CaveReferenceHeight*.28
        && Width<=CaveReferenceWidth*1.15f && Height<=CaveReferenceHeight*1.15f;
    const float WantedDepth=100.f*(bCaveReferenceUsesProfiles?CaveReferenceResolved.VisualDepthMetres:Profile.CaveVisualDepthMetres);
    if(bWanted && (!IsValid(CaveReferenceMesh) || CaveReferenceDepth!=WantedDepth || bCaveReferenceHybrid!=Profile.bCaveHybridFormations))
    {
        // Do not retry missing assets on every tick. Toggling the reference resets the latch.
        if(CaveReferenceDepth<0.f) bWanted=false;
        else if(!BuildCaveReference(Pawn,WantedDepth,Profile.bCaveHybridFormations))
        {
            CaveReferenceDepth=-1.f; bWanted=false;
            UE_LOG(LogTemp,Error,TEXT("VF_CAVE_REFERENCE failed to build; Stage 1 retained"));
        }
    }
    if(IsValid(CaveReferenceMesh))
    {
        if(CaveReferenceMesh->IsVisible()!=bWanted) CaveReferenceMesh->SetVisibility(bWanted);
        const float Flat=Profile.bDisableBackgroundAmbient ? .006f : .16f;
        if(Flat!=CaveReferenceFlatAlbedo)
        {
            for(const auto& M:CaveReferenceMaterials) if(IsValid(M))
                M->SetVectorParameterValue(TEXT("CaveFlatAlbedo"),FLinearColor(Flat,Flat*1.06f,Flat*1.12f,1.f));
            for(const auto& M:CaveReferenceInstanceMaterials)if(IsValid(M))
                M->SetVectorParameterValue(TEXT("CaveFlatAlbedo"),FLinearColor(Flat,Flat*1.06f,Flat*1.12f,1.f));
            CaveReferenceFlatAlbedo=Flat;
        }
        const int32 Debug=Profile.CaveMaterialDebug;
        if(Debug!=CaveReferenceLastDebug)
        {
            for(const auto& M:CaveReferenceMaterials) if(IsValid(M))
                M->SetScalarParameterValue(TEXT("CaveDebugMode"),float(Debug));
            for(const auto& M:CaveReferenceInstanceMaterials)if(IsValid(M))
                M->SetScalarParameterValue(TEXT("CaveDebugMode"),float(Debug));
            CaveReferenceLastDebug=Debug;
        }
        const bool Show[4]={Profile.bShowCaveNear,Profile.bShowCaveMid,Profile.bShowCaveFar,Profile.bShowCaveClosure};
        for(int32 I=0;I<4;++I)
        {
            if(CaveReferenceMesh->GetProcMeshSection(I) && CaveReferenceMesh->IsMeshSectionVisible(I)!=Show[I]) CaveReferenceMesh->SetMeshSectionVisible(I,Show[I]);
            const float T=Profile.CaveGrade(I).TextureIntensity*(bCaveReferenceUsesProfiles?CaveReferenceResolved.DetailIntensity:1.f);
            if(CaveReferenceMaterials.IsValidIndex(I) && CaveReferenceTextures[I]!=T)
            {
                CaveReferenceMaterials[I]->SetScalarParameterValue(TEXT("CaveTextureIntensity"),T);
                for(int32 K=0;K<CaveReferenceInstanceMaterials.Num();++K)
                    if(CaveReferenceInstancePlans[K]==I && IsValid(CaveReferenceInstanceMaterials[K]))
                        CaveReferenceInstanceMaterials[K]->SetScalarParameterValue(TEXT("CaveTextureIntensity"),T);
                CaveReferenceTextures[I]=T;
            }
        }
    }
    const bool VisiblePlans[4]={Profile.bShowCaveNear,Profile.bShowCaveMid,Profile.bShowCaveFar,Profile.bShowCaveClosure};
    for(int32 K=0;K<CaveReferenceInstances.Num();++K)if(IsValid(CaveReferenceInstances[K]))
    {
        const bool V=bWanted && VisiblePlans[CaveReferenceInstancePlans[K]];
        if(CaveReferenceInstances[K]->IsVisible()!=V)CaveReferenceInstances[K]->SetVisibility(V);
    }
    if(bCaveReferenceActive!=bWanted)
    {
        bCaveReferenceActive=bWanted;
        for(const auto& Pair:UndergroundChunks) ApplyChunkLayerVisibility(Pair.Key);
    }
}
