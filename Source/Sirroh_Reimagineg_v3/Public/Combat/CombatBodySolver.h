#pragma once

#include "Combat/CombatMotionTypes.h"
#include "Combat/CombatMotionCatalog.h"
#include "UObject/StrongObjectPtr.h"

class USkeletalMeshComponent;
struct FCombatWeaponDefinition;

struct FCombatPoseHints
{
    TArray<FQuat> WindUp, Destination, Finish;
    float WindTime = -1, DestinationTime = -1, FinishTime = -1;
    float Weight = 0;
};

struct FCombatMotionFrame
{
    TArray<FQuat> LocalRotations;
    FVector Feet[2];
    FVector RootTranslation = FVector::ZeroVector;
    FTransform Grip;
    FVector PelvisTranslation = FVector::ZeroVector;
};

struct FCombatSampledMotion
{
    TArray<FCombatMotionFrame> Frames;
    TStrongObjectPtr<UAnimSequence> Sequence;
    TArray<int32> SourceBones;
    bool bSameSkeleton = false;
    bool bAdditive = false;
    float Length = 0;
    bool IsValid() const { return Frames.Num()>1 && Length>0; }
};

/** Cached rig binding and one sampled style. No weapon trajectory or render dependency. */
struct FCombatBodyRig
{
    TSharedPtr<const FResolvedCombatMotionProfile> Catalog;
    TArray<FName> Names;
    TArray<int32> Parents;
    TArray<FQuat> StyleLocalRotations;
    TArray<FTransform> ReferenceLocalPose;
    FCombatPoseHints Hints[3];
    TArray<FCombatSampledMotion> Motions;
    TArray<FCombatMotionSource> Sources;
    TArray<bool> StyleBones;
    TArray<int32> Spine;
    float StandingPelvisHeight = 90;
    int32 Pelvis = INDEX_NONE;
    int32 Clavicle = INDEX_NONE;
    int32 Shoulder = INDEX_NONE;
    int32 Elbow = INDEX_NONE;
    int32 Hand = INDEX_NONE;
    int32 Hips[2] = { INDEX_NONE, INDEX_NONE };
    int32 Knees[2] = { INDEX_NONE, INDEX_NONE };
    int32 Feet[2] = { INDEX_NONE, INDEX_NONE };
    int32 GripBone = INDEX_NONE;
    FTransform GripLocal;
    int32 SupportClavicle = INDEX_NONE;
    int32 SupportShoulder = INDEX_NONE;
    int32 SupportElbow = INDEX_NONE;
    int32 SupportHand = INDEX_NONE;
    int32 SupportGripBone = INDEX_NONE;
    FTransform SupportGripLocal;
    /** Calibrates bone axes to the actor's anatomical axes in the reference skeleton. */
    FQuat ClavicleAnatomicalFrame = FQuat::Identity;
};

/** Smoothing is local simulation state, not replicated bones. Reset on rig/equipment/teleport. */
struct FCombatBodySolverState
{
    double PelvisSpring = 0;
    double PelvisSpringVelocity = 0;
    double PelvisYaw = 0;
    double TorsoYaw = 0;
    double TorsoPitch = 0;
    double ClavicleYaw = 0;
    double ClaviclePitch = 0;
    double ShoulderYaw = 0;
    double ShoulderPitch = 0;
    FQuat WristCorrection = FQuat::Identity;
    TArray<FQuat> AttackStartRotations;
    FVector AttackStartPelvisLocal = FVector::ZeroVector;
    FVector MotionFit = FVector::ZeroVector;
    FVector GripElbows[2]={FVector::ZeroVector,FVector::ZeroVector};
    bool bHasGripElbows=false;
    FVector LoadResponse = FVector::ZeroVector;
};

struct FCombatBodySolverDiagnostics
{
    bool bDirectionClamped = false;
    bool bFootReachClamped = false;
    double PrimaryArmExtension = 0;
    double WorkingPointErrorCm = 0;
    double SecondaryArmExtension = 0;
    double GripClosureErrorCm = 0;
    double GripClosureErrorDegrees = 0;
    double GripCorrectionCm = 0;
    bool bPoseLimited = false;
};

