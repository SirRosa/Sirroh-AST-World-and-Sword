#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "POIProp.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class UStaticMesh;
class UMaterialInterface;
class UTexture2D;

UENUM(BlueprintType)
enum class EPOIPropCollision : uint8 { Decoration, Solid, Physics };

/** Small reusable prop/light template. Visual geometry never defines its physics shape.
 * PNG cutouts can be supplied as a material on a plane mesh. No AST interaction adapter yet. */
UCLASS(Blueprintable)
class SIRROH_REIMAGINEG_V3_API APOIProp : public AActor
{
    GENERATED_BODY()
public:
    APOIProp();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="POI|Visual") TObjectPtr<UStaticMesh> VisualMesh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="POI|Visual") TObjectPtr<UMaterialInterface> VisualMaterial;
    /** Use M_POIImage on a plane; PNG alpha is supplied by the material. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="POI|Visual") TObjectPtr<UTexture2D> ImageTexture;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="POI|Visual") FTransform VisualTransform;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="POI|Collision") EPOIPropCollision CollisionMode=EPOIPropCollision::Solid;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="POI|Collision") FVector CollisionHalfExtent=FVector(50);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="POI|Collision", meta=(ClampMin="0.1")) float MassKg=10;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="POI|Light") bool bLightEnabled=false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="POI|Light") FLinearColor LightColor=FLinearColor(1,0.65f,0.3f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="POI|Light", meta=(ClampMin="0")) float LightIntensity=1000;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="POI|Light", meta=(ClampMin="1")) float LightRadius=600;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="POI|Light") FVector LightOffset=FVector(0,0,70);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="POI|Light") bool bCastLightShadows=false;
    UFUNCTION(BlueprintCallable, Category="POI") void SetLightEnabled(bool Enabled);
    virtual void OnConstruction(const FTransform& Transform) override;
protected:
    virtual void BeginPlay() override;
private:
    UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Body;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Visual;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> Light;
};
