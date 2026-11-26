#include "CombatDragonWeaponAdapter.h"
#include "Combat/GrimEffectsSubsystem.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Combat/DirectionalCombatComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Actor.h"

UCombatDragonWeaponAdapter::UCombatDragonWeaponAdapter()
{
    // The combat post-physics tick samples the result, then submits the next grip target.
    // Dragon's autonomous grab/arm calibration is intentionally not part of this adapter.
    PrimaryComponentTick.bCanEverTick=false;
    physanim_tag_index=7301;
    SetIsReplicatedByDefault(false);
}

bool UCombatDragonWeaponAdapter::Start(USkeletalMeshComponent& Mesh,
    const FCombatWeaponDefinition& Definition, const FTransform& WeaponWorld)
{
    Stop();
    if (!weapon_physics_handle)
    {
        Linear_Strength=500000; Angular_Strength=200000;
        Linear_Damp=5000; Angular_Damp=5000;
        FirstTimeInitialization(&Mesh);
        weapon_physics_handle->SetIsReplicated(false);
        weapon_physics_constraint->SetIsReplicated(false);
        weapon_physics_handle->bInterpolateTarget=false;
    }
    owning_skeleton=&Mesh;
    if (!Proxy)
    {
        Proxy=NewObject<UBoxComponent>(GetOwner(),TEXT("CombatWeaponBody"));
        Proxy->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Proxy->SetCollisionObjectType(ECC_GameTraceChannel7);
        Proxy->SetCollisionResponseToAllChannels(ECR_Ignore);
        Proxy->SetCollisionResponseToChannel(ECC_WorldStatic,ECR_Block);
        Proxy->SetCollisionResponseToChannel(ECC_WorldDynamic,ECR_Block);
        Proxy->SetCollisionResponseToChannel(ECC_GameTraceChannel5,ECR_Block);
        Proxy->SetCollisionResponseToChannel(ECC_GameTraceChannel7,ECR_Block);
        Proxy->SetGenerateOverlapEvents(false);
        Proxy->SetNotifyRigidBodyCollision(true);
        Proxy->SetUseCCD(true);
        Proxy->SetCanEverAffectNavigation(false);
        Proxy->SetHiddenInGame(true);
        Proxy->ComponentTags.Add(TEXT("CombatWeaponBody"));
        Proxy->OnComponentHit.AddDynamic(this,&UCombatDragonWeaponAdapter::OnWeaponHit);
        Proxy->RegisterComponent();
    }
    // Same authoritative primitive exists on dedicated servers, without a sword render actor.
    TInlineComponentArray<UPrimitiveComponent*> Primitives(GetOwner());
    for (auto* Component:Primitives) if (Component!=Proxy)
    {
        OwnerResponses.Add(Component,Component->GetCollisionResponseToChannel(ECC_GameTraceChannel7));
        Component->SetCollisionResponseToChannel(ECC_GameTraceChannel7,ECR_Ignore);
    }
    ProxyLocal=FTransform((Definition.BladeBaseLocal+Definition.BladeTipLocal)*0.5);
    Proxy->SetBoxExtent(FVector(Definition.BladeRadius,Definition.BladeRadius,
        FVector::Distance(Definition.BladeBaseLocal,Definition.BladeTipLocal)*0.5+Definition.BladeRadius));
    Proxy->SetWorldTransform(ProxyLocal*WeaponWorld,false,nullptr,ETeleportType::TeleportPhysics);
    if (const auto* Combat=GetOwner()->FindComponentByClass<UDirectionalCombatComponent>(); Combat && Combat->UsesIndicatorDefense())
        Proxy->SetCollisionResponseToChannel(ECC_GameTraceChannel7,ECR_Ignore);
    Proxy->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Proxy->SetSimulatePhysics(true);
    Proxy->SetMassOverrideInKg(NAME_None,FMath::Clamp(Definition.WeaponWeight,0.25f,20.0f));
    Proxy->SetEnableGravity(true);
    Proxy->SetPhysicsLinearVelocity(FVector::ZeroVector);
    Proxy->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
    const FTransform Grip=Definition.PrimaryGripLocal*WeaponWorld;
    weapon_input_component=Proxy;
    active_weapon_component=Proxy;
    weapon_physics_handle->GrabComponentAtLocationWithRotation(Proxy,NAME_None,Grip.GetLocation(),Grip.Rotator());
    is_grabbing=weapon_physics_handle->GetGrabbedComponent()==Proxy;
    perform_physics_simulation=is_grabbing;
    // Only explicit combat discontinuities reset the adapter; a blocked weapon never teleports.
    Teleport_Distance=UE_BIG_NUMBER;
    Weapon_Delta_Transform=Proxy->GetComponentTransform();
    WeaponId=Definition.WeaponInstanceId;
    DriveGrip(Grip,0);
    return is_grabbing;
}

