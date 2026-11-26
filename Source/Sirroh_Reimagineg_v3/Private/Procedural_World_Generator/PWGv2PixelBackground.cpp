#include "Procedural_World_Generator/PWGv2Generator.h"
#include "PWG/PWGEnvironmentV2.h"
#include "PWGv2Mesh.h"
#include "PWGv2PixelField.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "HAL/PlatformTime.h"

float APWGv2Generator::GetPixelBackgroundStart() const
{
    return FMath::Max(260.f,Settings.LaneSpacing+60.f);
}
float APWGv2Generator::GetPixelBackgroundSpan() const
{
    const float Earth=FMath::Min(Settings.BackgroundDepth,Settings.LaneSpacing+450.f);
    return FMath::Max(240.f,Earth-10.f-GetPixelBackgroundStart());
}
bool APWGv2Generator::EnsurePixelBackgroundMaterial()
{
    if (!bTriedPixelMaterial)
    {
        bTriedPixelMaterial=true;
        PixelBackgroundMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Systems/Procedural_World_Generator/PWG_v2/M_PWGv2_PixelBackground.M_PWGv2_PixelBackground"));
        if(!PixelBackgroundMaterial) UE_LOG(LogTemp,Error,TEXT("PWG_PIXEL2D: data material missing; retain Stage 1"));
    }
    return IsValid(PixelBackgroundMaterial);
}

void APWGv2Generator::BuildPixelBackground(int32 Index,float Start,float End,UProceduralMeshComponent* Mesh)
{
    if(!WorldData || !Mesh || !EnsurePixelBackgroundMaterial() || End<=Start) return;
    auto& Batch=DecorationChunks.FindOrAdd(Index);
    const double Beg=FPlatformTime::Seconds();
    // Global vertical strips, selected by actual opening/shaft bounds. Split at
    // fixed coordinates, not at arbitrary room/POI boundaries. The same integer
    // texel field is sampled on either side of chunk and strip boundaries.
    constexpr double TileHeight=1024.0;
    TMap<int32,FVector2D> Strips;
    for(int32 S=5;S<=16;++S)
    {
        if(S>9 && S<13) continue;
        const auto* Part=Mesh->GetProcMeshSection(S);
        if(!Part || !Part->SectionLocalBox.IsValid || Part->ProcIndexBuffer.IsEmpty()) continue;
        const double Low=Part->SectionLocalBox.Min.Z-100, High=Part->SectionLocalBox.Max.Z+100;
        for(int32 K=FMath::FloorToInt(Low/TileHeight);K<=FMath::FloorToInt(High/TileHeight);++K)
        {
            const double A=FMath::Max(Low,K*TileHeight), B=FMath::Min(High,(K+1)*TileHeight);
            if(B<=A)continue;
            if(auto* Range=Strips.Find(K)) {Range->X=FMath::Min(Range->X,A);Range->Y=FMath::Max(Range->Y,B);}
            else Strips.Add(K,FVector2D(A,B));
        }
    }
    if(Strips.IsEmpty() || Strips.Num()>128)
    {
        UE_LOG(LogTemp,Warning,TEXT("PWG_PIXEL2D: no valid opening strips or safety limit, chunk %d"),Index);
        return;
    }
    TArray<int32> Keys;Strips.GetKeys(Keys);Keys.Sort();
    for(int32 K:Keys)
    {
        const FVector2D Range=Strips.FindChecked(K);
        for(int32 P=0;P<4;++P)
        {
            const double Y=GetPixelBackgroundStart()+GetPixelBackgroundSpan()*P/3.0;
            const int32 Section=FMath::Max(Mesh->GetNumSections(),SolidCutawaySectionBase+4);
            FPWGv2MeshBuffers Card;
            Card.Quad(FVector(Start,Y,Range.X),FVector(End,Y,Range.X),FVector(End,Y,Range.Y),FVector(Start,Y,Range.Y),FLinearColor::White);
            Card.UV={FVector2D(0,0),FVector2D(1,0),FVector2D(1,1),FVector2D(0,1)};
            Card.Submit(Mesh,Section);
            auto* MID=UMaterialInstanceDynamic::Create(PixelBackgroundMaterial,this);
            Mesh->SetMaterial(Section,MID);
            Mesh->SetMeshSectionVisible(Section,false);
            FPWGPixelTile Tile;Tile.Section=Section;Tile.Plan=P;
            Tile.Bounds=FBox(FVector(Start,Y,Range.X),FVector(End,Y,Range.Y));
            Batch.PixelTiles.Add(Tile);Batch.PixelPlans.Add(Section,P);
        }
    }
    Batch.bPixelReady=RepaintPixelBackground(Index,Mesh);
    UpdatePixelMaterialParameters(Index,Mesh);
    UE_LOG(LogTemp,Display,TEXT("PWG_PIXEL2D_CHUNK index=%d ready=%d strips=%d triangles=%d build_ms=%.2f"),
        Index,Batch.bPixelReady?1:0,Strips.Num(),Batch.PixelTiles.Num()*2,(FPlatformTime::Seconds()-Beg)*1000.0);
}

