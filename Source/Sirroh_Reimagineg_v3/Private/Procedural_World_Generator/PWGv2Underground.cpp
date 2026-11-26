#include "Algo/BinarySearch.h"
#include "Procedural_World_Generator/PWGv2Generator.h"
#include "PWG/PWGEnvironmentV2.h"
#include "Sword/SwordWorldSubsystem.h"
#include "Sword/ProceduralSword.h"
#include "PWGv2Mesh.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "POI/POIArtPool.h"

float APWGv2Generator::LayerFloor(int32 Layer,float X) const
{
    if (Layer<=0 || !Settings.bEnableUnderground || WorldData->Layers.Num()!=4) return Height(X);
    const FPWGv2RoomChain* NearbyChain=nullptr;
    float ChainBlend=0;
    for (const auto& Chain:WorldData->RoomChains) if (Chain.Layer==Layer)
    {
        const float Distance=FMath::Max(Chain.StartX-X,X-Chain.EndX);
        if (Distance<=100) return Chain.FloorZ;
        if (Distance<800) { NearbyChain=&Chain; ChainBlend=1-FMath::SmoothStep(100.f,800.f,Distance); break; }
    }
    auto ChainFloor=[&](float Base)
    {
        const float Floor=NearbyChain?FMath::Lerp(Base,NearbyChain->FloorZ,ChainBlend):Base;
        // A distant shaft endpoint can pull the interpolated first layer above the
        // local terrain. Keep its 240 cm minimum headroom plus 100 cm of roof cover.
        // Exact authored room floors and shaft endpoints return before this blend.
        return Layer==1?FMath::Min(Floor,Height(X)-340.f):Floor;
    };
    const auto& L=WorldData->Layers[FMath::Clamp(Layer-1,0,3)];
    float Z=Settings.SeaLevel-L.FloorBelowSeaMetres*100;
    if (Layer==1) Z=FMath::Min(Z,Height(X)-300);
    if (!L.bRectangular) Z+=L.FloorVariationMetres*100*FMath::PerlinNoise1D(X/FMath::Max(2000.f,L.RoomLengthMetres*200)+WorldData->Seed%4096+Layer*171);
    if (!L.bRectangular)
    {
        const float Wavelength=FMath::Max(2400.f,L.RoomLengthMetres*150.f);
        Z+=180.f*FMath::Sin(X/Wavelength*2*PI+Layer*1.7f+WorldData->Seed%71);
    }
    // Nearest authored endpoint controls the floor across the entire shaft mouth.
    float Closest=MAX_flt,Target=Z,Blend=0;
    const FPWGv2Portal* Left=nullptr;
    const FPWGv2Portal* Right=nullptr;
    for (const auto& P:WorldData->Portals) if (P.bPOIFirst && (P.UpperLayer==Layer || P.LowerLayer==Layer))
    {
        const float Distance=FMath::Abs(X-P.X);
        if (Distance<=P.Width*.5f+100.f) return P.UpperLayer==Layer?P.TopZ:P.BottomZ;
        if (P.X<X && (!Left || P.X>Left->X)) Left=&P;
        if (P.X>X && (!Right || P.X<Right->X)) Right=&P;
        if (Distance<Closest)
        {
            Closest=Distance; Target=P.UpperLayer==Layer?P.TopZ:P.BottomZ;
            const float Reach=FMath::Max(1100.f,FMath::Abs(Target-Z)*2.f);
            Blend=1-FMath::SmoothStep(P.Width*.5f+100.f,P.Width*.5f+100.f+Reach,Distance);
        }
    }
    if (Left && Right)
    {
        const float A=Left->UpperLayer==Layer?Left->TopZ:Left->BottomZ;
        const float B=Right->UpperLayer==Layer?Right->TopZ:Right->BottomZ;
        return ChainFloor(FMath::Lerp(A,B,FMath::SmoothStep(Left->X+Left->Width*.5f+100.f,Right->X-Right->Width*.5f-100.f,X)));
    }
    Z=FMath::Lerp(Z,Target,Blend);
    return ChainFloor(Z);
}
float APWGv2Generator::LayerCeiling(int32 Layer,float X) const
{
    const auto& L=WorldData->Layers[Layer-1];
    float Extra=0;
    if (L.bRectangular)
    {
        FRandomStream R(HashCombineFast(WorldData->Seed,FMath::FloorToInt(X/(L.RoomLengthMetres*100)))+Layer);
        Extra=R.RandRange(0,2)*100.f;
    }
    else
    {
        // Narrow connecting tunnels open into chambers around each room centre.
        const float Room=FMath::Max(600.f,L.RoomLengthMetres*100);
        const float Phase=X/Room-FMath::FloorToFloat(X/Room);
        const float Chamber=1-FMath::SmoothStep(0.2f,0.45f,FMath::Abs(Phase-0.5f));
        Extra=FMath::Lerp(240.f,L.ClearHeightMetres*100,Chamber)-L.ClearHeightMetres*100;
    }
    const float Base=FMath::Max(LayerFloor(Layer,X)+240,FMath::Min(LayerFloor(Layer,X)+L.ClearHeightMetres*100+Extra,LayerFloor(Layer-1,X)-100));
    for (const auto& Chain:WorldData->RoomChains) if (Chain.Layer==Layer)
    {
        const float Distance=FMath::Max(Chain.StartX-X,X-Chain.EndX);
        if (Distance<=100) return Chain.FloorZ+Chain.ClearHeight;
        if (Distance<800) return FMath::Lerp(Chain.FloorZ+Chain.ClearHeight,Base,FMath::SmoothStep(100.f,800.f,Distance));
    }
    return Base;
}
bool APWGv2Generator::IsOpening(int32 Layer,float X,float Y) const
{
    if (!Settings.bEnableUnderground || Y < -Settings.LaneSpacing-50 || Y>50) return false;
    for (const auto& P:WorldData->Portals)
        if (Layer>=P.UpperLayer && Layer<P.LowerLayer && FMath::Abs(X-P.X)<P.Width/2-0.1f) return true;
    return false;
}
int32 APWGv2Generator::LayerAt(const FVector& Location) const
{
    if (!WorldData || !Settings.bEnableUnderground || WorldData->Layers.Num()!=4) return 0;
    const FVector P=GetActorTransform().InverseTransformPosition(Location);
    if (P.Z>=Height(P.X)-100) return 0;
    for (int32 L=1;L<=4;++L) if (P.Z>=LayerFloor(L,P.X)-50) return L;
    return 4;
}
float APWGv2Generator::GetCameraGroundHeight(const FVector& Location) const
{
    return LayerAt(Location)>0?Location.Z-90:GetSurfaceHeightAt(Location.X,Location.Y);
}
void APWGv2Generator::UpdateCutaway(const FVector& Player)
{
    bUndergroundView=LayerAt(Player)>0;
    if (ForegroundMaterial)
    {
        ForegroundMaterial->SetScalarParameterValue(TEXT("Opacity"),bUndergroundView?FMath::Clamp(Settings.UndergroundForegroundOpacity,0.f,1.f):1);
        ForegroundMaterial->SetScalarParameterValue(TEXT("CutawayRadius"),FMath::Max(1800.f,Settings.CutawayRadius));
        ForegroundMaterial->SetVectorParameterValue(TEXT("CutawayCenter"),FLinearColor(Player.X,Player.Y,Player.Z));
    }
    for (auto& Pair:AtlasMaterials)
    {
        Pair.Value->SetScalarParameterValue(TEXT("Opacity"),bUndergroundView?Settings.UndergroundForegroundOpacity:1.f);
        Pair.Value->SetScalarParameterValue(TEXT("CutawayRadius"),FMath::Max(1800.f,Settings.CutawayRadius));
        Pair.Value->SetVectorParameterValue(TEXT("CutawayCenter"),FLinearColor(Player.X,Player.Y,Player.Z));
    }
    for (auto& Pair:Chunks)
        if (Pair.Value->IsMeshSectionVisible(1) == bUndergroundView)
            Pair.Value->SetMeshSectionVisible(1,!bUndergroundView);
    if (EnvironmentV2)
    {
        const auto R=EcologyAt(GetActorTransform().InverseTransformPosition(Player).X);
        EnvironmentV2->SetRegionWeather(R.CloudMultiplier,R.WindStrength);
        EnvironmentV2->SetUndergroundView(bUndergroundView);
    }
}

