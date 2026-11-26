#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Combat/CombatMotionTypes.h"
#include "Combat/CombatPresentationProfile.h"
#include "CombatMotionCatalog.generated.h"

UENUM(BlueprintType)
enum class ECombatCatalogOverride : uint8 { Inherit, Override, Disabled };

/** Animation metadata is retained with the reference; borrowing a clip never borrows damage. */
USTRUCT(BlueprintType)
struct FCombatCatalogVariant
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Id;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FCombatMotionSource Motion;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName LegacyReactionId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector Incoming = FVector::ForwardVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bStrong = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bTurn = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bPierce = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bCut = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1")) float ContactPoseTime = 0.2f;
};

USTRUCT(BlueprintType)
struct FCombatVariantSet
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Name;
    /** At most ten per field. Additional sets retain large existing libraries without truncation. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(TitleProperty="Id")) TArray<FCombatCatalogVariant> Variants;
};

USTRUCT(BlueprintType)
struct FCombatMotionSlot
{
    GENERATED_BODY()
    /** Top, TopRight, Right, LowRight, Bottom, LowLeft, Left, TopLeft, Mid, Front, Back; aim may use explicit sample names. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Direction = TEXT("Mid");
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Context = TEXT("InPlace");
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Region = TEXT("Any");
    UPROPERTY(EditAnywhere, BlueprintReadWrite) ECombatCatalogOverride Mode = ECombatCatalogOverride::Inherit;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(TitleProperty="Name")) TArray<FCombatVariantSet> Sets;
};

USTRUCT(BlueprintType)
struct FCombatAnimationReuse
{
    GENERATED_BODY()
    /** Category path in this effective profile, e.g. Offense.Light or Received.Block. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Category;
    /** None preserves the requested context. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Context;
};

USTRUCT(BlueprintType)
struct FCombatMotionCategory
{
    GENERATED_BODY()
    /** Override merges explicitly overridden slots, rather than deleting the inherited category. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) ECombatCatalogOverride Mode = ECombatCatalogOverride::Inherit;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bOverrideReuse = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Use Other Animations")) FCombatAnimationReuse UseOtherAnimations;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bOverrideLayering = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Use Layered Animation")) bool bUseLayeredAnimation = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(TitleProperty="Direction")) TArray<FCombatMotionSlot> Slots;
};

USTRUCT(BlueprintType)
struct FCombatNamedMotionCategory
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Name;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FCombatMotionCategory Data;
};

USTRUCT(BlueprintType)
struct FCombatOffenseLibrary
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FCombatMotionCategory Heavy;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FCombatMotionCategory Light;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FCombatMotionCategory Pierce;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FCombatMotionCategory Kick;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FCombatMotionCategory InAir;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FCombatMotionCategory OnGround;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(TitleProperty="Name")) TArray<FCombatNamedMotionCategory> Specials;
};

USTRUCT(BlueprintType)
struct FCombatReceivedLibrary
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FCombatMotionCategory HitReaction;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FCombatMotionCategory Block;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FCombatMotionCategory PerfectBlock;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FCombatMotionCategory Parry;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FCombatMotionCategory InAir;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FCombatMotionCategory OnGround;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FCombatMotionCategory Knockdown;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FCombatMotionCategory GetUp;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FCombatMotionCategory Death;
};

USTRUCT(BlueprintType)
struct FCombatLocomotionLibrary
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FCombatMotionCategory Base;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FCombatMotionCategory Walk;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FCombatMotionCategory Transition;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FCombatMotionCategory Chill;
};

USTRUCT(BlueprintType)
struct FCombatMovementLibrary
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FCombatMotionCategory Roll;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FCombatMotionCategory Dodge;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FCombatLocomotionLibrary Combat;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FCombatLocomotionLibrary OutOfCombat;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(TitleProperty="Name")) TArray<FCombatNamedMotionCategory> Additional;
};

USTRUCT(BlueprintType)
struct FCombatOtherLibrary
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FCombatMotionCategory Taunt;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FCombatMotionCategory Guarding;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(TitleProperty="Name")) TArray<FCombatNamedMotionCategory> Additional;
};

USTRUCT(BlueprintType)
struct FCombatCatalogEffects
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) ECombatCatalogOverride Mode = ECombatCatalogOverride::Inherit;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FCombatEffectProfile Weak;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FCombatEffectProfile Medium;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FCombatEffectProfile Strong;
};

/** Single authoring table. Profiles inherit fields and references, never duplicate animation assets. */
USTRUCT(BlueprintType)
struct FCombatMotionProfile : public FTableRowBase
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Profile") FName ParentProfile;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Profile") FName OtherGripProfile;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Profile") bool bSelectable = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Profile") int32 Revision = 1;
    /** Only these reflected fields replace inherited style settings. Unknown names fail validation. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Advanced") TArray<FName> OverrideStyleFields;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Advanced") FCombatStyleDefinition Style;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Look Aim") FCombatMotionCategory LookAim;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Guard Aim") FCombatMotionCategory GuardAim;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Offense") FCombatOffenseLibrary Offense;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Received") FCombatReceivedLibrary Received;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement") FCombatMovementLibrary Movement;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Other") FCombatOtherLibrary Other;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Effects") FCombatCatalogEffects Effects;
};

USTRUCT(BlueprintType)
struct FResolvedCombatMotionProfile
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FName ProfileId;
    UPROPERTY(BlueprintReadOnly) FCombatStyleDefinition Style;
    UPROPERTY(BlueprintReadOnly) FCombatCatalogEffects Effects;
    UPROPERTY(BlueprintReadOnly) TArray<FCombatNamedMotionCategory> Categories;
};

USTRUCT(BlueprintType)
struct FCombatCatalogChoice
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) bool bFound = false;
    UPROPERTY(BlueprintReadOnly) bool bUseLayeredAnimation = false;
    UPROPERTY(BlueprintReadOnly) FName Category;
    UPROPERTY(BlueprintReadOnly) FName Direction;
    UPROPERTY(BlueprintReadOnly) FName Context;
    UPROPERTY(BlueprintReadOnly) FCombatCatalogVariant Variant;
    UPROPERTY(BlueprintReadOnly) FString Reason;
};

USTRUCT(BlueprintType)
struct FCombatCatalogQuery
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Category;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Direction = TEXT("Mid");
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Context = TEXT("InPlace");
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Region = TEXT("Any");
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Seed = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Airborne = -1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Sprint = -1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Crouch = -1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Guard = -1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bRestrictVariants = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> AllowedVariants;
};

UCLASS()
class SIRROH_REIMAGINEG_V3_API UCombatMotionCatalog : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Combat|Motion Catalog")
    static bool ResolveProfile(UDataTable* Table, FName Profile, FResolvedCombatMotionProfile& Result, FString& Error);
    UFUNCTION(BlueprintCallable, Category="Combat|Motion Catalog")
    static bool ValidateCatalog(UDataTable* Table, TArray<FString>& Errors);
    /** Resolver does not change gameplay type. Seed must be chosen once on authority. */
    UFUNCTION(BlueprintCallable, Category="Combat|Motion Catalog")
    static FCombatCatalogChoice Choose(const FResolvedCombatMotionProfile& Profile, FName Category,
        FName Direction, FName Context, FName Region, int32 Seed);
    static TArray<FCombatMotionSource> RuntimeSources(const FResolvedCombatMotionProfile& Profile);
    UFUNCTION(BlueprintCallable, Category="Combat|Motion Catalog")
    static FCombatCatalogChoice ChooseForState(const FResolvedCombatMotionProfile& Profile, const FCombatCatalogQuery& Query);
    static TSharedPtr<const FResolvedCombatMotionProfile> GetProfile(const FCombatStyleDefinition& Style);
    static FName DirectionName(FVector2D Direction);
    static bool StyleForProfile(UDataTable* Table, FName Profile, FCombatStyleDefinition& Style, FString& Error);
    static bool StyleForGrip(const FCombatStyleDefinition& Current, bool bTwoHanded, FCombatStyleDefinition& Style, FString& Error);
    static bool IsEnabled();
};