namespace CombatBodySolver
{
    SIRROH_REIMAGINEG_V3_API bool UsesPhysicsAssetBiomechanics();
    bool BuildPhysicsTargets(const FCombatBodyRig& Rig,const TArray<FTransform>& Live,const FTransform& MeshWorld,const FTransform& Root,const FCombatIntent& Intent,const FCombatBodyDrive& Drive,float Alpha,FCombatBodySolverState& State,FCombatFinalPose& Out);
    SIRROH_REIMAGINEG_V3_API TArray<FCombatMotionSource> LibrarySources(const FCombatStyleDefinition& Style);
    SIRROH_REIMAGINEG_V3_API bool ConstrainAnatomy(const FCombatBodyRig& Rig, FCombatFinalPose& Pose);
    /** Couples the body-derived hand preferences. No cursor or physical weapon target is accepted.
     * Transactional: an infeasible closure leaves the supplied pose unchanged. */
    SIRROH_REIMAGINEG_V3_API bool CloseGripPair(const FCombatBodyRig& Rig,
        const FCombatStyleDefinition& Style, const FCombatWeaponDefinition& Weapon,
        FCombatFinalPose& Pose, FCombatBodySolverDiagnostics& Diagnostics, const FVector* PreviousElbows=nullptr);
    /** Accept continuous motion unchanged; backtrack discontinuities after all pose layers.
     * Failure holds the previous feasible local pose transported by the current actor root. */
    SIRROH_REIMAGINEG_V3_API bool ProjectContinuousPose(const FCombatBodyRig& Rig,
        const FCombatStyleDefinition& Style, const FCombatWeaponDefinition& Weapon,
        const FCombatFinalPose& Previous, const FTransform& RootWorld,
        FCombatFinalPose& Requested, FCombatBodySolverDiagnostics& Diagnostics);
    SIRROH_REIMAGINEG_V3_API bool SampleMotion(const FCombatBodyRig& Rig, int32 Index, float Time,
        TArray<FTransform>& Local, TArray<FTransform>& Component);
    /** Bounded whole-body search, scored on blade geometry. Never assigns a weapon or hand target. */
    SIRROH_REIMAGINEG_V3_API bool SolveMotion(const FCombatBodyRig& Rig,
        const TArray<FTransform>& LiveComponentPose, const FTransform& MeshWorld,
        const FTransform& ActualRootWorld, const FCombatStyleDefinition& Style,
        const FCombatWeaponDefinition& Weapon, const FCombatIntent& Intent, const FCombatBodyDrive& Drive,
        float BlendAlpha, float DeltaSeconds, FCombatBodySolverState& State,
        FCombatMotionPlan& OutPlan, FCombatFinalPose& OutPose, FCombatBodySolverDiagnostics& OutDiagnostics);
    SIRROH_REIMAGINEG_V3_API bool BindRig(USkeletalMeshComponent& Mesh,
        const FCombatStyleDefinition& Style, FCombatBodyRig& OutRig, FString& OutError);

    /** Phase-2 single-hand guard spike. Proximal rotations -> bounded arm FK -> grip.
     * LiveComponentPose is copied after skeletal animation, before inventory hand overrides.
     * Feet retain the animated targets where reachable. This never translates the actor. */
    SIRROH_REIMAGINEG_V3_API bool SolveGuard(const FCombatBodyRig& Rig,
        const TArray<FTransform>& LiveComponentPose, const FTransform& MeshWorld,
        const FTransform& ActualRootWorld, const FCombatStyleDefinition& Style,
        const FCombatIntent& Intent, const FCombatBodyDrive& Drive, float BlendAlpha, float DeltaSeconds,
        FCombatBodySolverState& State, FCombatMotionPlan& OutPlan,
        FCombatFinalPose& OutPose, FCombatBodySolverDiagnostics& OutDiagnostics);
}
