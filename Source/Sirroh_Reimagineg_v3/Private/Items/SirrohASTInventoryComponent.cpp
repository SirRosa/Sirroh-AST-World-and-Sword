#include "Items/SirrohASTInventoryComponent.h"
#include "Items/SirrohCarryProps.h"
#include "Sword/ProceduralSword.h"
#include "Placeholder/PlaceholderBerry.h"
#include "Procedural_World_Generator/PWGv2WorldSession.h"
#include "Procedural_World_Generator/PWGv2Generator.h"
#include "PWG/PWGGameMode.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Items/SirrohWorldItem.h"
#include "Net/UnrealNetwork.h"
#include "Placeholder/PlaceholderHandsComponent.h"
#include "Combat/CombatVitalityComponent.h"
#include "Combat/DirectionalCombatComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/HorizontalBox.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Blueprint/UserWidget.h"
#include "PWG/PWGSidescrollerInputComponent.h"
#include "UObject/UnrealType.h"
#include "UObject/StructOnScope.h"
USirrohASTInventoryComponent::USirrohASTInventoryComponent()
{
    SetIsReplicatedByDefault(true);NativeSlots.SetNum(5);
    PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.TickInterval=.1f;
}
void USirrohASTInventoryComponent::BeginPlay()
{
    Super::BeginPlay();SetIsReplicated(true);
    // BPC_Inventory was authored as a non-ticking ActorComponent before reparenting.
    PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.TickInterval=.1f;
    RegisterAllComponentTickFunctions(true);SetComponentTickEnabled(true);
    if(GetOwner()->HasAuthority())
    {
        NativeSlots.SetNum(5);
        const auto* Mode=GetWorld()->GetAuthGameMode<APWGGameMode>();
        // POI tests/workshops have no persistent generated world to restore.
        if(Cast<APawn>(GetOwner()) && Mode && !Mode->bPOITestMode)
            GetWorld()->GetTimerManager().SetTimer(RestoreTimer,this,&USirrohASTInventoryComponent::RestoreNativeVisit,.25f,true);
        else bRestored=true;
    }
}
void USirrohASTInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);DOREPLIFETIME(USirrohASTInventoryComponent,NativeSlots);
}
ASirrohWorldItem* USirrohASTInventoryComponent::GetNativeItem(int32 Slot) const
{
    return NativeSlots.IsValidIndex(Slot) && IsValid(NativeSlots[Slot])?NativeSlots[Slot].Get():nullptr;
}
bool USirrohASTInventoryComponent::SetNativeItem(int32 Slot,ASirrohWorldItem* Item)
{
    if(!GetOwner()->HasAuthority() || !NativeSlots.IsValidIndex(Slot))return false;
    if(Item)
    {
        if(!IsValid(Item) || !Item->IsHeld() || Item->GetOwner()!=GetOwner())return false;
        if(const auto* Pose=Item->FindComponentByClass<USirrohCarryPoseComponent>())
            if((Slot!=Pose->HandSlot() && !(Item->IsA<ASirrohTorch>() && Slot<2)) || (Pose->bTwoHanded && GetNativeItem(1-Slot)))return false;
        if(Slot<2)if(auto* Other=GetNativeItem(1-Slot))
            if(const auto* Pose=Other->FindComponentByClass<USirrohCarryPoseComponent>();Pose && Pose->bTwoHanded)return false;
        for(int32 I=0;I<NativeSlots.Num();++I)if(I!=Slot && NativeSlots[I]==Item)return false;
        if(GetNativeItem(Slot) && GetNativeItem(Slot)!=Item)return false;
        Item->SetReplicates(true);Item->SetReplicateMovement(true);
    }
    if(auto* Torch=Cast<ASirrohTorch>(Item))Torch->CarryPose->SetTorchHand(Slot);
    NativeSlots[Slot]=Item;GetOwner()->ForceNetUpdate();return true;
}
bool USirrohASTInventoryComponent::SwapNativeItems(int32 A,int32 B)
{
    if(!GetOwner()->HasAuthority()){ServerSwapNativeItems(A,B);return false;}
    if(!NativeSlots.IsValidIndex(A) || !NativeSlots.IsValidIndex(B))return false;
    // Large props and burning torches stay in their authored hand slot, never inside a backpack.
    for(int32 Slot:{A,B})if(auto* Item=GetNativeItem(Slot))
        if(Item->FindComponentByClass<USirrohCarryPoseComponent>() && !(Item->IsA<ASirrohTorch>() && A<2 && B<2))return false;
    if(const auto* Health=GetOwner()->FindComponentByClass<UCombatVitalityComponent>();Health && (Health->IsIncapacitated()
        || (A<2 && GetNativeItem(B) && !Health->CanUseArm(A==1)) || (B<2 && GetNativeItem(A) && !Health->CanUseArm(B==1))))return false;
    auto* Mesh=GetOwner()->FindComponentByClass<USkeletalMeshComponent>();
    for(int32 Slot:{A,B})if(Slot>=2 && (!Mesh || !Mesh->DoesSocketExist(FName(*FString::Printf(TEXT("Back_Slot_%02d"),Slot-1)))))return false;
    if(auto* Hands=GetOwner()->FindComponentByClass<UPlaceholderHandsComponent>();Hands && Hands->IsASTPickupActive())return false;
    if(auto* Combat=GetOwner()->FindComponentByClass<UDirectionalCombatComponent>())Combat->LeaveCombat();
    NativeSlots.Swap(A,B);
    for(int32 Slot:{A,B})if(auto* Torch=Cast<ASirrohTorch>(GetNativeItem(Slot)))Torch->CarryPose->SetTorchHand(Slot);
    GetOwner()->ForceNetUpdate();return true;
}
void USirrohASTInventoryComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    GetWorld()->GetTimerManager().ClearTimer(RestoreTimer);
    if(BackpackPanel)BackpackPanel->RemoveFromParent();
    if(auto* P=FindFProperty<FObjectPropertyBase>(GetClass(),TEXT("WeaponSelectHUD")))
        if(auto* HUD=Cast<UUserWidget>(P->GetObjectPropertyValue_InContainer(this)))HUD->RemoveFromParent();
    EndingSnapshot=CaptureNativeItems();bEndingPlay=true;
    if(GetOwner()->HasAuthority() && bRestored && SavedWorld.IsValid() && SavedCharacterId.IsValid())
        SavedWorld->Visits.FindOrAdd(SavedCharacterId).NativeInventory=EndingSnapshot;
    if(GetOwner()->HasAuthority())for(ASirrohWorldItem* Item:NativeSlots)if(IsValid(Item) && Item->GetOwner()==GetOwner())Item->Destroy();
    Super::EndPlay(Reason);
}

