#pragma once


#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SirrohPoseBridgeComponent.generated.h"


class AActor;
class USkeletalMesh;
class USkeletalMeshComponent;
class UPoseableMeshComponent;


/**
 * Runtime bridge between the gameplay pose solvers and the visible 3D character.
 *
 * The regular USkeletalMeshComponent remains the animation/locomotion source. A lightweight
 * UPoseableMeshComponent mirrors that animated pose and receives the procedural hand/combat
 * world-space overrides. This removes Sprite2D from the pose pipeline while preserving the
 * existing locomotion AnimBP and socket/collision source.
 *
 * Later this component can be replaced by a post-process AnimBP / Control Rig sink without
 * changing the combat or inventory components: they only publish world-space bone overrides.
 */
UCLASS(ClassGroup=(Character), meta=(BlueprintSpawnableComponent))
class SIRROH_REIMAGINEG_V3_API USirrohPoseBridgeComponent : public UActorComponent
{
    GENERATED_BODY()


public:
    USirrohPoseBridgeComponent();


    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;


    /** Finds the actor's pose bridge or creates one at runtime. */
    static USirrohPoseBridgeComponent* FindOrCreate(AActor* Owner);


    /** Pose produced by the generic hand / inventory IK. Transforms are world-space. */
    void PublishHandPose(const TMap<FName, FTransform>& InPose);
    void ClearHandPose();


    /** Pose produced by combat. It has priority over the hand pose. Transforms are world-space. */
    void PublishCombatPose(const TMap<FName, FTransform>& InPose);
    void ClearCombatPose();


    /** Returns hand-pose transform only (or false when the hand solver does not override the bone). */
    bool TryGetHandBoneTransform(FName BoneName, FTransform& OutTransform) const;


    /** Returns final procedural transform (combat > hands), falling back to the animated source mesh. */
    bool TryGetFinalBoneTransform(FName BoneName, FTransform& OutTransform) const;
    FTransform GetFinalBoneTransform(FName BoneName) const;


    USkeletalMeshComponent* GetSourceMesh() const { return SourceMesh; }
    UPoseableMeshComponent* GetVisualMesh() const { return VisualMesh; }


    /** Create a visible poseable 3D mesh which mirrors the normal animated skeletal mesh. */
    UPROPERTY(EditAnywhere, Category="3D Presentation")
    bool bCreateRuntime3DVisual = true;


    /** Hide only the rendered source mesh. It keeps evaluating animation and collision. */
    UPROPERTY(EditAnywhere, Category="3D Presentation")
    bool bHideAnimatedSourceMesh = true;


    /** Copy material assignments from the animated source mesh to the runtime 3D visual. */
    UPROPERTY(EditAnywhere, Category="3D Presentation")
    bool bMirrorMaterials = true;


protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;


private:
    UPROPERTY(Transient)
    TObjectPtr<USkeletalMeshComponent> SourceMesh;


    UPROPERTY(Transient)
    TObjectPtr<UPoseableMeshComponent> VisualMesh;


    UPROPERTY(Transient)
    TObjectPtr<USkeletalMesh> CachedSkeletalMesh;


    TMap<FName, FTransform> HandPose;
    TMap<FName, FTransform> CombatPose;
    bool bReportedInvalidHandPose = false;
    bool bReportedInvalidCombatPose = false;
    bool bReportedInvalidVisualPose = false;
    bool bHasComposedNPCPose = false;


    void InitializeFromOwner();
    void EnsureVisualMesh();
    void SyncVisualPose();
    void SyncMaterials();
};
