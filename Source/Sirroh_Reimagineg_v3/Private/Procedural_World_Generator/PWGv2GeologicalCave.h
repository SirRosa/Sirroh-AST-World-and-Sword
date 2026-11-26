#pragma once
#include "PWGv2IndependentCave.h"

// Reference art only. Each depth band is a continuous geological volume, with
// a void carved between two height envelopes. No loose rock instances.
// Every occupied point has a vertical path to a roof or ground boundary.
namespace PWGGeologicalCave
{
    struct FSection
    {
        double X, Z, Width, Height, Y, Depth, Phase;
        int32 Plan;
        static double Hill(double U,double C,double R)
        {
            const double T=FMath::Clamp(1.-FMath::Abs(U-C)/R,0.,1.);
            return T*T*(3.-2.*T);
        }
        double Noise(double U,double F,double Offset) const
        {return FMath::PerlinNoise1D(float(U*F+Phase+Offset));}
        FVector2D Opening(double PX) const
        {
            const double U=(PX-X)/Width;
            // Each plane has its own skyline and opening, independent of the
            // playable tunnel boundary. Pillars are where the envelopes meet.
            double Ground=-.56,Roof=.58;
            if(Plan==0)
            {
                Ground+=.58*Hill(U,-.49,.33)+.35*Hill(U,.48,.29)+.17*Hill(U,-.13,.14);
                Roof-=.71*Hill(U,-.49,.25)+.38*Hill(U,.41,.26)+.10*Hill(U,.05,.13);
            }
            else if(Plan==1)
            {
                Ground=-.67+.94*Hill(U,.27,.25)+.39*Hill(U,-.37,.32);
                Roof=.76-.95*Hill(U,.29,.26)-.38*Hill(U,-.28,.29);
            }
            else if(Plan==2)
            {
                Ground=-.85+1.05*Hill(U,-.24,.24)+.48*Hill(U,.50,.30);
                Roof=1.02-1.10*Hill(U,-.26,.23)-.42*Hill(U,.41,.35);
            }
            else
            {
                Ground=-1.05+.72*Hill(U,.03,.38)+.42*Hill(U,-.54,.31);
                Roof=1.17-.66*Hill(U,.03,.42)-.39*Hill(U,.53,.30);
            }
            // Long fractures and erosion at two distinct scales. These perturb
            // boundaries, not independent occupied voxels, so no floating islands.
            const double A=Plan==0?1.:Plan==1?.8:Plan==2?.55:.32;
            Ground+=A*(.095*Noise(U,11.,7.)+.045*Noise(U,39.,12.)+.018*Noise(U,95.,31.));
            Roof+=A*(.11*Noise(U,9.,84.)+.052*Noise(U,34.,98.)+.021*Noise(U,89.,113.));
            return FVector2D(Z+Ground*Height,Z+Roof*Height);
        }
        double Solid(double PX,double PZ) const
        {
            const auto O=Opening(PX);
            return FMath::Max(O.X-PZ,PZ-O.Y);
        }
        FVector Front(double PX,double PZ) const
        {
            const double U=(PX-X)/Width,V=(PZ-Z)/Height;
            const double Edge=FMath::SmoothStep(0.,Height*.16,FMath::Max(0.,Solid(PX,PZ)));
            const double N=FMath::PerlinNoise2D(FVector2D(U*9.+Phase,V*4.+17.));
            const double M=FMath::PerlinNoise2D(FVector2D(U*28.+Phase,V*13.+42.));
            const double Bedding=FMath::PerlinNoise2D(FVector2D(U*3.+Phase,(V+.20*U)*26.));
            const double Relief=Edge*(.027+.011*N+.006*M+.004*Bedding);
            return FVector(PX,Y+Depth*(.018-Relief),PZ);
        }
        FVector Back(FVector2D P) const {return FVector(P.X,Y+Depth*.055,P.Y);}
        FVector Normal(FVector P) const
        {
            constexpr double E=3.;
            const FVector A=Front(P.X+E,P.Z)-Front(P.X-E,P.Z);
            const FVector B=Front(P.X,P.Z+E)-Front(P.X,P.Z-E);
            return FVector::CrossProduct(A,B).GetSafeNormal();
        }
    };
    inline void Face(FPWGv2MeshBuffers& B,const FSection& S,FVector A,FVector C,FVector D,bool bFront)
    {
        const FVector N=FVector::CrossProduct(C-A,D-A).GetSafeNormal();
        if(N.IsNearlyZero())return;
        const int32 I=B.V.Num();
        for(const auto& P:{A,C,D})
        {
            B.V.Add(P);
            B.N.Add(bFront?S.Normal(P):N);
            // Common X/Z coordinates make bedding and fissures continuous.
            B.UV.Add(bFront?FVector2D(P.X,-P.Z)/160.:PWGIndependentCave::Project(P,N));
            const float E=float(FMath::SmoothStep(0.,S.Height*.10,FMath::Max(0.,S.Solid(P.X,P.Z))));
            B.C.Add(FLinearColor(E,E,E,1));
        }
        B.T.Append({I,I+1,I+2});
    }
    inline void QuadWall(FPWGv2MeshBuffers& B,const FSection& S,FVector2D A,FVector2D C)
    {
        const FVector F=S.Front(A.X,A.Y),G=S.Front(C.X,C.Y),R=S.Back(A),T=S.Back(C);
        Face(B,S,G,F,R,false);Face(B,S,G,R,T,false);
    }
    inline void BuildPlan(FPWGv2MeshBuffers& B,const FSection& S,double HalfWidth,double HalfHeight)
    {
        // Sample envelopes once per column. Piecewise linear clipping makes
        // neighbouring triangles share the same intersections, even at a pinch.
        const int32 NX=FMath::Clamp(FMath::CeilToInt(HalfWidth*2./(S.Plan==0?13.:S.Plan==1?19.:27.)),16,240);
        const int32 NZ=FMath::Clamp(FMath::CeilToInt(HalfHeight*2./(S.Plan==0?19.:S.Plan==1?27.:35.)),16,160);
        const double X0=S.X-HalfWidth,Z0=S.Z-HalfHeight,X1=S.X+HalfWidth,Z1=S.Z+HalfHeight;
        TArray<FVector2D> Grid;TArray<double> Values;Grid.SetNum((NX+1)*(NZ+1));Values.SetNum(Grid.Num());
        auto Id=[&](int32 I,int32 J){return I*(NZ+1)+J;};
        for(int32 I=0;I<=NX;++I)
        {
            const double PX=FMath::Lerp(X0,X1,double(I)/NX);
            const auto O=S.Opening(PX);
            for(int32 J=0;J<=NZ;++J)
            {
                const double PZ=FMath::Lerp(Z0,Z1,double(J)/NZ);
                Grid[Id(I,J)]=FVector2D(PX,PZ);
                Values[Id(I,J)]=S.Plan==3?1.:FMath::Max(O.X-PZ,PZ-O.Y);
            }
        }
        auto Clip=[&](int32 A,int32 C,int32 D)
        {
            const int32 Ids[3]={A,C,D};
            TArray<FVector2D,TInlineAllocator<4>> Poly;
            TArray<FVector2D,TInlineAllocator<2>> Cross;
            for(int32 K=0;K<3;++K)
            {
                const int32 U=Ids[K],V=Ids[(K+1)%3];
                const double F=Values[U],G=Values[V];
                if(F>=0.)Poly.Add(Grid[U]);
                if((F>=0.)!=(G>=0.))
                {
                    const FVector2D P=FMath::Lerp(Grid[U],Grid[V],F/(F-G));
                    Poly.Add(P);Cross.Add(P);
                }
            }
            if(Poly.Num()<3)return;
            auto P=[&](int32 K){return S.Front(Poly[K].X,Poly[K].Y);};
            for(int32 K=1;K+1<Poly.Num();++K)
            {
                Face(B,S,P(0),P(K),P(K+1),true);
                Face(B,S,S.Back(Poly[0]),S.Back(Poly[K+1]),S.Back(Poly[K]),false);
            }
            // Oriented boundary edges of the clipped polygon, including the
            // outside domain. Internal edges require no wall.
            for(int32 K=0;K<Poly.Num();++K)
            {
                const auto U=Poly[K],V=Poly[(K+1)%Poly.Num()];
                const bool Rim=(FMath::IsNearlyEqual(U.X,X0)&&FMath::IsNearlyEqual(V.X,X0))||
                    (FMath::IsNearlyEqual(U.X,X1)&&FMath::IsNearlyEqual(V.X,X1))||
                    (FMath::IsNearlyEqual(U.Y,Z0)&&FMath::IsNearlyEqual(V.Y,Z0))||
                    (FMath::IsNearlyEqual(U.Y,Z1)&&FMath::IsNearlyEqual(V.Y,Z1));
                const bool Cut=Cross.Num()==2 && ((U.Equals(Cross[0],.001)&&V.Equals(Cross[1],.001))||
                    (V.Equals(Cross[0],.001)&&U.Equals(Cross[1],.001)));
                if(Rim||Cut)QuadWall(B,S,U,V);
            }
        };
        for(int32 I=0;I<NX;++I)for(int32 J=0;J<NZ;++J)
        {
            const int32 A=Id(I,J),C=Id(I+1,J),D=Id(I+1,J+1),E=Id(I,J+1);
            Clip(A,C,D);Clip(A,D,E);
        }
    }
    inline void Build(FPWGv2MeshBuffers (&B)[4],int32 Seed,double X,double Floor,double Roof,double Width,
        double ViewZ,double ViewHeight,double HalfWidth,double Start,double Depth)
    {
        const double H=FMath::Max(ViewHeight,Roof-Floor);
        const double Y[4]={.10,.36,.65,.92};
        const double S[4]={1.,1.16,1.35,1.55};
        const double Phase=double(uint32(Seed)%1021)*.071;
        for(int32 I=0;I<4;++I)
        {
            FSection Section{X,ViewZ,Width,H*S[I],Start+Depth*Y[I],Depth,Phase+I*47.83,I};
            BuildPlan(B[I],Section,I==3?HalfWidth:FMath::Min(HalfWidth,Width*1.1),H*2.0);
        }
    }
}
