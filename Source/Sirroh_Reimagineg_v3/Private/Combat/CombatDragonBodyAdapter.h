#pragma once
#include "DragonIKPhysicsComponent.h"
#include "Combat/CombatBodySolver.h"
#include "CombatDragonBodyAdapter.generated.h"

/** Isolated Chaos bodies: procedural pose drives Dragon handles; only contact feeds back. */
UCLASS(Transient)
class UCombatDragonBodyAdapter : public UDragonIKPhysicsComponent
{
    GENERATED_BODY()
public:
    UCombatDragonBodyAdapter();
    bool Initialize(USkeletalMeshComponent& Source, const FCombatFinalPose& Pose);
    void PrepareTargets(const FCombatFinalPose& Pose);
    UFUNCTION(BlueprintCallable, Category="Combat|Diagnostics", meta=(DevelopmentOnly))
    static FString DescribePhysicsAsset(USkeletalMesh* Mesh, UPhysicsAsset* Asset);
    void Hit(FName Bone, const FVector& Point, const FVector& Impulse, float Strength, double Now);
    void Solve(const FCombatBodyRig& Rig, FCombatFinalPose& Pose, double Now);
    bool DrivePhysicsAsset(const FCombatBodyRig& Rig,FCombatFinalPose& Pose,double Now);
    USkeletalMeshComponent* DebugPhysicsMesh() const { return Proxy; }
    void Stop();
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> Proxy;
    UPROPERTY(Transient) TObjectPtr<class UPhysicsControlComponent> NativeControls;
    TMap<FName,FName> NativeNames;
    double LastNativeTime=-1;

    struct FActiveBody { FName Bone; double Start=0; float Strength=0; };
    TArray<FActiveBody> Active;
    bool bMissingAssetLogged=false;
    TMap<FName,FTransform> Targets;
};
