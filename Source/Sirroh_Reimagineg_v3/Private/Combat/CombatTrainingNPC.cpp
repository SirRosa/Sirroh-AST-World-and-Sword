#include "Combat/CombatTrainingNPC.h"
#include "Character/SirrohCharacterIdentityComponent.h"
#include "Combat/CombatVitalityComponent.h"
#include "Combat/DirectionalCombatComponent.h"
#include "Sword/SwordWorldSubsystem.h"
#include "Procedural_World_Generator/PWGv2Generator.h"
#include "Components/CapsuleComponent.h"
#include "Components/TextRenderComponent.h"
#include "Animation/AnimSequence.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Engine/SkeletalMesh.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"


ACombatTrainingNPC::ACombatTrainingNPC()
{
    bReplicates=true; SetReplicateMovement(true);
    PrimaryActorTick.bCanEverTick=true;
    SetNetUpdateFrequency(30); SetMinNetUpdateFrequency(15);
    AutoPossessPlayer=EAutoReceiveInput::Disabled;
    AutoPossessAI=EAutoPossessAI::Disabled;
    Capsule=CreateDefaultSubobject<UCapsuleComponent>(TEXT("Body"));
    Capsule->InitCapsuleSize(30,90); Capsule->SetCollisionProfileName(TEXT("Pawn"));
    SetRootComponent(Capsule);
    Mesh=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PoseSkeleton"));
    Mesh->SetupAttachment(Capsule); Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    // The training NPC now uses the same normal 3D skeletal presentation path as the player.
    // DirectionalCombatComponent creates/fetches SirrohPoseBridge at runtime; the bridge keeps
    // this mesh as the animated pose source and renders the final procedural pose.
    Mesh->SetVisibility(true); Mesh->SetCastShadow(true);
    Mesh->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    CreateDefaultSubobject<USirrohCharacterIdentityComponent>(TEXT("CharacterIdentity"));
    Combat=CreateDefaultSubobject<UDirectionalCombatComponent>(TEXT("DirectionalCombat"));
    Combat->bUseMotionCoreV3=true;
    Combat->LightAttack.Duration=0.5f; Combat->LightAttack.Recovery=0.45f;
    Combat->HeavyAttack.Duration=0.7f; Combat->HeavyAttack.Recovery=0.65f;
    Combat->PierceAttack.Duration=0.45f; Combat->PierceAttack.Recovery=0.4f;
    Label=CreateDefaultSubobject<UTextRenderComponent>(TEXT("TrainingLabel"));
    Label->SetupAttachment(Capsule); Label->SetHorizontalAlignment(EHTA_Center);
    Label->SetWorldSize(12); Label->SetTextRenderColor(FColor(255,205,90));
    Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}


void ACombatTrainingNPC::ConfigureFromPlayer(APawn* Player,APWGv2Generator* Ground)
{
    if (!HasAuthority() || !Player) return;
    GroundProvider=Ground;
    const auto* SourceMesh=Player->FindComponentByClass<USkeletalMeshComponent>();
    const auto* SourceCapsule=Player->FindComponentByClass<UCapsuleComponent>();
    const FVector Center=SourceCapsule?SourceCapsule->GetComponentLocation():Player->GetActorLocation();
    if (SourceCapsule) CapsuleSize=FVector2D(SourceCapsule->GetScaledCapsuleRadius(),SourceCapsule->GetScaledCapsuleHalfHeight());
    if (SourceMesh)
    {
        VisualMesh=SourceMesh->GetSkeletalMeshAsset();
        RagdollPhysicsAsset=SourceMesh->GetPhysicsAsset();
        MeshOffset=SourceMesh->GetComponentTransform().GetRelativeTransform(FTransform(Player->GetActorQuat(),Center));
    }
    OnRep_Appearance();
    FSirrohCharacterProfile Profile;Profile.Name=TEXT("Wêdrowiec");Profile.BodyType=ESirrohBodyType::SimpleMan;
    Profile.Sex=FMath::RandBool()?ESirrohSex::Female:ESirrohSex::Male;
    Profile.LeftEye=ESirrohIrisColor(FMath::RandRange(0,1));Profile.RightEye=ESirrohIrisColor(FMath::RandRange(0,1));
    Profile.bHairEnabled=true;Profile.HairSeed=FMath::Rand();Profile.HairStyle=ESirrohHairStyle(FMath::RandRange(0,3));
    Profile.HairFrontLength=FMath::FRandRange(1.f,40.f);Profile.HairSideLength=FMath::FRandRange(1.f,40.f);
    Profile.HairBackLength=FMath::FRandRange(1.f,40.f);Profile.HairTopLength=FMath::FRandRange(1.f,40.f);
    Profile.HairDensity=FMath::FRandRange(.35f,1.5f);Profile.HairColor=FMath::RandRange(0,4);Profile.HairHighlightColor=FMath::RandRange(0,4);
    if(USirrohCharacterIdentityComponent::FindOrCreate(this)->ApplyProfile(Profile)){
        VisualMesh=Mesh->GetSkeletalMeshAsset();RagdollPhysicsAsset=Mesh->GetPhysicsAsset();ForceNetUpdate();
    }
}


