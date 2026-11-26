#include "Combat/DirectionalCombatComponent.h"
#include "Placeholder/PlaceholderHandsComponent.h"
#include "Sword/ProceduralSword.h"
#include "Animation/AnimSequence.h"
#include "Animation/PoseAsset.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

namespace CombatMotion
{
    float Ease(float T) { T=FMath::Clamp(T,0.0f,1.0f); return T*T*(3-2*T); }
    FVector Direction(float Angle) { return FVector(FMath::Cos(Angle),0,FMath::Sin(Angle)); }
}

void UDirectionalCombatComponent::LoadGuardProfiles()
{
    GuardProfiles.SetNum(3);
    GuardProfiles[0].GuardGrip=FVector(0.1,0,0.68); GuardProfiles[0].GuardBlade=FVector(-1,0,0);
    GuardProfiles[0].ReadyGrip=FVector(-0.18,0.2,0.75); GuardProfiles[0].ReadyBlade=FVector(-1,0.15,0.2);
    GuardProfiles[2].GuardGrip=FVector(0.55,0,-0.7); GuardProfiles[2].GuardBlade=FVector(0.65,0,-0.75);
    GuardProfiles[2].ReadyGrip=FVector(-0.15,0.3,-0.75); GuardProfiles[2].ReadyBlade=FVector(-0.9,0.35,-0.5);
    const TCHAR* Names[]={TEXT("Up"),TEXT("Front"),TEXT("Low")};
    const TCHAR* Guards[]={TEXT("UEFNLongsword_Block_R"),TEXT("UEFNLongsword_BlockLoop"),TEXT("UEFNLongsword_Block_D")};
    const TCHAR* Ready[]={TEXT("UEFNLongsword_Attack_D"),TEXT("UEFNLongsword_Attack_R"),TEXT("UEFNLongsword_Attack_p_LU")};
    auto Asset=[](const TCHAR* Name){return TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(FString::Printf(TEXT("/Game/Systems/Procedural_Combat/Animations/LongSword/%s.%s"),Name,Name)));};
    auto* Table=LoadObject<UDataTable>(nullptr,TEXT("/Game/Systems/Procedural_Combat/DT_Combat_Guards.DT_Combat_Guards"));
    for (int32 G=0;G<3;++G)
    {
        GuardProfiles[G].GuardPose=Asset(Guards[G]);
        GuardProfiles[G].LightPose=Asset(Ready[G]); GuardProfiles[G].HeavyPose=Asset(Ready[G]);
        GuardProfiles[G].PiercePose=Asset(TEXT("UEFNLongsword_Attack_Thrust"));
        if (Table) if (const auto* Row=Table->FindRow<FCombatGuardProfile>(Names[G],TEXT("Combat guard"),false)) GuardProfiles[G]=*Row;
    }
}