bool APWGv2Generator::RepaintPixelBackground(int32 Index,UProceduralMeshComponent* Mesh)
{
    auto* Batch=DecorationChunks.Find(Index);
    if(!WorldData || !Batch || Batch->PixelTiles.IsEmpty() || !Mesh) return false;
    const auto& Rect=Batch->PixelTiles[0].Bounds;
    const double Cell=PixelCellCm;
    // One texel of guard data on every edge; no neighbour-chunk dependency.
    const int32 GX=FMath::FloorToInt(Rect.Min.X/Cell)-1;
    const int32 W=FMath::CeilToInt(Rect.Max.X/Cell)-GX+1;
    if(W<1 || W>4096) return false;
    PWGPixelField::FSettings S;
    S.Scale=PixelStructureScale;S.Density=PixelDensity;S.Roughness=PixelRoughness;
    S.Erosion=PixelErosion;S.Detail=PixelDetail;
    const uint32 Seed=uint32(WorldData->Seed);
    TArray<PWGPixelField::FColumn> Columns[4];
    for(int32 P=0;P<4;++P) Columns[P].Reserve(W);
    for(int32 I=0;I<W;++I)
    {
        const double X=(GX+I+.5)*Cell;
        // At finite world edges, sample topology at the last valid coordinate.
        const float ContextX=float(FMath::Clamp(X,double(WorldData->MinX)+.01,double(WorldData->MaxX)-.01));
        PWGPixelField::FContext C;
        int32 A=0,B=0;float Alpha=0.f;BiomeMix(ContextX,A,B,Alpha);
        C.Biome=FMath::Lerp(float(A),float(B),Alpha);
        for(int32 L=1;L<=4;++L)
        {
            C.Layers[L-1]=FVector2D(LayerFloor(L,ContextX),LayerCeiling(L,ContextX));
            if(L!=1 || RegionAt(ContextX).Definition.EntranceStyle!=EPWGv2EntranceStyle::None)
                C.Openings.Add(C.Layers[L-1]);
        }
        for(const auto& P:WorldData->Portals)
        {
            if(P.UpperLayer<0 || P.LowerLayer>4 || P.UpperLayer>=P.LowerLayer || FMath::Abs(ContextX-P.X)>=P.Width*.5f) continue;
            const double Low=C.Layers[P.LowerLayer-1].X;
            const double High=P.UpperLayer==0?double(Height(ContextX)):C.Layers[P.UpperLayer-1].Y;
            if(High>Low) C.Openings.Add(FVector2D(Low,High));
        }
        for(int32 P=0;P<4;++P) Columns[P].Add(PWGPixelField::Compose(C,X,P,Seed,S));
    }
    // Stage textures first. Do not mix old and new mask grids on a failed allocation.
    TArray<UTexture2D*> NewTextures;NewTextures.Reserve(Batch->PixelTiles.Num());
    TArray<FLinearColor> Scales,Offsets;
    for(const auto& Tile:Batch->PixelTiles)
    {
        const auto& Box=Tile.Bounds;
        const int32 GZ=FMath::FloorToInt(Box.Min.Z/Cell)-1;
        const int32 H=FMath::CeilToInt(Box.Max.Z/Cell)-GZ+1;
        if(H<1 || H>4096 || int64(W)*H>4194304) return false;
        TArray<FColor> Data;Data.SetNumUninitialized(W*H);
        for(int32 J=0;J<H;++J)
        {
            const double Z=(GZ+J+.5)*Cell;
            for(int32 I=0;I<W;++I)
                Data[J*W+I]=PWGPixelField::Paint(Columns[Tile.Plan][I],(GX+I+.5)*Cell,Z,Tile.Plan,Seed,S);
        }
        UTexture2D* Tex=UTexture2D::CreateTransient(W,H,PF_B8G8R8A8);
        if(!Tex || !Tex->GetPlatformData() || Tex->GetPlatformData()->Mips.Num()==0) return false;
        Tex->SRGB=false;Tex->NeverStream=true;Tex->Filter=TF_Nearest;
        Tex->AddressX=TA_Clamp;Tex->AddressY=TA_Clamp;
        auto& Mip=Tex->GetPlatformData()->Mips[0];
        void* Dest=Mip.BulkData.Lock(LOCK_READ_WRITE);
        FMemory::Memcpy(Dest,Data.GetData(),Data.Num()*sizeof(FColor));
        Mip.BulkData.Unlock();Tex->UpdateResource();
        NewTextures.Add(Tex);
        Scales.Add(FLinearColor(float((Box.Max.X-Box.Min.X)/(W*Cell)),float((Box.Max.Z-Box.Min.Z)/(H*Cell)),0,1));
        Offsets.Add(FLinearColor(float((Box.Min.X-GX*Cell)/(W*Cell)),float((Box.Min.Z-GZ*Cell)/(H*Cell)),0,1));
    }
    for(int32 I=0;I<Batch->PixelTiles.Num();++I)
    {
        auto* MID=Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(Batch->PixelTiles[I].Section));
        if(!MID)return false;
        MID->SetTextureParameterValue(TEXT("PixelData"),NewTextures[I]);
        MID->SetVectorParameterValue(TEXT("DataScale"),Scales[I]);
        MID->SetVectorParameterValue(TEXT("DataOffset"),Offsets[I]);
    }
    return true;
}

