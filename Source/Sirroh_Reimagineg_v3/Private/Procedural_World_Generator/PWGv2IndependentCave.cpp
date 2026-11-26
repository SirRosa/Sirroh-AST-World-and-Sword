#include "PWGv2IndependentCave.h"
#include "Procedural_World_Generator/PWGv2Generator.h"
#include "PWG/PWGEnvironmentV2.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "HAL/PlatformTime.h"

void APWGv2Generator::ResetCaveReference()
{
    const bool bWasActive=bCaveReferenceActive;
    bCaveReferenceActive=false; bCaveReferencePinned=false;
    if(IsValid(CaveReferenceMesh)) CaveReferenceMesh->DestroyComponent();
    CaveReferenceMesh=nullptr; CaveReferenceMaterials.Reset();
    for(float& V:CaveReferenceTextures) V=-1.f;
    CaveReferenceFlatAlbedo=-1.f;
    CaveReferenceDepth=0.f;
    if(bWasActive) for(const auto& Pair:UndergroundChunks) ApplyChunkLayerVisibility(Pair.Key);
}

bool APWGv2Generator::BuildCaveReference(const FVector& Pawn,float Depth)
{
    if(!WorldData || !bCaveReferencePinned) return false;
    const int32 Layer=LayerAt(Pawn);
    if(Layer<=0 || !WorldData->Layers.IsValidIndex(Layer-1)) return false;
    const double Beg=FPlatformTime::Seconds();
    // Materials are created once per reference, not per rock. Asset failure leaves Stage 1 visible.
    if(CaveReferenceMaterials.Num()!=4)
    {
        CaveReferenceMaterials.Reset();
        for(int32 I=0;I<4;++I)
        {
            auto* M=CreateCaveReferenceMaterial(Layer);
            if(!M){CaveReferenceMaterials.Reset();return false;}
            CaveReferenceMaterials.Add(M);
        }
    }
    const float Floor=LayerFloor(Layer,CaveReferenceAnchor.X);
    const float Roof=LayerCeiling(Layer,CaveReferenceAnchor.X);
    const int32 Seed=int32(HashCombineFast(uint32(WorldData->Seed),uint32(FMath::FloorToInt(CaveReferenceAnchor.X/100.f))));
    CaveReferenceStart=FMath::Max(125.f,Settings.LaneSpacing+65.f);
    CaveReferenceDepth=Depth;
    FPWGv2MeshBuffers B[4];
    PWGIndependentCave::Build(B,Seed,CaveReferenceAnchor.X,Floor,Roof,CaveReferenceWidth,
        CaveReferenceAnchor.Z,CaveReferenceHeight,CaveReferenceHalfWidth,CaveReferenceStart,Depth);
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
        B[I].Submit(CaveReferenceMesh,I);
        CaveReferenceMesh->SetMaterial(I,CaveReferenceMaterials[I]);
        Vertices+=B[I].V.Num();Triangles+=B[I].T.Num()/3;
    }
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
    const FVector LocalView=GetActorTransform().InverseTransformPosition(View);
    if(bAllowed && !bCaveReferencePinned && bUndergroundView && Width>100.f && Height>100.f)
    {
        CaveReferenceAnchor=FVector(LocalView.X,0,LocalView.Z);
        CaveReferenceWidth=Width; CaveReferenceHeight=Height;
        CaveReferenceHalfWidth=Width*.95f+2.f*Settings.ChunkLengthMetres*100.f;
        bCaveReferencePinned=true;
    }
    bool bWanted=bAllowed && bUndergroundView && bCaveReferencePinned && BackgroundAuditIsolation==0
        && FMath::Abs(LocalView.X-CaveReferenceAnchor.X)<=CaveReferenceWidth*.22
        && FMath::Abs(LocalView.Z-CaveReferenceAnchor.Z)<=CaveReferenceHeight*.28
        && Width<=CaveReferenceWidth*1.15f && Height<=CaveReferenceHeight*1.15f;
    if(bWanted && (!IsValid(CaveReferenceMesh) || CaveReferenceDepth!=Profile.CaveVisualDepthMetres*100.f))
    {
        // Do not retry missing assets on every tick. Toggling the reference resets the latch.
        if(CaveReferenceDepth<0.f) bWanted=false;
        else if(!BuildCaveReference(Pawn,Profile.CaveVisualDepthMetres*100.f))
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
            CaveReferenceFlatAlbedo=Flat;
        }
        const bool Show[4]={Profile.bShowCaveNear,Profile.bShowCaveMid,Profile.bShowCaveFar,Profile.bShowCaveClosure};
        for(int32 I=0;I<4;++I)
        {
            if(CaveReferenceMesh->IsMeshSectionVisible(I)!=Show[I]) CaveReferenceMesh->SetMeshSectionVisible(I,Show[I]);
            const float T=Profile.CaveGrade(I).TextureIntensity;
            if(CaveReferenceMaterials.IsValidIndex(I) && CaveReferenceTextures[I]!=T)
            {
                CaveReferenceMaterials[I]->SetScalarParameterValue(TEXT("CaveTextureIntensity"),T);
                CaveReferenceTextures[I]=T;
            }
        }
    }
    if(bCaveReferenceActive!=bWanted)
    {
        bCaveReferenceActive=bWanted;
        for(const auto& Pair:UndergroundChunks) ApplyChunkLayerVisibility(Pair.Key);
    }
}
