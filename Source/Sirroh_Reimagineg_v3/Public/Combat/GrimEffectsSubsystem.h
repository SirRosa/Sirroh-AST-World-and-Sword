#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GrimEffectsSubsystem.generated.h"

class UProceduralMeshComponent;
class USoundBase;
class USoundAttenuation;
class USoundConcurrency;
class UPrimitiveComponent;

UENUM(BlueprintType)
enum class EGrimSurface : uint8 { Earth, Flesh, Stone, Metal, Wood };

/** One bounded, collision-free mesh for all local combat particles and blade ribbons. */
UCLASS()
class SIRROH_REIMAGINEG_V3_API UGrimEffectsSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void OnWorldBeginPlay(UWorld& World) override;
    virtual void Deinitialize() override;
    virtual void Tick(float Delta) override;
    virtual TStatId GetStatId() const override;
    virtual bool IsTickable() const override;
    bool IsNear(const FVector& Point) const;
    static EGrimSurface Surface(const AActor* Actor,const UPrimitiveComponent* Component=nullptr,const class UPhysicalMaterial* Material=nullptr);
    void Impact(EGrimSurface Kind,const FVector& Point,const FVector& Direction,float Strength=1);
    void Step(EGrimSurface Kind,const FVector& Point,bool Sprint);
    void Bleed(const FVector& Point,float Strength);
    void Swing(const FVector& Point,float Strength);
    void Trail(const FVector& OldBase,const FVector& OldTip,const FVector& Base,const FVector& Tip);
private:
    struct FParticle
    {
        FVector Position,Velocity;
        FVector Corners[4];
        FLinearColor Color;
        float Age=0,Life=1,Size=1,Gravity=0;
        bool Ribbon=false;
    };
    static constexpr int32 Capacity=512;
    TArray<FParticle> Particles;
    TArray<FVector> Vertices,Normals,RibbonVertices;
    TArray<FVector2D> UVs;
    TArray<FLinearColor> Colors;
    TArray<int32> Indices;
    UPROPERTY(Transient) TObjectPtr<AActor> RenderActor;
    UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> Mesh;
    UPROPERTY(Transient) TArray<TObjectPtr<USoundBase>> Sounds;
    UPROPERTY(Transient) TObjectPtr<USoundAttenuation> Attenuation;
    UPROPERTY(Transient) TObjectPtr<USoundConcurrency> Concurrency;
    FRandomStream Random{74031};
    float Clock=0,AudioWindow=0;
    int32 AudioCount=0;
    bool bReady=false,bVisible=false;
    void Burst(EGrimSurface Kind,const FVector& Point,const FVector& Direction,int32 Count,float Strength,bool Drip=false);
    void Sound(int32 Group,const FVector& Point,float Volume,float Pitch=1);
};
