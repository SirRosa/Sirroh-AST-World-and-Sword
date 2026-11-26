#include "PWG/PWGPlayerController.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "UObject/UnrealType.h"
#include "UObject/StructOnScope.h"
#include "Engine/OverlapResult.h"
#include "Components/ChildActorComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SplineComponent.h"
#include "Components/MeshComponent.h"
#include "POI/POIKitBlock.h"
#include "POI/POICorridorTools.h"
#include "POI/POIPushTraversalComponent.h"
#include "POI/POIMovingTraversalComponent.h"
#include "ProceduralMeshComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "DefaultMovementSet/InstantMovementEffects/BasicInstantMovementEffects.h"
#include "Debug/SirrohDebugSubsystem.h"
#include "Procedural_World_Generator/PWGv2Generator.h"
#include "Combat/CombatVitalityComponent.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/Pawn.h"
#include "MoverComponent.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "MoveLibrary/ConstrainedMoveUtils.h"
#include "PWG/PWGGameMode.h"
#include "PWG/PWGSidescrollerCamera.h"
#include "PWG/PWGSidescrollerInputComponent.h"
#include "UI/SirrohUISubsystem.h"
#include "InputCoreTypes.h"
#include "Placeholder/PlaceholderHandsComponent.h"
#include "Combat/DirectionalCombatComponent.h"
#include "Sword/ProceduralSword.h"
#include "Items/SirrohWorldItem.h"
#include "Items/SirrohASTInventoryComponent.h"
#include "Kismet/GameplayStatics.h"

void APWGPlayerController::SirrohPOIChain(int32 Index)
{
    APawn* ControlledPawn=GetPawn();
    auto* Mode=GetWorld()->GetAuthGameMode<APWGGameMode>();
    auto* Generator=Mode?Mode->GetWorldGenerator():nullptr;
    if (!ControlledPawn || !Generator) return;
    auto* Input=ControlledPawn->FindComponentByClass<UPWGSidescrollerInputComponent>();
    if (Input && (Input->IsTraversalActive() || Input->IsASTInteractionActive()))
    { ClientMessage(TEXT("Finish the AST interaction before teleporting.")); return; }
    auto* Capsule=ControlledPawn->FindComponentByClass<UCapsuleComponent>();
    auto* Mover=ControlledPawn->FindComponentByClass<UMoverComponent>();
    if (!Capsule || !Mover) return;
    FVector Destination;
    if (!Generator->DebugRoomChainDestination(Index,Capsule->GetScaledCapsuleHalfHeight(),Destination))
    { ClientMessage(TEXT("No chain at that index. Create a NEW world after publishing Underground fragments.")); return; }
    if (Input) Input->CancelLaneMove();
    bLaneSwitching=false; CurrentTerrainLane=EPWGTerrainLane::Middle;
    auto Constraint=Mover->GetPlanarConstraint(); Constraint.PlaneConstraintOrigin=Destination; Mover->SetPlanarConstraint(Constraint);
    auto Stop=MakeShared<FApplyVelocityEffect>(); Stop->VelocityToApply=FVector::ZeroVector; Stop->bAdditiveVelocity=false; Mover->QueueInstantMovementEffect(Stop);
    auto Teleport=MakeShared<FTeleportEffect>(); Teleport->TargetLocation=Destination; Teleport->bUseActorRotation=true; Mover->QueueInstantMovementEffect(Teleport);
    ClientMessage(FString::Printf(TEXT("POI chain %d. Walk right to test the complete room sequence."),Index));
}

namespace
{
    void RefreshASTPossession(APawn* ControlledPawn)
    {
        if (!ControlledPawn) return;
        // PWG finishes spawning before Possess. AST cached local ownership in
        // BeginPlay, so refresh that cache when the controller actually changes.
        for (UActorComponent* Component:ControlledPawn->GetComponents())
            if (Component && Component->ComponentHasTag(TEXT("StateManager")))
                if (UFunction* Refresh=Component->FindFunction(TEXT("Set Local Controlled")))
                    Component->ProcessEvent(Refresh,nullptr);
    }
}

APWGPlayerController::APWGPlayerController()
{
	SidescrollerCameraClass = APWGSidescrollerCamera::StaticClass();
	PrimaryActorTick.bTickEvenWhenPaused = true;
}