void APWGv2Generator::BuildUnderground(int32 Index,float Start,float End,bool AsyncCooking,const TArray<float>& SurfaceSamples)
{
    if (WorldData->Layers.Num()!=4) return;
    Start=FMath::Max(Start,WorldData->Regions[0].StartX); End=FMath::Min(End,WorldData->Regions.Last().EndX);
    if (Start>=End) return;
    auto* Mesh=NewObject<UProceduralMeshComponent>(this);
    Mesh->SetCanEverAffectNavigation(false);
    Mesh->SetupAttachment(GetRootComponent()); Mesh->SetMobility(EComponentMobility::Static); Mesh->RegisterComponent(); Mesh->SetCastShadow(false);
    Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); Mesh->SetCollisionObjectType(ECC_WorldStatic);
    Mesh->SetCollisionResponseToAllChannels(ECR_Block); Mesh->SetCollisionResponseToChannel(ECC_GameTraceChannel1,ECR_Ignore);
    Mesh->ComponentTags.Add(TEXT("CombatTerrain")); Mesh->bUseComplexAsSimpleCollision=false; Mesh->bUseAsyncCooking=AsyncCooking;
    TArray<TArray<FVector>> Hulls;
    auto Wall=[&](float X0,float X1,float Y0,float Y1,float Z0,float Z1)
    {
        Hulls.Add({FVector(X0,Y0,Z0),FVector(X1,Y0,Z0),FVector(X1,Y1,Z0),FVector(X0,Y1,Z0),FVector(X0,Y0,Z1),FVector(X1,Y0,Z1),FVector(X1,Y1,Z1),FVector(X0,Y1,Z1)});
    };
    auto Slab=[&](FPWGv2MeshBuffers& Buffer,float X0,float X1,float Y0,float Y1,float Z0,float Z1,float Thickness,FLinearColor Color,bool Closed=false)
    {
        FVector A(X0,Y0,Z0),B(X1,Y0,Z1),C(X1,Y1,Z1),D(X0,Y1,Z0);
        Buffer.Quad(A,B,C,D,Color);
        Buffer.Quad(A-FVector(0,0,Thickness),B-FVector(0,0,Thickness),B,A,Color*0.65f);
        if (Closed)
        {
            const FVector Down(0,0,Thickness);
            Buffer.Quad(D-Down,C-Down,B-Down,A-Down,Color*0.65f);
            Buffer.Quad(A-Down,A,D,D-Down,Color*0.8f);
            Buffer.Quad(B-Down,C-Down,C,B,Color*0.8f);
            Buffer.Quad(D,C,C-Down,D-Down,Color*0.65f);
        }
        Hulls.Add({A,B,C,D,A-FVector(0,0,Thickness),B-FVector(0,0,Thickness),C-FVector(0,0,Thickness),D-FVector(0,0,Thickness)});
    };
    // Pick a depth row that is present in the generated top mesh, behind the cave shell.
    const float EarthY = FMath::Min(Settings.BackgroundDepth,Settings.LaneSpacing+450.f);
    const auto SurfaceRowHeight = [&](float X)
    {
        const int32 Right = FMath::Clamp(Algo::LowerBound(SurfaceSamples,X),1,SurfaceSamples.Num()-1);
        const float LeftX=SurfaceSamples[Right-1], RightX=SurfaceSamples[Right];
        return FMath::Lerp(Height(LeftX,EarthY),Height(RightX,EarthY),(X-LeftX)/(RightX-LeftX));
    };
    TArray<float> Samples={Start,End};
    for (float X : SurfaceSamples) if (X>Start && X<End) Samples.AddUnique(X);
    for (float X=FMath::CeilToFloat(Start/100)*100;X<End;X+=100) Samples.AddUnique(X);
    for (const auto& P:WorldData->Portals)
        for (float Edge:{P.X-P.Width/2,P.X+P.Width/2}) if (Edge>Start && Edge<End) Samples.AddUnique(Edge);
    for (const auto& Region:WorldData->Regions)
        for (float Edge:{Region.StartX,Region.EndX}) if (Edge>Start && Edge<End) Samples.AddUnique(Edge);
    TMap<int32,FPWGv2MeshBuffers> POIWallBuffers;
    TMap<int32,TObjectPtr<UMaterialInstanceDynamic>> POIWallMaterials;
    for (int32 P=0;P<WorldData->AuthoredPOIs.Num();++P)
    {
        if (!WorldData->AuthoredPOIPositions.IsValidIndex(P)) continue;
        const auto& Row=WorldData->AuthoredPOIs[P]; const FVector Pos=WorldData->AuthoredPOIPositions[P];
        if (Row.UpperLayer<=0 || Row.ArtTheme.IsNull() || Pos.X+Row.Width*.5f<Start || Pos.X-Row.Width*.5f>End) continue;
        auto* Theme=Row.ArtTheme.LoadSynchronous();
        if (!Theme || !Theme->Backgrounds || Theme->Backgrounds->Images.IsEmpty()) continue;
        auto* Base=Theme->Material.LoadSynchronous(); if (!Base) continue;
        FRandomStream Random(HashCombineFast(WorldData->Seed,P+0xB671));
        const auto& Pair=Theme->Backgrounds->Images[Random.RandRange(0,Theme->Backgrounds->Images.Num()-1)];
        auto* Texture=Pair.Side.LoadSynchronous(); if (!Texture) continue;
        auto* Material=UMaterialInstanceDynamic::Create(Base,this);
        Material->SetTextureParameterValue(TEXT("ImageTexture"),Texture);
        Material->SetVectorParameterValue(TEXT("Tint"),FLinearColor::White);
        Material->SetVectorParameterValue(TEXT("UVRect"),FLinearColor(Pair.SideUV.X,Pair.SideUV.Y,Pair.SideUV.Z,Pair.SideUV.W));
        POIWallMaterials.Add(P,Material);
        for (float Edge:{Pos.X-Row.Width*.5f,Pos.X+Row.Width*.5f}) if (Edge>Start && Edge<End) Samples.AddUnique(Edge);
    }
    Samples.Sort();
    for (int32 Layer=1;Layer<=4;++Layer)
    {
        FPWGv2MeshBuffers Buffer, Backdrop, NearShell;
        for (int32 I=0;I<Samples.Num()-1;++I)
        {
            const float X0=Samples[I],X1=Samples[I+1],Mid=(X0+X1)/2;
            int32 WallPOI=INDEX_NONE;
            for (const auto& Pair:POIWallMaterials)
                if (WorldData->AuthoredPOIs[Pair.Key].UpperLayer==Layer && FMath::Abs(Mid-WorldData->AuthoredPOIPositions[Pair.Key].X)<=WorldData->AuthoredPOIs[Pair.Key].Width*.5f)
                { WallPOI=Pair.Key; break; }
            if (Layer==1 && RegionAt(Mid).Definition.EntranceStyle==EPWGv2EntranceStyle::None) continue;
            const float Z0=LayerFloor(Layer,X0),Z1=LayerFloor(Layer,X1);
            for (int32 Lane=0;Lane<2;++Lane)
            {
                const float Y=(Lane-1)*Settings.LaneSpacing;
                const float Y0=Lane==0?-Settings.LaneSpacing-50:-50, Y1=Lane==0?-50:50;
                if (!IsOpening(Layer,Mid,Y)) Slab(Buffer,X0,X1,Y0,Y1,Z0,Z1,45,FLinearColor(0.65,0.65,0.7));
                bool CeilingHole=false;
                for (const auto& P:WorldData->Portals)
                    if (Layer>P.UpperLayer && Layer<=P.LowerLayer && FMath::Abs(Mid-P.X)<P.Width/2) { CeilingHole=true; break; }
                if (!CeilingHole)
                {
                    const float C0=LayerCeiling(Layer,X0),C1=LayerCeiling(Layer,X1);
                    Slab(Buffer,X0,X1,Y0,Y1,C0+45,C1+45,45,FLinearColor(0.4,0.4,0.45));
                    Buffer.Quad(FVector(X0,Y1,C0),FVector(X1,Y1,C1),FVector(X1,Y0,C1),FVector(X0,Y0,C0),FLinearColor(0.4,0.4,0.45));
                }
            }

            // Cap the finite island, and seal the first layer at desert boundaries.
            for (float Edge:{X0,X1})
            {
                const bool EndOfIsland=FMath::IsNearlyEqual(Edge,WorldData->Regions[0].StartX) || FMath::IsNearlyEqual(Edge,WorldData->Regions.Last().EndX);
                const bool DesertEdge=Layer==1 && (RegionAt(Edge-1).Definition.EntranceStyle==EPWGv2EntranceStyle::None || RegionAt(Edge+1).Definition.EntranceStyle==EPWGv2EntranceStyle::None);
                if (EndOfIsland || DesertEdge) Wall(Edge-10,Edge+10,-Settings.LaneSpacing-50,75,LayerFloor(Layer,Edge)-100,LayerCeiling(Layer,Mid)+45);
            }
            // Solid earth beyond the carved volume also closes the space between strata.
            // This is fixed world geometry, not a camera-following backdrop.
            const int32 EarthTopLayer=Layer==2 && RegionAt(Mid).Definition.EntranceStyle==EPWGv2EntranceStyle::None?0:Layer-1;
            // Adjacent earth bands share this floor edge in the same depth plane.
            const auto EarthTop = [&](float X)
            {
                return EarthTopLayer == 0 ? SurfaceRowHeight(X) : LayerFloor(EarthTopLayer,X);
            };
            Backdrop.Quad(FVector(X0,EarthY,Z0),FVector(X1,EarthY,Z1),
                FVector(X1,EarthY,EarthTop(X1)),FVector(X0,EarthY,EarthTop(X0)),FLinearColor(0.55f,0.55f,0.55f));
            // Actual swept tunnel cross-sections, attached to the floor and roof.
            // The camera-side shell is cut away locally; the far shell remains opaque.
            auto Ring=[&](float X,float T,bool Front)
            {
                const float Floor=LayerFloor(Layer,X),Roof=LayerCeiling(Layer,X);
                const float Noise=FMath::PerlinNoise1D(X/700.f+Layer*193+WorldData->Seed%4096);
                const float Bulge=WorldData->Layers[Layer-1].bRectangular?0.f:FMath::Sin(PI*T)*(170.f+70.f*Noise);
                return FVector(X,Front?-Settings.LaneSpacing-75-Bulge:75+Bulge,FMath::Lerp(Floor,Roof,T));
            };
            // Stable, cheap outer limits for the two playable lanes. Decorative depth
            // beyond them does not multiply convex cooking cost by the number of ring faces.
            Wall(X0,X1,75,100,FMath::Min(Z0,Z1)-45,FMath::Max(LayerCeiling(Layer,X0),LayerCeiling(Layer,X1))+45);
            Wall(X0,X1,-Settings.LaneSpacing-100,-Settings.LaneSpacing-75,FMath::Min(Z0,Z1)-45,FMath::Max(LayerCeiling(Layer,X0),LayerCeiling(Layer,X1))+45);
            // Join the metrewide playable banks to the shell without cracks.
            Backdrop.Quad(FVector(X0,50,Z0),FVector(X1,50,Z1),Ring(X1,0,false),Ring(X0,0,false),FLinearColor::White);
            NearShell.Quad(Ring(X0,0,true),Ring(X1,0,true),FVector(X1,-Settings.LaneSpacing-50,Z1),FVector(X0,-Settings.LaneSpacing-50,Z0),FLinearColor::White);
            Backdrop.Quad(Ring(X0,1,false),Ring(X1,1,false),FVector(X1,50,LayerCeiling(Layer,X1)),FVector(X0,50,LayerCeiling(Layer,X0)),FLinearColor::White);
            NearShell.Quad(FVector(X0,-Settings.LaneSpacing-50,LayerCeiling(Layer,X0)),FVector(X1,-Settings.LaneSpacing-50,LayerCeiling(Layer,X1)),Ring(X1,1,true),Ring(X0,1,true),FLinearColor::White);
            for (int32 Segment=0;Segment<6;++Segment)
            {
                const float T0=Segment/6.f,T1=(Segment+1)/6.f;
                for (bool FrontSide:{false,true})
                {
                    const FVector A=Ring(X0,T0,FrontSide),B=Ring(X1,T0,FrontSide),C=Ring(X1,T1,FrontSide),D=Ring(X0,T1,FrontSide);
                    auto& Shell=FrontSide?NearShell:(WallPOI==INDEX_NONE?Backdrop:POIWallBuffers.FindOrAdd(WallPOI));
                    // Baked ambient depth for the unlit sprite style: the roof is darker.
                    const float Ambient=0.68f+0.22f*FMath::Cos(PI*(T0+T1)*0.5f);
                    const FLinearColor Shade(Ambient,Ambient,Ambient);
                    if (FrontSide) Shell.Quad(D,C,B,A,Shade);
                    else Shell.Quad(A,B,C,D,Shade);
                    const int32 UVStart=Shell.UV.Num()-4;
                    const FVector Points[4]={FrontSide?D:A,FrontSide?C:B,FrontSide?B:C,FrontSide?A:D};
                    for (int32 U=0;U<4;++U)
                    {
                        if (!FrontSide && WallPOI!=INDEX_NONE)
                        {
                            const auto& Row=WorldData->AuthoredPOIs[WallPOI]; const auto Pos=WorldData->AuthoredPOIPositions[WallPOI];
                            Shell.UV[UVStart+U]=FVector2D((Points[U].X-Pos.X)/Row.Width+.5f,1-(Points[U].Z-Pos.Z)/Row.SectionHeight);
                        }
                        else Shell.UV[UVStart+U]=FVector2D(Points[U].X/100,-Points[U].Z/100);
                    }

                }
            }
        }
        if (const auto* Row=TerrainTexturesTable?TerrainTexturesTable->FindRow<FPWGv2TerrainTextureRow>(WorldData->Layers[Layer-1].WallTexture,TEXT("Wall scale"),false):nullptr)
        {
            const float Scale=100.f/FMath::Max(25.f,Row->TilingWorldSize);
            for (auto& UV:Backdrop.UV) UV*=Scale;
            for (auto& UV:NearShell.UV) UV*=Scale;
        }
        NearShell.Submit(Mesh,16+Layer); Mesh->SetMaterial(16+Layer,WallMaterial(Layer,true));
        Backdrop.Submit(Mesh,Layer+4); Mesh->SetMaterial(Layer+4,WallMaterial(Layer));
        Buffer.Submit(Mesh,Layer);
        const auto& Atlas=WorldData->Layers[Layer-1].Atlas;
        Mesh->SetMaterial(Layer,Atlas.Texture.IsNull()?TerrainMaterialAt((Start+End)/2):AtlasMaterial(Atlas,FLinearColor(0.5,0.5,0.6),true));
    }
    TMap<int32,FPWGv2MeshBuffers> ShaftWalls, ShaftFronts;
    for (const auto& Portal:WorldData->Portals)
    {
        if (Portal.X+Portal.Width/2<Start || Portal.X-Portal.Width/2>End) continue;
        const float Upper=LayerFloor(Portal.UpperLayer,Portal.X),Lower=LayerFloor(Portal.LowerLayer,Portal.X);
        // A hole reveals the destination's opaque wall, never the sky behind the island.
        const float WallLeft=FMath::Max(Start,Portal.X-Portal.Width/2),WallRight=FMath::Min(End,Portal.X+Portal.Width/2);
        if (WallLeft >= WallRight) continue;
        auto& Shaft=ShaftWalls.FindOrAdd(Portal.LowerLayer);
        const float UpperLeft=LayerFloor(Portal.UpperLayer,WallLeft),UpperRight=LayerFloor(Portal.UpperLayer,WallRight);
        const float ShaftTop=FMath::Max3(Upper,UpperLeft,UpperRight);
        TArray<float> Bands={Lower,ShaftTop};
        for (float Z=Lower+100; Z<ShaftTop; Z+=100) Bands.Add(Z);
        for (float Edge : {WallLeft,WallRight})
            for (int32 L=FMath::Max(1,Portal.UpperLayer);L<=Portal.LowerLayer;++L)
                for (float Boundary : {LayerFloor(L,Edge),LayerCeiling(L,Edge)})
                    if (Boundary>Lower && Boundary<ShaftTop) Bands.AddUnique(Boundary);
        Bands.Sort();
        for (int32 I=1; I<Bands.Num(); ++I)
        {
            const float Z=Bands[I-1], Top=Bands[I];
            const float Back0=100+35*FMath::Sin(Z/240+Portal.X/600),Back1=100+35*FMath::Sin(Top/240+Portal.X/600);
            Shaft.Quad(FVector(WallLeft,Back0,FMath::Min(Z,UpperLeft)),FVector(WallRight,Back0,FMath::Min(Z,UpperRight)),
                FVector(WallRight,Back1,FMath::Min(Top,UpperRight)),FVector(WallLeft,Back1,FMath::Min(Top,UpperLeft)),FLinearColor::White);
            auto& Front=ShaftFronts.FindOrAdd(Portal.LowerLayer);
            Front.Quad(FVector(WallLeft,-75,FMath::Min(Z,UpperLeft)),FVector(WallRight,-75,FMath::Min(Z,UpperRight)),
                FVector(WallRight,-75,FMath::Min(Top,UpperRight)),FVector(WallLeft,-75,FMath::Min(Top,UpperLeft)),FLinearColor::White);
            bool InRoom=false;
            for (int32 L=FMath::Max(1,Portal.UpperLayer);L<=Portal.LowerLayer;++L)
                if (Top>LayerFloor(L,Portal.X) && Z<LayerCeiling(L,Portal.X)) { InRoom=true; break; }
            if (!InRoom)
            {
                // No lateral escape into empty space halfway down a shaft.
                Wall(WallLeft,WallRight,-Settings.LaneSpacing-100,-Settings.LaneSpacing-75,Z,Top);
                Wall(WallLeft,WallRight,75,100,Z,Top);
            }
            for (int32 Side:{-1,1})
            {
                const float Edge=Portal.X+Side*Portal.Width/2;
                if (Edge<WallLeft || Edge>WallRight) continue;
                bool Exit=false;
                for (int32 L=FMath::Max(1,Portal.UpperLayer);L<=Portal.LowerLayer;++L)
                    if (Top>LayerFloor(L,Edge) && Z<LayerCeiling(L,Edge)) { Exit=true; break; }
                if (Exit) continue; // Connections to rooms stay traversable.
                const float EdgeTop=FMath::Min(Top,LayerFloor(Portal.UpperLayer,Edge));
                if (Z>=EdgeTop) continue;
                const FVector A(Edge,-75,Z),B(Edge,Back0,Z),C(Edge,Back1,EdgeTop),D(Edge,-75,EdgeTop);
                if (Side<0) Shaft.Quad(A,B,C,D,FLinearColor::White);
                else Shaft.Quad(D,C,B,A,FLinearColor::White);
                const FVector Out(Side*35.f,0,0);
                Hulls.Add({A,B,C,D,A+Out,B+Out,C+Out,D+Out});
            }
        }
        if (Portal.bKitShaft)
        {
            // One owner chunk for the entire shaft, irrespective of how many chunks its walls cross.
            if (Portal.X>=Start && Portal.X<End)
            {
                FActorSpawnParameters Params; Params.Owner=this;
                auto* KitShaft=GetWorld()->SpawnActor<APOIShaft>(APOIShaft::StaticClass(),GetActorTransform().TransformPosition(FVector(Portal.X,0,Lower)),FRotator::ZeroRotator,Params);
                if (KitShaft)
                {
                    KitShaft->bIncrementalBuild=AsyncCooking;
                    KitShaft->bBuildBoundaryWalls=false; // PWG owns the lining and the exits at each layer.
                    KitShaft->Recipe=Portal.ShaftRecipe; KitShaft->Height=Upper-Lower;
                    KitShaft->BottomExitClearance=LayerCeiling(Portal.LowerLayer,Portal.X)-Lower;
                    KitShaft->SavedModules=Portal.ShaftModules;
                    KitShaft->PlannedSections=Portal.ShaftSectionPlan;
                    KitShaft->Seed=Portal.ShaftSeed;
                    KitShaft->bAssembleModules=!Portal.ShaftModules.IsEmpty();
                    const double ShaftStart=FPlatformTime::Seconds();
                    KitShaft->BuildShaft();
                    UE_LOG(LogTemp,Display,TEXT("PWG shaft build chunk=%d ms=%.1f levels=%d blocks=%d"),Index,(FPlatformTime::Seconds()-ShaftStart)*1000,KitShaft->GeneratedLevels.Num(),KitShaft->GeneratedBlocks.Num());
                    // Invalid or incomplete authoring must not remove the playable fallback.
                    if (!KitShaft->HasPendingBuild() && KitShaft->GeneratedBlocks.IsEmpty() && KitShaft->GeneratedNativeActors.IsEmpty() && KitShaft->GeneratedLevels.IsEmpty())
                    {
                        KitShaft->bAssembleModules=false;
                        KitShaft->PlannedSections.Reset();
                        KitShaft->BuildShaft();
                    }
                    DecorationChunks.FindOrAdd(Index).Actors.Add(KitShaft);
                }
            }
            continue;
        }
    }
    int32 POISection=25;
    for (auto& Pair:POIWallBuffers) { Pair.Value.Submit(Mesh,POISection); Mesh->SetMaterial(POISection++,POIWallMaterials[Pair.Key]); }
    for (auto& Entry:ShaftWalls) { Entry.Value.Submit(Mesh,12+Entry.Key); Mesh->SetMaterial(12+Entry.Key,AtlasMaterial(WorldData->Layers[Entry.Key-1].Atlas,FLinearColor::White,true)); }
    for (auto& Entry:ShaftFronts)
    {
        if (const auto* Row=TerrainTexturesTable?TerrainTexturesTable->FindRow<FPWGv2TerrainTextureRow>(WorldData->Layers[Entry.Key-1].WallTexture,TEXT("Shaft front"),false):nullptr)
            for (auto& UV:Entry.Value.UV) UV*=100.f/FMath::Max(25.f,Row->TilingWorldSize);
        Entry.Value.Submit(Mesh,20+Entry.Key); Mesh->SetMaterial(20+Entry.Key,WallMaterial(Entry.Key,true,true));
    }
    Mesh->SetCollisionConvexMeshes(Hulls);
    Mesh->SetCanEverAffectNavigation(Hulls.Num()>0);
    UndergroundChunks.Add(Index,Mesh);
    auto& SectionBounds = DecorationChunks.FindOrAdd(Index).UndergroundSectionBounds;
    for (int32 Section = 0; Section < Mesh->GetNumSections(); ++Section)
    {
        const FBox& Box = Mesh->GetProcMeshSection(Section)->SectionLocalBox;
        SectionBounds.Add(Box.IsValid ? Box.TransformBy(Mesh->GetComponentTransform()) : FBox(ForceInit));
    }
    if (AsyncCooking) PendingUndergroundCollision.Add(Index,Hulls);
    if (HasAuthority()) for (int32 Layer=1;Layer<=4;++Layer)
    {
        const auto& Definition=WorldData->Layers[Layer-1];
        if (Definition.SwordsPer100Metres<=0) continue;
        const float Room=FMath::Max(600.f,Definition.RoomLengthMetres*100);
        const float Spacing=FMath::Max(Room,10000.f/FMath::Clamp(Definition.SwordsPer100Metres,0.1f,5.f));
        for (int32 Cell=FMath::FloorToInt((Start-Room)/Spacing);Cell<=FMath::FloorToInt(End/Spacing);++Cell)
        {
            const float X=FMath::CeilToFloat(Cell*Spacing/Room)*Room+80;
            if (X<Start || X>=End || (Layer==1 && RegionAt(X).Definition.EntranceStyle==EPWGv2EntranceStyle::None)) continue;
            bool Clear=true;
            for (const auto& Portal:WorldData->Portals)
                if (Layer>=Portal.UpperLayer && Layer<=Portal.LowerLayer && FMath::Abs(X-Portal.X)<Portal.Width/2+250) Clear=false;
            if (!Clear || IsRoomChain(Layer,X,200)) continue;
            const FString Id=FString::Printf(TEXT("%s:sword:%d:%d"),*WorldData->WorldId.ToString(),Layer,Cell);
            if (WorldData->RemovedObjects.Contains(FName(*Id))) continue;
            const FVector Point=GetActorTransform().TransformPosition(FVector(X,-Settings.LaneSpacing,LayerFloor(Layer,X)));
            if (auto* Sword=GetWorld()->GetSubsystem<USwordWorldSubsystem>()->SpawnSword(Point,Point.Y,Id,0,HashCombineFast(WorldData->Seed,GetTypeHash(Id)),this,true))
            {
                Sword->Embed(Point,-FVector::UpVector,this);
                DecorationChunks.FindOrAdd(Index).Actors.Add(Sword);
            }
        }
    }
    // Cave decoration never introduces mandatory obstacles or enemies.
    if (RockMesh) for (int32 Layer=1;Layer<=4;++Layer)
    {
        auto* Rocks=NewObject<UHierarchicalInstancedStaticMeshComponent>(this);
        Rocks->SetupAttachment(GetRootComponent()); Rocks->SetStaticMesh(RockMesh); Rocks->SetMaterial(0,AtlasMaterial(WorldData->Layers[Layer-1].Atlas,FLinearColor::White,true));
        Rocks->SetCollisionEnabled(ECollisionEnabled::NoCollision); Rocks->SetCastShadow(false); Rocks->RegisterComponent();
        TArray<FTransform> Instances; FRandomStream Random(HashCombineFast(WorldData->Seed,Index)^0xD00D);
        {
            const int32 Count=FMath::Min(64,FMath::RoundToInt((End-Start)*FMath::Max(0.f,WorldData->Layers[Layer-1].RocksPer100Metres)/10000));
            for (int32 I=0;I<Count;++I)
            {
                const float X=Random.FRandRange(Start,End); if (IsOpening(Layer,X,0) || IsRoomChain(Layer,X,100)) continue;
                if (Layer==1 && RegionAt(X).Definition.EntranceStyle==EPWGv2EntranceStyle::None) continue;
                Instances.Add(FTransform(FRotator::ZeroRotator,FVector(X,40,LayerFloor(Layer,X)+10),FVector(0.7,0.4,0.4)));
            }
        }
        Rocks->AddInstances(Instances,false,false); DecorationChunks.FindOrAdd(Index).Components.Add(Rocks);
    }
}


