#include "POI/POIStateComponent.h"
#include "POI/POIGenerator.h"
#include "GameFramework/Actor.h"
#include "UObject/UnrealType.h"

namespace
{
bool SupportedProperty(const FProperty* P)
{
    return P && P->ArrayDim==1 && (CastField<FBoolProperty>(P) || CastField<FNumericProperty>(P) || CastField<FEnumProperty>(P) || CastField<FNameProperty>(P) || CastField<FStrProperty>(P));
}
}

UPOIStateComponent::UPOIStateComponent() { PrimaryComponentTick.bCanEverTick=false; }

void UPOIStateComponent::BindToGenerator(APOIGenerator* Generator,const FString& Key)
{
    OwningGenerator=Generator; PersistentKey=Key;
}

FPOIObjectState UPOIStateComponent::Capture(const FTransform& ModuleTransform) const
{
    FPOIObjectState S; S.bActive=bActive; S.bLootInitialized=bLootInitialized; S.Loot=Loot;
    const AActor* Owner=GetOwner();
    S.bHasTransform=bSaveTransform; S.LocalTransform=Owner->GetActorTransform().GetRelativeTransform(ModuleTransform);
    for (FName Name:SavedActorProperties)
    {
        const FProperty* P=Owner->GetClass()->FindPropertyByName(Name);
        if (SupportedProperty(P)) { FString Value; P->ExportText_InContainer(0,Value,Owner,Owner,const_cast<AActor*>(Owner),PPF_None); S.Properties.Add(Name,Value); }
    }
    return S;
}

void UPOIStateComponent::Restore(const FPOIObjectState& S,const FTransform& ModuleTransform)
{
    if (S.bRemoved) { GetOwner()->Destroy(); return; }
    bActive=S.bActive; bLootInitialized=S.bLootInitialized; Loot=S.Loot;
    if (bSaveTransform && S.bHasTransform) GetOwner()->SetActorTransform(S.LocalTransform*ModuleTransform,false,nullptr,ETeleportType::TeleportPhysics);
    for (FName Name:SavedActorProperties)
    {
        FProperty* P=GetOwner()->GetClass()->FindPropertyByName(Name); const FString* Value=S.Properties.Find(Name);
        if (Value && SupportedProperty(P)) P->ImportText_InContainer(**Value,GetOwner(),GetOwner(),PPF_None);
    }
    OnStateRestored.Broadcast();
}

void UPOIStateComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if (Reason==EEndPlayReason::Destroyed && OwningGenerator.IsValid() && !PersistentKey.IsEmpty()) OwningGenerator->RecordRemovedObject(PersistentKey);
    Super::EndPlay(Reason);
}
