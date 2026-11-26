#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameFramework/Actor.h"
#include "Kismet/KismetSystemLibrary.h"
#include "POICorridorTools.generated.h"
class UBlueprint;
class UStaticMesh;

UCLASS()
class SIRROH_REIMAGINEG_V3_API UPOICorridorTools : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintPure) static FText SafeHUDHealth(class UUserWidget* Widget);
    UFUNCTION(BlueprintCallable) static bool PatchWeaponHUD();
    static AActor* GetHeldPushable(AActor* Character);
    static bool ReleaseHeldPushable(AActor* Character);
    static void ConfigurePushPlane(AActor* Object);
    UFUNCTION(BlueprintPure) static bool ShouldReleasePhysicalPush(AActor* Object);
    static bool ApplyPropStrike(UPrimitiveComponent* Body,FVector ContactVelocity,float WeaponMass);
    UFUNCTION(BlueprintCallable, Category="POI|AST") static void BeginPhysicalPush(AActor* Object);
    UFUNCTION(BlueprintCallable, Category="POI|AST") static void PreparePhysicalPush(AActor* Object);
    UFUNCTION(BlueprintCallable, Category="POI|Editor")
    static UBlueprint* CreatePushableVariant(const FString& PackagePath,UStaticMesh* Mesh);
    UFUNCTION(BlueprintCallable, Category="POI|Editor") static FVector CenterPushMesh(UStaticMesh* Mesh);
    UFUNCTION(BlueprintCallable, Category="POI|Editor") static bool PatchASTPushGeometry();
    UFUNCTION(BlueprintPure, Category="POI|AST") static FVector PushSupportEnd(AActor* Object,FVector Start);
    UFUNCTION(BlueprintPure, Category="POI|AST") static FVector PushTraceExtent(AActor* Object);
    UFUNCTION(BlueprintPure, Category="POI|AST") static FVector PushProxyExtent(AActor* Object);
    UFUNCTION(BlueprintPure, Category="POI|AST") static FVector PushProxyStart(AActor* Object);
    UFUNCTION(BlueprintCallable, Category="POI|AST",meta=(WorldContext="WorldContextObject",AutoCreateRefTerm="ActorsToIgnore"))
    static bool PushFloorTrace(const UObject* WorldContextObject,FVector Start,FVector End,FVector HalfSize,FRotator Orientation,ETraceTypeQuery TraceChannel,bool bTraceComplex,const TArray<AActor*>& ActorsToIgnore,EDrawDebugTrace::Type DrawDebugType,FHitResult& OutHit,bool bIgnoreSelf,FLinearColor TraceColor,FLinearColor TraceHitColor,float DrawTime);
};

UCLASS(Blueprintable)
class SIRROH_REIMAGINEG_V3_API APOITorch : public AActor
{
    GENERATED_BODY()
public:
    APOITorch();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<class UStaticMeshComponent> Mesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<class UPointLightComponent> Light;
};
