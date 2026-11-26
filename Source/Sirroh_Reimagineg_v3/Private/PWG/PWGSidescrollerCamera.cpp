#include "PWG/PWGSidescrollerCamera.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetMathLibrary.h"
#include "Procedural_World_Generator/PWGv2Generator.h"
#include "Procedural_World_Generator/PWGv2Generator.h"
#include "Combat/DirectionalCombatComponent.h"

APWGSidescrollerCamera::APWGSidescrollerCamera()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	GetCameraComponent()->ProjectionMode = ECameraProjectionMode::Orthographic;
	GetCameraComponent()->OrthoWidth = 1520.0f;
	GetCameraComponent()->FieldOfView = 45.0f;
	GetCameraComponent()->bConstrainAspectRatio = false;
	GetCameraComponent()->bOverrideAspectRatioAxisConstraint = true;
	GetCameraComponent()->SetAspectRatioAxisConstraint(EAspectRatioAxisConstraint::AspectRatio_MaintainXFOV);
}

void APWGSidescrollerCamera::SetFollowTarget(AActor* NewTarget, const bool bSnapImmediately)
{
	FollowTarget = NewTarget;
	UpdateCamera(0.0f, bSnapImmediately);
}

void APWGSidescrollerCamera::SetGroundProvider(APWGv2Generator* NewGroundProvider)
{
	GroundProvider = NewGroundProvider;
    if (Cast<APWGv2Generator>(NewGroundProvider))
    {
        auto& Exposure=GetCameraComponent()->PostProcessSettings;
        Exposure.bOverride_AutoExposureMethod=true; Exposure.AutoExposureMethod=AEM_Manual;
        Exposure.bOverride_AutoExposureApplyPhysicalCameraExposure=true; Exposure.AutoExposureApplyPhysicalCameraExposure=false;
        Exposure.bOverride_AutoExposureBias=true; Exposure.AutoExposureBias=0;
        GetCameraComponent()->PostProcessBlendWeight=1;
    }
	bHasGroundHeight = false;
}

void APWGSidescrollerCamera::AddScrollZoom(float Steps)
{
    RequestedZoom=FMath::Clamp(RequestedZoom+Steps*.1f,0.f,1.f);
}

void APWGSidescrollerCamera::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateCamera(DeltaSeconds, false);
}

