#include "POI/POIInteractiveProp.h"
#include "Components/PrimitiveComponent.h"

APOIInteractiveProp::APOIInteractiveProp()
{
    PersistentState=CreateDefaultSubobject<UPOIStateComponent>(TEXT("PersistentState"));
    PersistentState->bSaveTransform=false;
}

void APOIInteractiveProp::BeginPlay()
{
    Super::BeginPlay(); ClosedTransform=GetActorTransform();
    PersistentState->OnStateRestored.AddDynamic(this,&APOIInteractiveProp::ApplyState);
    if (!PersistentState->bLootInitialized) { PersistentState->Loot=InitialLoot; PersistentState->bLootInitialized=true; }
    if (Kind==EPOIInteractionKind::ReleasedPhysics) PersistentState->bSaveTransform=true;
    ApplyState();
}

bool APOIInteractiveProp::CanInteract(AActor* User) const
{
    return IsValid(User) && User->GetWorld()==GetWorld() && FVector::DistSquared(User->GetActorLocation(),GetActorLocation())<=FMath::Square(FMath::Max(1.f,InteractionDistance));
}

bool APOIInteractiveProp::Interact(AActor* User)
{
    if (!CanInteract(User)) return false;
    SetActivated(!PersistentState->bActive);
    if (Kind==EPOIInteractionKind::Lever)
        for (APOIInteractiveProp* Target:Targets) if (IsValid(Target) && Target!=this && Target->GetLevel()==GetLevel()) Target->SetActivated(PersistentState->bActive);
    return true;
}

void APOIInteractiveProp::SetActivated(bool Active)
{
    PersistentState->bActive=Active; ApplyState();
}

void APOIInteractiveProp::ApplyState()
{
    const bool Active=PersistentState->bActive;
    if (Kind==EPOIInteractionKind::Door)
    {
        SetActorLocation(ClosedTransform.GetLocation()+(Active?ClosedTransform.TransformVector(OpenOffset):FVector::ZeroVector),false,nullptr,ETeleportType::TeleportPhysics);
        SetActorEnableCollision(!Active);
    }
    else if (Kind==EPOIInteractionKind::ReleasedPhysics)
    {
        if (auto* PhysicsBody=Cast<UPrimitiveComponent>(GetRootComponent())) PhysicsBody->SetSimulatePhysics(Active);
    }
}

bool APOIInteractiveProp::TakeLoot(AActor* User,FName Item)
{
    if (Kind!=EPOIInteractionKind::Container || !PersistentState->bActive || !CanInteract(User)) return false;
    const int32 Index=PersistentState->Loot.Find(Item);
    if (Index==INDEX_NONE) return false;
    PersistentState->Loot.RemoveAt(Index); return true;
}