void APWGPlayerController::PlayerTick(const float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
    SyncASTInputContexts();
    if (IsLocalController() && WasInputKeyJustPressed(EKeys::NumPadNine)) SirrohGroundCompare();
    if (IsLocalController() && WasInputKeyJustPressed(EKeys::NumPadSeven)) SirrohGroundReport();
    if (IsLocalController() && WasInputKeyJustPressed(EKeys::NumPadEight)) SirrohASTControls(!bUseASTControls);
    if (auto* Debug=GetGameInstance()?GetGameInstance()->GetSubsystem<USirrohDebugSubsystem>():nullptr)
    {
        if (IsLocalController() && WasInputKeyJustPressed(EKeys::F7)) Debug->Toggle(this);
        if (Debug->IsOpen() && WasInputKeyJustPressed(EKeys::Escape)) { Debug->Close(); return; }
        if (Debug->IsEditingText()) return;
    }
    if(APawn* P=GetPawn())if(auto* Inventory=P->FindComponentByClass<USirrohASTInventoryComponent>();Inventory && Inventory->IsBackpackActive() && WasInputKeyJustPressed(EKeys::Escape))
    {Inventory->ToggleBackpack();return;}
	if (WasInputKeyJustPressed(EKeys::Escape))
	{
		if (USirrohUISubsystem* UI = GetGameInstance() ? GetGameInstance()->GetSubsystem<USirrohUISubsystem>() : nullptr)
		{
			UI->TogglePauseMenu(this);
		}
	}
    if (GetWorld()->GetTimeSeconds()>=NextASTPromptUpdate)
    {
        NextASTPromptUpdate=GetWorld()->GetTimeSeconds()+0.1;
        UpdateASTInteractionPrompt();
    }
	APawn* ControlledPawn=GetPawn();
	if (CombatKeyPawn.Get()!=ControlledPawn) { PendingCombatKeyTime=-1; CombatKeyPawn=ControlledPawn; }
	if(!ControlledPawn) { ClearAudioListenerAttenuationOverride(); return; }
    // The foreground cutaway and middle lane share one playable shaft.
    if (auto* Mode=GetWorld()->GetAuthGameMode<APWGGameMode>())
    if (const auto* Generator=Mode->GetWorldGenerator())
    if (auto* Producer=ControlledPawn->FindComponentByClass<UPWGSidescrollerInputComponent>(); Producer && !Producer->IsTraversalActive() && !Producer->IsASTInteractionActive())
    if (auto* Capsule=ControlledPawn->FindComponentByClass<UCapsuleComponent>())
    {
        FVector Destination;
        if (Generator->GetShaftLaneDestination(ControlledPawn->GetActorLocation(),Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight(),Destination))
        if (auto* Mover=ControlledPawn->FindComponentByClass<UMoverComponent>())
        {
            Producer->CancelLaneMove(); bLaneSwitching=false; CurrentTerrainLane=EPWGTerrainLane::Middle;
            auto Constraint=Mover->GetPlanarConstraint();
            Constraint.PlaneConstraintOrigin=Destination;
            Mover->SetPlanarConstraint(Constraint);
            auto Effect=MakeShared<FTeleportEffect>(); Effect->TargetLocation=Destination; Effect->bUseActorRotation=true;
            Mover->QueueInstantMovementEffect(Effect);
            return; // Let Mover apply relocation before attempting AST acquisition.
        }
    }
    // Orthographic camera is ~18m away; distance attenuation belongs at the player.
    // Keep the camera listener orientation for stereo placement.
    if (IsLocalController() && Cast<APWGSidescrollerCamera>(GetViewTarget()))
        SetAudioListenerAttenuationOverride(nullptr,ControlledPawn->GetActorLocation());
    else ClearAudioListenerAttenuationOverride();
    if (WasInputKeyJustPressed(EKeys::NumPadZero)) ServerResetTrainingNPC();
    if (const auto* H=ControlledPawn->FindComponentByClass<UCombatVitalityComponent>(); H && H->IsIncapacitated()) return;
	if (UGameplayStatics::IsGamePaused(this))
	{
        HandleASTCombatInput(false);
		PendingCombatKeyTime=-1;
		if (UPlaceholderHandsComponent* Hands=ControlledPawn->FindComponentByClass<UPlaceholderHandsComponent>())
		{ Hands->CancelCharge(); Hands->CancelBackInput(); }
		return;
	}
    if(IsLocalController() && IsValid(SidescrollerCamera) && GetViewTarget()==SidescrollerCamera){
        const auto* Debug=GetGameInstance()?GetGameInstance()->GetSubsystem<USirrohDebugSubsystem>():nullptr;
        if(!Debug || !Debug->IsPointerOverMenu()){
            float Wheel=GetInputAnalogKeyState(EKeys::MouseWheelAxis);
            if(FMath::IsNearlyZero(Wheel))Wheel=WasInputKeyJustPressed(EKeys::MouseScrollUp)?1.f:(WasInputKeyJustPressed(EKeys::MouseScrollDown)?-1.f:0.f);
            if(!FMath::IsNearlyZero(Wheel))SidescrollerCamera->AddScrollZoom(Wheel);
        }
    }
    if (bUseASTControls) return;
    const bool bTraverse = IsLocalController() &&
        (WasInputKeyJustPressed(EKeys::SpaceBar) || WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Bottom));
    auto* TraversalInput = ControlledPawn->FindComponentByClass<UPWGSidescrollerInputComponent>();
    if (GetWorld()->GetTimeSeconds()>=NextPlayerDepthUpdate)
    {
        NextPlayerDepthUpdate=GetWorld()->GetTimeSeconds()+1;
        TArray<UMeshComponent*> Meshes; ControlledPawn->GetComponents(Meshes);
        for (auto* Mesh:Meshes) { Mesh->SetRenderCustomDepth(true);Mesh->SetCustomDepthStencilValue(254); }
    }
    if (bManualDropNeedsGrab && GetWorld()->GetTimeSeconds()-LastASTDrop>.1)
        if (auto* Mover=ControlledPawn->FindComponentByClass<UMoverComponent>(); Mover && Mover->GetMovementModeName()==TEXT("Walking")) bManualDropNeedsGrab=false;
    FinishProjectedHop();
    if (IsLocalController() && WasInputKeyJustPressed(EKeys::S) && TraversalInput && !TraversalInput->IsTraversalActive())
    {
        const double Now=GetWorld()->GetTimeSeconds();
        if (Now-LastPlatformDownTap<=.3) { TryDropThroughPlatform();LastPlatformDownTap=-1000; }
        else LastPlatformDownTap=Now;
    }
    const bool GrabHeld=IsInputKeyDown(EKeys::SpaceBar) || IsInputKeyDown(EKeys::W) || IsInputKeyDown(EKeys::Gamepad_FaceButton_Bottom);
    if (bManualDropNeedsGrab && WasInputKeyJustPressed(EKeys::W)) {bManualDropNeedsGrab=false;LastASTDrop=-1000;}
    // Ground crouch/drop remains owned by AST's inherited input action.
    const bool bDrop = IsLocalController() && TraversalInput && TraversalInput->IsTraversalActive() &&
        (WasInputKeyJustPressed(EKeys::C) || WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Right));
    if (bTraverse) UPOICorridorTools::ReleaseHeldPushable(ControlledPawn);
    if (bTraverse || bDrop) SirrohParkour(bDrop);
    // The screen-space selector is the single air-acquisition path in Sirroh mode.
    // Otherwise AST's parallel automatic probe can grab a different edge first.
    for (auto* Component:ControlledPawn->GetComponents())
        if (Component && Component->GetClass()->GetName().StartsWith(TEXT("BPC_ParkourSystem")))
            if (auto* AutoGrab=FindFProperty<FBoolProperty>(Component->GetClass(),TEXT("CanAirClimb"))) AutoGrab->SetPropertyValue_InContainer(Component,false);
    if (GrabHeld && !bTraverse && !bManualDropNeedsGrab && TraversalInput && !TraversalInput->IsTraversalActive() && GetWorld()->GetTimeSeconds()>=NextASTAirProbe)
    {
        NextASTAirProbe=GetWorld()->GetTimeSeconds()+.08;
        if (auto* Mover=ControlledPawn->FindComponentByClass<UMoverComponent>(); Mover && Mover->GetMovementModeName()==TEXT("Falling"))
            TryProjectedASTLedge(false);
    }
    if (TraversalInput && TraversalInput->IsTraversalActive())
    {
        TraversalInput->CancelLaneMove();
        bLaneSwitching = false;
        return;
    }
    if(auto* Inventory=ControlledPawn->FindComponentByClass<USirrohASTInventoryComponent>())
    {
        if(IsLocalController() && WasInputKeyJustPressed(EKeys::I))Inventory->ToggleBackpack();
        if(Inventory->IsBackpackActive())
        {
            HandleASTCombatInput(false);
            const FKey Keys[]={EKeys::One,EKeys::Two,EKeys::Three};
            for(int32 Slot=0;Slot<3;++Slot)if(WasInputKeyJustPressed(Keys[Slot]))Inventory->SelectBackpackSlot(Slot);
            if(auto* Hands=ControlledPawn->FindComponentByClass<UPlaceholderHandsComponent>()){Hands->CancelCharge();Hands->CancelBackInput();}
            return;
        }
    }
    if(auto* Hands=ControlledPawn->FindComponentByClass<UPlaceholderHandsComponent>();Hands && Hands->IsASTPickupActive())
        if(auto* Input=ControlledPawn->FindComponentByClass<UPWGSidescrollerInputComponent>();Input && !Input->IsASTInteractionActive())
            return; // Mover finishes the reserved lane approach before another action.
    const bool bASTConsumedInteract=IsLocalController() &&
        (WasInputKeyJustPressed(EKeys::E) || WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Top)) && TryASTSideInteraction();
    // AST motion warping owns the pawn while interacting; do not also attack or change lanes.
    if (auto* Input=ControlledPawn->FindComponentByClass<UPWGSidescrollerInputComponent>();
        Input && Input->IsASTInteractionActive())
    {
        HandleASTCombatInput(false);
        bWasASTInteracting=true;
        Input->CancelLaneMove(); bLaneSwitching=false;
        if (auto* Hands=ControlledPawn->FindComponentByClass<UPlaceholderHandsComponent>())
        { Hands->CancelCharge(); Hands->CancelBackInput(); }
        return;
    }
    if (bWasASTInteracting)
    {
        bWasASTInteracting=false;
        if (const auto* Mode=GetWorld()->GetAuthGameMode<APWGGameMode>())
            if (const auto* Generator=Mode->GetWorldGenerator())
            {
                float Closest=MAX_flt;
                for(int32 I=0;I<3;++I)
                {
                    const auto Lane=static_cast<EPWGTerrainLane>(I);
                    const float Distance=FMath::Abs(ControlledPawn->GetActorLocation().Y-Generator->GetLaneWorldY(Lane));
                    if(Distance<Closest){Closest=Distance;CurrentTerrainLane=Lane;}
                }
            }
    }
	if (!bShowMouseCursor)
	{
		bShowMouseCursor=true;
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::LockOnCapture);
		SetInputMode(InputMode);
	}
	if (UPlaceholderHandsComponent* Hands=ControlledPawn->FindComponentByClass<UPlaceholderHandsComponent>())
	{
        if(!bLaneSwitching && !Hands->IsASTPickupActive())
        {
            const FKey Slots[]={EKeys::One,EKeys::Two,EKeys::Three};
            for(int32 I=0;I<3;++I)if(WasInputKeyJustPressed(Slots[I]))Hands->SelectBackSlot(I);
        }
		if (bLaneSwitching) Hands->CancelBackInput();
		else Hands->HandleBackInput(WasInputKeyJustPressed(EKeys::F),IsInputKeyDown(EKeys::F),WasInputKeyJustReleased(EKeys::F));
		if (auto* Combat=ControlledPawn->FindComponentByClass<UDirectionalCombatComponent>())
		{
            const bool bNativeWeapon=IsValid(Cast<AProceduralSword>(Hands->GetItem(0))) || IsValid(Cast<AProceduralSword>(Hands->GetItem(1)));
            if (!bNativeWeapon && Combat->IsCombatActive()) Combat->LeaveCombat();
			// R is exclusively the double-tap grip gesture; Alt controls guard.
			const double KeyTime=GetWorld()->GetRealTimeSeconds();
			if (bLaneSwitching || WasInputKeyJustPressed(EKeys::Q)) PendingCombatKeyTime=-1;
			if (PendingCombatKeyTime>=0 && KeyTime-PendingCombatKeyTime>0.25)
			{ PendingCombatKeyTime=-1; }
			if (!bLaneSwitching && !WasInputKeyJustPressed(EKeys::Q) && WasInputKeyJustPressed(EKeys::R))
			{
				if (PendingCombatKeyTime>=0)
				{ PendingCombatKeyTime=-1; Combat->ToggleGripMode(); }
				else PendingCombatKeyTime=KeyTime;
			}
			// Changing lanes preserves the combat stance; throwing still leaves combat.
            if (WasInputKeyJustPressed(EKeys::Q)) Combat->LeaveCombat();
            if (WasInputKeyJustPressed(EKeys::NumPadFive)) Combat->CancelAttack();
            if (WasInputKeyJustPressed(EKeys::Comma)) Combat->CycleCombatStyle(-1);
            if (WasInputKeyJustPressed(EKeys::Period)) Combat->CycleCombatStyle(1);
            if (bNativeWeapon && (WasInputKeyJustPressed(EKeys::LeftAlt) || WasInputKeyJustPressed(EKeys::RightAlt))) Combat->ToggleCombat();
            if (!bLaneSwitching && Combat->IsCombatActive())
            {
                const auto* SideInput=ControlledPawn->FindComponentByClass<UPWGSidescrollerInputComponent>();
                const FVector Right=SideInput?SideInput->GetScreenRightDirection():-FVector::XAxisVector;
                if (WasInputKeyJustPressed(EKeys::A))
                { if (KeyTime-LastDodgeLeft<=0.28) { Combat->RequestLibraryAction(TEXT("Dodge"),-Right); LastDodgeLeft=-1000; } else LastDodgeLeft=KeyTime; }
                if (WasInputKeyJustPressed(EKeys::D))
                { if (KeyTime-LastDodgeRight<=0.28) { Combat->RequestLibraryAction(TEXT("Dodge"),Right); LastDodgeRight=-1000; } else LastDodgeRight=KeyTime; }
                if (WasInputKeyJustPressed(EKeys::G)) Combat->RequestLibraryAction(TEXT("Taunt"));
                if (!bASTConsumedInteract && WasInputKeyJustPressed(EKeys::E)) Combat->RequestLibraryAction(TEXT("Kick"));
            }
            const bool bAllowCombatClick=!Combat->IsStyleControlsHovered() && !(GetGameInstance() && GetGameInstance()->GetSubsystem<USirrohDebugSubsystem>()->IsPointerOverMenu());
            if (!bNativeWeapon) HandleASTCombatInput(bAllowCombatClick);
            if (bNativeWeapon && bAllowCombatClick)
            {
            if (WasInputKeyJustPressed(EKeys::LeftMouseButton)) Combat->BeginAttack(EDirectionalAttack::Light);
			else if (WasInputKeyJustPressed(EKeys::RightMouseButton) && !Combat->TryDefenseParry()) Combat->BeginAttack(EDirectionalAttack::Heavy);
			else if (WasInputKeyJustPressed(EKeys::MiddleMouseButton)) Combat->BeginAttack(EDirectionalAttack::Pierce);
            }
            if (bNativeWeapon && WasInputKeyJustReleased(EKeys::LeftMouseButton)) Combat->ReleaseAttack(EDirectionalAttack::Light);
			if (bNativeWeapon && WasInputKeyJustReleased(EKeys::RightMouseButton)) Combat->ReleaseAttack(EDirectionalAttack::Heavy);
			if (bNativeWeapon && WasInputKeyJustReleased(EKeys::MiddleMouseButton)) Combat->ReleaseAttack(EDirectionalAttack::Pierce);
		}
		if (WasInputKeyJustPressed(EKeys::NumPadFive)) Hands->DebugSpawnSword();
		if (bLaneSwitching) Hands->CancelCharge();
		else Hands->HandleInput(!bASTConsumedInteract && WasInputKeyJustPressed(EKeys::E) && (!ControlledPawn->FindComponentByClass<UDirectionalCombatComponent>() || !ControlledPawn->FindComponentByClass<UDirectionalCombatComponent>()->IsCombatActive()),WasInputKeyJustPressed(EKeys::Q),
			IsInputKeyDown(EKeys::Q),WasInputKeyJustReleased(EKeys::Q));
	}
    if (bLaneSwitching)
    {
        LaneSwitchElapsed+=DeltaTime;
        if (auto* Producer=ControlledPawn->FindComponentByClass<UPWGSidescrollerInputComponent>())
        {
            if (LaneSwitchElapsed>2) Producer->CancelLaneMove();
            if (!Producer->IsLaneMoveActive())
            {
                bLaneSwitching=false;
                if (FMath::Abs(ControlledPawn->GetActorLocation().Y-LaneSwitchTargetY)<5) CurrentTerrainLane=PendingTerrainLane;
            }
        }
        else bLaneSwitching=false;
        return;
    }
	const int32 Direction=WasInputKeyJustPressed(EKeys::W)?1:(WasInputKeyJustPressed(EKeys::S)?-1:0);
	if(Direction!=0)
	{
		if(const APWGGameMode* GameMode=GetWorld()?GetWorld()->GetAuthGameMode<APWGGameMode>():nullptr)
		{
			if(APWGv2Generator* Generator=GameMode->GetWorldGenerator())
			{
				EPWGTerrainLane TargetLane;
				FVector Destination;
				auto* Producer=ControlledPawn->FindComponentByClass<UPWGSidescrollerInputComponent>();
                if(Producer && Generator->FindLaneSwitchDestination(ControlledPawn->GetActorLocation(),Direction,TargetLane,Destination))
				{
					bLaneSwitching=true;
					LaneSwitchElapsed=0.0f;
					LaneSwitchStartY=ControlledPawn->GetActorLocation().Y;
					LaneSwitchTargetY=Destination.Y;
					PendingTerrainLane=TargetLane;
                    Producer->BeginLaneMove(Destination.Y);
				}
			}
		}
	}
}

void APWGPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
    RefreshASTPossession(InPawn);
	if (InPawn && HasAuthority() && !InPawn->FindComponentByClass<UDirectionalCombatComponent>())
	{
		InPawn->SetReplicates(true);
		auto* Combat=NewObject<UDirectionalCombatComponent>(InPawn,TEXT("DirectionalCombatPhysics"));
		InPawn->AddInstanceComponent(Combat);
		Combat->RegisterComponent();
	}
	ConfigureSidescrollerPawn(InPawn);
}

void APWGPlayerController::AcknowledgePossession(APawn* InPawn)
{
	Super::AcknowledgePossession(InPawn);
    RefreshASTPossession(InPawn);
	if (IsLocalController() && !HasAuthority()) ConfigureSidescrollerPawn(InPawn);
}

void APWGPlayerController::OnUnPossess()
{
    SetASTInteractionPrompt(nullptr);
    bOwnsASTPrompt=false;
    APawn* PreviousPawn=GetPawn();
    if (GetLocalPlayer())
        if (auto* Subsystem=GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
        {
            for (UInputMappingContext* Context:PWGInteractionContexts) Subsystem->RemoveMappingContext(Context);
            for (UInputMappingContext* Context:PWGClimbContexts) Subsystem->RemoveMappingContext(Context);
        }

	if (IsValid(SidescrollerCamera))
	{
		SidescrollerCamera->Destroy();
		SidescrollerCamera = nullptr;
	}
    if (IsValid(GroundComparisonPlatform)) GroundComparisonPlatform->Destroy();
    GroundComparisonPlatform = nullptr;
	Super::OnUnPossess();
    RefreshASTPossession(PreviousPawn);
}

void APWGPlayerController::ConfigureSidescrollerPawn(APawn* InPawn)
{
	if (!InPawn) return;
    if (bUseASTControls)
    {
        if (UMoverComponent* Mover = InPawn->FindComponentByClass<UMoverComponent>())
        {
            auto Constraint = Mover->GetPlanarConstraint();
            Constraint.bConstrainToPlane = false;
            Mover->SetPlanarConstraint(Constraint);
        }
        if (IsLocalController())
        {
            SetViewTargetWithBlend(InPawn, CameraBlendTime);
            bShowMouseCursor = false;
            SetInputMode(FInputModeGameOnly());
        }
        if (IsValid(SidescrollerCamera)) SidescrollerCamera->Destroy();
        SidescrollerCamera = nullptr;
        return;
    }
	const float LockedY = InPawn->GetActorLocation().Y;
	CurrentTerrainLane=EPWGTerrainLane::Middle;
	bLaneSwitching=false;
	SetPawnLaneConstraintY(InPawn,LockedY);
	if (UMoverComponent* Mover = InPawn->FindComponentByClass<UMoverComponent>())
	{
		// The procedural surface consists of short sloped triangles. Give Mover a
		// longer floor probe and explicitly accept ordinary terrain slopes, so a
		// fast airborne capsule transitions back to Walking instead of continuing
		// in Falling while touching the hill.
		if (UCommonLegacyMovementSettings* Settings = Mover->FindSharedSettings_Mutable<UCommonLegacyMovementSettings>())
		{
			Settings->MaxWalkSlopeCosine = FMath::Cos(FMath::DegreesToRadians(55.0f));
			Settings->FloorSweepDistance = FMath::Max(Settings->FloorSweepDistance, 75.0f);
			Settings->MaxStepHeight = FMath::Max(Settings->MaxStepHeight, 50.0f);
			Settings->GroundFriction = FMath::Max(Settings->GroundFriction, 10.0f);
			Settings->bUseFlatBaseForFloorChecks = true;
		}
	}

	bShowMouseCursor = true;
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::LockOnCapture);
	SetInputMode(InputMode);
	FlushPressedKeys();
	if ((IsLocalController() || HasAuthority()) && !InPawn->FindComponentByClass<UPlaceholderHandsComponent>())
	{
		UPlaceholderHandsComponent* Hands=NewObject<UPlaceholderHandsComponent>(InPawn,TEXT("PlaceholderHands"));
		InPawn->AddInstanceComponent(Hands);
		Hands->RegisterComponent();
	}

	if (IsLocalController() && GetWorld() && SidescrollerCameraClass)
	{
		SidescrollerCamera = GetWorld()->SpawnActor<APWGSidescrollerCamera>(SidescrollerCameraClass);
		if (SidescrollerCamera)
		{
			SidescrollerCamera->SetOwner(this);
			// Frame from the character's feet instead of guessing an absolute camera height.
			// In orthographic projection OrthoWidth is the horizontal world span, therefore
			// the vertical span is OrthoWidth * ViewportHeight / ViewportWidth.
			int32 ViewportWidth = 0;
			int32 ViewportHeight = 0;
			GetViewportSize(ViewportWidth, ViewportHeight);
			const float ViewportAspect = ViewportWidth > 0 && ViewportHeight > 0
				? static_cast<float>(ViewportWidth) / static_cast<float>(ViewportHeight)
				: (16.0f / 9.0f);
			const float VerticalViewSpan = SidescrollerOrthoWidth / FMath::Max(ViewportAspect, 0.01f);

			// The camera is anchored to PWG surface height, so this places the surface
			// itself at the requested screen position and is independent of the Pawn pivot.
			const float ViewCentreAboveActor =
				(FMath::Clamp(CharacterFeetScreenY, 0.5f, 0.98f) - 0.5f) * VerticalViewSpan;

			SidescrollerCamera->CameraHeight = ViewCentreAboveActor;
			SidescrollerCamera->CameraDistance = SidescrollerCameraDistance;
			SidescrollerCamera->CameraAngle = 0.0f;
			SidescrollerCamera->FocusOffset.Z = ViewCentreAboveActor;
			SidescrollerCamera->GetCameraComponent()->SetProjectionMode(ECameraProjectionMode::Orthographic);
			SidescrollerCamera->GetCameraComponent()->SetOrthoWidth(SidescrollerOrthoWidth);
			SidescrollerCamera->GetCameraComponent()->bOverrideAspectRatioAxisConstraint = true;
			SidescrollerCamera->GetCameraComponent()->SetAspectRatioAxisConstraint(EAspectRatioAxisConstraint::AspectRatio_MaintainXFOV);
			if (const APWGGameMode* PWGGameMode = GetWorld()->GetAuthGameMode<APWGGameMode>())
			{
				SidescrollerCamera->SetGroundProvider(PWGGameMode->GetWorldGenerator());
			}
			SidescrollerCamera->SetFollowTarget(InPawn, true);
			SetViewTargetWithBlend(SidescrollerCamera, CameraBlendTime);
			if (UPWGSidescrollerInputComponent* Input = InPawn->FindComponentByClass<UPWGSidescrollerInputComponent>())
			{
				Input->SetScreenRightDirection(SidescrollerCamera->GetScreenRightMovementDirection());
			}
		}
	}
}

