#include "Character/SirrohASTCombatBridgeComponent.h"
#include "POI/POICorridorTools.h"
#include "POI/POIWorkshopGameMode.h"
#include "Character/SirrohASTNPCComponent.h"
#include "Character/SirrohPoseBridgeComponent.h"
#include "Combat/DirectionalCombatComponent.h"
#include "Combat/CombatVitalityComponent.h"
#include "Placeholder/PlaceholderHandsComponent.h"
#include "Sword/ProceduralSword.h"
#include "Items/SirrohASTInventoryComponent.h"
#include "PWG/PWGGameMode.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "UObject/UnrealType.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "HAL/IConsoleManager.h"
static TAutoConsoleVariable<int32> UnarmedContactLog(TEXT("poi.UnarmedContactLog"),0,TEXT("Log AST fist activation and prop contacts."));
#include "AIController.h"
#include "GameplayTagContainer.h"
#include "UObject/StructOnScope.h"
#include "Components/SkeletalMeshComponent.h"

static FVector PhysicalHandPosition(AActor* Actor,USkeletalMeshComponent* Source,FName Bone)
{
 if(auto* Pose=Actor->FindComponentByClass<USirrohPoseBridgeComponent>())
 {FTransform Final;if(Pose->TryGetFinalBoneTransform(Bone,Final))return Final.GetLocation();}
 return Source->GetSocketLocation(Bone);
}

