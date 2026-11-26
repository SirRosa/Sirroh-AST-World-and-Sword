#include "Character/SirrohASTNPCComponent.h"
#include "Placeholder/PlaceholderHandsComponent.h"
#include "Items/SirrohASTInventoryComponent.h"
#include "Sword/ProceduralSword.h"
#include "Character/SirrohASTCombatBridgeComponent.h"
#include "Net/UnrealNetwork.h"
#include "Character/SirrohCharacterIdentityComponent.h"
#include "Combat/DirectionalCombatComponent.h"
#include "Combat/CombatVitalityComponent.h"
#include "Sword/SwordWorldSubsystem.h"
#include "GameFramework/Pawn.h"
#include "AIController.h"
#include "GameFramework/PlayerController.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig.h"
#include "Perception/AISense.h"
#include "BrainComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "UObject/UnrealType.h"
#include "UObject/StructOnScope.h"
#include "Engine/World.h"
#include "Components/SplineComponent.h"
#include "PWG/PWGGameMode.h"
#include "Procedural_World_Generator/PWGv2Generator.h"

USirrohASTNPCComponent::USirrohASTNPCComponent()
{
    PrimaryComponentTick.bCanEverTick=true;
    PrimaryComponentTick.TickInterval=0.05f;
    SetIsReplicatedByDefault(true);
}
USirrohASTNPCComponent* USirrohASTNPCComponent::Find(UObject* Context)
{
    AActor* Actor=Cast<AActor>(Context);
    if (auto* Component=Cast<UActorComponent>(Context)) Actor=Component->GetOwner();
    return Actor?Actor->FindComponentByClass<USirrohASTNPCComponent>():nullptr;
}
bool USirrohASTNPCComponent::OwnsCombat(UObject* Context) { return Find(Context)!=nullptr; }
void USirrohASTNPCComponent::BeginPlay()
{
    Super::BeginPlay();
    if (!GetOwner()->HasAuthority()) return;
    // The AST patrol task expects a real path actor. Give this spawned NPC its own
    // short route before the StateTree's first update; retain AST's path follower.
    for (UActorComponent* Component:GetOwner()->GetComponents())
        if (Component && Component->GetClass()->GetName().StartsWith(TEXT("AST_AI_Component")))
            if (auto* Property=FindFProperty<FObjectPropertyBase>(Component->GetClass(),TEXT("Patrol Path"));Property && !Property->GetObjectPropertyValue_InContainer(Component))
            {
                FActorSpawnParameters Params;Params.Owner=GetOwner();
                PatrolPath=GetWorld()->SpawnActor<AActor>(Property->PropertyClass,GetOwner()->GetActorLocation(),FRotator::ZeroRotator,Params);
                if (PatrolPath)
                {
                    PatrolPath->SetActorHiddenInGame(true);PatrolPath->SetActorEnableCollision(false);
                    if (auto* Spline=PatrolPath->FindComponentByClass<USplineComponent>())
                    {
                        Spline->ClearSplinePoints(false);
                        const auto* Mode=GetWorld()->GetAuthGameMode<APWGGameMode>();
                        for (float Offset:{-150.f,150.f})
                        {
                            FVector Point=GetOwner()->GetActorLocation()+FVector(Offset,0,0);
                            if (Mode && Mode->GetWorldGenerator())
                            {
                                const float Height=Mode->GetWorldGenerator()->GetSurfaceHeightAt(Point.X,Point.Y);
                                if (Height>-BIG_NUMBER*0.5f) Point.Z=Height;
                            }
                            Spline->AddSplinePoint(Point,ESplineCoordinateSpace::World,false);
                        }
                        Spline->UpdateSpline();
                    }
                    Property->SetObjectPropertyValue_InContainer(Component,PatrolPath);
                }
            }
    // Identity retains the AST animation blueprint and creates the shared final-pose/hair path.
    if (SpawnSeed==0) SpawnSeed=1+(GetTypeHash(FGuid::NewGuid())%MAX_int32);
    const FSirrohCharacterProfile Profile=GenerateProfile(SpawnSeed);
    if (!USirrohCharacterIdentityComponent::FindOrCreate(GetOwner())->ApplyProfile(Profile))
        UE_LOG(LogTemp,Error,TEXT("AST NPC: failed to apply Sirroh appearance on %s"),*GetOwner()->GetName());
    Combat=GetOwner()->FindComponentByClass<UDirectionalCombatComponent>();
    if (!Combat)
    {
        Combat=NewObject<UDirectionalCombatComponent>(GetOwner(),TEXT("SirrohNPCCombat"));
        GetOwner()->AddInstanceComponent(Combat);Combat->RegisterComponent();
    }
    Vitality=UCombatVitalityComponent::FindOrAdd(GetOwner());
}
FSirrohCharacterProfile USirrohASTNPCComponent::GenerateProfile(int32 Seed)
{
    FRandomStream R(Seed);FSirrohCharacterProfile P;
    P.Sex=ESirrohSex(R.RandRange(0,1));P.BodyType=ESirrohBodyType(R.RandRange(0,3));
    const TCHAR* Names[]={TEXT("Aren"),TEXT("Mira"),TEXT("Raven"),TEXT("Neri"),TEXT("Soren"),TEXT("Tala")};
    P.Name=Names[R.RandRange(0,5)];P.LeftEye=ESirrohIrisColor(R.RandRange(0,1));P.RightEye=ESirrohIrisColor(R.RandRange(0,1));
    P.bHairEnabled=true;P.HairSeed=R.RandRange(1,MAX_int32);P.HairStyle=ESirrohHairStyle(R.RandRange(0,3));
    P.HairLength=R.FRandRange(4,28);P.HairFrontLength=R.FRandRange(2,16);P.HairSideLength=R.FRandRange(4,28);
    P.HairBackLength=R.FRandRange(4,30);P.HairTopLength=R.FRandRange(4,24);
    P.HairColor=R.RandRange(0,4);P.HairHighlightColor=R.RandRange(0,4);P.HairDensity=R.FRandRange(.65f,1.25f);
    return P;
}
void USirrohASTNPCComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{ Super::GetLifetimeReplicatedProps(OutLifetimeProps);DOREPLIFETIME(USirrohASTNPCComponent,SpawnSeed); }
bool USirrohASTNPCComponent::RouteAttack(UObject* Context)
{
    auto* Bridge=Find(Context);if(!Bridge)return false;
    if(!Bridge->GetOwner()->HasAuthority())return true;
    if(Bridge->bDistanceSleeping)return true;
    const auto State=USirrohASTCombatBridgeComponent::EquipmentState(Bridge->GetOwner());
    if(State==ESirrohEquipmentState::Dead || State==ESirrohEquipmentState::Recovering || !Bridge->Combat || Bridge->Combat->IsMovementLocked())
    { Bridge->ReleaseASTToken();return true; }
    if(State==ESirrohEquipmentState::Unarmed)
    {
        if(Bridge->Vitality && (!Bridge->Vitality->CanUseArm(false) || !Bridge->Vitality->CanUseArm(true)))
        { Bridge->ReleaseASTToken();return true; }
        // The strategy can remain active after disarming; reconsider a reachable weapon before each combo.
        if(Bridge->TryNearbyWeaponPickup())return true;
        return false; // Original AST selection, warp, combo and notify damage.
    }
    if(State==ESirrohEquipmentState::ASTWeapon)return false;
    Bridge->RequestAttack();return true;
}
void USirrohASTNPCComponent::RequestAttack()
{
    if(bAttackInFlight)return;
    if(auto* Melee=USirrohASTCombatBridgeComponent::Component(GetOwner(),TEXT("BPC_MeleeCombat")))
        if(auto* P=FindFProperty<FObjectPropertyBase>(Melee->GetClass(),TEXT("Token Actor")))
            HeldTokenTarget=Cast<AActor>(P->GetObjectPropertyValue_InContainer(Melee));
    if (!GetOwner()->HasAuthority() || !bInitialized || !Combat || !Vitality || Vitality->IsRecoveryBlocked()
        || Combat->GetCombatPhase()!=EDirectionalCombatPhase::Combat)
    { ReleaseASTToken();return; }
    Combat->BeginAttack(EDirectionalAttack::Light);
    if (Combat->GetCombatPhase()==EDirectionalCombatPhase::Aiming)
    { ReleaseAt=GetWorld()->GetTimeSeconds()+0.35;TokenReleaseAt=GetWorld()->GetTimeSeconds()+5.;bAttackInFlight=true; }
    else ReleaseASTToken();
}
bool USirrohASTNPCComponent::RouteDamage(UObject* Context,double Damage,FName BoneName,FVector Direction,AActor* Causer)
{ return USirrohASTCombatBridgeComponent::RouteDamage(Context,Damage,BoneName,Direction,Causer); }
void USirrohASTNPCComponent::SyncHealth()
{
    if (!Vitality) return;
    for (UActorComponent* Component:GetOwner()->GetComponents())
        if (Component && Component->GetClass()->GetName().StartsWith(TEXT("BPC_DamageSystem")))
        {
            if (auto* Health=FindFProperty<FNumericProperty>(Component->GetClass(),TEXT("CurrentHealth")))
                Health->SetFloatingPointPropertyValue(Health->ContainerPtrToValuePtr<void>(Component),Vitality->LifeState==ECombatLifeState::Dead?0:100.f*Vitality->Blood/FMath::Max(1.f,Vitality->MaxBlood));
            if (auto* Dead=FindFProperty<FBoolProperty>(Component->GetClass(),TEXT("IsDead")))
                Dead->SetPropertyValue_InContainer(Component,Vitality->LifeState==ECombatLifeState::Dead);
        }
}
void USirrohASTNPCComponent::RestorePerception()
{
    if (auto* Perception = SleepingPerception.Get())
        for (const auto& Sense : SuspendedSenses) Perception->SetSenseEnabled(Sense, true);
    SuspendedSenses.Reset();
    SleepingPerception.Reset();
}

