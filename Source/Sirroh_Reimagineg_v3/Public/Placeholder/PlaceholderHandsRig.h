#pragma once


#include "CoreMinimal.h"
#include "ControlRig.h"
#include "PlaceholderHandsRig.generated.h"


class USkeletalMeshComponent;


/** Native Control Rig. Its world-space pose is published to the 3D pose bridge after locomotion. */
UCLASS()
class SIRROH_REIMAGINEG_V3_API UPlaceholderHandsRig : public UControlRig
{
    GENERATED_BODY()
public:
    void Solve(USkeletalMeshComponent* Mesh, const FVector* Targets, const float* Weights,
        TMap<FName,FTransform>& OutPose);
    virtual bool Execute_Internal(const FName& EventName) override;
private:
    FVector Goals[2];
    FVector Poles[2];
    float Alphas[2] = {0,0};
};