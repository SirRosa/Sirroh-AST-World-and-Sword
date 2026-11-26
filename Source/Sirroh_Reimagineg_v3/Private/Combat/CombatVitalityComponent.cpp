#include "Combat/CombatVitalityComponent.h"
#include "Combat/CombatReactionCatalog.h"
#include "Combat/DirectionalCombatComponent.h"
#include "Character/SirrohPoseBridgeComponent.h"
#include "PWG/PWGGameMode.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "MoverComponent.h"
#include "Animation/AnimInstance.h"
#include "Engine/SkeletalMesh.h"
#include "PhysicsEngine/BodyInstance.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"
#include "Net/UnrealNetwork.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Notifications/SProgressBar.h"

UCombatVitalityComponent::UCombatVitalityComponent()
{
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick=true;
    PrimaryComponentTick.TickInterval=0.1f;
    PrimaryComponentTick.TickGroup=TG_PostPhysics;
    const FName Bones[]={TEXT("spine_03"),TEXT("head"),TEXT("upperarm_l"),TEXT("lowerarm_l"),TEXT("hand_l"),
        TEXT("upperarm_r"),TEXT("lowerarm_r"),TEXT("hand_r"),TEXT("thigh_l"),TEXT("calf_l"),TEXT("foot_l"),
        TEXT("thigh_r"),TEXT("calf_r"),TEXT("foot_r")};
    const int32 Parents[]={-1,0,0,2,3,0,5,6,0,8,9,0,11,12};
    for (int32 I=0;I<UE_ARRAY_COUNT(Bones);++I)
    {
        auto& P=Parts.AddDefaulted_GetRef(); P.Bone=Bones[I]; P.Parent=Parents[I];
        P.Maximum=I==0?80.f:(I==1?40.f:30.f); P.Integrity=P.Maximum;
    }
}
UCombatVitalityComponent* UCombatVitalityComponent::FindOrAdd(AActor* Owner)
{
    if (!Owner) return nullptr;
    if (auto* Existing=Owner->FindComponentByClass<UCombatVitalityComponent>()) return Existing;
    if (!Owner->HasAuthority()) return nullptr;
    auto* Result=NewObject<UCombatVitalityComponent>(Owner,TEXT("CombatVitality"));
    Owner->AddInstanceComponent(Result); Result->RegisterComponent(); return Result;
}
void UCombatVitalityComponent::BeginPlay()
{
    Super::BeginPlay();
    Mesh=GetOwner()->FindComponentByClass<USkeletalMeshComponent>();
    if (Mesh) { MeshRelative=Mesh->GetRelativeTransform(); MeshCollisionProfile=Mesh->GetCollisionProfileName(); Mesh->bEnablePhysicsOnDedicatedServer=true; }
    PreviousPosition=GetOwner()->GetActorLocation();
    if (GetOwner()->HasAuthority()) Blood=MaxBlood;
}
void UCombatVitalityComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UCombatVitalityComponent,Blood);
    DOREPLIFETIME(UCombatVitalityComponent,MaxBlood);
    DOREPLIFETIME(UCombatVitalityComponent,LifeState);
    DOREPLIFETIME(UCombatVitalityComponent,Parts);
    DOREPLIFETIME(UCombatVitalityComponent,LifeId);
    DOREPLIFETIME(UCombatVitalityComponent,KnockdownUntil);
    DOREPLIFETIME(UCombatVitalityComponent,RagdollFrame);
    DOREPLIFETIME(UCombatVitalityComponent,DeathAnimationRow);
    DOREPLIFETIME(UCombatVitalityComponent,DeathServerTime);
}
int32 UCombatVitalityComponent::FindPart(FName Bone) const
{
    if (Bone==TEXT("pelvis") || Bone.ToString().StartsWith(TEXT("spine"))) return 0;
    if (Bone==TEXT("neck_01")) return 1;
    for (FName Candidate=Bone;!Candidate.IsNone();Candidate=Mesh?Mesh->GetParentBone(Candidate):NAME_None)
    {
        const int32 Index=Parts.IndexOfByPredicate([Candidate](const auto& P){return P.Bone==Candidate;});
        if (Index!=INDEX_NONE) return Index;
    }
    return INDEX_NONE;
}
bool UCombatVitalityComponent::IsDescendant(int32 Child,int32 Parent) const
{
    for (int32 I=Child;Parts.IsValidIndex(I);I=Parts[I].Parent) if (I==Parent) return true;
    return false;
}
float UCombatVitalityComponent::GetBleeding() const
{
    float Total=0; for (const auto& P:Parts) Total+=P.Bleeding; return Total;
}
bool UCombatVitalityComponent::IsIncapacitated() const
{
    if (LifeState!=ECombatLifeState::Conscious || ServerTime()<KnockdownUntil) return true;
    for (int32 I=8;I<Parts.Num();++I) if (Parts[I].bDestroyed) return true;
    return false;
}
bool UCombatVitalityComponent::CanUseArm(bool bLeft) const
{
    if (IsRecoveryBlocked()) return false;
    const int32 Start=bLeft?2:5;
    for (int32 I=Start;I<Start+3;++I) if (Parts[I].bDestroyed) return false;
    return true;
}
bool UCombatVitalityComponent::UsesRagdollPose() const
{
    const auto* Mover=GetOwner()->FindComponentByClass<UMoverComponent>();
    return IsIncapacitated() || (Mover && Mover->GetMovementModeName()==TEXT("Ragdoll"));
}
bool UCombatVitalityComponent::IsRecoveryBlocked() const
{
    return UsesRagdollPose() || bDownApplied || bRecovering;
}
bool UCombatVitalityComponent::IsRegionPresent(FName Bone) const
{
    const int32 Index=FindPart(Bone);
    return Index==INDEX_NONE || !Parts[Index].bSevered;
}
FVector UCombatVitalityComponent::GetTargetPoint() const
{
    if (Mesh && IsIncapacitated()) return Mesh->GetSocketLocation(TEXT("spine_03"));
    return GetOwner()->GetActorLocation();
}
void UCombatVitalityComponent::DestroyPart(int32 Index,bool bSever)
{
    for (int32 I=0;I<Parts.Num();++I) if (IsDescendant(I,Index))
    {
        auto& P=Parts[I]; P.Integrity=0; P.bDestroyed=true; P.Regrowth=0;
        if (bSever) { P.bSevered=true; if (I!=Index) { Parts[Index].Bleeding+=P.Bleeding; P.Bleeding=0; } }
    }
    if (bSever) Parts[Index].Bleeding+=10;
}
float UCombatVitalityComponent::ApplyContact(const FCombatMotionBodyHit& Hit,float WeaponMass)
{
    if (!GetOwner()->HasAuthority() || LifeState==ECombatLifeState::Dead || Hit.Target!=GetOwner()
        || !FMath::IsFinite(Hit.RelativeSpeed) || Hit.RelativeSpeed<=0) return 0;
    const int32 Index=FindPart(Hit.BodyRegion);
    if (Index==INDEX_NONE || Parts[Index].bSevered) return 0;
    const bool Unarmed=Hit.DamageKind==ECombatDamageKind::Unarmed;
    const bool Kick=Unarmed || Hit.DamageKind==ECombatDamageKind::Blunt || Hit.Action==TEXT("Kick");
    const bool Thrust=!Kick && Hit.AttackType==EDirectionalAttack::Pierce;
    const float Multiplier=Kick?0.5f:(Thrust?1.2f:(Hit.AttackType==EDirectionalAttack::Heavy?1.5f:1.f));
    const float Mass=Kick?1.f:((FMath::IsFinite(WeaponMass)&&WeaponMass>0)?WeaponMass:1.f);
    const float Damage=(Unarmed?Hit.AuthoredDamage:Hit.RelativeSpeed*0.01f*Mass*Multiplier)*FMath::Clamp(Hit.DamageScale,0.f,1.f);
    if (!FMath::IsFinite(Damage) || Damage<=0) return 0;
    const bool WasDown=IsIncapacitated();
    LastIncoming=GetOwner()->GetActorQuat().UnrotateVector(-Hit.ContactVelocity).GetSafeNormal();
    LastRegion=Index==1?(Hit.BodyRegion.ToString().Contains(TEXT("neck"))?TEXT("Neck"):TEXT("Head")):
        Index>=8?TEXT("Legs"):TEXT("Torso");
    if (auto* Combat=GetOwner()->FindComponentByClass<UDirectionalCombatComponent>()) LastHands=Combat->IsTwoHandedGrip()?2:1;
    bLastPierce=Thrust; bLastCut=!Kick&&!Thrust;
    LastImpactStrength=FMath::Clamp(Damage/FMath::Max(1.f,KnockdownDamage),0.f,1.f);
    if (Unarmed && !WasDown)
    {
        UnarmedStagger=ServerTime()-LastUnarmedImpact>UnarmedComboWindow?0:UnarmedStagger;
        UnarmedStagger+=Damage;LastUnarmedImpact=ServerTime();
        if (UnarmedStagger>=UnarmedKnockdownThreshold)
        { KnockdownUntil=ServerTime()+KnockdownSeconds;UnarmedStagger=0; }
    }
    LastImpactTime=ServerTime();
    if (!Unarmed && Damage>=KnockdownDamage) KnockdownUntil=FMath::Max(KnockdownUntil,ServerTime()+KnockdownSeconds);
    auto& Part=Parts[Index];
    // Blunt unarmed attacks cannot destroy organs/limbs or drain a healthy target into permanent unconsciousness.
    // Min preserves pre-existing wounds/low blood; hitting somebody never heals them.
    Part.Integrity=Unarmed?FMath::Min(Part.Integrity,FMath::Max(1.f,Part.Integrity-Damage)):FMath::Max(0.f,Part.Integrity-Damage);
    Blood=Unarmed?FMath::Min(Blood,FMath::Max(MaxBlood*0.15f,Blood-Damage)):FMath::Max(0.f,Blood-Damage);
    if (!Kick) Part.Bleeding+=Damage*CutBleedPerDamage*(Thrust?ThrustBleedMultiplier:1.f);
    if (!Unarmed && Part.Integrity<=0) DestroyPart(Index,!Kick && !Thrust && Index>1);
    const bool Fatal=Blood<=0 || Parts[0].bDestroyed || Parts[1].bDestroyed;
    if (!Fatal && (WasDown || IsIncapacitated())) SelectImpactAnimation(WasDown?TEXT("Ground"):TEXT("KO"));
    UpdateLifeState(); OnRep_Parts(); GetOwner()->ForceNetUpdate();
    if (auto* Combat=GetOwner()->FindComponentByClass<UDirectionalCombatComponent>()) Combat->ApplyVitalityRestrictions();
    return Damage;
}
void UCombatVitalityComponent::UpdateLifeState()
{
    const auto Previous=LifeState;
    if (Blood<=0 || Parts[0].bDestroyed || Parts[1].bDestroyed) LifeState=ECombatLifeState::Dead;
    else if (Blood<MaxBlood*0.1f) LifeState=ECombatLifeState::Unconscious;
    else if (LifeState==ECombatLifeState::Unconscious && Blood>=MaxBlood*0.15f) LifeState=ECombatLifeState::Conscious;
    if (LifeState==ECombatLifeState::Dead && DeathTime<0)
    {
        DeathTime=GetWorld()->GetTimeSeconds();
        DeathServerTime=GetWorld()->GetGameState()?GetWorld()->GetGameState()->GetServerWorldTimeSeconds():DeathTime;
        if (!bDownApplied) SelectDeathAnimation();
        else DeathAnimationRow=NAME_None; // A dead, already fallen body must not continue struggle/get-hit animation.
    }
    if (Previous!=LifeState) { OnRep_State(); GetOwner()->ForceNetUpdate(); }
}
void UCombatVitalityComponent::StepWounds()
{
    const float Total=GetBleeding();
    // Discrete user contract: 10 + 9 + ... + 1 = 55, closure follows blood loss.
    Blood=FMath::Max(0.f,Blood-Total);
    const float Closure=WoundClosurePerSecond*(1-FMath::Clamp(MovingSeconds,0.f,1.f)*(1-MovingClosureMultiplier));
    if (Total>0)
        for (auto& P:Parts) P.Bleeding=FMath::Max(0.f,P.Bleeding*(1-FMath::Min(Total,Closure)/Total));
    else if (MovingSeconds<0.1f && bSupported)
    {
        Blood=FMath::Min(MaxBlood,Blood+BloodRecoveryPerSecond);
        for (auto& P:Parts)
        {
            if (P.Parent!=INDEX_NONE && Parts[P.Parent].bDestroyed) continue;
            if (P.bDestroyed)
            {
                P.Regrowth+=1.f/FMath::Max(1.f,RegrowthSeconds);
                if (P.Regrowth>=1) { P.bDestroyed=false; P.bSevered=false; P.Integrity=P.Maximum; P.Regrowth=0; }
            }
            else P.Integrity=FMath::Min(P.Maximum,P.Integrity+0.5f);
        }
    }
    MovingSeconds=0; UpdateLifeState(); OnRep_Parts();
}
void UCombatVitalityComponent::OnRep_State()
{
    if (LifeState==ECombatLifeState::Dead && Mesh && !bDownApplied) DeathAnchor=Mesh->GetComponentTransform();
    UpdatePresentation();
}
void UCombatVitalityComponent::OnRep_Parts()
{
    UpdatePresentation();
    // HideBone affects presentation only. Remove all physical descendants from collision too.
    if (Mesh)
        for (int32 I=0;I<Mesh->GetNumBones();++I)
        {
            const FName Bone=Mesh->GetBoneName(I);
            auto* Body=Mesh->GetBodyInstance(Bone);
            if (!Body) continue;
            if (!IsRegionPresent(Bone))
            {
                if (!DisabledBodies.Contains(Bone)) DisabledBodies.Add(Bone,Body->GetCollisionEnabled());
                Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            }
            else if (auto* Original=DisabledBodies.Find(Bone))
            { Body->SetCollisionEnabled(*Original); DisabledBodies.Remove(Bone); }
        }
    if (auto* Bridge=GetOwner()->FindComponentByClass<USirrohPoseBridgeComponent>())
        if (auto* Visual=Bridge->GetVisualMesh())
            for (const auto& P:Parts)
            {
                if (P.bSevered) Visual->HideBoneByName(P.Bone,EPhysBodyOp::PBO_None);
                else Visual->UnHideBoneByName(P.Bone);
            }
}
void UCombatVitalityComponent::UpdatePresentation()
{
    const bool Down=IsIncapacitated();
    const bool Dead=LifeState==ECombatLifeState::Dead;
    const bool DeathTransition=Dead && !bDeathApplied;
    auto* Mover=GetOwner()->FindComponentByClass<UMoverComponent>();
    if (bRecovering && !Down && Mover && Mover->GetMovementModeName()!=TEXT("Ragdoll")
        && ServerTime()-RecoveryStartedAt>0.2 && (!Mesh || !Mesh->GetAnimInstance() || !Mesh->GetAnimInstance()->IsAnyMontagePlaying()))
        bRecovering=false;
    // Do not stand a still falling/sliding body upright. AST's exit event selects the GASP get-up montage.
    if (!Down && bDownApplied && Mover && Mesh && Mesh->IsAnySimulatingPhysics()
        && (!bSupported || Mesh->GetPhysicsLinearVelocity(TEXT("pelvis")).SizeSquared()>FMath::Square(150.f))) return;
    if (Down==bDownApplied && !DeathTransition)
    {
        // A mesh/physics-state refresh must not silently revive a downed non-Mover NPC.
        // Sleeping bodies still report simulation; this does not wake settled corpses every tick.
        if(Down && bFallbackPhysics && Mesh && Mesh->GetPhysicsAsset() && !Mesh->IsAnySimulatingPhysics()){
            Mesh->SetCollisionProfileName(TEXT("Ragdoll"));Mesh->SetSimulatePhysics(true);
            Mesh->SetAllBodiesSimulatePhysics(true);Mesh->SetAllBodiesPhysicsBlendWeight(1.f);Mesh->WakeAllRigidBodies();
            UE_LOG(LogTemp,Warning,TEXT("Restored interrupted fallback ragdoll for %s"),*GetNameSafe(GetOwner()));
        }
        return;
    }
    // PhysicsControl owns the Mover body's targets. Teleporting its bodies to a procedural
    // render pose after simulation has started produces constraint/depenetration impulses.
    if (Down && !bDownApplied && Mesh && !Mover)
        if (auto* Bridge=GetOwner()->FindComponentByClass<USirrohPoseBridgeComponent>())
            if (auto* Visual=Bridge->GetVisualMesh())
                for (int32 I=0;I<Mesh->GetNumBones();++I)
                    if (Mesh->GetBodyInstance(Mesh->GetBoneName(I)))
                        PendingPhysicalPose.Add(Mesh->GetBoneName(I),Visual->GetBoneTransformByName(Mesh->GetBoneName(I),EBoneSpaces::WorldSpace));
    if (!Down) { PendingPhysicalPose.Reset(); PendingImpulse=FVector::ZeroVector; }
    // Death must replace the stunned/get-up setup even if the body was already down.
    bDeathApplied=Dead;
    bDownApplied=Down;
    if (auto* Pawn=Cast<APawn>(GetOwner()))
        if (auto* PC=Cast<APlayerController>(Pawn->GetController()))
        {
            // The Pawn's Enhanced Input also has a manual get-up action.
            if (Down) Pawn->DisableInput(PC); else Pawn->EnableInput(PC);
        }
    if (auto* Combat=GetOwner()->FindComponentByClass<UDirectionalCombatComponent>())
    { Combat->CancelAttack(); if (Down) Combat->ApplyVitalityRestrictions(); }
    if (auto* Bridge=GetOwner()->FindComponentByClass<USirrohPoseBridgeComponent>())
    { Bridge->ClearCombatPose(); Bridge->ClearHandPose(); }
    if (Down) bRecovering=false;
    else if (Mover) { bRecovering=true; RecoveryStartedAt=ServerTime(); }
    if (Mover && Mover->MovementModes.Contains(TEXT("Ragdoll")))
    {
        // Use the verified child Blueprint entry/exit events, including its PhysicsControl setup.
        UFunction* Event=GetOwner()->FindFunction(Down?TEXT("TriggerRagdoll"):TEXT("ExitRagdoll"));
        if (Event)
        {
            FStructOnScope Args(Event);
            if (Down)
            {
                if (auto* Stop=FindFProperty<FBoolProperty>(Event,TEXT("StopActiveMontages")))
                    Stop->SetPropertyValue_InContainer(Args.GetStructMemory(),true);
                if (auto* Injury=FindFProperty<FByteProperty>(Event,TEXT("Ragdoll_InjuryState")); Injury && Injury->Enum)
                    for (int32 I=0;I<Injury->Enum->NumEnums();++I)
                        if (Injury->Enum->GetDisplayNameTextByIndex(I).ToString()==(LifeState==ECombatLifeState::Conscious && ServerTime()<KnockdownUntil?TEXT("Stunned"):TEXT("Limp")))
                            Injury->SetPropertyValue_InContainer(Args.GetStructMemory(),uint8(Injury->Enum->GetValueByIndex(I)));
            }
            GetOwner()->ProcessEvent(Event,Args.GetStructMemory());
        }
        else Mover->QueueNextMode(Down?FName(TEXT("Ragdoll")):Mover->StartingMovementMode);
    }
    else if (Mesh && Mesh->GetPhysicsAsset())
    {
        // Training pawn has no Mover; only this fallback owns its mesh simulation.
        if (Down)
        {
            bFallbackPhysics=true;
            Mesh->SetCollisionProfileName(TEXT("Ragdoll"));
            Mesh->SetSimulatePhysics(true);
            Mesh->SetAllBodiesSimulatePhysics(true); Mesh->SetAllBodiesPhysicsBlendWeight(1.f); Mesh->WakeAllRigidBodies();
        }
        else if (bFallbackPhysics)
        {
            Mesh->SetSimulatePhysics(false);
            Mesh->SetAllBodiesSimulatePhysics(false);
            Mesh->AttachToComponent(GetOwner()->GetRootComponent(),FAttachmentTransformRules::KeepRelativeTransform);
            Mesh->SetRelativeTransform(MeshRelative);
            Mesh->SetCollisionProfileName(MeshCollisionProfile); bFallbackPhysics=false;
        }
    }
}
void UCombatVitalityComponent::ResetVitality()
{
    if (!GetOwner()->HasAuthority()) return;
    UnarmedStagger=0;LastUnarmedImpact=-100;
    ++LifeId; KnockdownUntil=0; RagdollFrame=FCombatRagdollFrame(); ReceivedRagdollFrame=PreviousRagdollFrame=FCombatRagdollFrame(); Blood=MaxBlood; LifeState=ECombatLifeState::Conscious; DeathTime=-1;
    DeathAnimationRow=LoadedDeathAnimation=NAME_None; DeathServerTime=-1; DeathRig=FCombatBodyRig();
    WoundClock=MovingSeconds=0;
    for (auto& P:Parts) { P.Integrity=P.Maximum; P.Bleeding=0; P.bDestroyed=P.bSevered=false; P.Regrowth=0; }
    OnRep_State(); OnRep_Parts(); GetOwner()->ForceNetUpdate();
}