void APWGPlayerController::SetPawnLaneConstraintY(APawn* InPawn,const float WorldY) const
{
	if(!InPawn) return;
	if(UMoverComponent* Mover=InPawn->FindComponentByClass<UMoverComponent>())
	{
		FPlanarConstraint Constraint;
		Constraint.bConstrainToPlane=true;
		Constraint.PlaneConstraintNormal=FVector::YAxisVector;
		Constraint.PlaneConstraintOrigin=FVector(0.0f,WorldY,0.0f);
		Mover->SetPlanarConstraint(Constraint);
	}
}

void APWGPlayerController::StartGameAfterCharacterCreation()
{
	if (HasAuthority())
	{
		if (APWGGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<APWGGameMode>() : nullptr)
		{
			GameMode->StartGameAfterCharacterCreation();
		}
	}
	else
	{
		ServerStartGameAfterCharacterCreation();
	}
}

void APWGPlayerController::ServerStartGameAfterCharacterCreation_Implementation()
{
	StartGameAfterCharacterCreation();
}

void APWGPlayerController::ServerResetTrainingNPC_Implementation()
{
    if (auto* Mode=GetWorld()->GetAuthGameMode<APWGGameMode>()) Mode->ResetTrainingNPC();
}




void APWGPlayerController::SirrohASTControls(bool bEnabled)
{
    // This comparison switch is intentionally limited to the local single-player test.
    if (GetNetMode() != NM_Standalone) return;
    APawn* ControlledPawn = GetPawn();
    if (ControlledPawn)
    {
        if (const auto* Health = ControlledPawn->FindComponentByClass<UCombatVitalityComponent>(); Health && Health->UsesRagdollPose()) return;
        if (auto* Input = ControlledPawn->FindComponentByClass<UPWGSidescrollerInputComponent>())
        {
            if (Input->IsTraversalActive()) return;
            Input->CancelLaneMove();
        }
        if (auto* Combat = ControlledPawn->FindComponentByClass<UDirectionalCombatComponent>()) Combat->LeaveCombat();
        if (auto* Hands = ControlledPawn->FindComponentByClass<UPlaceholderHandsComponent>())
        { Hands->CancelCharge(); Hands->CancelBackInput(); }
    }
    bLaneSwitching = false;
    PendingCombatKeyTime = -1;
    LastDodgeLeft = LastDodgeRight = -1000;
    bUseASTControls = bEnabled;
    if (ControlledPawn) ConfigureSidescrollerPawn(ControlledPawn);
    UE_LOG(LogTemp, Display, TEXT("Integration controls: %s"), bEnabled ? TEXT("AST") : TEXT("Sirroh"));
    if (GEngine) GEngine->AddOnScreenDebugMessage(8108, 5.f, FColor::Cyan,
        bEnabled ? TEXT("Num8: AST controls") : TEXT("Num8: Sirroh controls"));
}

void APWGPlayerController::SirrohGroundReport()
{
    APawn* ControlledPawn = GetPawn();
    auto* Capsule = ControlledPawn ? ControlledPawn->FindComponentByClass<UCapsuleComponent>() : nullptr;
    auto* Mover = ControlledPawn ? ControlledPawn->FindComponentByClass<UMoverComponent>() : nullptr;
    if (!Capsule || !Mover) return;
    FHitResult Floor;
    const bool HasFloor = Mover->TryGetFloorCheckHitResult(Floor);
    auto* Ground = Floor.GetComponent();
    auto* Procedural = Cast<UProceduralMeshComponent>(Ground);
    const int32 Hulls = Procedural && Procedural->GetBodySetup() ? Procedural->GetBodySetup()->AggGeom.ConvexElems.Num() : 0;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(SirrohGroundReport), false, ControlledPawn);
    FHitResult Hit;
    const FVector Start = Capsule->GetComponentLocation();
    const double Before = FPlatformTime::Seconds();
    for (int32 Index = 0; Index < 8; ++Index)
        GetWorld()->SweepSingleByChannel(Hit, Start, Start - FVector(0,0,100), Capsule->GetComponentQuat(),
            Capsule->GetCollisionObjectType(), Capsule->GetCollisionShape(), Query,
            FCollisionResponseParams(Capsule->GetCollisionResponseToChannels()));
    const double AverageMs = (FPlatformTime::Seconds() - Before) * 1000.0 / 8.0;
    UE_LOG(LogTemp, Display, TEXT("PWG_GROUND mode=%s platform=%d frameMs=%.2f floor=%d component=%s mobility=%d hulls=%d normal=%s penetration=%d depth=%.3f sweepAvgMs=%.4f sweepHit=%s sweepPenetration=%d"),
        *Mover->GetMovementModeName().ToString(), IsValid(GroundComparisonPlatform), GetWorld()->GetDeltaSeconds()*1000.f,
        HasFloor, *GetPathNameSafe(Ground), Ground ? int32(Ground->Mobility) : -1, Hulls,
        *Floor.ImpactNormal.ToString(), Floor.bStartPenetrating, Floor.PenetrationDepth, AverageMs,
        *GetPathNameSafe(Hit.GetComponent()), Hit.bStartPenetrating);
    if (GEngine) GEngine->AddOnScreenDebugMessage(8109, 6.f, FColor::Yellow,
        FString::Printf(TEXT("Ground report saved: %s, sweep %.3f ms"), *Mover->GetMovementModeName().ToString(), AverageMs));
}

