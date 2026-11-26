#include "Procedural_World_Generator/PWGv2Generator.h"
#include "POI/POIShaft.h"
#include "Materials/MaterialInterface.h"

int32 APWGv2Generator::VisualRoleForSection(int32 S)
{
    // Every existing PWG underground section has an explicit visual role.
    // SolidEarth is a far backing, not the opaque front cutaway; surface uses Legacy.
    if ((S>=5 && S<=9) || (S>=13 && S<=16) || IsEarthSection(S) || S>=SolidCutawaySectionBase+4) return VisualRear;
    if (IsSolidCutawaySection(S) || (S>=17 && S<=24) || S==10) return VisualCutaway;
    if (S>=1 && S<=4) return VisualGameplay;
    return 0;
}

void APWGv2Generator::SetVisualMaskPassEnabled(bool bEnabled)
{
    if (bVisualMaskPassEnabled==bEnabled) return;
    bVisualMaskPassEnabled=bEnabled;
    for (const auto& Pair:UndergroundChunks)
    {
        if (bEnabled) BuildVisualMasks(Pair.Key);
        SyncVisualMasks(Pair.Key);
    }
    for (const auto& Pair:DecorationChunks)
        for (AActor* Actor:Pair.Value.Actors)
            if (auto* POI=Cast<APOIShaft>(Actor)) POI->SetVisualMaskEnabled(IsVisualMaskPassEnabled());
}

void APWGv2Generator::BuildVisualMasks(int32 Index)
{
    auto* Batch=DecorationChunks.Find(Index);
    auto* Source=UndergroundChunks.FindRef(Index).Get();
    if (!Batch || !Source || Batch->bVisualMasksBuilt) return;
    Batch->bVisualMasksBuilt=true;
    for (int32 StencilRole:{VisualRear,VisualCutaway,VisualGameplay})
    {
        UProceduralMeshComponent* Proxy=nullptr;
        for (int32 S=0;S<Source->GetNumSections();++S)
        {
            const FProcMeshSection* Part=Source->GetProcMeshSection(S);
            if (VisualRoleForSection(S)!=StencilRole || !Part || Part->ProcIndexBuffer.IsEmpty()) continue;
            // The translucent near-shell is intentionally invisible in the cutaway;
            // never let a translucent mask occlude the actual visible rear depth.
            if(auto* Mat=Source->GetMaterial(S); Mat && Mat->GetBlendMode()!=BLEND_Opaque && Mat->GetBlendMode()!=BLEND_Masked) continue;
            if (!Proxy)
            {
                Proxy=NewObject<UProceduralMeshComponent>(this);
                Proxy->SetupAttachment(GetRootComponent());
                Proxy->SetMobility(EComponentMobility::Static);
                Proxy->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                Proxy->SetCanEverAffectNavigation(false);
                Proxy->SetCastShadow(false);
                Proxy->SetRenderInMainPass(false);
                Proxy->SetRenderInDepthPass(false);
                Proxy->SetRenderCustomDepth(true);
                Proxy->SetCustomDepthStencilValue(StencilRole);
                Proxy->SetVisibility(false);
                Proxy->bUseComplexAsSimpleCollision=false;
                Proxy->ComponentTags.Add(TEXT("PWGVisualMask"));
            }
            FProcMeshSection Copy=*Part;
            Copy.bEnableCollision=false;
            Proxy->SetProcMeshSection(S,Copy);
            Proxy->SetMaterial(S,Source->GetMaterial(S));
        }
        if (Proxy)
        {
            Proxy->RegisterComponent();
            Batch->VisualMasks.Add(Proxy);
        }
    }
    SyncVisualMasks(Index);
}

void APWGv2Generator::SyncVisualMasks(int32 Index)
{
    auto* Batch=DecorationChunks.Find(Index);
    auto* Source=UndergroundChunks.FindRef(Index).Get();
    if (!Batch || !Source) return;
    const bool bShow=bUndergroundView && bVisualMaskPassEnabled;
    for (UProceduralMeshComponent* Proxy:Batch->VisualMasks)
    {
        if (!IsValid(Proxy)) continue;
        if (Proxy->IsVisible()!=bShow) Proxy->SetVisibility(bShow);
        if (!bShow) continue;
        for (int32 S=0;S<Proxy->GetNumSections();++S)
        {
            const auto* Part=Proxy->GetProcMeshSection(S);
            if (!Part || Part->ProcIndexBuffer.IsEmpty()) continue;
            const bool bVisible=Source->IsMeshSectionVisible(S);
            if (Proxy->IsMeshSectionVisible(S)!=bVisible) Proxy->SetMeshSectionVisible(S,bVisible);
            if (Proxy->GetMaterial(S)!=Source->GetMaterial(S)) Proxy->SetMaterial(S,Source->GetMaterial(S));
        }
    }
}

