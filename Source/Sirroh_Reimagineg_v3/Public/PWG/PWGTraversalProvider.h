#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "PWGTraversalProvider.generated.h"

UINTERFACE(BlueprintType)
class SIRROH_REIMAGINEG_V3_API UPWGTraversalProvider : public UInterface
{
	GENERATED_BODY()
};

class SIRROH_REIMAGINEG_V3_API IPWGTraversalProvider
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="PWG|Traversal")
	bool GetPWGLedgeTransforms(FTransform& FrontLedge, FTransform& BackLedge) const;
};