void APWGPlayerController::SirrohGroundCompare()
{
    if (GetNetMode() != NM_Standalone) return;
    APawn* ControlledPawn = GetPawn();
    auto* Capsule = ControlledPawn ? ControlledPawn->FindComponentByClass<UCapsuleComponent>() : nullptr;
    auto* Mover = ControlledPawn ? ControlledPawn->FindComponentByClass<UMoverComponent>() : nullptr;
    if (!Capsule || !Mover) return;
    if (Mover->GetMovementModeName() != FName(TEXT("Walking")))
    {
        if (GEngine) GEngine->AddOnScreenDebugMessage(8109, 5.f, FColor::Yellow, TEXT("Ground comparison: stand on the ground first (Walking, not flight)."));
        return;
    }
    if (auto* Input = ControlledPawn->FindComponentByClass<UPWGSidescrollerInputComponent>(); Input && Input->IsTraversalActive()) return;
    if (const auto* Health = ControlledPawn->FindComponentByClass<UCombatVitalityComponent>(); Health && Health->UsesRagdollPose()) return;
    SirrohGroundReport();
    FVector Destination;
    if (IsValid(GroundComparisonPlatform))
    {
        GroundComparisonPlatform->Destroy();
        GroundComparisonPlatform = nullptr;
        Destination = GroundComparisonReturnLocation;
    }
    else
    {
        GroundComparisonReturnLocation = ControlledPawn->GetActorLocation();
        const FVector Centre = Capsule->GetComponentLocation();
        const float TopZ = Centre.Z - Capsule->GetScaledCapsuleHalfHeight() + 500.f;
        FActorSpawnParameters Spawn;
        Spawn.ObjectFlags |= RF_Transient;
        GroundComparisonPlatform = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FVector(Centre.X,Centre.Y,TopZ-25), FRotator::ZeroRotator, Spawn);
        if (!GroundComparisonPlatform) return;
        auto* Box = NewObject<UBoxComponent>(GroundComparisonPlatform, TEXT("GroundComparisonBox"));
        GroundComparisonPlatform->SetRootComponent(Box);
        GroundComparisonPlatform->AddInstanceComponent(Box);
        Box->SetWorldLocation(FVector(Centre.X,Centre.Y,TopZ-25));
        Box->SetBoxExtent(FVector(1000,600,25));
        Box->SetCollisionProfileName(TEXT("BlockAll"));
        Box->SetCollisionResponseToChannel(ECC_GameTraceChannel1,ECR_Ignore);
        Box->SetMobility(EComponentMobility::Static);
        Box->SetHiddenInGame(false); // Show the box outline; no new mesh asset is needed.
        Box->RegisterComponent();
        Destination = GroundComparisonReturnLocation + FVector(0,0,503);
    }
    auto Stop = MakeShared<FApplyVelocityEffect>();
    Stop->VelocityToApply = FVector::ZeroVector;
    Stop->bAdditiveVelocity = false;
    Mover->QueueInstantMovementEffect(Stop);
    auto Teleport = MakeShared<FTeleportEffect>();
    Teleport->TargetLocation = Destination;
    Teleport->bUseActorRotation = true;
    Mover->QueueInstantMovementEffect(Teleport);
    UE_LOG(LogTemp, Display, TEXT("PWG_GROUND comparison switched to %s; collision and Walking mode retained"), IsValid(GroundComparisonPlatform) ? TEXT("single box") : TEXT("PWG"));
    if (GEngine) GEngine->AddOnScreenDebugMessage(8109, 8.f, FColor::Yellow,
        IsValid(GroundComparisonPlatform) ? TEXT("Single box ground. Walk a few steps; Num7 saves report, Num9 returns to PWG.") : TEXT("Returned to PWG ground. Num7 saves report."));
}


// Keep shared AST assets untouched: only this local PWG controller uses these transient mappings.
void APWGPlayerController::SyncASTInputContexts()
{
    if (!IsLocalController() || !GetLocalPlayer() || !GetPawn()) return;
    auto* Subsystem=GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
    if (!Subsystem) return;
    if (ASTOriginalContexts.IsEmpty())
    {
        const TCHAR* Paths[]={TEXT("/Game/Input/IMC_Sandbox.IMC_Sandbox"),TEXT("/AST_StateManager/Inputs/IMC_AST.IMC_AST")};
        for (const TCHAR* Path:Paths)
        {
            auto* Original=LoadObject<UInputMappingContext>(nullptr,Path);
            if (!Original) continue;
            auto* Context=DuplicateObject<UInputMappingContext>(Original,this);
            const auto Mappings=Context->GetMappings();
            for (const auto& Mapping:Mappings)
            {
                if (!Mapping.Action) continue;
                const FName Name=Mapping.Action->GetFName();
                if (Name==TEXT("IA_Interact"))
                {
                    Context->UnmapKey(Mapping.Action,Mapping.Key);
                    // PWG selects an item spatially; AST camera/hand-cone selection is 3D-only.
                    continue;
                }
                // Sirroh owns its camera, combat and item keys. AST interaction/inventory stay available.
                if (Name==TEXT("IA_Look") || Name==TEXT("IA_Look_Gamepad") ||
                    Name==TEXT("IA_Strafe") || Name==TEXT("IA_Takedown") ||
                    Name==TEXT("IA_NextPawn") || Name==TEXT("IA_NextVisualOverride") ||
                    Name==TEXT("IA_TeleportToTarget") || Name==TEXT("IA_TwinStick_AimDirection") ||
                    Name==TEXT("IA_MeleeAttack") || Name==TEXT("IA_Attack") ||
                    Name==TEXT("IA_TakeCover") || Name==TEXT("IA_ReloadWeapon") ||
                    Name==TEXT("IA_DropWeapon") || Name==TEXT("IA_UseMedkit"))
                    Context->UnmapKey(Mapping.Action,Mapping.Key);
            }
            PWGClimbContexts.Add(DuplicateObject<UInputMappingContext>(Context,this));
            for (const auto& Mapping:Mappings)
                if (Mapping.Action && (Mapping.Action->GetFName()==TEXT("IA_Aim") ||
                    (Mapping.Action->GetFName()==TEXT("IA_Move") && (Mapping.Key==EKeys::W || Mapping.Key==EKeys::S))))
                    Context->UnmapKey(Mapping.Action,Mapping.Key);
            ASTOriginalContexts.Add(Original); PWGInteractionContexts.Add(Context);
        }
    }
    const auto* SideInput=GetPawn()->FindComponentByClass<UPWGSidescrollerInputComponent>();
    const bool bClimbing=SideInput && SideInput->IsTraversalActive();
    for (int32 I=0;I<ASTOriginalContexts.Num();++I)
    {
        int32 Priority=0;
        UInputMappingContext* Desired=bUseASTControls?ASTOriginalContexts[I].Get():
            (bClimbing?PWGClimbContexts[I].Get():PWGInteractionContexts[I].Get());
        for (UInputMappingContext* Candidate:{ASTOriginalContexts[I].Get(),PWGClimbContexts[I].Get(),PWGInteractionContexts[I].Get()})
        {
            if (Candidate==Desired || !Subsystem->HasMappingContext(Candidate,Priority)) continue;
            Subsystem->RemoveMappingContext(Candidate);
            if (!Subsystem->HasMappingContext(Desired)) Subsystem->AddMappingContext(Desired,Priority);
        }
    }
}

namespace
{
    UObject* ASTResultActor(UActorComponent* Component)
    {
        auto* Result=FindFProperty<FStructProperty>(Component->GetClass(),TEXT("Parkour Result"));
        if (!Result) return nullptr;
        void* Data=Result->ContainerPtrToValuePtr<void>(Component);
        for (TFieldIterator<FObjectProperty> It(Result->Struct);It;++It)
            if (It->GetName().StartsWith(TEXT("LedgeActor_"))) return It->GetObjectPropertyValue_InContainer(Data);
        return nullptr;
    }
    bool ASTWithLedge(UActorComponent* Component,FName Name,AActor* Ledge,FVector Point,FVector Direction)
    {
        UFunction* Function=Component->FindFunction(Name);
        auto* Info=Function?FindFProperty<FStructProperty>(Function,TEXT("LedgeInfo")):nullptr;
        if (!Info) return false;
        FStructOnScope Args(Function); void* Data=Info->ContainerPtrToValuePtr<void>(Args.GetStructMemory());
        for (TFieldIterator<FProperty> It(Info->Struct);It;++It)
        {
            if (It->GetName().StartsWith(TEXT("LedgeActor_")))
            { if (auto* P=CastField<FObjectProperty>(*It)) P->SetObjectPropertyValue_InContainer(Data,Ledge); }
            else if (It->GetName().StartsWith(TEXT("LedgeLocation_"))) *It->ContainerPtrToValuePtr<FVector>(Data)=Point;
            else if (It->GetName().StartsWith(TEXT("LedgeDirection_"))) *It->ContainerPtrToValuePtr<FVector>(Data)=Direction;
        }
        UPOIMovingTraversalComponent::PrepareLedge(Component->GetOwner(),Ledge);
        Component->ProcessEvent(Function,Args.GetStructMemory()); UPOIPushTraversalComponent::ProtectTarget(Component->GetOwner()); return true;
    }
}

