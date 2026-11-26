#include "Character/SirrohCharacterCustomizationComponent.h"








#include "Animation/AnimInstance.h"
#include "Character/SirrohCharacterCustomizationCatalog.h"
#include "Character/SirrohPoseBridgeComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"








DEFINE_LOG_CATEGORY_STATIC(LogSirrohCharacterCustomization, Log, All);








USirrohCharacterCustomizationComponent::USirrohCharacterCustomizationComponent()
{
        PrimaryComponentTick.bCanEverTick = false;
        SetIsReplicatedByDefault(true);
}








USirrohCharacterCustomizationComponent* USirrohCharacterCustomizationComponent::FindOrCreate(AActor* Owner)
{
        if (!Owner)
        {
                return nullptr;
        }
        if (USirrohCharacterCustomizationComponent* Existing = Owner->FindComponentByClass<USirrohCharacterCustomizationComponent>())
        {
                Existing->InitializePosePipeline();
                return Existing;
        }








        USirrohCharacterCustomizationComponent* Component = NewObject<USirrohCharacterCustomizationComponent>(Owner, TEXT("SirrohCharacterCustomization"));
        if (!Component)
        {
                return nullptr;
        }
        Owner->AddInstanceComponent(Component);
        Component->RegisterComponent();
        Component->InitializePosePipeline();
        return Component;
}








void USirrohCharacterCustomizationComponent::BeginPlay()
{
        Super::BeginPlay();
        InitializePosePipeline();
        BeginApplyPreset(BodyPresetId);
}








void USirrohCharacterCustomizationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
        ClearBodyPartVisuals();
        if (PendingLoadHandle.IsValid())
        {
                PendingLoadHandle->CancelHandle();
                PendingLoadHandle.Reset();
        }
        Super::EndPlay(EndPlayReason);
}








void USirrohCharacterCustomizationComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
        Super::GetLifetimeReplicatedProps(OutLifetimeProps);
        DOREPLIFETIME(USirrohCharacterCustomizationComponent, BodyPresetId);
        DOREPLIFETIME(USirrohCharacterCustomizationComponent, BodyParts);
}








bool USirrohCharacterCustomizationComponent::ApplyBodyPreset(const FName PresetId)
{
        const USirrohCharacterCustomizationCatalog* Catalog = USirrohCharacterCustomizationCatalog::Get();
        if (!Catalog || Catalog->BodyPresets.IsEmpty())
        {
                UE_LOG(LogSirrohCharacterCustomization, Error, TEXT("Character customization catalog is empty."));
                return false;
        }








        const FName ResolvedId = Catalog->SanitizePresetId(PresetId);
        if (ResolvedId.IsNone())
        {
                return false;
        }








        if (BodyPresetId != ResolvedId && GetOwner() && GetOwner()->HasAuthority())
        {
                BodyParts.Reset();
        }
        BodyPresetId = ResolvedId;
        BeginApplyPreset(ResolvedId);
        return true;
}








void USirrohCharacterCustomizationComponent::OnRep_BodyPresetId()
{
        BeginApplyPreset(BodyPresetId);
}








void USirrohCharacterCustomizationComponent::InitializePosePipeline()
{
        AActor* Owner = GetOwner();
        if (!Owner || Owner->GetNetMode() == NM_DedicatedServer)
        {
                return;
        }








        PoseBridge = USirrohPoseBridgeComponent::FindOrCreate(Owner);
        if (!PoseBridge)
        {
                UE_LOG(LogSirrohCharacterCustomization, Warning, TEXT("%s has no usable SirrohPoseBridge."), *Owner->GetName());
                return;
        }
        EnsureRuntimeMeshes();
}








void USirrohCharacterCustomizationComponent::EnsureRuntimeMeshes()
{
        if (!PoseBridge || !PoseBridge->GetSourceMesh() || !PoseBridge->GetVisualMesh() || !GetOwner())
        {
                return;
        }








        if (!RetargetSourceMesh)
        {
                RetargetSourceMesh = NewObject<USkeletalMeshComponent>(GetOwner(), TEXT("SirrohRetargetSourceMesh"));
                GetOwner()->AddInstanceComponent(RetargetSourceMesh);
                ConfigureCommonVisual(RetargetSourceMesh);
                RetargetSourceMesh->SetVisibility(false, false);
                RetargetSourceMesh->SetHiddenInGame(true, false);
                RetargetSourceMesh->SetCastShadow(false);
                RetargetSourceMesh->RegisterComponent();
                RetargetSourceMesh->PrimaryComponentTick.TickGroup = TG_PostPhysics;
                RetargetSourceMesh->AddTickPrerequisiteComponent(PoseBridge);
        }








        if (!CharacterVisualMesh)
        {
                CharacterVisualMesh = NewObject<USkeletalMeshComponent>(GetOwner(), TEXT("SirrohCharacterVisualMesh"));
                GetOwner()->AddInstanceComponent(CharacterVisualMesh);
                ConfigureCommonVisual(CharacterVisualMesh);
                CharacterVisualMesh->RegisterComponent();
                CharacterVisualMesh->PrimaryComponentTick.TickGroup = TG_PostPhysics;
                CharacterVisualMesh->AddTickPrerequisiteComponent(PoseBridge);
        }








        USkeletalMesh* SourceAsset = PoseBridge->GetSourceMesh()->GetSkeletalMeshAsset();
        if (SourceAsset && RetargetSourceMesh->GetSkeletalMeshAsset() != SourceAsset)
        {
                RetargetSourceMesh->SetSkeletalMeshAsset(SourceAsset);
        }








        // PoseBridge's poseable visual contains locomotion + procedural hand/combat overrides.
        // This hidden USkeletalMeshComponent follows that final pose so ABP_GenericRetarget can
        // use a real skeletal component as its ParentSkeletalMeshComponent source.
        RetargetSourceMesh->SetLeaderPoseComponent(PoseBridge->GetVisualMesh(), true, true);
        ConfigurePlacement(RetargetSourceMesh, FTransform::Identity);
}








