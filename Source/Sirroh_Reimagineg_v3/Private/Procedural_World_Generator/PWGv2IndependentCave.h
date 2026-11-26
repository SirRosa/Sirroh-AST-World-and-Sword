#pragma once
#include "PWGv2Mesh.h"

// Independent closed rocky volumes, not fragments of the tunnel's rear shell.
// The orthographic composition is expressed in the opening's own X/Z frame.
namespace PWGIndependentCave
{
    inline void Tri(FPWGv2MeshBuffers& B,FVector A,FVector C,FVector D,FVector2D U,FVector2D V,FVector2D W)
    {
        const FVector N=FVector::CrossProduct(C-A,D-A).GetSafeNormal();
        if (N.IsNearlyZero()) return;
        const int32 I=B.V.Num();
        B.V.Append({A,C,D}); B.N.Append({N,N,N}); B.T.Append({I,I+1,I+2});
        B.UV.Append({U,V,W}); B.C.Append({FLinearColor::White,FLinearColor::White,FLinearColor::White});
    }

    // A stack of irregular elliptical rings around a bending centreline.
    // Radius envelope controls pillars, spires and roof-hung stalactites.
    inline void Rock(FPWGv2MeshBuffers& B,FRandomStream& R,double X,double Y,double Z,
        double H,double RX,double RY,double Lean,double Bottom,double Top,double Repeat,int32 Sides=9)
    {
        constexpr int32 Rings=17;
        FVector V[Rings][24], Norm[Rings][24]; FVector2D UV[Rings][24];
        const double Phase=R.FRandRange(0.f,2.f*PI);
        const double Phase2=R.FRandRange(0.f,2.f*PI);
        const double U0=R.FRandRange(0.f,32.f), V0=R.FRandRange(0.f,32.f);
        const double Turn=R.FRandRange(-.45f,.45f);
        const double Jitter=R.FRandRange(.12f,.27f);
        Sides=FMath::Clamp(Sides*2,14,24);
        for(int32 K=0;K<Rings;++K)
        {
            const double T=double(K)/(Rings-1);
            const double Radius=FMath::Max(.035,FMath::Lerp(Bottom,Top,T)*
                (1.0+.18*FMath::Sin(T*PI*3.0+Phase)+.11*FMath::Sin(T*PI*7.0+Phase2)));
            const double CentreX=X+Lean*T+RX*.23*FMath::Sin(T*PI*2.0+Phase);
            const double CentreY=Y+RY*.19*FMath::Sin(T*PI*2.0+Phase2);
            for(int32 J=0;J<Sides;++J)
            {
                const double A=double(J)*2.0*PI/Sides;
                const double Lobe=1.0+Jitter*FMath::Sin(3.0*A+Phase+T*1.5)+.12*FMath::Cos(5.0*A+Phase2+T*3.0)
                    +.055*FMath::Sin(9.0*A+Phase-T*8.0)+.04*FMath::Cos(13.0*A+T*11.0+Phase2);
                const double Angle=A+Turn*T+Phase;
                V[K][J]=FVector(CentreX+RX*Radius*Lobe*FMath::Cos(Angle),
                    CentreY+RY*Radius*Lobe*FMath::Sin(Angle),Z+H*T+H*.025*FMath::Sin(A+T*PI+Phase2));
                UV[K][J]=FVector2D(U0+double(J)/Sides*RX*Repeat/45.0,V0+T*H*Repeat/100.0);
            }
        }
        // Smooth shared ring normals, retaining the actual irregular rock outline.
        for(int32 K=0;K<Rings;++K) for(int32 J=0;J<Sides;++J)
        {
            const FVector Around=V[K][(J+1)%Sides]-V[K][(J+Sides-1)%Sides];
            const FVector Along=V[FMath::Min(K+1,Rings-1)][J]-V[FMath::Max(0,K-1)][J];
            Norm[K][J]=FVector::CrossProduct(Around,Along).GetSafeNormal();
        }
        auto SmoothTri=[&](FVector A,FVector C,FVector D,FVector2D U,FVector2D V0,FVector2D W,FVector N0,FVector N1,FVector N2)
        {
            const int32 I=B.N.Num(); Tri(B,A,C,D,U,V0,W);
            if(B.N.Num()==I+3){B.N[I]=N0; B.N[I+1]=N1; B.N[I+2]=N2;}
        };
        for(int32 K=0;K<Rings-1;++K)
            for(int32 J=0;J<Sides;++J)
            {
                const int32 N=(J+1)%Sides;
                const FVector2D U=UV[K][J],U1=N?UV[K][N]:UV[K][N]+FVector2D(RX*Repeat/45.0,0);
                const FVector2D W=UV[K+1][J],W1=N?UV[K+1][N]:UV[K+1][N]+FVector2D(RX*Repeat/45.0,0);
                SmoothTri(V[K][J],V[K][N],V[K+1][N],U,U1,W1,Norm[K][J],Norm[K][N],Norm[K+1][N]);
                SmoothTri(V[K][J],V[K+1][N],V[K+1][J],U,W1,W,Norm[K][J],Norm[K+1][N],Norm[K+1][J]);
            }
        for(int32 K:{0,Rings-1})
        {
            FVector Centre=FVector::ZeroVector;
            for(int32 J=0;J<Sides;++J) Centre+=V[K][J];
            Centre/=Sides;
            for(int32 J=0;J<Sides;++J)
            {
                const int32 N=(J+1)%Sides;
                if(K==0) Tri(B,Centre,V[K][N],V[K][J],FVector2D(U0,V0),UV[K][N],UV[K][J]);
                else Tri(B,Centre,V[K][J],V[K][N],FVector2D(U0,V0),UV[K][J],UV[K][N]);
            }
        }
    }