void ACombatTrainingNPC::OnRep_Appearance()
{
    Capsule->SetCapsuleSize(CapsuleSize.X,CapsuleSize.Y);
    Mesh->SetRelativeTransform(MeshOffset);
    if (VisualMesh && Mesh->GetSkeletalMeshAsset()!=VisualMesh)
    {
        Mesh->SetSkeletalMesh(VisualMesh);
        Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
        if (auto* Pose=Combat->CombatStyle.LoadSynchronous())
        {
            // Only sample a compatible skeleton; the combat layer can retarget rotations itself.
            if (Pose->GetSkeleton()==VisualMesh->GetSkeleton())
            { Mesh->SetAnimation(Pose); Mesh->SetPosition(0,false); }
        }
    }
    if (RagdollPhysicsAsset && Mesh->GetPhysicsAsset()!=RagdollPhysicsAsset) Mesh->SetPhysicsAsset(RagdollPhysicsAsset);
    if(auto* Identity=FindComponentByClass<USirrohCharacterIdentityComponent>())Identity->RefreshAppearance();
}


void ACombatTrainingNPC::BeginPlay()
{
    Super::BeginPlay();
    OnRep_Appearance();
    Home=GetActorLocation();
    if (!HasAuthority()) return;
    // Override the player's DT timings after the combat component has loaded its profile.
    Combat->LightAttack.Duration=0.5f; Combat->LightAttack.Recovery=0.45f;
    Combat->HeavyAttack.Duration=0.7f; Combat->HeavyAttack.Recovery=0.65f;
    Combat->PierceAttack.Duration=0.45f; Combat->PierceAttack.Recovery=0.4f;
    Combat->OnKnockback.AddUObject(this,&ACombatTrainingNPC::AddTrainingKnockback);
    Combat->StartAICombat(GetWorld()->GetSubsystem<USwordWorldSubsystem>()->Generate(0,73191,true));
    NextDecision=GetWorld()->GetTimeSeconds()+1.0;
}


