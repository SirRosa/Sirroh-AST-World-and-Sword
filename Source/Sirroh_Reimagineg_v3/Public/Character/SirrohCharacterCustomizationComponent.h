#pragma once








#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SirrohCharacterCustomizationComponent.generated.h"








class AActor;
class USkeletalMesh;
class USkeletalMeshComponent;
class USirrohPoseBridgeComponent;
struct FStreamableHandle;
class FLifetimeProperty;
struct FSirrohCharacterBodyPreset;

/** A named material section is one independently replaceable body part. */
USTRUCT(BlueprintType)
struct FSirrohBodyPartOverride
{
        GENERATED_BODY()

        UPROPERTY(EditAnywhere, BlueprintReadOnly)
        FName Part;

        UPROPERTY(EditAnywhere, BlueprintReadOnly)
        TSoftObjectPtr<USkeletalMesh> Mesh;

        UPROPERTY(EditAnywhere, BlueprintReadOnly)
        bool bHidden = false;
};








/**
 * Runtime owner of the selected character body.
 * Gameplay remains on SandboxCharacter_Mover_Ragdoll; this component changes presentation only.
 */
UCLASS(ClassGroup=(Character), meta=(BlueprintSpawnableComponent))
class SIRROH_REIMAGINEG_V3_API USirrohCharacterCustomizationComponent : public UActorComponent
{
        GENERATED_BODY()








public:
        USirrohCharacterCustomizationComponent();








        static USirrohCharacterCustomizationComponent* FindOrCreate(AActor* Owner);








        UFUNCTION(BlueprintCallable, Category="Character|Customization")
        bool ApplyBodyPreset(FName PresetId);

        /** Authority only. Null replacement restores the original named section.
         * Hidden removes only this part, not its child bones or the shared GASP physics bodies. */
        UFUNCTION(BlueprintCallable, Category="Character|Customization")
        bool SetBodyPart(FName Part, USkeletalMesh* Replacement, bool bHidden = false);

        UFUNCTION(BlueprintCallable, Category="Character|Customization")
        void ResetBodyParts();








        UFUNCTION(BlueprintPure, Category="Character|Customization")
        FName GetBodyPresetId() const { return BodyPresetId; }








        UFUNCTION(BlueprintPure, Category="Character|Customization")
        USkeletalMeshComponent* GetCharacterVisualMesh() const { return CharacterVisualMesh; }








        virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;








protected:
        virtual void BeginPlay() override;
        virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;








private:
        UPROPERTY(ReplicatedUsing=OnRep_BodyPresetId)
        FName BodyPresetId = TEXT("Default");

        UPROPERTY(ReplicatedUsing=OnRep_BodyParts)
        TArray<FSirrohBodyPartOverride> BodyParts;

        UPROPERTY(Transient)
        TArray<TObjectPtr<USkeletalMeshComponent>> PartVisuals;

        UFUNCTION()
        void OnRep_BodyParts();

        void RefreshBodyParts();
        void ClearBodyPartVisuals();








        UPROPERTY(Transient)
        TObjectPtr<USirrohPoseBridgeComponent> PoseBridge;








        UPROPERTY(Transient)
        TObjectPtr<USkeletalMeshComponent> CharacterVisualMesh;








        /** Hidden real skeletal component used as ABP_GenericRetarget's parent source. */
        UPROPERTY(Transient)
        TObjectPtr<USkeletalMeshComponent> RetargetSourceMesh;








        TSharedPtr<FStreamableHandle> PendingLoadHandle;
        uint32 LoadGeneration = 0;








        UFUNCTION()
        void OnRep_BodyPresetId();








        void InitializePosePipeline();
        void BeginApplyPreset(FName PresetId);
        void HandlePresetAssetsLoaded(FName PresetId, uint32 Generation);
        void ApplyResolvedPreset(const FSirrohCharacterBodyPreset& Preset, USkeletalMesh* Mesh, UClass* GenericRetargetClass);
        void ShowDefaultSourceVisual();
        void EnsureRuntimeMeshes();
        void ConfigureCommonVisual(USkeletalMeshComponent* MeshComponent);
        void ConfigurePlacement(USkeletalMeshComponent* MeshComponent, const FTransform& Offset) const;
};
