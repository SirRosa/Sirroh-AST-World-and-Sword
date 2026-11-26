#include "Combat/CombatEffectsComponent.h"
#include "Combat/DirectionalCombatComponent.h"
#include "Combat/CombatVitalityComponent.h"
#include "Combat/GrimEffectsSubsystem.h"
#include "DragonIKFootStepsComponent.h"
#include "MoverComponent.h"
#include "Engine/World.h"
#include "Components/PrimitiveComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

UCombatEffectsComponent::UCombatEffectsComponent()
{
    PrimaryComponentTick.bCanEverTick=true;
    PrimaryComponentTick.TickGroup=TG_PostUpdateWork;
}
void UCombatEffectsComponent::BeginPlay()
{
    Super::BeginPlay();
    Combat=GetOwner()->FindComponentByClass<UDirectionalCombatComponent>();
    if (!Combat || GetNetMode()==NM_DedicatedServer) { SetComponentTickEnabled(false); return; }
    Effects=GetWorld()->GetSubsystem<UGrimEffectsSubsystem>();
    Vitality=GetOwner()->FindComponentByClass<UCombatVitalityComponent>();
    Mover=GetOwner()->FindComponentByClass<UMoverComponent>();
    Feet=GetOwner()->FindComponentByClass<UDragonIKFootStepsComponent>();
    if (Feet) Feet->OnCharacterFootStep.AddDynamic(this,&UCombatEffectsComponent::FootContact);
    PreviousPosition=GetOwner()->GetActorLocation();
    AddTickPrerequisiteComponent(Combat);
    Combat->OnContact.AddUObject(this,&UCombatEffectsComponent::Contact);
    Combat->OnPhysicalWeaponContact.AddDynamic(this,&UCombatEffectsComponent::PhysicalContact);
    Combat->OnMotionBodyHit.AddDynamic(this,&UCombatEffectsComponent::BodyContact);
}
bool UCombatEffectsComponent::AcceptImpact(const FVector& Point)
{
    if (!Effects || !Effects->IsNear(Point)) return false;
    const float Now=GetWorld()->GetTimeSeconds();
    if (Now-LastImpact<0.075f && FVector::DistSquared(Point,LastImpactPoint)<FMath::Square(30.f)) return false;
    LastImpact=Now; LastImpactPoint=Point; return true;
}
void UCombatEffectsComponent::Contact(const FCombatContactEvent& Event)
{
    if (Event.Attacker!=GetOwner() || !AcceptImpact(Event.Point)) return;
    const EGrimSurface Kind=Event.EffectSurface<=4?static_cast<EGrimSurface>(Event.EffectSurface):Event.bSwordCollision?EGrimSurface::Metal:
        Event.Target && Event.Target->FindComponentByClass<UCombatVitalityComponent>()?EGrimSurface::Flesh:
        UGrimEffectsSubsystem::Surface(Event.Target);
    Effects->Impact(Kind,Event.Point,Event.Direction,0.8f+int32(Event.Intensity)*0.3f);
}
void UCombatEffectsComponent::PhysicalContact(const FCombatPhysicalContact& Event)
{
    if (!Combat || !Event.bSelfAttacking) return;
    if (Event.bOtherAttacking && Event.OtherActor && GetOwner()->GetPathName()>Event.OtherActor->GetPathName()) return;
    // Body hits have their own confirmed damage path; proxy collisions must not duplicate blood.
    if (!Event.bWeaponClash && Event.OtherActor && Event.OtherActor->FindComponentByClass<UCombatVitalityComponent>()) return;
    if (!AcceptImpact(Event.Point)) return;
    const auto Kind=Event.bWeaponClash?EGrimSurface::Metal:static_cast<EGrimSurface>(FMath::Min(Event.EffectSurface,uint8(4)));
    Effects->Impact(Kind,Event.Point,Event.Normal,0.8f+int32(Combat->GetSwingIntensity())*0.3f);
}
void UCombatEffectsComponent::BodyContact(const FCombatMotionBodyHit& Event)
{
    // Projectile events are broadcast by the defender, melee by the attacker.
    if ((!Event.bRanged && Event.Attacker!=GetOwner()) || Event.DamageApplied<=0 || Event.DamageScale<=0 || Event.Action==TEXT("Kick")) return;
    if (AcceptImpact(Event.Point)) Effects->Impact(EGrimSurface::Flesh,Event.Point,Event.ContactVelocity.GetSafeNormal(),
        Event.AttackType==EDirectionalAttack::Heavy?1.5f:1.f);
}
void UCombatEffectsComponent::FootContact(FName Bone,FTransform Transform,float Height,FHitResult Hit)
{
    if (!Effects || !Hit.bBlockingHit || GetOwner()->GetVelocity().Size2D()<35) return;
    if (Mover && !Mover->HasGameplayTag(Mover_IsOnGround,false)) return;
    const float Now=GetWorld()->GetTimeSeconds(); LastDragonStep=Now;
    if (Now-LastStep<0.16f) return;
    LastStep=Now; StepDistance=0;
    Effects->Step(UGrimEffectsSubsystem::Surface(Hit.GetActor(),Hit.GetComponent(),Hit.PhysMaterial.Get()),Hit.ImpactPoint,GetOwner()->GetVelocity().Size2D()>350);
}
void UCombatEffectsComponent::Movement(float Delta)
{
    const FVector Position=GetOwner()->GetActorLocation();
    const float Distance=FVector::Dist2D(Position,PreviousPosition); PreviousPosition=Position;
    if (Distance>150 || (Mover && !Mover->HasGameplayTag(Mover_IsOnGround,false)) || (Vitality && Vitality->IsIncapacitated()))
    { StepDistance=0; return; }
    const float Speed=Distance/FMath::Max(Delta,0.001f);
    if (Speed<35) { StepDistance=0; return; }
    StepDistance+=Distance;
    const float Now=GetWorld()->GetTimeSeconds();
    // Some generated pose bridges have no Dragon foot-notify node. Distance cadence is a presentation fallback.
    if (Now-LastDragonStep<0.65f || Now-LastStep<0.20f || StepDistance<(Speed>350?115:85)) return;
    StepDistance=0;
    FTransform Foot; FVector Start=Position;
    if (Combat->TryGetContactBone(bLeftFoot?TEXT("foot_l"):TEXT("foot_r"),Foot)) Start=Foot.GetLocation();
    FHitResult Hit; FCollisionQueryParams Params(SCENE_QUERY_STAT(GrimFootstep),false,GetOwner()); Params.bReturnPhysicalMaterial=true;
    if (GetWorld()->LineTraceSingleByChannel(Hit,Start+FVector(0,0,18),Start-FVector(0,0,35),ECC_Visibility,Params)
        && Hit.ImpactNormal.Z>0.35f)
    {
        LastStep=Now; bLeftFoot=!bLeftFoot;
        Effects->Step(UGrimEffectsSubsystem::Surface(Hit.GetActor(),Hit.GetComponent(),Hit.PhysMaterial.Get()),Hit.ImpactPoint,Speed>350);
    }
}
void UCombatEffectsComponent::TickComponent(float Delta,ELevelTick Tick,FActorComponentTickFunction* Function)
{
    Super::TickComponent(Delta,Tick,Function);
    if (!Combat || !Effects) return;
    if (!Effects->IsNear(GetOwner()->GetActorLocation()))
    { bTrailHistory=false; PreviousPosition=GetOwner()->GetActorLocation(); StepDistance=0; return; }
    MotionClock+=Delta;
    if (MotionClock>=0.05f) { Movement(MotionClock); MotionClock=0; }
    BleedClock+=Delta;
    if (BleedClock>=0.2f)
    {
        BleedClock=0;
        if (Vitality && Vitality->Blood>0)
        {
            int32 Count=0;
            for (const auto& Part:Vitality->Parts) if (Part.Bleeding>0.1f)
            {
                FTransform Wound;
                if (Combat->TryGetContactBone(Part.Bone,Wound)) Effects->Bleed(Wound.GetLocation(),Part.Bleeding*0.12f);
                if (++Count>=4) break;
            }
        }
    }
    const auto* Sample=Combat->GetMotionCoreWeaponSample();
    if (!Combat->IsCombatActive() || Combat->GetCombatPhase()!=EDirectionalCombatPhase::Attacking || (Sample && (!Sample->bAttackActive || Sample->bDiscontinuity)))
    { bTrailHistory=false; return; }
    const auto Frame=Combat->GetWeaponFrame();
    const FVector Base=Sample?Sample->BladeBase:Frame.Base(Combat->GetWeaponPixelScale());
    const FVector Tip=Sample?Sample->BladeTip:Frame.Tip(Combat->GetWeaponPixelScale());
    if (SwingId!=Combat->GetAttackId())
    {
        SwingId=Combat->GetAttackId(); bTrailHistory=false;
        Effects->Swing((Base+Tip)*0.5,0.8f+int32(Combat->GetSwingIntensity())*0.3f);
    }
    if (bTrailHistory && FVector::DistSquared(OldTip,Tip)>1) Effects->Trail(OldBase,OldTip,Base,Tip);
    OldBase=Base; OldTip=Tip; bTrailHistory=true;
}
void UCombatEffectsComponent::EndPlay(EEndPlayReason::Type Reason)
{
    if (Combat)
    {
        Combat->OnContact.RemoveAll(this);
        Combat->OnPhysicalWeaponContact.RemoveDynamic(this,&UCombatEffectsComponent::PhysicalContact);
        Combat->OnMotionBodyHit.RemoveDynamic(this,&UCombatEffectsComponent::BodyContact);
    }
    if (Feet) Feet->OnCharacterFootStep.RemoveDynamic(this,&UCombatEffectsComponent::FootContact);
    Super::EndPlay(Reason);
}
