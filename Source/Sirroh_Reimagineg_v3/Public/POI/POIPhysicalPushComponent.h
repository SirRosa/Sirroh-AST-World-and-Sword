#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MoverComponent.h"
#include "POIPhysicalPushComponent.generated.h"

/** AST owns interaction/IK; this component owns only physical transport. */
UCLASS()
class SIRROH_REIMAGINEG_V3_API UPOIPhysicalPushComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UPOIPhysicalPushComponent();
 bool IsWithinReach() const;
 void StartDrive();
 void CaptureBeforeGrab();
 bool IsDrivenBy(const AActor* Pawn) const;
 float LimitInput(float WorldAxis);
 static void LimitMotion(double BodyX,double BodyVelocity,double Offset,float WorldAxis,FVector& Position,FVector& Velocity);
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 virtual void TickComponent(float Dt,ELevelTick Type,FActorComponentTickFunction* Tick) override;
private:
 UFUNCTION() void AfterMovement(const FMoverTimeStep& TimeStep,FMoverSyncState& State,FMoverAuxStateContext& Aux);
 TWeakObjectPtr<UMoverComponent> DriverMover;
 float RequestedAxis=0;
 TWeakObjectPtr<AActor> Driver;
 TWeakObjectPtr<class UPrimitiveComponent> Body;
 FVector LocalGrip;
 float OffsetX=0;
 FTransform GrabTransform;
 FVector GrabVelocity=FVector::ZeroVector;
 bool bCaptured=false;
 bool IsHeld() const;
};
