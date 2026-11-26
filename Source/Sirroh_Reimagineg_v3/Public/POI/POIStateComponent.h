#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "POIStateComponent.generated.h"

class APOIGenerator;
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPOIStateRestored);

USTRUCT(BlueprintType)
struct SIRROH_REIMAGINEG_V3_API FPOIObjectState
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) FTransform LocalTransform;
    UPROPERTY(SaveGame) bool bHasTransform=false;
    UPROPERTY(SaveGame) bool bRemoved=false;
    UPROPERTY(SaveGame) bool bActive=false;
    UPROPERTY(SaveGame) bool bLootInitialized=false;
    UPROPERTY(SaveGame) TArray<FName> Loot;
    /** Only explicitly selected scalar actor properties; never object pointers. */
    UPROPERTY(SaveGame) TMap<FName,FString> Properties;
};

/** Add to a native or AST actor. LocalId must be unique inside its source map.
 * The generator namespaces it by module instance and owns saved state. */
UCLASS(ClassGroup=(POI),meta=(BlueprintSpawnableComponent))
class SIRROH_REIMAGINEG_V3_API UPOIStateComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UPOIStateComponent();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="POI|Persistence") FName LocalId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="POI|Persistence") bool bSaveTransform=true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="POI|Persistence") TArray<FName> SavedActorProperties;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="POI|State") bool bActive=false;
    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="POI|State") bool bLootInitialized=false;
    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="POI|State") TArray<FName> Loot;
    UPROPERTY(BlueprintAssignable, Category="POI|State") FPOIStateRestored OnStateRestored;
    FPOIObjectState Capture(const FTransform& ModuleTransform) const;
    void Restore(const FPOIObjectState& State,const FTransform& ModuleTransform);
    void BindToGenerator(APOIGenerator* Generator,const FString& Key);
protected:
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    TWeakObjectPtr<APOIGenerator> OwningGenerator;
    FString PersistentKey;
};
