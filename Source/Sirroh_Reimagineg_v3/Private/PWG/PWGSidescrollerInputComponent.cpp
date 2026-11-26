#include "PWG/PWGSidescrollerInputComponent.h"
#include "Character/SirrohASTCombatBridgeComponent.h"
#include "PWG/PWGPlayerController.h"
#include "PWG/PWGPhysicsContactComponent.h"
#include "POI/POIPhysicalPushComponent.h"
#include "Debug/SirrohDebugSubsystem.h"
#include "Engine/GameInstance.h"
#include "DefaultMovementSet/InstantMovementEffects/BasicInstantMovementEffects.h"


#include "GameFramework/Pawn.h"
#include "UObject/UnrealType.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "MoverDataModelTypes.h"
#include "MoverComponent.h"
#include "MoveLibrary/ConstrainedMoveUtils.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "Combat/DirectionalCombatComponent.h"
#include "Combat/CombatVitalityComponent.h"
#include "Items/SirrohASTInventoryComponent.h"
#include "POI/POICorridorTools.h"
#include "POI/POIPushTraversalComponent.h"


UPWGSidescrollerInputComponent::UPWGSidescrollerInputComponent()
{
        PrimaryComponentTick.bCanEverTick = false;
        bWantsInitializeComponent = true;
        SetIsReplicatedByDefault(false);
}

void UPWGSidescrollerInputComponent::InitializeComponent()
{
    Super::InitializeComponent();
    if(GetOwner()->HasAuthority() && !GetOwner()->FindComponentByClass<USirrohASTCombatBridgeComponent>())
    {
        auto* Bridge=NewObject<USirrohASTCombatBridgeComponent>(GetOwner(),TEXT("ASTPhysicalMelee"));
        GetOwner()->AddInstanceComponent(Bridge);Bridge->RegisterComponent();
    }
    if (!GetOwner()->FindComponentByClass<UPWGPhysicsContactComponent>())
    {
        auto* Contact=NewObject<UPWGPhysicsContactComponent>(GetOwner(),TEXT("PWGPhysicsContact"));
        GetOwner()->AddInstanceComponent(Contact);Contact->RegisterComponent();
    }
    // Blueprint SCS components do not exist yet when a deferred pawn is allocated.
    // Bind after construction, before Mover caches its producers in BeginPlay.
    if (auto* Mover = GetOwner()->FindComponentByClass<UMoverComponent>())
    {
        Mover->InputProducer = this;
        Mover->bGatherInputFromAllInputProducerComponents = false;
    }
}


void UPWGSidescrollerInputComponent::SetScreenRightDirection(const FVector& Direction)
{
        ScreenRightDirection = FVector(Direction.X, 0.0f, 0.0f).GetSafeNormal();
        if (ScreenRightDirection.IsNearlyZero()) ScreenRightDirection = -FVector::XAxisVector;
}


