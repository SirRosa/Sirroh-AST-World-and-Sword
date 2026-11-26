#pragma once


#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MoverSimulationTypes.h"
#include "PWGSidescrollerInputComponent.generated.h"


/** Asset-free keyboard/gamepad input producer for a Mover sidescroller Pawn. */
UCLASS(ClassGroup=(PWG), meta=(BlueprintSpawnableComponent))
class SIRROH_REIMAGINEG_V3_API UPWGSidescrollerInputComponent
        : public UActorComponent, public IMoverInputProducerInterface
{
        GENERATED_BODY()


public:
        UPWGSidescrollerInputComponent();

        virtual void InitializeComponent() override;


        virtual void ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult) override;


        void SetScreenRightDirection(const FVector& Direction);
        void BeginLaneMove(float TargetY);
        void CancelLaneMove();
        bool IsLaneMoveActive() const { return bLaneMove; }
        bool IsTraversalActive() const;
        bool IsASTInteractionActive() const;
        FVector GetScreenRightDirection() const { return ScreenRightDirection; }


        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PWG|Input")
        bool bEnableJump = true;
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PWG|Input", meta=(ClampMin="0",ClampMax="0.5"))
        float CoyoteTime = 0.2f;
        void InvalidateCoyoteTime() { LastGroundedMs=-1000000; bCoyoteConsumed=true; }



private:
        TWeakObjectPtr<AActor> HeldPushable;
        FVector PushFacing=FVector::ZeroVector;
        float PushLaneY=0;
        FVector ScreenRightDirection = -FVector::XAxisVector;
        bool bWasJumpPressed = false;
        int32 LastGroundedMs=-1000000;
        bool bWasGrounded=false, bCoyoteConsumed=false;
        bool bLaneMove = false;
        bool bRestorePlaneAfterRagdoll = false;
        bool bRestorePlaneAfterTraversal = false;
        float LaneTargetY = 0;
        float LaneStartY = 0;
        FVector StableCombatFacing = FVector::ZeroVector;
        float SideFacingSign = 1.f;
        bool bWasLeftDown=false, bWasRightDown=false;
        int32 LastLeftTap=-1000, LastRightTap=-1000;
};
