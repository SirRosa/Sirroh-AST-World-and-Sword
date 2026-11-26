#pragma once
#include "CoreMinimal.h"
#include "DragonIKWeaponPhysicsComponent.h"
#include "Combat/CombatWeaponDefinition.h"
#include "Combat/CombatMotionTypes.h"
#include "CombatDragonWeaponAdapter.generated.h"

class UBoxComponent;

/** Body-driven use of Dragon's physical handle. No WeaponArmSolver or target-from-cursor. */
UCLASS()
class UCombatDragonWeaponAdapter : public UDragonIKWeaponPhysicsComponent
{
    GENERATED_BODY()
public:
    UCombatDragonWeaponAdapter();
    bool Start(USkeletalMeshComponent& Mesh, const FCombatWeaponDefinition& Definition, const FTransform& WeaponWorld);
    void Stop();
    void ApplyBodyContact(const FVector& Impulse, const FVector& Point);
    bool ReadWeapon(FTransform& Out) const;
    void DriveGrip(const FTransform& Grip, uint32 AttackId, float Compliance=0, float Damping=1.5f);
    bool ReadMotion(FVector& Linear, FVector& Angular, FVector& Centre) const;
    bool ConsumeContact(FCombatPhysicalContact& Out);
    virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
private:
    UPROPERTY() TObjectPtr<UBoxComponent> Proxy;
    FTransform ProxyLocal;
    FGuid WeaponId;
    uint32 CurrentAttackId = 0;
    bool bPendingContact = false;
    FCombatPhysicalContact PendingContact;
    TMap<TWeakObjectPtr<UPrimitiveComponent>, ECollisionResponse> OwnerResponses;
    UFUNCTION() void OnWeaponHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit);
};
