#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "POIPendulum.generated.h"
class UStaticMeshComponent;
class UPhysicsConstraintComponent;
UCLASS()
class SIRROH_REIMAGINEG_V3_API APOIPendulum : public AActor
{
 GENERATED_BODY()
public:
 APOIPendulum();
 virtual void OnConstruction(const FTransform& Transform) override;
 virtual void BeginPlay() override;
 virtual void Tick(float Dt) override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
 UPROPERTY(EditAnywhere,Category="Pendulum",meta=(ClampMin="100",ClampMax="1000")) float Length=300;
 UPROPERTY(EditAnywhere,Category="Pendulum",meta=(ClampMin="0",ClampMax="200")) float InitialSpeed=100;
 UPROPERTY(EditAnywhere,Category="Pendulum") bool bTestMotor=false;
 UPROPERTY(EditAnywhere,Category="Pendulum",meta=(ClampMin="0",ClampMax="100")) float TestForceNewtons=15;
 UPROPERTY(EditAnywhere,Category="Pendulum") TSubclassOf<AActor> PayloadClass;
 UPROPERTY(EditInstanceOnly,Replicated,Category="Pendulum") TObjectPtr<AActor> Payload;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Rod;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UPhysicsConstraintComponent> AnchorJoint;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UPhysicsConstraintComponent> PayloadJoint;
private:
 UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Links;
 UPROPERTY(Replicated) FTransform RodPose;
 bool bSpawnedPayload=false;
 float RepTime=0;
};
