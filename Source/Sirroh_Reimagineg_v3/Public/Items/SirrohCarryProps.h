#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Items/SirrohWorldItem.h"
#include "Sword/ProceduralSword.h"
#include "SirrohCarryProps.generated.h"

class UAnimSequence;
class UStaticMesh;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class UASTFireReactiveComponent;

/** Imported GASPALS overlay poses, composed over AST locomotion by the existing pose bridge. */
UCLASS(ClassGroup=(Inventory), meta=(BlueprintSpawnableComponent))
class SIRROH_REIMAGINEG_V3_API USirrohCarryPoseComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, Category="Carry|Overlay") TObjectPtr<UAnimSequence> StandingIdle;
    UPROPERTY(EditAnywhere, Category="Carry|Overlay") TObjectPtr<UAnimSequence> StandingMove;
    UPROPERTY(EditAnywhere, Category="Carry|Overlay") TObjectPtr<UAnimSequence> Crouching;
    UPROPERTY(EditAnywhere, Category="Carry|Overlay") FName GripBone=TEXT("hand_r");
    UPROPERTY(EditAnywhere, Category="Carry|Overlay") FTransform GripOffset;
    UPROPERTY(EditAnywhere, Category="Carry|Overlay") bool bTwoHanded=false;
    UPROPERTY(EditAnywhere, Category="Carry|Overlay") bool bMirrorPose=false;
    UPROPERTY(EditAnywhere, Category="Carry|Overlay") float BlendSeconds=.2f;
    void SetTorchHand(int32 Slot);
    void ApplyPose(USkeletalMeshComponent* Mesh,TMap<FName,FTransform>& Pose,float Delta);
    UFUNCTION(BlueprintCallable, Category="Carry|Setup") bool ConfigureImportedPose(const FString& Kind);
    UFUNCTION(BlueprintCallable, Category="Carry|Setup") static bool PrepareImportedMesh(UStaticMesh* Mesh);
    FTransform HeldTransform(const FTransform& Hand) const { return GripOffset*Hand; }
    int32 HandSlot() const { return GripBone.ToString().EndsWith(TEXT("_l"))?1:0; }
private:
    float Alpha=0;
    TWeakObjectPtr<UAnimSequence> CachedMirrorSequence;
    TArray<FTransform> MirrorPoseWorld,MirrorRefWorld;
};

/** A real one-handed combat weapon using AST's native Loot and Q throw path. */
UCLASS(Blueprintable)
class SIRROH_REIMAGINEG_V3_API ASirrohTorch : public AProceduralSword
{
    GENERATED_BODY()
public:
    ASirrohTorch();
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void Tick(float Delta) override;
    virtual void Take(AActor* Holder) override;
    virtual void Throw(const FVector& Origin,const FVector& Velocity,AActor* Thrower) override;
    virtual void SetHeldTransform(const FTransform& Transform) override;
    virtual void SetCombatTransform(const FTransform& Transform) override;
    virtual FCombatWeaponDefinition GetCombatDefinition() const override;
    virtual FName HandSocket(int32 Slot) const override;
    virtual void SetHighlighted(bool Enabled) override;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> TorchMesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USirrohCarryPoseComponent> CarryPose;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> FlameHead;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UASTFireReactiveComponent> Fire;
    UPROPERTY(EditAnywhere, Category="Torch|Fire") FVector FlameTip=FVector(0,0,47);
protected:
    virtual void BeginPlay() override;
    virtual void OnRep_HeldState() override;
};

/** Props are loot, not weapons. Box uses two hands; barrel uses the authored left-arm carry. */
UCLASS(Blueprintable)
class SIRROH_REIMAGINEG_V3_API ASirrohCarryProp : public ASirrohWorldItem
{
    GENERATED_BODY()
public:
    ASirrohCarryProp();
    virtual void Tick(float Delta) override;
    virtual void Take(AActor* Holder) override;
    virtual void Throw(const FVector& Origin,const FVector& Velocity,AActor* Thrower) override;
    virtual void SetHeldTransform(const FTransform& Transform) override;
    virtual FName HandSocket(int32 Slot) const override;
    virtual void SetHighlighted(bool Enabled) override;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> PropMesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USirrohCarryPoseComponent> CarryPose;
    UPROPERTY(EditAnywhere, Category="Carry") float MassKg=12;
protected:
    virtual void BeginPlay() override;
    virtual void OnRep_HeldState() override;
};
