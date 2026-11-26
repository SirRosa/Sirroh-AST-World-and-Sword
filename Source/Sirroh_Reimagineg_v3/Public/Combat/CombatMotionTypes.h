#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Combat/CombatAttackTypes.h"
#include "CombatMotionTypes.generated.h"

class UAnimSequence;
class UChooserTable;

// V3 contracts deliberately do not reuse legacy Blocking or weapon trajectory state.
UENUM(BlueprintType)
enum class ECombatMotionPhase : uint8 { Inactive, Combat, Aiming, Attacking, Recovery, Staggered, Disarmed };

UENUM(BlueprintType)
enum class ECombatHand : uint8 { Right, Left };

UENUM(BlueprintType)
enum class ECombatSupportFoot : uint8 { None, Left, Right, Both };

USTRUCT()
struct FCombatGripMode
{
    GENERATED_BODY()
    UPROPERTY() bool bTwoHanded = false;
    UPROPERTY() uint32 Serial = 0;
    UPROPERTY() double ChangedAt = 0;
};

/** All positions/directions are world-space; target depth is never removed. */
USTRUCT(BlueprintType)
struct FCombatIntent
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector AttackTarget = FVector::ZeroVector;
    /** Continuous outward defended sector, never quantized to Up/Front/Low. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector GuardDirectionWorld = FVector::ForwardVector;
    /** Optional incoming travel toward the defender; distinct from outward guard intent. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector IncomingTravelDirectionWorld = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bHasIncomingTravelDirection = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EDirectionalAttack AttackType = EDirectionalAttack::Light;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) ECombatMotionPhase Phase = ECombatMotionPhase::Inactive;
    UPROPERTY() FGuid WeaponInstanceId;
    UPROPERTY() FName StyleId;
    UPROPERTY() uint32 AttackId = 0;
    UPROPERTY() double SimulationTime = 0;
};

/** Degrees and normalized limb lengths; these are budgets, not stretch permissions. */
USTRUCT(BlueprintType)
struct FCombatAnatomicalBudget
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float PelvisYawDegrees = 30;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float TorsoYawDegrees = 45;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float TorsoLeanDegrees = 20;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float ClavicleDegrees = 20;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float ShoulderYawDegrees = 75;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float ShoulderPitchDegrees = 80;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float WristTwistDegrees = 45;
    /** Attack-specific swing budget; guard retains its narrower interception correction. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="90")) float AttackWristSwingDegrees = 90;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="45")) float WristSwingDegrees = 25;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.05", ClampMax="0.98")) float MaximumArmExtension = 0.95f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.01", ClampMax="0.9")) float MinimumArmExtension = 0.15f;
};

USTRUCT(BlueprintType)
struct FCombatAttackMovementTuning
{
    GENERATED_BODY()
    /** Centimetres. Zero selects the family default (light 90, heavy 200, thrust 170). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) float MaximumDistance = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) float StepDriveThreshold = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) float LungeDriveThreshold = 3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.01")) float DurationSeconds = 0.3f;
};

/** Optional sampled guidance. The animation is never played as an attack montage. */
USTRUCT(BlueprintType)
struct FCombatAnimationHints
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<UAnimSequence> Animation;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="0.5")) float PoseWeight = 0.3f;
};

/** A motion archetype, not a direction slot. Times are normalized clip positions. */
USTRUCT(BlueprintType)
struct FCombatMotionSource : public FTableRowBase
{
    GENERATED_BODY()
    /** Filled by the catalog adapter; stable across row ordering. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName CatalogVariantId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName CatalogCategory;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<UAnimSequence> Animation;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bEnabled = true;
    /** 0=shared generic action, 1=one hand, 2=two hands. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Hands = 2;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName WeaponFamily;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Action = TEXT("Attack");
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bMoving = false;
    /** YZ swing direction or normalized yaw/pitch coordinates for aim samples. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector2D Direction = FVector2D::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Intensity = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Speed = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Height = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Variant = 0;
    /** -1=any, 0=forbidden, 1=required. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Airborne = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Sprint = -1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Crouch = -1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Guard = -1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Priority = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName FallbackRow;
    /** Normalized activation intervals; one interval per physical strike. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FVector2D> StrikeWindows;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FVector2D> StrikeDirections;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bCuts = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bThrusts = false;
    /** Family whose neutral trajectory is authored in this clip. Other families adapt its shape. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EDirectionalAttack NativeFamily = EDirectionalAttack::Light;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1")) float LoadTime = 0.2f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1")) float ContactTime = 0.55f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1")) float FinishTime = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1")) float PoseWeight = 1.0f;
    /** Actor-facing coordinates relative to the pelvis at attack acceptance. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector NeutralTarget = FVector(100,0,0);
    /** Bounds residual adaptation of the source, not anatomical limits. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="90")) float AdaptationDegrees = 55;
};

/** Chooser input (one struct context). Result type: Animation Sequence from MotionSources. */
USTRUCT(BlueprintType)
struct FCombatMotionSelection
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName StyleId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EDirectionalAttack AttackType = EDirectionalAttack::Light;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 ComboIndex = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bChained = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float TargetDistance = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float WeaponWeight = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector BladeDirection = FVector::ForwardVector;
};