void UCombatVitalityComponent::SelectImpactAnimation(FName Action)
{
    if (!Mesh) return;
    const auto* ProfileCombat=GetOwner()->FindComponentByClass<UDirectionalCombatComponent>();
    DeathAnimationRow=CombatReactions::Select(Action,LastRegion,LastIncoming,LastHands,LastImpactStrength,
        bLastPierce,bLastCut,LifeId+uint32(ServerTime()*1000),DeathAnimationRow,true,ProfileCombat?&ProfileCombat->MotionCoreStyle:nullptr);
    DeathServerTime=ServerTime(); DeathAnchor=Mesh->GetComponentTransform();
    if (auto* Combat=GetOwner()->FindComponentByClass<UDirectionalCombatComponent>())
        if (const auto* Cached=Combat->GetReactionRig(DeathAnimationRow))
        { DeathRig=*Cached; LoadedDeathAnimation=DeathAnimationRow; }
}
void UCombatVitalityComponent::SelectDeathAnimation()
{
    if (ServerTime()-LastImpactTime>0.5) { bLastPierce=bLastCut=false; LastRegion=TEXT("Any"); }
    SelectImpactAnimation(TEXT("Death"));
}
void UCombatVitalityComponent::BlendDeathPose(UPoseableMeshComponent& Visual)
{
    if (DeathAnimationRow.IsNone() || !Mesh || !UsesRagdollPose()) return;
    const double Elapsed=ServerTime()-DeathServerTime;
    if (Elapsed<0) return;
    if (LoadedDeathAnimation!=DeathAnimationRow)
    {
        auto* Combat=GetOwner()->FindComponentByClass<UDirectionalCombatComponent>();
        const auto* Cached=Combat?Combat->GetReactionRig(DeathAnimationRow):nullptr;
        if (!Cached) return;
        DeathRig=*Cached; LoadedDeathAnimation=DeathAnimationRow;
    }
    if (DeathRig.Motions.IsEmpty() || !DeathRig.Motions[0].IsValid()) return;
    const double Duration=DeathRig.Motions[0].Length;
    if (Elapsed>=Duration) return;
    TArray<FTransform> Local,Component;
    if (!CombatBodySolver::SampleMotion(DeathRig,0,float(Elapsed/Duration),Local,Component)) return;
    const float Weight=0.65f*FMath::Min(float(Elapsed/0.08),1.f)*
        (1-FMath::SmoothStep(0.35f,1.f,float(Elapsed/Duration)));
    for (int32 I=0;I<Local.Num() && I<Visual.BoneSpaceTransforms.Num();++I)
    {
        if (DeathRig.Parents[I]==INDEX_NONE || I==DeathRig.Pelvis) continue;
        auto& T=Visual.BoneSpaceTransforms[I];
        T.SetRotation(FQuat::Slerp(T.GetRotation(),Local[I].GetRotation(),Weight).GetNormalized());
    }
}
void UCombatVitalityComponent::TickComponent(float Delta,ELevelTick Tick,FActorComponentTickFunction* Function)
{
    Super::TickComponent(Delta,Tick,Function);
    CreateHUD();
    UpdatePresentation();
    if (Mesh && UsesRagdollPose() && Mesh->IsAnySimulatingPhysics())
    {
        for (const auto& Entry:PendingPhysicalPose)
            if (auto* Body=Mesh->GetBodyInstance(Entry.Key); Body && Body->IsInstanceSimulatingPhysics())
                Body->SetBodyTransform(Entry.Value,ETeleportType::TeleportPhysics);
        PendingPhysicalPose.Reset();
        if (!PendingImpulse.IsNearlyZero() && ServerTime()<=PendingImpulseUntil)
        {
            // Distribute total momentum by body mass, preserving one common velocity change.
            const float Mass=FMath::Max(1.f,Mesh->GetMass());
            Mesh->AddImpulseToAllBodiesBelow((PendingImpulse/Mass).GetClampedToMaxSize(300.f),NAME_None,true,true);
            PendingImpulse=FVector::ZeroVector;
        }
    }
    CaptureNetworkRagdoll();
    for (const auto& Entry:DisabledBodies)
        if (Mesh) if (auto* Body=Mesh->GetBodyInstance(Entry.Key)) Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    if (!GetOwner()->HasAuthority()) return;
    if (IsIncapacitated())
        if (auto* Mover=GetOwner()->FindComponentByClass<UMoverComponent>(); Mover && Mover->MovementModes.Contains(TEXT("Ragdoll"))
            && Mover->GetMovementModeName()!=TEXT("Ragdoll") && Mover->GetNextMovementModeName()!=TEXT("Ragdoll"))
            Mover->QueueNextMode(TEXT("Ragdoll"));
    if (LifeState==ECombatLifeState::Dead)
    {
        auto* Pawn=Cast<APawn>(GetOwner());
        auto* PC=Pawn?Cast<APlayerController>(Pawn->GetController()):nullptr;
        if (PC && DeathTime>=0 && GetWorld()->GetTimeSeconds()-DeathTime>=2)
            if (auto* Mode=GetWorld()->GetAuthGameMode<APWGGameMode>())
                if (Mode->RespawnPlayer(PC)) ResetVitality();
        return;
    }
    const FVector Position=IsIncapacitated()?GetTargetPoint():GetOwner()->GetActorLocation();
    const bool Moving=FVector::Distance(Position,PreviousPosition)>10*Delta;
    PreviousPosition=Position;
    FHitResult Floor;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(VitalitySupport),false,GetOwner());
    float Reach=100;
    if (!IsIncapacitated())
        if (const auto* Capsule=GetOwner()->FindComponentByClass<UCapsuleComponent>()) Reach=Capsule->GetScaledCapsuleHalfHeight()+10;
    bSupported=GetWorld()->LineTraceSingleByChannel(Floor,Position,Position-FVector(0,0,Reach),ECC_Visibility,Query)
        && Floor.ImpactNormal.Z>0.4f;
    const auto* ProfileCombat=GetOwner()->FindComponentByClass<UDirectionalCombatComponent>();
    const auto* ActiveClip=CombatReactions::Find(DeathAnimationRow,ProfileCombat?&ProfileCombat->MotionCoreStyle:nullptr);
    if (LifeState==ECombatLifeState::Conscious && IsIncapacitated() && !bSupported && Mesh && Mesh->GetPhysicsLinearVelocity(TEXT("pelvis")).Size()>100)
    {
        if (!ActiveClip || ActiveClip->Action!=TEXT("Air") ||
            (!DeathRig.Motions.IsEmpty() && ServerTime()-DeathServerTime>=DeathRig.Motions[0].Length))
        { SelectImpactAnimation(TEXT("Air")); GetOwner()->ForceNetUpdate(); }
    }
    else if (ActiveClip && ActiveClip->Action==TEXT("Air"))
    { DeathAnimationRow=NAME_None; GetOwner()->ForceNetUpdate(); }
    // Split elapsed time at second boundaries; hitches never skip blood loss.
    float Remaining=Delta;
    while (Remaining>UE_SMALL_NUMBER && LifeState!=ECombatLifeState::Dead)
    {
        const float Step=FMath::Min(Remaining,1.f-WoundClock);
        WoundClock+=Step; Remaining-=Step;
        if (Moving) MovingSeconds+=Step;
        if (WoundClock>=1.f-UE_SMALL_NUMBER) { WoundClock=0; StepWounds(); }
    }
}
void UCombatVitalityComponent::CreateHUD()
{
    if (HUD || GetNetMode()==NM_DedicatedServer) return;
    auto* Pawn=Cast<APawn>(GetOwner());
    if (!Pawn || !Pawn->IsLocallyControlled() || !Pawn->IsPlayerControlled() || !GetWorld()->GetGameViewport()) return;
    TWeakObjectPtr<UCombatVitalityComponent> Weak=this;
    TSharedRef<SVerticalBox> Rows=SNew(SVerticalBox);
    Rows->AddSlot().AutoHeight()[SNew(STextBlock).Text_Lambda([Weak] {
        if (!Weak.IsValid()) return FText::GetEmpty();
        return FText::FromString(FString::Printf(TEXT("Krew %.0f / %.0f | -%.1f/s"),Weak->Blood,Weak->MaxBlood,Weak->GetBleeding()));
    })];
    Rows->AddSlot().AutoHeight().Padding(0,5)[SNew(SProgressBar)
        .Percent_Lambda([Weak]()->TOptional<float>{return Weak.IsValid()?Weak->Blood/FMath::Max(1.f,Weak->MaxBlood):0.f;})
        .FillColorAndOpacity(FLinearColor(0.75f,0.03f,0.03f))];
    Rows->AddSlot().AutoHeight()[SNew(STextBlock).Text_Lambda([Weak] {
        if (!Weak.IsValid()) return FText::GetEmpty();
        return FText::FromString(Weak->LifeState==ECombatLifeState::Dead?TEXT("Smierc | respawn 2 s"):
            (Weak->LifeState==ECombatLifeState::Unconscious?TEXT("Nieprzytomny | prog 10%"):
            (Weak->ServerTime()<Weak->KnockdownUntil?TEXT("Powalenie"):
            (Weak->IsIncapacitated()?TEXT("Uszkodzone nogi"):TEXT("Prog omdlenia: 10%")))));
    })];
    for (int32 I=0;I<Parts.Num();++I)
        Rows->AddSlot().AutoHeight()[SNew(STextBlock).Text_Lambda([Weak,I] {
            if (!Weak.IsValid() || !Weak->Parts.IsValidIndex(I)) return FText::GetEmpty();
            const auto& P=Weak->Parts[I];
            return FText::FromString(FString::Printf(TEXT("%s: %.0f/%.0f%s"),*P.Bone.ToString(),P.Integrity,P.Maximum,
                P.bSevered?TEXT(" [odcieta]"):(P.bDestroyed?TEXT(" [zniszczona]"):TEXT(""))));
        })];
    HUD=SNew(SBox).HAlign(HAlign_Right).VAlign(VAlign_Center).Padding(20)
        [SNew(SBox).WidthOverride(265)[SNew(SBorder).Padding(12)[Rows]]];
    HUD->SetVisibility(EVisibility::HitTestInvisible);
    GetWorld()->GetGameViewport()->AddViewportWidgetContent(HUD.ToSharedRef(),15);
}
void UCombatVitalityComponent::EndPlay(EEndPlayReason::Type Reason)
{
    if (HUD && GetWorld() && GetWorld()->GetGameViewport()) GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(HUD.ToSharedRef());
    HUD.Reset(); Super::EndPlay(Reason);
}