void USirrohCharacterCustomizationComponent::ConfigureCommonVisual(USkeletalMeshComponent* MeshComponent)
{
        if (!MeshComponent)
        {
                return;
        }
        MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        MeshComponent->SetGenerateOverlapEvents(false);
        MeshComponent->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
}








void USirrohCharacterCustomizationComponent::ConfigurePlacement(USkeletalMeshComponent* MeshComponent, const FTransform& Offset) const
{
        if (!MeshComponent || !PoseBridge || !PoseBridge->GetVisualMesh())
        {
                return;
        }








        UPoseableMeshComponent* FinalPose = PoseBridge->GetVisualMesh();
        USceneComponent* Parent = FinalPose->GetAttachParent();
        if (Parent)
        {
                MeshComponent->AttachToComponent(Parent, FAttachmentTransformRules::KeepRelativeTransform, FinalPose->GetAttachSocketName());
                MeshComponent->SetRelativeTransform(Offset * FinalPose->GetRelativeTransform());
        }
        else if (USkeletalMeshComponent* Source = PoseBridge->GetSourceMesh())
        {
                MeshComponent->AttachToComponent(Source, FAttachmentTransformRules::KeepRelativeTransform);
                MeshComponent->SetRelativeTransform(Offset);
        }
}








void USirrohCharacterCustomizationComponent::BeginApplyPreset(const FName PresetId)
{
        InitializePosePipeline();
        AActor* Owner = GetOwner();
        if (!Owner || !PoseBridge || Owner->GetNetMode() == NM_DedicatedServer)
        {
                return;
        }








        const USirrohCharacterCustomizationCatalog* Catalog = USirrohCharacterCustomizationCatalog::Get();
        const FSirrohCharacterBodyPreset* Preset = Catalog ? Catalog->FindBodyPreset(PresetId) : nullptr;
        if (!Preset)
        {
                UE_LOG(LogSirrohCharacterCustomization, Warning, TEXT("Unknown body preset '%s'."), *PresetId.ToString());
                ShowDefaultSourceVisual();
                return;
        }








        ++LoadGeneration;
        const uint32 Generation = LoadGeneration;
        if (PendingLoadHandle.IsValid())
        {
                PendingLoadHandle->CancelHandle();
                PendingLoadHandle.Reset();
        }








        if (Preset->BindingMode == ESirrohCharacterPoseBinding::DefaultSource)
        {
                ShowDefaultSourceVisual();
                return;
        }








        TArray<FSoftObjectPath> Paths;
        if (!Preset->Mesh.IsNull())
        {
                Paths.Add(Preset->Mesh.ToSoftObjectPath());
        }
        if (Preset->BindingMode == ESirrohCharacterPoseBinding::IKRetarget && Catalog && !Catalog->GenericRetargetAnimClass.IsNull())
        {
                Paths.Add(Catalog->GenericRetargetAnimClass.ToSoftObjectPath());
        }
        if (Paths.IsEmpty())
        {
                UE_LOG(LogSirrohCharacterCustomization, Error, TEXT("Preset '%s' has no mesh."), *PresetId.ToString());
                ShowDefaultSourceVisual();
                return;
        }








        FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
        PendingLoadHandle = Streamable.RequestAsyncLoad(
                Paths,
                FStreamableDelegate::CreateUObject(this, &USirrohCharacterCustomizationComponent::HandlePresetAssetsLoaded, PresetId, Generation));
        if (!PendingLoadHandle.IsValid())
        {
                HandlePresetAssetsLoaded(PresetId, Generation);
        }
}








