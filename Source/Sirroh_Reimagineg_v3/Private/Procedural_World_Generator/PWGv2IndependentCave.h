#pragma once
#include "PWGv2Mesh.h"

// Independent solid geometry. The six families have different topologies and
// silhouettes; none samples the old wall or uses a circular ring generator.
namespace PWGIndependentCave
{
    enum class EFamily:uint8 { Strata, Fracture, Overhang, Speleothem, Boulders, Ridge };
    struct FRecipe
    {
        double Scale=1., Density=1.;
        uint32 Families=0x3f;
        int32 PlanCount=3; // plus closure
        bool Allows(EFamily F) const {return (Families&(1u<<uint8(F)))!=0;}
    };
    inline FVector2D Project(FVector P,FVector N)
    {
        // Face-aligned projection in world units, not per-rock stretched UVs.
        const FVector A=N.GetAbs();
        if(A.Z>A.X && A.Z>A.Y) return FVector2D(P.X,P.Y)/160.;
        if(A.X>A.Y) return FVector2D(P.Y,-P.Z)/160.;
        return FVector2D(P.X,-P.Z)/160.;
    }
    inline void Tri(FPWGv2MeshBuffers& B,FVector A,FVector C,FVector D,float Shade)
    {
        const FVector N=FVector::CrossProduct(C-A,D-A).GetSafeNormal();
        if(N.IsNearlyZero())return;
        const int32 I=B.V.Num();
        B.V.Append({A,C,D});B.T.Append({I,I+1,I+2});B.N.Append({N,N,N});
        B.UV.Append({Project(A,N),Project(C,N),Project(D,N)});
        const FLinearColor Col(Shade,Shade,Shade,1);B.C.Append({Col,Col,Col});
    }
    // Bevelled irregular polygonal slab. Front/back are tessellated, bevels
    // have separate normals; large planar surfaces and chipped edges coexist.
    inline void Slab(FPWGv2MeshBuffers& B,FRandomStream& R,FVector Centre,double RX,double RY,double RZ,
        std::initializer_list<FVector2D> Shape,float Shade,double Tilt=0.)
    {
        TArray<FVector2D> P;for(const auto& V:Shape)P.Add(V);
        if(P.Num()<3)return;
        FVector2D Mean=FVector2D::ZeroVector;
        for(auto& V:P){V.X*=R.FRandRange(.91f,1.09f);V.Y*=R.FRandRange(.92f,1.08f);Mean+=V;}
        Mean/=P.Num();
        const double Bevel=R.FRandRange(.12f,.25f);
        TArray<FVector> F,M,G;
        for(const auto& V:P)
        {
            auto Pt=[&](double K,double Y)
            {
                const FVector2D Q=Mean+(V-Mean)*K;
                return Centre+FVector(Q.X*RX+Q.Y*RZ*Tilt,Y,Q.Y*RZ);
            };
            F.Add(Pt(1.-Bevel,-RY*R.FRandRange(.70f,.99f)));
            M.Add(Pt(1.,-RY*.20));
            G.Add(Pt(.83,RY*.85));
        }
        const FVector FC=Centre+FVector(Mean.X*RX+Mean.Y*RZ*Tilt,-RY*.95,Mean.Y*RZ);
        const FVector GC=Centre+FVector(Mean.X*RX+Mean.Y*RZ*Tilt,RY*.85,Mean.Y*RZ);
        for(int32 I=0;I<P.Num();++I)
        {
            const int32 J=(I+1)%P.Num();
            Tri(B,FC,F[I],F[J],Shade);
            Tri(B,F[I],M[I],M[J],Shade*.85f);Tri(B,F[I],M[J],F[J],Shade*.85f);
            Tri(B,M[I],G[I],G[J],Shade*.73f);Tri(B,M[I],G[J],M[J],Shade*.73f);
            Tri(B,GC,G[J],G[I],Shade*.84f);
        }
    }
    inline void Formation(FPWGv2MeshBuffers& B,FRandomStream& R,EFamily Family,FVector C,double RX,double RY,double H,bool bHanging=false)
    {
        const auto S=[&](FVector Pos,double X,double Z,std::initializer_list<FVector2D> P,float Shade,double Tilt=0.)
        {Slab(B,R,Pos,X,RY*R.FRandRange(.7f,1.f),Z,P,Shade,Tilt);};
        const auto Block=[&](FVector Pos,double X,double Z,float Shade,double Tilt=0.)
        {S(Pos,X,Z,{{-1,-.55},{-.65,-1},{.55,-.92},{1,-.25},{.74,.78},{-.18,1},{-.92,.46}},Shade,Tilt);};
        switch(Family)
        {
        case EFamily::Strata:
            for(int32 I=0;I<5;++I)
            {
                const double T=(I+.5)/5.;
                S(C+FVector(RX*R.FRandRange(-.15f,.15f),RY*R.FRandRange(-.13f,.13f),(T-.5)*H),
                    RX*R.FRandRange(.74f,1.13f),H*.115,
                    {{-1,-.62},{-.48,-.96},{.75,-.75},{1,-.25},{.72,.86},{-.60,.95},{-.94,.42}},
                    R.FRandRange(.57f,.93f),R.FRandRange(-.34f,.34f));
            }break;
        case EFamily::Fracture:
            for(int32 I=0;I<4;++I)
            {
                const double T=(I+.5)/4.;
                Block(C+FVector(RX*(T-.5)*.55+RX*R.FRandRange(-.16f,.16f),RY*R.FRandRange(-.10f,.10f),(T-.5)*H),
                    RX*R.FRandRange(.68f,.98f),H*.147,R.FRandRange(.55f,.95f),R.FRandRange(-.32f,.32f));
            }break;
        case EFamily::Overhang:
            S(C,RX,H*.5,{{-1,-.12},{-.63,-.25},{-.28,-.48},{.18,-1},{.72,-.56},{1,.36},{.72,.9},{-.74,1},{-1,.62}},.83f,-.18);
            Block(C+FVector(-RX*.47,RY*.12,H*.24),RX*.54,H*.28,.60f,-.35);break;
        case EFamily::Speleothem:
        {
            const double Sign=bHanging?-1.:1.;
            const FVector Anchor=C-FVector(0,0,Sign*H*.40);
            // Stepped tapered asymmetric wedge; no ring or uniformly smoothed normals.
            auto Tip=[&](double Offset,double X,double Len,double Lean)
            {
                const FVector Pos=Anchor+FVector(Offset,0,Sign*Len*.42);
                if(bHanging)S(Pos,X,Len*.5,{{-1,.86},{.83,1},{.72,.28},{.38,-.37},{-.12,-1},{-.38,-.35},{-.76,.3}},.79f,Lean);
                else S(Pos,X,Len*.5,{{-1,-.86},{-.73,-.3},{-.32,.44},{.15,1},{.40,.29},{.77,-.27},{1,-1}},.76f,Lean);
            };
            Tip(0,RX*.63,H,.28);Tip(-RX*.62,RX*.37,H*.58,-.33);Tip(RX*.57,RX*.31,H*.39,.24);break;
        }
        case EFamily::Boulders:
            for(int32 I=0;I<6;++I)
            {
                const double X=(I%3-1)*RX*.64+RX*R.FRandRange(-.20f,.20f);
                const double Z=(I/3-.5)*H*.40+H*R.FRandRange(-.10f,.10f);
                Block(C+FVector(X,RY*R.FRandRange(-.12f,.12f),Z),RX*R.FRandRange(.28f,.50f),
                    H*R.FRandRange(.17f,.30f),R.FRandRange(.52f,.95f),R.FRandRange(-.45f,.45f));
            }break;
        case EFamily::Ridge:
            for(int32 I=0;I<4;++I)
            {
                double X=(I-1.5)*RX*.48;
                double Z=H*(I-1.5)*.11;
                S(C+FVector(X,RY*R.FRandRange(-.13f,.13f),Z),RX*.43,H*R.FRandRange(.28f,.49f),
                    {{-1,-.75},{-.52,-1},{.79,-.74},{1,.13},{.37,.47},{-.12,1},{-.65,.64}},R.FRandRange(.55f,.88f),-.42);
            }break;
        }
    }
    inline void Build(FPWGv2MeshBuffers (&B)[4],int32 Seed,double X,double Floor,double Roof,double Width,
        double ViewZ,double ViewHeight,double HalfWidth,double Start,double Depth,const FRecipe& Recipe=FRecipe())
    {
        const double H=FMath::Max(240.0,Roof-Floor);
        FRandomStream R(Seed);
        auto Form=[&](int32 Plan,EFamily Kind,double Pos,double Low,double High,double Radius,bool Hanging=false)
        {
            if(Plan>=Recipe.PlanCount || !Recipe.Allows(Kind))return;
            const double Fractions[3]={.10,.36,.65};
            const double Y=Start+Depth*Fractions[Plan];
            const double J=R.FRandRange(-.022f,.022f);
            const double Size=FMath::Clamp(Recipe.Scale,.4,1.6);
            const double HH=(High-Low)*H*Size;
            if(Recipe.Density<1. && R.FRand()>Recipe.Density)return;
            Formation(B[Plan],R,Kind,FVector(X+(Pos+J)*Width,Y,Floor+(Low+High)*.5*H),
                Radius*Width*Size*R.FRandRange(.88f,1.12f),Depth*(Plan==0?.028:Plan==1?.038:.045),HH,Hanging);
        };
        Form(0,EFamily::Strata,-.42,-.26,1.24,.115);
        Form(0,EFamily::Boulders,.43,-.23,.35,.105);
        Form(0,EFamily::Overhang,.40,.63,1.40,.12,true);
        Form(1,EFamily::Fracture,-.23,-.23,1.26,.075);
        Form(1,EFamily::Speleothem,.15,-.12,.55,.07);
        Form(1,EFamily::Speleothem,-.02,.54,1.34,.069,true);
        Form(1,EFamily::Ridge,.49,-.23,1.24,.13);
        Form(2,EFamily::Ridge,-.34,-.40,1.31,.18);
        Form(2,EFamily::Boulders,-.03,-.28,.27,.15);
        Form(2,EFamily::Overhang,.23,.56,1.40,.14,true);
        Form(2,EFamily::Strata,.40,-.30,1.30,.14);
        constexpr int32 NX=16,NZ=8;
        const double Z0=ViewZ-ViewHeight*1.8,Z1=ViewZ+ViewHeight*1.8;
        auto Point=[&](int32 I,int32 K)
        {
            return FVector(X+FMath::Lerp(-HalfWidth,HalfWidth,double(I)/NX),
                Start+Depth*(.95+.013*FMath::Sin(I*.83+K*.39)+.008*FMath::Cos(K*.92-I*.31)),
                FMath::Lerp(Z0,Z1,double(K)/NZ));
        };
        for(int32 I=0;I<NX;++I)for(int32 K=0;K<NZ;++K)
        {
            const FVector A=Point(I,K),C=Point(I+1,K),D=Point(I+1,K+1),E=Point(I,K+1);
            Tri(B[3],A,C,D,.65f);Tri(B[3],A,D,E,.65f);
        }
    }
    inline void Submit(const FPWGv2MeshBuffers& B,UProceduralMeshComponent* Mesh,int32 Section)
    {
        TArray<FProcMeshTangent> Tangents;Tangents.SetNum(B.V.Num());
        for(int32 I=0;I+2<B.V.Num();I+=3)
        {
            const FVector E=B.V[I+1]-B.V[I],F=B.V[I+2]-B.V[I];
            const FVector2D U=B.UV[I+1]-B.UV[I],V=B.UV[I+2]-B.UV[I];
            const double Det=U.X*V.Y-U.Y*V.X;
            FVector T=FVector::ZeroVector,Bit=FVector::ZeroVector;
            if(FMath::Abs(Det)>1.e-8){T=((E*V.Y-F*U.Y)/Det).GetSafeNormal();Bit=((F*U.X-E*V.X)/Det).GetSafeNormal();}
            if(T.IsNearlyZero())T=FVector::CrossProduct(B.N[I],FVector::ZAxisVector).GetSafeNormal();
            if(T.IsNearlyZero())T=FVector::XAxisVector;
            const bool Flip=FVector::DotProduct(FVector::CrossProduct(B.N[I],T),Bit)<0;
            for(int32 K=0;K<3;++K)Tangents[I+K]=FProcMeshTangent(T,Flip);
        }
        Mesh->CreateMeshSection_LinearColor(Section,B.V,B.T,B.N,B.UV,B.C,Tangents,false);
    }
}
