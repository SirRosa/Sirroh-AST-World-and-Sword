#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MoverComponent.h"
#include "POIMovingTraversalComponent.generated.h"

/** Keeps AST targets in the support's frame; Mover alone moves the pawn. */
UCLASS()
class SIRROH_REIMAGINEG_V3_API UPOIMovingTraversalComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 bool IsTracking(const UPrimitiveComponent* Support) const { return Base.Get()==Support && IsActive(); }
 static void Track(AActor* Pawn,UPrimitiveComponent* Support,bool FreshSelection=false);
 static void PrepareLedge(AActor* Pawn,AActor* Edge);
 static void StopTracking(AActor* Pawn);
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
 UFUNCTION() void AfterMovement(const FMoverTimeStep& TimeStep,FMoverSyncState& State,FMoverAuxStateContext& Aux);
 UFUNCTION() void BeforeWarp(class UMotionWarpingComponent* Warp);
 bool IsActive() const;
 void ClearTargets();
 TWeakObjectPtr<UPrimitiveComponent> Base;
 FTransform SelectionTransform=FTransform::Identity;
 TWeakObjectPtr<UMoverComponent> Mover;
 TWeakObjectPtr<class UMotionWarpingComponent> Warping;
 UPROPERTY(Transient) TMap<FName,TObjectPtr<USceneComponent>> Anchors;
};