void UDirectionalCombatComponent::SelectGuard()
{
    if (!IsLocal()) return;
    const auto Phase=GetCombatPhase();
    if (Phase==EDirectionalCombatPhase::Aiming || Phase==EDirectionalCombatPhase::Attacking || Phase==EDirectionalCombatPhase::Recovery) return;
    const float Z=AimOffset.Z;
    constexpr float Margin=5;
    const float Top=Rig.HeadHeight-StanceLowering,Bottom=Rig.PelvisHeight-StanceLowering;
    ECombatGuard Next=CurrentGuard;
    if (Z>Top+(CurrentGuard==ECombatGuard::Up?-Margin:Margin)) Next=ECombatGuard::Up;
    else if (Z<Bottom+(CurrentGuard==ECombatGuard::Low?Margin:-Margin)) Next=ECombatGuard::Low;
    else Next=ECombatGuard::Front;
    if (Next!=CurrentGuard)
    {
        CurrentGuard=Next;
        if (GetOwner()->HasAuthority()) GetOwner()->ForceNetUpdate(); else ServerSetGuard(Next);
    }
}
void UDirectionalCombatComponent::ServerSetGuard_Implementation(ECombatGuard Guard)
{
    if (bUseMotionCoreV3) return;
    if (static_cast<uint8>(Guard)>2) return;
    const auto Phase=GetCombatPhase();
    if (Phase==EDirectionalCombatPhase::Aiming || Phase==EDirectionalCombatPhase::Attacking || Phase==EDirectionalCombatPhase::Recovery) return;
    CurrentGuard=Guard; GetOwner()->ForceNetUpdate();
}
float UDirectionalCombatComponent::GetChargeSeconds() const
{
    if (bUseMotionCoreV3) return BodyAttack.bPrepared?FMath::Max(0.0,(BodyAttack.bReleased?BodyAttack.ReleasedAt:CombatTime())-BodyAttack.PreparedAt):0;
    return AttackMotion.bPrepared?(AttackMotion.bReleased?AttackMotion.ChargeSeconds:FMath::Max(0.0,CombatTime()-AttackMotion.PreparedAt)):0;
}
void UDirectionalCombatComponent::ComputeCharge(FDirectionalAttackMotion& Motion) const
{
    const float LightThreshold=FMath::Max(0.1f,Charging.LightStaggerSeconds);
    const float HeavyThreshold=FMath::Max(0.1f,Charging.HeavyDisarmSeconds);
    Motion.ChargeSeconds=FMath::Clamp(Motion.ChargeSeconds,0.0f,Motion.Type==EDirectionalAttack::Heavy?HeavyThreshold:FMath::Max(LightThreshold,Charging.LightMaximumSeconds));
    Motion.bPush=false; Motion.ImpactScore=FMath::Clamp(Charging.LightImpact,0.0f,5.0f); Motion.Intensity=ECombatIntensity::Weak;
    if (Motion.Type==EDirectionalAttack::Heavy)
    {
        Motion.bPush=true;
        const bool Disarm=Motion.ChargeSeconds>=HeavyThreshold-0.001f;
        Motion.ImpactScore=Disarm?FMath::Clamp(Charging.DisarmImpact,15.01f,20.0f):FMath::Clamp(Charging.StaggerImpact,8.01f,15.0f);
        Motion.Intensity=Disarm?ECombatIntensity::Strong:ECombatIntensity::Medium;
    }
    else if (Motion.Type==EDirectionalAttack::Light && Motion.ChargeSeconds>=LightThreshold-0.001f)
    {
        Motion.ImpactScore=FMath::Clamp(Charging.StaggerImpact,8.01f,15.0f);
        Motion.bPush=Motion.ChargeSeconds>LightThreshold+0.001f;
        Motion.Intensity=ECombatIntensity::Medium;
    }
}

double UDirectionalCombatComponent::CombatTime() const
{
    const auto* State=GetWorld()->GetGameState();
    return State?State->GetServerWorldTimeSeconds():GetWorld()->GetTimeSeconds();
}
const FDirectionalAttackSettings& UDirectionalCombatComponent::SettingsFor(EDirectionalAttack Type) const
{ return Type==EDirectionalAttack::Heavy?HeavyAttack:Type==EDirectionalAttack::Pierce?PierceAttack:LightAttack; }

EDirectionalCombatPhase UDirectionalCombatComponent::GetCombatPhase() const
{
    if (bUseMotionCoreV3)
    {
        if (Reaction.bDisarmed) return EDirectionalCombatPhase::Disarmed;
        if (CombatTime()<MotionReaction.GuardAt) return EDirectionalCombatPhase::Staggered;
        if (!bCombatActive || !BodyAttack.bPrepared || BodyAttack.WeaponId!=EquippedSword.InstanceId) return EDirectionalCombatPhase::Combat;
        if (!BodyAttack.bReleased || CombatTime()<BodyAttack.StrikeAt) return EDirectionalCombatPhase::Aiming;
        if (BodyAttack.ReturnAt<0) return EDirectionalCombatPhase::Attacking;
        if (CombatTime()<BodyAttack.ReturnAt+BodyAttack.Recovery) return EDirectionalCombatPhase::Recovery;
        return EDirectionalCombatPhase::Combat;
    }
    if (Reaction.bDisarmed) return EDirectionalCombatPhase::Disarmed;
    if (Reaction.bInterrupt && CombatTime()<Reaction.EndTime)
        return Reaction.Result==ECombatContactResult::Stagger?EDirectionalCombatPhase::Staggered:EDirectionalCombatPhase::Recovery;
    if (bCombatActive && bBlocking) return EDirectionalCombatPhase::Blocking;
    if (!bCombatActive || !AttackMotion.bPrepared) return EDirectionalCombatPhase::Combat;
    if (!AttackMotion.bReleased) return EDirectionalCombatPhase::Aiming;
    const double Elapsed=CombatTime()-AttackMotion.StartTime;
    if (Elapsed<AttackMotion.Duration) return EDirectionalCombatPhase::Attacking;
    return Elapsed<AttackMotion.Duration+AttackMotion.Recovery?EDirectionalCombatPhase::Recovery:EDirectionalCombatPhase::Combat;
}
FVector UDirectionalCombatComponent::GetCombatFacingOffset() const
{
    if (bUseMotionCoreV3 && BodyAttack.bPrepared && BodyAttack.bReleased && GetCombatPhase()!=EDirectionalCombatPhase::Combat)
        return FVector(BodyAttack.Target)-GetOwner()->GetActorLocation();
    return FVector(AimOffset);
}

