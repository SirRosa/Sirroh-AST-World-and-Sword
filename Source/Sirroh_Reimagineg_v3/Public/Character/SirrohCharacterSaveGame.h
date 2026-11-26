#pragma once


#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Character/SirrohCharacterProfile.h"
#include "SirrohCharacterSaveGame.generated.h"


UENUM(BlueprintType)
enum class ESirrohCharacterGender : uint8
{
        Male,
        Female
};


inline constexpr int32 SirrohSkinToneCount = 6;


inline FLinearColor GetSirrohSkinTone(const int32 Index)
{
        static const FColor SkinTones[SirrohSkinToneCount] =
        {
                FColor(246, 218, 194), // porcelain
                FColor(232, 185, 151), // fair
                FColor(205, 145, 103), // warm
                FColor(171, 113, 78),  // olive
                FColor(121, 75, 52),   // brown
                FColor(73, 43, 34)     // deep
        };
        return FLinearColor::FromSRGBColor(SkinTones[FMath::Clamp(Index, 0, SirrohSkinToneCount - 1)]);
}


USTRUCT(BlueprintType)
struct FSirrohBodyPartAppearance
{
        GENERATED_BODY()


        UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
        FName PartId;


        UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, meta = (ClampMin = "0.5", ClampMax = "1.5"))
        float Width = 1.0f;


        UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, meta = (ClampMin = "0.5", ClampMax = "1.5"))
        float Height = 1.0f;


        UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, meta = (ClampMin = "0.0", ClampMax = "1.0"))
        float Color = 0.08f;


        UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, meta = (ClampMin = "0", ClampMax = "5"))
        int32 SkinToneIndex = 1;


        UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, meta = (ClampMin = "0.5", ClampMax = "1.5"))
        float Brightness = 1.0f;
};


USTRUCT(BlueprintType)
struct FSirrohCharacterAppearance
{
        GENERATED_BODY()


        UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
        ESirrohCharacterGender Gender = ESirrohCharacterGender::Male;


        UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, meta = (ClampMin = "0"))
        int32 HeadIndex = 0;


        UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, meta = (ClampMin = "0.5", ClampMax = "1.5"))
        float GlobalWidth = 1.0f;


        UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, meta = (ClampMin = "0.5", ClampMax = "1.5"))
        float GlobalHeight = 1.0f;


        UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, meta = (ClampMin = "0.0", ClampMax = "1.0"))
        float GlobalColor = 0.08f;


        UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, meta = (ClampMin = "0", ClampMax = "5"))
        int32 GlobalSkinToneIndex = 1;


        UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, meta = (ClampMin = "0.5", ClampMax = "1.5"))
        float GlobalBrightness = 1.0f;


        UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
        TArray<FSirrohBodyPartAppearance> BodyParts;
};


UCLASS(BlueprintType)
class SIRROH_REIMAGINEG_V3_API USirrohCharacterSaveGame : public USaveGame
{
        GENERATED_BODY()


public:
        UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
        int32 ProfileVersion = 0;
        UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
        FSirrohCharacterProfile Profile;
        UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
        FString CharacterName;
        UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
        FGuid CharacterId;


        UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
        int32 OriginIndex = 0;


        UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
        int32 ClassIndex = 0;


        /**
         * Stable ID from USirrohCharacterCustomizationCatalog.
         * New character customization uses this as the appearance authority.
         */
        UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
        FName BodyPresetId = TEXT("Default");


        // Legacy Sprite2D-era data. Kept for backwards compatibility; no longer authoritative.
        UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
        FSirrohCharacterAppearance Appearance;
};