void APWGv2Generator::DebugVisualCoverage(const FTransform& Camera,float Width,float Height) const
{
    const FVector Right=Camera.GetUnitAxis(EAxis::Y), Up=Camera.GetUnitAxis(EAxis::Z), Forward=Camera.GetUnitAxis(EAxis::X);
    // Inspect actual source triangles, rather than collision or proxy bounds.
    for (float U:{-.46f,-.30f,0.f,.30f,.46f}) for(float V:{-.35f,0.f,.35f})
    {
        const FVector Ray=Camera.GetLocation()+Right*(U*Width)+Up*(V*Height);
        float Best=MAX_flt; int32 Section=INDEX_NONE, Chunk=INDEX_NONE, Stencil=0; bool Visible=false;
        for (const auto& Pair:UndergroundChunks)
        {
            auto* Mesh=Pair.Value.Get();
            if (!Mesh) continue;
            const FBox Bounds=Mesh->Bounds.GetBox();
            const FVector Offset=Bounds.GetCenter()-Ray, Extent=Bounds.GetExtent();
            if(FMath::Abs(Offset.Dot(Right))>Extent.Dot(Right.GetAbs())+1 || FMath::Abs(Offset.Dot(Up))>Extent.Dot(Up.GetAbs())+1) continue;
            for(int32 S=0;S<Mesh->GetNumSections();++S)
            {
                const auto* Part=Mesh->GetProcMeshSection(S);
                if(!Part || Part->ProcIndexBuffer.IsEmpty()) continue;
                auto* Mat=Mesh->GetMaterial(S);
                if(Mat && Mat->GetBlendMode()!=BLEND_Opaque && Mat->GetBlendMode()!=BLEND_Masked) continue;
                for(int32 I=0;I+2<Part->ProcIndexBuffer.Num();I+=3)
                {
                    const FVector A=Mesh->GetComponentTransform().TransformPosition(Part->ProcVertexBuffer[Part->ProcIndexBuffer[I]].Position)-Ray;
                    const FVector B=Mesh->GetComponentTransform().TransformPosition(Part->ProcVertexBuffer[Part->ProcIndexBuffer[I+1]].Position)-Ray;
                    const FVector C=Mesh->GetComponentTransform().TransformPosition(Part->ProcVertexBuffer[Part->ProcIndexBuffer[I+2]].Position)-Ray;
                    const FVector2D P(A.Dot(Right),A.Dot(Up)), Q(B.Dot(Right),B.Dot(Up)), R(C.Dot(Right),C.Dot(Up));
                    const FVector2D E=Q-P,F=R-P;
                    const double Det=E.X*F.Y-E.Y*F.X;
                    if(FMath::Abs(Det)<1.e-6) continue;
                    const double T=(-P.X*F.Y+P.Y*F.X)/Det, K=(-E.X*P.Y+E.Y*P.X)/Det;
                    if(T<-.0001 || K<-.0001 || T+K>1.0001)continue;
                    const float D=float((A+(B-A)*T+(C-A)*K).Dot(Forward));
                    if(D<0 || D>=Best)continue;
                    Best=D; Section=S; Chunk=Pair.Key; Stencil=VisualRoleForSection(S); Visible=Mesh->IsMeshSectionVisible(S);
                }
            }
        }
        UE_LOG(LogTemp,Display,TEXT("VF_COVERAGE u=%.2f v=%.2f chunk=%d section=%d role=%d visible=%d depth=%.2f"),U,V,Chunk,Section,Stencil,Visible,Best);
    }
    int32 Proxies=0,Triangles=0,POIs=0;
    for(const auto& Pair:DecorationChunks)
    {
        for(UProceduralMeshComponent* Proxy:Pair.Value.VisualMasks) if(Proxy && Proxy->IsVisible())
        {
            ++Proxies;
            for(int32 S=0;S<Proxy->GetNumSections();++S)
                if(Proxy->IsMeshSectionVisible(S)) if(const auto* Part=Proxy->GetProcMeshSection(S)) Triangles+=Part->ProcIndexBuffer.Num()/3;
        }
        for(AActor* Actor:Pair.Value.Actors) if(auto* POI=Cast<APOIShaft>(Actor))
        {
            ++POIs;
            POI->LogVisualMaskCoverage();
        }
    }
    UE_LOG(LogTemp,Display,TEXT("VF_MASK_COUNTS chunks=%d under=%d proxies=%d visible_triangles=%d POIs=%d"),Chunks.Num(),UndergroundChunks.Num(),Proxies,Triangles,POIs);
}