bool USirrohASTNPCComponent::UpdateDistanceSleep(AAIController* AI)
{
    const double Now = GetWorld()->GetTimeSeconds();
    if (Now < NextDistanceCheck) return bDistanceSleeping;
    NextDistanceCheck = Now + 0.5;
    // Different sleep and wake radii prevent repeated path and perception restarts on the boundary.
    const double Radius = bDistanceSleeping ? 6000.0 : 8000.0;
    bool bShouldSleep = true;
    for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        const APawn* Player = It->IsValid() ? It->Get()->GetPawn() : nullptr;
        if (Player && FVector::DistSquared(Player->GetActorLocation(), GetOwner()->GetActorLocation()) <= Radius * Radius)
        {
            bShouldSleep = false;
            break;
        }
    }
    if (bShouldSleep != bDistanceSleeping)
    {
        bDistanceSleeping = bShouldSleep;
        SetComponentTickInterval(bShouldSleep ? 0.5f : 0.05f);
        if (bShouldSleep)
        {
            bHiddenByDistance = !GetOwner()->IsHidden();
            if (bHiddenByDistance) GetOwner()->SetActorHiddenInGame(true);
            CancelWeaponRecovery();
            Combat->CancelAttack();
            ReleaseAt = -1;
            ReleaseASTToken();
            AI->ClearFocus(EAIFocusPriority::Gameplay);
        }
        else
        {
            if (bHiddenByDistance) GetOwner()->SetActorHiddenInGame(false);
            bHiddenByDistance = false;
            RestorePerception();
        }
    }
    if (bDistanceSleeping)
    {
        if (auto* Brain = AI->GetBrainComponent(); Brain && !Brain->IsPaused() && (Brain->IsRunning() || Brain->IsComponentTickEnabled()))
        {
            Brain->PauseLogic(TEXT("Outside NPC activity radius"));
            Brain->SetComponentTickEnabled(false);
            bBrainPaused = true; // Normal vitality/pickup logic owns the eventual resume.
        }
        auto* Perception = AI->GetAIPerceptionComponent();
        if (Perception && Perception != SleepingPerception.Get())
        {
            RestorePerception();
            SleepingPerception = Perception;
            for (auto It = Perception->GetSensesConfigIterator(); It; ++It)
            {
                if (!*It) continue;
                const auto Sense = (*It)->GetSenseImplementation();
                if (Sense && Perception->IsSenseEnabled(Sense)) SuspendedSenses.AddUnique(Sense);
            }
            for (const auto& Sense : SuspendedSenses) Perception->SetSenseEnabled(Sense, false);
        }
    }
    return bDistanceSleeping;
}

