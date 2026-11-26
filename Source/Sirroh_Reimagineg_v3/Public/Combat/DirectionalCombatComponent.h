#pragma once


#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Sword/SwordTypes.h"
#include "Combat/CombatAttackTypes.h"
#include "Combat/CombatPresentationProfile.h"
#include "Combat/CombatBodySolver.h"
#include "Combat/CombatDefenseResolver.h"
#include "Combat/CombatWeaponDefinition.h"
#include "DirectionalCombatComponent.generated.h"


class UAnimSequence;
class USkeletalMeshComponent;
class USkeletalMesh;
class USirrohPoseBridgeComponent;
class UPlaceholderHandsComponent;
class AProceduralSword;
class UCombatContactSubsystem;
class UCombatDragonWeaponAdapter;
class UCombatMotionContactSubsystem;


DECLARE_MULTICAST_DELEGATE_OneParam(FOnCombatContact, const FCombatContactEvent&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCombatKnockback, const FVector&);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCombatPhysicalContact, const FCombatPhysicalContact&, Contact);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCombatMotionBodyHit, const FCombatMotionBodyHit&, Hit);


/** Shared weapon model, visual IK and replicated combat commands/reactions. */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class SIRROH_REIMAGINEG_V3_API UDirectionalCombatComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UDirectionalCombatComponent();
    FCombatWeaponDefinition GetEquippedWeaponGeometry() const;
    virtual void TickComponent(float Delta, ELevelTick Tick, FActorComponentTickFunction* Function) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Combat|Defense") bool bUseIndicatorDefense=true;
    bool UsesIndicatorDefense() const { return bUseMotionCoreV3 && bUseIndicatorDefense; }
    FCombatDefenseFrame GetDefenseFrame() const;
    double GetDefenseTime() const { return CombatTime(); }
    const TArray<FCombatDefenseThreat>& GetDefenseThreats() const { return DefenseThreats; }
    const FCombatDefenseResult& GetLastDefenseResult() const { return LastDefenseResult; }
    bool TryDefenseParry();
    float GetDefenseDurability() const { return DefenseDurability; }
    void ToggleCombat();
    void ToggleGripMode();
    void RequestLibraryAction(FName Action, FVector Direction=FVector::ZeroVector);
    UFUNCTION(BlueprintPure, Category="Combat") bool IsTwoHandedGrip() const { return MotionCoreStyle.bTwoHanded; }
    void LeaveCombat();
    bool IsCombatActive() const { return bCombatActive; }
    /** Server-selected phase-2 guard spike; legacy attacks/contacts are isolated from it. */
    UPROPERTY(BlueprintAssignable, Category="Combat|Motion Core V3") FOnCombatPhysicalContact OnPhysicalWeaponContact;
    UPROPERTY(BlueprintAssignable, Category="Combat|Motion Core V3") FOnCombatMotionBodyHit OnMotionBodyHit;
    bool TryGetContactBone(FName Name, FTransform& Out) const;
    bool UsesMotionCoreV3() const { return bUseMotionCoreV3; }
    bool IsMotionRigPrepared() const { return bMotionRigReady && bOtherGripReady; }
    bool OwnsMotionCoreWeapon() const { return bUseMotionCoreV3 && bMotionCoreValid && (bCombatActive || Alpha>0); }
    const FCombatFinalPose* GetMotionCorePose() const { return bMotionCoreValid?&MotionFinalPose:nullptr; }
    const FCombatWeaponSample* GetMotionCoreWeaponSample() const { return bMotionCoreValid?&MotionWeaponSample:nullptr; }
    UFUNCTION(BlueprintPure, Category="Combat|Motion Core V3") FCombatAttackQuality GetMotionAttackQuality() const { return MotionAttackQuality; }
    /** Set on the authority's actor class before spawn. Latched/replicated for this actor lifetime. */
    // Local development controls, deliberately unavailable through network RPCs.
    UPROPERTY(EditAnywhere, Transient, Category="Debug Solvers") bool bDebugBodySolver = true;
    UPROPERTY(EditAnywhere, Transient, Category="Debug Solvers") bool bDebugFeetSolver = true;
    UPROPERTY(EditAnywhere, Transient, Category="Debug Solvers") bool bDebugLookSolver = true;
    UPROPERTY(EditAnywhere, Transient, Category="Debug Solvers") bool bDebugBodyPhysics = true;
    UPROPERTY(EditAnywhere, Transient, Category="Debug Solvers") bool bDebugWeaponPhysics = true;
    UPROPERTY(EditAnywhere, Transient, Category="Debug Solvers", meta=(ClampMin="0",ClampMax="1")) float DebugLookWeight = 1;
    UPROPERTY(EditAnywhere, Transient, Category="Debug Solvers", meta=(ClampMin="0",ClampMax="1")) float DebugAnimationWeight = 1;
    UPROPERTY(EditAnywhere, Transient, Category="Debug Solvers", meta=(ClampMin="0",ClampMax="5")) float DebugPhysicsStrength = 1;
    UPROPERTY(EditAnywhere, Transient, Category="Debug Solvers", meta=(ClampMin="0",ClampMax="5")) float DebugPhysicsDamping = 1;
    UPROPERTY(EditAnywhere, Transient, Category="Debug Solvers", meta=(ClampMin="0",ClampMax="5")) float DebugHitImpulse = 1;
    UPROPERTY(EditAnywhere, Transient, Category="Debug Solvers") bool bDebugAnimationGuidance = true;
    UPROPERTY(EditAnywhere, Transient, Category="Debug Solvers", meta=(ClampMin="0",ClampMax="1")) float DebugBodyWeight = 1;
    UPROPERTY(EditAnywhere, Transient, Category="Debug Solvers", meta=(ClampMin="0",ClampMax="1")) float DebugFeetWeight = 1;
    UPROPERTY(EditAnywhere, Transient, Category="Debug Solvers", meta=(ClampMin="0",ClampMax="5")) float DebugWeaponStrength = 1;
    UPROPERTY(EditAnywhere, Transient, Category="Debug Solvers", meta=(ClampMin="0",ClampMax="5")) float DebugWeaponDamping = 1;
    void DebugRefreshSolvers();
    void DebugRefreshConfiguration() { OnRep_MotionCoreConfiguration(); }

    UPROPERTY(EditDefaultsOnly, ReplicatedUsing=OnRep_MotionCoreConfiguration, Category="Combat|Motion Core V3") bool bUseMotionCoreV3=false;
    UPROPERTY(EditDefaultsOnly, ReplicatedUsing=OnRep_MotionStyle, Category="Combat|Motion Core V3") FCombatStyleDefinition MotionCoreStyle;
    UPROPERTY(EditDefaultsOnly, Category="Combat|Styles") TSoftObjectPtr<class UDataTable> CombatStyles;
    UFUNCTION(BlueprintCallable, Category="Combat|Styles") void CycleCombatStyle(int32 Direction);
    UFUNCTION(BlueprintPure, Category="Combat|Styles") FText GetCombatStyleName() const;
    bool IsStyleControlsHovered() const;
    FVector GetAimDirection() const;
    FVector GetCombatFacingOffset() const;
    void BeginAttack(EDirectionalAttack Type);
    void ReleaseAttack(EDirectionalAttack Type);
    void CancelAttack();
    void SetBlocking(bool bEnabled);
    bool IsMovementLocked() const;
    bool CanFight() const;
    void ApplyVitalityRestrictions();
    /** Shared authority damage/reaction path for native contacts and AST scalar hits. */
    void ReceiveBodyContact(FCombatMotionBodyHit& Hit,float SourceMass);
    bool BlendHitAnimation(FCombatFinalPose& Pose,double Now);
    bool IsDisarmed() const { return Reaction.bDisarmed; }
    void StartAICombat(const FSwordInstance& Sword);
    void SetAITarget(const FVector& WorldTarget);
    void RearmAI();
    /** Bridge replicated disarm drops into the existing local placeholder inventory. */
    void ClaimWorldSword(AProceduralSword* Sword,int32 Slot);
    FCombatWeaponFrame EvaluateWeapon(double Time, const FVector& Origin) const;
    static FCombatWeaponFrame EvaluateAttackFrame(const FDirectionalAttackMotion& Motion,const FCombatWeaponFrame& Rest,double Time,const FVector& Origin);
    FCombatWeaponFrame GetWeaponFrame() const;
    ECombatGuard GetGuard() const { return CurrentGuard; }
    ECombatGuard GetAttackGuard() const { return AttackMotion.Guard; }
    float GetChargeSeconds() const;
    ECombatIntensity GetSwingIntensity() const { return bUseMotionCoreV3?(BodyAttack.Type==EDirectionalAttack::Heavy?ECombatIntensity::Strong:BodyAttack.Type==EDirectionalAttack::Pierce?ECombatIntensity::Medium:ECombatIntensity::Weak):AttackMotion.Intensity; }
    uint32 GetAttackId() const { return bUseMotionCoreV3?BodyAttack.Id:AttackMotion.Id; }
    float GetWeaponPixelScale() const { return Rig.PixelScale; }
    float GetGuardTargetHeight(ECombatGuard Guard) const
    {
        return (Guard==ECombatGuard::Up ? Rig.HeadHeight+35 :
            Guard==ECombatGuard::Low ? Rig.PelvisHeight-35 :
            (Rig.HeadHeight+Rig.PelvisHeight)*0.5f)-StanceLowering;
    }
    FOnCombatContact OnContact;
    /** Called on authority after a stagger; a custom mover can subscribe here. */
    FOnCombatKnockback OnKnockback;
    EDirectionalCombatPhase GetCombatPhase() const;
    FText GetCombatHint() const;


    UPROPERTY(EditAnywhere, Category="Combat|Pose") TSoftObjectPtr<UAnimSequence> CombatStyle;
    UPROPERTY(EditAnywhere, Category="Combat|Pose") FName ProfileRow=TEXT("OneHandedSword");
    UPROPERTY(EditAnywhere, Category="Combat|Pose", meta=(ClampMin="0")) float PoseTime=0.0f;
    UPROPERTY(EditAnywhere, Category="Combat|Pose", meta=(ClampMin="0.01")) float BlendSeconds=0.2f;
    UPROPERTY(EditAnywhere, Category="Combat|Aim", meta=(ClampMin="0.1")) float FollowSpeed=12.0f;
    UPROPERTY(EditAnywhere, Category="Combat|Aim", meta=(ClampMin="0.2",ClampMax="0.95")) float ArmExtension=0.78f;
    UPROPERTY(EditAnywhere, Category="Combat|Aim", meta=(ClampMin="0",ClampMax="60")) float MaxTorsoLeanDegrees=12.0f;
    UPROPERTY(EditAnywhere, Category="Combat|Movement", meta=(ClampMin="0.1",ClampMax="2")) float CombatMoveSpeed=1.0f;
    UPROPERTY(EditAnywhere, Category="Combat|Movement", meta=(ClampMin="20",ClampMax="100",Units="cm/s")) float GuardWalkSpeed=100;
    float GetGuardWalkSpeed() const { return FMath::Clamp(GuardWalkSpeed,20.f,100.f); }
    FVector GetPendingAttackFacing() const { return CombatTime()<MotionReaction.GuardAt && !ReactionFacing.IsNearlyZero()?ReactionFacing:PendingAttackFacing; }
    const FCombatBodyRig* GetReactionRig(FName Id);
    UPROPERTY(EditAnywhere, Category="Combat|Attack") FDirectionalAttackSettings LightAttack;
    UPROPERTY(EditAnywhere, Category="Combat|Attack") FDirectionalAttackSettings HeavyAttack;
    UPROPERTY(EditAnywhere, Category="Combat|Attack") FDirectionalAttackSettings PierceAttack;
    UPROPERTY(EditAnywhere, Category="Combat|Contact") FCombatContactSettings ContactSettings;
    UPROPERTY(EditAnywhere, Category="Combat|Pose") TSoftObjectPtr<UAnimSequence> BlockPose;
    UPROPERTY(EditAnywhere, Category="Combat|Pose") float BlockPoseTime=0;
    UPROPERTY(EditAnywhere, Category="Combat|Charge") FCombatChargeSettings Charging;
    UPROPERTY(EditAnywhere, Category="Combat|Movement") float CombatSpeedMultiplier=0.65f;
    UPROPERTY(EditAnywhere, Category="Combat|Camera") float CombatViewWidthMultiplier=0.7f;
    UPROPERTY(EditAnywhere, Category="Combat|Camera") float CombatCameraBlendSeconds=0.3f;
    UPROPERTY(EditAnywhere, Category="Combat|Pose") float StanceLowering=7;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    void UpdateIndicatorDefense();
    void RemoveDefenseOverlay();
    void AddDefenseThreat(const FCombatDefenseThreat& Threat);
    void BeginDefenseStep(double Time);
    void AdvanceDefenseTo(double Time);
    double NextDefenseEvent(double Time) const;
    void CancelInvalidDefenseThreats(double Time);
    void HandleDefenseResult(const FCombatDefenseResult& Result);
    UFUNCTION(Server,Reliable) void ServerDefenseParry();
    UFUNCTION(NetMulticast,Reliable) void MulticastDefenseResult(FCombatDefenseResult Result);
    UPROPERTY(Replicated) TArray<FCombatDefenseThreat> DefenseThreats;
    UPROPERTY(Replicated) float DefenseDurability=100;
    UPROPERTY(Replicated) FCombatDefenseResult LastDefenseResult;
    FCombatDefenseResolver DefenseResolver;
    FCombatDefenseFrame DefensePreviousFrame,DefenseCurrentFrame;
    double DefenseStepStart=-1,DefenseStepEnd=-1,DefenseAdvancedTime=-1;
    FGuid DefenseWeapon;
    TWeakObjectPtr<class UCapsuleComponent> DefenseCapsule;
    float DefenseOriginalCapsuleRadius=-1;
    uint32 DefenseLifeId=0;
    bool bDefenseConsumedHeavy=false;
    TSharedPtr<class SWidget> DefenseOverlay;
    friend class UCombatContactSubsystem;
    friend class UCombatMotionContactSubsystem;
    UFUNCTION(NetMulticast,Reliable) void MulticastMotionBodyHit(FCombatMotionBodyHit Hit);
    int32 MotionBodyHitSerial = 0;
    UPROPERTY(Replicated) ECombatGuard CurrentGuard=ECombatGuard::Front;
    UFUNCTION(Server,Reliable) void ServerSetGuard(ECombatGuard Guard);
    UPROPERTY(Transient) TArray<FCombatGuardProfile> GuardProfiles;
    TArray<FQuat> GuardRotations[3];
    TArray<FQuat> ReadyRotations[9];
    TArray<FQuat> SmoothedGuardPose;
    FVector PreviousOwnerPosition=FVector::ZeroVector;
    float LowerBodyAlpha=0;
    UPROPERTY(Replicated) bool bAIControlled=false;
    UPROPERTY(Replicated) FCombatRig Rig;
    UPROPERTY(Replicated) bool bBlocking=false;
    UPROPERTY(ReplicatedUsing=OnRep_Reaction) FCombatReaction Reaction;
    UFUNCTION() void OnRep_Reaction();
    UFUNCTION(Server,Reliable) void ServerSetBlocking(bool bEnabled);
    UFUNCTION(Client,Reliable) void ClientCorrectBlocking(bool bEnabled);
    UFUNCTION(NetMulticast,Reliable) void MulticastContact(FCombatContactEvent Event);
    UFUNCTION(NetMulticast,Reliable) void MulticastReaction(FCombatReaction Value);
    UFUNCTION(Server,Reliable) void ServerClaimWorldSword(AProceduralSword* Sword,int32 Slot);
