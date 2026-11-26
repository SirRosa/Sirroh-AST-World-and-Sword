#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Combat/CombatMotionTypes.h"
#include "CombatMotionContactSubsystem.generated.h"
class UDirectionalCombatComponent;
class ACombatDefenseProjectile;

struct FCombatMotionBodyCapsule
{
    FVector A=FVector::ZeroVector, B=FVector::ZeroVector;
    FName Region;
    float Radius=0;
    bool bValid=false;
};
struct FCombatMotionContactSnapshot
{
    double Time=0;
    FTransform Root;
    FCombatWeaponSample Weapon;
    bool bWeapon=false;
    bool bKicking=false;
    uint32 ActionId=0, LifeId=0;
    TArray<FCombatMotionBodyCapsule> Body;
};
struct FCombatMotionContactTrack
{
    TWeakObjectPtr<UDirectionalCombatComponent> Combat;
    FCombatMotionContactSnapshot Previous, Current;
    bool bHistory=false;
    FBox WeaponBounds=FBox(ForceInit), BodyBounds=FBox(ForceInit);
    uint32 AttackId=0, Epoch=0;
    int32 LastStrikePhase=0;
    FGuid WeaponId;
    TSet<TWeakObjectPtr<AActor>> HitTargets;
    TSet<TWeakObjectPtr<AActor>> KickTargets;
};

/** V3 only. Runs after all actors publish their final physical weapon and body pose. */
UCLASS()
class UCombatMotionContactSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    void RegisterCombatant(UDirectionalCombatComponent* Combat);
    void UnregisterCombatant(UDirectionalCombatComponent* Combat);
    void CommitAttack(UDirectionalCombatComponent* Combat);
    void RegisterProjectile(ACombatDefenseProjectile* Projectile);
    void CancelProjectile(ACombatDefenseProjectile* Projectile);
    void QueueProjectileHit(ACombatDefenseProjectile* Projectile,const FCombatMotionBodyHit& Hit);
private:
    void AfterActors(UWorld* World,ELevelTick Tick,float Delta);
    void AdvanceDefenses(double Time);
    FCombatMotionContactSnapshot Capture(UDirectionalCombatComponent& Combat) const;
    FDelegateHandle TickHandle;
    TArray<FCombatMotionContactTrack> Tracks;
    TArray<TWeakObjectPtr<ACombatDefenseProjectile>> Projectiles;
    struct FProjectileHit { TWeakObjectPtr<ACombatDefenseProjectile> Projectile; FCombatMotionBodyHit Hit; };
    TArray<FProjectileHit> ProjectileHits;
};
