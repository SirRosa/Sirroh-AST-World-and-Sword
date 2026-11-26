#pragma once
#include "CoreMinimal.h"
#include "POITerrain.generated.h"

UENUM(BlueprintType)
enum class EPOITerrainOperation : uint8 { Flatten, Offset };

/** Rectangular terrain stamp in PWG LOCAL coordinates (centimeters).
 * Stable ordering: priority ascending, then ID. Does not carve tunnels. */
USTRUCT(BlueprintType)
struct SIRROH_REIMAGINEG_V3_API FPOITerrainStamp
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Terrain") FName Id;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Terrain") FVector2D Center=FVector2D::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Terrain") FVector2D HalfExtent=FVector2D(400,400);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Terrain", meta=(ClampMin="1")) float BlendDistance=400;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Terrain") EPOITerrainOperation Operation=EPOITerrainOperation::Flatten;
    /** Absolute local Z for Flatten; height delta for Offset. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Terrain") float Height=0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Terrain") int32 Priority=0;
    static bool ValidateAndSort(TArray<FPOITerrainStamp>& Stamps,FString& Error);
    static float Apply(const TArray<FPOITerrainStamp>& Stamps,float X,float Y,float Base);
};