UPROPERTY(Replicated) bool bCombatActive=false;
    UPROPERTY(Replicated) FSwordInstance EquippedSword;
    UPROPERTY(Replicated) FVector_NetQuantize10 AimOffset=FVector(100,0,30);
    UFUNCTION(Server,Reliable) void ServerSetCombat(bool bEnabled, FSwordInstance Sword, FVector_NetQuantize10 Aim, FCombatRig Geometry);
    UFUNCTION(Server,Unreliable) void ServerSetAim(FVector_NetQuantize10 Aim);
    UPROPERTY(ReplicatedUsing=OnRep_AttackMotion) FDirectionalAttackMotion AttackMotion;
    UFUNCTION() void OnRep_AttackMotion();
    UFUNCTION(Client,Reliable) void ClientCorrectAttack(FDirectionalAttackMotion Motion);
    UFUNCTION(Server,Reliable) void ServerSetAttack(FDirectionalAttackMotion Motion);
    UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> Mesh;
    UPROPERTY(Transient) TObjectPtr<USkeletalMesh> CachedMesh;
    UPROPERTY(Transient) TObjectPtr<USirrohPoseBridgeComponent> PoseBridge;
    UPROPERTY(Transient) TObjectPtr<UPlaceholderHandsComponent> Hands;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> Stance;
    UPROPERTY(Transient) TObjectPtr<AProceduralSword> RemoteSword;
    TWeakObjectPtr<AProceduralSword> LastSword;
    TArray<FTransform> Pose;
    TMap<FName,FTransform> CombatPose;
    TArray<FQuat> StanceRotations;
    UPROPERTY(Transient) TArray<TSoftObjectPtr<UAnimSequence>> AttackPoseAssets;
    TArray<FQuat> AttackRotations[3];
    TArray<FQuat> BlockRotations;
    TArray<bool> UpperBody;
    float Alpha=0;
    float SendElapsed=0;
    float AppliedMoveScale=1;
    float UnscaledMoveSpeed=-1, LastAppliedMoveSpeed=-1;
    FVector LastSentAim=FVector::ZeroVector;
    FVector SmoothedDirection=FVector(1,0,0);
    FCombatWeaponFrame RestWeapon;
    float BlockAlpha=0;
    double BlockDisabledUntil=0;
    uint32 LastAppliedReaction=0;
    FCombatReaction AppliedReaction;
    bool bPoseReady=false;
    bool IsLocal() const;
    void BindComponents();
    void CacheStance();
    void ApplyPose(float Delta, AProceduralSword* Sword);
    void ApplyMoveScale(float Scale);
    void PublishState();
    void UpdateRemoteSword();
    void CacheAttackPoses();
    void UpdateCursorAim();
    void PublishAttack();
    double CombatTime() const;
    const FDirectionalAttackSettings& SettingsFor(EDirectionalAttack Type) const;
    float AttackPoseWeight() const;
    void UpdateWeapon(float Delta);
    void CacheRig();
    AProceduralSword* ActiveSword() const;
    void React(ECombatContactResult Result, double Time, float Duration, bool bInterrupt, bool bDisarm=false);
    void ApplyKnockback(const FVector& Velocity);
    void LoadGuardProfiles();
    void SelectGuard();
    void ComputeCharge(FDirectionalAttackMotion& Motion) const;
    UFUNCTION() void OnRep_MotionCoreConfiguration();
    UFUNCTION() void OnRep_MotionStyle(const FCombatStyleDefinition& Previous);
    UFUNCTION(Server, Reliable) void ServerToggleGripMode();
    UFUNCTION() void OnRep_GripMode();
    UPROPERTY(ReplicatedUsing=OnRep_GripMode) FCombatGripMode GripMode;
    int8 PendingGripMode = -1;
    FCombatFinalPose GripTransitionStart;
    void UpdateGripMode();
    void PlanDirectionalAttack(const FCombatIntent& Intent, FCombatBodyDrive& Drive) const;
    bool BlendGripTransition(FCombatFinalPose& Pose, double Now);
    UPROPERTY(Replicated) FCombatBodyAttack BodyAttack;
    struct FBufferedBodyAttack
    {
        bool bPending=false, bReleased=false;
        EDirectionalAttack Type=EDirectionalAttack::Light;
        FGuid WeaponId;
        FVector Target=FVector::ZeroVector;
        double ExpiresAt=0;
    };
    FBufferedBodyAttack BufferedBodyAttack;
    void ProcessBufferedBodyAttack();
    int32 SelectCatalogAction(FName Action,FVector2D Direction,bool Moving,int32 Seed) const;
    int32 SelectLibraryAttack(const TArray<int32>& Candidates, EDirectionalAttack Type, const FVector& Target);
    void EvaluateLibraryGuard(FCombatIntent& Intent, FCombatBodyDrive& Drive);
    void BlendLibraryGuidance(FCombatBodyDrive& Drive,const FCombatIntent& Intent);
    void ApplyCombatLook(FCombatFinalPose& Pose,const FCombatIntent& Intent,const FCombatBodyDrive& Drive,float Delta);
    int32 LibraryLoopSource=INDEX_NONE, GuidanceSource=INDEX_NONE;
    uint32 GuidanceAttack=0;
    ECombatMotionPhase GuidancePhase=ECombatMotionPhase::Inactive;
    double LibraryLoopTime=0, LibraryLastTime=0, GuidanceBlendAt=-1;
    bool bLibraryWalking=false;
    bool bLibraryWasGuard=false;
    TArray<FQuat> GuidanceOffsets;
    FVector GuidancePelvisOffset=FVector::ZeroVector;
    double GuidanceDuration=0.5;
    FCombatBodyRig OtherGripRig;
    bool bOtherGripReady=false;
    bool bOtherGripTwoHanded=false;
    void WarmOtherGrip();
    FVector SmoothedLookDirection=FVector::ZeroVector;
    float LookWeight=0;
    UFUNCTION(Server,Reliable) void ServerLibraryAction(FName Action,FVector_NetQuantizeNormal Direction);
    UPROPERTY(Replicated) int32 LibraryActionSource=INDEX_NONE;
    UPROPERTY(Replicated) FName LibraryActionVariantId;
    UPROPERTY(Replicated) double LibraryActionStart=0;
    UPROPERTY(Replicated) uint32 LibraryActionSerial=0;
    UPROPERTY(Replicated) FVector_NetQuantizeNormal LibraryActionDirection=FVector::ForwardVector;
    UPROPERTY(Replicated) FVector_NetQuantizeNormal PendingAttackFacing=FVector::ZeroVector;
    uint32 GuidanceAction=0;
    bool EvaluateLibraryAction(FCombatIntent& Intent,FCombatBodyDrive& Drive);
    bool IsLibraryActionPlaying() const;
    int32 MotionSubmittedStrikePhase=0;
    double BodyChainTime() const;
    double ReturnBlendAt=-1;
    ECombatMotionPhase PreviousPresentedPhase=ECombatMotionPhase::Inactive;
    UPROPERTY(Transient) TObjectPtr<UCombatDragonWeaponAdapter> MotionWeaponAdapter;
    UPROPERTY(Transient) TObjectPtr<class UCombatDragonBodyAdapter> MotionBodyAdapter;
    UPROPERTY(ReplicatedUsing=OnRep_MotionReaction) FCombatMotionReaction MotionReaction;
    UFUNCTION() void OnRep_MotionReaction();
    void ReceiveMotionBodyHit(FCombatMotionBodyHit& Hit);
    void ReceiveMotionWeaponHit(const FCombatPhysicalContact& Hit);
    void BeginMotionReaction(FName Bone, const FVector& Point, const FVector& Impulse, float Strength, bool bWeapon);
    void QueueMotionKnockback(const FVector& Displacement);
    int32 MotionAppliedReaction = 0;
    int32 MotionPhysicsReaction = 0;
    UPROPERTY(ReplicatedUsing=OnRep_PhysicalContact) FCombatPhysicalContact PhysicalContact;
    UFUNCTION() void OnRep_PhysicalContact();
    FVector MotionContactReaction = FVector::ZeroVector;
    double MotionContactPublishTime = -1;
    uint32 MotionSubmittedAttackId = 0;
    EDirectionalAttack MotionSubmittedAttackType=EDirectionalAttack::Light;
    bool bMotionSubmittedAttackActive = false;
    uint32 MotionInertiaAttackId = 0;
    uint32 MotionPreparedRootId = 0;
    float MotionContinuation = 0;
    float MotionSettledSeconds = 0;
    bool bMotionPullIssued = false;
    FVector MotionInertiaReaction = FVector::ZeroVector;
    FVector MotionSwingAxis = FVector::UpVector;
    void UpdateWeaponInertia(float Delta, const FVector& RootVelocity);
    void CancelInertiaMovement();
    void CaptureAttackStart();
    uint32 MotionSnapshotAttackId = 0;
    void PlanCombatFeet(float Delta, const FVector& RootVelocity, FCombatBodyDrive& Drive);
    bool bMotionFeetReady = false;
    FVector PreviousLoadVelocity=FVector::ZeroVector;
    bool bHasLoadVelocity=false;
    bool bMotionHoldingSupport = false;
    FQuat MotionSupportFacing = FQuat::Identity;
    FTransform MotionFeet[2];
    FTransform MotionStepStart, MotionStepEnd;
    float MotionFootSole[2] = { 4, 4 };
    int32 MotionSwingFoot = INDEX_NONE;
    float MotionFootProgress = 0;
    UFUNCTION(Server,Reliable) void ServerBodyAttack(uint8 Command, EDirectionalAttack Type, FGuid WeaponId, FVector_NetQuantize10 Target);
    void SubmitBodyAttack(uint8 Command, EDirectionalAttack Type);
    FCombatBodyDrive EvaluateBodyAttack(FCombatIntent& Intent) const;
    void TickMotionCore(float Delta);
    void InitializeCombatStyles();
    void UpdateCombatStyles();
    bool ApplyCombatStyle(FName RowName);
    UFUNCTION(Server,Reliable) void ServerCycleCombatStyle(int32 Direction);
    FName PendingCombatStyle;
    TSharedPtr<class SWidget> StyleControls;
    TSharedPtr<class SWidget> StyleControlsLayer;
    void RemoveStyleControls();
    void ResetMotionCore();
    void PrepareMotionRig();
    bool bMotionWarmupAttempted=false;
    void FailMotionCore(const FString& Reason);
    FString MotionCoreFailure;
    FCombatBodyRig MotionRig;
    FCombatBodySolverState MotionSolverState;
    FCombatMotionPlan MotionPlan;
    FCombatFinalPose MotionFinalPose;
    FCombatFinalPose MotionPlannedPose;
    FCombatAttackQuality MotionAttackQuality;
    FCombatBodyRig HitAnimationRig;
    FName LoadedHitAnimation;
    TMap<FName,FCombatBodyRig> ReactionRigs;
    FVector ReactionFacing=FVector::ZeroVector;
    void SelectHitAnimation();
    bool QueueReactionRootMotion();
    FCombatWeaponSample MotionWeaponSample;
    FCombatBodySolverDiagnostics MotionDiagnostics;
    TWeakObjectPtr<USkeletalMesh> MotionRigMesh;
    FTransform MotionRigRelativeTransform;
    bool bMotionRigAttempted=false;
    bool bMotionRigReady=false;
    bool bMotionCoreValid=false;
    bool bMotionFailureLogged=false;
    FGuid MotionWeaponId;
    float MotionPixelScale=0;
    uint32 MotionStep=0;
    uint32 MotionEpoch=0;
    FVector MotionPreviousRoot=FVector::ZeroVector;
};