USTRUCT(BlueprintType)
struct FCombatDebugStages
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, Transient, meta=(ClampMin="0",ClampMax="1")) float MotionFit=1;
    UPROPERTY(EditAnywhere, Transient, meta=(ClampMin="0",ClampMax="1")) float BasePose=1;
    UPROPERTY(EditAnywhere, Transient, meta=(ClampMin="0",ClampMax="1")) float AuthoredPose=1;
    UPROPERTY(EditAnywhere, Transient, meta=(ClampMin="0",ClampMax="1")) float AnimationHints=1;
    UPROPERTY(EditAnywhere, Transient, meta=(ClampMin="0",ClampMax="1")) float PelvisYaw=1;
    UPROPERTY(EditAnywhere, Transient, meta=(ClampMin="0",ClampMax="1")) float Torso=1;
    UPROPERTY(EditAnywhere, Transient, meta=(ClampMin="0",ClampMax="1")) float Clavicles=1;
    UPROPERTY(EditAnywhere, Transient, meta=(ClampMin="0",ClampMax="1")) float ShoulderAim=1;
    UPROPERTY(EditAnywhere, Transient, meta=(ClampMin="0",ClampMax="1")) float ElbowFlex=1;
    UPROPERTY(EditAnywhere, Transient, meta=(ClampMin="0",ClampMax="1")) float ChestClamp=1;
    UPROPERTY(EditAnywhere, Transient, meta=(ClampMin="0",ClampMax="1")) float Wrist=1;
    UPROPERTY(EditAnywhere, Transient, meta=(ClampMin="0",ClampMax="1")) float CoreLimits=1;
    UPROPERTY(EditAnywhere, Transient, meta=(ClampMin="0",ClampMax="1")) float StandingRecovery=1;
    UPROPERTY(EditAnywhere, Transient, meta=(ClampMin="0",ClampMax="1")) float FootRestore=1;
    UPROPERTY(EditAnywhere, Transient, meta=(ClampMin="0",ClampMax="1")) float EntryPoseBlend=1;
    UPROPERTY(EditAnywhere, Transient, meta=(ClampMin="0",ClampMax="1")) float GuardLibrary=1;
    UPROPERTY(EditAnywhere, Transient, meta=(ClampMin="0",ClampMax="1")) float ActionLibrary=1;
    UPROPERTY(EditAnywhere, Transient, meta=(ClampMin="0",ClampMax="1")) float Guidance=1;
    UPROPERTY(EditAnywhere, Transient, meta=(ClampMin="0",ClampMax="1")) float Inertia=1;
    UPROPERTY(EditAnywhere, Transient, meta=(ClampMin="0",ClampMax="1")) float HitAnimation=1;
    UPROPERTY(EditAnywhere, Transient, meta=(ClampMin="0",ClampMax="1")) float ContinuityFallback=1;
    UPROPERTY(EditAnywhere, Transient, meta=(ClampMin="0",ClampMax="1")) float GripPair=1;
    UPROPERTY(EditAnywhere, Transient, meta=(ClampMin="0",ClampMax="1")) float GripTransition=1;
    UPROPERTY(EditAnywhere, Transient, meta=(ClampMin="0",ClampMax="1")) float AnatomyPass=1;
    UPROPERTY(EditAnywhere, Transient, meta=(ClampMin="0",ClampMax="1")) float WindUp=1;
    UPROPERTY(EditAnywhere, Transient, meta=(ClampMin="0",ClampMax="1")) float Light=1;
    UPROPERTY(EditAnywhere, Transient, meta=(ClampMin="0",ClampMax="1")) float Heavy=1;
    UPROPERTY(EditAnywhere, Transient, meta=(ClampMin="0",ClampMax="1")) float Thrust=1;
};