AActor* USirrohASTCombatBridgeComponent::ActorFor(UObject* Context)
{
    if (auto* C=Cast<UActorComponent>(Context)) return C->GetOwner();
    return Cast<AActor>(Context);
}
UActorComponent* USirrohASTCombatBridgeComponent::Component(AActor* Actor,const TCHAR* Prefix)
{
    if (Actor) for (UActorComponent* C:Actor->GetComponents()) if(C && C->GetClass()->GetName().StartsWith(Prefix))return C;
    return nullptr;
}
bool USirrohASTCombatBridgeComponent::InPWG(UObject* Context)
{ return Context && Context->GetWorld() && (Context->GetWorld()->GetAuthGameMode<APWGGameMode>() || Context->GetWorld()->GetAuthGameMode<APOIWorkshopGameMode>()); }
ESirrohEquipmentState USirrohASTCombatBridgeComponent::EquipmentState(AActor* Actor)
{
    if (!IsValid(Actor)) return ESirrohEquipmentState::Dead;
    if (auto* H=Actor->FindComponentByClass<UCombatVitalityComponent>())
    {
        if(H->LifeState==ECombatLifeState::Dead)return ESirrohEquipmentState::Dead;
        if(H->IsRecoveryBlocked())return ESirrohEquipmentState::Recovering;
    }
    auto* C=Actor->FindComponentByClass<UDirectionalCombatComponent>();
    if (Actor->FindComponentByClass<USirrohASTNPCComponent>() && C && C->IsCombatActive() && !C->IsDisarmed())return ESirrohEquipmentState::NativeWeapon;
    if (auto* Hands=Actor->FindComponentByClass<UPlaceholderHandsComponent>())
        if (Cast<AProceduralSword>(Hands->GetItem(0)) || Cast<AProceduralSword>(Hands->GetItem(1)))return ESirrohEquipmentState::NativeWeapon;
    if (auto* Inventory=Component(Actor,TEXT("BPC_Inventory")))
        if(auto* P=FindFProperty<FStructProperty>(Inventory->GetClass(),TEXT("EquippedWeapon")))
            for(TFieldIterator<FObjectPropertyBase> F(P->Struct);F;++F)
                if(F->GetName().StartsWith(TEXT("Weapon_")) && IsValid(F->GetObjectPropertyValue_InContainer(P->ContainerPtrToValuePtr<void>(Inventory))))return ESirrohEquipmentState::ASTWeapon;
    return ESirrohEquipmentState::Unarmed;
}
USirrohASTCombatBridgeComponent* USirrohASTCombatBridgeComponent::FindOrAdd(AActor* Actor)
{
    if(!Actor || !Actor->HasAuthority())return nullptr;
    if(auto* C=Actor->FindComponentByClass<USirrohASTCombatBridgeComponent>())return C;
    auto* C=NewObject<USirrohASTCombatBridgeComponent>(Actor);Actor->AddInstanceComponent(C);C->RegisterComponent();return C;
}
bool USirrohASTCombatBridgeComponent::BeginASTMelee(UObject* Context)
{
    auto* Actor=ActorFor(Context);
    if(!InPWG(Context) || !Actor || !Actor->HasAuthority())return false;
    const auto State=EquipmentState(Actor);
    if(State==ESirrohEquipmentState::Dead || State==ESirrohEquipmentState::Recovering)return true;
    if(State==ESirrohEquipmentState::Unarmed)
        if(auto* Health=Actor->FindComponentByClass<UCombatVitalityComponent>())
            if(!Health->CanUseArm(false) || !Health->CanUseArm(true))return true;
    return false;
}
bool USirrohASTCombatBridgeComponent::ObserveASTMelee(UObject* Context)
{
    auto* Actor=ActorFor(Context);
    if(!InPWG(Context) || !Actor || !Actor->HasAuthority() || EquipmentState(Actor)!=ESirrohEquipmentState::Unarmed)return false;
    auto* Result=FindFProperty<FStructProperty>(Context->GetClass(),TEXT("MeleeMontageResult"));
    UAnimMontage* Montage=nullptr;
    if(Result) for(TFieldIterator<FObjectPropertyBase> P(Result->Struct);P;++P)
        if(P->GetName().StartsWith(TEXT("AttackerAnimMontage_")))
            Montage=Cast<UAnimMontage>(P->GetObjectPropertyValue_InContainer(Result->ContainerPtrToValuePtr<void>(Context)));
    if(!Montage)return false;
    auto* C=FindOrAdd(Actor);
    // PlayMontageOnMoverActor queues movement work. Its exec output does not
    // guarantee that an AnimMontageInstance exists in this frame.
    C->PendingMeleeContext=Context;C->PendingMeleeMontage=Montage;
    C->PendingMeleeUntil=Actor->GetWorld()->GetTimeSeconds()+.75;
    C->SetComponentTickEnabled(true);
    return false;
}
bool USirrohASTCombatBridgeComponent::TryStartMelee(UObject* Context,UAnimMontage* Montage)
{
    if(!IsValid(Context) || !Montage)return false;
    for(auto* Part:GetOwner()->GetComponents())if(auto* Mesh=Cast<USkeletalMeshComponent>(Part))
        if(auto* Anim=Mesh->GetAnimInstance())if(auto* Instance=Anim->GetActiveInstanceForMontage(Montage))
        {
            if(AttackAnimInstance==Anim && MontageInstanceId==Instance->GetInstanceID())continue;
            auto* Phase=FindFProperty<FIntProperty>(Context->GetClass(),TEXT("ComboCount"));if(!Phase)return false;
            Combo=FMath::Clamp(Phase->GetPropertyValue_InContainer(Context),0,2)+1;
            AttackAnimInstance=Anim;MontageInstanceId=Instance->GetInstanceID();
            StartedAt=GetWorld()->GetTimeSeconds();ExpiresAt=StartedAt+Montage->GetPlayLength()+.5;
            HitTargets.Reset();PropHitTargets.Reset();
            if(PunchMesh.IsValid())RemoveTickPrerequisiteComponent(PunchMesh.Get());
            PunchMesh=Mesh;AddTickPrerequisiteComponent(Mesh);
            if(auto* Pose=GetOwner()->FindComponentByClass<USirrohPoseBridgeComponent>())AddTickPrerequisiteComponent(Pose);
            SetComponentTickInterval(0);
            PreviousHands[0]=PhysicalHandPosition(GetOwner(),Mesh,TEXT("hand_l"));PreviousHands[1]=PhysicalHandPosition(GetOwner(),Mesh,TEXT("hand_r"));
            if(UnarmedContactLog.GetValueOnGameThread())UE_LOG(LogTemp,Log,TEXT("POI fists started montage=%s mesh=%s"),*Montage->GetName(),*Mesh->GetName());
            return true;
        }
    return false;
}
bool USirrohASTCombatBridgeComponent::RouteDamage(UObject* Context,double Damage,FName Bone,FVector Direction,AActor* Causer)
{
    auto* Victim=ActorFor(Context);
    if(!InPWG(Context) || !Victim || !Cast<APawn>(Victim))return false;
    if(!Victim->HasAuthority() || !FMath::IsFinite(Damage) || Damage<=0)return true;
    auto* Health=UCombatVitalityComponent::FindOrAdd(Victim);
    if(!Health || Health->LifeState==ECombatLifeState::Dead)return true;
    auto* Attack=IsValid(Causer)?Causer->FindComponentByClass<USirrohASTCombatBridgeComponent>():nullptr;
    const auto CauserState=EquipmentState(Causer);
    if(IsValid(Causer) && (CauserState==ESirrohEquipmentState::Dead || CauserState==ESirrohEquipmentState::Recovering))return true;
    const bool Unarmed=Cast<APawn>(Causer) && CauserState==ESirrohEquipmentState::Unarmed;
    // Late/invalid unarmed notifies must never fall through to lethal generic damage.
    if(Unarmed && (!Attack || Attack->Combo<=0 || Victim->GetWorld()->GetTimeSeconds()>Attack->ExpiresAt))return true;
    if(Unarmed && Attack->HitTargets.Contains(Victim))return true;
    if(Unarmed) { Attack->HitTargets.Add(Victim);Damage=5*Attack->Combo; }
    FCombatMotionBodyHit Hit;Hit.Target=Victim;Hit.Attacker=Causer;Hit.TargetLifeId=Health->LifeId;
    Hit.BodyRegion=Bone.IsNone()?FName(TEXT("spine_03")):Bone;
    Hit.Action=Unarmed?TEXT("Unarmed"):TEXT("Kick");
    Hit.DamageKind=Unarmed?ECombatDamageKind::Unarmed:ECombatDamageKind::Blunt;
    Hit.AuthoredDamage=Damage;Hit.RelativeSpeed=Damage*200.;Hit.DamageScale=1;
    if(Direction.IsNearlyZero() && IsValid(Causer))Direction=Victim->GetActorLocation()-Causer->GetActorLocation();
    Hit.ContactVelocity=Direction.GetSafeNormal()*Hit.RelativeSpeed;Hit.Point=Health->GetTargetPoint();
    if(auto* Combat=Victim->FindComponentByClass<UDirectionalCombatComponent>())Combat->ReceiveBodyContact(Hit,1);
    else Health->ApplyContact(Hit,1);
    if(auto* System=Component(Victim,TEXT("BPC_DamageSystem")))
    {
        if(auto* P=FindFProperty<FNumericProperty>(System->GetClass(),TEXT("CurrentHealth")))P->SetFloatingPointPropertyValue(P->ContainerPtrToValuePtr<void>(System),Health->LifeState==ECombatLifeState::Dead?0:100.*Health->Blood/FMath::Max(1.f,Health->MaxBlood));
        if(auto* P=FindFProperty<FBoolProperty>(System->GetClass(),TEXT("IsDead")))P->SetPropertyValue_InContainer(System,Health->LifeState==ECombatLifeState::Dead);
    }
    return true;
}
bool USirrohASTCombatBridgeComponent::SuppressDetectionHUD(UObject* Context)
{
    if(!InPWG(Context))return false;
    if(auto* P=FindFProperty<FArrayProperty>(Context->GetClass(),TEXT("DetectionHUDS")))
        if(auto* Object=CastField<FObjectPropertyBase>(P->Inner))
        {
            FScriptArrayHelper Widgets(P,P->ContainerPtrToValuePtr<void>(Context));
            if(auto* Actors=FindFProperty<FArrayProperty>(Context->GetClass(),TEXT("Whoidentifieds")))
            {
                FScriptArrayHelper Identified(Actors,Actors->ContainerPtrToValuePtr<void>(Context));
                if(Widgets.Num()<Identified.Num())Widgets.AddValues(Identified.Num()-Widgets.Num());
            }
            for(int32 I=0;I<Widgets.Num();++I)
                if(auto* Widget=Object->GetObjectPropertyValue(Widgets.GetRawPtr(I));IsValid(Widget))
                    if(auto* SetOpacity=Widget->FindFunction(TEXT("SetRenderOpacity")))
                    {
                        FStructOnScope Args(SetOpacity);
                        if(auto* Opacity=FindFProperty<FFloatProperty>(SetOpacity,TEXT("InOpacity")))
                        { Opacity->SetPropertyValue_InContainer(Args.GetStructMemory(),0.f);Widget->ProcessEvent(SetOpacity,Args.GetStructMemory()); }
                    }
        }
    // Keep AST's detected-actor bookkeeping. Only the widget's rendering is suppressed.
    return false;
}
bool USirrohASTCombatBridgeComponent::PrepareDetectionArrays(UObject* Context)
{
    if(!InPWG(Context))return false;
    auto* Targets=FindFProperty<FArrayProperty>(Context->GetClass(),TEXT("Target Actors"));
    if(!Targets)return false;
    FScriptArrayHelper T(Targets,Targets->ContainerPtrToValuePtr<void>(Context));
    // AST indexes parallel per-target arrays before Array_Set(size-to-fit). Initialize missing slots first.
    for(const TCHAR* Name:{TEXT("DetectionMeter"),TEXT("IncreaseDetectMeterTimer"),TEXT("DecreaseDetectMeterTimer"),TEXT("Target Actor Sight Sense Quantities")})
        if(auto* P=FindFProperty<FArrayProperty>(Context->GetClass(),Name))
        { FScriptArrayHelper A(P,P->ContainerPtrToValuePtr<void>(Context));if(A.Num()<T.Num())A.AddValues(T.Num()-A.Num()); }
    return false;
}
bool USirrohASTCombatBridgeComponent::ValidateDetectionTarget(UObject* Context,AActor* Target,int32 TargetIndex)
{
    if(!InPWG(Context))return false;
    PrepareDetectionArrays(Context);
    auto* P=FindFProperty<FArrayProperty>(Context->GetClass(),TEXT("Target Actors"));if(!P)return true;
    FScriptArrayHelper A(P,P->ContainerPtrToValuePtr<void>(Context));
    auto* Object=CastField<FObjectPropertyBase>(P->Inner);
    return !IsValid(Target) || !A.IsValidIndex(TargetIndex) || !Object || Object->GetObjectPropertyValue(A.GetRawPtr(TargetIndex))!=Target;
}

