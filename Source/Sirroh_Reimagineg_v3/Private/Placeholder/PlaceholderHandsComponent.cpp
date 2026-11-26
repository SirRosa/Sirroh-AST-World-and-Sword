#include "Placeholder/PlaceholderHandsComponent.h"
#include "Items/SirrohWorldItem.h"
#include "Items/SirrohASTInventoryComponent.h"
#include "PWG/PWGSidescrollerInputComponent.h"
#include "PWG/PWGGameMode.h"
#include "Procedural_World_Generator/PWGv2Generator.h"
#include "Components/CapsuleComponent.h"
#include "UObject/UnrealType.h"
#include "UObject/StructOnScope.h"
#include "Components/SceneComponent.h"
#include "Sword/SwordWorldSubsystem.h"
#include "Sword/ProceduralSword.h"
#include "Items/SirrohCarryProps.h"
#include "Combat/DirectionalCombatComponent.h"
#include "Combat/CombatVitalityComponent.h"
#include "Placeholder/PlaceholderHandsRig.h"
#include "Character/SirrohPoseBridgeComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"


UPlaceholderHandsComponent::UPlaceholderHandsComponent()
{
    PrimaryComponentTick.bCanEverTick=true;
    PrimaryComponentTick.TickGroup=TG_PostPhysics;
}
void UPlaceholderHandsComponent::BeginPlay()
{
    Super::BeginPlay();
    Mesh=GetOwner()->FindComponentByClass<USkeletalMeshComponent>();
    if (Mesh) AddTickPrerequisiteComponent(Mesh);
    PoseBridge=USirrohPoseBridgeComponent::FindOrCreate(GetOwner());
    if (PoseBridge) PoseBridge->AddTickPrerequisiteComponent(this);
    Rig=NewObject<UPlaceholderHandsRig>(this);
    Rig->Initialize();
    BuildHUD();
}
void UPlaceholderHandsComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    CancelASTPickup();
    if (Pending.IsValid()) Pending->ReleasePickup(GetOwner());
    if (Focus.IsValid()) Focus->SetHighlighted(false);
    if (PoseBridge) { PoseBridge->ClearHandPose(); PoseBridge->RemoveTickPrerequisiteComponent(this); }
    if (HUD.IsValid() && GEngine && GEngine->GameViewport) GEngine->GameViewport->RemoveViewportWidgetContent(HUD.ToSharedRef());
    HUD.Reset();
    Super::EndPlay(Reason);
}
USirrohASTInventoryComponent* UPlaceholderHandsComponent::Inventory() const { return GetOwner()->FindComponentByClass<USirrohASTInventoryComponent>(); }
ASirrohWorldItem* UPlaceholderHandsComponent::GetItem(int32 Slot) const { const auto* I=Inventory();return I?I->GetNativeItem(Slot):nullptr; }
ASirrohWorldItem* UPlaceholderHandsComponent::Item(int32 Slot) const { return GetItem(Slot); }
bool UPlaceholderHandsComponent::SetItem(int32 Slot,ASirrohWorldItem* Item) { auto* I=Inventory();return I && I->SetNativeItem(Slot,Item); }
bool UPlaceholderHandsComponent::HasBerry(int32 Slot) const { return IsValid(Item(Slot)); }


