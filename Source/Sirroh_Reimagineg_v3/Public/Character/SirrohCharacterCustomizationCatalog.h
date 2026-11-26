#pragma once


#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/SkeletalMesh.h"
#include "SirrohCharacterCustomizationCatalog.generated.h"


UENUM(BlueprintType)
enum class ESirrohCharacterPoseBinding : uint8
{
        DefaultSource,
        SameSkeleton,
        IKRetarget
};


USTRUCT(BlueprintType)
struct SIRROH_REIMAGINEG_V3_API FSirrohCharacterBodyPreset
{
        GENERATED_BODY()


        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Character")
        FName Id = NAME_None;


        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Character")
        FString DisplayName;


        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Character")
        TSoftObjectPtr<USkeletalMesh> Mesh;


        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Character")
        ESirrohCharacterPoseBinding BindingMode = ESirrohCharacterPoseBinding::DefaultSource;


        /** Tag consumed by GASP's ABP_GenericRetarget to select the appropriate IK Retargeter. */
        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Character")
        FName RetargeterTag = NAME_None;


        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Character")
        FTransform VisualOffset = FTransform::Identity;
};


/**
 * Data-driven registry for character body presets.
 *
 * Version 1 is config-backed so the first creator requires no hand-authored editor asset.
 * The public customization API can later keep working if this data moves to a DataAsset.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Sirroh Character Customization"))
class SIRROH_REIMAGINEG_V3_API USirrohCharacterCustomizationCatalog : public UDeveloperSettings
{
        GENERATED_BODY()


public:
        UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Characters")
        TArray<FSirrohCharacterBodyPreset> BodyPresets;


        UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Characters")
        TSoftClassPtr<UAnimInstance> GenericRetargetAnimClass;


        const FSirrohCharacterBodyPreset* FindBodyPreset(FName Id) const;
        FName SanitizePresetId(FName Id) const;
        static const USirrohCharacterCustomizationCatalog* Get();
};