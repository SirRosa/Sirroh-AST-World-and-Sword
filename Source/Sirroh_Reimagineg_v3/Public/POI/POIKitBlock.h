#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "POIKitBlock.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UChildActorComponent;
class UCapsuleComponent;
class UTexture2D;
class UMaterialInterface;

UENUM(BlueprintType)
enum class EPOIKitKind : uint8 { GASP, AST, Both, SideWall, Background };
UENUM(BlueprintType)
enum class EPOIKitCollision : uint8 { Solid, NoPhysicalCollision, OneWay };

UENUM(BlueprintType)
enum class EPOIASTLedge : uint8 { Ledge, Vault, Mantle };
UENUM(BlueprintType)
enum class EPOIASTHang : uint8 { Automatic, Braced, Free };

/** All dimensions in cm; actor origin is the CENTRE OF THE TOP surface. */
USTRUCT(BlueprintType)
struct SIRROH_REIMAGINEG_V3_API FPOIKitSettings
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Block") EPOIKitKind Kind=EPOIKitKind::GASP;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Block") EPOIKitCollision Collision=EPOIKitCollision::Solid;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Block") FVector Size=FVector(240,100,100);
    /** AST grip faces +X if true, -X otherwise. GASP exposes all four original ledges. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Block") bool bFacePositiveX=true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance") TSoftObjectPtr<UTexture2D> Image;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Climbing") bool bBothASTSides=false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Climbing") EPOIASTLedge ASTLedge=EPOIASTLedge::Ledge;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Climbing") EPOIASTHang ASTHang=EPOIASTHang::Automatic;
    /** Same image on the horizontal top and underside. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance") TSoftObjectPtr<UTexture2D> TopImage;
    /** Stretch the non-transparent bounds to the full face, retaining holes within the artwork. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance") bool bFitImageToAlpha=true;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Appearance") FVector4 ImageUVRect=FVector4(0,0,1,1);
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Appearance") FVector4 TopImageUVRect=FVector4(0,0,1,1);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance") TSoftObjectPtr<UMaterialInterface> Material;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance") FLinearColor Tint=FLinearColor::White;
};

UCLASS(Blueprintable)
class SIRROH_REIMAGINEG_V3_API APOIKitBlock : public AActor
{
    GENERATED_BODY()
public:
    APOIKitBlock();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="POI Kit") FPOIKitSettings Settings;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="POI Kit") FString Status;
    UFUNCTION(CallInEditor, BlueprintCallable, Category="POI Kit") void RefreshBlock();
    UFUNCTION(BlueprintPure, Category="POI Kit") static FVector4 GetImageAlphaBounds(UTexture2D* Texture);
    UFUNCTION(BlueprintPure, Category="POI Kit") UBoxComponent* GetPhysicalBody() const { return Body; }
    UFUNCTION(BlueprintPure, Category="POI Kit") AActor* GetGASPActor() const;
    UFUNCTION(BlueprintPure, Category="POI Kit") AActor* GetASTActor() const;
    void RefreshAppearance();
    bool BeginDropThrough(UCapsuleComponent* Capsule);
    virtual void OnConstruction(const FTransform& Transform) override;
    static bool ShouldPassFromBelow(float CapsuleBottom,float PlatformTop,bool AlreadyIgnoring);
protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Body;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Proximity;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Visual;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> TopVisual;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> BottomVisual;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UChildActorComponent> GASP;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UChildActorComponent> AST;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UChildActorComponent> ASTOpposite;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UChildActorComponent> ASTFront;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UChildActorComponent> ASTBack;
    UPROPERTY() TSoftClassPtr<AActor> GASPClass;
    UPROPERTY() TSoftClassPtr<AActor> ASTClass;
    TMap<TWeakObjectPtr<UCapsuleComponent>,bool> OriginalIgnores;
    TMap<TWeakObjectPtr<UCapsuleComponent>,double> DropThroughUntil;
    void UpdateCapsule(UCapsuleComponent* Capsule);
    void RestoreCapsule(UCapsuleComponent* Capsule,bool Original);
    UFUNCTION() void Enter(UPrimitiveComponent* Overlapped,AActor* Other,UPrimitiveComponent* OtherComp,int32 BodyIndex,bool Sweep,const FHitResult& Hit);
    UFUNCTION() void Leave(UPrimitiveComponent* Overlapped,AActor* Other,UPrimitiveComponent* OtherComp,int32 BodyIndex);
};