bool APWGPlayerController::TryProjectedASTLedge(bool Hop, bool ProbeOnly)
{
    APawn* Climber=GetPawn();
    if (!Climber || bUseASTControls || GetWorld()->GetTimeSeconds()-LastASTDrop<1 || UGameplayStatics::IsGamePaused(this)) return false;
    if (!Hop && !ProbeOnly && !IsInputKeyDown(EKeys::SpaceBar) && !IsInputKeyDown(EKeys::W) && !IsInputKeyDown(EKeys::Gamepad_FaceButton_Bottom)) return false;
    if (auto* Health=Climber->FindComponentByClass<UCombatVitalityComponent>(); Health && Health->UsesRagdollPose()) return false;
    auto* Input=Climber->FindComponentByClass<UPWGSidescrollerInputComponent>();
    if (!Input || Input->IsASTInteractionActive()) return false;
    UActorComponent* Parkour=nullptr;
    for (auto* C:Climber->GetComponents()) if (C && C->GetClass()->GetName().StartsWith(TEXT("BPC_ParkourSystem"))) { Parkour=C;break; }
    if (!Parkour) return false;
    if (auto* Busy=FindFProperty<FBoolProperty>(Parkour->GetClass(),TEXT("DoingTraversalAction")); Busy && Busy->GetPropertyValue_InContainer(Parkour)) return false;
    const FVector Position=Climber->GetActorLocation();
    const float ScreenSign=FMath::Sign(Input->GetScreenRightDirection().X);
    float Horizontal=((IsInputKeyDown(EKeys::D)?1.f:0.f)-(IsInputKeyDown(EKeys::A)?1.f:0.f)+GetInputAnalogKeyState(EKeys::Gamepad_LeftX))*ScreenSign;
    if (!Hop && FMath::Abs(Horizontal)<.1)
        if (auto* Mover=Climber->FindComponentByClass<UMoverComponent>(); Mover && FMath::Abs(Mover->GetVelocity().X)>50) Horizontal=FMath::Sign(Mover->GetVelocity().X);
    const float Vertical=(IsInputKeyDown(EKeys::W)?1.f:0.f)-(IsInputKeyDown(EKeys::S)?1.f:0.f);
    if (Hop && FMath::IsNearlyZero(Horizontal) && FMath::IsNearlyZero(Vertical)) return false;
    AActor* Current=Cast<AActor>(ASTResultActor(Parkour));
    FVector Origin=Position;
    if (Hop && Current) if (auto* S=Current->FindComponentByClass<USplineComponent>()) Origin=S->FindLocationClosestToWorldLocation(Position,ESplineCoordinateSpace::World);
    TArray<FOverlapResult> Hits;
    FCollisionObjectQueryParams Objects; Objects.AddObjectTypesToQuery(ECC_GameTraceChannel4);
    // Only acquisition is projected into screen X/Z. Original AST owns chooser, IK, warping and traversal.
    GetWorld()->OverlapMultiByObjectType(Hits,Origin+FVector(0,0,Hop?0:75),FQuat::Identity,Objects,
        FCollisionShape::MakeBox(FVector(Hop?250:145,2000,Hop?200:85)),FCollisionQueryParams(SCENE_QUERY_STAT(ASTProjectedLedge),false,Climber));
    AActor* Best=nullptr; FVector BestPoint=FVector::ZeroVector,BestDirection=FVector::ZeroVector; float Score=BIG_NUMBER; int32 BestPriority=MAX_int32;
    for (const auto& Hit:Hits)
    {
        AActor* Edge=Hit.GetActor(); if (!Edge || Edge==Current) continue;
        auto* Spline=Edge->FindComponentByClass<USplineComponent>(); if (!Spline || !Edge->FindFunction(TEXT("Get LedgeInfo"))) continue;
        FVector Probe=Origin; Probe.Y=Edge->GetActorLocation().Y;
        const float Length=Spline->GetSplineLength();
        const float Margin=FMath::Min(25.f,Length*.5f);
        const float Along=Spline->GetDistanceAlongSplineAtLocation(Probe,ESplineCoordinateSpace::World);
        // Landing exactly on a spline endpoint continually triggers AST's corner/end checks.
        FVector Point=Spline->GetLocationAtDistanceAlongSpline(FMath::Clamp(Along,Margin,Length-Margin),ESplineCoordinateSpace::World);
        FVector Inward=-Spline->FindRotationClosestToWorldLocation(Point,ESplineCoordinateSpace::World).RotateVector(FVector::YAxisVector);
        // Front grips work from underneath; side grips must face the intended approach.
        const bool Front=Inward.Y>.5;
        const bool Side=!Hop && FMath::Abs(Horizontal)>.1 && Inward.X*Horizontal>.5;
        if (!Front && !Side) continue;
        const FVector Delta=Point-Origin;
        if (!Hop && (Delta.Z<20 || Delta.Z>145 || FMath::Abs(Delta.X)>130)) continue;
        if (Hop && ((Vertical>0 && Delta.Z<30) || (Vertical<0 && Delta.Z>-30) ||
            (FMath::Abs(Horizontal)>.1 && Delta.X*Horizontal<30) || Delta.X*Delta.X+Delta.Z*Delta.Z<900)) continue;
        int32 Priority=0;
        if (!Hop && FMath::Abs(Horizontal)>.1)
        {
            const float Forward=Delta.X*FMath::Sign(Horizontal);
            if (Forward<-15) continue;
            Priority=Forward>15?0:1; // Directed target before a neutral overhead grip.
        }
        const float Candidate=Delta.X*Delta.X+Delta.Z*Delta.Z;
        if (Priority<BestPriority || (Priority==BestPriority && Candidate<Score))
        { BestPriority=Priority;Score=Candidate;Best=Edge;BestPoint=Point;BestDirection=Inward; }
    }
    if (ProbeOnly) return Best!=nullptr;
    if (Best)
        if (auto* Platform=Cast<APOIKitBlock>(Best->GetParentActor()); Platform && Platform->Settings.ASTHang==EPOIASTHang::Automatic)
        {
            // Let the native foot IK use a braced pose when a real wall supports the feet,
            // including a background wall behind a thin platform.
            FHitResult Support; FCollisionQueryParams Query(SCENE_QUERY_STAT(ASTFootSupport),false,Climber);
            const FVector Feet=BestPoint-FVector(0,0,110);
            const bool Braced=GetWorld()->SweepSingleByChannel(Support,Feet-BestDirection*50,Feet+BestDirection*130,
                FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(8),Query);
            if (auto* Style=FindFProperty<FByteProperty>(Best->GetClass(),TEXT("ClimbStyle"))) Style->SetPropertyValue_InContainer(Best,Braced?0:1);
        }
    if (!Best || !ASTWithLedge(Parkour,Hop?TEXT("Find Hop Montage"):TEXT("Find Air Montage"),Best,BestPoint,BestDirection)) return false;
    if (Hop)
    {
        bProjectedHopStarted=true; bProjectedHopSawBusy=false;
        if (auto* Busy=FindFProperty<FBoolProperty>(Parkour->GetClass(),TEXT("DoingTraversalAction"))) bProjectedHopSawBusy=Busy->GetPropertyValue_InContainer(Parkour);
        // Mirror the native Check Parkour path after dispatch, including its DoOnce reset.
        if (UFunction* Reset=Parkour->FindFunction(TEXT("Reset Hop"))) Parkour->ProcessEvent(Reset,nullptr);
    }
    return true;
}

void APWGPlayerController::TryDropThroughPlatform()
{
    APawn* Climber=GetPawn(); if (!Climber) return;
    auto* Input=Climber->FindComponentByClass<UPWGSidescrollerInputComponent>();
    auto* Mover=Climber->FindComponentByClass<UMoverComponent>();
    auto* Capsule=Climber->FindComponentByClass<UCapsuleComponent>();
    if (!Mover || !Input || !Capsule || Mover->GetMovementModeName()!=TEXT("Walking")) return;
    FHitResult Floor;
    if (!Mover->TryGetFloorCheckHitResult(Floor)) return;
    auto* Platform=Cast<APOIKitBlock>(Floor.GetActor());
    if (!Platform || !Platform->BeginDropThrough(Capsule)) return;
    Input->InvalidateCoyoteTime();
    auto Fall=MakeShared<FApplyVelocityEffect>(); Fall->VelocityToApply=Mover->GetVelocity();Fall->VelocityToApply.Z=-150;
    Fall->bAdditiveVelocity=false; Mover->QueueInstantMovementEffect(Fall);
    Mover->QueueNextMode(TEXT("Falling"));
    bManualDropNeedsGrab=true;LastASTDrop=GetWorld()->GetTimeSeconds();
}

void APWGPlayerController::FinishProjectedHop()
{
    if (!bProjectedHopStarted || !GetPawn()) return;
    for (auto* Component:GetPawn()->GetComponents())
    {
        if (!Component || !Component->GetClass()->GetName().StartsWith(TEXT("BPC_ParkourSystem"))) continue;
        auto* Busy=FindFProperty<FBoolProperty>(Component->GetClass(),TEXT("DoingTraversalAction"));
        if (Busy && Busy->GetPropertyValue_InContainer(Component)) {bProjectedHopSawBusy=true;return;}
        if (!bProjectedHopSawBusy) return; // A long reach may still be awaiting native AST confirmation.
        for (const FName Name:{FName(TEXT("Set Try Reach Ledge Network")),FName(TEXT("Reset Hop"))})
            if (auto* Function=Component->FindFunction(Name))
            { FStructOnScope Args(Function);Component->ProcessEvent(Function,Args.GetStructMemory()); }
        bProjectedHopStarted=false; bProjectedHopSawBusy=false;
        break;
    }
}

