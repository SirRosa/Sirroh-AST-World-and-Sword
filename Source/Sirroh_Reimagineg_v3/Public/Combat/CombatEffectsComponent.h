#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/CombatAttackTypes.h"
#include "Combat/CombatMotionTypes.h"
#include "CombatEffectsComponent.generated.h"
class UDirectionalCombatComponent;
class UGrimEffectsSubsystem;
class UDragonIKFootStepsComponent;
class UCombatVitalityComponent;
class UMoverComponent;

/** Local presentation only; damage and movement remain authoritative elsewhere. */
UCLASS()
class SIRROH_REIMAGINEG_V3_API UCombatEffectsComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UCombatEffectsComponent();
    virtual void TickComponent(float Delta,ELevelTick Tick,FActorComponentTickFunction* Function) override;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
    UPROPERTY(Transient) TObjectPtr<UDirectionalCombatComponent> Combat;
    UPROPERTY(Transient) TObjectPtr<UGrimEffectsSubsystem> Effects;
    UPROPERTY(Transient) TObjectPtr<UDragonIKFootStepsComponent> Feet;
    UPROPERTY(Transient) TObjectPtr<UCombatVitalityComponent> Vitality;
    UPROPERTY(Transient) TObjectPtr<UMoverComponent> Mover;
    uint32 SwingId=0;
    bool bTrailHistory=false,bLeftFoot=false;
    FVector OldBase=FVector::ZeroVector,OldTip=FVector::ZeroVector,PreviousPosition=FVector::ZeroVector;
    float StepDistance=0,BleedClock=0,MotionClock=0,LastStep=-100,LastDragonStep=-100,LastImpact=-100;
    FVector LastImpactPoint=FVector::ZeroVector;
    void Contact(const FCombatContactEvent& Event);
    bool AcceptImpact(const FVector& Point);
    void Movement(float Delta);
    UFUNCTION() void PhysicalContact(const FCombatPhysicalContact& Event);
    UFUNCTION() void BodyContact(const FCombatMotionBodyHit& Event);
    UFUNCTION() void FootContact(FName Bone,FTransform Transform,float Height,FHitResult Hit);
};