/** One base pose plus optional motion archetypes; continuous direction belongs to the planner. */
USTRUCT(BlueprintType)
struct FCombatStyleDefinition : public FTableRowBase
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, Transient, Category="Debug") FCombatDebugStages DebugStages;
    /** Preserve authored attacks: fade competing pose/aim corrections, retain grip and anatomy limits.
     * Disable to compare the legacy procedural adaptation without changing individual debug stages. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Motion")
    bool bPreserveAuthoredAttackPose = true;

    /** None selects the legacy source table; otherwise MotionLibrary contains profiles. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName CatalogProfile;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<UDataTable> MotionLibrary;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName MotionFamily;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Version = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName StyleId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FText DisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<UAnimSequence> BasePose;
    /** Socket or bone in the dominant hand subtree (including auxiliary weapon bones). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Grip Type - Right Socket")) FName PrimaryGripSocket = TEXT("Weapon_R");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Grip Type - Left Socket")) FName SecondaryGripSocket = TEXT("Weapon_L");
    /** Used only when PrimaryGripSocket is None; explicit rig calibration, not a weapon target. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FTransform PrimaryGripInHand;
    /** Used when SecondaryGripSocket is None. Calibrated on the supporting hand, not on the weapon. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FTransform SecondaryGripInHand;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.1", ClampMax="40")) float GuardResponseSpeed = 8;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) float BasePoseTime = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) ECombatHand DominantHand = ECombatHand::Right;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bTwoHanded = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) ECombatSupportFoot PreferredSupportFoot = ECombatSupportFoot::Left;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FCombatAnatomicalBudget Anatomy;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float PelvisParticipation = 0.3f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float TorsoParticipation = 0.45f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float ShoulderParticipation = 0.25f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float CounterLoadDegrees = 75;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MinimumPreparationSeconds = 0.24f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="60")) float ChargedExtraLoadDegrees = 45;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.1", ClampMax="5")) float MaximumChargeSeconds = 1.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="8", ClampMax="80")) float AttackResponseSpeed = 40;
    /** Base on BasePose selects the master pose; missing markers preserve procedural defaults. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bReadAnimationHints = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FCombatAnimationHints LightHints;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FCombatAnimationHints HeavyHints;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FCombatAnimationHints PierceHints;
    /** Ordered combo library. Runtime accepts at most ten validated sources. Empty preserves procedural fallback. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FCombatMotionSource> MotionSources;
    /** Separate thrust library; entries are treated as native Pierce sources. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FCombatMotionSource> ThrustMotionSources;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<UChooserTable> MotionChooser;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float ChargeStartSeconds = 0.4f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float WeaponWeightParticipation = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="80")) float MaximumInertiaPullCm = 35;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.1", ClampMax="3")) float FollowThroughDamping = 1.5f;
    /** Soft body-planner objective only; cannot override anatomical limits or contact. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1")) float GuardInterceptionWeight = 0.75f;
    /** Pole offset in the anatomical body frame; mirrored by the selected arm. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector ElbowPoleOffset = FVector(0, 30, -25);
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float SupportPlantAlpha = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float PlantReleaseDistance = 45;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bAllowAirborneLunge = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bEnableAttackApproach = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FCombatAttackMovementTuning LightMovement;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FCombatAttackMovementTuning HeavyMovement;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FCombatAttackMovementTuning PierceMovement;
};

/** One bounded request per attack; a Mover adapter must consume it, never SetActorLocation. */
USTRUCT()
struct FCombatAttackMovementRequest
{
    GENERATED_BODY()
    UPROPERTY() uint32 AttackId = 0;
    UPROPERTY() double StartTime = 0;
    UPROPERTY() FVector GroundDisplacement = FVector::ZeroVector;
    UPROPERTY() float MaximumDistance = 0;
    UPROPERTY() float DurationSeconds = 0;
    UPROPERTY() bool bAllowAirborne = false;
};

