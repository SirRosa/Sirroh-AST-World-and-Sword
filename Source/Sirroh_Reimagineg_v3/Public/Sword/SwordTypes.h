#pragma once
#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "SwordTypes.generated.h"

UENUM(BlueprintType)
enum class ESwordCulture:uint8 { European,Greek,Slavic,Elven,Dwarven,Nordic,Magical,Cursed };
UENUM(BlueprintType)
enum class ESwordStyle:uint8 { Plain,Ornate,Worn,Witch,Sacred,Bandit,Royal,Demonic };

USTRUCT(BlueprintType)
struct FSwordPart
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadWrite,SaveGame) ESwordCulture Culture=ESwordCulture::European;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,SaveGame) ESwordStyle Style=ESwordStyle::Plain;
    int32 Index() const { return static_cast<int32>(Style)*8+static_cast<int32>(Culture); }
    static FSwordPart FromIndex(int32 Index)
    { FSwordPart P; Index=FMath::Clamp(Index,0,63); P.Culture=static_cast<ESwordCulture>(Index%8); P.Style=static_cast<ESwordStyle>(Index/8); return P; }
};

USTRUCT(BlueprintType)
struct FSwordInstance
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,SaveGame) int32 Version=1;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,SaveGame) FGuid InstanceId;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,SaveGame) float Durability=100;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,SaveGame) FSwordPart Blade;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,SaveGame) FSwordPart Guard;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,SaveGame) FSwordPart Hilt;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,SaveGame,meta=(ClampMin="0",ClampMax="2")) int32 BladeVariant=0;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,SaveGame) int32 Seed=0;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,SaveGame) FName CombinationRule;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,SaveGame,meta=(ClampMin="0.1")) float WeaponWeight=1;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,SaveGame,meta=(ClampMin="0.1")) float WeaponPower=1;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,SaveGame,meta=(ClampMin="0.1")) float WeaponAngle=1;
};

USTRUCT(BlueprintType)
struct FSwordWeightedPart
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FSwordPart Part;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin="0")) float Weight=1;
};

/** One row per spawning rule, not per possible sword. SurfaceStyle uses PWG indices. */
USTRUCT(BlueprintType)
struct FSwordSpawnRule : public FTableRowBase
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadWrite) bool bEnabled=true;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) int32 SurfaceStyle=0;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) ESwordCulture Culture=ESwordCulture::European;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) ESwordStyle Style=ESwordStyle::Plain;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin="0",ClampMax="1")) float SwordChancePerSmallRock=0.04f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin="0",ClampMax="1")) float ForeignPartChance=0.10f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TArray<FSwordWeightedPart> Blades;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TArray<FSwordWeightedPart> Guards;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TArray<FSwordWeightedPart> Hilts;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FName SpawnContext=TEXT("SmallRock");
};

/** Optional overrides. -1 is a wildcard; higher priority wins, ties use row-name ordering. */
USTRUCT(BlueprintType)
struct FSwordCombinationRule : public FTableRowBase
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadWrite) int32 Priority=0;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) int32 BladeIndex=-1;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) int32 GuardIndex=-1;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) int32 HiltIndex=-1;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) int32 BladeVariant=-1;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FName DisplayNameKey;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TMap<FName,float> Stats;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TArray<FName> EffectIds;
};

USTRUCT(BlueprintType)
struct FSwordSaveRecord
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadWrite,SaveGame) FSwordInstance Sword;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,SaveGame) FString SpawnId;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,SaveGame) FTransform Transform;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,SaveGame) float LaneY=0;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,SaveGame) bool bHeld=false;
};
