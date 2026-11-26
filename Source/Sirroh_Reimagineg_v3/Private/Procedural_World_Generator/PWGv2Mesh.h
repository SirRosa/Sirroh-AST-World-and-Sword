#pragma once
#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"

struct FPWGv2MeshBuffers
{
    TArray<FVector> V,N;
    TArray<int32> T;
    TArray<FVector2D> UV;
    TArray<FLinearColor> C;
    void Quad(FVector A,FVector B,FVector D,FVector E,FLinearColor Color,float Blend0=0,float Blend1=0)
    {
        const int32 I=V.Num(); const FVector Normal=FVector::CrossProduct(B-A,D-A).GetSafeNormal();
        V.Append({A,B,D,E}); T.Append({I,I+1,I+2,I,I+2,I+3}); N.Append({Normal,Normal,Normal,Normal});
        const bool Vertical=FMath::Abs(Normal.Z)<0.5f;
        const bool AlongY=Vertical && FMath::Abs(Normal.X)>FMath::Abs(Normal.Y);
        for (const FVector& P:{A,B,D,E}) UV.Add(FVector2D((AlongY?P.Y:P.X)/100,Vertical?-P.Z/100:P.Y/100));
        FLinearColor C0=Color,C1=Color; C0.A=Blend0; C1.A=Blend1; C.Append({C0,C1,C1,C0});
    }
    void Submit(UProceduralMeshComponent* Mesh,int32 Section)
    { Mesh->CreateMeshSection_LinearColor(Section,V,T,N,UV,C,TArray<FProcMeshTangent>(),false); }
};