void USirrohASTNPCComponent::TickComponent(float Delta,ELevelTick Tick,FActorComponentTickFunction* Function)
{
    Super::TickComponent(Delta,Tick,Function);
    if (!GetOwner()->HasAuthority() || !Combat || !Vitality) return;
    if (!bInitialized)
    {
        if(!GetOwner()->FindComponentByClass<UPlaceholderHandsComponent>())
        {auto* H=NewObject<UPlaceholderHandsComponent>(GetOwner());GetOwner()->AddInstanceComponent(H);H->RegisterComponent();}
        Combat->StartAICombat(GetWorld()->GetSubsystem<USwordWorldSubsystem>()->Generate(SpawnSeed%8,SpawnSeed,false));
        bInitialized=true;
    }
    auto* Pawn=Cast<APawn>(GetOwner());auto* AI=Pawn?Cast<AAIController>(Pawn->GetController()):nullptr;
    if (AI && UpdateDistanceSleep(AI)) return;
    SyncHealth();
    if (bAttackInFlight && ((ReleaseAt<0 && Combat->GetCombatPhase()==EDirectionalCombatPhase::Combat)
        || GetWorld()->GetTimeSeconds()>=TokenReleaseAt || Combat->IsDisarmed()))
    { Combat->CancelAttack();ReleaseAt=-1;ReleaseASTToken(); }
    if (!AI) return;
    auto* Hands=GetOwner()->FindComponentByClass<UPlaceholderHandsComponent>();
    if(Hands && !RecoveryWeapon.IsValid() && !Hands->IsASTPickupActive()
        && USirrohASTCombatBridgeComponent::EquipmentState(GetOwner())==ESirrohEquipmentState::Unarmed)
        TryNearbyWeaponPickup();
    UpdateWeaponRecovery();
    const bool Picking=Hands && Hands->IsASTPickupActive();
    if(RecoveryWeapon.IsValid())return;
    if(!Picking && Combat->IsDisarmed() && !Vitality->IsRecoveryBlocked())
        if(auto* I=GetOwner()->FindComponentByClass<USirrohASTInventoryComponent>())
            if(auto* Sword=Cast<AProceduralSword>(I->GetNativeItem(0)))Combat->StartAICombat(Sword->Definition);
    const bool Down=Vitality->IsRecoveryBlocked() || Combat->IsMovementLocked() || Picking;
    if (Down!=bBrainPaused)
    {
        if (auto* Brain=AI->GetBrainComponent())
        {
            if (Down) { AI->StopMovement();Brain->PauseLogic(TEXT("Sirroh vitality"));Brain->SetComponentTickEnabled(false); }
            else {Brain->SetComponentTickEnabled(true);Brain->ResumeLogic(TEXT("Sirroh vitality recovered"));}
        }
        bBrainPaused=Down;
    }
    if (Down) { Combat->CancelAttack();ReleaseAt=-1;ReleaseASTToken();return; }
    AActor* Target=nullptr;
    if (auto* Property=FindFProperty<FObjectPropertyBase>(AI->GetClass(),TEXT("Closest Enemy")))
        Target=Cast<AActor>(Property->GetObjectPropertyValue_InContainer(AI));
    const auto* TargetHealth=IsValid(Target)?Target->FindComponentByClass<UCombatVitalityComponent>():nullptr;
    if (!IsValid(Target) || (TargetHealth && TargetHealth->LifeState==ECombatLifeState::Dead))
    {
        Combat->CancelAttack();ReleaseAt=-1;ReleaseASTToken();
        // No combat target is normal during AST patrol/search. Do not abort its MoveTo every tick.
        if(IsValid(Target) && TargetHealth && TargetHealth->LifeState==ECombatLifeState::Dead)
        { AI->StopMovement();AI->ClearFocus(EAIFocusPriority::Gameplay); }
        return;
    }
    Combat->SetAITarget(Target->GetActorLocation()+FVector(0,0,20));
    if (ReleaseAt>=0 && GetWorld()->GetTimeSeconds()>=ReleaseAt)
    { Combat->ReleaseAttack(EDirectionalAttack::Light);ReleaseAt=-1; }
}