void UPWGSidescrollerInputComponent::ProduceInput_Implementation(
        const int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult)
{
        APawn* Pawn = Cast<APawn>(GetOwner());
        APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
        // Detach through AST before the native producer can advance jump/traversal motion.
        if (PC && (PC->WasInputKeyJustPressed(EKeys::SpaceBar) || PC->WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Bottom)))
            UPOICorridorTools::ReleaseHeldPushable(Pawn);
        // Native AST interprets ledge inputs relative to control yaw. Keep that frame
        // aligned with the fixed side camera, not the last ground-facing direction.
        if (PC && IsTraversalActive()) PC->SetControlRotation(FRotator(0,ScreenRightDirection.X<0?90.f:-90.f,0));
        // Preserve GASP gait/traversal/ragdoll input structs before replacing only side-scroll movement.
        if (Pawn && Pawn->GetClass()->ImplementsInterface(UMoverInputProducerInterface::StaticClass()))
            IMoverInputProducerInterface::Execute_ProduceInput(Pawn, SimTimeMs, InputCmdResult);
        UPOIPushTraversalComponent::ProtectTarget(Pawn);
        FCharacterDefaultInputs& Inputs = InputCmdResult.InputCollection.FindOrAddMutableDataByType<FCharacterDefaultInputs>();


        const auto* Health = Pawn ? Pawn->FindComponentByClass<UCombatVitalityComponent>() : nullptr;
        auto* RagdollMover = Pawn ? Pawn->FindComponentByClass<UMoverComponent>() : nullptr;
        const bool bRagdoll = (Health && Health->UsesRagdollPose()) ||
            (RagdollMover && (RagdollMover->GetMovementModeName() == TEXT("Ragdoll") ||
                             RagdollMover->GetNextMovementModeName() == TEXT("Ragdoll")));
        if (RagdollMover)
        {
            if (bRagdoll) CancelLaneMove();
            auto Constraint = RagdollMover->GetPlanarConstraint();
            if (bRagdoll && Constraint.bConstrainToPlane)
            {
                // Ragdoll tracks a simulated pelvis in three dimensions. Pinning its capsule
                // to a different Y feeds displacement back into the physics controls.
                Constraint.bConstrainToPlane = false;
                RagdollMover->SetPlanarConstraint(Constraint);
                bRestorePlaneAfterRagdoll = true;
            }
            else if (!bRagdoll && bRestorePlaneAfterRagdoll && !IsTraversalActive() && !IsASTInteractionActive())
            {
                Constraint.bConstrainToPlane = true;
                Constraint.PlaneConstraintOrigin = Pawn->GetActorLocation();
                RagdollMover->SetPlanarConstraint(Constraint);
                bRestorePlaneAfterRagdoll = false;
            }
        }
        if (bRagdoll)
        {
            Inputs.SetMoveInput(EMoveInputType::DirectionalIntent,FVector::ZeroVector);
            Inputs.OrientationIntent=FVector::ZeroVector;
            Inputs.bIsJumpPressed=Inputs.bIsJumpJustPressed=false;
            bWasJumpPressed=false;
            CancelLaneMove();
            return;
        }
        if (PC && PC->GetGameInstance() && PC->GetGameInstance()->GetSubsystem<USirrohDebugSubsystem>()->IsEditingText())
        {
            Inputs.SetMoveInput(EMoveInputType::DirectionalIntent,FVector::ZeroVector);
            Inputs.OrientationIntent=FVector::ZeroVector;
            Inputs.bIsJumpPressed=Inputs.bIsJumpJustPressed=false;
            bWasJumpPressed=false; return;
        }
        if (const auto* PWGController = Cast<APWGPlayerController>(PC); PWGController && PWGController->bUseASTControls)
        {
            bWasJumpPressed = false;
            bRestorePlaneAfterTraversal = false;
            return; // Preserve the complete AST input collection, including its custom data.
        }
        if (const auto* Inventory=Pawn?Pawn->FindComponentByClass<USirrohASTInventoryComponent>():nullptr;Inventory && Inventory->IsBackpackActive())
        {
            Inputs.SetMoveInput(EMoveInputType::DirectionalIntent,FVector::ZeroVector);
            Inputs.OrientationIntent=FVector::ZeroVector;
            Inputs.bIsJumpPressed=Inputs.bIsJumpJustPressed=false;
            CancelLaneMove();bWasJumpPressed=false;return;
        }
        if (AActor* Object=UPOICorridorTools::GetHeldPushable(Pawn))
        {
            if (HeldPushable.Get()!=Object)
            {
                HeldPushable=Object;
                PushLaneY=Pawn->GetActorLocation().Y;
                PushFacing=FVector(FMath::Sign(Object->GetActorLocation().X-Pawn->GetActorLocation().X),0,0);
                if (PushFacing.IsNearlyZero()) PushFacing=Pawn->GetActorForwardVector();
                UPOICorridorTools::ConfigurePushPlane(Object);
            }
            const float Axis=PC?FMath::Clamp((PC->IsInputKeyDown(EKeys::D)?1.f:0.f)-(PC->IsInputKeyDown(EKeys::A)?1.f:0.f)+PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftX),-1.f,1.f):0.f;
            auto* Drive=Object->FindComponentByClass<UPOIPhysicalPushComponent>();
            const float WorldAxis=ScreenRightDirection.X*Axis;
            Inputs.SetMoveInput(EMoveInputType::DirectionalIntent,FVector(Drive?Drive->LimitInput(WorldAxis):WorldAxis,0,0));
            Inputs.OrientationIntent=PushFacing;
            Inputs.bIsJumpPressed=Inputs.bIsJumpJustPressed=false;
            if (auto* Mover=Pawn->FindComponentByClass<UMoverComponent>())
            {
                auto Constraint=Mover->GetPlanarConstraint();
                Constraint.bConstrainToPlane=true; Constraint.PlaneConstraintNormal=FVector::YAxisVector;
                Constraint.PlaneConstraintOrigin=FVector(0,PushLaneY,0); Mover->SetPlanarConstraint(Constraint);
            }
            bLaneMove=false; bRestorePlaneAfterTraversal=true; bWasJumpPressed=false;
            return;
        }
        HeldPushable.Reset(); PushFacing=FVector::ZeroVector;
        if (IsASTInteractionActive())
        {
            if (Pawn) if (auto* Mover=Pawn->FindComponentByClass<UMoverComponent>())
            {
                auto Constraint=Mover->GetPlanarConstraint(); Constraint.bConstrainToPlane=false;
                Mover->SetPlanarConstraint(Constraint);
            }
            bRestorePlaneAfterTraversal=true; bLaneMove=false; bWasJumpPressed=false;
            return; // Preserve AST's input and root-motion/warping state.
        }
        if (IsTraversalActive())
        {
            InvalidateCoyoteTime(); bWasGrounded=false;
            // Motion warping owns movement until the traversal montage has finished.
            if (!bRestorePlaneAfterTraversal && Pawn)
                if (auto* Mover=Pawn->FindComponentByClass<UMoverComponent>())
                {
                    auto Constraint=Mover->GetPlanarConstraint(); Constraint.bConstrainToPlane=false;
                    Mover->SetPlanarConstraint(Constraint);
                }
            bRestorePlaneAfterTraversal=true; bLaneMove=false;
            // The pawn produced its AST input above, including ledge movement and jump.
            bWasJumpPressed=false;
            return;
        }
        if (bRestorePlaneAfterTraversal && Pawn)
        {
            bRestorePlaneAfterTraversal=false;
            if (auto* Mover=Pawn->FindComponentByClass<UMoverComponent>())
            {
                auto Constraint=Mover->GetPlanarConstraint(); Constraint.bConstrainToPlane=true;
                Constraint.PlaneConstraintNormal=FVector::YAxisVector;
                Constraint.PlaneConstraintOrigin=FVector(0,Pawn->GetActorLocation().Y,0);
                Mover->SetPlanarConstraint(Constraint);
            }
        }
        float HorizontalInput = 0.0f;
        bool bJumpPressed = false;
        if (PC && PC->IsLocalController())
        {
                HorizontalInput += PC->IsInputKeyDown(EKeys::D) ? 1.0f : 0.0f;
                HorizontalInput -= PC->IsInputKeyDown(EKeys::A) ? 1.0f : 0.0f;
                HorizontalInput += PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftX);
                bJumpPressed = bEnableJump && (PC->IsInputKeyDown(EKeys::SpaceBar)
                        || PC->IsInputKeyDown(EKeys::Gamepad_FaceButton_Bottom));
        }


    HorizontalInput = FMath::Clamp(HorizontalInput, -1.0f, 1.0f);
    if (const auto* Combat=Pawn?Pawn->FindComponentByClass<UDirectionalCombatComponent>():nullptr)
        if (Combat->IsMovementLocked()) { HorizontalInput=0; bJumpPressed=false; }


        const FVector WorldMoveIntent = ScreenRightDirection * HorizontalInput;
        Inputs.SetMoveInput(EMoveInputType::DirectionalIntent, WorldMoveIntent);
        Inputs.OrientationIntent = WorldMoveIntent.GetSafeNormal();
        if (const auto* Combat=Pawn?Pawn->FindComponentByClass<UDirectionalCombatComponent>():nullptr)
                if (Combat->IsCombatActive())
                {
                    if (Combat->UsesMotionCoreV3())
                        Inputs.SetMoveInput(EMoveInputType::Velocity,WorldMoveIntent*Combat->GetGuardWalkSpeed());
                    if (Combat->UsesMotionCoreV3())
                    {
                        if (StableCombatFacing.IsNearlyZero()) StableCombatFacing=Pawn->GetActorForwardVector();
                        const bool Left=PC && PC->IsInputKeyDown(EKeys::A), Right=PC && PC->IsInputKeyDown(EKeys::D);
                        if (Left && !bWasLeftDown)
                        { StableCombatFacing=-ScreenRightDirection; }
                        if (Right && !bWasRightDown)
                        { StableCombatFacing=ScreenRightDirection; }
                        bWasLeftDown=Left; bWasRightDown=Right;
                        if (!Combat->GetPendingAttackFacing().IsNearlyZero()) StableCombatFacing=Combat->GetPendingAttackFacing();
                        Inputs.OrientationIntent=StableCombatFacing;
                    }
                    else
                    {
                        const float AimX=Combat->GetAimDirection().X;
                        Inputs.OrientationIntent=FMath::Abs(AimX)>0.05f?FVector(FMath::Sign(AimX),0,0):Pawn->GetActorForwardVector();
                    }
                }
        if (const auto* ActiveCombat=Pawn?Pawn->FindComponentByClass<UDirectionalCombatComponent>():nullptr;
            !ActiveCombat || !ActiveCombat->IsCombatActive())
        {
            StableCombatFacing=Pawn?Pawn->GetActorForwardVector():FVector::ZeroVector;
            bWasLeftDown=PC && PC->IsInputKeyDown(EKeys::A); bWasRightDown=PC && PC->IsInputKeyDown(EKeys::D);
            LastLeftTap=LastRightTap=-1000;
        }
        if (bLaneMove && Pawn)
        {
            const float Error=LaneTargetY-Pawn->GetActorLocation().Y;
            if (FMath::Abs(Error)<2 || Error*(LaneTargetY-LaneStartY)<=0) CancelLaneMove();
            else
            {
                auto* Mover=Pawn->FindComponentByClass<UMoverComponent>();
                const auto* Movement=Mover?Mover->FindSharedSettings<UCommonLegacyMovementSettings>():nullptr;
                const float Speed=Movement?Movement->MaxSpeed:600;
                const float Step=FMath::Max(0.008f,GetWorld()->GetDeltaSeconds());
                  const float Lateral=FMath::Sign(Error)*FMath::Min3(FMath::Abs(Error)*6.f,150.f,FMath::Abs(Error)/Step);
                  Inputs.SetMoveInput(EMoveInputType::Velocity,WorldMoveIntent*Speed+FVector(0,Lateral,0));
                  if (Mover)
                  {
                      auto Effect=MakeShared<FApplyVelocityEffect>();
                      Effect->VelocityToApply=Mover->GetVelocity(); Effect->VelocityToApply.Y=Lateral;
                      Effect->bAdditiveVelocity=false; Mover->QueueInstantMovementEffect(Effect);
                  }
            }
        }
        // GASP also reads the controller directly when calculating sprint/aim facing.
        // Neither mouse aim nor an interaction's exit yaw may leave the 2D pawn
        // facing the camera. Preserve a signed world-X heading even while idle.
        if (FMath::Abs(Inputs.OrientationIntent.X)>KINDA_SMALL_NUMBER)
            SideFacingSign=FMath::Sign(Inputs.OrientationIntent.X);
        Inputs.OrientationIntent=FVector(SideFacingSign,0,0);
        StableCombatFacing=Inputs.OrientationIntent;
        Inputs.ControlRotation=FRotator(0,SideFacingSign<0?180.f:0.f,0);
        if (PC && PC->IsLocalController()) PC->SetControlRotation(Inputs.ControlRotation);
        Inputs.bUsingMovementBase = false;
        Inputs.MovementBase = nullptr;
        Inputs.MovementBaseBoneName = NAME_None;
        if (RagdollMover)
        {
            const bool Grounded=RagdollMover->GetMovementModeName()==TEXT("Walking");
            if (Grounded && !bWasGrounded) bCoyoteConsumed=false;
            if (Grounded && !bCoyoteConsumed) LastGroundedMs=SimTimeMs;
            if (bJumpPressed && !bWasJumpPressed)
            {
                if (!Grounded && !bCoyoteConsumed && SimTimeMs>=LastGroundedMs && SimTimeMs-LastGroundedMs<=CoyoteTime*1000.f && RagdollMover->GetMovementModeName()==TEXT("Falling"))
                {
                    auto Jump=MakeShared<FJumpImpulseEffect>();
                    const auto* Movement=RagdollMover->FindSharedSettings<UCommonLegacyMovementSettings>();
                    Jump->UpwardsSpeed=Movement?Movement->JumpUpwardsSpeed:500.f;
                    RagdollMover->QueueInstantMovementEffect(Jump);
                }
                bCoyoteConsumed=true;
            }
            bWasGrounded=Grounded;
        }
        Inputs.bIsJumpPressed = bJumpPressed;
        Inputs.bIsJumpJustPressed = bJumpPressed && !bWasJumpPressed;
        bWasJumpPressed = bJumpPressed;
}