USTRUCT()
struct FCombatFootPlan
{
    GENERATED_BODY()
    UPROPERTY() ECombatSupportFoot SupportFoot = ECombatSupportFoot::Both;
    UPROPERTY() ECombatSupportFoot LeadFoot = ECombatSupportFoot::None;
    UPROPERTY() float PlantAlphaLeft = 0;
    UPROPERTY() float PlantAlphaRight = 0;
};

/** Body deltas and soft grip goals are component-space, relative to actual Mover root. */
USTRUCT()
struct FCombatMotionPlan
{
    GENERATED_BODY()
    UPROPERTY() FCombatIntent Intent;
    UPROPERTY() FVector StrikeDirection = FVector::ForwardVector;
    UPROPERTY() FVector CounterLoadDirection = -FVector::ForwardVector;
    /** World-space soft defensive objectives. Body solver may relax these; never drive weapon directly. */
    UPROPERTY() FVector GuardCoveragePointWorld = FVector::ZeroVector;
    UPROPERTY() FVector PreferredBladeAxisWorld = FVector::ZeroVector;
    UPROPERTY() float GuardInterceptionWeight = 0;
    UPROPERTY() float AttackPhaseAlpha = 0;
    UPROPERTY() FCombatAttackMovementRequest Movement;
    UPROPERTY() FQuat RootFacingIntent = FQuat::Identity;
    UPROPERTY() FCombatFootPlan Feet;
    UPROPERTY() FTransform PelvisDelta;
    UPROPERTY() FRotator TorsoRotation = FRotator::ZeroRotator;
    UPROPERTY() FRotator ClavicleRotation = FRotator::ZeroRotator;
    UPROPERTY() float ShoulderParticipation = 0;
    UPROPERTY() FTransform PrimaryGripGoal;
    UPROPERTY() FTransform SecondaryGripGoal;
    UPROPERTY() FCombatAnatomicalBudget Anatomy;
};

/** Local-only solved output. Never replicate this map bone-by-bone. */
USTRUCT()
struct FCombatFinalPose
{
    GENERATED_BODY()
    UPROPERTY() double SimulationTime = 0;
    UPROPERTY() uint32 SimulationStep = 0;
    UPROPERTY() uint32 Epoch = 0;
    UPROPERTY() FTransform ActualRootWorld;
    UPROPERTY() TMap<FName, FTransform> BonesWorld;
    UPROPERTY() FTransform PrimaryGripWorld;
    UPROPERTY() FTransform SecondaryGripWorld;
    UPROPERTY() bool bHasPrimaryGrip = false;
    UPROPERTY() bool bHasSecondaryGrip = false;
};

/** Measured after the weapon adapter/physics step, never an analytic attack trajectory. */
USTRUCT()
struct FCombatWeaponSample
{
    GENERATED_BODY()
    UPROPERTY() double SimulationTime = 0;
    UPROPERTY() uint32 SimulationStep = 0;
    UPROPERTY() uint32 Epoch = 0;
    UPROPERTY() uint32 AttackId = 0;
    UPROPERTY() FGuid WeaponInstanceId;
    UPROPERTY() FTransform WeaponWorld;
    UPROPERTY() FTransform PrimaryGripWorld;
    UPROPERTY() FVector BladeBase = FVector::ZeroVector;
    UPROPERTY() FVector BladeTip = FVector::ZeroVector;
    UPROPERTY() float BladeRadius = 0;
    UPROPERTY() FVector LinearVelocity = FVector::ZeroVector;
    UPROPERTY() FVector AngularVelocity = FVector::ZeroVector;
    UPROPERTY() int32 StrikePhase = 0;
    UPROPERTY() EDirectionalAttack AttackType=EDirectionalAttack::Light;
    UPROPERTY() bool bAttackActive = false;
    UPROPERTY() bool bContactEnabled = false;
    UPROPERTY() bool bDiscontinuity = true;
};

