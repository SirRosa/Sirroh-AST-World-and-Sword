#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Sword/SwordTypes.h"
#include "SwordWorldSubsystem.generated.h"

class ASirrohWorldItem;
class AProceduralSword;
class UMaterialInterface;
struct FSwordSurface
{
    TWeakObjectPtr<AActor> Owner;
    FVector A,B;
    float Radius=1;
    float LaneY=0;
    FVector PreviousA=FVector::ZeroVector,PreviousB=FVector::ZeroVector;
};

/** Session state and a spatial hash of weapon-only surfaces. Never blocks pawn movement. */
UCLASS()
class SIRROH_REIMAGINEG_V3_API USwordWorldSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    TArray<TWeakObjectPtr<ASirrohWorldItem>> Items;
    TSet<FString> RemovedFromSpawn;
    void RegisterSurface(AActor* Owner,FVector A,FVector B,float Radius,float LaneY,const FVector* PreviousA=nullptr,const FVector* PreviousB=nullptr);
    void RemoveSurfaces(AActor* Owner);
    bool FindSurface(const FVector& BladeStart,const FVector& BladeEnd,float Radius,FVector& Point,AActor*& Owner,float& LaneY);
    /** Combat broad phase: return every surface in a swept X/Z bounding box. */
    void QueryCombatSurfaces(const FVector& Min,const FVector& Max,TArray<FSwordSurface>& Out) const;
    FSwordSpawnRule RuleFor(int32 SurfaceStyle,FName Context=TEXT("SmallRock"));
    FSwordInstance Generate(int32 SurfaceStyle,int32 Seed,bool bUnrestricted=false);
    AProceduralSword* SpawnSword(const FVector& Position,float LaneY,const FString& SpawnId,int32 SurfaceStyle,int32 Seed,AActor* Owner=nullptr,bool bUnrestricted=false);
    UMaterialInterface* WorldMaterial();
    UMaterialInterface* IconMaterial();
    UFUNCTION(BlueprintCallable,Category="Swords|Save") TArray<FSwordSaveRecord> CaptureSwordRecords() const;
private:
    void LoadData();
    bool bLoaded=false;
    TMap<int32,FSwordSurface> Surfaces;
    TMap<FIntPoint,TArray<int32>> Grid;
    TMap<TWeakObjectPtr<AActor>,TArray<int32>> OwnerSurfaces;
    int32 NextSurfaceId=1;
    TArray<int32> FreeSurfaceIds;
    UPROPERTY(Transient) TObjectPtr<UDataTable> SpawnRules;
    UPROPERTY(Transient) TObjectPtr<UDataTable> CombinationRules;
    UPROPERTY(Transient) TObjectPtr<UMaterialInterface> SwordMaterial;
    UPROPERTY(Transient) TObjectPtr<UMaterialInterface> SwordIconMaterial;
};