void UPWGSidescrollerInputComponent::BeginLaneMove(float TargetY)
{
    if (IsTraversalActive()) return;
    LaneStartY=GetOwner()->GetActorLocation().Y; LaneTargetY=TargetY; bLaneMove=true;
    if (auto* Mover=GetOwner()->FindComponentByClass<UMoverComponent>())
    {
        FPlanarConstraint Constraint=Mover->GetPlanarConstraint(); Constraint.bConstrainToPlane=false;
        Mover->SetPlanarConstraint(Constraint);
    }
}
void UPWGSidescrollerInputComponent::CancelLaneMove()
{
    if (!bLaneMove) return;
    if (IsTraversalActive() || IsASTInteractionActive()) { bLaneMove=false; bRestorePlaneAfterTraversal=true; return; }
    bLaneMove=false;
    if (auto* Mover=GetOwner()->FindComponentByClass<UMoverComponent>())
    {
        FVector Settled=GetOwner()->GetActorLocation();
        const auto* Health=GetOwner()->FindComponentByClass<UCombatVitalityComponent>();
        if (!Health || !Health->UsesRagdollPose())
        {
            // Never pin a completed/cancelled switch outside a lane when a swept recovery is clear.
            if (const auto* Body=Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent()))
            {
                const bool TargetNear=FMath::Abs(Settled.Y-LaneTargetY)<=FMath::Abs(Settled.Y-LaneStartY);
                for (float Y:{TargetNear?LaneTargetY:LaneStartY,TargetNear?LaneStartY:LaneTargetY})
                {
                    FVector Candidate=Settled; Candidate.Y=Y;
                    FHitResult Hit; FCollisionQueryParams Query(SCENE_QUERY_STAT(PWGLaneFinish),false,GetOwner());
                    if (!GetWorld()->SweepSingleByChannel(Hit,Settled,Candidate,Body->GetComponentQuat(),ECC_Pawn,Body->GetCollisionShape(),Query))
                    {
                        auto Snap=MakeShared<FTeleportEffect>(); Snap->TargetLocation=Candidate; Snap->bUseActorRotation=true;
                        Mover->QueueInstantMovementEffect(Snap); Settled=Candidate; break;
                    }
                }
            }
        }
        FPlanarConstraint Constraint; Constraint.bConstrainToPlane=true;
        Constraint.PlaneConstraintNormal=FVector::YAxisVector;
        Constraint.PlaneConstraintOrigin=FVector(0,Settled.Y,0);
        auto Effect=MakeShared<FApplyVelocityEffect>();
        Effect->VelocityToApply=Mover->GetVelocity(); Effect->VelocityToApply.Y=0;
        Effect->bAdditiveVelocity=false; Mover->QueueInstantMovementEffect(Effect);
        Mover->SetPlanarConstraint(Constraint);
    }
}

