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
    inline FColor Paint(const FColumn& C,double X,double Z,int32 Plan,uint32 Seed,const FSettings& S)
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
}