void USirrohASTNPCComponent::ReleaseASTToken()
{
    TokenReleaseAt=-1;bAttackInFlight=false;
    for (UActorComponent* Component:GetOwner()->GetComponents())
        if (Component && Component->GetClass()->GetName().StartsWith(TEXT("BPC_MeleeCombat")))
        {
            bool OwnsToken=false;
            if(auto* P=FindFProperty<FByteProperty>(Component->GetClass(),TEXT("RequestedAttackToken")))
                OwnsToken=P->GetPropertyValue_InContainer(Component)>0;
            AActor* TokenActor=HeldTokenTarget.Get();
            if (auto* Property=FindFProperty<FObjectPropertyBase>(Component->GetClass(),TEXT("Token Actor")))
                if(!TokenActor)TokenActor=Cast<AActor>(Property->GetObjectPropertyValue_InContainer(Component));
            if (UFunction* Clear=Component->FindFunction(TEXT("Set Requested Attack Token")))
            { FStructOnScope Args(Clear);Component->ProcessEvent(Clear,Args.GetStructMemory()); }
            if (OwnsToken && IsValid(TokenActor)) for (UActorComponent* TargetComponent:TokenActor->GetComponents())
                if (TargetComponent && TargetComponent->GetClass()->GetName().StartsWith(TEXT("BPC_MeleeCombat")))
                    if (UFunction* Clear=TargetComponent->FindFunction(TEXT("Set Attack Token")))
                    {
                        FStructOnScope Args(Clear);
                        if(auto* P=FindFProperty<FByteProperty>(Clear,TEXT("AttackToken")))P->SetPropertyValue_InContainer(Args.GetStructMemory(),1);
                        TargetComponent->ProcessEvent(Clear,Args.GetStructMemory());
                    }
        }
    HeldTokenTarget.Reset();
}