void ACombatTrainingNPC::Think(double Now)
{
    if (Now>=NextTargetSearch)
    {
        NextTargetSearch=Now+0.25;
        APawn* Closest=nullptr; double Best=FMath::Square(AggroRange);
        for (auto It=GetWorld()->GetPlayerControllerIterator(); It; ++It)
        {
            APawn* Pawn=It->Get()?It->Get()->GetPawn():nullptr;
            const auto* Other=Pawn?Pawn->FindComponentByClass<UDirectionalCombatComponent>():nullptr;
            const auto* Health=Pawn?Pawn->FindComponentByClass<UCombatVitalityComponent>():nullptr;
            if (!Other || (Health && Health->LifeState==ECombatLifeState::Dead) || (!Other->IsCombatActive() && (!Health || !Health->IsIncapacitated()))) continue;
            const double Distance=FVector::DistSquared(Pawn->GetActorLocation(),GetActorLocation());
            if (Distance<Best && FVector::DistSquared(Pawn->GetActorLocation(),Home)<FMath::Square(HomeRadius))
            { Best=Distance; Closest=Pawn; }
        }
        if (Target.Get()!=Closest) { Combat->CancelAttack(); Intent=EIntent::Guard; NextDecision=Now+0.5; }
        Target=Closest;
    }
    if (Combat->IsDisarmed())
    {
        if (RearmAt<=0) RearmAt=Now+3;
        if (Now>=RearmAt) { Combat->RearmAI(); RearmAt=0; Intent=EIntent::Guard; NextDecision=Now+1; }
        return;
    }
    if (Combat->IsMovementLocked()) { NextDecision=Now+0.1; Intent=EIntent::Guard; return; }
    APawn* Enemy=Target.Get();
    if (!Enemy) { Combat->SetAITarget(GetActorLocation()+GetActorForwardVector()*200); return; }
    const FVector Toward=Enemy->GetActorLocation()-GetActorLocation();
    const auto Phase=Combat->GetCombatPhase();
    if (Phase==EDirectionalCombatPhase::Combat)
        SetActorRotation(FRotator(0,Toward.Rotation().Yaw,0));
    FVector TargetPoint=Enemy->GetActorLocation()+FVector(0,0,20+25*FMath::Sin(Cycle*1.7));
    // Continuous interception hint. The body solver chooses a transverse blade orientation.
    if (Intent==EIntent::Guard)
    {
        const auto* Other=Enemy->FindComponentByClass<UDirectionalCombatComponent>();
        if (Other && Other->GetCombatPhase()==EDirectionalCombatPhase::Attacking)
            TargetPoint=GetActorLocation()-Other->GetAimDirection()*180;
    }
    if (const auto* H=Enemy->FindComponentByClass<UCombatVitalityComponent>(); H && H->IsIncapacitated()) TargetPoint=H->GetTargetPoint();
    Combat->SetAITarget(TargetPoint);
    if (Toward.Size2D()>AttackDistance+60 || Now<NextDecision) return;
    switch (Intent)
    {
        case EIntent::Guard: Intent=EIntent::Raise; NextDecision=Now+0.6; break;
        case EIntent::Raise:
            AttackType=static_cast<EDirectionalAttack>(Cycle%3);
            Combat->BeginAttack(AttackType);
            if (Combat->GetCombatPhase()==EDirectionalCombatPhase::Aiming)
            { ++Cycle; Intent=EIntent::Prepare; NextDecision=Now+(Cycle%2?0.3:0.7); }
            break;
        case EIntent::Prepare:
            Combat->ReleaseAttack(AttackType); Intent=EIntent::Wait; NextDecision=Now+0.65; break;
        case EIntent::Wait:
            Intent=EIntent::Raise; NextDecision=Now+0.2; break;
    }
}

void ACombatTrainingNPC::QueueCombatDisplacement(const FVector& Delta,float Seconds,bool bHit)
{
    if (!HasAuthority() || (!bHit && bCombatMoveHit && CombatMoveElapsed<CombatMoveDuration)) return;
    CombatRootPath.Reset();
    CombatDisplacement=FVector::VectorPlaneProject(Delta,FVector::UpVector).GetClampedToMaxSize(bHit?220:200);
    CombatMoveElapsed=0; CombatMoveDuration=FMath::Max(0.1f,Seconds); bCombatMoveHit=bHit;
}
void ACombatTrainingNPC::QueueCombatPath(const TArray<FVector>& Path,float Seconds)
{
    if (!HasAuthority() || Path.Num()<2 || !FMath::IsFinite(Seconds) || Seconds<=0) return;
    for (const auto& Point:Path) if (Point.ContainsNaN()) return;
    CombatRootPath=Path; CombatMoveElapsed=0; CombatMoveDuration=Seconds; bCombatMoveHit=true;
}
void ACombatTrainingNPC::CancelCombatApproach()
{
    if (!bCombatMoveHit) CombatMoveDuration=0;
}

void ACombatTrainingNPC::AddTrainingKnockback(const FVector& Velocity)
{
    Knockback+=FVector::VectorPlaneProject(Velocity,FVector::UpVector);
}


