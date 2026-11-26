#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Combat/CombatAttackTypes.h"
#include "CombatTrainingNPC.generated.h"


class UCapsuleComponent;
class UTextRenderComponent;
class USkeletalMeshComponent;
class USkeletalMesh;
class UDirectionalCombatComponent;
class APWGv2Generator;


/** Deliberately primitive, server-driven sparring pawn. No behavior tree or Blueprint required. */
UCLASS()
class SIRROH_REIMAGINEG_V3_API ACombatTrainingNPC : public APawn
{
    GENERATED_BODY()
public:
    ACombatTrainingNPC();
    void QueueCombatDisplacement(const FVector& Delta, float Seconds, bool bHit);
    void QueueCombatPath(const TArray<FVector>& Path,float Seconds);
    void CancelCombatApproach();
    void ConfigureFromPlayer(APawn* Player,APWGv2Generator* Ground);
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UCapsuleComponent> Capsule;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USkeletalMeshComponent> Mesh;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UDirectionalCombatComponent> Combat;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Label;
    UPROPERTY(EditAnywhere,Category="Training") float WalkSpeed=65;
    UPROPERTY(EditAnywhere,Category="Training") float AggroRange=500;
    UPROPERTY(EditAnywhere,Category="Training") float HomeRadius=650;
    UPROPERTY(EditAnywhere,Category="Training") float AttackDistance=145;
protected:
    virtual void BeginPlay() override;
private:
    UPROPERTY(ReplicatedUsing=OnRep_Appearance) TObjectPtr<USkeletalMesh> VisualMesh;
    UPROPERTY(ReplicatedUsing=OnRep_Appearance) TObjectPtr<class UPhysicsAsset> RagdollPhysicsAsset;
    UPROPERTY(ReplicatedUsing=OnRep_Appearance) FTransform MeshOffset;
    UPROPERTY(ReplicatedUsing=OnRep_Appearance) FVector2D CapsuleSize=FVector2D(30,90);
    UFUNCTION() void OnRep_Appearance();
    TWeakObjectPtr<APWGv2Generator> GroundProvider;
    TWeakObjectPtr<APawn> Target;
    FVector Home=FVector::ZeroVector;
    FVector Knockback=FVector::ZeroVector;
    double NextDecision=0,NextTargetSearch=0,RearmAt=0;
    int32 Cycle=0;
    enum class EIntent:uint8 { Guard,Raise,Prepare,Wait };
    EIntent Intent=EIntent::Guard;
    EDirectionalAttack AttackType=EDirectionalAttack::Light;
    void Think(double Now);
    void MoveOnGround(float Delta,const FVector& Direction);
    FVector CombatDisplacement=FVector::ZeroVector;
    TArray<FVector> CombatRootPath;
    float CombatMoveElapsed=0,CombatMoveDuration=0;
    bool bCombatMoveHit=false;
    void AddTrainingKnockback(const FVector& Velocity);
};