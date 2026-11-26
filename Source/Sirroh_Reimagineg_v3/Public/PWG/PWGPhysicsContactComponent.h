#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PWGPhysicsContactComponent.generated.h"
UCLASS(ClassGroup=(PWG))
class SIRROH_REIMAGINEG_V3_API UPWGPhysicsContactComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UPWGPhysicsContactComponent();
 virtual void BeginPlay() override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 virtual void TickComponent(float Dt,ELevelTick Type,FActorComponentTickFunction* Tick) override;
 UPROPERTY(EditAnywhere,Category="Contact",meta=(ClampMin="1")) float PlayerMassKg=80;
 UPROPERTY(EditAnywhere,Category="Contact",meta=(ClampMin="1")) float MaxContactSpeed=1500;
 static double InelasticImpulse(double ClosingSpeed,double PlayerMass,double ObjectMass);
 /** Ground locomotion absorbs the remaining impact; props get a small nudge. */
 static double NudgeImpulse(double ClosingSpeed,double PlayerMass,double ObjectMass,double BudgetSeconds);
private:
 UFUNCTION() void OnHit(UPrimitiveComponent* HitComponent,AActor* OtherActor,UPrimitiveComponent* OtherComponent,FVector NormalImpulse,const FHitResult& Hit);
 TWeakObjectPtr<UPrimitiveComponent> Capsule;
 TMap<TWeakObjectPtr<UPrimitiveComponent>,uint64> LastContactFrame;
 TMap<TWeakObjectPtr<UPrimitiveComponent>,double> LastContactTime;
 TMap<TWeakObjectPtr<UPrimitiveComponent>,ECollisionEnabled::Type> KinematicColliders;
 TMap<TWeakObjectPtr<UPrimitiveComponent>,TPair<ECollisionResponse,ECollisionResponse>> AnimatedResponses;
 void RestoreCollisionPolicy();
 ECollisionEnabled::Type OriginalCollision=ECollisionEnabled::QueryAndPhysics;
};
