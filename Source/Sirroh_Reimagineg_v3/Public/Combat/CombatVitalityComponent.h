#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/CombatMotionTypes.h"
#include "Combat/CombatBodySolver.h"
#include "CombatVitalityComponent.generated.h"

UENUM(BlueprintType)
enum class ECombatLifeState : uint8 { Conscious, Unconscious, Dead };

USTRUCT()
struct FCombatRagdollKey
{
    GENERATED_BODY()
    UPROPERTY() FName Bone;
    UPROPERTY() FRotator Rotation=FRotator::ZeroRotator;
};

USTRUCT()
struct FCombatRagdollFrame
{
    GENERATED_BODY()
    UPROPERTY() uint32 Life=0;
    UPROPERTY() float Time=0;
    UPROPERTY() FVector_NetQuantize10 Pelvis=FVector::ZeroVector;
    UPROPERTY() TArray<FCombatRagdollKey> Bodies;
};

USTRUCT(BlueprintType)
struct FCombatBodyPartHealth
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FName Bone;
    UPROPERTY(BlueprintReadOnly) int32 Parent=INDEX_NONE;
    UPROPERTY(BlueprintReadOnly) float Maximum=40;
    UPROPERTY(BlueprintReadOnly) float Integrity=40;
    UPROPERTY(BlueprintReadOnly) float Bleeding=0;
    UPROPERTY(BlueprintReadOnly) bool bDestroyed=false;
    UPROPERTY(BlueprintReadOnly) bool bSevered=false;
    UPROPERTY(BlueprintReadOnly) float Regrowth=0;
};

/** Authority owns wounds. Rendering and physical reactions consume state, never author damage. */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class SIRROH_REIMAGINEG_V3_API UCombatVitalityComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UCombatVitalityComponent();
    static UCombatVitalityComponent* FindOrAdd(AActor* Owner);
    virtual void TickComponent(float Delta,ELevelTick Tick,FActorComponentTickFunction* Function) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    float ApplyContact(const FCombatMotionBodyHit& Hit,float WeaponMass);
    UFUNCTION(BlueprintCallable,Category="Health") void ResetVitality();
    UFUNCTION(BlueprintPure,Category="Health") float GetBleeding() const;
    UFUNCTION(BlueprintPure,Category="Health") bool IsIncapacitated() const;
    bool UsesRagdollPose() const;
    bool IsRecoveryBlocked() const;
    bool CanUseArm(bool bLeft) const;
    bool IsRegionPresent(FName Bone) const;
    FVector GetTargetPoint() const;
    void BlendDeathPose(class UPoseableMeshComponent& Visual);
    void ApplyNetworkRagdoll(class UPoseableMeshComponent& Visual);
    void QueueRagdollImpulse(const FVector& Impulse);
    UPROPERTY(Replicated,BlueprintReadOnly,Category="Health") float Blood=500;
    UPROPERTY(ReplicatedUsing=OnRep_State,BlueprintReadOnly,Category="Health") ECombatLifeState LifeState=ECombatLifeState::Conscious;
    UPROPERTY(ReplicatedUsing=OnRep_Parts,BlueprintReadOnly,Category="Health") TArray<FCombatBodyPartHealth> Parts;
    UPROPERTY(Replicated) uint32 LifeId=1;
    UPROPERTY(Replicated) FName DeathAnimationRow;
    UPROPERTY(Replicated) float DeathServerTime=-1;
    UPROPERTY(ReplicatedUsing=OnRep_State) float KnockdownUntil=0;
    UPROPERTY(ReplicatedUsing=OnRep_Ragdoll) FCombatRagdollFrame RagdollFrame;
    UPROPERTY(EditAnywhere,Category="Health",meta=(ClampMin="1")) float UnarmedKnockdownThreshold=30;
    UPROPERTY(EditAnywhere,Category="Health",meta=(ClampMin="0.1")) float UnarmedComboWindow=2;
    UPROPERTY(EditAnywhere,Category="Health",meta=(ClampMin="1")) float KnockdownDamage=25;
    UPROPERTY(EditAnywhere,Category="Health",meta=(ClampMin="0.1")) float KnockdownSeconds=1.5f;
    UPROPERTY(EditAnywhere,Replicated,BlueprintReadWrite,Category="Health",meta=(ClampMin="1")) float MaxBlood=500;
    UPROPERTY(EditAnywhere,Category="Health",meta=(ClampMin="0")) float WoundClosurePerSecond=1;
    UPROPERTY(EditAnywhere,Category="Health",meta=(ClampMin="0")) float MovingClosureMultiplier=0.5f;
    UPROPERTY(EditAnywhere,Category="Health",meta=(ClampMin="0")) float BloodRecoveryPerSecond=1;
    UPROPERTY(EditAnywhere,Category="Health",meta=(ClampMin="0")) float CutBleedPerDamage=0.25f;
    UPROPERTY(EditAnywhere,Category="Health",meta=(ClampMin="1")) float ThrustBleedMultiplier=1.5f;
    UPROPERTY(EditAnywhere,Category="Health",meta=(ClampMin="1")) float RegrowthSeconds=60;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
    UFUNCTION() void OnRep_State();
    UFUNCTION() void OnRep_Parts();
    UFUNCTION() void OnRep_Ragdoll();
    float ServerTime() const;
    void CaptureNetworkRagdoll();
    FCombatRagdollFrame PreviousRagdollFrame,ReceivedRagdollFrame;
    void UpdateLifeState();
    void StepWounds();
    void UpdatePresentation();
    void CreateHUD();
    int32 FindPart(FName Bone) const;
    bool IsDescendant(int32 Child,int32 Parent) const;
    void DestroyPart(int32 Index,bool bSever);
    UPROPERTY(Transient) TObjectPtr<class USkeletalMeshComponent> Mesh;
    FTransform MeshRelative;
    FName MeshCollisionProfile;
    FVector PreviousPosition=FVector::ZeroVector;
    float WoundClock=0,MovingSeconds=0;
    double DeathTime=-1;
    bool bDownApplied=false,bFallbackPhysics=false,bDeathApplied=false;
    TMap<FName,TEnumAsByte<ECollisionEnabled::Type>> DisabledBodies;
    bool bSupported=false;
    bool bRecovering=false;
    double RecoveryStartedAt=-1;
    FCombatBodyRig DeathRig;
    FName LoadedDeathAnimation;
    FTransform DeathAnchor;
    TMap<FName,FTransform> PendingPhysicalPose;
    FVector PendingImpulse=FVector::ZeroVector;
    float PendingImpulseUntil=0;
    void SelectDeathAnimation();
    void SelectImpactAnimation(FName Action);
    FVector LastIncoming=FVector::ForwardVector;
    FName LastRegion=TEXT("Torso");
    int32 LastHands=1;
    float LastImpactStrength=0;
    bool bLastPierce=false,bLastCut=false;
    double LastImpactTime=-100;
    float UnarmedStagger=0;
    double LastUnarmedImpact=-100;

    TSharedPtr<class SWidget> HUD;
};