float UCombatVitalityComponent::ServerTime() const
{
    const auto* State=GetWorld()->GetGameState();
    return State?State->GetServerWorldTimeSeconds():GetWorld()->GetTimeSeconds();
}
void UCombatVitalityComponent::CaptureNetworkRagdoll()
{
    if (!GetOwner()->HasAuthority() || GetNetMode()==NM_Standalone || !Mesh || !UsesRagdollPose()) return;
    FCombatRagdollFrame Frame;
    Frame.Life=LifeId; Frame.Time=ServerTime(); Frame.Pelvis=Mesh->GetSocketLocation(TEXT("pelvis"));
    const auto& Component=Mesh->GetComponentSpaceTransforms();
    for (int32 I=0;I<Mesh->GetNumBones() && Frame.Bodies.Num()<32;++I)
    {
        const FName Bone=Mesh->GetBoneName(I);
        if (!Mesh->GetBodyInstance(Bone) && Bone!=TEXT("pelvis")) continue;
        const int32 Parent=Mesh->GetBoneIndex(Mesh->GetParentBone(Bone));
        if (!Component.IsValidIndex(I)) continue;
        auto& Key=Frame.Bodies.AddDefaulted_GetRef(); Key.Bone=Bone;
        Key.Rotation=(Component.IsValidIndex(Parent)?Component[I].GetRelativeTransform(Component[Parent]):Component[I]).Rotator();
    }
    RagdollFrame=MoveTemp(Frame);
}
void UCombatVitalityComponent::OnRep_Ragdoll()
{
    if (RagdollFrame.Time<=ReceivedRagdollFrame.Time && RagdollFrame.Life==ReceivedRagdollFrame.Life) return;
    PreviousRagdollFrame=ReceivedRagdollFrame.Life==RagdollFrame.Life?ReceivedRagdollFrame:RagdollFrame;
    ReceivedRagdollFrame=RagdollFrame;
}
void UCombatVitalityComponent::ApplyNetworkRagdoll(UPoseableMeshComponent& Visual)
{
    if (GetOwner()->HasAuthority() || !Mesh || ReceivedRagdollFrame.Life!=LifeId || ReceivedRagdollFrame.Bodies.IsEmpty()) return;
    // Interpolate a bounded physics-body snapshot at 10 Hz, not the complete render skeleton.
    const float Span=ReceivedRagdollFrame.Time-PreviousRagdollFrame.Time;
    const float Alpha=Span>UE_SMALL_NUMBER?FMath::Clamp((ServerTime()-0.1f-PreviousRagdollFrame.Time)/Span,0.f,1.f):1.f;
    for (const auto& Key:ReceivedRagdollFrame.Bodies)
    {
        const int32 I=Visual.GetBoneIndex(Key.Bone);
        if (!Visual.BoneSpaceTransforms.IsValidIndex(I)) continue;
        const auto* Prior=PreviousRagdollFrame.Bodies.FindByPredicate([&](const auto& P){return P.Bone==Key.Bone;});
        const FQuat Rotation=FQuat::Slerp(Prior?Prior->Rotation.Quaternion():Key.Rotation.Quaternion(),Key.Rotation.Quaternion(),Alpha).GetNormalized();
        Visual.BoneSpaceTransforms[I].SetRotation(Rotation);
    }
    const int32 Pelvis=Visual.GetBoneIndex(TEXT("pelvis"));
    if (Visual.BoneSpaceTransforms.IsValidIndex(Pelvis))
    {
        const auto& Ref=Mesh->GetSkeletalMeshAsset()->GetRefSkeleton();
        TArray<FTransform> Component=Visual.BoneSpaceTransforms;
        for (int32 I=0;I<Component.Num();++I) if (Ref.GetParentIndex(I)!=INDEX_NONE) Component[I]*=Component[Ref.GetParentIndex(I)];
        const int32 Parent=Ref.GetParentIndex(Pelvis);
        const FTransform ParentWorld=Parent==INDEX_NONE?Visual.GetComponentTransform():Component[Parent]*Visual.GetComponentTransform();
        const FVector Location=FMath::Lerp(FVector(PreviousRagdollFrame.Pelvis),FVector(ReceivedRagdollFrame.Pelvis),Alpha);
        Visual.BoneSpaceTransforms[Pelvis].SetTranslation(ParentWorld.InverseTransformPosition(Location));
    }
}

void UCombatVitalityComponent::QueueRagdollImpulse(const FVector& Impulse)
{
    if (!GetOwner()->HasAuthority() || Impulse.ContainsNaN()) return;
    // Momentum is already scaled at the contact. Do not multiply it again on ragdoll entry.
    PendingImpulse=(PendingImpulse+Impulse).GetClampedToMaxSize(6500.f);
    PendingImpulseUntil=ServerTime()+0.75f;
}