void USirrohCharacterCustomizationComponent::HandlePresetAssetsLoaded(const FName PresetId, const uint32 Generation)
{
        if (Generation != LoadGeneration || PresetId != BodyPresetId)
        {
                return;
        }








        const USirrohCharacterCustomizationCatalog* Catalog = USirrohCharacterCustomizationCatalog::Get();
        const FSirrohCharacterBodyPreset* Preset = Catalog ? Catalog->FindBodyPreset(PresetId) : nullptr;
        if (!Preset)
        {
                return;
        }








        USkeletalMesh* Mesh = Preset->Mesh.Get();
        UClass* GenericRetargetClass = nullptr;
        if (Preset->BindingMode == ESirrohCharacterPoseBinding::IKRetarget && Catalog)
        {
                GenericRetargetClass = Catalog->GenericRetargetAnimClass.Get();
        }
        ApplyResolvedPreset(*Preset, Mesh, GenericRetargetClass);
        PendingLoadHandle.Reset();
}








void USirrohCharacterCustomizationComponent::ApplyResolvedPreset(
        const FSirrohCharacterBodyPreset& Preset,
        USkeletalMesh* Mesh,
        UClass* GenericRetargetClass)
{
        EnsureRuntimeMeshes();
        if (!PoseBridge || !PoseBridge->GetVisualMesh() || !PoseBridge->GetSourceMesh() || !CharacterVisualMesh || !Mesh)
        {
                ShowDefaultSourceVisual();
                return;
        }








        ClearBodyPartVisuals();
        CharacterVisualMesh->SetLeaderPoseComponent(nullptr);
        CharacterVisualMesh->SetAnimInstanceClass(nullptr);
        CharacterVisualMesh->ComponentTags.Reset();
        CharacterVisualMesh->SetSkeletalMeshAsset(Mesh);








        if (Preset.BindingMode == ESirrohCharacterPoseBinding::SameSkeleton)
        {
                USkeletalMesh* SourceAsset = PoseBridge->GetSourceMesh()->GetSkeletalMeshAsset();
                if (!SourceAsset || SourceAsset->GetSkeleton() != Mesh->GetSkeleton())
                {
                        UE_LOG(LogSirrohCharacterCustomization, Error,
                                TEXT("Preset '%s' was configured as SameSkeleton, but '%s' does not share the source Skeleton asset. Falling back to Default."),
                                *Preset.Id.ToString(), *GetNameSafe(Mesh));
                        ShowDefaultSourceVisual();
                        return;
                }








                ConfigurePlacement(CharacterVisualMesh, Preset.VisualOffset);
                CharacterVisualMesh->SetLeaderPoseComponent(PoseBridge->GetVisualMesh(), true, true);
        }
        else if (Preset.BindingMode == ESirrohCharacterPoseBinding::IKRetarget)
        {
                if (!GenericRetargetClass)
                {
                        UE_LOG(LogSirrohCharacterCustomization, Error, TEXT("ABP_GenericRetarget failed to load for preset '%s'."), *Preset.Id.ToString());
                        ShowDefaultSourceVisual();
                        return;
                }








                ConfigurePlacement(RetargetSourceMesh, FTransform::Identity);
                CharacterVisualMesh->AttachToComponent(RetargetSourceMesh, FAttachmentTransformRules::KeepRelativeTransform);
                CharacterVisualMesh->SetRelativeTransform(Preset.VisualOffset);
                CharacterVisualMesh->ComponentTags.AddUnique(Preset.RetargeterTag);
                CharacterVisualMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
                CharacterVisualMesh->SetAnimInstanceClass(GenericRetargetClass);
                CharacterVisualMesh->AddTickPrerequisiteComponent(RetargetSourceMesh);
        }








        PoseBridge->GetVisualMesh()->SetHiddenInGame(true, false);
        PoseBridge->GetVisualMesh()->SetVisibility(false, false);
        CharacterVisualMesh->SetHiddenInGame(false, false);
        CharacterVisualMesh->SetVisibility(true, false);
        RefreshBodyParts();








        UE_LOG(LogSirrohCharacterCustomization, Display, TEXT("Applied body preset '%s' using mesh '%s'."),
                *Preset.Id.ToString(), *GetNameSafe(Mesh));
}








void USirrohCharacterCustomizationComponent::ShowDefaultSourceVisual()
{
        ClearBodyPartVisuals();
        InitializePosePipeline();
        if (!PoseBridge || !PoseBridge->GetVisualMesh())
        {
                return;
        }
        if (CharacterVisualMesh)
        {
                CharacterVisualMesh->SetHiddenInGame(true, false);
                CharacterVisualMesh->SetVisibility(false, false);
                CharacterVisualMesh->SetLeaderPoseComponent(nullptr);
                CharacterVisualMesh->SetAnimInstanceClass(nullptr);
        }
        if (RetargetSourceMesh)
        {
                RetargetSourceMesh->SetHiddenInGame(true, false);
                RetargetSourceMesh->SetVisibility(false, false);
        }
        PoseBridge->GetVisualMesh()->SetHiddenInGame(false, false);
        PoseBridge->GetVisualMesh()->SetVisibility(true, false);


}