void ACombatTrainingNPC::MoveOnGround(float Delta,const FVector& Direction)
{
    FVector Move=Direction*WalkSpeed*Delta;
    if (CombatMoveElapsed<CombatMoveDuration)
    {
        const auto Position=[&](float Time)
        {
            const float T=FMath::Clamp(Time/CombatMoveDuration,0.f,1.f);
            if (CombatRootPath.Num()>1)
            {
                const float Key=T*(CombatRootPath.Num()-1); const int32 I=FMath::FloorToInt(Key);
                return FMath::Lerp(CombatRootPath[I],CombatRootPath[FMath::Min(I+1,CombatRootPath.Num()-1)],Key-I);
            }
            return CombatDisplacement*(T*T*(3-2*T));
        };
        const FVector Before=Position(CombatMoveElapsed);
        CombatMoveElapsed+=Delta;
        Move=Position(CombatMoveElapsed)-Before;
    }
    Move+=Knockback*Delta;
    Knockback*=FMath::Exp(-5*Delta);
    if (const auto* Ground=GroundProvider.Get())
    {
        const FVector Position=GetActorLocation()+Move;
        // Sample the nearby physical floor, not the surface height above this dungeon.
        const float Feet=GetActorLocation().Z-Capsule->GetScaledCapsuleHalfHeight();
        FHitResult FloorHit;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(TrainingNPCFloor),false,this);
        const bool Supported=GetWorld()->LineTraceSingleByObjectType(FloorHit,
            FVector(Position.X,Position.Y,Feet+45),FVector(Position.X,Position.Y,Feet-90),
            FCollisionObjectQueryParams(ECC_WorldStatic),Query) && FloorHit.ImpactNormal.Z>0.5f;
        const float Height=Supported?FloorHit.ImpactPoint.Z:-BIG_NUMBER;
        if (!Supported) Move=FVector::ZeroVector;
        if (Supported)
        {
            const float DesiredZ=Height+Capsule->GetScaledCapsuleHalfHeight()+3;
            // This placeholder walks gentle terrain only and does not climb cliffs.
            if (FMath::Abs(DesiredZ-GetActorLocation().Z)>45 && Knockback.IsNearlyZero(5)) Move=FVector::ZeroVector;
            else Move.Z=DesiredZ-GetActorLocation().Z;
        }
    }
    AddActorWorldOffset(Move,true);
}


void ACombatTrainingNPC::Tick(float Delta)
{
    Super::Tick(Delta);
    if (const auto* H=FindComponentByClass<UCombatVitalityComponent>(); H && H->IsIncapacitated()) return;
    if (HasAuthority())
    {
        const double Now=GetWorld()->GetTimeSeconds();
        Think(Now);
        FVector Move=FVector::ZeroVector;
        if (!Combat->IsMovementLocked() && !Combat->IsDisarmed())
        {
            if (const auto* Enemy=Target.Get())
            {
                const FVector Distance=FVector::VectorPlaneProject(Enemy->GetActorLocation()-GetActorLocation(),FVector::UpVector);
                const auto Phase=Combat->GetCombatPhase();
                if (Phase==EDirectionalCombatPhase::Combat || Phase==EDirectionalCombatPhase::Blocking)
                    Move=Distance.Size()>AttackDistance?Distance.GetSafeNormal():FVector::ZeroVector;
            }
            else
            {
                const float PatrolX=Home.X+FMath::Sin(Now*0.3)*120;
                Move.X=FMath::Clamp((PatrolX-GetActorLocation().X)/30.0,-1.0,1.0);
                if (FMath::Abs(Move.X)>0.1) SetActorRotation(FRotator(0,Move.X>=0?0:180,0));
            }
        }
        MoveOnGround(Delta,Move);
    }
    if (GetNetMode()!=NM_DedicatedServer)
    {
        FString State=TEXT("NPC treningowy");
        switch (Combat->GetCombatPhase())
        {
            case EDirectionalCombatPhase::Blocking: State+=TEXT(" | BLOK"); break;
            case EDirectionalCombatPhase::Aiming: State+=FString::Printf(TEXT(" | LADUJE %.1fs"),Combat->GetChargeSeconds()); break;
            case EDirectionalCombatPhase::Attacking: State+=TEXT(" | ATAK"); break;
            case EDirectionalCombatPhase::Staggered: State+=TEXT(" | STAGGER"); break;
            case EDirectionalCombatPhase::Disarmed: State+=TEXT(" | BEZ BRONI"); break;
            default: break;
        }
        State+=TEXT(" | COMBAT V3");
        Label->SetText(FText::FromString(State));
        Label->SetWorldLocation(GetActorLocation()+FVector(0,0,Capsule->GetScaledCapsuleHalfHeight()+24));
        if (const auto* Camera=UGameplayStatics::GetPlayerCameraManager(this,0))
            Label->SetWorldRotation(FRotationMatrix::MakeFromX(-Camera->GetCameraRotation().Vector()).Rotator());
    }
}


void ACombatTrainingNPC::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ACombatTrainingNPC,RagdollPhysicsAsset); DOREPLIFETIME(ACombatTrainingNPC,VisualMesh); DOREPLIFETIME(ACombatTrainingNPC,MeshOffset);
    DOREPLIFETIME(ACombatTrainingNPC,CapsuleSize);
}
