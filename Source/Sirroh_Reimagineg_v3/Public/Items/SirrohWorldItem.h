#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Styling/SlateBrush.h"
#include "SirrohWorldItem.generated.h"

/** Small pickup contract. The sword generator does not depend on the placeholder inventory. */
UCLASS(Abstract)
class SIRROH_REIMAGINEG_V3_API ASirrohWorldItem : public AActor
{
    GENERATED_BODY()
public:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    void SetPickupPromptVisible(bool bVisible);
    UPROPERTY(Transient) TObjectPtr<class UWidgetComponent> PickupPrompt;
    virtual bool CanPickUp() const { return !bHeld && !bFlying; }
    virtual void SetHighlighted(bool bEnabled) {}
    UFUNCTION(BlueprintPure, Category="Item|Pickup") bool CanPickUpBy(AActor* Holder) const;
    UFUNCTION(BlueprintCallable, Category="Item|Pickup") bool ReservePickup(AActor* Holder, float Duration=3.f);
    UFUNCTION(BlueprintCallable, Category="Item|Pickup") void ReleasePickup(AActor* Holder);
    UFUNCTION(BlueprintCallable, Category="Item|Pickup") bool CommitPickup(AActor* Holder);
    virtual void Take(AActor* Holder);
    virtual void Throw(const FVector& Origin,const FVector& InitialVelocity,AActor* Thrower) {}
    virtual void SetHeldLocation(const FVector& Location) { SetActorLocation(Location); }
    virtual void SetHeldTransform(const FTransform& Transform) { SetHeldLocation(Transform.GetLocation()); }
    virtual FName HandSocket(int32 Slot) const { return NAME_None; }
    virtual void UpdateInventoryBrush(FSlateBrush& Brush) const {}
    virtual float GetItemHitRadius() const { return 8.0f; }
    virtual void ReceiveProjectileImpact(const FVector& Impulse,AActor* Source) {}
    bool IsHeld() const { return bHeld; }
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Replicated, Category="Item") FString PersistentId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Replicated, Category="Item") float LaneY=0;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    void MarkDisplaced();
    // A short-lived server lease, not inventory ownership or a saved item copy.
    TWeakObjectPtr<AActor> PickupReservation;
    double PickupReservationUntil=0;
    UFUNCTION() virtual void OnRep_HeldState() {}
    UPROPERTY(ReplicatedUsing=OnRep_HeldState) bool bHeld=false;
    UPROPERTY(Replicated) bool bFlying=false;
};
