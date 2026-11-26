#pragma once
#include "LayeredMove.h"
#include "CombatInertiaMove.generated.h"

/** Bounded ground-plane displacement; Mover performs collision and owns actual translation. */
USTRUCT()
struct FCombatInertiaMove : public FLayeredMoveBase
{
    GENERATED_BODY()
    UPROPERTY() bool bHitReaction = false;
    UPROPERTY() bool bLocalPath = false;
    UPROPERTY() bool bReactionTurn=false;
    UPROPERTY() float ReactionStartYaw=0;
    UPROPERTY() float ReactionTurnYaw=0;
    UPROPERTY() TArray<FVector> MotionPath;
    UPROPERTY() FVector Displacement = FVector::ZeroVector;
    UPROPERTY() FName GroundMode;
    UPROPERTY() float TravelMs = 250;
    FCombatInertiaMove();
    virtual bool SupportsAsync() const override { return true; }
    virtual bool GenerateMove(const FMoverTickStartData&, const FMoverTimeStep&, const UMoverComponent*, UMoverBlackboard*, FProposedMove&) override;
    virtual bool GenerateMove_Async(const FMoverTickStartData&, const FMoverTimeStep&, UMoverBlackboard*, FProposedMove&) override;
    virtual FLayeredMoveBase* Clone() const override { return new FCombatInertiaMove(*this); }
    virtual UScriptStruct* GetScriptStruct() const override { return StaticStruct(); }
    virtual void NetSerialize(FArchive& Ar) override;
    virtual bool HasGameplayTag(FGameplayTag Tag, bool Exact) const override;
};
