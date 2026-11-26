#include "Character/SirrohCharacterIdentityComponent.h"
#include "Character/SirrohProceduralHairComponent.h"
#include "Character/SirrohCharacterSaveGame.h"
#include "Character/SirrohPoseBridgeComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

USirrohCharacterIdentityComponent::USirrohCharacterIdentityComponent()
{
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick = false;
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> Mesh(TEXT("/Game/Characters/GrimModular/Meshes/SKM_Grim_Default.SKM_Grim_Default"));
    CharacterMesh = Mesh.Object;
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Game/Characters/GrimModular/Materials/M_Grim_Ink.M_Grim_Ink"));
    CharacterMaterial = Material.Object;
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> V7Mesh(TEXT("/Game/Characters/GrimModularV7/Meshes/SKM_GrimV7_Default.SKM_GrimV7_Default"));
    GrimV7Mesh = V7Mesh.Object;
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> V7Material(TEXT("/Game/Characters/GrimModularV7/Materials/M_GrimV7_Ink.M_GrimV7_Ink"));
    GrimV7Material = V7Material.Object;
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> V10Mesh(TEXT("/Game/Characters/GrimModularV10/Meshes/SKM_GrimV10_Default.SKM_GrimV10_Default"));
    GrimV10Mesh = V10Mesh.Object;
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> V10Material(TEXT("/Game/Characters/GrimModularV10/Materials/M_GrimV10_Ink.M_GrimV10_Ink"));
    GrimV10Material = V10Material.Object;
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> SimpleMesh(TEXT("/Game/Characters/SimpleMan/Meshes/SKM_SimpleMan_Default.SKM_SimpleMan_Default"));
    SimpleManMesh = SimpleMesh.Object;
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> SimpleMaterial(TEXT("/Game/Characters/SimpleMan/Materials/M_SimpleMan_Ink.M_SimpleMan_Ink"));
    SimpleManMaterial = SimpleMaterial.Object;
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> GirlMesh(TEXT("/Game/Characters/GrimGirl/Meshes/SKM_GrimGirl_Default.SKM_GrimGirl_Default"));
    GrimGirlMesh = GirlMesh.Object;
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> GirlMaterial(TEXT("/Game/Characters/GrimGirl/Materials/M_GrimGirl_Ink.M_GrimGirl_Ink"));
    GrimGirlMaterial = GirlMaterial.Object;
}

USirrohCharacterIdentityComponent* USirrohCharacterIdentityComponent::FindOrCreate(AActor* Owner)
{
    if (!Owner) return nullptr;
    if (auto* Existing = Owner->FindComponentByClass<USirrohCharacterIdentityComponent>()) return Existing;
    auto* Component = NewObject<USirrohCharacterIdentityComponent>(Owner, TEXT("CharacterIdentity"));
    Owner->AddInstanceComponent(Component);
    Component->RegisterComponent();
    return Component;
}

bool USirrohCharacterIdentityComponent::ApplySaved(AActor* Owner)
{
    if (!Owner || !Owner->HasAuthority()) return false;
    const auto* Save = Cast<USirrohCharacterSaveGame>(UGameplayStatics::LoadGameFromSlot(TEXT("SirrohCharacter"), 0));
    if (!Save) return false;
    FSirrohCharacterProfile Data = Save->Profile;
    if (Save->ProfileVersion < 2) Data.Name = Save->CharacterName.TrimStartAndEnd();
    if (Save->ProfileVersion < 3) Data.BodyType = ESirrohBodyType::GrimV1;
    return FindOrCreate(Owner)->ApplyProfile(Data);
}

bool USirrohCharacterIdentityComponent::ApplyProfile(const FSirrohCharacterProfile& InProfile)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !InProfile.IsValid()) return false;
    const FSirrohCharacterProfile Previous = Profile;
    Profile = InProfile;
    if (!ApplyVisual()) { Profile = Previous; return false; }
    if (auto* Pawn = Cast<APawn>(GetOwner()))
        if (auto* State = Pawn->GetPlayerState()) State->SetPlayerName(Profile.Name);
    GetOwner()->ForceNetUpdate();
    return true;
}

void USirrohCharacterIdentityComponent::OnRep_Profile() { ApplyVisual(); }

