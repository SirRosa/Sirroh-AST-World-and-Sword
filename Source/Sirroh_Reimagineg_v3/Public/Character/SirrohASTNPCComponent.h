#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Character/SirrohCharacterProfile.h"
#include "SirrohASTNPCComponent.generated.h"

/** AST owns perception, navigation and attack decisions; Sirroh owns physical combat and wounds. */
UCLASS(ClassGroup=(Character),meta=(BlueprintSpawnableComponent))
class SIRROH_REIMAGINEG_V3_API USirrohASTNPCComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    USirrohASTNPCComponent();
    UPROPERTY(EditAnywhere,Replicated,SaveGame,Category="NPC") int32 SpawnSeed=0;
    static FSirrohCharacterProfile GenerateProfile(int32 Seed);
    bool TryNearbyWeaponPickup();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    UFUNCTION(BlueprintCallable) static bool RouteAttack(UObject* SourceContext);
    UFUNCTION(BlueprintCallable) static bool RouteDamage(UObject* SourceContext,double Damage,FName BoneName,FVector ShotFromDirection,AActor* DamageCauser);
    UFUNCTION(BlueprintCallable) static bool OwnsCombat(UObject* SourceContext);
    virtual void TickComponent(float Delta,ELevelTick Tick,FActorComponentTickFunction* Function) override;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
    static USirrohASTNPCComponent* Find(UObject* Context);
    void RequestAttack();
    void SyncHealth();
    bool UpdateDistanceSleep(class AAIController* AI);
    void RestorePerception();
    TWeakObjectPtr<class UAIPerceptionComponent> SleepingPerception;
    TArray<TSubclassOf<class UAISense>> SuspendedSenses;
    bool bDistanceSleeping=false;
    bool bHiddenByDistance=false;
    double NextDistanceCheck=0;
    void UpdateWeaponRecovery();
    void CancelWeaponRecovery(bool bReleaseReservation=true);
    TWeakObjectPtr<class AProceduralSword> RecoveryWeapon;
    double RecoveryDeadline=0,RecoveryStarted=0;
    void ReleaseASTToken();
    UPROPERTY() TObjectPtr<class UDirectionalCombatComponent> Combat;
    UPROPERTY() TObjectPtr<class UCombatVitalityComponent> Vitality;
    UPROPERTY() TObjectPtr<AActor> PatrolPath;
    bool bInitialized=false,bBrainPaused=false;
    double NextWeaponSearch=0;
    double ReleaseAt=-1;
    double TokenReleaseAt=-1;
    bool bAttackInFlight=false;
    TWeakObjectPtr<AActor> HeldTokenTarget;
};
