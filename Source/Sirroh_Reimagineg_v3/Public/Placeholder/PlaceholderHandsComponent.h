#pragma once


#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Styling/SlateBrush.h"
#include "PlaceholderHandsComponent.generated.h"


class ASirrohWorldItem;
class USirrohASTInventoryComponent;
class UPlaceholderHandsRig;
class USkeletalMeshComponent;
class USirrohPoseBridgeComponent;
class UTexture2D;
class SWidget;


UCLASS(ClassGroup=(Placeholder), meta=(BlueprintSpawnableComponent))
class SIRROH_REIMAGINEG_V3_API UPlaceholderHandsComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UPlaceholderHandsComponent();
    virtual void TickComponent(float Delta, ELevelTick Tick, FActorComponentTickFunction* Function) override;
    void HandleInput(bool bPick, bool bChargePressed, bool bChargeHeld=false, bool bChargeReleased=false);
    void CancelCharge();
    UFUNCTION(BlueprintCallable, Category="Sirroh|AST") static bool RouteASTLootIK(UObject* SourceContext);
    ASirrohWorldItem* GetPickupCandidate();
    bool RequestPickup(ASirrohWorldItem* Target);
    bool IsASTPickupActive() const { return bASTPickup; }
    UFUNCTION(BlueprintCallable, Category="Sirroh|AST") static bool RouteASTLootAttach(UObject* SourceContext);
    UFUNCTION(BlueprintCallable, Category="Sirroh|AST") static bool RouteASTLootStore(UObject* SourceContext);

    void DebugSpawnSword();
    void HandleBackInput(bool bPressed, bool bHeld, bool bReleased);
    void CancelBackInput();
    void UpdateBackItems();
    bool HasBerry(int32 Slot) const;
    ASirrohWorldItem* GetItem(int32 Slot) const;
    bool SetItem(int32 Slot,ASirrohWorldItem* Item);
    void SelectBackSlot(int32 Slot);
    void StowRightItem();
    void SwapHandItems();
    USirrohASTInventoryComponent* Inventory() const;

    FText GetHint() const;
    UPROPERTY(EditAnywhere, Category="Placeholder|Interaction", meta=(ClampMin="20")) float Reach = 145.0f;
    UPROPERTY(EditAnywhere, Category="Placeholder|Interaction", meta=(ClampMin="1")) float LaneTolerance = 60.0f;
    UPROPERTY(EditAnywhere, Category="Placeholder|Throw", meta=(ClampMin="100")) float MaxThrowDistance = 5000.0f;
    UPROPERTY(EditAnywhere, Category="Placeholder|Throw", meta=(ClampMin="0.1")) float FullChargeSeconds = 3.0f;
    UPROPERTY(EditAnywhere, Category="Placeholder|Throw", meta=(ClampMin="1")) float MinimumThrowSpeed = 350.0f;
    UPROPERTY(EditAnywhere, Category="Placeholder|Throw", meta=(ClampMin="1")) float MaximumThrowSpeed = 2200.0f;
    UPROPERTY(EditAnywhere, Category="Placeholder|Animation") bool bAnimateHands = true;
    UPROPERTY(EditAnywhere, Category="Placeholder|Animation") FVector HandItemOffset = FVector(0,-3,0);
    UPROPERTY(EditAnywhere, Category="Placeholder|Animation", meta=(ClampMin="0.15")) float PickupDuration = 0.45f;
    UPROPERTY(EditAnywhere, Category="Placeholder|Animation", meta=(ClampMin="0.15")) float ThrowDuration = 0.38f;
    UPROPERTY(EditAnywhere, Category="Placeholder|Back", meta=(ClampMin="0.3")) float BackHoldSeconds=0.5f;
    UPROPERTY(EditAnywhere, Category="Placeholder|Back", meta=(ClampMin="0.1",ClampMax="0.5")) float BackDoubleTapSeconds=0.28f;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    enum class EAction : uint8 { None, Pickup, Throw };
    UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> Mesh;
    UPROPERTY(Transient) TObjectPtr<USirrohPoseBridgeComponent> PoseBridge;
    UPROPERTY(Transient) TObjectPtr<UPlaceholderHandsRig> Rig;
    UPROPERTY(Transient) TObjectPtr<UTexture2D> FrameTexture;
    UPROPERTY(Transient) TObjectPtr<UTexture2D> BerryTexture;
    TWeakObjectPtr<ASirrohWorldItem> Focus;
    TWeakObjectPtr<ASirrohWorldItem> Pending;
    TSharedPtr<SWidget> HUD;
    FSlateBrush FrameBrush;
    FSlateBrush BerryBrushes[5];
    bool bCharging=false;
    float ChargeTime=0;
    int32 ChargeSlot=0;
    void ReleaseChargedThrow();
    EAction Action = EAction::None;
    int32 ActionSlot = 0;
    float ActionTime = 0;
    bool bCommitted = false;
    bool bASTPickup = false;
    bool bASTApproaching = false;
    bool BeginASTPickup();
    void CancelASTPickup();

    FVector ActionTarget = FVector::ZeroVector;
    FVector ReleaseOrigin = FVector::ZeroVector;
    TMap<FName,FTransform> RigPose;
    TArray<int32> BackOrder;
    bool bBackDown=false;
    bool bBackHoldHandled=false;
    bool bBackPendingTap=false;
    bool bBackSecondTap=false;
    double BackPressedAt=0;
    double BackTapDeadline=0;
    void PrepareInventoryChange();
    void DrawLastBackItem();
    void FindFocus();
    bool IsReachable(const ASirrohWorldItem* Berry) const;
    void UpdatePose(float Delta);
    FVector HandLocation(int32 Slot) const;
    FVector CursorTarget() const;
    void BuildHUD();
    ASirrohWorldItem* Item(int32 Slot) const;

};