bool USirrohCharacterIdentityComponent::ApplyVisual()
{
    USkeletalMesh* SelectedMesh = Profile.Sex == ESirrohSex::Female ? GrimGirlMesh.Get() : Profile.BodyType == ESirrohBodyType::SimpleMan ? SimpleManMesh.Get() : Profile.BodyType == ESirrohBodyType::GrimV10 ? GrimV10Mesh.Get() :
        (Profile.BodyType == ESirrohBodyType::GrimV7 ? GrimV7Mesh.Get() : CharacterMesh.Get());
    UMaterialInterface* SelectedMaterial = Profile.Sex == ESirrohSex::Female ? GrimGirlMaterial.Get() : Profile.BodyType == ESirrohBodyType::SimpleMan ? SimpleManMaterial.Get() : Profile.BodyType == ESirrohBodyType::GrimV10 ? GrimV10Material.Get() :
        (Profile.BodyType == ESirrohBodyType::GrimV7 ? GrimV7Material.Get() : CharacterMaterial.Get());
    if (!GetOwner() || !SelectedMesh || !SelectedMaterial) return false;
    // Preview actors render their own mesh in reference pose. Only gameplay pawns need the pose bridge.
    const bool bGameplay = GetOwner()->IsA<APawn>();
    auto* Bridge = bGameplay ? USirrohPoseBridgeComponent::FindOrCreate(GetOwner()) : nullptr;
    USkeletalMeshComponent* Mesh = bGameplay ? (Bridge ? Bridge->GetSourceMesh() : nullptr)
        : GetOwner()->FindComponentByClass<USkeletalMeshComponent>();
    if (!Mesh || !Mesh->GetSkeletalMeshAsset()) return false;
    if (Mesh->GetSkeletalMeshAsset()->GetSkeleton() != SelectedMesh->GetSkeleton())
    {
        UE_LOG(LogTemp, Error, TEXT("CharacterIdentity: incompatible player skeleton on %s"), *GetNameSafe(GetOwner()));
        return false;
    }
    // Replace the actual Mover mesh, not just a detached creator preview. Keep its AnimBP.
    if (Mesh->GetSkeletalMeshAsset() != SelectedMesh) Mesh->SetSkeletalMesh(SelectedMesh, false);
    Mesh->EmptyOverrideMaterials();
    for (int32 Slot = 0; Slot < SelectedMesh->GetMaterials().Num(); ++Slot)
        Mesh->SetMaterial(Slot, SelectedMaterial);
    Mesh->SetPhysicsAsset(SelectedMesh->GetPhysicsAsset(), false);
    for (int32 Eye = 0; Eye < 2; ++Eye)
    {
        const int32 Slot = Mesh->GetMaterialIndex(Eye == 0 ? TEXT("Eye_l") : TEXT("Eye_r"));
        if (Slot == INDEX_NONE) return false;
        auto* Material = Mesh->CreateDynamicMaterialInstance(Slot, SelectedMaterial);
        if (!Material) return false;
        const ESirrohIrisColor Color = Eye == 0 ? Profile.LeftEye : Profile.RightEye;
        Material->SetScalarParameterValue(TEXT("OverrideIrisColor"), 1.f);
        Material->SetVectorParameterValue(TEXT("IrisColor"), FLinearColor::FromSRGBColor(
            Color == ESirrohIrisColor::Cyan ? FColor(26,196,206) : FColor(195,216,40)));
    }
    // Refresh the existing final-pose bridge and mirror the new materials immediately.
    if (bGameplay) USirrohPoseBridgeComponent::FindOrCreate(GetOwner());
    else { Mesh->SetVisibility(true); Mesh->SetHiddenInGame(false); }
    auto* Hair=GetOwner()->FindComponentByClass<USirrohProceduralHairComponent>();
    if (!Hair) {
        Hair=NewObject<USirrohProceduralHairComponent>(GetOwner(),TEXT("ProceduralHair"));
        GetOwner()->AddInstanceComponent(Hair);
        Hair->SetupAttachment(GetOwner()->GetRootComponent());
        Hair->RegisterComponent();
    }
    Hair->Configure(Mesh,Profile);
    UE_LOG(LogTemp, Display, TEXT("CharacterIdentity applied '%s': %s, eyes %d/%d"),
        *Profile.Name, *GetNameSafe(SelectedMesh), uint8(Profile.LeftEye), uint8(Profile.RightEye));
    return true;
}

void USirrohCharacterIdentityComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(USirrohCharacterIdentityComponent, Profile);
}
