#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Character/SirrohCharacterProfile.h"
#include "SirrohCharacterIdentityComponent.generated.h"
class USkeletalMesh;
class UMaterialInterface;

/** Single appearance authority shared by creator, saved player and respawn. */
UCLASS(ClassGroup=(Character), meta=(BlueprintSpawnableComponent))
class SIRROH_REIMAGINEG_V3_API USirrohCharacterIdentityComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    USirrohCharacterIdentityComponent();
    static USirrohCharacterIdentityComponent* FindOrCreate(AActor* Owner);
    static bool ApplySaved(AActor* Owner);
    bool RefreshAppearance() { return Profile.IsValid() && ApplyVisual(); }
    bool ApplyProfile(const FSirrohCharacterProfile& InProfile);
    UFUNCTION(BlueprintPure, Category="Character")
    FSirrohCharacterProfile GetProfile() const { return Profile; }
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
private:
    UPROPERTY(ReplicatedUsing=OnRep_Profile)
    FSirrohCharacterProfile Profile;
    UPROPERTY()
    TObjectPtr<USkeletalMesh> CharacterMesh;
    UPROPERTY()
    TObjectPtr<UMaterialInterface> CharacterMaterial;
    UPROPERTY()
    TObjectPtr<USkeletalMesh> GrimV7Mesh;
    UPROPERTY()
    TObjectPtr<UMaterialInterface> GrimV7Material;
    UPROPERTY()
    TObjectPtr<USkeletalMesh> GrimV10Mesh;
    UPROPERTY()
    TObjectPtr<UMaterialInterface> GrimV10Material;
    UPROPERTY()
    TObjectPtr<USkeletalMesh> SimpleManMesh;
    UPROPERTY()
    TObjectPtr<UMaterialInterface> SimpleManMaterial;
    UFUNCTION()
    void OnRep_Profile();
    UPROPERTY()
    TObjectPtr<USkeletalMesh> GrimGirlMesh;
    UPROPERTY()
    TObjectPtr<UMaterialInterface> GrimGirlMaterial;
    bool ApplyVisual();
};