void USirrohASTInventoryComponent::ServerSwapNativeItems_Implementation(int32 A,int32 B){SwapNativeItems(A,B);}
void USirrohASTInventoryComponent::ServerThrowNativeItem_Implementation(int32 Slot,FVector Origin,FVector Velocity){ThrowNativeItem(Slot,Origin,Velocity);}
void USirrohASTInventoryComponent::ServerRequestNativePickup_Implementation(ASirrohWorldItem* Item)
{
    if(auto* Hands=GetOwner()->FindComponentByClass<UPlaceholderHandsComponent>())Hands->RequestPickup(Item);
}
void USirrohASTInventoryComponent::ThrowNativeItem(int32 Slot,FVector Origin,FVector Velocity)
{
    if(!GetOwner()->HasAuthority()){ServerThrowNativeItem(Slot,Origin,Velocity);return;}
    auto* Item=GetNativeItem(Slot);
    if(Slot<0 || Slot>1 || !Item || !Item->IsHeld() || Item->GetOwner()!=GetOwner() || Origin.ContainsNaN() || Velocity.ContainsNaN()
        || FVector::DistSquared(Origin,GetOwner()->GetActorLocation())>FMath::Square(220.f))return;
    if(const auto* Health=GetOwner()->FindComponentByClass<UCombatVitalityComponent>();Health && (Health->IsIncapacitated() || !Health->CanUseArm(Slot==1)))return;
    Item->Throw(Origin,Velocity.GetClampedToMaxSize(3000.f),GetOwner());
    SetNativeItem(Slot,nullptr);
}

