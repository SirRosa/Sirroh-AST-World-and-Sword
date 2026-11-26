#pragma once
#include "CoreMinimal.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "PWGNavigationBounds.generated.h"
/** Runtime bounds around the integration spawn; uses generated terrain collision. */
UCLASS()
class SIRROH_REIMAGINEG_V3_API APWGNavigationBounds : public ANavMeshBoundsVolume
{
    GENERATED_BODY()
public:
    APWGNavigationBounds(const FObjectInitializer& ObjectInitializer);
};