FVector UDirectionalCombatComponent::GetAimDirection() const
{
    if (bUseMotionCoreV3 && BodyAttack.bReleased && GetCombatPhase()!=EDirectionalCombatPhase::Combat) return BodyAttack.Direction;
    if (bUseMotionCoreV3) return bMotionCoreValid?MotionPlan.Intent.GuardDirectionWorld:FVector(AimOffset).GetSafeNormal();
    // A frozen preparation must not rotate with the cursor or movement direction.
    const auto Phase=GetCombatPhase();
    if (AttackMotion.bPrepared && (Phase==EDirectionalCombatPhase::Aiming || Phase==EDirectionalCombatPhase::Attacking || Phase==EDirectionalCombatPhase::Recovery)) return FVector(AttackMotion.Facing,0,0);
    return AimOffset.GetSafeNormal();
}
FText UDirectionalCombatComponent::GetCombatHint() const
{
    if (bUseMotionCoreV3)
    {
        if (!MotionCoreFailure.IsEmpty()) return FText::FromString(TEXT("Combat V3: ")+MotionCoreFailure);
        return FText::FromString(bMotionCoreValid?
            TEXT("Motion Core V3 | LPM: lekki | PPM: ciezki | SPM: pchniecie | Pusc przycisk: atak | Dragon: kontakt broni i swiata"):
            TEXT("Motion Core V3: przygotowanie pozy i chwytu"));
    }
    switch (GetCombatPhase())
    {
        case EDirectionalCombatPhase::Aiming:
            return FText::FromString(FString::Printf(TEXT("Ladowanie %.1f s | Lekki 1 s: stagger | Ciezki 3 s: disarm"),GetChargeSeconds()));
        case EDirectionalCombatPhase::Attacking: return FText::FromString(TEXT("Atak"));
        case EDirectionalCombatPhase::Recovery: return FText::FromString(TEXT("Powrot do postawy"));
        case EDirectionalCombatPhase::Blocking: return FText::FromString(TEXT("Blok | Mysz - kierunek | Pusc Shift - koniec bloku"));
        case EDirectionalCombatPhase::Staggered: return FText::FromString(TEXT("Zachwianie - blok niedostepny"));
        case EDirectionalCombatPhase::Disarmed: return FText::FromString(TEXT("Rozbrojenie | E - podnies bron | NumPad5 - nowy miecz"));
        default: return FText::FromString(TEXT("LPM - lekkie | PPM - ciezkie | SPM - pchniecie | Shift - blok | R - schowaj | Q - rzut"));
    }
}
void UDirectionalCombatComponent::UpdateCursorAim()
{
    if (!IsLocal()) return;
    auto* P=Cast<APawn>(GetOwner()); auto* PC=P?Cast<APlayerController>(P->GetController()):nullptr;
    FVector Origin,Direction;
    if (PC && PC->DeprojectMousePositionToWorld(Origin,Direction) && FMath::Abs(Direction.Y)>0.0001)
    {
        const double T=(GetOwner()->GetActorLocation().Y-Origin.Y)/Direction.Y;
        if (T>0) AimOffset=(Origin+Direction*T-GetOwner()->GetActorLocation()).GetClampedToMaxSize(5000);
    }
}
void UDirectionalCombatComponent::BeginAttack(EDirectionalAttack Type)
{
    if (bUseMotionCoreV3)
    {
        if (!bCombatActive && IsLocal()) ToggleCombat();
        SubmitBodyAttack(0,Type); return;
    }
    if (!IsLocal() || !bCombatActive || GetCombatPhase()!=EDirectionalCombatPhase::Combat ||
        static_cast<uint8>(Type)>static_cast<uint8>(EDirectionalAttack::Pierce)) return;
    if (!bAIControlled && !IsValid(ActiveSword())) return;
    if (Hands) Hands->CancelCharge();
    UpdateCursorAim();
    SelectGuard();
    if (GuardProfiles.Num()!=3) LoadGuardProfiles();
    const FCombatWeaponFrame Frame=GetWeaponFrame();
    const uint32 NextId=AttackMotion.Id+1;
    AttackMotion=FDirectionalAttackMotion(); AttackMotion.Id=NextId;
    AttackMotion.bPrepared=true; AttackMotion.Type=Type; AttackMotion.StartTime=CombatTime();
    AttackMotion.PreparedAt=AttackMotion.StartTime; AttackMotion.Guard=CurrentGuard;
    AttackMotion.Facing=GetOwner()->GetActorForwardVector().X>=0?1.0f:-1.0f;
    AttackMotion.StartGrip=Frame.Grip-GetOwner()->GetActorLocation();
    AttackMotion.StartAngle=Frame.Angle;
    AttackMotion.StartRotation=Frame.Rotation;
    const auto& Guard=GuardProfiles[static_cast<int32>(CurrentGuard)];
    const FVector Shoulder=CombatSpace::Local(Rig.Shoulder,AttackMotion.Facing)-FVector(0,0,StanceLowering);
    AttackMotion.ReadyGrip=Shoulder+CombatSpace::Local(Guard.ReadyGrip.GetClampedToMaxSize(0.95)*Rig.ArmLength,AttackMotion.Facing);
    AttackMotion.ReadyRotation=CombatSpace::Orientation(CombatSpace::Local(Guard.ReadyBlade,AttackMotion.Facing));
    const auto& Settings=SettingsFor(Type);
    AttackMotion.Duration=FMath::Max(0.08f,Settings.Duration);
    AttackMotion.Recovery=FMath::Max(0.08f,Settings.Recovery);
    PublishAttack();
}
void UDirectionalCombatComponent::ReleaseAttack(EDirectionalAttack Type)
{
    if (Type==EDirectionalAttack::Heavy && bDefenseConsumedHeavy) { bDefenseConsumedHeavy=false; return; }
    if (bUseMotionCoreV3) { SubmitBodyAttack(1,Type); return; }
    if (!IsLocal() || !bCombatActive || !AttackMotion.bPrepared || AttackMotion.bReleased || AttackMotion.Type!=Type) return;
    if (!bAIControlled && !IsValid(ActiveSword())) { CancelAttack(); return; }
    UpdateCursorAim();
    const FVector Origin=GetOwner()->GetActorLocation();
    const auto Prepared=EvaluateWeapon(CombatTime(),Origin);
    AttackMotion.ChargeSeconds=FMath::Max(0.0,CombatTime()-AttackMotion.PreparedAt);
    ComputeCharge(AttackMotion);
    AttackMotion.StartGrip=Prepared.Grip-Origin;
    AttackMotion.StartRotation=Prepared.Rotation;
    AttackMotion.LockedTarget=Origin+FVector(AimOffset);
    FVector Direction=AttackMotion.LockedTarget-(Origin+AttackMotion.StartGrip);
    if (!Direction.Normalize()) Direction=CombatMotion::Direction(AttackMotion.StartAngle);
    const float TargetAngle=FMath::Atan2(Direction.Z,Direction.X);
    const float Difference=FMath::FindDeltaAngleRadians(AttackMotion.StartAngle,TargetAngle);
    const auto& Settings=SettingsFor(Type);
    const FVector Shoulder=CombatSpace::Local(Rig.Shoulder,AttackMotion.Facing)-FVector(0,0,StanceLowering);
    const double ArmLength=Rig.ArmLength;
    const float Side=FMath::Abs(Difference)>0.05f?FMath::Sign(Difference):-AttackMotion.Facing;
    AttackMotion.EndGrip=Shoulder+Direction*ArmLength*FMath::Clamp(Settings.ReachFraction,0.2f,0.95f);
    AttackMotion.ContactRotation=CombatSpace::Orientation(Direction);
    AttackMotion.EndRotation=AttackMotion.ContactRotation;
    AttackMotion.ControlGrip=(AttackMotion.StartGrip+AttackMotion.EndGrip)*0.5;
    if (Type==EDirectionalAttack::Pierce)
    {
        AttackMotion.SweepAngle=Difference;
        // The initial portion aligns the point; the remainder extends the hand in a straight line.
    }
    else
    {
        AttackMotion.SweepAngle=Difference+Side*FMath::DegreesToRadians(FMath::Clamp(Settings.FollowThroughDegrees,5.0f,150.0f));
        AttackMotion.ControlGrip+=FVector(-Direction.Z,0,Direction.X)*Side*ArmLength*0.3;
    }
    // The front cut travels across depth; high and low cuts arc down/up with a smaller depth component.
    if (Type!=EDirectionalAttack::Pierce)
    {
        const float Facing=AttackMotion.Facing;
        const FVector EndDirection=AttackMotion.Guard==ECombatGuard::Up?FVector(Facing*0.5,-0.15,-1):
            AttackMotion.Guard==ECombatGuard::Low?FVector(Facing*0.5,-0.15,1):FVector(Facing*0.25,-1,0);
        AttackMotion.EndRotation=CombatSpace::Orientation(EndDirection);
        AttackMotion.ControlGrip=(AttackMotion.StartGrip+AttackMotion.EndGrip)*0.5+
            FVector(Facing*ArmLength*0.2,-ArmLength*0.35,0);
        AttackMotion.EndGrip.Y=Shoulder.Y-ArmLength*0.25;
    }
    AttackMotion.SweepAngle=AttackMotion.StartRotation.AngularDistance(AttackMotion.ContactRotation)+AttackMotion.ContactRotation.AngularDistance(AttackMotion.EndRotation);
    AttackMotion.PreparationAlpha=AttackPoseWeight();
    AttackMotion.bReleased=true; AttackMotion.StartTime=CombatTime();
    PublishAttack();
}
void UDirectionalCombatComponent::CancelAttack()
{
    if (bUseMotionCoreV3) { SubmitBodyAttack(2,BodyAttack.Type); return; }
    if (!AttackMotion.bPrepared) return;
    AttackMotion.bPrepared=false; AttackMotion.bReleased=false;
    if (IsLocal()) PublishAttack();
}
void UDirectionalCombatComponent::PublishAttack()
{
    if (GetOwner()->HasAuthority()) GetOwner()->ForceNetUpdate();
    else ServerSetAttack(AttackMotion);
}
void UDirectionalCombatComponent::ServerSetAttack_Implementation(FDirectionalAttackMotion Motion)
{
    if (bUseMotionCoreV3) return;
    if (Motion.bPrepared && (!bCombatActive || bBlocking || Reaction.bDisarmed ||
        (Reaction.bInterrupt && (CombatTime()<Reaction.EndTime || Motion.Id<=Reaction.InterruptedAttackId))))
    { ClientCorrectAttack(AttackMotion); return; }
    if (static_cast<uint8>(Motion.Type)>static_cast<uint8>(EDirectionalAttack::Pierce) ||
        static_cast<uint8>(Motion.Guard)>2 || !FMath::IsFinite(Motion.PreparedAt) || !FMath::IsFinite(Motion.ChargeSeconds) ||
        Motion.StartRotation.ContainsNaN() || Motion.ReadyRotation.ContainsNaN() || Motion.ContactRotation.ContainsNaN() || Motion.EndRotation.ContainsNaN() || Motion.ReadyGrip.ContainsNaN() ||
        !FMath::IsFinite(Motion.StartTime) || !FMath::IsFinite(Motion.StartAngle) || !FMath::IsFinite(Motion.SweepAngle) ||
        !FMath::IsFinite(Motion.Duration) || !FMath::IsFinite(Motion.Recovery) || !FMath::IsFinite(Motion.Facing) || !FMath::IsFinite(Motion.PreparationAlpha) ||
        Motion.StartGrip.ContainsNaN() || Motion.EndGrip.ContainsNaN() || Motion.ControlGrip.ContainsNaN() || Motion.LockedTarget.ContainsNaN()) return;
    // Co-op trusts owner input. Bounds protect simulation from malformed values, not cheating.
    Motion.Duration=FMath::Clamp(Motion.Duration,0.08f,10.0f); Motion.Recovery=FMath::Clamp(Motion.Recovery,0.08f,10.0f);
    Motion.Facing=Motion.Facing>=0?1.0f:-1.0f;
    Motion.StartGrip=Motion.StartGrip.GetClampedToMaxSize(600);
    Motion.EndGrip=Motion.EndGrip.GetClampedToMaxSize(600);
    Motion.ControlGrip=Motion.ControlGrip.GetClampedToMaxSize(600);
    Motion.SweepAngle=FMath::Clamp(Motion.SweepAngle,-2*PI,2*PI);
    Motion.PreparationAlpha=FMath::Clamp(Motion.PreparationAlpha,0.0f,1.0f);
    Motion.StartRotation.Normalize(); Motion.ReadyRotation.Normalize(); Motion.ContactRotation.Normalize(); Motion.EndRotation.Normalize();
    Motion.ReadyGrip=Motion.ReadyGrip.GetClampedToMaxSize(600);
    // Use the preparation already observed by the server. The 250 ms allowance covers receipt jitter.
    if (Motion.bReleased && AttackMotion.Id==Motion.Id && AttackMotion.bPrepared && !AttackMotion.bReleased)
        Motion.ChargeSeconds=FMath::Min(Motion.ChargeSeconds,static_cast<float>(FMath::Max(0.0,CombatTime()-AttackMotion.PreparedAt+0.25)));
    ComputeCharge(Motion);
    Motion.StartTime=FMath::Clamp(Motion.StartTime,CombatTime()-0.25,CombatTime());
    if (Motion.Id<AttackMotion.Id || (Motion.Id==AttackMotion.Id && AttackMotion.bReleased && Motion.bPrepared && !Motion.bReleased)) return;
    AttackMotion=Motion; GetOwner()->ForceNetUpdate();
    CurrentGuard=Motion.Guard;
}
void UDirectionalCombatComponent::ClientCorrectAttack_Implementation(FDirectionalAttackMotion Motion)
{
    AttackMotion=Motion;
    OnRep_AttackMotion();
}
void UDirectionalCombatComponent::OnRep_AttackMotion()
{
    if (bUseMotionCoreV3) { AttackMotion=FDirectionalAttackMotion(); return; }
    if (Reaction.bInterrupt && AttackMotion.Id<=Reaction.InterruptedAttackId)
    { AttackMotion.bPrepared=false; AttackMotion.bReleased=false; }
}
// Retain the finished description until the next command. The server must still sweep the
// final interval after a long frame; phase and pose weight are derived from its timestamps.
float UDirectionalCombatComponent::AttackPoseWeight() const
{
    if (Reaction.bInterrupt && CombatTime()<Reaction.EndTime)
        return Reaction.PoseWeight*(1-CombatMotion::Ease(static_cast<float>((CombatTime()-Reaction.StartTime)/FMath::Max(0.01,Reaction.EndTime-Reaction.StartTime))));
    if (!AttackMotion.bPrepared) return 0;
    if (!AttackMotion.bReleased) return CombatMotion::Ease(static_cast<float>((CombatTime()-AttackMotion.StartTime)/0.15));
    const double Elapsed=CombatTime()-AttackMotion.StartTime;
    if (Elapsed<AttackMotion.Duration) return FMath::Lerp(AttackMotion.PreparationAlpha,1.0f,CombatMotion::Ease(Elapsed/FMath::Min(0.12f,AttackMotion.Duration)));
    return 1-CombatMotion::Ease(static_cast<float>((Elapsed-AttackMotion.Duration)/AttackMotion.Recovery));
}
FCombatWeaponFrame UDirectionalCombatComponent::EvaluateWeapon(double Time,const FVector& Origin) const
{
    if (bUseMotionCoreV3) return GetWeaponFrame();
    FCombatWeaponFrame Frame=RestWeapon;
    Frame.Grip+=Origin;
    if (Reaction.bInterrupt && Time<Reaction.EndTime && Time>=Reaction.StartTime)
    {
        const float T=CombatMotion::Ease(static_cast<float>((Time-Reaction.StartTime)/FMath::Max(0.01,Reaction.EndTime-Reaction.StartTime)));
        Frame.Grip=Origin+FMath::Lerp(Reaction.Start.Grip,Reaction.End.Grip,static_cast<double>(T));
        Frame.Angle=FMath::Lerp(Reaction.Start.Angle,Reaction.End.Angle,T);
        Frame.Rotation=FQuat::Slerp(Reaction.Start.Rotation,Reaction.End.Rotation,T).GetNormalized();
        // Finish every interruption at the live stance, without an end-of-recovery jump.
        const float Return=CombatMotion::Ease((T-0.55f)/0.45f);
        Frame.Grip=FMath::Lerp(Frame.Grip,Origin+RestWeapon.Grip,static_cast<double>(Return));
        Frame.Angle+=FMath::FindDeltaAngleRadians(Frame.Angle,RestWeapon.Angle)*Return;
        Frame.Rotation=FQuat::Slerp(Frame.Rotation,RestWeapon.Rotation,Return).GetNormalized();
        if (Reaction.Result==ECombatContactResult::Shake)
            Frame.Rotation=FQuat(FVector::YAxisVector,FMath::Sin(static_cast<float>((Time-Reaction.StartTime)*65))*0.08f*(1-T))*Frame.Rotation;
        return Frame;
    }
    const bool bLiveAttack=AttackMotion.bPrepared && (!AttackMotion.bReleased || Time<AttackMotion.StartTime+AttackMotion.Duration+AttackMotion.Recovery);
    if (!bLiveAttack)
    {
        if (Time<Reaction.EndTime && Time>=Reaction.StartTime)
            Frame.Rotation=FQuat(FVector::YAxisVector,FMath::Sin(static_cast<float>((Time-Reaction.StartTime)*65))*0.08f*
                static_cast<float>((Reaction.EndTime-Time)/FMath::Max(0.01,Reaction.EndTime-Reaction.StartTime)))*Frame.Rotation;
        return Frame;
    }
    Frame=EvaluateAttackFrame(AttackMotion,RestWeapon,Time,Origin);
    if (!Reaction.bInterrupt && Time>=Reaction.StartTime && Time<Reaction.EndTime)
        Frame.Rotation=FQuat(FVector::YAxisVector,FMath::Sin(static_cast<float>((Time-Reaction.StartTime)*65))*0.08f*
            static_cast<float>((Reaction.EndTime-Time)/FMath::Max(0.01,Reaction.EndTime-Reaction.StartTime)))*Frame.Rotation;
    return Frame;
}
FCombatWeaponFrame UDirectionalCombatComponent::EvaluateAttackFrame(const FDirectionalAttackMotion& Motion,const FCombatWeaponFrame& Rest,double Time,const FVector& Origin)
{
    FCombatWeaponFrame Frame=Rest;
    Frame.Grip+=Origin;
    if (!Motion.bPrepared) return Frame;
    FVector Offset=Motion.StartGrip;
    FQuat Rotation=Motion.StartRotation;
    float Angle=Motion.StartAngle;
    float ReturnWeight=0;
    if (!Motion.bReleased)
    {
        const float Prepare=CombatMotion::Ease((Time-Motion.PreparedAt)/0.2);
        Frame.Grip=Origin+FMath::Lerp(Motion.StartGrip,Motion.ReadyGrip,static_cast<double>(Prepare));
        Frame.Rotation=FQuat::Slerp(Motion.StartRotation,Motion.ReadyRotation,Prepare).GetNormalized();
        return Frame;
    }
    if (Motion.bReleased)
    {
        const double Elapsed=FMath::Max(0.0,Time-Motion.StartTime);
        const float T=FMath::Clamp(static_cast<float>(Elapsed/FMath::Max(0.08f,Motion.Duration)),0.0f,1.0f);
        if (Motion.Type==EDirectionalAttack::Pierce)
        {
            Rotation=FQuat::Slerp(Motion.StartRotation,Motion.ContactRotation,CombatMotion::Ease(T/0.25f)).GetNormalized();
            Angle+=Motion.SweepAngle*CombatMotion::Ease(T/0.25f);
            Offset=FMath::Lerp(Motion.StartGrip,Motion.EndGrip,static_cast<double>(CombatMotion::Ease((T-0.25f)/0.75f)));
        }
        else
        {
            const float P=CombatMotion::Ease(T);
            // Piecewise orientations make the weapon pass through the selected contact direction.
            Rotation=P<0.55f?FQuat::Slerp(Motion.StartRotation,Motion.ContactRotation,CombatMotion::Ease(P/0.55f)):
                FQuat::Slerp(Motion.ContactRotation,Motion.EndRotation,CombatMotion::Ease((P-0.55f)/0.45f));
            Angle+=Motion.SweepAngle*P;
            Offset=(1-P)*(1-P)*Motion.StartGrip+2*(1-P)*P*Motion.ControlGrip+P*P*Motion.EndGrip;
        }
        ReturnWeight=CombatMotion::Ease(static_cast<float>((Elapsed-Motion.Duration)/FMath::Max(0.08f,Motion.Recovery)));
    }
    Frame.Grip=Origin+FMath::Lerp(Offset,Rest.Grip,static_cast<double>(ReturnWeight));
    Frame.Angle=Angle+FMath::FindDeltaAngleRadians(Angle,Rest.Angle)*ReturnWeight;
    Frame.Rotation=FQuat::Slerp(Rotation,Rest.Rotation,ReturnWeight).GetNormalized();
    return Frame;
}
void UDirectionalCombatComponent::CacheAttackPoses()
{
    if (!CachedMesh) return;
    if (GuardProfiles.Num()!=3) LoadGuardProfiles();
    const auto& Ref=CachedMesh->GetRefSkeleton();
    for (int32 Type=0;Type<4;++Type)
    {
        TArray<FQuat>& Rotations=Type==3?BlockRotations:AttackRotations[Type];
        Rotations=StanceRotations;
        UAnimSequence* Sequence=Type==3?BlockPose.LoadSynchronous():(AttackPoseAssets.IsValidIndex(Type)?AttackPoseAssets[Type].LoadSynchronous():nullptr);
        if (!Sequence || !Sequence->GetSkeleton()) continue;
        const auto& Source=Sequence->GetSkeleton()->GetReferenceSkeleton();
        const float Time=FMath::Clamp(Type==3?BlockPoseTime:SettingsFor(static_cast<EDirectionalAttack>(Type)).PoseTime,0.0f,static_cast<float>(Sequence->GetPlayLength()));
        for (int32 I=0;I<Ref.GetNum();++I)
        {
            const int32 SourceIndex=Source.FindBoneIndex(Ref.GetBoneName(I));
            if (!UpperBody[I] || SourceIndex==INDEX_NONE) continue;
            FTransform Sample;
            Sequence->GetBoneTransform(Sample,FSkeletonPoseBoneIndex(SourceIndex),FAnimExtractContext(static_cast<double>(Time)),false);
            Rotations[I]=(Sample.GetRotation()*Source.GetRefBonePose()[SourceIndex].GetRotation().Inverse()*Ref.GetRefBonePose()[I].GetRotation()).GetNormalized();
        }
    }
    auto SamplePose=[&](TArray<FQuat>& Out,TSoftObjectPtr<UAnimSequence> Asset,float Time,const TArray<FQuat>& Fallback,
        TSoftObjectPtr<UPoseAsset> PoseSource,FName PoseName)
    {
        Out=Fallback;
        if (!PoseSource.IsNull())
        {
            const auto* SavedPose=PoseSource.LoadSynchronous();
            const int32 PoseIndex=SavedPose?(PoseName.IsNone()?0:SavedPose->GetPoseIndexByName(PoseName)):INDEX_NONE;
            TArray<FTransform> Transforms;
            TArray<FPoseAssetInfluenceForPose> Influences;
            if (SavedPose && SavedPose->GetSkeleton() && PoseIndex>=0 && PoseIndex<SavedPose->GetNumPoses() &&
                SavedPose->ConvertToFullPose(PoseIndex,Transforms,Influences))
            {
                const auto& Source=SavedPose->GetSkeleton()->GetReferenceSkeleton();
                const auto& Tracks=SavedPose->GetTrackNames();
                for (const auto& Influence:Influences)
                {
                    if (!Tracks.IsValidIndex(Influence.TrackIndex) || !Transforms.IsValidIndex(Influence.BoneTransformIndex)) continue;
                    const FName Bone=Tracks[Influence.TrackIndex];
                    const int32 I=Ref.FindBoneIndex(Bone),SourceIndex=Source.FindBoneIndex(Bone);
                    if (I==INDEX_NONE || SourceIndex==INDEX_NONE || !UpperBody[I]) continue;
                    Out[I]=(Transforms[Influence.BoneTransformIndex].GetRotation()*Source.GetRefBonePose()[SourceIndex].GetRotation().Inverse()*Ref.GetRefBonePose()[I].GetRotation()).GetNormalized();
                }
                return;
            }
            UE_LOG(LogTemp,Warning,TEXT("Combat pose: cannot extract %s / %s; using single-frame fallback."),*PoseSource.ToString(),*PoseName.ToString());
        }
        const auto* Sequence=Asset.LoadSynchronous();
        if (!Sequence || !Sequence->GetSkeleton()) return;
        if (Time<0 || Time>Sequence->GetPlayLength())
            UE_LOG(LogTemp,Warning,TEXT("Combat pose: %s sample %.3f seconds exceeds duration %.3f; clamped to a single frame. PoseTime is seconds, not frames."),*Sequence->GetName(),Time,Sequence->GetPlayLength());
        const auto& Source=Sequence->GetSkeleton()->GetReferenceSkeleton();
        for (int32 I=0;I<Ref.GetNum();++I)
        {
            const int32 Index=Source.FindBoneIndex(Ref.GetBoneName(I));
            if (!UpperBody[I] || Index==INDEX_NONE) continue;
            FTransform Sample;
            Sequence->GetBoneTransform(Sample,FSkeletonPoseBoneIndex(Index),FAnimExtractContext(static_cast<double>(FMath::Clamp(Time,0.0f,static_cast<float>(Sequence->GetPlayLength())))),false);
            Out[I]=(Sample.GetRotation()*Source.GetRefBonePose()[Index].GetRotation().Inverse()*Ref.GetRefBonePose()[I].GetRotation()).GetNormalized();
        }
    };
    for (int32 G=0;G<3;++G)
    {
        const auto& Profile=GuardProfiles[G];
        SamplePose(GuardRotations[G],Profile.GuardPose,Profile.GuardPoseTime,BlockRotations,Profile.GuardPoseAsset,Profile.GuardPoseName);
        SamplePose(ReadyRotations[G*3],Profile.LightPose,Profile.LightPoseTime,AttackRotations[0],Profile.LightPoseAsset,Profile.LightPoseName);
        SamplePose(ReadyRotations[G*3+1],Profile.HeavyPose,Profile.HeavyPoseTime,AttackRotations[1],Profile.HeavyPoseAsset,Profile.HeavyPoseName);
        SamplePose(ReadyRotations[G*3+2],Profile.PiercePose,Profile.PiercePoseTime,AttackRotations[2],Profile.PiercePoseAsset,Profile.PiercePoseName);
    }
}
