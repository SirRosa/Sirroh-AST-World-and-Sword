#include "PWG/PWGLedgeActor.h"

#include "Components/BoxComponent.h"

APWGLedgeActor::APWGLedgeActor()
{
	PrimaryActorTick.bCanEverTick = false;
	DetectionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TraversalDetection"));
	SetRootComponent(DetectionBox);
	DetectionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	DetectionBox->SetCollisionObjectType(ECC_WorldDynamic);
	DetectionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	DetectionBox->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
	DetectionBox->SetGenerateOverlapEvents(false);
}

void APWGLedgeActor::ConfigureLedge(const FVector& WallCenter, const FVector& WallExtent,
	const FTransform& InFrontLedge, const FTransform& InBackLedge)
{
	SetActorLocation(WallCenter);
	DetectionBox->SetBoxExtent(WallExtent, true);
	FrontLedgeTransform = InFrontLedge;
	BackLedgeTransform = InBackLedge;
}

bool APWGLedgeActor::GetPWGLedgeTransforms_Implementation(FTransform& FrontLedge, FTransform& BackLedge) const
{
	FrontLedge = FrontLedgeTransform;
	BackLedge = BackLedgeTransform;
	return true;
}

