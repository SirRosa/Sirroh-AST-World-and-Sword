#include "PWG/PWGPhysicsContactComponent.h"
#include "PWG/PWGSidescrollerInputComponent.h"
#include "POI/POIPushTraversalComponent.h"
#include "Combat/CombatVitalityComponent.h"
#include "Components/PrimitiveComponent.h"
#include "MoverComponent.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "DefaultMovementSet/InstantMovementEffects/BasicInstantMovementEffects.h"
#include "HAL/IConsoleManager.h"
static TAutoConsoleVariable<int32> LogContacts(TEXT("poi.PhysicsContactLog"),0,TEXT("Log bounded player/pushable contact impulses."));
UPWGPhysicsContactComponent::UPWGPhysicsContactComponent(){PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.TickGroup=TG_PrePhysics;}
double UPWGPhysicsContactComponent::InelasticImpulse(double Speed,double MP,double MB)
{return Speed>0 && MP>0 && MB>0 ? Speed/(1./MP+1./MB):0.;}
double UPWGPhysicsContactComponent::NudgeImpulse(double Speed,double MP,double MB,double Dt)
{
 // 100 N sustained effort, at most 90 cm/s for props up to 25 kg from a fresh impact. Repeated hits
 // share a time budget instead of each receiving a new sprint-sized impulse.
 return FMath::Min3(InelasticImpulse(Speed,MP,MB)*.25,MB*90.*FMath::Min(1.,25./FMath::Max(1.,MB)),Dt>=.08?6000.:10000.*FMath::Max(Dt,0.));
}
void UPWGPhysicsContactComponent::BeginPlay()
{
 Super::BeginPlay();AddTickPrerequisiteActor(GetOwner());Capsule=Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent());
 if(auto* C=Capsule.Get()){OriginalCollision=C->GetCollisionEnabled();C->OnComponentHit.AddDynamic(this,&UPWGPhysicsContactComponent::OnHit);}
 TickComponent(0,LEVELTICK_All,nullptr);
}
void UPWGPhysicsContactComponent::TickComponent(float Dt,ELevelTick Type,FActorComponentTickFunction* Tick)
{
 Super::TickComponent(Dt,Type,Tick);
 const auto* Health=GetOwner()->FindComponentByClass<UCombatVitalityComponent>();
 const bool Ragdoll=Health && Health->UsesRagdollPose();
 if(Ragdoll)
 {
  RestoreCollisionPolicy();return;
 }
 // Animated mesh bodies also act as kinematic hammers, even if the capsule
 // itself is query-only. Keep query collisions/overlaps but one impulse owner.
 TArray<UPrimitiveComponent*> Parts;GetOwner()->GetComponents(Parts);
 for(auto* C:Parts)if(C && C->GetCollisionEnabled()==ECollisionEnabled::QueryAndPhysics)
 {
  if(!C->IsAnySimulatingPhysics())
  {KinematicColliders.FindOrAdd(C,C->GetCollisionEnabled());C->SetCollisionEnabled(ECollisionEnabled::QueryOnly);}
  else
  {
   // PhysicsControl may simulate animated bones outside ragdoll. Preserve that
   // simulation, but let the capsule/contact model own interactions with props.
   AnimatedResponses.FindOrAdd(C,TPair<ECollisionResponse,ECollisionResponse>(C->GetCollisionResponseToChannel(ECC_PhysicsBody),C->GetCollisionResponseToChannel(ECC_WorldDynamic)));
   C->SetCollisionResponseToChannel(ECC_PhysicsBody,ECR_Ignore);
   C->SetCollisionResponseToChannel(ECC_WorldDynamic,ECR_Ignore);
  }
 }
 if(LastContactFrame.Num()>64)LastContactFrame.Reset();
 if(LastContactTime.Num()>64)for(auto It=LastContactTime.CreateIterator();It;++It)
  if(!It.Key().IsValid() || GetWorld()->GetTimeSeconds()-It.Value()>1.)It.RemoveCurrent();
}
void UPWGPhysicsContactComponent::OnHit(UPrimitiveComponent* HitComponent,AActor* OtherActor,UPrimitiveComponent* Body,FVector NormalImpulse,const FHitResult& Hit)
{
 if(!Body || !OtherActor || OtherActor->IsA<APawn>() || !Body->IsSimulatingPhysics() || Hit.bStartPenetrating)return;
 if(auto* Health=GetOwner()->FindComponentByClass<UCombatVitalityComponent>();Health && Health->UsesRagdollPose())return;
 if(auto* Input=GetOwner()->FindComponentByClass<UPWGSidescrollerInputComponent>();Input && (Input->IsTraversalActive() || Input->IsASTInteractionActive()))return;
 if(LastContactFrame.FindRef(Body)==GFrameCounter)return;
 const FVector N=Hit.ImpactNormal.GetSafeNormal();
 if(FMath::Abs(N.Z)>.5 || FMath::Abs(N.Y)>.5)return; // Standing is Mover based movement, not an impact-driven weight impulse.
 auto* Mover=GetOwner()->FindComponentByClass<UMoverComponent>();if(!Mover)return;
 const FVector Sweep=Hit.TraceEnd-Hit.TraceStart;
 const FVector V=Sweep.IsNearlyZero()?Mover->GetVelocity():Sweep/FMath::Max(GetWorld()->GetDeltaSeconds(),1.f/240.f);
 const FVector VB=Body->GetPhysicsLinearVelocityAtPoint(Hit.ImpactPoint);
 const double Speed=FMath::Clamp(-FVector::DotProduct(V-VB,N),0.,double(MaxContactSpeed));
 const double Now=GetWorld()->GetTimeSeconds();
 const double* Last=LastContactTime.Find(Body);
 const double Mass=Body->GetMass();
 const double SpeedBudget=Mass*FMath::Max(0.,90.*FMath::Min(1.,25./FMath::Max(1.,Mass))-FVector::DotProduct(VB,-N));
 const double Elapsed=Last?Now-*Last:.08;
 const double J=FMath::Min(NudgeImpulse(Speed,PlayerMassKg,Mass,Elapsed>.25?.08:Elapsed),SpeedBudget);
 if(J<=0)return;
 LastContactFrame.Add(Body,GFrameCounter);
 LastContactTime.Add(Body,Now);
 // Apply at centre of mass: a capsule-height contact must not catapult a light prop by torque.
 Body->AddImpulse(-N*J);
 // The sweep may already have removed the incoming velocity by the time the
 // effect executes. Applying a negative delta then would manufacture recoil.
 auto Reaction=MakeShared<FApplyVelocityEffect>();Reaction->bAdditiveVelocity=false;
 Reaction->VelocityToApply=V+N*Speed; // Grounded controller absorbs impact; don't re-inject sprint into the blocked direction.
 Mover->QueueInstantMovementEffect(Reaction);
 if(LogContacts.GetValueOnGameThread())UE_LOG(LogTemp,Log,TEXT("POI contact target=%s mass=%.2f closing=%.2f impulse=%.2f"),*OtherActor->GetName(),Mass,Speed,J);
}
void UPWGPhysicsContactComponent::EndPlay(const EEndPlayReason::Type Reason)
{
 if(auto* C=Capsule.Get()){C->OnComponentHit.RemoveDynamic(this,&UPWGPhysicsContactComponent::OnHit);C->SetCollisionEnabled(OriginalCollision);}
 RestoreCollisionPolicy();
 Super::EndPlay(Reason);
}
void UPWGPhysicsContactComponent::RestoreCollisionPolicy()
{
 for(auto& Pair:KinematicColliders)if(auto* C=Pair.Key.Get();C && C->GetCollisionEnabled()==ECollisionEnabled::QueryOnly)C->SetCollisionEnabled(Pair.Value);
 KinematicColliders.Reset();
 for(auto& Pair:AnimatedResponses)if(auto* C=Pair.Key.Get())
 {C->SetCollisionResponseToChannel(ECC_PhysicsBody,Pair.Value.Key);C->SetCollisionResponseToChannel(ECC_WorldDynamic,Pair.Value.Value);}
 AnimatedResponses.Reset();
}