/** Accepted attack intent. No hand, weapon trajectory or bone transforms cross the network. */
USTRUCT()
struct FCombatBodyAttack
{
    GENERATED_BODY()
    UPROPERTY() FName MotionVariantId;
    UPROPERTY() bool bLayeredAnimation = false;
    UPROPERTY() uint32 Id = 0;
    UPROPERTY() FGuid WeaponId;
    UPROPERTY() EDirectionalAttack Type = EDirectionalAttack::Light;
    UPROPERTY() bool bPrepared = false;
    UPROPERTY() bool bReleased = false;
    UPROPERTY() double PreparedAt = 0;
    UPROPERTY() double ReleasedAt = 0;
    UPROPERTY() double StrikeAt = 0;
    /** Authority sets this only after measured weapon motion settles. -1 means still committed. */
    UPROPERTY() double ReturnAt = -1;
    UPROPERTY() bool bChained = false;
    UPROPERTY() int32 MotionSourceIndex = INDEX_NONE;
    UPROPERTY() int32 ComboIndex = 0;
    UPROPERTY() float SourceEntryTime = 0;
    UPROPERTY() float SourceStrikeTime = 0;
    UPROPERTY() FVector NeutralTargetWorld = FVector::ZeroVector;
    UPROPERTY() FVector StartOrigin = FVector::ZeroVector;
    UPROPERTY() FVector StartWorkingPoint = FVector::ZeroVector;
    UPROPERTY() FVector StartWorkingVelocity = FVector::ZeroVector;
    UPROPERTY() FQuat PlanningFrame = FQuat::Identity;
    UPROPERTY() FVector StartArmDirection = FVector::ForwardVector;
    UPROPERTY() float StartExtension = 0.6f;
    UPROPERTY() FVector ApproachDisplacement = FVector::ZeroVector;
    UPROPERTY() FVector ApproachOrigin = FVector::ZeroVector;
    UPROPERTY() float ApproachSeconds = 0.3f;
    UPROPERTY() float Preparation = 0.24f;
    UPROPERTY() float Duration = 0.3f;
    UPROPERTY() float Recovery = 0.3f;
    UPROPERTY() FVector Target = FVector::ZeroVector;
    UPROPERTY() FVector Direction = FVector::ForwardVector;
};

/** Observation only; neither a hit flag nor an alternative contact authority. */
USTRUCT(BlueprintType)
struct FCombatAttackQuality
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) bool bValid = false;
    UPROPERTY(BlueprintReadOnly) bool bApproximateGeometry = true;
    UPROPERTY(BlueprintReadOnly) int32 AttackId = 0;
    UPROPERTY(BlueprintReadOnly) float WorkingRegionTargetErrorCm = 0;
    UPROPERTY(BlueprintReadOnly) float WorkingPointPathErrorCm = 0;
    UPROPERTY(BlueprintReadOnly) float WorkingPointSpeedCmPerSecond = 0;
    UPROPERTY(BlueprintReadOnly) float MovementFulfillment = 1;
};

/** Procedural anatomical demand, never a weapon/hand transform. */
struct FCombatBodyDrive
{
    int32 MotionSourceIndex = INDEX_NONE;
    float MotionTime = 0;
    float SupportMotionTime = 0;
    float MotionWeight = 0;
    float LegMotionWeight = 0;
    float MotionAdaptationDegrees = 0;
    float AdaptationWeight = 1;
    TArray<FTransform> AuthoredLocalPose;
    TArray<FTransform> AuthoredComponentPose;
    FVector AuthoredWorkingVelocity = FVector::ZeroVector;
    FVector AuthoredWorkingAcceleration = FVector::ZeroVector;
    float AuthoredFootLift[2] = { 0, 0 };
    FVector WorkingPointGoal = FVector::ZeroVector;
    FVector WorkingTravelDirection = FVector::ForwardVector;
    FVector LoadPoint = FVector::ZeroVector;
    FRotator TrajectoryCorrection = FRotator::ZeroRotator;
    bool bTrajectoryPlan = false;
    float WorkingPointWeight = 0;
    float MovementFulfillment = 1;
    FVector AuthoredPelvisOffset = FVector::ZeroVector;
    FVector Direction = FVector::ForwardVector;
    FVector StrikeDirection = FVector::ForwardVector;
    float Weight = 0;
    float Extension = 0.65f;
    float PhaseAlpha = 0;
    float RecoveryAlpha = 0;
    float FollowThrough = 0;
    FVector SwingAxis = FVector::UpVector;
    float StartPoseBlend = 0;
    bool bRecoverStandingPosture = false;
    bool bUseFootTargets = false;
    FTransform FootTargets[2];
    FVector PelvisOffset = FVector::ZeroVector;
    FRotator CoreRotation = FRotator::ZeroRotator;
    FVector ReactionWorld = FVector::ZeroVector;
    float WeaponLoad = 1;
};

