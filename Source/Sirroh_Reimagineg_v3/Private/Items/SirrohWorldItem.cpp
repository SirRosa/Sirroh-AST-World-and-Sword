#include "Items/SirrohWorldItem.h"
#include "Components/WidgetComponent.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "UObject/UnrealType.h"
#include "Sword/SwordWorldSubsystem.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Items/SirrohCarryProps.h"
#include "Items/SirrohASTInventoryComponent.h"

void ASirrohWorldItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASirrohWorldItem,bHeld);
    DOREPLIFETIME(ASirrohWorldItem,bFlying);
    DOREPLIFETIME(ASirrohWorldItem,LaneY);
    DOREPLIFETIME(ASirrohWorldItem,PersistentId);
}

void ASirrohWorldItem::BeginPlay()
{
    Super::BeginPlay();
    if(HasAuthority() && PersistentId.IsEmpty())PersistentId=FGuid::NewGuid().ToString();
    GetWorld()->GetSubsystem<USwordWorldSubsystem>()->Items.Add(this);
    if(GetNetMode()!=NM_DedicatedServer)
    {
        PickupPrompt=NewObject<UWidgetComponent>(this,TEXT("ASTLootPrompt"));
        PickupPrompt->SetupAttachment(GetRootComponent());
        PickupPrompt->SetWidgetClass(LoadClass<UUserWidget>(nullptr,TEXT("/AST_InventorySystem/UI/InteractionWidget.InteractionWidget_C")));
        PickupPrompt->SetWidgetSpace(EWidgetSpace::Screen);
        PickupPrompt->SetDrawSize(FVector2D(102,75));
        PickupPrompt->SetRelativeLocation(FVector(0,0,35));
        PickupPrompt->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        PickupPrompt->SetVisibility(false);PickupPrompt->RegisterComponent();
    }
}
void ASirrohWorldItem::SetPickupPromptVisible(bool bVisible)
{
    if(!PickupPrompt)return;
    const bool Show=bVisible && CanPickUp();
    PickupPrompt->SetVisibility(Show);
    if(auto* Widget=PickupPrompt->GetUserWidgetObject())
    {
        const ESlateVisibility Visibility=Show?ESlateVisibility::HitTestInvisible:ESlateVisibility::Hidden;
        Widget->SetVisibility(Visibility);
        if(auto* Key=FindFProperty<FNameProperty>(Widget->GetClass(),TEXT("DataKey")))
            Key->SetPropertyValue_InContainer(Widget,TEXT("E"));
        // AST's widget starts hidden internally; component visibility alone is insufficient.
        if(Widget->WidgetTree)
        {
            if(Widget->WidgetTree->RootWidget)Widget->WidgetTree->RootWidget->SetVisibility(Visibility);
            for(const FName Name:{FName(TEXT("InteractionImage")),FName(TEXT("InteractionBorder")),FName(TEXT("InteractionText"))})
                if(auto* Part=Widget->WidgetTree->FindWidget(Name))Part->SetVisibility(Visibility);
        }
    }
}
void ASirrohWorldItem::EndPlay(const EEndPlayReason::Type Reason)
{
    if (auto* State=GetWorld()->GetSubsystem<USwordWorldSubsystem>())
        State->Items.RemoveAll([this](auto Item){return !Item.IsValid() || Item.Get()==this;});
    Super::EndPlay(Reason);
}
void ASirrohWorldItem::MarkDisplaced()
{
    if (!PersistentId.IsEmpty()) GetWorld()->GetSubsystem<USwordWorldSubsystem>()->RemovedFromSpawn.Add(PersistentId);
}
void ASirrohWorldItem::Take(AActor* Holder)
{
    if(!CanPickUpBy(Holder))return;
    ReleasePickup(Holder);
    MarkDisplaced();
    DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    bHeld=true; bFlying=false;
    SetOwner(Holder);
    SetHighlighted(false);SetPickupPromptVisible(false);
    SetActorTickEnabled(false);
}

bool ASirrohWorldItem::CanPickUpBy(AActor* Holder) const
{
    if(Holder)if(const auto* Inventory=Holder->FindComponentByClass<USirrohASTInventoryComponent>())
    {
        for(int32 Slot=0;Slot<2;++Slot)if(auto* Held=Inventory->GetNativeItem(Slot))
            if(auto* Pose=Held->FindComponentByClass<USirrohCarryPoseComponent>();Pose && Pose->bTwoHanded)return false;
        if(const auto* Pose=FindComponentByClass<USirrohCarryPoseComponent>())
            if(Inventory->GetNativeItem(Pose->HandSlot()) || (Pose->bTwoHanded && Inventory->GetNativeItem(1-Pose->HandSlot())))return false;
    }
    return IsValid(Holder) && CanPickUp() && (!PickupReservation.IsValid()
        || !GetWorld() || GetWorld()->GetTimeSeconds()>=PickupReservationUntil || PickupReservation.Get()==Holder);
}
bool ASirrohWorldItem::ReservePickup(AActor* Holder,float Duration)
{
    if(!HasAuthority() || !GetWorld() || !FMath::IsFinite(Duration) || Duration<=0 || !CanPickUpBy(Holder))return false;
    PickupReservation=Holder;PickupReservationUntil=GetWorld()->GetTimeSeconds()+FMath::Clamp(Duration,.1f,30.f);
    return true;
}
void ASirrohWorldItem::ReleasePickup(AActor* Holder)
{
    if(PickupReservation.Get()==Holder){PickupReservation.Reset();PickupReservationUntil=0;}
}
bool ASirrohWorldItem::CommitPickup(AActor* Holder)
{
    if(!HasAuthority() || !GetWorld() || PickupReservation.Get()!=Holder
        || GetWorld()->GetTimeSeconds()>=PickupReservationUntil || !CanPickUpBy(Holder))return false;
    Take(Holder);
    const bool Taken=IsHeld() && GetOwner()==Holder;
    ReleasePickup(Holder);
    return Taken;
}
