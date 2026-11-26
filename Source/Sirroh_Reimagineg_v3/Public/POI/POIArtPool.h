#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "POI/POIKitBlock.h"
#include "POIArtPool.generated.h"

USTRUCT(BlueprintType)
struct FPOIImagePair
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<UTexture2D> Side;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<UTexture2D> Top;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FVector4 SideUV=FVector4(0,0,1,1);
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FVector4 TopUV=FVector4(0,0,1,1);
};

/** Side + top form a pair; top is also used on the underside. */
UCLASS(BlueprintType)
class SIRROH_REIMAGINEG_V3_API UPOIArtPool : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FPOIImagePair> Images;
    UFUNCTION(CallInEditor, BlueprintCallable) void BakeAlphaBounds();
#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
#endif
};

UCLASS(BlueprintType)
class SIRROH_REIMAGINEG_V3_API UPOIArtTheme : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UPOIArtPool> Backgrounds;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UPOIArtPool> Platforms;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<UMaterialInterface> Material;
    static UPOIArtTheme* ForLayer(int32 Layer);
    void Apply(FPOIKitSettings& Settings, FRandomStream& Random) const;
};
