#pragma once
#include "CoreMinimal.h"
#include "SirrohCharacterProfile.generated.h"

UENUM(BlueprintType)
enum class ESirrohIrisColor : uint8 { Lime, Cyan };
UENUM(BlueprintType)
enum class ESirrohBodyType : uint8 { GrimV1, GrimV7, GrimV10, SimpleMan };

UENUM(BlueprintType)
enum class ESirrohSex : uint8 { Male, Female };

UENUM(BlueprintType)
enum class ESirrohHairStyle : uint8 { Straight, Wavy, Curly, Spiky };

USTRUCT(BlueprintType)
struct FSirrohCharacterProfile
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FString Name;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    ESirrohSex Sex = ESirrohSex::Male;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    ESirrohBodyType BodyType = ESirrohBodyType::SimpleMan;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    ESirrohIrisColor LeftEye = ESirrohIrisColor::Cyan;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    ESirrohIrisColor RightEye = ESirrohIrisColor::Lime;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    bool bHairEnabled = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    int32 HairSeed = 7163;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    float HairLength = 16.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    uint8 HairColor = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    uint8 HairHighlightColor = 2;

    // Zero preserves the global length of profiles saved before regional controls existed.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    float HairFrontLength = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    float HairSideLength = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    float HairBackLength = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    float HairTopLength = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    ESirrohHairStyle HairStyle = ESirrohHairStyle::Straight;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    float HairDensity = 1.f;

    bool IsValid() const
    {
        if (Name.Len() < 2 || Name.Len() > 24 || Name != Name.TrimStartAndEnd()) return false;
        bool bHasLetter = false;
        for (TCHAR C : Name)
        {
            bHasLetter |= FChar::IsAlpha(C);
            if (!FChar::IsAlpha(C) && C != TEXT(' ') && C != TEXT('-') && C != TEXT('\'')) return false;
        }
        for (float Length : {HairFrontLength, HairSideLength, HairBackLength, HairTopLength})
            if (!FMath::IsFinite(Length) || (Length != 0.f && (Length < 1.f || Length > 40.f))) return false;
        if (uint8(HairStyle) > 3 || !FMath::IsFinite(HairDensity) || HairDensity < .35f || HairDensity > 1.5f) return false;
        return bHasLetter && FMath::IsFinite(HairLength) && HairLength >= 4.f && HairLength <= 32.f && HairColor < 5 && HairHighlightColor < 5 && uint8(Sex) <= 1 && uint8(LeftEye) <= 1 && uint8(RightEye) <= 1 && uint8(BodyType) <= 3;
    }
};
