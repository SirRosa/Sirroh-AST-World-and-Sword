#pragma once
#include "CoreMinimal.h"
#include "Items/SirrohWorldItem.h"
#include "Sword/SwordTypes.h"
#include "Combat/CombatWeaponDefinition.h"
#include "ProceduralSword.generated.h"

class UProceduralMeshComponent;
class UMaterialInstanceDynamic;
class UBoxComponent;

UCLASS(BlueprintType)
class SIRROH_REIMAGINEG_V3_API AProceduralSword : public ASirrohWorldItem
{
    GENERATED_BODY()
public:
    AProceduralSword();
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    virtual void SetHighlighted(bool bEnabled) override;
    virtual void Take(AActor* Holder) override;
    virtual void Throw(const FVector& Origin,const FVector& InitialVelocity,AActor* Thrower) override;
    virtual void SetHeldTransform(const FTransform& Transform) override;
    /** Combat frame: +X points along blade, +Y is the card normal. No camera projection. */
    virtual void SetCombatTransform(const FTransform& Transform);
    /** V3 metadata in actor-local space; callable without materials, rendering or BeginPlay. */
    virtual FCombatWeaponDefinition GetCombatDefinition() const;
    static FCombatWeaponDefinition BuildCombatDefinition(const FSwordInstance& Instance, float PixelScale);
    /** Inert proxy until the single V3 weapon adapter explicitly takes ownership. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Sword|Combat") TObjectPtr<UBoxComponent> CombatPhysicsProxy;
    virtual FName HandSocket(int32 Slot) const override { return Slot==0?TEXT("Weapon_R"):TEXT("Weapon_L"); }
    virtual void UpdateInventoryBrush(FSlateBrush& Brush) const override;
    virtual float GetItemHitRadius() const override { return 12.0f; }
    virtual void ReceiveProjectileImpact(const FVector& Impulse,AActor* Source) override;
    void Embed(const FVector& Point,const FVector& Direction,AActor* Support=nullptr);
    UPROPERTY(EditAnywhere,BlueprintReadOnly,SaveGame,ReplicatedUsing=OnRep_Definition,Category="Sword") FSwordInstance Definition;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,ReplicatedUsing=OnRep_CombatScale,Category="Sword|Assembly",meta=(ClampMin="0.05")) float WorldUnitsPerPixel=0.30f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Sword|Assembly") FRotator SocketRotationOffset=FRotator::ZeroRotator;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Sword|Flight") float SpinDegreesPerSecond=720;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Sword|Flight") float EmbedDepth=10;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UProceduralMeshComponent> Visual;
protected:
    virtual void BeginPlay() override;
private:
    UFUNCTION() void OnRep_Definition();
    UFUNCTION() void OnRep_CombatScale();
    void RefreshCombatProxy();
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> Material;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> InventoryMaterial;
    FVector Velocity=FVector::ZeroVector;
    TWeakObjectPtr<AActor> IgnoredActor;
    float FlightTime=0;
    float Angle=0;
    void ApplyRecipe(UMaterialInstanceDynamic* MID);
    FVector BladeTop(FVector Position,FQuat Rotation) const { return Position+Rotation.RotateVector(FVector(0,0,16*WorldUnitsPerPixel)); }
    FVector BladeTip(FVector Position,FQuat Rotation) const { return Position+Rotation.RotateVector(FVector(0,0,-368*WorldUnitsPerPixel)); }
};