bool UPlaceholderHandsComponent::IsReachable(const ASirrohWorldItem* Berry) const
{
    if (!IsValid(Berry) || !Berry->CanPickUpBy(GetOwner())) return false;
    const FVector Position=GetOwner()->GetActorLocation();
    const FVector Delta=Berry->GetActorLocation()-Position;
    if (FVector2D(Delta.X,Delta.Z).SizeSquared()>FMath::Square(Reach)) return false;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(PlaceholderBerryReach),false,GetOwner());
    Params.AddIgnoredActor(Berry);
    FCollisionObjectQueryParams Objects(ECC_WorldStatic);
    FHitResult Hit;
    // In the side view only X/Z separation exists. Check terrain in the
    // player's plane; background depth must not block a visible pickup.
    FVector ProjectedTarget=Berry->GetActorLocation();ProjectedTarget.Y=Position.Y;
    if(GetWorld()->LineTraceSingleByObjectType(Hit,Position,ProjectedTarget,Objects,Params))return false;
    return true;
}
void UPlaceholderHandsComponent::FindFocus()
{
    if(const auto* Pawn=Cast<APawn>(GetOwner());Pawn && !Pawn->IsLocallyControlled())return;
    ASirrohWorldItem* Best=nullptr;
    double BestDistance=DBL_MAX;
    if (Action==EAction::None && !bCharging)
    {
        for (const TWeakObjectPtr<ASirrohWorldItem>& Candidate : GetWorld()->GetSubsystem<USwordWorldSubsystem>()->Items)
        {
            ASirrohWorldItem* Berry=Candidate.Get();
            if (!IsReachable(Berry)) continue;
            const FVector Delta=Berry->GetActorLocation()-GetOwner()->GetActorLocation();
            const double Distance=Delta.X*Delta.X+Delta.Z*Delta.Z;
            if (Distance<BestDistance) { Best=Berry; BestDistance=Distance; }
        }
    }
    if (Best!=Focus.Get())
    {
        if (Focus.IsValid()) {Focus->SetHighlighted(false);Focus->SetPickupPromptVisible(false);}
        Focus=Best;
        if (Best) {Best->SetHighlighted(true);Best->SetPickupPromptVisible(true);}
    }
}
FText UPlaceholderHandsComponent::GetHint() const
{
    if (const auto* Combat=GetOwner()->FindComponentByClass<UDirectionalCombatComponent>())
        if (Combat->IsCombatActive()) return Combat->GetCombatHint();
    if (bCharging) return FText::FromString(FString::Printf(TEXT("Sila rzutu: %d%% | Pusc Q"),
        FMath::RoundToInt(100*FMath::Clamp(ChargeTime/FMath::Max(0.1f,FullChargeSeconds),0.0f,1.0f))));
    if (Action!=EAction::None) return FText::FromString(TEXT("..."));
    if (Focus.IsValid()) return FText::FromString(HasBerry(0)&&HasBerry(1)?TEXT("Rece pelne  |  Q - rzuc"):TEXT("E - podnies  |  Q - rzuc"));
    if (Cast<AProceduralSword>(GetItem(0))) return FText::FromString(TEXT("Alt - postawa bojowa | Przytrzymaj Q - rzut (3 s)"));
    return FText::FromString(HasBerry(0)||HasBerry(1)?TEXT("Przytrzymaj Q - naladuj rzut (3 s)"):TEXT("E - podnies przedmiot"));
}
FVector UPlaceholderHandsComponent::CursorTarget() const
{
    const APawn* Pawn=Cast<APawn>(GetOwner());
    APlayerController* PC=Pawn?Cast<APlayerController>(Pawn->GetController()):nullptr;
    FVector Origin,Direction;
    const FVector Player=GetOwner()->GetActorLocation();
    if (PC && PC->DeprojectMousePositionToWorld(Origin,Direction) && FMath::Abs(Direction.Y)>0.0001)
    {
        const double T=(Player.Y-Origin.Y)/Direction.Y;
        if (T>0) return Player+(Origin+Direction*T-Player).GetClampedToMaxSize(MaxThrowDistance);
    }
    return Player+FVector(GetOwner()->GetActorForwardVector().X>=0?350:-350,0,80);
}
void UPlaceholderHandsComponent::CancelCharge()
{
    bCharging=false;
    ChargeTime=0;
}
void UPlaceholderHandsComponent::ReleaseChargedThrow()
{
    ASirrohWorldItem* Berry=Item(ChargeSlot);
    if (!IsValid(Berry)) { CancelCharge(); return; }
    FVector Origin=Berry->GetActorLocation();
    Origin.Y=GetOwner()->GetActorLocation().Y;
    FVector Delta=CursorTarget()-Origin;
    Delta.Y=0;
    const float Alpha=FMath::Clamp(ChargeTime/FMath::Max(0.1f,FullChargeSeconds),0.0f,1.0f);
    const double Speed=FMath::Lerp(FMath::Max(1.0f,MinimumThrowSpeed),FMath::Max(MinimumThrowSpeed,MaximumThrowSpeed),Alpha);
    FVector Direction=Delta.GetSafeNormal();
    if (Direction.IsNearlyZero()) Direction=FVector(GetOwner()->GetActorForwardVector().X>=0?1:-1,0,0);
    FVector Velocity=Direction*Speed;
    const double X=FMath::Abs(Delta.X), G=980.0, V2=Speed*Speed;
    const double Discriminant=V2*V2-G*(G*X*X+2.0*Delta.Z*V2);
    if (X>1.0 && Discriminant>=0)
    {
        const double Tan=(V2-FMath::Sqrt(Discriminant))/(G*X);
        const double Horizontal=Speed/FMath::Sqrt(1.0+Tan*Tan);
        Velocity=FVector(Delta.X>=0?Horizontal:-Horizontal,0,Horizontal*Tan);
    }
    // Unreachable targets use a direct launch at the charged speed; charging never silently adds force.
    if(auto* I=Inventory())I->ThrowNativeItem(ChargeSlot,Origin,Velocity);
    CancelCharge();
}
void UPlaceholderHandsComponent::DebugSpawnSword()
{
    if(!GetOwner()->HasAuthority())return;
    CancelBackInput();
    CancelASTPickup();
    CancelCharge(); Action=EAction::None; if(Pending.IsValid())Pending->ReleasePickup(GetOwner()); Pending.Reset();
    const int32 Slot=!HasBerry(0)?0:(!HasBerry(1)?1:0);
    auto* Sword=GetWorld()->GetSubsystem<USwordWorldSubsystem>()->SpawnSword(GetOwner()->GetActorLocation(),
        GetOwner()->GetActorLocation().Y,FGuid::NewGuid().ToString(),0,static_cast<int32>(FGuid::NewGuid().A),nullptr,true);
    if (!Sword) return;
    if (IsValid(Item(Slot))) Item(Slot)->Destroy();
    Sword->Take(GetOwner()); SetItem(Slot,Sword);
}
void UPlaceholderHandsComponent::HandleInput(bool bPick,bool bChargePressed,bool bChargeHeld,bool bChargeReleased)
{
    if (bPick || bChargePressed) CancelBackInput();
    if (UGameplayStatics::IsGamePaused(this)) { CancelCharge(); return; }
    if (const auto* Vitality=GetOwner()->FindComponentByClass<UCombatVitalityComponent>(); Vitality && Vitality->UsesRagdollPose()) return;
    if (Action!=EAction::None) return;
    if (bCharging)
    {
        if (bChargeReleased) ReleaseChargedThrow();
        else if (!bChargeHeld) CancelCharge();
        return;
    }
    FindFocus();
    if (bChargePressed && (HasBerry(0)||HasBerry(1)))
    {
        ChargeSlot=HasBerry(0)?0:1;
        ChargeTime=0;
        bCharging=true;
        if (bChargeReleased) ReleaseChargedThrow();
        return;
    }
    else if (bPick && Focus.IsValid() && (!HasBerry(0)||!HasBerry(1)))
    {
        // AST owns the animation and its contact notify; do not run the legacy timer too.
        if(auto* I=Inventory())
        {
            if(GetOwner()->HasAuthority())BeginASTPickup();else I->ServerRequestNativePickup(Focus.Get());
            return;
        }
        if(GetOwner()->HasAuthority() && !Focus->ReservePickup(GetOwner(),FMath::Max(3.f,PickupDuration+1.f)))return;
        Action=EAction::Pickup;
        ActionSlot=HasBerry(0)?1:0;
        Pending=Focus;
        ActionTarget=Pending->GetActorLocation();ActionTarget.Y=GetOwner()->GetActorLocation().Y;
    }
    else return;
    ActionTime=0;
    bCommitted=false;
}
FVector UPlaceholderHandsComponent::HandLocation(int32 Slot) const
{
    const FName Bone=Slot==0?TEXT("hand_r"):TEXT("hand_l");
    if (const FTransform* T=RigPose.Find(Bone)) return T->GetLocation()+HandItemOffset;
    if (Mesh && Mesh->GetBoneIndex(Bone)!=INDEX_NONE) return Mesh->GetSocketLocation(Bone)+HandItemOffset;
    return GetOwner()->GetActorLocation()+FVector(Slot==0?20:-20,-5,5);
}
void UPlaceholderHandsComponent::UpdatePose(float Delta)
{
    FVector Targets[2];
    float Weights[2]={0,0};
    for (int32 Side=0;Side<2;++Side)
    {
        const FName ShoulderBone=Side==0?TEXT("upperarm_r"):TEXT("upperarm_l");
        const FVector Shoulder=Mesh?Mesh->GetSocketLocation(ShoulderBone):GetOwner()->GetActorLocation();
        const double Facing=GetOwner()->GetActorForwardVector().X>=0?1:-1;
        Targets[Side]=Shoulder+FVector(Facing*28,0,-35);
        Weights[Side]=HasBerry(Side)?0.7f:0;
        if (Side!=ActionSlot || Action==EAction::None) continue;
        const float Duration=Action==EAction::Pickup?PickupDuration:ThrowDuration;
        const float P=FMath::Clamp(ActionTime/FMath::Max(0.15f,Duration),0.0f,1.0f);
        if (Action==EAction::Pickup)
        {
            const float W=P<0.55f?FMath::SmoothStep(0.0f,0.55f,P):1.0f-FMath::SmoothStep(0.55f,1.0f,P);
            Targets[Side]=FMath::Lerp(Targets[Side],ActionTarget,static_cast<double>(W));
            Weights[Side]=FMath::Max(Weights[Side],W);
        }
        else
        {
            FVector Direction=ActionTarget-Shoulder; Direction.Y=0;
            Direction=Direction.GetSafeNormal();
            if (Direction.IsNearlyZero()) Direction=FVector(Facing,0,0);
            const FVector Back=Shoulder-Direction*22+FVector(0,0,10);
            const FVector Forward=Shoulder+Direction*58;
            Targets[Side]=P<0.3f?FMath::Lerp(Targets[Side],Back,static_cast<double>(FMath::SmoothStep(0.0f,0.3f,P)))
                :FMath::Lerp(Back,Forward,static_cast<double>(FMath::SmoothStep(0.3f,0.6f,P)));
            Weights[Side]=P<0.6f?1.0f:1.0f-FMath::SmoothStep(0.6f,1.0f,P);
        }
    }
    RigPose.Reset();
    if (bAnimateHands && Rig && Mesh) Rig->Solve(Mesh,Targets,Weights,RigPose);
    if(Action==EAction::None)
        for(int32 Side=0;Side<2;++Side)if(auto* Held=Item(Side))
            if(auto* Carry=Held->FindComponentByClass<USirrohCarryPoseComponent>())
            {if(Held->IsA<ASirrohTorch>())Carry->SetTorchHand(Side);Carry->ApplyPose(Mesh,RigPose,Delta);}
    if (PoseBridge) PoseBridge->PublishHandPose(RigPose);
}
void UPlaceholderHandsComponent::TickComponent(float Delta,ELevelTick Tick,FActorComponentTickFunction* Function)
{
    Super::TickComponent(Delta,Tick,Function);
    if (UGameplayStatics::IsGamePaused(this)) return;
    const auto* Vitality=GetOwner()->FindComponentByClass<UCombatVitalityComponent>();
    const bool bRagdoll=Vitality && Vitality->UsesRagdollPose();
    if (bRagdoll)
    {
        CancelASTPickup();
        CancelCharge();
        Action=EAction::None; if(Pending.IsValid())Pending->ReleasePickup(GetOwner()); Pending.Reset(); ActionTime=0;
        RigPose.Reset();
        if (PoseBridge) PoseBridge->ClearHandPose();
    }
    if (bCharging)
    {
        if (!HasBerry(ChargeSlot)) CancelCharge();
        else ChargeTime=FMath::Min(ChargeTime+Delta,FMath::Max(0.1f,FullChargeSeconds));
    }
    if (Action!=EAction::None) ActionTime+=Delta;
    if (bASTPickup)
    {
        RigPose.Reset();
        if (PoseBridge) PoseBridge->ClearHandPose();
        auto* Input=GetOwner()->FindComponentByClass<UPWGSidescrollerInputComponent>();
        const bool Interrupted=Input && (Input->IsTraversalActive() || (!bASTApproaching && !Input->IsASTInteractionActive()));
        if (!Pending.IsValid() || ActionTime>(bASTApproaching?4.f:3.f) || (ActionTime>.15f && Interrupted)
            || (!bCommitted && (!IsReachable(Pending.Get()) || HasBerry(ActionSlot)))) CancelASTPickup();
        else if(bASTApproaching && FMath::Abs(GetOwner()->GetActorLocation().Y-Pending->LaneY)<3.f)
        {
            if(Input)Input->CancelLaneMove();
            bASTApproaching=false;ActionTime=0;
            UActorComponent* State=nullptr;
            for(UActorComponent* C:GetOwner()->GetComponents())if(C && C->ComponentHasTag(TEXT("StateManager"))){State=C;break;}
            // BeginASTPickup already validated the reflected setter. Delay its
            // invocation until the swept Mover lane approach reaches the item.
            UFunction* Fn=State?State->FindFunction(TEXT("Set Interaction State")):nullptr;
            bool Started=false;
            if(Fn){FStructOnScope Args(Fn);for(TFieldIterator<FByteProperty> P(Fn);P;++P)
                if(P->HasAnyPropertyFlags(CPF_Parm) && !P->HasAnyPropertyFlags(CPF_ReturnParm))
                {P->SetPropertyValue_InContainer(Args.GetStructMemory(),ActionSlot==0?1:2);State->ProcessEvent(Fn,Args.GetStructMemory());Started=true;break;}}
            if(!Started)CancelASTPickup();
        }
    }
    else if(auto* I=Inventory();I && I->IsBackpackActive())
    {RigPose.Reset();if(PoseBridge)PoseBridge->ClearHandPose();}
    else if (!bRagdoll) UpdatePose(Delta);
    if (Action!=EAction::None && !bASTPickup)
    {
        const float Duration=FMath::Max(0.15f,Action==EAction::Pickup?PickupDuration:ThrowDuration);
        const float Commit=Action==EAction::Pickup?0.55f:0.60f;
        if (!bCommitted && ActionTime>=Duration*Commit)
        {
            bCommitted=true;
            if (Action==EAction::Pickup)
            {
                if (IsReachable(Pending.Get()) && !HasBerry(ActionSlot))
                {
                    auto* Sword=Cast<AProceduralSword>(Pending.Get());
                    auto* Combat=GetOwner()->FindComponentByClass<UDirectionalCombatComponent>();
                    if (Sword && Sword->GetIsReplicated() && Combat) Combat->ClaimWorldSword(Sword,ActionSlot);
                    else
                    {
                        Pending->Take(GetOwner());
                        if(Pending->IsHeld() && Pending->GetOwner()==GetOwner())SetItem(ActionSlot,Pending.Get());
                    }
                }
            }
            else if (IsValid(Item(ActionSlot)))
            {
                FVector Origin=HandLocation(ActionSlot);
                Origin.Y=GetOwner()->GetActorLocation().Y;
                FVector Difference=ActionTarget-Origin; Difference.Y=0;
                Difference=Difference.GetClampedToMaxSize(MaxThrowDistance);
                const float Time=FMath::Clamp(static_cast<float>(Difference.Size())/650.0f,0.3f,0.95f);
                FVector Speed=Difference/Time+FVector(0,0,490.0f*Time);
                if(auto* I=Inventory())I->ThrowNativeItem(ActionSlot,Origin,Speed);
            }
        }
        if (ActionTime>=Duration) { Action=EAction::None; if(Pending.IsValid())Pending->ReleasePickup(GetOwner()); Pending.Reset(); }
    }
    for (int32 Side=0;Side<2;++Side) if (IsValid(Item(Side)) && !(bASTPickup && Side==ActionSlot))
    {
        const FName Socket=Item(Side)->HandSocket(Side);
        FTransform Held(FRotator::ZeroRotator,HandLocation(Side));
        if (!Socket.IsNone() && Mesh && Mesh->DoesSocketExist(Socket))
        {
            Held=Mesh->GetSocketTransform(Socket);
            const auto* Carry=Item(Side)->FindComponentByClass<USirrohCarryPoseComponent>();
            const FName Hand=Carry?Carry->GripBone:(Side==0?TEXT("hand_r"):TEXT("hand_l"));
            if (const FTransform* VisualHand=RigPose.Find(Hand))
                Held=Held.GetRelativeTransform(Mesh->GetSocketTransform(Hand))*(*VisualHand);
        }
        const auto* Combat=GetOwner()->FindComponentByClass<UDirectionalCombatComponent>();
        if (!(Side==0 && Combat && Combat->OwnsMotionCoreWeapon())) Item(Side)->SetHeldTransform(Held);
        Item(Side)->UpdateInventoryBrush(BerryBrushes[Side]);
    }
    UpdateBackItems();
    for(int32 Slot=2;Slot<5;++Slot)if(auto* Stored=GetItem(Slot))Stored->UpdateInventoryBrush(BerryBrushes[Slot]);
    FindFocus();
}
void UPlaceholderHandsComponent::BuildHUD()
{
    const APawn* Pawn=Cast<APawn>(GetOwner());
    if (!Pawn || !Pawn->IsPlayerControlled() || !Pawn->IsLocallyControlled() || !GEngine || !GEngine->GameViewport) return;
    FrameTexture=LoadObject<UTexture2D>(nullptr,TEXT("/Game/Systems/Sprite2Bone/UI/UI_SmallSquare_B.UI_SmallSquare_B"));
    BerryTexture=LoadObject<UTexture2D>(nullptr,TEXT("/Game/Systems/Inventory/Icons/Berry_Atlas.Berry_Atlas"));
    FrameBrush.SetResourceObject(FrameTexture); FrameBrush.ImageSize=FVector2D(72,72); FrameBrush.DrawAs=ESlateBrushDrawType::Image;
    for (FSlateBrush& Brush:BerryBrushes)
    { Brush.SetResourceObject(BerryTexture); Brush.ImageSize=FVector2D(32,32); Brush.DrawAs=ESlateBrushDrawType::Image; }
    TWeakObjectPtr<UPlaceholderHandsComponent> WeakThis(this);
    auto Slot=[this,WeakThis](int32 Index,const TCHAR* Label)->TSharedRef<SWidget>
    {
        return SNew(SVerticalBox)
            +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
            [SNew(STextBlock).Text(FText::FromString(Label)).Font(FCoreStyle::GetDefaultFontStyle("Bold",12))]
            +SVerticalBox::Slot().AutoHeight()
            [SNew(SBox).WidthOverride(72).HeightOverride(72)
                [SNew(SOverlay)
                    +SOverlay::Slot()[SNew(SImage).Image(&FrameBrush)]
                    +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
                    [SNew(SBox).WidthOverride_Lambda([WeakThis,Index](){return WeakThis.IsValid() && Cast<AProceduralSword>(WeakThis->Item(Index))?52.0f:32.0f;})
                        .HeightOverride_Lambda([WeakThis,Index](){return WeakThis.IsValid() && Cast<AProceduralSword>(WeakThis->Item(Index))?52.0f:32.0f;})
                        [SNew(SImage).Image(&BerryBrushes[Index]).Visibility_Lambda([WeakThis,Index]()
                            { return WeakThis.IsValid()&&WeakThis->HasBerry(Index)?EVisibility::HitTestInvisible:EVisibility::Hidden; })]]]];
    };
    HUD=SNew(SOverlay).Visibility(EVisibility::HitTestInvisible)
        +SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(24)
        [SNew(SVerticalBox)
            +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0,0,0,8)
                [SNew(STextBlock).Text_Lambda([WeakThis]() { return WeakThis.IsValid()?WeakThis->GetHint():FText::GetEmpty(); })
                    .ShadowOffset(FVector2D(1,1)).ShadowColorAndOpacity(FLinearColor::Black)]
            +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
                [SNew(SHorizontalBox)
                    +SHorizontalBox::Slot().AutoWidth().Padding(0,0,8,0)[Slot(1,TEXT("L"))]
                    +SHorizontalBox::Slot().AutoWidth().Padding(0,0,8,0)[Slot(0,TEXT("P"))]
                    +SHorizontalBox::Slot().AutoWidth().Padding(0,0,8,0)[Slot(2,TEXT("1"))]
                    +SHorizontalBox::Slot().AutoWidth().Padding(0,0,8,0)[Slot(3,TEXT("2"))]
                    +SHorizontalBox::Slot().AutoWidth()[Slot(4,TEXT("3"))]]
            +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(TEXT("1-3: sloty | F x2: zamien rece | F trzymaj: schowaj | Q: rzut")))]];
    GEngine->GameViewport->AddViewportWidgetContent(HUD.ToSharedRef(),100);
}

