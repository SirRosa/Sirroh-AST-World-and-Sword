#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SirrohASTCombatBridgeComponent.generated.h"
UENUM(BlueprintType)
enum class ESirrohEquipmentState : uint8 { Unarmed, NativeWeapon, ASTWeapon, Recovering, Dead };
/** Transient combat adapter. AST inventory and the native held weapon remain the owners of item instances. */
UCLASS()
class SIRROH_REIMAGINEG_V3_API USirrohASTCombatBridgeComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    USirrohASTCombatBridgeComponent();
    static bool TracePropFist(AActor* Attacker,FVector Start,FVector End,float Dt,TSet<TWeakObjectPtr<AActor>>& HitTargets);
    virtual void TickComponent(float Dt,ELevelTick Type,FActorComponentTickFunction* Tick) override;
    UFUNCTION(BlueprintCallable) static bool BeginASTMelee(UObject* SourceContext);
    UFUNCTION(BlueprintCallable) static bool ObserveASTMelee(UObject* SourceContext);
    UFUNCTION(BlueprintCallable) static bool RouteMeleeStrategy(UObject* SourceContext);
    UFUNCTION(BlueprintCallable) static bool SuppressDetectionHUD(UObject* SourceContext);
    UFUNCTION(BlueprintCallable) static bool PrepareDetectionArrays(UObject* SourceContext);
    UFUNCTION(BlueprintCallable) static bool ValidateDetectionTarget(UObject* SourceContext,AActor* Target,int32 TargetIndex);
    static ESirrohEquipmentState EquipmentState(AActor* Actor);
    static bool RouteDamage(UObject* Context,double Damage,FName Bone,FVector Direction,AActor* Causer);
    static AActor* ActorFor(UObject* Context);
    static UActorComponent* Component(AActor* Actor,const TCHAR* ClassPrefix);
private:
    static bool InPWG(UObject* Context);
    static USirrohASTCombatBridgeComponent* FindOrAdd(AActor* Actor);
    bool TryStartMelee(UObject* Context,class UAnimMontage* Montage);
    TWeakObjectPtr<UObject> PendingMeleeContext;
    TWeakObjectPtr<class UAnimMontage> PendingMeleeMontage;
    double PendingMeleeUntil=0;
    TWeakObjectPtr<class USkeletalMeshComponent> PunchMesh;
    FVector PreviousHands[2];
    TSet<TWeakObjectPtr<AActor>> PropHitTargets;
    double StartedAt=-100,ExpiresAt=-100;
    int32 Combo=0, MontageInstanceId=INDEX_NONE;
    TWeakObjectPtr<UObject> AttackAnimInstance;
    TSet<TWeakObjectPtr<AActor>> HitTargets;
};
