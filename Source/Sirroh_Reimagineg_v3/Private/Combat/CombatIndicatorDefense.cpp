#include "Combat/DirectionalCombatComponent.h"
#include "Combat/CombatVitalityComponent.h"
#include "Combat/CombatDefenseProjectile.h"
#include "Combat/CombatReactionCatalog.h"
#include "Sword/ProceduralSword.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "CombatDragonWeaponAdapter.h"
#include "Components/CapsuleComponent.h"

namespace
{
    TArray<TWeakObjectPtr<UDirectionalCombatComponent>> Fighters;
    TMap<TWeakObjectPtr<UWorld>,TMap<FGuid,float>> WeaponDurability;
    class SCombatDefenseOverlay : public SLeafWidget
    {
    public:
        SLATE_BEGIN_ARGS(SCombatDefenseOverlay) {} SLATE_END_ARGS()
        void Construct(const FArguments&,UDirectionalCombatComponent* InOwner) { Owner=InOwner; SetVisibility(EVisibility::HitTestInvisible); }
        virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }
        virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle&,bool) const override
        {
            if (!Owner.IsValid()) return Layer;
            const auto* Pawn=Cast<APawn>(Owner->GetOwner());
            auto* PC=Pawn?Cast<APlayerController>(Pawn->GetController()):nullptr;
            if (!PC) return Layer;
            int32 W,H; PC->GetViewportSize(W,H); if (W<=0 || H<=0) return Layer;
            auto Project=[&](FVector P,FVector2D& Screen)
            { if (!PC->ProjectWorldLocationToScreen(P,Screen)) return false; Screen*=G.GetLocalSize()/FVector2D(W,H); return true; };
            auto Line=[&](const TArray<FVector2D>& Points,FLinearColor Color,float Thickness)
            { if (Points.Num()>1) FSlateDrawElement::MakeLines(Out,Layer,G.ToPaintGeometry(),Points,ESlateDrawEffect::None,Color,true,Thickness); };
            for (const auto& Weak:Fighters)
            {
                const auto* C=Weak.Get(); if (!C || C->GetWorld()!=Owner->GetWorld() || !C->UsesIndicatorDefense()) continue;
                const auto Frame=C->GetDefenseFrame(); const double Now=C->GetDefenseTime();
                if (C->IsCombatActive() && Frame.bCanDefend)
                {
                    auto Arc=[&](double Start,double End,float Radius,FLinearColor Color,float Width)
                    {
                        TArray<FVector2D> P;
                        const int32 Steps=FMath::Max(8,FMath::CeilToInt(FMath::Abs(End-Start)*20));
                        for (int32 I=0;I<=Steps;++I)
                        { const double A=FMath::Lerp(Start,End,double(I)/Steps); FVector2D S;
                          if (Project(Frame.Centre+FVector(FMath::Cos(A)*Radius,0,FMath::Sin(A)*Radius),S)) P.Add(S); }
                        Line(P,Color,Width);
                    };
                    Arc(0,2*PI,Frame.Radius,FLinearColor(0.3,0.65,0.85,0.35),1);
                    const double Aim=FMath::Atan2(Frame.Paddle.Y,Frame.Paddle.X);
                    const float Half=FCombatDefenseResolver::PaddleHalfAngle(Frame);
                    Arc(Aim-Half,Aim+Half,Frame.Radius,FLinearColor(0.1,0.8,1),5);
                    Arc(Aim-Half*Frame.PerfectFraction,Aim+Half*Frame.PerfectFraction,Frame.Radius,FLinearColor(1,0.9,0.2),6);
                }
                for (const auto& T:C->GetDefenseThreats())
                {
                    if (Now<T.StartTime || Now>T.ExpireTime || (T.Outcome!=ECombatDefenseOutcome::Pending && T.Outcome!=ECombatDefenseOutcome::Miss)) continue;
                    const auto& R=C->GetLastDefenseResult();
                    if (R.ThreatId==T.Id && (R.IsDefense() || R.Outcome==ECombatDefenseOutcome::Cancelled || R.Outcome==ECombatDefenseOutcome::BodyHit)) continue;
                    FVector2D S; if (!Project(T.Position(Now),S)) continue;
                    const float Alpha=T.IsFlashing(Now)?(FMath::Fmod(Now*12,1)<0.5?1.f:0.2f):0.85f;
                    FVector2D Ahead;
                    if (Project(T.Position(Now)+T.Velocity.GetSafeNormal()*10,Ahead))
                    {
                        const FVector2D Forward=(Ahead-S).GetSafeNormal();
                        const FVector2D Side(-Forward.Y,Forward.X);
                        // Open incoming chevron: reads as a direction cue, not a flying ball.
                        Line({S-Forward*9+Side*10,S,S-Forward*9-Side*10},FLinearColor(1,0.04,0.02,Alpha),3);
                    }
                }
                const auto& R=C->GetLastDefenseResult();
                if (R.IsDefense() && Now-R.Time>=0 && Now-R.Time<0.55)
                {
                    FVector2D S; if (Project(Frame.Centre+FVector(0,0,Frame.Radius+15),S))
                    {
                        const FString Text=R.Outcome==ECombatDefenseOutcome::Parry?TEXT("PARRY"):R.Outcome==ECombatDefenseOutcome::PerfectBlock?TEXT("PERFECT BLOCK"):TEXT("BLOCK");
                        FSlateDrawElement::MakeText(Out,Layer+1,G.ToPaintGeometry(FVector2D(220,24),FSlateLayoutTransform(S-FVector2D(55,0))),Text,FCoreStyle::GetDefaultFontStyle("Bold",12),ESlateDrawEffect::None,FLinearColor(1,0.9,0.2));
                    }
                }
            }
            if (Owner->UsesIndicatorDefense() && Owner->IsCombatActive())
                FSlateDrawElement::MakeText(Out,Layer+1,G.ToPaintGeometry(FVector2D(450,24),FSlateLayoutTransform(FVector2D(20,G.GetLocalSize().Y-65))),
                    FString::Printf(TEXT("BLOCK V3 | PPM: Parry podczas migania | %.0f/100"),Owner->GetDefenseDurability()),FCoreStyle::GetDefaultFontStyle("Regular",11),ESlateDrawEffect::None,FLinearColor::White);
            return Layer+1;
        }
        TWeakObjectPtr<UDirectionalCombatComponent> Owner;
    };
}

