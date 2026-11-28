#pragma once
#include "CoreMinimal.h"

// Global integer-lattice data, not a projected 3D rock mesh. Every occupied
// column joins a floor, ceiling, rib or inter-stratum mass. No random rock objects.
namespace PWGPixelField
{
    struct FSettings
    {
        float Scale=1.f, Density=.55f, Roughness=.7f, Erosion=.65f, Detail=.7f;
    };
    struct FContext
    {
        TArray<FVector2D,TInlineAllocator<8>> Openings;
        FVector2D Layers[4];
        float Biome=0.f;
    };
    struct FColumn
    {
        TArray<FVector2D,TInlineAllocator<8>> Voids;
        FVector2D Layers[4];
    };
    inline uint32 Hash(int32 X,int32 Z,uint32 Seed)
    {
        uint32 H=uint32(X)*0x9E3779B1u ^ uint32(Z)*0x85EBCA77u ^ Seed;
        H^=H>>16; H*=0x7FEB352Du; H^=H>>15; H*=0x846CA68Bu; H^=H>>16;
        return H;
    }
    inline double Sample(int32 X,int32 Z,uint32 Seed)
    { return double(Hash(X,Z,Seed)&0x00FFFFFFu)/8388607.5-1.0; }
    inline double Noise(double X,double Z,uint32 Seed)
    {
        const int32 IX=FMath::FloorToInt(X), IZ=FMath::FloorToInt(Z);
        double A=X-IX,B=Z-IZ; A=A*A*(3-2*A); B=B*B*(3-2*B);
        return FMath::Lerp(FMath::Lerp(Sample(IX,IZ,Seed),Sample(IX+1,IZ,Seed),A),
            FMath::Lerp(Sample(IX,IZ+1,Seed),Sample(IX+1,IZ+1,Seed),A),B);
    }
    inline FColumn Compose(const FContext& C,double X,int32 Plan,uint32 Seed,const FSettings& S)
    {
        FColumn Out;
        for(int32 L=0;L<4;++L) Out.Layers[L]=C.Layers[L];
        const uint32 Key=Seed+uint32(Plan)*0x9E3779B9u;
        const double Scale=S.Scale*(1.0+.27*Plan);
        for(const FVector2D& B:C.Openings)
        {
            const double H=FMath::Max(60.0,B.Y-B.X);
            // Band height follows world topology, but rock composition is a distinct
            // field. Large supports, asymmetric shelves and connected strata.
            const double Q=X/(380.0*Scale), Z=(B.X+B.Y)/(1100.0*Scale);
            const double Macro=Noise(Q,Z,Key);
            const double Mezo=Noise(Q*3.1+Macro*.55,Z*2.7,Key+91u);
            const double Branch=Noise(Q*.73+4.7,Z+13.0,Key+217u);
            const double Top=Noise(Q*1.31+6.0,Z-8.0,Key+817u);
            const double Chips=Noise(X/(17.0*Scale),Z*9.0,Key+13u);
            const double ChipGate=FMath::Max(0.0,Mezo+.25);
            const double Pillar=FMath::SmoothStep(.43,.88,Macro+.12*Branch);
            const double Hanging=FMath::SmoothStep(.48,.90,Top-.15*Macro);
            const double Biome=.06*FMath::Sin(double(C.Biome)*1.3);
            const double Base=.08+.27*S.Density+Biome-.075*Plan;
            const double Jagged=(.035*Mezo+.028*S.Roughness*Chips-.05*S.Erosion*FMath::Max(0.0,Chips)*ChipGate);
            const double Low=B.X+H*(Base+.16*Macro+.10*Mezo+.77*Pillar+Jagged);
            const double High=B.Y-H*(Base+.15*Top+.08*Branch+.70*Hanging-Jagged*.73);
            if(Low<High) Out.Voids.Add(FVector2D(Low,High));
        }
        return Out;
    }
    inline FColor PaintLegacy(const FColumn& C,double X,double Z,int32 Plan,uint32 Seed,const FSettings& S)
    {
        double Edge=1.e10;
        bool Open=false;
        for(const FVector2D& B:C.Voids)
        {
            if(Z>B.X && Z<B.Y) Open=true;
            Edge=FMath::Min(Edge,FMath::Min(FMath::Abs(Z-B.X),FMath::Abs(Z-B.Y)));
        }
        if(Plan<3 && Open) return FColor(0,0,0,0);
        int32 Layer=0; double Best=1.e10;
        for(int32 L=0;L<4;++L)
        {
            const double D=FMath::Abs(Z-(C.Layers[L].X+C.Layers[L].Y)*.5);
            if(D<Best) {Best=D;Layer=L;}
        }
        const uint32 Key=Seed+uint32(Plan)*0x9E3779B9u;
        // Coherent colour islands rather than per-pixel independent noise.
        const double Scale=S.Scale*(1.0+.26*Plan);
        const double U=X/(190.0*Scale), V=Z/(145.0*Scale);
        const double Large=Noise(U,V,Key+1001u);
        const double Medium=Noise(U*3.7+Large*.8,V*2.8-Large*.6,Key+1003u);
        const double Fine=Noise(U*12.7+Medium*.9,V*10.1-Large,Key+1009u);
        const double Detail=S.Detail*(1.0-.17*Plan);
        const double Tone=Large*.67+Medium*.26+Fine*.12*Detail;
        int32 I=Tone<-.42?1:Tone<-.12?2:Tone<.22?3:4;
        // Cracks and chipped rims cluster in eroded patches. They are colour
        // indices, not detached geometry or photographic surface samples.
        if(Detail>.05 && FMath::Abs(Medium)<.045+.03*S.Erosion && Large<.10 && Fine<.62) I=0;
        if(Edge<28.0*Scale && Fine>.30 && Medium>-.10 && Detail>.2) I=4;
        if(Edge<42.0*Scale && Large<-.16 && Fine<-.12 && S.Erosion>.25) I=1;
        if(Large>.22 && Medium<-.26 && Detail>.15) I=5;
        if(Medium>.52 && Fine>.12 && Large>-.18 && Detail>.42) I=6;
        if(Large<-.40 && Medium>.2 && Detail>.25) I=7;
        if(Plan==3 && I==6) I=5;
        return FColor(uint8(I*36),Layer>=2?255:0,0,255);
    }
    // The mask is EXACTLY the Compose field, not a new composition algorithm.
    // These are semantic surface indices: 0 fissure, 1 recess, 2 shadowed
    // material, 3 body, 4 subtle raised material, 5/6/7 mineral traces.
    // The B byte is independent of the indices: a smoothly varying biome tint.
    inline uint8 BiomeTint(int32 A,int32 B,float Alpha)
    {
        const auto Tone=[](int32 Id){return .5+.5*FMath::Sin(double(Id)*2.39996323+.35);};
        return uint8(FMath::Clamp(FMath::RoundToInt(255.*FMath::Lerp(Tone(A),Tone(B),double(FMath::Clamp(Alpha,0.f,1.f)))),0,255));
    }
    inline double ContourZ(const FColumn* C,double E,bool bUpper)
    {
        if(!C)return E;
        double Result=E,Best=1.e10;
        for(const auto& B:C->Voids)
        {
            const double Candidate=bUpper?B.Y:B.X;
            const double D=FMath::Abs(Candidate-E);
            if(D<Best){Best=D;Result=Candidate;}
        }
        // A missing/discontinuous opening is not a continuous contour.
        return Best<96.0?Result:E;
    }
    inline FColor Paint(const FColumn& C,double X,double Z,int32 Plan,uint32 Seed,const FSettings& S,
                        const FColumn* Left=nullptr,const FColumn* Right=nullptr,double Cell=4.0,uint8 Biome=128)
    {
        double Edge=1.e10,E=Z;
        bool Open=false,bUpper=false;
        for(const FVector2D& B:C.Voids)
        {
            if(Z>B.X && Z<B.Y)Open=true;
            const double DL=FMath::Abs(Z-B.X),DH=FMath::Abs(Z-B.Y);
            if(DL<Edge){Edge=DL;E=B.X;bUpper=false;}
            if(DH<Edge){Edge=DH;E=B.Y;bUpper=true;}
        }
        if(Plan<3 && Open)return FColor(0,0,0,0);
        int32 Layer=0;double Best=1.e10;
        for(int32 L=0;L<4;++L)
        {
            const double D=FMath::Abs(Z-(C.Layers[L].X+C.Layers[L].Y)*.5);
            if(D<Best){Best=D;Layer=L;}
        }
        const uint32 Key=Seed+uint32(Plan)*0x9E3779B9u;
        const double Scale=S.Scale*(1.0+.30*Plan);
        const double Detail=S.Detail;
        // A broad sedimentary structure has a dominant, nearly uniform body.
        // Noise perturbs the bedding phase, not the identity of the material.
        const double U=X/(340.0*Scale),V=Z/(260.0*Scale);
        const double Warp=Noise(U,V,Key+1001u);
        const double Bend=Noise(U*.83+5.0,V*.66,Key+1003u);
        const double Phase=Z/(145.0*Scale)+.32*U+.65*Warp+.45*Bend;
        const double Strata=FMath::Sin(Phase*2.15);
        const double Broad=.72*Strata+.28*Warp;
        int32 I=3;
        if(Broad<-.62)I=2;
        else if(Broad>.72)I=4;
        // The closure is a quiet broad structure; no fissure, eroded rims,
        // accent islands or microtexture survive this depth.
        if(Plan==3)
        {
            I=Broad<-.80?2:Broad>.84?4:3;
            return FColor(uint8(I*36),Layer>=2?255:0,Biome,255);
        }
        // Normal distance is approximated from the actual contour tangent.
        // This avoids thick outlines on sloped edges. Only isolated segments
        // get a recess or a chipped edge; never a continuous border.
        double Dist=Edge;
        if(Edge<110.*Scale)
        {
            const double Slope=FMath::Clamp((ContourZ(Right,E,bUpper)-ContourZ(Left,E,bUpper))/FMath::Max(2.*Cell,1.),-5.,5.);
            Dist=Edge/FMath::Sqrt(1.+Slope*Slope);
        }
        const double Along=Noise(X/(96.*Scale),E/(230.*Scale),Key+3011u);
        const double Relief=Noise(X/(95.*Scale),Z/(48.*Scale),Key+3013u);
        if(Plan==2)
        {
            if(Dist<28.*Scale && Along<-.36 && Relief<-.15 && Detail>.2)I=2;
            return FColor(uint8(I*36),Layer>=2?255:0,Biome,255);
        }
        // Thin, discontinuous fissures follow bedding, plus contour-local
        // pockets. Small-scale noise only gates existing structural details.
        const double Micro=Noise(X/(19.*Scale),Z/(13.*Scale),Key+3017u);
        const double Fracture=FMath::Sin(Phase*3.1+.65*Relief);
        if(Dist<58.*Scale && Along<-.28 && Relief<.03 && S.Erosion>.12)I=1;
        if(Dist<25.*Scale && Along>.26 && Relief>.24 && Detail>.18)I=4;
        if(Detail>.15 && FMath::Abs(Fracture)<(.07+.045*S.Roughness) &&
            Relief<-.17 && Along<.25 && Micro<.58)
            I=Plan==0?0:1;
        // Mineral traces occur only in small, erosion-associated islands.
        // No entire low-frequency noise lobe is assigned a contrasting hue.
        if(Plan==0 && Detail>.32 && S.Erosion>.2 && I!=0 && Dist<100.*Scale &&
            FMath::Abs(Fracture)<.32 && Relief>.33 && Micro>.53 && Along>.05)
        {
            I=Along>.60?6:Along<.24?7:5;
        }
        else if(Plan==1 && Detail>.45 && S.Erosion>.4 && I==3 &&
                 Dist<60.*Scale && Relief>.56 && Micro>.65 && Along>.25)I=5;
        return FColor(uint8(I*36),Layer>=2?255:0,Biome,255);
    }

}
