#pragma once
#include "PWGv2Mesh.h"

// Prototype only: cut sparse, differently shaped relief patches out of the existing
// connected opening domain. The backing remains continuous. No physics or save data.
namespace PWGDepthPrototype
{
    struct FPoint
    {
        FVector P;
        FVector2D UV;
        double Field;
    };

    inline double FieldAt(const FVector& P,int32 Seed,int32 Plan)
    {
        const double Scale=Plan==0?340.0:540.0;
        const double Phase=double(uint32(Seed)%8191u)*.113+Plan*37.71;
        const FVector2D Q(P.X/Scale+Phase,P.Z/(Scale*2.1)+Phase*.73);
        return FMath::PerlinNoise2D(Q)+.27*FMath::PerlinNoise2D(Q*2.7+FVector2D(19.7,5.4))
            -(Plan==0?.19:.06);
    }

    inline FVector PositionAt(const FVector& P,int32 Seed,int32 Plan,double Span)
    {
        const double Phase=double(uint32(Seed)%4093u)*.173+Plan*57.13;
        const double Relief=FMath::PerlinNoise2D(FVector2D(P.X/230.0+Phase,P.Z/260.0-Phase));
        return FVector(P.X,250.0+Span*((Plan==0?.12:.49)+.075*Relief),P.Z);
    }

    inline void Triangle(FPWGv2MeshBuffers& B,const FVector& A,const FVector& C,const FVector& D,
        const FVector2D& UV0,const FVector2D& UV1,const FVector2D& UV2)
    {
        const FVector N=FVector::CrossProduct(C-A,D-A).GetSafeNormal();
        if (N.IsNearlyZero()) return;
        const int32 I=B.V.Num();
        B.V.Append({A,C,D}); B.T.Append({I,I+1,I+2}); B.N.Append({N,N,N});
        B.UV.Append({UV0,UV1,UV2});
        B.C.Append({FLinearColor::White,FLinearColor::White,FLinearColor::White});
    }

    inline void BuildFormation(const FProcMeshSection& Src,int32 Seed,int32 Plan,double Span,FPWGv2MeshBuffers& Out)
    {
        const auto& V=Src.ProcVertexBuffer;
        const auto& T=Src.ProcIndexBuffer;
        for (int32 I=0;I+2<T.Num();I+=3)
        {
            const auto& A=V[T[I]]; const auto& B=V[T[I+1]]; const auto& C=V[T[I+2]];
            // Skip edge-on floor/ceiling banks; retain only the actual rear opening.
            if (A.Position.Y<100.0 || B.Position.Y<100.0 || C.Position.Y<100.0) continue;
            if (FMath::Abs(FVector::CrossProduct(B.Position-A.Position,C.Position-A.Position).Y)<.01) continue;
            FPoint In[3]={{A.Position,A.UV0,FieldAt(A.Position,Seed,Plan)},
                          {B.Position,B.UV0,FieldAt(B.Position,Seed,Plan)},
                          {C.Position,C.UV0,FieldAt(C.Position,Seed,Plan)}};
            TArray<FPoint,TInlineAllocator<4>> Poly;
            TArray<FPoint,TInlineAllocator<2>> Cuts;
            for (int32 J=0;J<3;++J)
            {
                const FPoint& P=In[J]; const FPoint& Q=In[(J+1)%3];
                if (P.Field>=0.0) Poly.Add(P);
                if ((P.Field>=0.0)!=(Q.Field>=0.0))
                {
                    const double Alpha=P.Field/(P.Field-Q.Field);
                    const FPoint Cut{FMath::Lerp(P.P,Q.P,Alpha),FMath::Lerp(P.UV,Q.UV,Alpha),0.0};
                    Poly.Add(Cut); Cuts.Add(Cut);
                }
            }
            for (int32 J=1;J+1<Poly.Num();++J)
                Triangle(Out,PositionAt(Poly[0].P,Seed,Plan,Span),PositionAt(Poly[J].P,Seed,Plan,Span),
                    PositionAt(Poly[J+1].P,Seed,Plan,Span),Poly[0].UV,Poly[J].UV,Poly[J+1].UV);
            // Physical side faces at the cut contour, not at chunk edges.
            if (Cuts.Num()==2)
            {
                const FVector P=PositionAt(Cuts[0].P,Seed,Plan,Span), Q=PositionAt(Cuts[1].P,Seed,Plan,Span);
                const FVector D(0,Span*.055,0);
                Triangle(Out,P,Q,Q+D,Cuts[0].UV,Cuts[1].UV,Cuts[1].UV+FVector2D(0,.15));
                Triangle(Out,P,Q+D,P+D,Cuts[0].UV,Cuts[1].UV+FVector2D(0,.15),Cuts[0].UV+FVector2D(0,.15));
            }
        }
    }

    inline FProcMeshSection BuildBacking(const FProcMeshSection& Src,double Span)
    {
        FProcMeshSection Copy=Src;
        Copy.bEnableCollision=false;
        Copy.SectionLocalBox=FBox(ForceInit);
        for (auto& V:Copy.ProcVertexBuffer)
        {
            if (V.Position.Y>100.0)
                V.Position.Y=250.0+Span*(.90+.045*FMath::Clamp((V.Position.Y-185.0)/60.0,-1.0,1.0));
            Copy.SectionLocalBox+=V.Position;
        }
        // All source rear sections are emitted by FPWGv2MeshBuffers::Quad.
        // Recompute normals from the transformed geometry.
        for (int32 I=0;I+3<Copy.ProcVertexBuffer.Num();I+=4)
        {
            auto& V=Copy.ProcVertexBuffer;
            const FVector N=FVector::CrossProduct(V[I+1].Position-V[I].Position,V[I+2].Position-V[I].Position).GetSafeNormal();
            if (!N.IsNearlyZero()) for(int32 J=0;J<4;++J) V[I+J].Normal=N;
        }
        return Copy;
    }
}