FCombatDefenseFrame UDirectionalCombatComponent::GetDefenseFrame() const
{
    FCombatDefenseFrame F; F.Centre=GetOwner()->GetActorLocation()+FVector(0,0,Rig.PelvisHeight+25);
    FTransform Torso; if (TryGetContactBone(TEXT("spine_02"),Torso)) F.Centre=Torso.GetLocation();
    const auto W=GetEquippedWeaponGeometry();
    F.WeaponLength=FVector::Distance(W.BladeBaseLocal,W.BladeTipLocal)+W.HandleLength;
    F.Radius=90*GetOwner()->GetActorScale3D().GetAbsMax(); F.HalfDepth=35*GetOwner()->GetActorScale3D().GetAbsMax();
    const FVector Aim=GetOwner()->GetActorLocation()+FVector(AimOffset)-F.Centre;
    F.Paddle=FVector2D(Aim.X,Aim.Z).GetSafeNormal();
    F.bCanDefend=UsesIndicatorDefense() && bCombatActive && CanFight() && !Reaction.bDisarmed && EquippedSword.InstanceId.IsValid();
    return F;
}
void UDirectionalCombatComponent::UpdateIndicatorDefense()
{
    // Use the movement root so walking, attack root motion and NPC sweeps share separation.
    // 54 cm per fighting capsule gives 108 cm spacing around a 90 cm guard, still within sword reach.
    auto* Capsule=Cast<UCapsuleComponent>(GetOwner()->GetRootComponent());
    if (DefenseCapsule.Get()!=Capsule)
    {
        if (DefenseCapsule.IsValid() && DefenseOriginalCapsuleRadius>0)
            DefenseCapsule->SetCapsuleRadius(DefenseOriginalCapsuleRadius);
        DefenseCapsule=Capsule;
        DefenseOriginalCapsuleRadius=Capsule?Capsule->GetUnscaledCapsuleRadius():-1;
    }
    if (Capsule && DefenseOriginalCapsuleRadius>0)
    {
        const bool Fighting=UsesIndicatorDefense() && bCombatActive && CanFight() && !Reaction.bDisarmed;
        const float Radius=Fighting?FMath::Max(DefenseOriginalCapsuleRadius,
            FMath::Min(54.f,Capsule->GetUnscaledCapsuleHalfHeight())):DefenseOriginalCapsuleRadius;
        if (!FMath::IsNearlyEqual(Capsule->GetUnscaledCapsuleRadius(),Radius)) Capsule->SetCapsuleRadius(Radius);
    }
    if (!UsesIndicatorDefense()) return;
    Fighters.RemoveAll([](const auto& F){return !F.IsValid();}); Fighters.AddUnique(this);
    if (IsLocal() && !bAIControlled && !DefenseOverlay && GetWorld()->GetGameViewport())
    { DefenseOverlay=SNew(SCombatDefenseOverlay,this); GetWorld()->GetGameViewport()->AddViewportWidgetContent(DefenseOverlay.ToSharedRef(),12); }
    if (!GetOwner()->HasAuthority())
    {
        EquippedSword.Durability=DefenseDurability;
        if (auto* Sword=ActiveSword(); Sword && Sword->Definition.InstanceId==EquippedSword.InstanceId) Sword->Definition.Durability=DefenseDurability;
        return;
    }
    for (auto It=WeaponDurability.CreateIterator();It;++It) if (!It.Key().IsValid()) It.RemoveCurrent();
    if (const auto* Health=GetOwner()->FindComponentByClass<UCombatVitalityComponent>(); Health && DefenseLifeId!=Health->LifeId)
    {
        DefenseLifeId=Health->LifeId; DefenseResolver.Reset(); DefenseThreats.Reset(); DefenseWeapon.Invalidate();
        DefenseStepStart=DefenseStepEnd=DefenseAdvancedTime=-1;
    }
    auto& Values=WeaponDurability.FindOrAdd(GetWorld());
    if (DefenseWeapon!=EquippedSword.InstanceId)
    {
        DefenseWeapon=EquippedSword.InstanceId;
        if (DefenseWeapon.IsValid())
        {
            if (!Values.Contains(DefenseWeapon)) Values.Add(DefenseWeapon,FMath::IsFinite(EquippedSword.Durability)?FMath::Clamp(EquippedSword.Durability,0.f,100.f):100.f);
            DefenseDurability=Values.FindChecked(DefenseWeapon);
        }
        else DefenseDurability=0;
        DefenseResolver.SetWeapon(DefenseWeapon,DefenseDurability);
        GetOwner()->ForceNetUpdate();
    }
    EquippedSword.Durability=DefenseDurability;
}
void UDirectionalCombatComponent::RemoveDefenseOverlay()
{
    Fighters.Remove(this);
    if (DefenseCapsule.IsValid() && DefenseOriginalCapsuleRadius>0)
        DefenseCapsule->SetCapsuleRadius(DefenseOriginalCapsuleRadius);
    DefenseCapsule.Reset(); DefenseOriginalCapsuleRadius=-1;
    if (DefenseOverlay && GetWorld() && GetWorld()->GetGameViewport()) GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(DefenseOverlay.ToSharedRef());
    DefenseOverlay.Reset();
}
void UDirectionalCombatComponent::AddDefenseThreat(const FCombatDefenseThreat& Threat)
{
    if (!GetOwner()->HasAuthority() || !UsesIndicatorDefense()) return;
    UpdateIndicatorDefense();
    if (!DefenseResolver.AddThreat(Threat)) return;
    DefenseThreats.Add(Threat); GetOwner()->ForceNetUpdate();
}
void UDirectionalCombatComponent::BeginDefenseStep(double Time)
{
    if (!UsesIndicatorDefense()) return;
    UpdateIndicatorDefense();
    DefensePreviousFrame=DefenseCurrentFrame; DefenseCurrentFrame=GetDefenseFrame();
    DefenseStepStart=DefenseStepEnd; DefenseStepEnd=Time;
    if (DefenseStepStart<0) { DefenseStepStart=Time; DefensePreviousFrame=DefenseCurrentFrame; DefenseAdvancedTime=Time; }
    CancelInvalidDefenseThreats(Time);
}
void UDirectionalCombatComponent::CancelInvalidDefenseThreats(double Time)
{
    for (const auto& T:DefenseThreats)
    {
        if (T.bRanged) continue;
        const auto* A=IsValid(T.Attacker)?T.Attacker->FindComponentByClass<UDirectionalCombatComponent>():nullptr;
        if (!A || !A->BodyAttack.bPrepared || A->BodyAttack.Id!=uint32(T.AttackId) || A->EquippedSword.InstanceId!=T.WeaponId || A->MotionEpoch!=T.Epoch)
        {
            const auto* R=DefenseResolver.FindResult(T.Id);
            if (R && R->Outcome==ECombatDefenseOutcome::Pending)
            { DefenseResolver.Cancel(T.Id,Time); HandleDefenseResult(*DefenseResolver.FindResult(T.Id)); }
        }
    }
}
double UDirectionalCombatComponent::NextDefenseEvent(double Time) const
{
    if (!UsesIndicatorDefense() || DefenseStepEnd<=DefenseStepStart || Time<=DefenseAdvancedTime || DefenseAdvancedTime>=DefenseStepEnd) return Time;
    Time=FMath::Min(Time,DefenseStepEnd);
    auto At=[&](double T){ auto F=DefenseCurrentFrame; F.Centre=FMath::Lerp(DefensePreviousFrame.Centre,DefenseCurrentFrame.Centre,FMath::Clamp((T-DefenseStepStart)/(DefenseStepEnd-DefenseStepStart),0.,1.)); return F; };
    // Preview has no actor callbacks. Partition all fighters at the earliest event before committing costs/cancels.
    auto Preview=DefenseResolver; TArray<FCombatDefenseResult> Results;
    Preview.Advance(DefenseAdvancedTime,Time,At(DefenseAdvancedTime),At(Time),Results);
    double Next=Time; for (const auto& R:Results) Next=FMath::Min(Next,R.Time);
    return FMath::Min(Time,FMath::Max(DefenseAdvancedTime+1.e-7,Next));
}
void UDirectionalCombatComponent::AdvanceDefenseTo(double Time)
{
    if (!UsesIndicatorDefense() || DefenseStepEnd<=DefenseStepStart || Time<=DefenseAdvancedTime) return;
    Time=FMath::Min(Time,DefenseStepEnd);
    auto At=[&](double T){ auto F=DefenseCurrentFrame; F.Centre=FMath::Lerp(DefensePreviousFrame.Centre,DefenseCurrentFrame.Centre,FMath::Clamp((T-DefenseStepStart)/(DefenseStepEnd-DefenseStepStart),0.,1.)); return F; };
    TArray<FCombatDefenseResult> Results;
    DefenseResolver.Advance(DefenseAdvancedTime,Time,At(DefenseAdvancedTime),At(Time),Results); DefenseAdvancedTime=Time;
    for (const auto& R:Results) HandleDefenseResult(R);
    if (DefenseThreats.RemoveAll([&](const auto& T){return Time>T.ExpireTime;})>0) GetOwner()->ForceNetUpdate();
}
bool UDirectionalCombatComponent::TryDefenseParry()
{
    bDefenseConsumedHeavy=false;
    if (!UsesIndicatorDefense() || !GetDefenseFrame().bCanDefend) return false;
    const double Now=CombatTime();
    for (const auto& T:DefenseThreats) if (T.Outcome==ECombatDefenseOutcome::Pending && T.IsFlashing(Now))
    { bDefenseConsumedHeavy=true; ServerDefenseParry(); return true; }
    return false;
}
void UDirectionalCombatComponent::ServerDefenseParry_Implementation()
{ if (UsesIndicatorDefense()) DefenseResolver.TryParry(CombatTime(),GetDefenseFrame()); }
void UDirectionalCombatComponent::HandleDefenseResult(const FCombatDefenseResult& R)
{
    if (R.IsDefense())
    {
        if (R.DamageScale<=0)
            if (const auto* T=DefenseThreats.FindByPredicate([&](const auto& V){return V.Id==R.ThreatId;}))
                if (auto* P=Cast<ACombatDefenseProjectile>(T->Projectile)) P->DefenseStopTime=FMath::Min(P->DefenseStopTime,R.Time);
        DefenseDurability=DefenseResolver.GetDurability(); EquippedSword.Durability=DefenseDurability;
        WeaponDurability.FindOrAdd(GetWorld()).Add(DefenseWeapon,DefenseDurability);
        if (auto* Sword=ActiveSword()) { Sword->Definition.Durability=DefenseDurability; Sword->ForceNetUpdate(); }
        // Guard interception interrupts one's own attack, but never extends the omniblock clock.
        BodyAttack.bPrepared=false; BufferedBodyAttack=FBufferedBodyAttack(); CancelInertiaMovement(); bMotionSubmittedAttackActive=false;
        auto Profile=UCombatMotionCatalog::GetProfile(MotionCoreStyle);
        if (!Profile)
        {
            auto* Table=LoadObject<UDataTable>(nullptr,TEXT("/Game/Systems/Procedural_Combat/DT_Combat_Motion_v3.DT_Combat_Motion_v3"));
            FCombatStyleDefinition Style; FString Error;
            if (UCombatMotionCatalog::StyleForProfile(Table,IsTwoHandedGrip()?TEXT("Gaidheal_2H"):TEXT("Gaidheal_1H"),Style,Error)) Profile=UCombatMotionCatalog::GetProfile(Style);
        }
        if (Profile)
        {
            const auto Frame=GetDefenseFrame(); const FVector Offset=R.Point-Frame.Centre;
            const FName Direction=UCombatMotionCatalog::DirectionName(FVector2D(Offset.X*FMath::Sign(GetOwner()->GetActorForwardVector().X),Offset.Z));
            const FName Category=R.Outcome==ECombatDefenseOutcome::Parry?TEXT("Received.Parry"):
                R.Outcome==ECombatDefenseOutcome::PerfectBlock?TEXT("Received.PerfectBlock"):TEXT("Received.Block");
            const auto Choice=UCombatMotionCatalog::Choose(*Profile,Category,Direction,TEXT("InPlace"),TEXT("Any"),int32(GetTypeHash(R.ThreatId)));
            if (Choice.bFound)
            {
                ++MotionReaction.Serial; MotionReaction.Start=CombatTime(); MotionReaction.GuardAt=MotionReaction.Start+0.15;
                MotionReaction.bWeapon=true; MotionReaction.bInterruptsAttack=true; MotionReaction.Strength=0.85;
                MotionReaction.Impulse=FVector::ZeroVector; MotionReaction.Point=R.Point;
                MotionReaction.AnimationRow=Choice.Variant.Id; MotionReaction.DefenseProfile=Profile->ProfileId;
                MotionReaction.AnimationEntry=FMath::Clamp(Choice.Variant.ContactPoseTime-0.06f,0.f,1.f);
                MotionReaction.AnimationExit=FMath::Clamp(Choice.Variant.ContactPoseTime+0.12f,0.f,1.f);
                MotionReaction.AnimationDuration=0.32f; OnRep_MotionReaction();
            }
        }
        if (R.Outcome==ECombatDefenseOutcome::Parry)
            if (const auto* T=DefenseThreats.FindByPredicate([&](const auto& V){return V.Id==R.ThreatId;}))
                if (IsValid(T->Attacker)) if (auto* A=T->Attacker->FindComponentByClass<UDirectionalCombatComponent>())
                {
                    A->BodyAttack.bPrepared=false; A->BufferedBodyAttack=FBufferedBodyAttack();
                    A->bMotionSubmittedAttackActive=false; A->CancelInertiaMovement();
                    A->MotionReaction.GuardAt=FMath::Max(A->MotionReaction.GuardAt,A->CombatTime()+0.2);
                    A->GetOwner()->ForceNetUpdate();
                }
    }
    if (auto* T=DefenseThreats.FindByPredicate([&](const auto& V){return V.Id==R.ThreatId;})) T->Outcome=R.Outcome;
    MulticastDefenseResult(R); GetOwner()->ForceNetUpdate();
}
void UDirectionalCombatComponent::MulticastDefenseResult_Implementation(FCombatDefenseResult R)
{
    LastDefenseResult=R;
    if (R.IsDefense())
    {
        FCombatContactEvent Event; Event.Attacker=GetOwner(); Event.Point=R.Point;
        Event.Direction=(R.Point-GetDefenseFrame().Centre).GetSafeNormal();
        Event.Intensity=ECombatIntensity::Medium; Event.bSwordCollision=true; OnContact.Broadcast(Event);
    }
}