bool USirrohASTCombatBridgeComponent::RouteMeleeStrategy(UObject* Context)
{
    if(!Context)return false;
    auto* PawnProperty=FindFProperty<FObjectPropertyBase>(Context->GetClass(),TEXT("Pawn"));
    auto* Pawn=PawnProperty?Cast<APawn>(PawnProperty->GetObjectPropertyValue_InContainer(Context)):nullptr;
    if(!Pawn || !Pawn->HasAuthority() || !Pawn->FindComponentByClass<USirrohASTNPCComponent>())return false;
    const auto State=EquipmentState(Pawn);
    if(State!=ESirrohEquipmentState::NativeWeapon && State!=ESirrohEquipmentState::Unarmed)return false;
    if(State==ESirrohEquipmentState::Unarmed)
        if(auto* NPC=Pawn->FindComponentByClass<USirrohASTNPCComponent>();NPC && NPC->TryNearbyWeaponPickup())return true;
    auto* AI=Cast<AAIController>(Pawn->GetController());
    auto* TargetProperty=AI?FindFProperty<FObjectPropertyBase>(AI->GetClass(),TEXT("Closest Enemy")):nullptr;
    auto* Target=TargetProperty?Cast<AActor>(TargetProperty->GetObjectPropertyValue_InContainer(AI)):nullptr;
    if(!IsValid(Target) || EquipmentState(Target)==ESirrohEquipmentState::Dead)return true;
    auto* Flag=FindFProperty<FBoolProperty>(Context->GetClass(),TEXT("IsMeleeDistance"));
    auto* Send=Context->FindFunction(TEXT("SendEvent"));
    auto* Event=Send?FindFProperty<FStructProperty>(Send,TEXT("Event")):nullptr;
    if(!Flag || !Event)return false;
    // This flag selects AST's melee strategy; its own approach tasks still enforce real attack range.
    Flag->SetPropertyValue_InContainer(Context,true);
    if(auto* Manager=Component(Pawn,TEXT("BPC_StateManager")))
        if(auto* Tag=FindFProperty<FStructProperty>(Manager->GetClass(),TEXT("AIStateTreeTag")))
            if(Tag->Struct==FGameplayTag::StaticStruct() && Tag->ContainerPtrToValuePtr<FGameplayTag>(Manager)->MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("AST.ST.Strategic.Combat.MeleeCombat"))))return true;
    FStructOnScope Args(Send);
    if(!Event->ImportText_Direct(TEXT("(Tag=(TagName=\"AST.StateTree.MoveToMeleeCombat\"))"),Event->ContainerPtrToValuePtr<void>(Args.GetStructMemory()),Context,PPF_None))return false;
    Context->ProcessEvent(Send,Args.GetStructMemory());
    return true;
}