TArray<FSirrohNativeItemSave> USirrohASTInventoryComponent::CaptureNativeItems() const
{
    if(bEndingPlay)return EndingSnapshot;
    TArray<FSirrohNativeItemSave> Result;
    for(int32 Slot=0;Slot<5;++Slot)if(auto* Item=GetNativeItem(Slot))
    {
        FSirrohNativeItemSave Saved;Saved.Slot=Slot;Saved.PersistentId=Item->PersistentId;Saved.Scale=Item->GetActorScale3D();
        if(Item->FindComponentByClass<USirrohCarryPoseComponent>())Saved.CarryClass=FSoftClassPath(Item->GetClass());
        else if(auto* Sword=Cast<AProceduralSword>(Item)){Saved.bSword=true;Saved.Sword=Sword->Definition;Saved.PixelScale=Sword->WorldUnitsPerPixel;}
        else if(auto* Berry=Cast<APlaceholderBerry>(Item)){Saved.AtlasFrame=Berry->AtlasFrame;Saved.BerrySize=Berry->Size;}
        else continue;
        Result.Add(Saved);
    }
    return Result;
}
void USirrohASTInventoryComponent::RestoreNativeVisit()
{
    if(bRestored || ++RestoreAttempts>120){GetWorld()->GetTimerManager().ClearTimer(RestoreTimer);return;}
    const auto* Pawn=Cast<APawn>(GetOwner());
    if(!Pawn || !Pawn->GetController())return; // BeginPlay can precede possession.
    if(!Pawn->IsPlayerControlled()){bRestored=true;GetWorld()->GetTimerManager().ClearTimer(RestoreTimer);return;}
    const auto* Mode=GetWorld()->GetAuthGameMode<APWGGameMode>();
    auto* Generator=Mode?Mode->GetWorldGenerator():nullptr;
    auto* Data=Generator?Generator->GetDebugWorld():nullptr;
    const auto* Session=GetWorld()->GetGameInstance()->GetSubsystem<UPWGv2WorldSession>();
    if(!Data || !Session)return;
    SavedWorld=Data;SavedCharacterId=Session->CharacterId;
    bRestored=true;GetWorld()->GetTimerManager().ClearTimer(RestoreTimer);
    const auto* Visit=Data->Visits.Find(Session->CharacterId);if(!Visit)return;
    TSet<FString> RestoredIds;
    for(const auto& Saved:Visit->NativeInventory)
    {
        if(Saved.Slot<0 || Saved.Slot>=5 || GetNativeItem(Saved.Slot) || Saved.PersistentId.IsEmpty() || RestoredIds.Contains(Saved.PersistentId)
            || Saved.Scale.ContainsNaN() || !FMath::IsFinite(Saved.PixelScale) || !FMath::IsFinite(Saved.BerrySize))continue;
        const FTransform Transform(FRotator::ZeroRotator,GetOwner()->GetActorLocation(),Saved.Scale.ComponentMax(FVector(.01f)).ComponentMin(FVector(10.f)));
        UClass* Class=Saved.bSword?AProceduralSword::StaticClass():APlaceholderBerry::StaticClass();
        if(!Saved.CarryClass.IsNull())
        {
            Class=Saved.CarryClass.TryLoadClass<ASirrohWorldItem>();
            if(!Class || (!Class->IsChildOf(ASirrohTorch::StaticClass()) && !Class->IsChildOf(ASirrohCarryProp::StaticClass())) || Saved.Slot>1)continue;
        }
        auto* Item=GetWorld()->SpawnActorDeferred<ASirrohWorldItem>(Class,Transform,GetOwner(),nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if(!Item)continue;
        Item->PersistentId=Saved.PersistentId;Item->LaneY=GetOwner()->GetActorLocation().Y;
        if(auto* Sword=Cast<AProceduralSword>(Item)){Sword->Definition=Saved.Sword;Sword->WorldUnitsPerPixel=FMath::Clamp(Saved.PixelScale,.05f,10.f);}
        if(auto* Berry=Cast<APlaceholderBerry>(Item)){Berry->AtlasFrame=FMath::Clamp(Saved.AtlasFrame,0,63);Berry->Size=FMath::Clamp(Saved.BerrySize,1.f,100.f);}
        UGameplayStatics::FinishSpawningActor(Item,Transform);
        if(auto* Torch=Cast<ASirrohTorch>(Item))Torch->CarryPose->SetTorchHand(Saved.Slot);
        Item->Take(GetOwner());
        if(!SetNativeItem(Saved.Slot,Item)){Item->Destroy();continue;}
        RestoredIds.Add(Saved.PersistentId);
    }
}

namespace
{
UActorComponent* BackpackStateManager(const AActor* Owner)
{
    for(UActorComponent* C:Owner->GetComponents())if(C && C->ComponentHasTag(TEXT("StateManager")))return C;
    return nullptr;
}
uint8 BackpackState(const AActor* Owner)
{
    auto* State=BackpackStateManager(Owner);
    auto* P=State?FindFProperty<FByteProperty>(State->GetClass(),TEXT("BackpackState")):nullptr;
    return P?P->GetPropertyValue_InContainer(State):0;
}
bool SetBackpackState(AActor* Owner,uint8 Value)
{
    auto* State=BackpackStateManager(Owner);
    UFunction* Fn=State?State->FindFunction(TEXT("Set Backpack State")):nullptr;
    auto* P=Fn?FindFProperty<FByteProperty>(Fn,TEXT("NewBackpackState")):nullptr;
    if(!P)return false;
    FStructOnScope Args(Fn);P->SetPropertyValue_InContainer(Args.GetStructMemory(),Value);
    State->ProcessEvent(Fn,Args.GetStructMemory());return true;
}
}
bool USirrohASTInventoryComponent::IsBackpackActive() const {return BackpackState(GetOwner())!=0;}
void USirrohASTInventoryComponent::ServerToggleBackpack_Implementation(){ToggleBackpack();}
void USirrohASTInventoryComponent::ToggleBackpack()
{
    if(!GetOwner()->HasAuthority()){ServerToggleBackpack();return;}
    const uint8 State=BackpackState(GetOwner());
    // Verified AST enum: 0 NoAction, 1 OpenBackpack, 2 Backpack, 3 CloseBackpack.
    if(State==1 || State==3)return;
    if(State==2){SetBackpackState(GetOwner(),3);BackpackTransitionDeadline=GetWorld()->GetTimeSeconds()+5.;return;}
    if(const auto* Health=GetOwner()->FindComponentByClass<UCombatVitalityComponent>();Health && (Health->IsIncapacitated() || !Health->CanUseArm(false) || !Health->CanUseArm(true)))return;
    auto* Hands=GetOwner()->FindComponentByClass<UPlaceholderHandsComponent>();
    if(!bRestored || !Hands || Hands->IsASTPickupActive())return;
    if(auto* Input=GetOwner()->FindComponentByClass<UPWGSidescrollerInputComponent>();Input && (Input->IsASTInteractionActive() || Input->IsTraversalActive()))return;
    // Item-to-bag animations are deferred by user request. Both hands must be free
    // for AST's existing backpack pose; preflight all storage before moving either item.
    TArray<int32> Empty;
    for(int32 Slot=2;Slot<5;++Slot)if(!GetNativeItem(Slot))Empty.Add(Slot);
    const int32 Held=(GetNativeItem(0)?1:0)+(GetNativeItem(1)?1:0);
    if(Empty.Num()<Held)return;
    Hands->CancelCharge();Hands->CancelBackInput();
    for(int32 Hand=0;Hand<2;++Hand)if(GetNativeItem(Hand))
    {if(!SwapNativeItems(Hand,Empty[0]))return;Empty.RemoveAt(0);}
    if(auto* Combat=GetOwner()->FindComponentByClass<UDirectionalCombatComponent>())Combat->LeaveCombat();
    if(SetBackpackState(GetOwner(),1))BackpackTransitionDeadline=GetWorld()->GetTimeSeconds()+5.;
}
void USirrohASTInventoryComponent::SelectBackpackSlot(int32 Slot)
{
    if(BackpackState(GetOwner())!=2 || Slot<0 || Slot>2)return;
    if(auto* Hands=GetOwner()->FindComponentByClass<UPlaceholderHandsComponent>())Hands->SelectBackSlot(Slot);
    ToggleBackpack();
}
void USirrohASTInventoryComponent::SelectBackpack1(){SelectBackpackSlot(0);}
void USirrohASTInventoryComponent::SelectBackpack2(){SelectBackpackSlot(1);}
void USirrohASTInventoryComponent::SelectBackpack3(){SelectBackpackSlot(2);}
void USirrohASTInventoryComponent::RefreshBackpackPanel()
{
    const auto* Pawn=Cast<APawn>(GetOwner());if(!Pawn || !Pawn->IsLocallyControlled() || !Pawn->IsPlayerControlled())return;
    auto* P=FindFProperty<FObjectPropertyBase>(GetClass(),TEXT("WeaponSelectHUD"));
    auto* HUD=P?Cast<UUserWidget>(P->GetObjectPropertyValue_InContainer(this)):nullptr;
    if(!HUD && P)
    {
        // PWG possesses after BeginPlay; AST's one-shot local initialization can
        // therefore miss its HUD. Create the original HUD once ownership is known.
        auto* PC=Cast<APlayerController>(Pawn->GetController());
        UClass* Class=LoadClass<UUserWidget>(nullptr,TEXT("/AST_InventorySystem/UI/WeaponSelect/WeaponSelectHUD.WeaponSelectHUD_C"));
        if(PC && Class)HUD=CreateWidget<UUserWidget>(PC,Class);
        if(HUD){P->SetObjectPropertyValue_InContainer(this,HUD);HUD->AddToViewport(2);HUD->SetVisibility(ESlateVisibility::Hidden);}
    }
    if(!HUD)return;
    if(!BackpackPanel)
    {
        auto* Canvas=Cast<UCanvasPanel>(HUD->GetWidgetFromName(TEXT("CanvasPanel")));if(!Canvas)return;
        BackpackPanel=NewObject<UHorizontalBox>(HUD);
        auto* PanelSlot=Canvas->AddChildToCanvas(BackpackPanel);
        PanelSlot->SetAnchors(FAnchors(.5f,.5f));PanelSlot->SetAlignment(FVector2D(.5f,.5f));
        PanelSlot->SetPosition(FVector2D(0,160));PanelSlot->SetAutoSize(true);
        for(int32 I=0;I<3;++I)
        {
            auto* Button=NewObject<UButton>(HUD);auto* Label=NewObject<UTextBlock>(HUD);
            Label->SetMargin(FMargin(18,12));Button->AddChild(Label);BackpackPanel->AddChildToHorizontalBox(Button);BackpackLabels.Add(Label);
            if(I==0)Button->OnClicked.AddDynamic(this,&USirrohASTInventoryComponent::SelectBackpack1);
            if(I==1)Button->OnClicked.AddDynamic(this,&USirrohASTInventoryComponent::SelectBackpack2);
            if(I==2)Button->OnClicked.AddDynamic(this,&USirrohASTInventoryComponent::SelectBackpack3);
        }
    }
    const bool Open=BackpackState(GetOwner())==2;
    BackpackPanel->SetVisibility(Open?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    if(Open)
    {
        HUD->SetVisibility(ESlateVisibility::Visible);
        for(int32 I=0;I<BackpackLabels.Num();++I)
        {
            auto* Item=GetNativeItem(I+2);
            const TCHAR* Name=Cast<AProceduralSword>(Item)?TEXT("Miecz"):Item?TEXT("Przedmiot"):TEXT("Pusty");
            BackpackLabels[I]->SetText(FText::FromString(FString::Printf(TEXT("%d: %s"),I+1,Name)));
        }
    }
}
void USirrohASTInventoryComponent::TickComponent(float Delta,ELevelTick Tick,FActorComponentTickFunction* Function)
{
    Super::TickComponent(Delta,Tick,Function);
    if(GetOwner()->HasAuthority() && IsBackpackActive())
    {
        const auto* Health=GetOwner()->FindComponentByClass<UCombatVitalityComponent>();
        const uint8 State=BackpackState(GetOwner());
        if((Health && Health->IsIncapacitated()) || ((State==1 || State==3) && BackpackTransitionDeadline>0 && GetWorld()->GetTimeSeconds()>BackpackTransitionDeadline))
        {SetBackpackState(GetOwner(),0);BackpackTransitionDeadline=0;}
    }
    RefreshBackpackPanel();
}
