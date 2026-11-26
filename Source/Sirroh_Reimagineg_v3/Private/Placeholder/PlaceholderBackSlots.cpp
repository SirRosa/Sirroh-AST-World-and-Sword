#include "Placeholder/PlaceholderHandsComponent.h"
#include "Items/SirrohWorldItem.h"
#include "Items/SirrohASTInventoryComponent.h"
#include "Combat/CombatVitalityComponent.h"
#include "Combat/DirectionalCombatComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Character/SirrohPoseBridgeComponent.h"
#include "Engine/World.h"


namespace PlaceholderBack
{
    const FName Sockets[]={TEXT("Back_Slot_01"),TEXT("Back_Slot_02"),TEXT("Back_Slot_03")};
}
void UPlaceholderHandsComponent::CancelBackInput()
{
    bBackDown=false; bBackHoldHandled=false; bBackPendingTap=false; bBackSecondTap=false;
}
void UPlaceholderHandsComponent::HandleBackInput(bool bPressed,bool bHeld,bool bReleased)
{
    const double Now=GetWorld()->GetTimeSeconds();
    // Wait for the double-tap window before committing the single-tap action.
    if (bBackPendingTap && !bBackDown && Now>BackTapDeadline)
    { bBackPendingTap=false; DrawLastBackItem(); }
    if (bPressed)
    {
        bBackSecondTap=bBackPendingTap && Now<=BackTapDeadline;
        bBackPendingTap=false; bBackDown=true; bBackHoldHandled=false; BackPressedAt=Now;
    }
    if (bBackDown && (bHeld || bReleased) && !bBackHoldHandled && Now-BackPressedAt>=FMath::Max(0.3f,BackHoldSeconds))
    {
        bBackHoldHandled=true; bBackPendingTap=false; bBackSecondTap=false;
        StowRightItem();
    }
    if (bReleased && bBackDown)
    {
        bBackDown=false;
        if (bBackHoldHandled) return;
        if (bBackSecondTap) { bBackSecondTap=false; SwapHandItems(); }
        else { bBackPendingTap=true; BackTapDeadline=Now+FMath::Clamp(BackDoubleTapSeconds,0.1f,0.5f); }
    }
    else if (bBackDown && !bHeld && !bPressed) CancelBackInput();
}
void UPlaceholderHandsComponent::PrepareInventoryChange()
{
    CancelASTPickup();
    if(Pending.IsValid())Pending->ReleasePickup(GetOwner());
    CancelCharge(); Action=EAction::None; Pending.Reset(); bCommitted=false;
    if (auto* Combat=GetOwner()->FindComponentByClass<UDirectionalCombatComponent>()) Combat->LeaveCombat();
}
void UPlaceholderHandsComponent::StowRightItem()
{
    if (!GetItem(0) || !Mesh || !Inventory()) return;
    for (int32 Slot=0;Slot<3;++Slot)
    {
        if (GetItem(Slot+2) || !Mesh->DoesSocketExist(PlaceholderBack::Sockets[Slot])) continue;
        PrepareInventoryChange();
        if(!Inventory()->SwapNativeItems(0,Slot+2))return;
        BackOrder.Remove(Slot);BackOrder.Add(Slot);UpdateBackItems();return;
    }
}
void UPlaceholderHandsComponent::SelectBackSlot(int32 Slot)
{
    if(Slot<0 || Slot>2 || !Inventory() || !Mesh || !Mesh->DoesSocketExist(PlaceholderBack::Sockets[Slot]))return;
    if(!GetItem(Slot+2) && !GetItem(0))return;
    if(const auto* Health=GetOwner()->FindComponentByClass<UCombatVitalityComponent>();Health && (Health->IsIncapacitated() || !Health->CanUseArm(false)))return;
    PrepareInventoryChange();
    if(!Inventory()->SwapNativeItems(0,Slot+2))return;
    BackOrder.Remove(Slot);if(GetItem(Slot+2))BackOrder.Add(Slot);UpdateBackItems();
}
void UPlaceholderHandsComponent::DrawLastBackItem()
{
    BackOrder.RemoveAll([this](int32 Slot){return Slot<0 || Slot>2 || !GetItem(Slot+2);});
    if(!BackOrder.IsEmpty()){SelectBackSlot(BackOrder.Last());return;}
    for(int32 Slot=2;Slot>=0;--Slot)if(GetItem(Slot+2)){SelectBackSlot(Slot);return;}
}
void UPlaceholderHandsComponent::SwapHandItems()
{
    if(!Inventory() || (!GetItem(0) && !GetItem(1)))return;
    if(const auto* Health=GetOwner()->FindComponentByClass<UCombatVitalityComponent>();Health && (Health->IsIncapacitated()
        || (GetItem(0) && !Health->CanUseArm(true)) || (GetItem(1) && !Health->CanUseArm(false))))return;
    PrepareInventoryChange();Inventory()->SwapNativeItems(0,1);
}
void UPlaceholderHandsComponent::UpdateBackItems()
{
    if (!Mesh) return;
    for (int32 Slot=0;Slot<3;++Slot)
    {
        auto* Stored=GetItem(Slot+2);
        if (!Stored || !Mesh->DoesSocketExist(PlaceholderBack::Sockets[Slot])) continue;
        const FName Socket=PlaceholderBack::Sockets[Slot];
        FTransform Pose=Mesh->GetSocketTransform(Socket);
        const FName Bone=Mesh->GetSocketBoneName(Socket);
        if (PoseBridge && Mesh->GetBoneIndex(Bone)!=INDEX_NONE)
        {
            FTransform FinalBone;
            if (PoseBridge->TryGetFinalBoneTransform(Bone,FinalBone))
                Pose=Pose.GetRelativeTransform(Mesh->GetSocketTransform(Bone))*FinalBone;
        }
        Stored->SetHeldTransform(Pose);
    }
}
