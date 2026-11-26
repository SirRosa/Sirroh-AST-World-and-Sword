#include "Procedural_World_Generator/PWGv2Generator.h"

int32 APWGv2Generator::VisualRoleForSection(int32 S)
{
    // Only explicit rear wall sections are eligible for ambient lift. Keep SolidEarth,
    // collision banks, props and unclassified geometry out of the rear mask.
    if ((S>=5 && S<=9) || (S>=13 && S<=16) || S>=SolidCutawaySectionBase+4) return VisualRear;
    if (IsSolidCutawaySection(S)) return VisualCutaway;
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
}

void APWGv2Generator::BuildVisualMasks(int32 Index)
{
    auto* Batch=DecorationChunks.Find(Index);
    auto* Source=UndergroundChunks.FindRef(Index).Get();
    if (!Batch || !Source || Batch->bVisualMasksBuilt) return;
    Batch->bVisualMasksBuilt=true;
    for (int32 Role:{VisualRear,VisualCutaway})
    {
        UProceduralMeshComponent* Proxy=nullptr;
        for (int32 S=0;S<Source->GetNumSections();++S)
        {
            const FProcMeshSection* Part=Source->GetProcMeshSection(S);
            if (VisualRoleForSection(S)!=Role || !Part || Part->ProcIndexBuffer.IsEmpty()) continue;
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
                Proxy->SetCustomDepthStencilValue(Role);
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
        }
    }
}