bool UPWGSidescrollerInputComponent::IsTraversalActive() const
{
    if (!GetOwner()) return false;
    for (const UActorComponent* Component : GetOwner()->GetComponents())
    {
        if (!Component || !Component->ComponentHasTag(TEXT("StateManager"))) continue;
        const auto* State = FindFProperty<FByteProperty>(Component->GetClass(), TEXT("ParkourState"));
        if (State && State->Enum && State->Enum->GetNameStringByValue(State->GetPropertyValue_InContainer(Component)) != TEXT("NewEnumerator0"))
            return true; // AST Ground is the only state driven by normal lane movement.
    }
    // GASP also exposes traversal before the AST state transition.
    for (const auto* Component:GetOwner()->GetComponents())
        if (Component && (Component->GetClass()->GetName().StartsWith(TEXT("AC_TraversalLogic")) ||
            Component->GetClass()->GetName().StartsWith(TEXT("BPC_ParkourSystem"))))
            if (const auto* Property=FindFProperty<FBoolProperty>(Component->GetClass(),TEXT("DoingTraversalAction")))
                if (Property->GetPropertyValue_InContainer(Component)) return true;
    return false;
}


bool UPWGSidescrollerInputComponent::IsASTInteractionActive() const
{
    if (!GetOwner()) return false;
    for (const auto* Component:GetOwner()->GetComponents())
    {
        if (!Component || !Component->GetClass()->GetName().StartsWith(TEXT("BPC_StateManager"))) continue;
        if (const auto* Property=FindFProperty<FByteProperty>(Component->GetClass(),TEXT("InteractionState")))
        {
            const uint8 Value=Property->GetPropertyValue_InContainer(Component);
            return Property->Enum && Property->Enum->GetNameStringByValue(Value)!=TEXT("NewEnumerator0");
        }
    }
    return false;
}
