#include "POI/POITerrain.h"

bool FPOITerrainStamp::ValidateAndSort(TArray<FPOITerrainStamp>& Stamps,FString& Error)
{
    if (Stamps.Num()>64) { Error=TEXT("Prototype supports at most 64 terrain stamps per world."); return false; }
    TSet<FName> Ids;
    for (const auto& S:Stamps)
    {
        if (S.Id.IsNone() || Ids.Contains(S.Id) || !FMath::IsFinite(S.Center.X) || !FMath::IsFinite(S.Center.Y) ||
            !FMath::IsFinite(S.HalfExtent.X) || !FMath::IsFinite(S.HalfExtent.Y) || S.HalfExtent.X<1 || S.HalfExtent.Y<1 ||
            !FMath::IsFinite(S.BlendDistance) || S.BlendDistance<1 || !FMath::IsFinite(S.Height) ||
            (S.Operation!=EPOITerrainOperation::Flatten && S.Operation!=EPOITerrainOperation::Offset))
        { Error=TEXT("Terrain stamps require unique IDs, finite dimensions and positive extents/blend."); return false; }
        Ids.Add(S.Id);
    }
    Stamps.Sort([](const auto& A,const auto& B){return A.Priority==B.Priority?A.Id.LexicalLess(B.Id):A.Priority<B.Priority;});
    Error.Reset(); return true;
}

float FPOITerrainStamp::Apply(const TArray<FPOITerrainStamp>& Stamps,float X,float Y,float Base)
{
    float Result=Base;
    for (const auto& S:Stamps)
    {
        const double DX=FMath::Max(0.0,FMath::Abs(X-S.Center.X)-S.HalfExtent.X);
        const double DY=FMath::Max(0.0,FMath::Abs(Y-S.Center.Y)-S.HalfExtent.Y);
        const float T=FMath::Clamp(float(FMath::Sqrt(DX*DX+DY*DY)/FMath::Max(1.f,S.BlendDistance)),0.f,1.f);
        const float Alpha=1-T*T*(3-2*T);
        Result=S.Operation==EPOITerrainOperation::Flatten?FMath::Lerp(Result,S.Height,Alpha):Result+S.Height*Alpha;
    }
    return Result;
}