/** Authority notification of a real Chaos contact, independent of legacy guard enums. */
USTRUCT(BlueprintType)
struct FCombatPhysicalContact
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) int32 Serial = 0;
    UPROPERTY(BlueprintReadOnly) int32 AttackId = 0;
    UPROPERTY(BlueprintReadOnly) FGuid WeaponId;
    UPROPERTY(BlueprintReadOnly) FVector Point = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) FVector Impulse = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) TObjectPtr<AActor> OtherActor = nullptr;
    UPROPERTY(BlueprintReadOnly) uint8 EffectSurface = 0;
    UPROPERTY(BlueprintReadOnly) FVector Normal = FVector::UpVector;
    UPROPERTY(BlueprintReadOnly) bool bWeaponClash = false;
    UPROPERTY(BlueprintReadOnly) bool bOtherAttacking = false;
    UPROPERTY(BlueprintReadOnly) bool bSelfAttacking = false;
};

UENUM(BlueprintType)
enum class ECombatDamageKind : uint8 { Weapon, Unarmed, Blunt, Projectile };

/** Confirmed server body contact; HP/damage remains an external consumer. */
USTRUCT(BlueprintType)
struct FCombatMotionBodyHit
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) ECombatDamageKind DamageKind=ECombatDamageKind::Weapon;
    UPROPERTY(BlueprintReadOnly) float AuthoredDamage=-1;
    UPROPERTY(BlueprintReadOnly) float DamageApplied=0;
    UPROPERTY(BlueprintReadOnly) bool bRanged=false;
    UPROPERTY(BlueprintReadOnly) float SourceMass=1;
    UPROPERTY() FGuid ThreatId;
    UPROPERTY(BlueprintReadOnly) float DamageScale=1;
    UPROPERTY(BlueprintReadOnly) float ImpulseScale=1;
    UPROPERTY() uint32 WeaponEpoch=0;
    UPROPERTY() int32 StrikePhase=0;
    UPROPERTY(BlueprintReadOnly) EDirectionalAttack AttackType=EDirectionalAttack::Light;
    UPROPERTY() uint32 TargetLifeId=0;
    UPROPERTY(BlueprintReadOnly) FName Action;
    UPROPERTY(BlueprintReadOnly) int32 Serial = 0;
    UPROPERTY(BlueprintReadOnly) int32 AttackId = 0;
    UPROPERTY(BlueprintReadOnly) FGuid WeaponId;
    UPROPERTY(BlueprintReadOnly) TObjectPtr<AActor> Attacker = nullptr;
    UPROPERTY(BlueprintReadOnly) TObjectPtr<AActor> Target = nullptr;
    UPROPERTY(BlueprintReadOnly) FName BodyRegion;
    UPROPERTY(BlueprintReadOnly) FVector Point = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) FVector ContactVelocity = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) float RelativeSpeed = 0;
    UPROPERTY(BlueprintReadOnly) double Time = 0;
};

/** Server-authored contact response. Physics offsets are reproduced locally, never trusted as damage. */
USTRUCT()
struct FCombatMotionReaction
{
    GENERATED_BODY()
    UPROPERTY() FName AnimationRow;
    UPROPERTY() FName DefenseProfile;
    UPROPERTY() float AnimationEntry=0;
    UPROPERTY() float AnimationExit=1;
    UPROPERTY() float AnimationDuration=0.5f;
    UPROPERTY() FVector EntryFacing=FVector::ForwardVector;
    UPROPERTY() int32 Serial = 0;
    UPROPERTY() double Start = 0;
    UPROPERTY() double GuardAt = 0;
    UPROPERTY() FName Bone;
    UPROPERTY() FVector Point = FVector::ZeroVector;
    UPROPERTY() FVector Impulse = FVector::ZeroVector;
    UPROPERTY() float Strength = 0;
    UPROPERTY() bool bWeapon = false;
    UPROPERTY() bool bInterruptsAttack = false;
};
