#pragma once
#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "Combat/CombatAttackTypes.h"
#include "CombatContactSubsystem.generated.h"

class UDirectionalCombatComponent;
class ASirrohWorldItem;

/** History belongs to the server; clients only evaluate their visual weapon and consume results. */
struct FCombatHistorySample
{
    double Time=0;
    FVector Origin=FVector::ZeroVector;
    FVector BodyA=FVector::ZeroVector,BodyB=FVector::ZeroVector;
    float BodyRadius=30;
    FCombatWeaponFrame Weapon;
    FCombatWeaponFrame Rest;
    FDirectionalAttackMotion Motion;
    double InterruptedAt=-DBL_MAX;
    float PixelScale=0.3f;
    bool bWeapon=false;
    bool bBlocking=false;
    ECombatGuard Guard=ECombatGuard::Front;
    bool bAttacking=false;
    uint32 AttackId=0;
};

struct FCombatContactTrack
{
    TWeakObjectPtr<UDirectionalCombatComponent> Combat;
    TArray<FCombatHistorySample> History;
    uint32 AttackId=0;
    double CheckedUntil=0;
    TSet<TWeakObjectPtr<AActor>> HitBodies;
    TSet<TWeakObjectPtr<AActor>> HitWeapons;
};

UCLASS()
class SIRROH_REIMAGINEG_V3_API UCombatContactSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    void RegisterCombatant(UDirectionalCombatComponent* Combat);
    void UnregisterCombatant(UDirectionalCombatComponent* Combat);
private:
    void AfterActors(UWorld* World,ELevelTick Tick,float Delta);
    void Capture(FCombatContactTrack& Track,double Now);
    void CollectAttack(int32 TrackIndex,double Now);
    FCombatHistorySample Sample(const FCombatContactTrack& Track,double Time) const;
    void RebuildGrid();
    TArray<FCombatContactTrack> Tracks;
    TMap<FIntPoint,TArray<int32>> DynamicGrid;
    struct FItemHistory { FVector Previous,Current; double Time=0; };
    TMap<TWeakObjectPtr<ASirrohWorldItem>,FItemHistory> ItemHistory;
    TArray<TWeakObjectPtr<ASirrohWorldItem>> Items;
    TMap<FIntPoint,TArray<int32>> ItemGrid;
    struct FPendingContact
    {
        int32 Attacker=INDEX_NONE,Defender=INDEX_NONE;
        TWeakObjectPtr<AActor> Target;
        double Time=0;
        FVector Point=FVector::ZeroVector,Direction=FVector::ZeroVector;
        float Speed=0;
        bool bWeapon=false,bBlocking=false,bBody=false;
        uint32 AttackId=0;
    };
    TArray<FPendingContact> Pending;
    void Resolve(const FPendingContact& Contact,double Now);
    FDelegateHandle TickHandle;
    static constexpr double HistorySeconds=0.25;
};