void APWGv2Generator::UpdatePixelMaterialParameters(int32 Index,UProceduralMeshComponent* Mesh)
{
    const auto* Batch=DecorationChunks.Find(Index);
    if(!Batch || !Mesh)return;
    for(const auto& Tile:Batch->PixelTiles)
        if(auto* MID=Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(Tile.Section)))
        {
            MID->SetScalarParameterValue(TEXT("PaletteOverride"),float(PixelPalette));
            MID->SetScalarParameterValue(TEXT("Plan"),float(Tile.Plan));
            MID->SetScalarParameterValue(TEXT("GradeStrength"),PixelDepthGrade);
        }
}

void APWGv2Generator::SetPixelBackgroundPresentation(const FPWGVisualProfile& P,bool bEnabled)
{
    bEnabled=bEnabled && EnsurePixelBackgroundMaterial();
    const bool bRepaint=PixelCellCm!=P.PixelCellCm || PixelStructureScale!=P.PixelStructureScale ||
        PixelDensity!=P.PixelDensity || PixelRoughness!=P.PixelRoughness || PixelErosion!=P.PixelErosion || PixelDetail!=P.PixelDetail;
    const float Grade=P.bEnableDepthGrading?P.PixelDepthGrade:0.f;
    const bool bPalette=PixelPalette!=P.PixelPalette || PixelDepthGrade!=Grade;
    const bool bVisibility=bPixelBackgroundActive!=bEnabled || PixelPlanCount!=P.PixelPlanCount;
    if(!bRepaint && !bPalette && !bVisibility)return;
    bPixelBackgroundActive=bEnabled;PixelPlanCount=P.PixelPlanCount;PixelPalette=P.PixelPalette;PixelDepthGrade=Grade;
    PixelCellCm=P.PixelCellCm;PixelStructureScale=P.PixelStructureScale;PixelDensity=P.PixelDensity;
    PixelRoughness=P.PixelRoughness;PixelErosion=P.PixelErosion;PixelDetail=P.PixelDetail;
    for(const auto& Pair:UndergroundChunks)
    {
        auto* Mesh=Pair.Value.Get();auto* Batch=DecorationChunks.Find(Pair.Key);
        if(!Mesh || !Batch)continue;
        if(bRepaint && !Batch->PixelTiles.IsEmpty())
        {
            Batch->bPixelReady=RepaintPixelBackground(Pair.Key,Mesh);
            if(!Batch->bPixelReady)UE_LOG(LogTemp,Error,TEXT("PWG_PIXEL2D: repaint failed chunk %d; Stage 1 retained"),Pair.Key);
        }
        if(bPalette || bRepaint)UpdatePixelMaterialParameters(Pair.Key,Mesh);
        if(bVisibility || bRepaint)ApplyChunkLayerVisibility(Pair.Key);
    }
}