USirrohASTCombatBridgeComponent::USirrohASTCombatBridgeComponent()
{
 PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.bStartWithTickEnabled=true;PrimaryComponentTick.TickGroup=TG_PostPhysics;PrimaryComponentTick.TickInterval=.05f;
}
void USirrohASTCombatBridgeComponent::TickComponent(float Dt,ELevelTick Type,FActorComponentTickFunction* Tick)
{
 Super::TickComponent(Dt,Type,Tick);
 // Also observe the montage that is actually playing. A valid AST attack must
 // not depend on one particular Blueprint PlayMontage execution branch.
 auto* PreviousAnim=PunchMesh.IsValid()?PunchMesh->GetAnimInstance():nullptr;
 auto* PreviousInstance=PreviousAnim?PreviousAnim->GetMontageInstanceForID(MontageInstanceId):nullptr;
 if((!PreviousInstance || !PreviousInstance->IsActive()) && GetOwner()->HasAuthority() && EquipmentState(GetOwner())==ESirrohEquipmentState::Unarmed)
 {
  if(auto* Context=Component(GetOwner(),TEXT("BPC_MeleeCombat")))
   if(auto* Result=FindFProperty<FStructProperty>(Context->GetClass(),TEXT("MeleeMontageResult")))
    for(TFieldIterator<FObjectPropertyBase> P(Result->Struct);P;++P)
     if(P->GetName().StartsWith(TEXT("AttackerAnimMontage_")))
      if(auto* Montage=Cast<UAnimMontage>(P->GetObjectPropertyValue_InContainer(Result->ContainerPtrToValuePtr<void>(Context))))
       if(TryStartMelee(Context,Montage)){PendingMeleeContext.Reset();PendingMeleeMontage.Reset();}
 }

 if(PendingMeleeContext.IsValid())
 {
  if(TryStartMelee(PendingMeleeContext.Get(),PendingMeleeMontage.Get()) || GetWorld()->GetTimeSeconds()>PendingMeleeUntil)
  {if(GetWorld()->GetTimeSeconds()>PendingMeleeUntil && UnarmedContactLog.GetValueOnGameThread())UE_LOG(LogTemp,Warning,TEXT("POI fists: timed out waiting for AST montage"));PendingMeleeContext.Reset();PendingMeleeMontage.Reset();}
  else return;
 }
 auto* Mesh=PunchMesh.Get();auto* Anim=Mesh?Mesh->GetAnimInstance():nullptr;
 auto* Instance=Anim?Anim->GetMontageInstanceForID(MontageInstanceId):nullptr;
 if(!GetOwner()->HasAuthority() || !Instance || !Instance->IsActive() || GetWorld()->GetTimeSeconds()>ExpiresAt || EquipmentState(GetOwner())!=ESirrohEquipmentState::Unarmed)
 {SetComponentTickInterval(.05f);return;}
 for(int Hand=0;Hand<2;++Hand)
 {
  const FName Bone=Hand==0?TEXT("hand_l"):TEXT("hand_r");if(!Mesh->DoesSocketExist(Bone))continue;
  const FVector Now=PhysicalHandPosition(GetOwner(),Mesh,Bone),Previous=PreviousHands[Hand];PreviousHands[Hand]=Now;
  TracePropFist(GetOwner(),Previous,Now,Dt,PropHitTargets);
 }
}
bool USirrohASTCombatBridgeComponent::TracePropFist(AActor* Attacker,FVector Start,FVector End,float Dt,TSet<TWeakObjectPtr<AActor>>& Targets)
{
 if(!IsValid(Attacker) || !Attacker->HasAuthority() || !Attacker->GetWorld() || Dt<=0)return false;
 const FVector Delta=End-Start,Velocity=Delta/Dt;
 if(Delta.Size()>80 || Velocity.Size()<120)return false; // Reject warps and stationary overlaps.
 FCollisionQueryParams Q(SCENE_QUERY_STAT(POIFists),false,Attacker);
 TArray<AActor*> Attached;Attacker->GetAttachedActors(Attached,true,true);Q.AddIgnoredActors(Attached);
 FCollisionObjectQueryParams Types(ECC_PhysicsBody);Types.AddObjectTypesToQuery(ECC_WorldStatic);
 TArray<FHitResult> Hits;Attacker->GetWorld()->SweepMultiByObjectType(Hits,Start,End,FQuat::Identity,Types,FCollisionShape::MakeSphere(8),Q);
 bool Applied=false;
 for(const auto& Hit:Hits)
 {
  auto* Target=Hit.GetActor();auto* Body=Hit.GetComponent();
  if(Target && Body && !Targets.Contains(Target) && Body->IsSimulatingPhysics())
  {
   const FVector Relative=Velocity-Body->GetPhysicsLinearVelocityAtPoint(Hit.ImpactPoint);
   // Approach the contacted surface, independently of actor yaw/visual facing.
   const FVector Normal=Hit.bStartPenetrating?(Start-Body->Bounds.Origin).GetSafeNormal():FVector(Hit.ImpactNormal);
   if(FVector::DotProduct(Relative,Normal)<-1 && UPOICorridorTools::ApplyPropStrike(Body,Velocity.GetClampedToMaxSize(1200),3.f))
   {Targets.Add(Target);Applied=true;if(UnarmedContactLog.GetValueOnGameThread())UE_LOG(LogTemp,Log,TEXT("POI fist contact target=%s speed=%.1f"),*Target->GetName(),Velocity.Size());}
  }
  if(Hit.bBlockingHit)break;
 }
 return Applied;
}