namespace
{
UActorComponent* PickupComponent(AActor* Owner,FName Tag)
{
    if (Owner) for(UActorComponent* C:Owner->GetComponents()) if(C && C->ComponentHasTag(Tag))return C;
    return nullptr;
}
AActor* PickupClosest(UActorComponent* State)
{
    if(State) if(auto* P=FindFProperty<FObjectPropertyBase>(State->GetClass(),TEXT("ClosestActor")))
        return Cast<AActor>(P->GetObjectPropertyValue_InContainer(State));
    return nullptr;
}
bool PickupState(UActorComponent* State,uint8 Value)
{
    UFunction* Fn=State?State->FindFunction(TEXT("Set Interaction State")):nullptr;
    if(!Fn)return false;
    FStructOnScope Args(Fn);
    for(TFieldIterator<FByteProperty> P(Fn);P;++P) if(P->HasAnyPropertyFlags(CPF_Parm) && !P->HasAnyPropertyFlags(CPF_ReturnParm))
    {P->SetPropertyValue_InContainer(Args.GetStructMemory(),Value);State->ProcessEvent(Fn,Args.GetStructMemory());return true;}
    return false;
}
}
bool UPlaceholderHandsComponent::BeginASTPickup()
{
    auto* State=PickupComponent(GetOwner(),TEXT("StateManager"));
    auto* Inventory=PickupComponent(GetOwner(),TEXT("InventorySystem"));
    auto* Target=Focus.Get();
    auto* Input=GetOwner()->FindComponentByClass<UPWGSidescrollerInputComponent>();
    if(!State || !Inventory || !Cast<USirrohASTInventoryComponent>(Inventory) || !Cast<USirrohASTInventoryComponent>(Inventory)->IsNativeInventoryReady() || !IsReachable(Target) || (Input && (Input->IsASTInteractionActive() || Input->IsTraversalActive())))return false;
    const auto* Health=GetOwner()->FindComponentByClass<UCombatVitalityComponent>();
    if(Health && Health->IsIncapacitated())return false;
    int32 Slot=INDEX_NONE;
    for(int32 Side=0;Side<2;++Side)if(!HasBerry(Side) && (!Health || Health->CanUseArm(Side==1))){Slot=Side;break;}
    if(const auto* Carry=Target->FindComponentByClass<USirrohCarryPoseComponent>())
    {
        Slot=Carry->HandSlot();
        if(HasBerry(Slot) || (Health && !Health->CanUseArm(Slot==1)))return false;
        if(Carry->bTwoHanded && (HasBerry(1-Slot) || (Health && !Health->CanUseArm(Slot==0))))return false;
    }
    if(Slot==INDEX_NONE)return false;
    auto* Loot=FindFProperty<FObjectPropertyBase>(Inventory->GetClass(),Slot==0?TEXT("RightLootSlot"):TEXT("LeftLootSlot"));
    UFunction* SetClosest=State->FindFunction(TEXT("Set Closest Actor"));
    if(!Loot || !IsValid(Loot->GetObjectPropertyValue_InContainer(Inventory)) || !SetClosest)return false;
    FStructOnScope Args(SetClosest);
    auto* ActorArg=FindFProperty<FObjectPropertyBase>(SetClosest,TEXT("NewClosestActor"));
    if(!ActorArg || !State->FindFunction(TEXT("Set Interaction State")) || !Target->ReservePickup(GetOwner(),8.f))return false;
    ActorArg->SetObjectPropertyValue_InContainer(Args.GetStructMemory(),Target);
    State->ProcessEvent(SetClosest,Args.GetStructMemory());
    Action=EAction::Pickup;ActionSlot=Slot;ActionTime=0;bCommitted=false;bASTPickup=true;Pending=Target;
    // Loot crosses visual depth at attachment; never move the player into the background.
    bASTApproaching=false;
    // Verified E_InteractionState: None=0, RH Loot=1, LH Loot=2.
    if(!PickupState(State,Slot==0?1:2)){CancelASTPickup();return false;}
    return true;
}
void UPlaceholderHandsComponent::CancelASTPickup()
{
    if(!bASTPickup)return;
    if(Pending.IsValid())Pending->ReleasePickup(GetOwner());
    if(bASTApproaching)if(auto* Input=GetOwner()->FindComponentByClass<UPWGSidescrollerInputComponent>())Input->CancelLaneMove();
    bASTApproaching=false;
    auto* State=PickupComponent(GetOwner(),TEXT("StateManager"));
    // Do not reset a later interaction that has already replaced our target.
    if(State && PickupClosest(State)==Pending.Get())
    {
        if(auto* Picked=FindFProperty<FBoolProperty>(State->GetClass(),TEXT("PickedClosedItem")))Picked->SetPropertyValue_InContainer(State,false);
        PickupState(State,0);
    }
    bASTPickup=false;Action=EAction::None;ActionTime=0;Pending.Reset();
}
bool UPlaceholderHandsComponent::RouteASTLootAttach(UObject* SourceContext)
{
    auto* Inventory=Cast<UActorComponent>(SourceContext);
    auto* Owner=Inventory?Inventory->GetOwner():nullptr;
    auto* State=PickupComponent(Owner,TEXT("StateManager"));
    auto* Target=Cast<ASirrohWorldItem>(PickupClosest(State));
    if(!Target)return false; // Preserve all existing AST items.
    auto* Hands=Owner->FindComponentByClass<UPlaceholderHandsComponent>();
    if(!Owner->HasAuthority() || !Hands || !Hands->bASTPickup || Hands->Pending.Get()!=Target)return true;
    if(Hands->bCommitted)return true;
    if(!Hands->IsReachable(Target) || Hands->HasBerry(Hands->ActionSlot)) { Hands->CancelASTPickup();return true; }
    if(const auto* Health=Owner->FindComponentByClass<UCombatVitalityComponent>();Health && (Health->IsIncapacitated() || !Health->CanUseArm(Hands->ActionSlot==1)))
    {Hands->CancelASTPickup();return true;}
    auto* Slot=FindFProperty<FObjectPropertyBase>(Inventory->GetClass(),Hands->ActionSlot==0?TEXT("RightLootSlot"):TEXT("LeftLootSlot"));
    auto* Parent=Slot?Cast<USceneComponent>(Slot->GetObjectPropertyValue_InContainer(Inventory)):nullptr;
    if(!IsValid(Parent) || !Target->CommitPickup(Owner)){Hands->CancelASTPickup();return true;}
    if(!Hands->SetItem(Hands->ActionSlot,Target))
    {Target->Throw(Target->GetActorLocation(),FVector::ZeroVector,Owner);Hands->CancelASTPickup();return true;}
    Hands->bCommitted=true;
    Target->AttachToComponent(Parent,FAttachmentTransformRules::SnapToTargetNotIncludingScale);
    return true;
}
bool UPlaceholderHandsComponent::RouteASTLootStore(UObject* SourceContext)
{
    auto* Inventory=Cast<UActorComponent>(SourceContext);
    // AST Add Item destroys consumable actors. Native instances must retain their
    // procedural data and persistent identity after the contact notify takes them.
    return Inventory && Cast<ASirrohWorldItem>(PickupClosest(PickupComponent(Inventory->GetOwner(),TEXT("StateManager"))))!=nullptr;
}

