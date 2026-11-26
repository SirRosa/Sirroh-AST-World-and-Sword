#include "Combat/DirectionalCombatComponent.h"
#include "Items/SirrohASTInventoryComponent.h"
#include "Combat/CombatVitalityComponent.h"
#include "Combat/CombatPoseProfile.h"
#include "CombatDragonWeaponAdapter.h"
#include "CombatDragonBodyAdapter.h"
#include "CombatMotionContactSubsystem.h"
#include "Combat/CombatContactSubsystem.h"
#include "Combat/CombatEffectsComponent.h"
#include "Placeholder/PlaceholderHandsComponent.h"
#include "Sword/ProceduralSword.h"
#include "Character/SirrohPoseBridgeComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "TwoBoneIK.h"
#include "MoverComponent.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "HAL/IConsoleManager.h"

namespace
{
    TAutoConsoleVariable<int32> CVarMotionCoreV3(TEXT("Combat.MotionCoreV3"),1,
        TEXT("Enable the guard-only body-authority spike on authority actors at BeginPlay. Replicated to clients; no live switching."));
}


UDirectionalCombatComponent::UDirectionalCombatComponent()
{
    PrimaryComponentTick.bCanEverTick=true;
    PrimaryComponentTick.TickGroup=TG_PostPhysics;
    SetIsReplicatedByDefault(true);
    HeavyAttack.Duration=0.48f; HeavyAttack.Recovery=0.42f; HeavyAttack.FollowThroughDegrees=100;
    PierceAttack.Duration=0.22f; PierceAttack.Recovery=0.22f; PierceAttack.ReachFraction=0.94f;
    CombatStyles=TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/Systems/Procedural_Combat/DT_Combat_Styles.DT_Combat_Styles")));
    CombatStyle=TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/Systems/Procedural_Combat/Animations/LongSword/UEFNLongsword_Idle1.UEFNLongsword_Idle1")));
    BlockPose=TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/Systems/Procedural_Combat/Animations/LongSword/UEFNLongsword_BlockLoop.UEFNLongsword_BlockLoop")));
}
bool UDirectionalCombatComponent::IsLocal() const
{ const APawn* P=Cast<APawn>(GetOwner()); return (bAIControlled && GetOwner()->HasAuthority()) || (P && P->IsLocallyControlled()); }
void UDirectionalCombatComponent::BeginPlay()
{
    Super::BeginPlay();
    UCombatVitalityComponent::FindOrAdd(GetOwner());
    if (GetOwner()->HasAuthority() && CVarMotionCoreV3.GetValueOnGameThread()!=0) bUseMotionCoreV3=true;
    if (bUseMotionCoreV3)
    {
        // A single existing style asset is the fallback, never guard/attack pose caches.
        if (MotionCoreStyle.BasePose.IsNull())
        {
            MotionCoreStyle.BasePose=CombatStyle;
            MotionCoreStyle.BasePoseTime=PoseTime;
        }
        if (MotionCoreStyle.StyleId.IsNone()) MotionCoreStyle.StyleId=TEXT("BodyAuthoritySpike");
        BindComponents();
        InitializeCombatStyles();
        // Prepare both grips and reaction poses during level startup, not on first Loot.
        PrepareMotionRig();
        if (GetNetMode()!=NM_DedicatedServer && !GetOwner()->FindComponentByClass<UCombatEffectsComponent>())
        {
            auto* Effects=NewObject<UCombatEffectsComponent>(GetOwner());
            GetOwner()->AddInstanceComponent(Effects); Effects->RegisterComponent();
        }
        GetWorld()->GetSubsystem<UCombatMotionContactSubsystem>()->RegisterCombatant(this);
        PreviousOwnerPosition=GetOwner()->GetActorLocation();
        UE_LOG(LogTemp,Log,TEXT("Motion Core V3 on %s: body-driven attacks and Dragon physical weapon; legacy contacts disabled."),*GetOwner()->GetName());
        return;
    }
    const TCHAR* PoseNames[]={TEXT("UEFNLongsword_Attack_R"),TEXT("UEFNLongsword_Attack_D"),TEXT("UEFNLongsword_Attack_Thrust")};
    for (const TCHAR* Name:PoseNames)
        AttackPoseAssets.Add(TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(FString::Printf(TEXT("/Game/Systems/Procedural_Combat/Animations/LongSword/%s.%s"),Name,Name))));
    if (auto* Table=LoadObject<UDataTable>(nullptr,TEXT("/Game/Systems/Procedural_Combat/DT_Combat_Stats.DT_Combat_Stats")))
        if (const auto* Row=Table->FindRow<FCombatPoseProfile>(ProfileRow,TEXT("Combat pose"),false))
        {
            if (!Row->CombatStyle.IsNull()) CombatStyle=Row->CombatStyle;
            PoseTime=Row->PoseTime; BlendSeconds=FMath::Max(0.01f,Row->BlendSeconds);
            FollowSpeed=FMath::Max(0.1f,Row->FollowSpeed); ArmExtension=FMath::Clamp(Row->ArmExtension,0.2f,0.95f);
            MaxTorsoLeanDegrees=FMath::Clamp(Row->MaxTorsoLeanDegrees,0.0f,60.0f);
            CombatMoveSpeed=FMath::Clamp(Row->CombatMoveSpeed,0.1f,2.0f);
            LightAttack=Row->LightAttack; HeavyAttack=Row->HeavyAttack; PierceAttack=Row->PierceAttack;
            if (!Row->AttackLightPose.IsNull()) AttackPoseAssets[0]=Row->AttackLightPose;
            if (!Row->AttackHeavyPose.IsNull()) AttackPoseAssets[1]=Row->AttackHeavyPose;
            if (!Row->AttackPiercePose.IsNull()) AttackPoseAssets[2]=Row->AttackPiercePose;
            if (!Row->BlockPose.IsNull()) BlockPose=Row->BlockPose;
            BlockPoseTime=Row->BlockPoseTime; ContactSettings=Row->Contacts;
            Charging=Row->Charging;
            CombatSpeedMultiplier=FMath::Clamp(Row->CombatSpeedMultiplier,0.1f,1.0f);
            CombatViewWidthMultiplier=FMath::Clamp(Row->CombatViewWidthMultiplier,0.3f,1.0f);
            CombatCameraBlendSeconds=FMath::Max(0.05f,Row->CombatCameraBlendSeconds);
            StanceLowering=FMath::Clamp(Row->StanceLowering,0.0f,20.0f);
        }
    BindComponents();
    PreviousOwnerPosition=GetOwner()->GetActorLocation();
    GetWorld()->GetSubsystem<UCombatContactSubsystem>()->RegisterCombatant(this);
    if (GetNetMode()!=NM_DedicatedServer && !GetOwner()->FindComponentByClass<UCombatEffectsComponent>())
    {
        auto* Effects=NewObject<UCombatEffectsComponent>(GetOwner());
        GetOwner()->AddInstanceComponent(Effects);
        Effects->RegisterComponent();
    }
}
void UDirectionalCombatComponent::BindComponents()
{
    if (!Mesh)
    {
        Mesh=GetOwner()->FindComponentByClass<USkeletalMeshComponent>();
        if (Mesh) AddTickPrerequisiteComponent(Mesh);
    }
    if (Mesh && bUseMotionCoreV3)
    {
        Mesh->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
        Mesh->bEnableUpdateRateOptimizations=false;
    }
    if (Mesh && Mesh->GetSkeletalMeshAsset()!=CachedMesh)
    { CachedMesh=Mesh->GetSkeletalMeshAsset(); bPoseReady=false; if (!bUseMotionCoreV3) CacheStance(); }
    if (!Hands)
    {
        Hands=GetOwner()->FindComponentByClass<UPlaceholderHandsComponent>();
        if (Hands) AddTickPrerequisiteComponent(Hands);
    }
    if (!PoseBridge)
    {
        PoseBridge=USirrohPoseBridgeComponent::FindOrCreate(GetOwner());
        if (PoseBridge) PoseBridge->AddTickPrerequisiteComponent(this);
    }
}
void UDirectionalCombatComponent::CacheStance()
{
    if (!Mesh || !Mesh->GetSkeletalMeshAsset()) return;
    const FReferenceSkeleton& Ref=Mesh->GetSkeletalMeshAsset()->GetRefSkeleton();
    const int32 Root=Ref.FindBoneIndex(TEXT("spine_01"));
    if (Root==INDEX_NONE || Ref.FindBoneIndex(TEXT("hand_r"))==INDEX_NONE ||
        Ref.FindBoneIndex(TEXT("upperarm_r"))==INDEX_NONE || Ref.FindBoneIndex(TEXT("lowerarm_r"))==INDEX_NONE)
    { UE_LOG(LogTemp,Warning,TEXT("Combat: skeleton needs spine_01 and right arm bones.")); return; }
    Stance=CombatStyle.LoadSynchronous();
    const FReferenceSkeleton* Source=Stance && Stance->GetSkeleton()? &Stance->GetSkeleton()->GetReferenceSkeleton():nullptr;
    Pose.SetNum(Ref.GetNum()); StanceRotations.SetNum(Ref.GetNum()); UpperBody.Init(false,Ref.GetNum());
    for (int32 I=0;I<Ref.GetNum();++I)
    {
        const int32 Parent=Ref.GetParentIndex(I);
        const FString BoneName=Ref.GetBoneName(I).ToString();
        UpperBody[I]=(I==Root || (Parent!=INDEX_NONE && UpperBody[Parent])) &&
            !BoneName.StartsWith(TEXT("neck")) && !BoneName.StartsWith(TEXT("head"));
        StanceRotations[I]=Ref.GetRefBonePose()[I].GetRotation();
        const int32 SourceIndex=Source?Source->FindBoneIndex(Ref.GetBoneName(I)):INDEX_NONE;
        if (!UpperBody[I] || SourceIndex==INDEX_NONE) continue;
        FTransform Sample;
        Stance->GetBoneTransform(Sample,FSkeletonPoseBoneIndex(SourceIndex),
            FAnimExtractContext(static_cast<double>(FMath::Clamp(PoseTime,0.0f,static_cast<float>(Stance->GetPlayLength())))),false);
        // Rotation-only transfer preserves the target character's bone lengths.
        StanceRotations[I]=(Sample.GetRotation()*Source->GetRefBonePose()[SourceIndex].GetRotation().Inverse()*StanceRotations[I]).GetNormalized();
    }
    if (!Source) UE_LOG(LogTemp,Warning,TEXT("Combat: stance unavailable; reference upper-body pose will be used."));
    bPoseReady=true;
    CacheAttackPoses();
    CacheRig();
}
void UDirectionalCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(UDirectionalCombatComponent,bUseMotionCoreV3,COND_InitialOnly);
    DOREPLIFETIME(UDirectionalCombatComponent,MotionCoreStyle);
    DOREPLIFETIME(UDirectionalCombatComponent,GripMode);
    DOREPLIFETIME_CONDITION(UDirectionalCombatComponent,bCombatActive,COND_SkipOwner);
    DOREPLIFETIME_CONDITION(UDirectionalCombatComponent,EquippedSword,COND_SkipOwner);
    DOREPLIFETIME_CONDITION(UDirectionalCombatComponent,AimOffset,COND_SkipOwner);
    DOREPLIFETIME_CONDITION(UDirectionalCombatComponent,AttackMotion,COND_SkipOwner);
    DOREPLIFETIME(UDirectionalCombatComponent,BodyAttack);
    DOREPLIFETIME(UDirectionalCombatComponent,bUseIndicatorDefense);
    DOREPLIFETIME(UDirectionalCombatComponent,DefenseThreats);
    DOREPLIFETIME(UDirectionalCombatComponent,DefenseDurability);
    DOREPLIFETIME(UDirectionalCombatComponent,LastDefenseResult);
    DOREPLIFETIME(UDirectionalCombatComponent,LibraryActionSource);
    DOREPLIFETIME(UDirectionalCombatComponent,LibraryActionVariantId);
    DOREPLIFETIME(UDirectionalCombatComponent,LibraryActionStart);
    DOREPLIFETIME(UDirectionalCombatComponent,LibraryActionSerial);
    DOREPLIFETIME(UDirectionalCombatComponent,LibraryActionDirection);
    DOREPLIFETIME(UDirectionalCombatComponent,PendingAttackFacing);
    DOREPLIFETIME(UDirectionalCombatComponent,MotionReaction);
    DOREPLIFETIME(UDirectionalCombatComponent,PhysicalContact);
    DOREPLIFETIME(UDirectionalCombatComponent,bAIControlled);
    DOREPLIFETIME(UDirectionalCombatComponent,Rig);
    DOREPLIFETIME_CONDITION(UDirectionalCombatComponent,bBlocking,COND_SkipOwner);
    DOREPLIFETIME(UDirectionalCombatComponent,Reaction);
    DOREPLIFETIME_CONDITION(UDirectionalCombatComponent,CurrentGuard,COND_SkipOwner);
}
void UDirectionalCombatComponent::ServerSetCombat_Implementation(bool bEnabled,FSwordInstance Sword,FVector_NetQuantize10 Aim,FCombatRig Geometry)
{
    if (bEnabled && Reaction.bDisarmed && CombatTime()<Reaction.EndTime) return;
    if (Geometry.Shoulder.ContainsNaN() || !FMath::IsFinite(Geometry.ArmLength) || !FMath::IsFinite(Geometry.PixelScale) || !FMath::IsFinite(Geometry.HeadHeight) || !FMath::IsFinite(Geometry.PelvisHeight)) return;
    Geometry.Shoulder=Geometry.Shoulder.GetClampedToMaxSize(300);
    Geometry.ArmLength=FMath::Clamp(Geometry.ArmLength,10.0f,200.0f);
    Geometry.PixelScale=FMath::Clamp(Geometry.PixelScale,0.05f,2.0f);
    Geometry.HeadHeight=FMath::Clamp(Geometry.HeadHeight,-100.0f,300.0f);
    Geometry.PelvisHeight=FMath::Clamp(Geometry.PelvisHeight,-200.0f,Geometry.HeadHeight-10);
    Rig=Geometry;
    if (bEnabled && Reaction.bDisarmed) { Reaction.bDisarmed=false; ++Reaction.Serial; }
    if (!bEnabled || EquippedSword.InstanceId!=Sword.InstanceId)
    {
        BodyAttack.bPrepared=false;
        CancelInertiaMovement();
        if (MotionWeaponAdapter) MotionWeaponAdapter->Stop();
    }
    bCombatActive=bEnabled; EquippedSword=Sword;
    if (!bEnabled) { AttackMotion.bPrepared=false; AttackMotion.bReleased=false; bBlocking=false; }
    if (!FVector(Aim).ContainsNaN()) AimOffset=FVector(Aim).GetClampedToMaxSize(5000);
    GetOwner()->ForceNetUpdate();
}
void UDirectionalCombatComponent::ServerSetAim_Implementation(FVector_NetQuantize10 Aim)
{ if (bCombatActive && !FVector(Aim).ContainsNaN()) AimOffset=FVector(Aim).GetClampedToMaxSize(5000); }
void UDirectionalCombatComponent::PublishState()
{
    if (GetOwner()->HasAuthority()) GetOwner()->ForceNetUpdate();
    else ServerSetCombat(bCombatActive,EquippedSword,AimOffset,Rig);
}
void UDirectionalCombatComponent::ToggleCombat()
{
    if (!IsLocal()) return;
    if (bCombatActive) { LeaveCombat(); return; }
    BindComponents();
    AProceduralSword* Sword=Hands?Cast<AProceduralSword>(Hands->GetItem(0)):nullptr;
    if (!Sword || (!bUseMotionCoreV3 && !bPoseReady)) return;
    if (Reaction.bDisarmed && CombatTime()<Reaction.EndTime) return;
    if (Reaction.bDisarmed)
    {
        Reaction.bDisarmed=false;
        if (GetOwner()->HasAuthority()) ++Reaction.Serial;
    }
    Rig.PixelScale=Sword->WorldUnitsPerPixel;
    Hands->CancelCharge();
    bCombatActive=true; EquippedSword=Sword->Definition; LastSword=Sword;
    SmoothedDirection=FVector(GetOwner()->GetActorForwardVector().X>=0?1:-1,0,0.3).GetSafeNormal();
    AimOffset=SmoothedDirection*200; PublishState();
    UpdateWeapon(0);
}
void UDirectionalCombatComponent::LeaveCombat()
{
    if (!bCombatActive) return;
    if (MotionWeaponAdapter) MotionWeaponAdapter->Stop();
    CancelInertiaMovement();
    CancelAttack();
    bBlocking=false;
    bCombatActive=false; if (IsLocal()) PublishState();
}
void UDirectionalCombatComponent::UpdateRemoteSword()
{
    auto* NativeInventory=GetOwner()->FindComponentByClass<USirrohASTInventoryComponent>();
    if(NativeInventory)
    {
        if(auto* Native=Cast<AProceduralSword>(NativeInventory->GetNativeItem(0)))
        {
            RemoteSword=Native;return;
        }
        if(!bAIControlled)return;
    }
    if (IsLocal() && !bAIControlled) return;
    const bool Carry=bUseMotionCoreV3 && !MotionCoreStyle.MotionLibrary.IsNull() && EquippedSword.InstanceId.IsValid();
    if (RemoteSword && ((((!bCombatActive && !Carry) || Reaction.bDisarmed) && Alpha<=0) || RemoteSword->Definition.InstanceId!=EquippedSword.InstanceId))
    { RemoteSword->Destroy(); RemoteSword=nullptr; }
    if ((!bCombatActive && !Carry) || Reaction.bDisarmed || RemoteSword || GetNetMode()==NM_DedicatedServer) return;
    const FTransform Transform(GetOwner()->GetActorLocation());
    RemoteSword=GetWorld()->SpawnActorDeferred<AProceduralSword>(AProceduralSword::StaticClass(),Transform,GetOwner(),nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (RemoteSword)
    {
        RemoteSword->Definition=EquippedSword;
        RemoteSword->WorldUnitsPerPixel=Rig.PixelScale;
        UGameplayStatics::FinishSpawningActor(RemoteSword,Transform);
        RemoteSword->Take(GetOwner());
        if(NativeInventory && GetOwner()->HasAuthority())NativeInventory->SetNativeItem(0,RemoteSword);
    }
}
void UDirectionalCombatComponent::ApplyMoveScale(float Scale)
{
    if (UMoverComponent* Mover=GetOwner()->FindComponentByClass<UMoverComponent>())
        if (auto* Settings=Mover->FindSharedSettings_Mutable<UCommonLegacyMovementSettings>())
        {
            // An external locomotion mode may replace MaxSpeed. Never compound
            // yesterday's combat multiplier or restore over a newer setting.
            if (UnscaledMoveSpeed<0 || !FMath::IsNearlyEqual(Settings->MaxSpeed,LastAppliedMoveSpeed,0.01f))
                UnscaledMoveSpeed=Settings->MaxSpeed;
            float Desired=UnscaledMoveSpeed*Scale;
            if (bUseMotionCoreV3 && bCombatActive) Desired=FMath::Min(Desired,GetGuardWalkSpeed());
            Settings->MaxSpeed=Desired; LastAppliedMoveSpeed=Desired; AppliedMoveScale=Scale;
        }
}
void UDirectionalCombatComponent::TickComponent(float Delta,ELevelTick Tick,FActorComponentTickFunction* Function)
{
    Super::TickComponent(Delta,Tick,Function);
    UpdateIndicatorDefense();
    if (UGameplayStatics::IsGamePaused(this)) return;
    BindComponents();
    if (const auto* Health=GetOwner()->FindComponentByClass<UCombatVitalityComponent>(); Health && Health->IsRecoveryBlocked())
    {
        if (bMotionCoreValid) ResetMotionCore();
        if (PoseBridge) { PoseBridge->ClearCombatPose(); PoseBridge->ClearHandPose(); }
        return;
    }
    AProceduralSword* Sword=ActiveSword();
    if (bUseMotionCoreV3 && IsLocal() && !bAIControlled && !bCombatActive)
    {
        if (Sword && Sword->Definition.InstanceId!=EquippedSword.InstanceId)
        { EquippedSword=Sword->Definition; Rig.PixelScale=Sword->WorldUnitsPerPixel; LastSword=Sword; PublishState(); }
        else if (!Sword && EquippedSword.InstanceId.IsValid())
        { EquippedSword=FSwordInstance(); PublishState(); }
    }
    if (IsLocal() && !bAIControlled && bCombatActive)
    {
        if (!Sword) LeaveCombat();
        else
        {
            if (Sword!=LastSword.Get()) { CancelAttack(); bBlocking=false; LastSword=Sword; EquippedSword=Sword->Definition; Rig.PixelScale=Sword->WorldUnitsPerPixel; PublishState(); }
            UpdateCursorAim();
            SendElapsed+=Delta;
            // Periodic refresh also repairs a lost final unreliable aim packet.
            if (SendElapsed>=0.05f)
            {
                SendElapsed=0; LastSentAim=AimOffset;
                if (GetOwner()->HasAuthority()) GetOwner()->ForceNetUpdate(); else ServerSetAim(AimOffset);
            }
        }
    }
    if (bUseMotionCoreV3)
    {
        UpdateCombatStyles();
        TickMotionCore(Delta);
        return;
    }
    UpdateWeapon(Delta);
    Alpha=FMath::FInterpConstantTo(Alpha,bCombatActive && !Reaction.bDisarmed?1.0f:0.0f,Delta,1.0f/FMath::Max(0.01f,BlendSeconds));
    BlockAlpha=FMath::FInterpConstantTo(BlockAlpha,GetCombatPhase()==EDirectionalCombatPhase::Blocking?1.0f:0.0f,Delta,1.0f/FMath::Max(0.01f,BlendSeconds));
    ApplyMoveScale(bCombatActive?FMath::Clamp(CombatMoveSpeed*CombatSpeedMultiplier,0.1f,2.0f):1.0f);
    const float Speed=FVector::Dist2D(GetOwner()->GetActorLocation(),PreviousOwnerPosition)/FMath::Max(0.001f,Delta);
    PreviousOwnerPosition=GetOwner()->GetActorLocation();
    LowerBodyAlpha=FMath::FInterpTo(LowerBodyAlpha,Alpha*FMath::Clamp(1-Speed/120,0.25f,1.0f),Delta,8);
    UpdateRemoteSword();
    Sword=ActiveSword();
    if (PoseBridge) PoseBridge->ClearCombatPose();
    if (Sword && !Reaction.bDisarmed && (bCombatActive || Alpha>0))
    {
        const FCombatWeaponFrame Frame=GetWeaponFrame();
        Sword->SetCombatTransform(FTransform(Frame.Rotation,Frame.Grip));
        if (Alpha>0 && bPoseReady && GetNetMode()!=NM_DedicatedServer) ApplyPose(Delta,Sword);
    }
    if (Hands) Hands->UpdateBackItems();
}
void UDirectionalCombatComponent::ApplyPose(float Delta,AProceduralSword* Sword)
{
    if (!Mesh || !Mesh->GetSkeletalMeshAsset()) return;
    const FReferenceSkeleton& Ref=Mesh->GetSkeletalMeshAsset()->GetRefSkeleton();
    if (Ref.GetNum()!=Pose.Num()) { CacheStance(); return; }
    const float PreparationWeight=AttackPoseWeight();
    const int32 AttackIndex=FMath::Clamp(static_cast<int32>(AttackMotion.Type),0,2);
    const int32 GuardIndex=FMath::Clamp(static_cast<int32>(CurrentGuard),0,2);
    if (SmoothedGuardPose.Num()!=Ref.GetNum()) SmoothedGuardPose=StanceRotations;
    for (int32 I=0;I<Ref.GetNum();++I)
    {
        const int32 Parent=Ref.GetParentIndex(I);
        const FName Bone=Ref.GetBoneName(I);
        FTransform Live=Mesh->GetSocketTransform(Bone);
        if (PoseBridge)
        {
            FTransform HandPose;
            if (PoseBridge->TryGetHandBoneTransform(Bone,HandPose)) Live=HandPose;
        }
        if (!UpperBody[I] || Parent==INDEX_NONE) { Pose[I]=Live; continue; }
        const FName ParentName=Ref.GetBoneName(Parent);
        FTransform ParentLive=Mesh->GetSocketTransform(ParentName);
        if (PoseBridge)
        {
            FTransform HandParent;
            if (PoseBridge->TryGetHandBoneTransform(ParentName,HandParent)) ParentLive=HandParent;
        }
        FTransform Local=Live.GetRelativeTransform(ParentLive);
        const FQuat GuardRotation=GuardRotations[GuardIndex].IsValidIndex(I)?GuardRotations[GuardIndex][I]:StanceRotations[I];
        SmoothedGuardPose[I]=FQuat::Slerp(SmoothedGuardPose[I],GuardRotation,1-FMath::Exp(-Delta/FMath::Max(0.05f,BlendSeconds))).GetNormalized();
        const auto& Ready=ReadyRotations[GuardIndex*3+AttackIndex];
        FQuat Desired=Ready.IsValidIndex(I)?FQuat::Slerp(SmoothedGuardPose[I],Ready[I],PreparationWeight).GetNormalized():SmoothedGuardPose[I];
        Local.SetRotation(FQuat::Slerp(Local.GetRotation(),Desired,Alpha).GetNormalized());
        Pose[I]=Local*Pose[Parent];
    }
    const int32 A=Ref.FindBoneIndex(TEXT("upperarm_r")), B=Ref.FindBoneIndex(TEXT("lowerarm_r")), C=Ref.FindBoneIndex(TEXT("hand_r"));
    const TArray<FTransform> BeforeCrouch=Pose;
    // Move the body down while feet retain their locomotion targets. Idle stance widens slightly;
    // during movement the existing foot animation remains the source of placement.
    for (FTransform& Bone:Pose) Bone.AddToTranslation(FVector(0,0,-StanceLowering*LowerBodyAlpha));
    const float Facing=GetOwner()->GetActorForwardVector().X>=0?1.0f:-1.0f;
    for (int32 Side=0;Side<2;++Side)
    {
        const int32 Hip=Ref.FindBoneIndex(Side==0?TEXT("thigh_r"):TEXT("thigh_l"));
        const int32 Knee=Ref.FindBoneIndex(Side==0?TEXT("calf_r"):TEXT("calf_l"));
        const int32 Foot=Ref.FindBoneIndex(Side==0?TEXT("foot_r"):TEXT("foot_l"));
        if (Hip==INDEX_NONE || Knee==INDEX_NONE || Foot==INDEX_NONE) continue;
        const FTransform OldHip=Pose[Hip],OldKnee=Pose[Knee],OldFoot=Pose[Foot];
        const float Idle=FMath::Clamp((LowerBodyAlpha-Alpha*0.25f)/0.75f,0.0f,1.0f);
        const FVector FootTarget=BeforeCrouch[Foot].GetLocation()+FVector(Facing*(Side==0?8:-8)*Idle,0,0);
        AnimationCore::SolveTwoBoneIK(Pose[Hip],Pose[Knee],Pose[Foot],Pose[Hip].GetLocation()+FVector(Facing*80,0,-20),FootTarget,false,1.0,1.0);
        Pose[Foot].SetRotation(BeforeCrouch[Foot].GetRotation());
        for (int32 I=0;I<Pose.Num();++I)
        {
            if (I==Hip || I==Knee || I==Foot) continue;
            int32 Parent=Ref.GetParentIndex(I);
            while (Parent!=INDEX_NONE && Parent!=Hip && Parent!=Knee && Parent!=Foot) Parent=Ref.GetParentIndex(Parent);
            if (Parent!=INDEX_NONE) Pose[I]=Pose[I].GetRelativeTransform(Parent==Hip?OldHip:Parent==Knee?OldKnee:OldFoot)*Pose[Parent];
        }
    }
    // Small distributed visual lean; pelvis and legs retain locomotion.
    const FCombatWeaponFrame Frame=GetWeaponFrame();
    const float Twist=FMath::Clamp(static_cast<float>((Frame.Grip.Y-GetOwner()->GetActorLocation().Y-Rig.Shoulder.Y)/FMath::Max(10.0f,Rig.ArmLength)),-0.6f,0.6f)*Facing*Alpha;
    const FQuat Lean=FQuat(FVector::ZAxisVector,Twist)*FQuat(FVector::YAxisVector,FMath::DegreesToRadians(MaxTorsoLeanDegrees)*Facing*0.35f*Alpha);
    const FVector Pivot=Pose[Ref.FindBoneIndex(TEXT("spine_01"))].GetLocation();
    for (int32 I=0;I<Pose.Num();++I) if (UpperBody[I])
    { Pose[I].SetLocation(Pivot+Lean.RotateVector(Pose[I].GetLocation()-Pivot)); Pose[I].SetRotation(Lean*Pose[I].GetRotation()); }
    const double Length=FVector::Distance(Pose[A].GetLocation(),Pose[B].GetLocation())+FVector::Distance(Pose[B].GetLocation(),Pose[C].GetLocation());
    FTransform Grip(Frame.Rotation*Sword->SocketRotationOffset.Quaternion().Inverse(),Frame.Grip);
    const FTransform SocketLocal=Mesh->DoesSocketExist(TEXT("Weapon_R"))?
        Mesh->GetSocketTransform(TEXT("Weapon_R")).GetRelativeTransform(Mesh->GetSocketTransform(TEXT("hand_r"))):FTransform::Identity;
    FTransform Hand=SocketLocal.Inverse()*Grip;
    FVector ToHand=Hand.GetLocation()-Pose[A].GetLocation();
    Hand.SetLocation(Pose[A].GetLocation()+ToHand.GetClampedToMaxSize(Length*0.97));
    const FTransform OldA=Pose[A],OldB=Pose[B],OldC=Pose[C];
    const FVector Target=FMath::Lerp(OldC.GetLocation(),Hand.GetLocation(),static_cast<double>(Alpha));
    const FVector ElbowPole=OldB.GetLocation()+FVector(Facing*10,-20,CurrentGuard==ECombatGuard::Up?15:-20);
    AnimationCore::SolveTwoBoneIK(Pose[A],Pose[B],Pose[C],ElbowPole,Target,false,1.0,1.0);
    Pose[C].SetRotation(FQuat::Slerp(OldC.GetRotation(),Hand.GetRotation(),Alpha).GetNormalized());
    // Carry twist and finger descendants along with the solved three-bone chain.
    for (int32 I=0;I<Pose.Num();++I)
    {
        if (I==A || I==B || I==C) continue;
        int32 Ancestor=Ref.GetParentIndex(I);
        while (Ancestor!=INDEX_NONE && Ancestor!=A && Ancestor!=B && Ancestor!=C) Ancestor=Ref.GetParentIndex(Ancestor);
        if (Ancestor!=INDEX_NONE)
            Pose[I]=Pose[I].GetRelativeTransform(Ancestor==A?OldA:Ancestor==B?OldB:OldC)*Pose[Ancestor];
    }
    CombatPose.Reset();
    for (int32 I=0;I<Pose.Num();++I)
    {
        if (UpperBody[I] || LowerBodyAlpha>0.001f) CombatPose.Add(Ref.GetBoneName(I),Pose[I]);
    }
    if (PoseBridge) PoseBridge->PublishCombatPose(CombatPose);
    if (!bCombatActive) Sword->SetHeldTransform(SocketLocal*Pose[C]);
    // The passive left hand may still contain an item from the placeholder inventory.
    if (IsLocal() && Hands && IsValid(Hands->GetItem(1)))
    {
        const int32 Left=Ref.FindBoneIndex(TEXT("hand_l"));
        if (Left!=INDEX_NONE)
        {
            const FName Socket=Hands->GetItem(1)->HandSocket(1);
            const FTransform Local=!Socket.IsNone() && Mesh->DoesSocketExist(Socket)?
                Mesh->GetSocketTransform(Socket).GetRelativeTransform(Mesh->GetSocketTransform(TEXT("hand_l"))):FTransform(FQuat::Identity,Hands->HandItemOffset);
            Hands->GetItem(1)->SetHeldTransform(Local*Pose[Left]);
        }
    }
}
void UDirectionalCombatComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if (auto* Contacts=GetWorld()->GetSubsystem<UCombatContactSubsystem>()) Contacts->UnregisterCombatant(this);
    ApplyMoveScale(1);
    if (PoseBridge) { PoseBridge->ClearCombatPose(); PoseBridge->RemoveTickPrerequisiteComponent(this); }
    GetWorld()->GetSubsystem<UCombatMotionContactSubsystem>()->UnregisterCombatant(this);
    CancelInertiaMovement();
    RemoveStyleControls();
    RemoveDefenseOverlay();
    if (MotionWeaponAdapter) MotionWeaponAdapter->DestroyComponent();
    if (MotionBodyAdapter) MotionBodyAdapter->DestroyComponent();
    if (RemoteSword) RemoteSword->Destroy();
    Super::EndPlay(Reason);
}
