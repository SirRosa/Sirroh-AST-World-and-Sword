#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CombatDefenseProjectile.generated.h"
class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
/** Minimal ranged adapter: real swept collision, shared defense ledger, no bow/ammo system. */
UCLASS(Blueprintable)
class SIRROH_REIMAGINEG_V3_API ACombatDefenseProjectile : public AActor
{
    GENERATED_BODY()
public:
    ACombatDefenseProjectile();
    UFUNCTION(BlueprintCallable,Category="Combat|Ranged",meta=(WorldContext="WorldContext"))
    static ACombatDefenseProjectile* Launch(UObject* WorldContext,AActor* Shooter,FVector Origin,FVector Velocity,bool Pierce=true,float Mass=0.2f,float GravityScale=0);
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Combat|Ranged") bool bPierce=true;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Combat|Ranged") float ProjectileMass=0.2f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Combat|Ranged") float Lifetime=5;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<USphereComponent> Collision;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Visual;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UProjectileMovementComponent> Movement;
    FGuid ThreatId;
    double DefenseStopTime=DBL_MAX;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(EEndPlayReason::Type Reason) override;
    UFUNCTION() void Impact(UPrimitiveComponent* HitComponent,AActor* OtherActor,UPrimitiveComponent* OtherComponent,FVector Impulse,const FHitResult& Hit);
    bool bImpactQueued=false;
};
