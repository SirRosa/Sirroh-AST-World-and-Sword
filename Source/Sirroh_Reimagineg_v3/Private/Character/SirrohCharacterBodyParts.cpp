#include "Character/SirrohCharacterCustomizationComponent.h"
#include "Character/SirrohCharacterCustomizationCatalog.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Actor.h"

bool USirrohCharacterCustomizationComponent::SetBodyPart(FName Part, USkeletalMesh* Replacement, bool bHidden)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return false;
    // Dedicated servers have no presentation components; validate against the selected asset.
    const FSirrohCharacterBodyPreset* Preset = USirrohCharacterCustomizationCatalog::Get()->FindBodyPreset(BodyPresetId);
    USkeletalMesh* Base = Preset ? Preset->Mesh.LoadSynchronous() : nullptr;
    if (!Base || !Base->GetMaterials().ContainsByPredicate([Part](const FSkeletalMaterial& Material)
        { return Material.MaterialSlotName == Part; })) return false;
    if (Replacement && (Replacement->GetSkeleton() != Base->GetSkeleton() ||
        Replacement->GetPhysicsAsset() != Base->GetPhysicsAsset())) return false;

    FSirrohBodyPartOverride* State = BodyParts.FindByPredicate([Part](const auto& Item) { return Item.Part == Part; });
    if (!State)
    {
        State = &BodyParts.AddDefaulted_GetRef();
        State->Part = Part;
    }
    State->Mesh = Replacement;
    State->bHidden = bHidden;
    RefreshBodyParts();
    GetOwner()->ForceNetUpdate();
    return true;
}

void USirrohCharacterCustomizationComponent::ResetBodyParts()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    BodyParts.Reset();
    RefreshBodyParts();
    GetOwner()->ForceNetUpdate();
}

void USirrohCharacterCustomizationComponent::OnRep_BodyParts()
{
    RefreshBodyParts();
}

void USirrohCharacterCustomizationComponent::ClearBodyPartVisuals()
{
    for (USkeletalMeshComponent* Part : PartVisuals)
        if (Part) Part->DestroyComponent();
    PartVisuals.Reset();
    if (CharacterVisualMesh)
        for (int32 LOD = 0; LOD < CharacterVisualMesh->GetNumLODs(); ++LOD)
            CharacterVisualMesh->ShowAllMaterialSections(LOD);
}

void USirrohCharacterCustomizationComponent::RefreshBodyParts()
{
    ClearBodyPartVisuals();
    if (!CharacterVisualMesh || !CharacterVisualMesh->IsVisible() || !GetOwner() ||
        GetOwner()->GetNetMode() == NM_DedicatedServer) return;
    USkeletalMesh* Base = CharacterVisualMesh->GetSkeletalMeshAsset();
    if (!Base) return;
    for (const FSirrohBodyPartOverride& State : BodyParts)
    {
        const int32 Slot = CharacterVisualMesh->GetMaterialIndex(State.Part);
        if (Slot == INDEX_NONE) continue;
        USkeletalMesh* Replacement = State.bHidden ? nullptr : State.Mesh.LoadSynchronous();
        if (Replacement && (Replacement->GetSkeleton() != Base->GetSkeleton() ||
            Replacement->GetPhysicsAsset() != Base->GetPhysicsAsset())) continue;
        if (State.bHidden || Replacement)
            for (int32 LOD = 0; LOD < CharacterVisualMesh->GetNumLODs(); ++LOD)
                CharacterVisualMesh->ShowMaterialSection(Slot, INDEX_NONE, false, LOD);
        if (!Replacement) continue;
        USkeletalMeshComponent* Visual = NewObject<USkeletalMeshComponent>(GetOwner());
        GetOwner()->AddInstanceComponent(Visual);
        Visual->SetupAttachment(CharacterVisualMesh);
        ConfigureCommonVisual(Visual);
        Visual->SetSkeletalMeshAsset(Replacement);
        // Use the final pose directly: a leader can be the existing poseable bridge.
        Visual->SetLeaderPoseComponent(CharacterVisualMesh->LeaderPoseComponent.IsValid()
            ? CharacterVisualMesh->LeaderPoseComponent.Get() : CharacterVisualMesh.Get(), true, true);
        Visual->RegisterComponent();
        PartVisuals.Add(Visual);
    }
}