void APWGPlayerController::SirrohParkour(bool bDrop)
{
    APawn* ParkourPawn = GetPawn();
    if (!ParkourPawn || bUseASTControls || UGameplayStatics::IsGamePaused(this)) return;
    if (auto* Health = ParkourPawn->FindComponentByClass<UCombatVitalityComponent>(); Health && Health->UsesRagdollPose()) return;
    if (auto* Inventory = ParkourPawn->FindComponentByClass<USirrohASTInventoryComponent>(); Inventory && Inventory->IsBackpackActive()) return;
    auto* Input = ParkourPawn->FindComponentByClass<UPWGSidescrollerInputComponent>();
    if (!Input || Input->IsASTInteractionActive()) return;
    if (!bDrop && bManualDropNeedsGrab)
    {
        bManualDropNeedsGrab=false; LastASTDrop=-1000;
        if (TryProjectedASTLedge(false)) return;
    }
    // Under an overhead platform, allow the normal jump first. Do not let AST's
    // ground-only vertical-wall fallback consume it before the air grab can run.
    if (!bDrop && !Input->IsTraversalActive())
        if (auto* Mover=ParkourPawn->FindComponentByClass<UMoverComponent>(); Mover && Mover->GetMovementModeName()==TEXT("Walking") && TryProjectedASTLedge(false,true)) return;
    if (bDrop) LastASTDrop=GetWorld()->GetTimeSeconds();
    const bool UpOnly=IsInputKeyDown(EKeys::W) && !IsInputKeyDown(EKeys::A) && !IsInputKeyDown(EKeys::D);
    if (!bDrop && Input->IsTraversalActive() && !UpOnly && TryProjectedASTLedge(true)) return;
    if (!bDrop && !Input->IsTraversalActive())
        if (auto* Mover=ParkourPawn->FindComponentByClass<UMoverComponent>(); Mover && Mover->GetMovementModeName()==TEXT("Falling"))
        { TryProjectedASTLedge(false); return; }
    for (UActorComponent* Component : ParkourPawn->GetComponents())
    {
        if (!Component || !Component->GetClass()->GetName().StartsWith(TEXT("BPC_ParkourSystem"))) continue;
        if (!bDrop)
            if (auto* Busy=FindFProperty<FBoolProperty>(Component->GetClass(),TEXT("DoingTraversalAction")); Busy && Busy->GetPropertyValue_InContainer(Component)) return;
        if (UFunction* Action = Component->FindFunction(bDrop ? TEXT("Check Parkour Drop") : TEXT("Check Parkour")))
        {
            AActor* Edge=Cast<AActor>(ASTResultActor(Component));
            auto* Platform=Edge?Cast<APOIKitBlock>(Edge->GetParentActor()):nullptr;
            auto* Type=Edge?FindFProperty<FByteProperty>(Edge->GetClass(),TEXT("LedgeType")):nullptr;
            const uint8 SavedType=Type?Type->GetPropertyValue_InContainer(Edge):0;
            // AST's Ledge branch intentionally has no mantle exit. Enable the native Mantle
            // branch only for a physical POI platform when the player explicitly climbs up.
            const bool ClimbUp=!bDrop && Input->IsTraversalActive() && Platform && Type && SavedType==0 && Platform->Settings.Collision!=EPOIKitCollision::NoPhysicalCollision;
            if (ClimbUp) Type->SetPropertyValue_InContainer(Edge,2);
            bool RequestedClimb=false;
            if (ClimbUp && UpOnly)
                if (auto* Spline=Edge->FindComponentByClass<USplineComponent>())
                {
                    const FVector Point=Spline->FindLocationClosestToWorldLocation(ParkourPawn->GetActorLocation(),ESplineCoordinateSpace::World);
                    const FVector Inward=-Spline->FindRotationClosestToWorldLocation(Point,ESplineCoordinateSpace::World).RotateVector(FVector::YAxisVector);
                    // W+Space means climb onto the current support first, not select a distant hop.
                    RequestedClimb=ASTWithLedge(Component,TEXT("Select Hang Parkour Type"),Edge,Point,Inward);
                    if (auto* Reset=Component->FindFunction(TEXT("Reset Hop"))) Component->ProcessEvent(Reset,nullptr);
                }
            if (!RequestedClimb) { if(Input->IsTraversalActive())UPOIMovingTraversalComponent::PrepareLedge(ParkourPawn,Edge);FStructOnScope Parameters(Action);Component->ProcessEvent(Action,Parameters.GetStructMemory()); }
            UPOIPushTraversalComponent::ProtectTarget(ParkourPawn);
            if (ClimbUp) Type->SetPropertyValue_InContainer(Edge,SavedType);
            if (!bDrop && UpOnly)
                if (auto* Busy=FindFProperty<FBoolProperty>(Component->GetClass(),TEXT("DoingTraversalAction")); Busy && !Busy->GetPropertyValue_InContainer(Component)) TryProjectedASTLedge(true);
        }
        break;
    }
}

void APWGPlayerController::SirrohInteract()
{
    TryASTSideInteraction();
}

AActor* APWGPlayerController::FindASTSideInteraction()
{
    APawn* InteractionPawn=GetPawn();
    if (!InteractionPawn || bUseASTControls || UGameplayStatics::IsGamePaused(this)) return nullptr;
    auto* SideInput=InteractionPawn->FindComponentByClass<UPWGSidescrollerInputComponent>();
    if (SideInput && (SideInput->IsASTInteractionActive() || SideInput->IsTraversalActive())) return nullptr;
    UActorComponent* Interaction=nullptr;
    for (UActorComponent* Component:InteractionPawn->GetComponents())
        if (Component && Component->GetClass()->GetName().StartsWith(TEXT("BPC_Interaction"))) { Interaction=Component;break; }
    UFunction* Function=Interaction?Interaction->FindFunction(TEXT("Check Interaction")):nullptr;
    if (!Function) return nullptr;

    // Shared by the prompt (10 Hz) and key press, without scanning the world.
    // Include child items: DoorActor itself is not the AST interactable interface.
    FVector Reach=ASTInteractionReach.ComponentMax(FVector(1.f));
    if (const auto* Mode=GetWorld()->GetAuthGameMode<APWGGameMode>())
        if (const auto* Generator=Mode->GetWorldGenerator())
        {
            const float Spacing=FMath::Abs(Generator->GetLaneWorldY(static_cast<EPWGTerrainLane>(0))-Generator->GetLaneWorldY(EPWGTerrainLane::Middle));
            Reach.Y=FMath::Min(Reach.Y,Spacing+50.f);
        }
    TArray<FOverlapResult> Overlaps;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(PWGInteraction),false,InteractionPawn);
    GetWorld()->OverlapMultiByObjectType(Overlaps,InteractionPawn->GetActorLocation(),FQuat::Identity,
        FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllObjects),FCollisionShape::MakeBox(Reach),Query);
    TSet<AActor*> Candidates;
    for (const FOverlapResult& Overlap:Overlaps)
        if (AActor* Actor=Overlap.GetActor(); Actor && Actor!=InteractionPawn)
        {
            Candidates.Add(Actor);
            TArray<AActor*> ChildItems; Actor->GetAllChildActors(ChildItems,true);
            for (AActor* Child:ChildItems) Candidates.Add(Child);
        }
    // Preserve items already collected by AST's overlap system, including query-only triggers.
    if (const auto* Array=FindFProperty<FArrayProperty>(Interaction->GetClass(),TEXT("RangedActors")))
        if (const auto* Inner=CastField<FObjectPropertyBase>(Array->Inner))
        {
            FScriptArrayHelper Values(Array,Array->ContainerPtrToValuePtr<void>(Interaction));
            for (int32 I=0;I<Values.Num();++I)
                if (auto* Actor=Cast<AActor>(Inner->GetObjectPropertyValue(Values.GetRawPtr(I)))) Candidates.Add(Actor);
        }
    AActor* Best=nullptr; double BestDistance=DBL_MAX;
    const FVector Origin=InteractionPawn->GetActorLocation();
    for (AActor* Candidate:Candidates)
    {
        if (!IsValid(Candidate) || !Candidate->ActorHasTag(TEXT("Item"))) continue;
        if (const AActor* Parent=Candidate->GetParentActor(); Parent && Parent->FindFunction(TEXT("Get PullPush IK Location")))
        {
            const FVector Out=Candidate->GetActorForwardVector();
            const FVector TowardPlayer=(Origin-Candidate->GetActorLocation()).GetSafeNormal2D();
            if (FMath::Abs(Out.X)<.7f || FVector::DotProduct(Out,TowardPlayer)<.25f) continue;
        }
        const FVector Delta=Candidate->GetActorLocation()-Origin;
        // Include the adjacent lane, while rejecting distant floors and objects.
        if (FMath::Abs(Delta.Y)>Reach.Y || FMath::Abs(Delta.Z)>Reach.Z || FMath::Abs(Delta.X)>Reach.X) continue;
        FHitResult Hit;
        FVector Aim=Candidate->GetActorLocation();
        Aim.Z=FMath::Clamp(Origin.Z,Aim.Z+30.f,Aim.Z+120.f);
        const bool Blocked=GetWorld()->LineTraceSingleByChannel(Hit,Origin,Aim,ECC_Visibility,Query);
        if (Blocked && Hit.GetActor()!=Candidate && Hit.GetActor()!=Candidate->GetParentActor()) continue;
        // Visibility alone can fit through a gap the character cannot. Check the
        // body corridor too; the target's own interaction geometry is allowed.
        if (const auto* Capsule=Cast<UCapsuleComponent>(InteractionPawn->GetRootComponent()))
        {
            FCollisionQueryParams ApproachQuery=Query;
            ApproachQuery.AddIgnoredActor(Candidate);
            if (AActor* Parent=Candidate->GetParentActor())
            {
                ApproachQuery.AddIgnoredActor(Parent);
                TArray<AActor*> TargetParts; Parent->GetAllChildActors(TargetParts,true);
                for(AActor* Part:TargetParts) ApproachQuery.AddIgnoredActor(Part);
            }
            FVector End=Candidate->GetActorLocation();End.Z=Origin.Z;
            const auto Shape=FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),FMath::Max(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight()-2.f));
            if(GetWorld()->SweepSingleByChannel(Hit,Origin,End,FQuat::Identity,Capsule->GetCollisionObjectType(),Shape,ApproachQuery)) continue;
        }
        const double Distance=Delta.X*Delta.X+Delta.Z*Delta.Z;
        if (Distance<BestDistance || (Distance==BestDistance && Best && Candidate->GetPathName()<Best->GetPathName()))
        {BestDistance=Distance;Best=Candidate;}
    }
    // One E selector: a closer native item wins over a farther AST fixture.
    if(auto* Hands=InteractionPawn->FindComponentByClass<UPlaceholderHandsComponent>())
        if(auto* Native=Hands->GetPickupCandidate();Native && (!Best || FVector2D(Origin.X-Native->GetActorLocation().X,Origin.Z-Native->GetActorLocation().Z).SizeSquared()<BestDistance))return nullptr;
    return Best;
}

