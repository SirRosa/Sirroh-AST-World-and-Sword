#pragma once
#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include "Character/SirrohCharacterProfile.h"
#include "SirrohProceduralHairComponent.generated.h"
class USkeletalMeshComponent;
class UMaterialInstanceDynamic;
UCLASS(ClassGroup=(Character))
class SIRROH_REIMAGINEG_V3_API USirrohProceduralHairComponent : public UProceduralMeshComponent
{
    GENERATED_BODY()
public:
    USirrohProceduralHairComponent(const FObjectInitializer& ObjectInitializer);
    UFUNCTION(BlueprintCallable, Category="Hair")
    void Configure(USkeletalMeshComponent* InMesh, const FSirrohCharacterProfile& Profile);
    UFUNCTION(BlueprintCallable, Category="Hair")
    static USirrohProceduralHairComponent* CreateForMesh(USkeletalMeshComponent* InMesh, const FSirrohCharacterProfile& Profile);
    virtual void TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction) override;
private:
    struct FLock { TArray<FVector> Rest,Current,Previous,GroomTargets; TArray<float> SegmentLengths,FreeRadii; FVector Width, RootShift=FVector::ZeroVector, Bary=FVector::ZeroVector; int32 RootTriangle=0; FLinearColor Color; int32 Pinned=1; float CurlPhase=0, CurlRadius=0, CurlFrequency=0; };
    TArray<FLock> Locks;
    TArray<FTransform> VisiblePose,InverseReferencePose;
    int32 HeadBone=INDEX_NONE;
    void RefreshVisiblePose();
    FVector SkinScalpPoint(int32 Index,const FVector& Point) const;
    TArray<float> ScalpCollisionRadii;
    float MaxCollisionRadius=0.f;
    ESirrohSex CollisionSex=ESirrohSex::Male;
    void BuildScalpCollision();
    FVector ProjectOutsideScalp(const FVector& Point,const FTransform& Head,float Margin) const;
    TArray<FVector> Scalp,Vertices,Normals;
    TArray<int32> ScalpIndices,Triangles;
    TArray<FVector2D> UV;
    TArray<FLinearColor> Colors;
    TArray<FProcMeshTangent> Tangents;
    UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> Source;
    UPROPERTY() TObjectPtr<UMaterialInterface> HairMaterial;
    FTransform RefHead,LastHead;
    FSirrohCharacterProfile Settings;
    int32 RenderStride=1;
    float Accumulator=0;
    bool bReady=false;
    FTransform CurrentHead() const;
    void Rebuild();
    void UpdateGeometry(const FTransform& Head,bool bCreate);
    FVector Target(const FVector& P,const FTransform& Head) const;
};
