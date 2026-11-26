#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Items/SirrohNativeItemSave.h"
#include "TimerManager.h"
#include "SirrohASTInventoryComponent.generated.h"
class ASirrohWorldItem;
class UPWGv2WorldSave;
/** Native item extension of BPC_Inventory, not a second inventory component.
 * AST firearm structs require skeletal meshes; procedural items keep their actual actors. */
UCLASS(Blueprintable, ClassGroup=(Inventory))
class SIRROH_REIMAGINEG_V3_API USirrohASTInventoryComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    USirrohASTInventoryComponent();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    UFUNCTION(BlueprintPure, Category="Inventory|Native") ASirrohWorldItem* GetNativeItem(int32 Slot) const;
    TArray<FSirrohNativeItemSave> CaptureNativeItems() const;
    void ToggleBackpack();
    bool IsBackpackActive() const;
    void SelectBackpackSlot(int32 Slot);
    virtual void TickComponent(float Delta,ELevelTick Tick,FActorComponentTickFunction* Function) override;
    UFUNCTION(Server,Reliable) void ServerToggleBackpack();
    bool IsNativeInventoryReady() const { return bRestored; }
    bool SetNativeItem(int32 Slot, ASirrohWorldItem* Item);
    bool SwapNativeItems(int32 A, int32 B);
    void ThrowNativeItem(int32 Slot,FVector Origin,FVector Velocity);
    UFUNCTION(Server,Reliable) void ServerSwapNativeItems(int32 A,int32 B);
    UFUNCTION(Server,Reliable) void ServerThrowNativeItem(int32 Slot,FVector Origin,FVector Velocity);
    UFUNCTION(Server,Reliable) void ServerRequestNativePickup(ASirrohWorldItem* Item);

    UFUNCTION(BlueprintPure, Category="Inventory|Native") int32 GetNativeSlotCount() const { return 5; }
private:
    void RestoreNativeVisit();
    void RefreshBackpackPanel();
    UFUNCTION() void SelectBackpack1();
    UFUNCTION() void SelectBackpack2();
    UFUNCTION() void SelectBackpack3();
    UPROPERTY(Transient) TObjectPtr<class UHorizontalBox> BackpackPanel;
    UPROPERTY(Transient) TArray<TObjectPtr<class UTextBlock>> BackpackLabels;
    double BackpackTransitionDeadline=0;

    bool bRestored=false,bEndingPlay=false;
    int32 RestoreAttempts=0;
    FTimerHandle RestoreTimer;
    FGuid SavedCharacterId;
    TWeakObjectPtr<UPWGv2WorldSave> SavedWorld;
    TArray<FSirrohNativeItemSave> EndingSnapshot;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    // 0/1 hands; 2/3/4 existing Sirroh back sockets. Sole native item storage.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category="Inventory|Native") TArray<TObjectPtr<ASirrohWorldItem>> NativeSlots;
};