    inline void Build(FPWGv2MeshBuffers (&B)[4],int32 Seed,double X,double Floor,double Roof,double Width,
        double ViewZ,double ViewHeight,double HalfWidth,double Start,double Depth)
    {
        const double H=FMath::Max(240.0,Roof-Floor);
        FRandomStream R(Seed);
        auto Form=[&](int32 Plan,double Pos,double Low,double High,double Radius,double Lean,double Base,double Tip,int32 Sides)
        {
            const double Fractions[3]={.10,.36,.65};
            const double Repeats[3]={1.15,.62,.24};
            const double J=R.FRandRange(-.035f,.035f);
            const double Y=Start+Depth*Fractions[Plan];
            Rock(B[Plan],R,X+(Pos+J)*Width,Y,Floor+Low*H,(High-Low)*H,
                Radius*Width*R.FRandRange(.86f,1.14f),Depth*(Plan==0?.027:Plan==1?.038:.048),
                Lean*Width,Base,Tip,Repeats[Plan],Sides);
        };
        // Deliberate negative space around the centre and an asymmetrical silhouette.
        // L2: near framing spur and overhang. L3: slender arch-like pillars and hanging rock.
        Form(0,-.43,-.24,1.42,.128,.06,1.15,.60,10);
        Form(0,.44,-.30,.49,.115,-.12,1.18,.04,9);
        Form(0,.31,.60,1.42,.105,.12,.035,1.15,8);
        Form(1,-.23,-.35,.93,.071,.085,.96,.48,9);
        Form(1,.17,-.38,1.43,.049,-.055,.90,.77,8);
        Form(1,-.05,.62,1.38,.085,-.065,.04,1.12,9);
        Form(1,.49,-.35,1.35,.115,-.06,1.0,.72,10);
        // L4: wider, lower-frequency distant silhouettes; irregular valley in the middle.
        Form(2,-.35,-.45,1.45,.19,.10,1.10,.75,8);
        Form(2,-.02,-.40,.28,.13,.09,1.10,.06,7);
        Form(2,.22,.65,1.55,.14,.08,.04,1.20,8);
        Form(2,.40,-.42,1.22,.16,-.11,1.0,.47,9);
        // L5: very low-frequency closing surface, beyond all rock volumes.
        constexpr int32 NX=16,NZ=8;
        const double Z0=ViewZ-ViewHeight*1.8,Z1=ViewZ+ViewHeight*1.8;
        auto Point=[&](int32 I,int32 K)
        {
            const double PX=X+FMath::Lerp(-HalfWidth,HalfWidth,double(I)/NX);
            const double PZ=FMath::Lerp(Z0,Z1,double(K)/NZ);
            return FVector(PX,Start+Depth*(.95+.013*FMath::Sin(I*.83+K*.39)+.008*FMath::Cos(K*.92-I*.31)),PZ);
        };
        for(int32 I=0;I<NX;++I)for(int32 K=0;K<NZ;++K)
        {
            const FVector A=Point(I,K),C=Point(I+1,K),D=Point(I+1,K+1),E=Point(I,K+1);
            B[3].Quad(A,C,D,E,FLinearColor::White);
            for(int32 J=0;J<4;++J) B[3].UV[B[3].UV.Num()-4+J]*=.08;
        }
    }
}
