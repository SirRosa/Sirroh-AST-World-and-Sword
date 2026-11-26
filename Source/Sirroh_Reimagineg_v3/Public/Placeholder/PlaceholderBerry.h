#pragma once

#include "CoreMinimal.h"
#include "Items/SirrohWorldItem.h"
#include "PlaceholderBerry.generated.h"

class USphereComponent;
class UProceduralMeshComponent;
class UMaterialInstanceDynamic;
class UTexture2D;

UCLASS()
class SIRROH_REIMAGINEG_V3_API APlaceholderBerry : public ASirrohWorldItem
{
    GENERATED_BODY()
public:
    APlaceholderBerry();
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    virtual bool CanPickUp() const override;
    virtual void SetHighlighted(bool bEnabled) override;
    virtual void Take(AActor* Holder) override;
    virtual void Throw(const FVector& Origin, const FVector& InitialVelocity, AActor* Thrower) override;
    virtual void SetHeldLocation(const FVector& Location) override;
    virtual void UpdateInventoryBrush(FSlateBrush& Brush) const override;
    virtual float GetItemHitRadius() const override { return GetFruitHitRadius(); }
    virtual void ReceiveProjectileImpact(const FVector& Impulse,AActor* Source) override { KnockDown(Impulse,Source); }
    float GetFruitHitRadius() const { return 2.0f * FMath::Max(4.0f,Size*0.35f) * GetActorScale3D().GetAbsMax(); }
    void KnockDown(const FVector& Impulse, AActor* Source);
    UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_Appearance, Category="Placeholder|Atlas", meta=(ClampMin="0", ClampMax="63"))
    int32 AtlasFrame = 32;

    UPROPERTY(EditAnywhere, ReplicatedUsing=OnRep_Appearance, Category="Placeholder") float Size = 14.0f;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> Collision;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UProceduralMeshComponent> Visual;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UFUNCTION() void OnRep_Appearance();
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> Material;
    UPROPERTY(Transient) TObjectPtr<UTexture2D> IconTexture;
    FVector Velocity = FVector::ZeroVector;
    TWeakObjectPtr<AActor> IgnoredThrower;
    float FlightTime = 0.0f;
    float PickupDelay = 0.0f;
    TSet<TWeakObjectPtr<ASirrohWorldItem>> HitFruit;
};