void UCombatDragonWeaponAdapter::DriveGrip(const FTransform& Grip, uint32 AttackId, float Compliance, float Damping)
{
    CurrentAttackId=AttackId;
    const auto* Debug=GetOwner()->FindComponentByClass<UDirectionalCombatComponent>();
    const float Strength=Debug?Debug->DebugWeaponStrength:1.f, DampingScale=Debug?Debug->DebugWeaponDamping:1.f;
    if (weapon_physics_handle)
    {
        // Keep the grip attached but let angular momentum continue past destination.
        weapon_physics_handle->SetAngularStiffness(Strength*FMath::Lerp(200000.0f,2500.0f,FMath::Clamp(Compliance,0.0f,1.0f)));
        weapon_physics_handle->SetAngularDamping(DampingScale*FMath::Lerp(5000.0f,250.0f,FMath::Clamp(Compliance,0.0f,1.0f)));
    }
    if (Proxy) Proxy->SetAngularDamping(FMath::Clamp(Damping,0.1f,3.0f));
    dominant_hand_transform=Grip;
    ModifyWeaponTransform(); // Dragon physical handle consumes the already solved grip frame.
}

bool UCombatDragonWeaponAdapter::ReadWeapon(FTransform& Out) const
{
    if (!is_grabbing || !IsValid(Proxy) || !Proxy->IsSimulatingPhysics()) return false;
    Out=ProxyLocal.Inverse()*Proxy->GetComponentTransform();
    Out.SetScale3D(FVector::OneVector);
    return !Out.ContainsNaN() && Out.GetRotation().IsNormalized();
}

void UCombatDragonWeaponAdapter::OnWeaponHit(UPrimitiveComponent*, AActor* OtherActor,
    UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit)
{
    if (!is_grabbing || !GetOwner()->HasAuthority() || OtherActor==GetOwner()
        || !OtherComponent || NormalImpulse.ContainsNaN() || Hit.ImpactPoint.ContainsNaN()) return;
    // Physics may report multiple manifold points; retain the strongest event this frame.
    if (NormalImpulse.SizeSquared()<1 || (bPendingContact && NormalImpulse.SizeSquared()<=PendingContact.Impulse.SizeSquared())) return;
    PendingContact.WeaponId=WeaponId;
    PendingContact.AttackId=static_cast<int32>(CurrentAttackId);
    PendingContact.OtherActor=OtherActor;
    PendingContact.Point=Hit.ImpactPoint;
    PendingContact.Normal=Hit.ImpactNormal;
    PendingContact.EffectSurface=uint8(UGrimEffectsSubsystem::Surface(OtherActor,OtherComponent,Hit.PhysMaterial.Get()));
    PendingContact.Impulse=NormalImpulse.GetClampedToMaxSize(100000);
    const auto* OtherCombat=OtherActor?OtherActor->FindComponentByClass<UDirectionalCombatComponent>():nullptr;
    const auto* OwnCombat=GetOwner()->FindComponentByClass<UDirectionalCombatComponent>();
    PendingContact.bOtherAttacking=OtherCombat && OtherCombat->GetCombatPhase()==EDirectionalCombatPhase::Attacking;
    PendingContact.bSelfAttacking=OwnCombat && OwnCombat->GetCombatPhase()==EDirectionalCombatPhase::Attacking;
    PendingContact.bWeaponClash=OtherComponent->ComponentHasTag(TEXT("CombatWeaponBody"));
    bPendingContact=true;
}

bool UCombatDragonWeaponAdapter::ConsumeContact(FCombatPhysicalContact& Out)
{
    if (!bPendingContact) return false;
    Out=PendingContact;
    bPendingContact=false;
    return true;
}

void UCombatDragonWeaponAdapter::Stop()
{
    is_grabbing=false; perform_physics_simulation=false; perform_grab_weapon=false;
    bPendingContact=false;
    if (weapon_physics_handle) weapon_physics_handle->ReleaseComponent();
    if (IsValid(Proxy)) { Proxy->SetSimulatePhysics(false); Proxy->SetCollisionEnabled(ECollisionEnabled::NoCollision); }
    for (const auto& Entry:OwnerResponses) if (Entry.Key.IsValid())
        Entry.Key->SetCollisionResponseToChannel(ECC_GameTraceChannel7,Entry.Value);
    OwnerResponses.Empty();
}

void UCombatDragonWeaponAdapter::OnComponentDestroyed(bool bDestroyingHierarchy)
{
    Stop();
    if (IsValid(Proxy)) Proxy->DestroyComponent();
    Super::OnComponentDestroyed(bDestroyingHierarchy);
}

bool UCombatDragonWeaponAdapter::ReadMotion(FVector& Linear, FVector& Angular, FVector& Centre) const
{
    if (!is_grabbing || !IsValid(Proxy) || !Proxy->IsSimulatingPhysics()) return false;
    Linear=Proxy->GetPhysicsLinearVelocity();
    Angular=Proxy->GetPhysicsAngularVelocityInRadians();
    Centre=Proxy->GetCenterOfMass();
    return !Linear.ContainsNaN() && !Angular.ContainsNaN() && !Centre.ContainsNaN();
}

void UCombatDragonWeaponAdapter::ApplyBodyContact(const FVector& Impulse,const FVector& Point)
{
    if (Proxy && Proxy->IsSimulatingPhysics()) Proxy->AddImpulseAtLocation(Impulse,Point);
}