void APWGPlayerController::SetASTInteractionPrompt(AActor* Target)
{
    auto SetIcon=[](AActor* Actor,bool Visible)
    {
        if (!IsValid(Actor)) return;
        if (UFunction* Function=Actor->FindFunction(TEXT("Set Icon Visibility")))
        {
            FStructOnScope Args(Function);
            if (auto* Value=FindFProperty<FBoolProperty>(Function,TEXT("Visibility")))
            {
                Value->SetPropertyValue_InContainer(Args.GetStructMemory(),Visible);
                Actor->ProcessEvent(Function,Args.GetStructMemory());
            }
        }
    };
    if (ASTPromptActor.Get()!=Target) SetIcon(ASTPromptActor.Get(),false);
    ASTPromptActor=Target;
    SetIcon(Target,true);
    if (APawn* PromptPawn=GetPawn())
        for (UActorComponent* Component:PromptPawn->GetComponents())
            if (Component && Component->ComponentHasTag(TEXT("StateManager")))
            {
                if (auto* Current=FindFProperty<FObjectPropertyBase>(Component->GetClass(),TEXT("ClosestActor")))
                    if (auto* Previous=Cast<AActor>(Current->GetObjectPropertyValue_InContainer(Component));Previous!=Target)
                        SetIcon(Previous,false);
                if (UFunction* Function=Component->FindFunction(TEXT("Set Closest Actor")))
                {
                    FStructOnScope Args(Function);
                    if (auto* Value=FindFProperty<FObjectPropertyBase>(Function,TEXT("NewClosestActor")))
                    {
                        Value->SetObjectPropertyValue_InContainer(Args.GetStructMemory(),Target);
                        Component->ProcessEvent(Function,Args.GetStructMemory());
                    }
                }
            }
}

void APWGPlayerController::UpdateASTInteractionPrompt()
{
    if (!IsLocalController()) return;
    APawn* PromptPawn=GetPawn();
    // The loot notify resolves this target throughout the AST animation.
    if (PromptPawn) if (auto* Hands=PromptPawn->FindComponentByClass<UPlaceholderHandsComponent>();Hands && Hands->IsASTPickupActive()) return;
    UActorComponent* Interaction=nullptr;
    if (PromptPawn) for (UActorComponent* Component:PromptPawn->GetComponents())
        if (Component && Component->GetClass()->GetName().StartsWith(TEXT("BPC_Interaction"))) { Interaction=Component;break; }
    if (bUseASTControls)
    {
        if (bOwnsASTPrompt)
        {
            SetASTInteractionPrompt(nullptr);
            if (Interaction) if (UFunction* Start=Interaction->FindFunction(TEXT("Start Update Closest Actor")))
                Interaction->ProcessEvent(Start,nullptr);
            bOwnsASTPrompt=false;
        }
        return;
    }
    // AST's hand-range timer otherwise hides an icon valid for the side-scroller reach.
    if (Interaction) if (UFunction* Stop=Interaction->FindFunction(TEXT("Stop Update Closest Actor")))
        Interaction->ProcessEvent(Stop,nullptr);
    bOwnsASTPrompt=true;
    if (PromptPawn) if (const auto* Health=PromptPawn->FindComponentByClass<UCombatVitalityComponent>(); Health && Health->IsIncapacitated())
    { SetASTInteractionPrompt(nullptr); return; }
    SetASTInteractionPrompt(FindASTSideInteraction());
    if(auto* Hands=PromptPawn->FindComponentByClass<UPlaceholderHandsComponent>())
        if(auto* Native=Hands->GetPickupCandidate())Native->SetPickupPromptVisible(!ASTPromptActor.IsValid());
}

bool APWGPlayerController::TryASTSideInteraction()
{
    AActor* TargetActor=FindASTSideInteraction();
    if (!TargetActor) return false;
    for (UActorComponent* Component:GetPawn()->GetComponents())
        if (Component && Component->GetClass()->GetName().StartsWith(TEXT("BPC_Interaction")))
            if (UFunction* Function=Component->FindFunction(TEXT("Check Interaction")))
            {
                FStructOnScope Args(Function);
                if (auto* Target=FindFProperty<FObjectPropertyBase>(Function,TEXT("InteractionActor")))
                {
                    Target->SetObjectPropertyValue_InContainer(Args.GetStructMemory(),TargetActor);
                    Component->ProcessEvent(Function,Args.GetStructMemory());
                    SetASTInteractionPrompt(nullptr);
                    return true;
                }
            }
    return false;
}

void APWGPlayerController::HandleASTCombatInput(bool bAllowStart)
{
    APawn* CombatPawn=GetPawn();
    if (!CombatPawn || bLaneSwitching || bUseASTControls) return;
    if (auto* Input=CombatPawn->FindComponentByClass<UPWGSidescrollerInputComponent>();Input && (Input->IsTraversalActive() || Input->IsASTInteractionActive())) return;
    UActorComponent* Inventory=nullptr;UActorComponent* Melee=nullptr;UActorComponent* State=nullptr;
    for (UActorComponent* Component:CombatPawn->GetComponents())
    {
        if (!Component) continue;
        if (Component->ComponentHasTag(TEXT("InventorySystem"))) Inventory=Component;
        if (Component->ComponentHasTag(TEXT("StateManager"))) State=Component;
        if (Component->GetClass()->GetName().StartsWith(TEXT("BPC_MeleeCombat"))) Melee=Component;
    }
    bool bASTWeapon=false;
    if (Inventory) if (auto* Equipped=FindFProperty<FStructProperty>(Inventory->GetClass(),TEXT("EquippedWeapon")))
        for (TFieldIterator<FObjectPropertyBase> Field(Equipped->Struct);Field;++Field)
            if (Field->GetName().StartsWith(TEXT("Weapon_")))
                bASTWeapon=IsValid(Field->GetObjectPropertyValue_InContainer(Equipped->ContainerPtrToValuePtr<void>(Inventory)));
    if (bASTWeapon)
    {
        // Keep AST's weapon-specific dispatch (firearm/melee), not the unarmed entry.
        const bool Press=bAllowStart && WasInputKeyJustPressed(EKeys::LeftMouseButton);
        if (!bAllowStart || Press || WasInputKeyJustReleased(EKeys::LeftMouseButton))
            if (UFunction* Function=Inventory->FindFunction(TEXT("Check Attack")))
            {
                FStructOnScope Args(Function);
                if (auto* Wants=FindFProperty<FBoolProperty>(Function,TEXT("WantsAttack")))
                { Wants->SetPropertyValue_InContainer(Args.GetStructMemory(),Press);Inventory->ProcessEvent(Function,Args.GetStructMemory()); }
            }
        if(!bAllowStart || WasInputKeyJustPressed(EKeys::RightMouseButton) || WasInputKeyJustReleased(EKeys::RightMouseButton))
            if(UFunction* Aim=State?State->FindFunction(TEXT("Set Input Aim")):nullptr)
            {
                FStructOnScope Args(Aim);
                if(auto* Wants=FindFProperty<FBoolProperty>(Aim,TEXT("InputAim")))
                {Wants->SetPropertyValue_InContainer(Args.GetStructMemory(),bAllowStart && IsInputKeyDown(EKeys::RightMouseButton));State->ProcessEvent(Aim,Args.GetStructMemory());}
            }
        if(bAllowStart && WasInputKeyJustPressed(EKeys::R))
            if(UFunction* Reload=Inventory->FindFunction(TEXT("Reload Weapon Network")))Inventory->ProcessEvent(Reload,nullptr);
        return;
    }
    if (!bAllowStart || (!WasInputKeyJustPressed(EKeys::LeftMouseButton) && !WasInputKeyJustPressed(EKeys::RightMouseButton))) return;
    if (!Melee || !State) return;
    // Preserve the current overlay: AST excludes Pullpush (enum6), it does not require it.
    // Both empty-hand buttons use the same existing melee eligibility/montage path.
    if (UFunction* Attack=Melee->FindFunction(TEXT("CheckingMeleeAttack"))) Melee->ProcessEvent(Attack,nullptr);
}
