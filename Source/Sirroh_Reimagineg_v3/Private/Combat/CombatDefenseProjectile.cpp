#include "Combat/CombatDefenseProjectile.h"
#include "CombatMotionContactSubsystem.h"
#include "Combat/DirectionalCombatComponent.h"
#include "Combat/CombatVitalityComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Engine/StaticMesh.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

ACombatDefenseProjectile::ACombatDefenseProjectile()
{
    bReplicates=true; SetReplicateMovement(true); NetUpdateFrequency=30;
    Collision=CreateDefaultSubobject<USphereComponent>(TEXT("ProjectileCollision")); SetRootComponent(Collision);
    Collision->InitSphereRadius(3); Collision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Collision->SetCollisionObjectType(ECC_WorldDynamic); Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
    for (auto Channel:{ECC_WorldStatic,ECC_WorldDynamic,ECC_Pawn,ECC_GameTraceChannel5}) Collision->SetCollisionResponseToChannel(Channel,ECR_Block);
    Collision->OnComponentHit.AddDynamic(this,&ACombatDefenseProjectile::Impact);
    Visual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileVisual")); Visual->SetupAttachment(Collision);
    Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Visual->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"))); Visual->SetRelativeScale3D(FVector(0.06));
    Movement=CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
    Movement->UpdatedComponent=Collision; Movement->InitialSpeed=0; Movement->MaxSpeed=0; Movement->ProjectileGravityScale=0;
    Movement->bRotationFollowsVelocity=true; Movement->bForceSubStepping=true;
}
ACombatDefenseProjectile* ACombatDefenseProjectile::Launch(UObject* Context,AActor* Shooter,FVector Origin,FVector Velocity,bool Pierce,float Mass,float Gravity)
{
    UWorld* World=GEngine?GEngine->GetWorldFromContextObject(Context,EGetWorldErrorMode::ReturnNull):nullptr;
    if (!World || World->GetNetMode()==NM_Client || !IsValid(Shooter) || Origin.ContainsNaN() || Velocity.ContainsNaN() || Velocity.IsNearlyZero()
        || !FMath::IsFinite(Mass) || Mass<=0 || !FMath::IsFinite(Gravity)) return nullptr;
    const FTransform Transform(Velocity.Rotation(),Origin);
    auto* P=World->SpawnActorDeferred<ACombatDefenseProjectile>(StaticClass(),Transform,Shooter,Cast<APawn>(Shooter),ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!P) return nullptr;
    P->bPierce=Pierce; P->ProjectileMass=Mass; P->Movement->Velocity=Velocity; P->Movement->ProjectileGravityScale=Gravity;
    P->FinishSpawning(Transform); return P;
}
void ACombatDefenseProjectile::BeginPlay()
{
    Super::BeginPlay();
    if (GetOwner()) Collision->IgnoreActorWhenMoving(GetOwner(),true);
    if (!HasAuthority()) { Movement->SetComponentTickEnabled(false); Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision); return; }
    Lifetime=FMath::IsFinite(Lifetime)?FMath::Clamp(Lifetime,0.05f,30.f):5.f;
    ProjectileMass=FMath::IsFinite(ProjectileMass)?FMath::Max(0.01f,ProjectileMass):0.2f;
    ThreatId=FGuid::NewGuid(); SetLifeSpan(Lifetime);
    GetWorld()->GetSubsystem<UCombatMotionContactSubsystem>()->RegisterProjectile(this);
}
void ACombatDefenseProjectile::Impact(UPrimitiveComponent*,AActor* Other,UPrimitiveComponent*,FVector,const FHitResult& Hit)
{
    if (!HasAuthority() || bImpactQueued || Other==GetOwner()) return;
    const auto* Combat=IsValid(Other)?Other->FindComponentByClass<UDirectionalCombatComponent>():nullptr;
    if (!Combat) { Destroy(); return; }
    bImpactQueued=true;
    FCombatMotionBodyHit Body; Body.bRanged=true; Body.SourceMass=ProjectileMass; Body.ThreatId=ThreatId;
    Body.Attacker=GetOwner()?GetOwner():this; Body.Target=Other; Body.AttackType=bPierce?EDirectionalAttack::Pierce:EDirectionalAttack::Light;
    Body.BodyRegion=TEXT("pelvis"); Body.Point=Hit.ImpactPoint; Body.ContactVelocity=Movement->Velocity; Body.RelativeSpeed=Body.ContactVelocity.Size();
    Body.Time=GetWorld()->GetTimeSeconds(); Body.Action=TEXT("Projectile");
    double Distance=DBL_MAX;
    for (FName Bone:{FName(TEXT("head")),FName(TEXT("spine_03")),FName(TEXT("pelvis")),FName(TEXT("thigh_l")),FName(TEXT("thigh_r"))})
    { FTransform T; if (Combat->TryGetContactBone(Bone,T)) { const double D=FVector::DistSquared(T.GetLocation(),Body.Point); if (D<Distance) {Distance=D; Body.BodyRegion=Bone;} } }
    if (const auto* H=Other->FindComponentByClass<UCombatVitalityComponent>()) Body.TargetLifeId=H->LifeId;
    Movement->StopMovementImmediately(); Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GetWorld()->GetSubsystem<UCombatMotionContactSubsystem>()->QueueProjectileHit(this,Body);
}
void ACombatDefenseProjectile::EndPlay(EEndPlayReason::Type Reason)
{
    if (HasAuthority() && GetWorld()) if (auto* S=GetWorld()->GetSubsystem<UCombatMotionContactSubsystem>()) S->CancelProjectile(this);
    Super::EndPlay(Reason);
}
