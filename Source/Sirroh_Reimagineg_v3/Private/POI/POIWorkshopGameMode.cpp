#include "POI/POIWorkshopGameMode.h"
#include "PWG/PWGPlayerController.h"
#include "PWG/PWGSidescrollerInputComponent.h"
#include "MoverComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

APOIWorkshopGameMode::APOIWorkshopGameMode()
{
    DefaultPawnClass=nullptr; PlayerControllerClass=APWGPlayerController::StaticClass();
}
UClass* APOIWorkshopGameMode::GetDefaultPawnClassForController_Implementation(AController*)
{ return LoadClass<APawn>(nullptr,TEXT("/Game/SirrohIntegration/BP_SirrohCharacter.BP_SirrohCharacter_C")); }
APawn* APOIWorkshopGameMode::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer,const FTransform& SpawnTransform)
{
    UClass* PawnClass=GetDefaultPawnClassForController(NewPlayer); if (!PawnClass) return nullptr;
    auto* Pawn=GetWorld()->SpawnActorDeferred<APawn>(PawnClass,SpawnTransform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!Pawn) return nullptr;
    auto* Input=NewObject<UPWGSidescrollerInputComponent>(Pawn,TEXT("POIWorkshopInput"));
    Pawn->AddInstanceComponent(Input); Input->RegisterComponent();
    if (auto* Mover=Pawn->FindComponentByClass<UMoverComponent>()) { Mover->InputProducer=Input; Mover->bGatherInputFromAllInputProducerComponents=false; }
    UGameplayStatics::FinishSpawningActor(Pawn,SpawnTransform); return Pawn;
}
