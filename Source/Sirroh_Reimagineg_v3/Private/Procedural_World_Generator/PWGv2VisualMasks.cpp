#include "Procedural_World_Generator/PWGv2Generator.h"
#include "POI/POIShaft.h"
#include "Materials/MaterialInterface.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"

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

bool APWGv2Generator::DebugVisualScene(int32 Scene,APawn* Pawn,FVector& Target,FVector& Focus,FString& Label)
{
    auto* Capsule=Pawn?Pawn->FindComponentByClass<UCapsuleComponent>():nullptr;
    if(!WorldData || !Capsule || !IsDebugSession())return false;
    const float Half=Capsule->GetScaledCapsuleHalfHeight(),Radius=Capsule->GetScaledCapsuleRadius();
    const float CurrentX=GetActorTransform().InverseTransformPosition(Pawn->GetActorLocation()).X;
    if(Scene==0)
    {
        FString Error;
        Label=TEXT("Tunnel");
        if(!DebugFindDestination(TEXT("Mountains"),TEXT("Shininglands"),1,Pawn,Target,Error) &&
           !DebugFindDestination(NAME_None,NAME_None,1,Pawn,Target,Error)) return false;
        Focus=Target+FVector(0,0,150);
        return true;
    }
    float Centre=0,PreferredX=0,FocusZ=0;
    int32 Layer=1;
    if(Scene==1)
    {
        Label=TEXT("Shaft");
        const FPWGv2Portal* Best=nullptr; float Distance=MAX_flt;
        for(const auto& P:WorldData->Portals)
            if(P.bKitShaft && P.LowerLayer>1 && P.LowerLayer<=4 && FMath::Abs(P.X-CurrentX)<Distance)
            {Best=&P;Distance=FMath::Abs(P.X-CurrentX);}
        if(!Best)return false;
        Centre=Best->X; Layer=Best->LowerLayer;
        PreferredX=Centre+(Best->Width*.5f+100.f);
        FocusZ=LayerFloor(Layer,Centre)+300.f;
    }
    else
    {
        Label=Scene==2?TEXT("POI"):TEXT("Transition");
        const FPWGv2RoomChain* Best=nullptr; float Distance=MAX_flt;
        for(const auto& Chain:WorldData->RoomChains)
            if(Chain.Layer>0 && Chain.Layer<=4 && Chain.EndX-Chain.StartX>300 && FMath::Abs(Chain.StartX-CurrentX)<Distance)
            {Best=&Chain;Distance=FMath::Abs(Chain.StartX-CurrentX);}
        if(!Best)
        {
            // Old saves may have isolated authored rooms rather than room chains.
            int32 Index=INDEX_NONE;
            for(int32 I=0;I<WorldData->AuthoredPOIs.Num() && I<WorldData->AuthoredPOIPositions.Num();++I)
                if(WorldData->AuthoredPOIs[I].UpperLayer>0 && FMath::Abs(WorldData->AuthoredPOIPositions[I].X-CurrentX)<Distance)
                {Index=I;Distance=FMath::Abs(WorldData->AuthoredPOIPositions[I].X-CurrentX);}
            if(Index==INDEX_NONE)return false;
            const auto& Row=WorldData->AuthoredPOIs[Index]; const auto& Pos=WorldData->AuthoredPOIPositions[Index];
            Layer=Row.UpperLayer; Centre=Pos.X+(Scene==3?-Row.Width*.5f:0.f);
            PreferredX=Centre; FocusZ=LayerFloor(Layer,Centre)+200.f;
        }
        else
        {
            Layer=Best->Layer;
            Centre=Scene==2?Best->StartX+FMath::Min((Best->EndX-Best->StartX)*.5f,600.f):Best->StartX;
            PreferredX=Centre; FocusZ=Best->FloorZ+200.f;
        }
    }
    FCollisionQueryParams Query(SCENE_QUERY_STAT(VisualQATeleport),false,Pawn);
    for(float Offset:{0.f,100.f,-100.f,250.f,-250.f,450.f,-450.f,700.f,-700.f})
    {
        const float X=PreferredX+Offset;
        if(X<=WorldData->MinX || X>=WorldData->MaxX || LayerCeiling(Layer,X)-LayerFloor(Layer,X)<Half*2.f+20.f)continue;
        StreamAt(X,true);
        GetWorld()->FlushLevelStreaming(EFlushLevelStreamingType::Full);
        const FVector Expected=GetActorTransform().TransformPosition(FVector(X,0,LayerFloor(Layer,X)));
        FHitResult Hit;
        if(!GetWorld()->LineTraceSingleByChannel(Hit,Expected+FVector(0,0,100),Expected-FVector(0,0,100),ECC_Pawn,Query) || Hit.ImpactNormal.Z<.7f)continue;
        const FVector PawnCentre=Hit.ImpactPoint+FVector(0,0,Half+5.f);
        if(GetWorld()->OverlapBlockingTestByChannel(PawnCentre,Capsule->GetComponentQuat(),ECC_Pawn,FCollisionShape::MakeCapsule(Radius,Half),Query))continue;
        Target=PawnCentre-(Capsule->GetComponentLocation()-Pawn->GetActorLocation());
        if(LayerAt(Target)!=Layer)continue;
        DebugTeleportPinX=X; DebugTeleportPinUntil=GetWorld()->GetTimeSeconds()+3.f;
        Focus=GetActorTransform().TransformPosition(FVector(Centre,0,FocusZ));
        UE_LOG(LogTemp,Display,TEXT("VF_SCENE %s target=%s focus=%s layer=%d chunksize=%.0f"),*Label,*Target.ToCompactString(),*Focus.ToCompactString(),Layer,Settings.ChunkLengthMetres*100.f);
        return true;
    }
    return false;
}
