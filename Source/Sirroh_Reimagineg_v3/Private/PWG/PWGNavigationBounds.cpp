#include "PWG/PWGNavigationBounds.h"
#include "Components/BoxComponent.h"
APWGNavigationBounds::APWGNavigationBounds(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
    auto* Bounds=CreateDefaultSubobject<UBoxComponent>(TEXT("RuntimeBounds"));
    Bounds->SetupAttachment(GetRootComponent());
    Bounds->SetMobility(EComponentMobility::Static);
    Bounds->SetBoxExtent(FVector(3000,900,1800));
    Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Bounds->SetCanEverAffectNavigation(false);
    Bounds->SetHiddenInGame(true);
}
