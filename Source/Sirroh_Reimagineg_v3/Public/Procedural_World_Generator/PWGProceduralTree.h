#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Procedural_World_Generator/PWGv2Data.h"
#include "PWGProceduralTree.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class APWGv2Generator;

/** Connected square bark, circular leaf clusters and a bounded planar joint solver. */
UCLASS()
class SIRROH_REIMAGINEG_V3_API APWGProceduralTree : public AActor
{
    GENERATED_BODY()
public:
    APWGProceduralTree();
    void Initialize(APWGv2Generator* Generator, FName Id, const FPWGv2TreeRow& Row,
        EPWGTreeShape Shape, int32 Seed, float Height, float Wind,
        UMaterialInterface* Bark, UMaterialInterface* Crown, UMaterialInterface* Fruit, float FruitMultiplier, const FPWGTreeSave* Saved);
    UFUNCTION(BlueprintCallable, Category="Trees")
    void DamageAt(FVector WorldPoint, float Damage, FVector Impulse);
    virtual float TakeDamage(float Damage, const FDamageEvent& Event, AController* EventInstigator, AActor* Causer) override;
    void CaptureState();
    static TArray<FPWGTreeShapeWeight> DefaultShapeWeights(FName Region, FName Biome);
    static EPWGTreeShape SelectShape(const TArray<FPWGTreeShapeWeight>& Weights, FName Region, FName Biome, FRandomStream& Random);
protected:
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    struct FSegment
    {
        int32 Parent = INDEX_NONE;
        FVector A, B, PreviousA, PreviousB, RestA, RestB, ContactA, ContactB;
        float Size = 1, Length = 1, RestAngle = 0, RelativeAngle = 0, Health = 100;
        float QuietTime = 0;
        int32 Cell = 1;
        bool Broken = false, Dynamic = false, Sleeping = false, Grounded = false;
    };
    struct FLeaf
    {
        int32 Segment = 0, ParentLeaf = INDEX_NONE, Generation = 0;
        FVector RestCentre = FVector::ZeroVector;
        FVector2D ParentOffset = FVector2D::ZeroVector;
        float Depth = 0;
        bool BehindWood = false;
        float Size = 1, Along = 0.5f, Offset = 0, Influence = 0.7f, Phase = 0;
    };
    UPROPERTY(VisibleAnywhere) TObjectPtr<UProceduralMeshComponent> Mesh;
    UPROPERTY(Transient) TObjectPtr<APWGv2Generator> Terrain;
    UPROPERTY(Transient) TObjectPtr<UPWGv2WorldSave> SaveWorldData;
    UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> TreeMaterials;
    FPWGv2TreeRow Definition;
    TArray<FSegment> Segments;
    TArray<FLeaf> Leaves;
    TArray<int32> Tips;
    FName PersistentId;
    EPWGTreeShape ShapeType = EPWGTreeShape::Ovoid;
    int32 TreeSeed = 0;
    float TreeHeight = 600, BaseSize = 60, WindStrength = 1, Phase = 0, Accumulator = 0;
    float HitTime = -100, HitAmplitude = 0, NextGroundBreakTime = 0;
    FVector HitPosition = FVector::ZeroVector;
    int32 GroundBreakCount = 0;
    void UpdateDepthLighting();
    bool Dirty = false, MeshCreated = false, HasFruit = false;
    void Generate();
    void PoseWind(float Time);
    void Simulate(float Step);
    void UpdateMesh();
    void UpdateSurfaces();
    void BreakJoint(int32 Index, const FVector& Impulse, bool GroundImpact = false);
    int32 FragmentRoot(int32 Index) const;
    void WakeFragment(int32 Index, const FVector& Impulse);
    float GroundHeight(const FVector& Point) const;
};
