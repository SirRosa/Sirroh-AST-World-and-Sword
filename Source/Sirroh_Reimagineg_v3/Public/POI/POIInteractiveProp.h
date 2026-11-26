#pragma once
#include "CoreMinimal.h"
#include "POI/POIProp.h"
#include "POI/POIStateComponent.h"
#include "POIInteractiveProp.generated.h"

UENUM(BlueprintType)
enum class EPOIInteractionKind : uint8 { Door, Lever, Container, ReleasedPhysics };

/** Native prototype interaction. Call Interact from your input/AST interaction adapter.
 * Does not replace existing AST Blueprints or bind a conflicting keyboard action. */
UCLASS(Blueprintable)
class SIRROH_REIMAGINEG_V3_API APOIInteractiveProp : public APOIProp
{
    GENERATED_BODY()
public:
    APOIInteractiveProp();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="POI") TObjectPtr<UPOIStateComponent> PersistentState;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="POI|Interaction") EPOIInteractionKind Kind=EPOIInteractionKind::Door;
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="POI|Interaction") TArray<TObjectPtr<APOIInteractiveProp>> Targets;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="POI|Interaction",meta=(ClampMin="1")) float InteractionDistance=250;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="POI|Interaction") FVector OpenOffset=FVector(0,0,250);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="POI|Loot") TArray<FName> InitialLoot;
    UFUNCTION(BlueprintCallable, Category="POI|Interaction") bool Interact(AActor* User);
    UFUNCTION(BlueprintCallable, Category="POI|Interaction") void SetActivated(bool Active);
    /** Removes a token only; the caller must integrate it with the inventory. */
    UFUNCTION(BlueprintCallable, Category="POI|Loot") bool TakeLoot(AActor* User,FName Item);
    UFUNCTION(BlueprintPure, Category="POI|Interaction") bool CanInteract(AActor* User) const;
protected:
    virtual void BeginPlay() override;
private:
    FTransform ClosedTransform;
    UFUNCTION() void ApplyState();
};