bool UPlaceholderHandsComponent::RouteASTLootIK(UObject* SourceContext)
{
    auto* State=Cast<UActorComponent>(SourceContext);
    auto* Target=Cast<ASirrohWorldItem>(PickupClosest(State));
    if(!Target)return false;
    auto* Hands=State->GetOwner()->FindComponentByClass<UPlaceholderHandsComponent>();
    if(!Hands || !Hands->bASTPickup || Hands->Pending.Get()!=Target)return true;
    if(auto* P=FindFProperty<FStructProperty>(State->GetClass(),Hands->ActionSlot==0?TEXT("IKTargetWS_R_Static"):TEXT("IKTargetWS_L_Static"));
        P && P->Struct==TBaseStructure<FVector>::Get())
    {
        FVector Contact=Target->GetActorLocation();Contact.Y=State->GetOwner()->GetActorLocation().Y;
        *P->ContainerPtrToValuePtr<FVector>(State)=Contact;
    }
    return true;
}

ASirrohWorldItem* UPlaceholderHandsComponent::GetPickupCandidate()
{
    if(bASTPickup || (HasBerry(0) && HasBerry(1)))return nullptr;
    if(const auto* Combat=GetOwner()->FindComponentByClass<UDirectionalCombatComponent>();Combat && Combat->IsCombatActive())return nullptr;
    if(const auto* Health=GetOwner()->FindComponentByClass<UCombatVitalityComponent>();Health && (Health->IsIncapacitated()
        || ((HasBerry(0) || !Health->CanUseArm(false)) && (HasBerry(1) || !Health->CanUseArm(true)))))return nullptr;
    FindFocus();return Focus.Get();
}

bool UPlaceholderHandsComponent::RequestPickup(ASirrohWorldItem* Target)
{
    if(!GetOwner()->HasAuthority() || Action!=EAction::None || bCharging || !IsReachable(Target))return false;
    Focus=Target;return BeginASTPickup();
}
