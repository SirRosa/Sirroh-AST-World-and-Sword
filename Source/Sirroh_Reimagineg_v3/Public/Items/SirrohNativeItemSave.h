#pragma once
#include "CoreMinimal.h"
#include "Sword/SwordTypes.h"
#include "SirrohNativeItemSave.generated.h"
USTRUCT()
struct FSirrohNativeItemSave
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) int32 Slot=INDEX_NONE;
    UPROPERTY(SaveGame) FString PersistentId;
    UPROPERTY(SaveGame) bool bSword=false;
    UPROPERTY(SaveGame) FSoftClassPath CarryClass;
    UPROPERTY(SaveGame) FSwordInstance Sword;
    UPROPERTY(SaveGame) float PixelScale=.3f;
    UPROPERTY(SaveGame) int32 AtlasFrame=32;
    UPROPERTY(SaveGame) float BerrySize=14.f;
    UPROPERTY(SaveGame) FVector Scale=FVector::OneVector;
};