void USirrohASTNPCComponent::EndPlay(EEndPlayReason::Type Reason)
{
    if (bHiddenByDistance) GetOwner()->SetActorHiddenInGame(false);
    RestorePerception();
    CancelWeaponRecovery();
    ReleaseASTToken();
    if (GetOwner()->HasAuthority() && IsValid(PatrolPath)) PatrolPath->Destroy();
    Super::EndPlay(Reason);
}

bool USirrohASTNPCComponent::TryNearbyWeaponPickup()
{
    if (bDistanceSleeping) return false;
    if(!GetOwner()->HasAuthority() || !Combat || !Vitality || Vitality->IsRecoveryBlocked() || Combat->IsMovementLocked())return false;
    auto* Hands=GetOwner()->FindComponentByClass<UPlaceholderHandsComponent>();
    auto* Pawn=Cast<APawn>(GetOwner());
    auto* AI=Pawn?Cast<AAIController>(Pawn->GetController()):nullptr;
    if(!Hands || !AI)return false;
    if(Hands->IsASTPickupActive() || RecoveryWeapon.IsValid())return true;
    if(Hands->GetItem(0) || !Vitality->CanUseArm(false) || GetWorld()->GetTimeSeconds()<NextWeaponSearch)return false;
    NextWeaponSearch=GetWorld()->GetTimeSeconds()+1.0;
    AProceduralSword* Best=nullptr;double Distance=DBL_MAX;
    for(const auto& Weak:GetWorld()->GetSubsystem<USwordWorldSubsystem>()->Items)
        if(auto* Sword=Cast<AProceduralSword>(Weak.Get());Sword && Sword->CanPickUpBy(GetOwner()))
        {
            const FVector D=Sword->GetActorLocation()-GetOwner()->GetActorLocation();
            if(FMath::Abs(D.Z)>220.f || D.SizeSquared()>FMath::Square(900.f))continue;
            if(D.SizeSquared()<Distance){Best=Sword;Distance=D.SizeSquared();}
        }
    if(!Best || !Best->ReservePickup(GetOwner(),8.f))return false;
    ReleaseASTToken();ReleaseAt=-1;Combat->CancelAttack();
    if(Hands->RequestPickup(Best))return true;
    // Suspend the existing AST decision tree only for this bounded recovery action.
    // Its controller/path follower still owns navigation, including obstacle avoidance.
    if(auto* Brain=AI->GetBrainComponent()){Brain->PauseLogic(TEXT("Recover reserved weapon"));Brain->SetComponentTickEnabled(false);}
    bBrainPaused=true;AI->StopMovement();
    RecoveryWeapon=Best;RecoveryStarted=GetWorld()->GetTimeSeconds();RecoveryDeadline=RecoveryStarted+6.0;
    const auto Result=AI->MoveToActor(Best,45.f,true,true,false,nullptr,false);
    if(Result==EPathFollowingRequestResult::Failed){CancelWeaponRecovery();return false;}
    return true;
}
void USirrohASTNPCComponent::CancelWeaponRecovery(bool bReleaseReservation)
{
    if(bReleaseReservation && RecoveryWeapon.IsValid())RecoveryWeapon->ReleasePickup(GetOwner());
    if(auto* Pawn=Cast<APawn>(GetOwner()))if(auto* AI=Cast<AAIController>(Pawn->GetController()))AI->StopMovement();
    RecoveryWeapon.Reset();RecoveryDeadline=0;
    NextWeaponSearch=GetWorld()->GetTimeSeconds()+3.0;
    // Tick resumes AST when both vitality and the shared pickup action allow it.
}
void USirrohASTNPCComponent::UpdateWeaponRecovery()
{
    if(RecoveryDeadline<=0)return;
    auto* Hands=GetOwner()->FindComponentByClass<UPlaceholderHandsComponent>();
    auto* Pawn=Cast<APawn>(GetOwner());auto* AI=Pawn?Cast<AAIController>(Pawn->GetController()):nullptr;
    auto* Weapon=RecoveryWeapon.Get();
    const double Now=GetWorld()->GetTimeSeconds();
    if(!Weapon || !AI || !Hands || Vitality->IsRecoveryBlocked() || Combat->IsMovementLocked()
        || !Vitality->CanUseArm(false) || Hands->GetItem(0) || !Weapon->CanPickUpBy(GetOwner()) || Now>=RecoveryDeadline)
    {CancelWeaponRecovery();return;}
    if(Hands->RequestPickup(Weapon)){CancelWeaponRecovery(false);return;}
    if(Now-RecoveryStarted>.5 && AI->GetMoveStatus()==EPathFollowingStatus::Idle)CancelWeaponRecovery();
}