void APWGSidescrollerCamera::UpdateCamera(const float DeltaSeconds, const bool bSnap)
{
	if (!IsValid(FollowTarget)) return;
	const FVector TargetLocation = FollowTarget->GetActorLocation();
	if (NormalOrthoWidth < 0) NormalOrthoWidth = GetCameraComponent()->OrthoWidth;
	const auto* Combat = FollowTarget->FindComponentByClass<UDirectionalCombatComponent>();
	const float Goal = Combat && Combat->IsCombatActive() ? 1.f : 0.f;
	const float Blend = Combat ? FMath::Max(0.01f, Combat->CombatCameraBlendSeconds) : 0.3f;
	CombatAlpha = bSnap ? Goal : FMath::FInterpConstantTo(CombatAlpha, Goal, DeltaSeconds, 1.f / Blend);
	const float Weight = FMath::SmoothStep(0.f, 1.f, CombatAlpha);
    int32 ViewWidth=0,ViewHeight=0;
    if(auto* PC=Cast<APlayerController>(GetOwner()))PC->GetViewportSize(ViewWidth,ViewHeight);
    const float Aspect=ViewWidth>0 && ViewHeight>0?float(ViewWidth)/ViewHeight:16.f/9.f;
    FVector BodyCenter=TargetLocation;
    float BodyHeight=180.f;
    if(auto* Capsule=FollowTarget->FindComponentByClass<UCapsuleComponent>()){
        BodyHeight=2*Capsule->GetScaledCapsuleHalfHeight();BodyCenter=Capsule->GetComponentLocation();
    }
    // Reference body bounds avoid zoom pumping with each animation, weapons or hair movement.
    if(auto* Mesh=FollowTarget->FindComponentByClass<USkeletalMeshComponent>();Mesh && Mesh->GetSkeletalMeshAsset()){
        const auto Bounds=Mesh->GetSkeletalMeshAsset()->GetBounds().TransformBy(Mesh->GetComponentTransform());
        BodyHeight=FMath::Max(1.f,float(Bounds.BoxExtent.Z*2));BodyCenter=Bounds.Origin;
    }
    const float MinWidth=BodyHeight*Aspect/.9f;
    const float MaxWidth=FMath::Max(NormalOrthoWidth,MinWidth);
    SmoothedZoom=bSnap?RequestedZoom:FMath::FInterpTo(SmoothedZoom,RequestedZoom,DeltaSeconds,10.f);
    const float ManualWidth=FMath::Lerp(MaxWidth,MinWidth,SmoothedZoom);
    const float ViewWidthWorld=FMath::Clamp(ManualWidth*FMath::Lerp(1.f,
        Combat?FMath::Clamp(Combat->CombatViewWidthMultiplier,.2f,1.f):1.f,Weight),MinWidth,MaxWidth);
    GetCameraComponent()->SetOrthoWidth(ViewWidthWorld);
    const float CloseBlend=MaxWidth>MinWidth+1.f?FMath::Clamp((MaxWidth-ViewWidthWorld)/(MaxWidth-MinWidth),0.f,1.f):1.f;
	float DesiredGroundHeight = TargetLocation.Z;
	if (IsValid(GroundProvider))
	{
		const float SurfaceHeight = GroundProvider->GetCameraGroundHeight(TargetLocation);
		if (SurfaceHeight > -BIG_NUMBER * 0.5f) DesiredGroundHeight = SurfaceHeight;
	}
	if (bSnap || !bHasGroundHeight)
	{
		SmoothedGroundHeight = DesiredGroundHeight;
		bHasGroundHeight = true;
	}
	else
	{
		SmoothedGroundHeight = FMath::FInterpTo(
			SmoothedGroundHeight, DesiredGroundHeight, DeltaSeconds, GroundHeightLagSpeed);
	}

	FVector FocusPoint(TargetLocation.X + FocusOffset.X, TargetLocation.Y + FocusOffset.Y,
		FMath::Lerp(SmoothedGroundHeight + FocusOffset.Z, TargetLocation.Z + 20, Weight));
	FocusPoint=FMath::Lerp(FocusPoint,BodyCenter,CloseBlend);
	const float AngleRadians = FMath::DegreesToRadians(CameraAngle);
	const FVector CameraOffset(FMath::Sin(AngleRadians) * CameraDistance,
		-FMath::Cos(AngleRadians) * CameraDistance, CameraHeight);
	FVector DesiredLocation(TargetLocation.X + CameraOffset.X, TargetLocation.Y + CameraOffset.Y,
		FMath::Lerp(SmoothedGroundHeight + CameraOffset.Z, TargetLocation.Z + 20, Weight));
	DesiredLocation=FMath::Lerp(DesiredLocation,BodyCenter+FVector(CameraOffset.X,CameraOffset.Y,0),CloseBlend);
	FVector NewLocation = bSnap || FollowLagSpeed <= 0.0f
		? DesiredLocation
		: FMath::VInterpTo(GetActorLocation(), DesiredLocation, DeltaSeconds, FollowLagSpeed);
	// At maximum magnification keep the full body framed, including during jumps.
	NewLocation=FMath::Lerp(NewLocation,DesiredLocation,CloseBlend);
	SetActorLocationAndRotation(NewLocation, UKismetMathLibrary::FindLookAtRotation(NewLocation, FocusPoint));
    if (IsValid(GroundProvider) && GetNetMode() != NM_DedicatedServer)
    {
        const bool bSingleOrthoView = GetCameraComponent()->ProjectionMode == ECameraProjectionMode::Orthographic &&
            GetGameInstance() && GetGameInstance()->GetNumLocalPlayers() == 1;
        GroundProvider->UpdateGameplayView(GetCameraComponent()->GetComponentTransform(), bSingleOrthoView ? ViewWidthWorld : 0.f, Aspect);
    }
}

FVector APWGSidescrollerCamera::GetScreenRightMovementDirection() const
{
	const FVector CameraRight = GetCameraComponent()->GetRightVector();
	const float XSign = CameraRight.X >= 0.0f ? 1.0f : -1.0f;
	return FVector(XSign, 0.0f, 0.0f);
}


