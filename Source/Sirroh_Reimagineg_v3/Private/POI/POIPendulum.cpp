#include "POI/POIPendulum.h"
#include "POI/POIMovingTraversalComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
APOIPendulum::APOIPendulum()
{
 bReplicates=true;SetReplicateMovement(true);PrimaryActorTick.bCanEverTick=true;SetNetUpdateFrequency(20);
 auto* Anchor=CreateDefaultSubobject<UBoxComponent>(TEXT("Anchor"));Anchor->SetBoxExtent(FVector(5));Anchor->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);Anchor->SetCollisionResponseToAllChannels(ECR_Ignore);SetRootComponent(Anchor);
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
 Rod=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PhysicsRod"));Rod->SetupAttachment(RootComponent);Rod->SetStaticMesh(Cube.Object);Rod->SetCollisionProfileName(TEXT("PhysicsActor"));Rod->SetCollisionResponseToAllChannels(ECR_Ignore);Rod->SetHiddenInGame(true);
 for(int I=0;I<6;++I){auto* Link=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Link%d"),I));Link->SetupAttachment(RootComponent);Link->SetStaticMesh(Cube.Object);Link->SetCollisionEnabled(ECollisionEnabled::NoCollision);Links.Add(Link);}
 AnchorJoint=CreateDefaultSubobject<UPhysicsConstraintComponent>(TEXT("AnchorJoint"));AnchorJoint->SetupAttachment(RootComponent);
 PayloadJoint=CreateDefaultSubobject<UPhysicsConstraintComponent>(TEXT("PayloadJoint"));PayloadJoint->SetupAttachment(RootComponent);
 static ConstructorHelpers::FClassFinder<AActor> Crate(TEXT("/Game/POIKit/Underground/Actors/BP_Pushable_U_Crate"));PayloadClass=Crate.Class;
}
void APOIPendulum::OnConstruction(const FTransform& T)
{
 Super::OnConstruction(T);Length=FMath::Clamp(Length,100.f,1000.f);
 Rod->SetRelativeLocation(FVector(0,0,-Length*.5));Rod->SetRelativeScale3D(FVector(.08,.08,Length/100));
 for(int I=0;I<Links.Num();++I){Links[I]->SetRelativeLocation(FVector(0,0,-Length*(I+.5)/Links.Num()));Links[I]->SetRelativeScale3D(FVector(.1,.1,Length/Links.Num()/110));}
 PayloadJoint->SetRelativeLocation(FVector(0,0,-Length));
}
void APOIPendulum::BeginPlay()
{
 Super::BeginPlay();
 if(!HasAuthority())return;
 RodPose=Rod->GetComponentTransform();
 const FVector Bottom=GetActorLocation()-FVector(0,0,Length);
 if(!Payload && PayloadClass){FActorSpawnParameters P;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;Payload=GetWorld()->SpawnActor<AActor>(PayloadClass,Bottom,FRotator::ZeroRotator,P);bSpawnedPayload=true;}
 auto* Body=Payload?Cast<UPrimitiveComponent>(Payload->GetRootComponent()):nullptr;
 if(!Body){SetActorTickEnabled(false);return;}
 if(bSpawnedPayload)Payload->AddActorWorldOffset(FVector(0,0,-Body->Bounds.BoxExtent.Z),false,nullptr,ETeleportType::TeleportPhysics);
 Payload->SetReplicates(true);Payload->SetReplicateMovement(true);
 Body->SetSimulatePhysics(true);Body->SetUseCCD(true);
 Rod->BodyInstance.bLockYTranslation=true;Rod->BodyInstance.bLockXRotation=true;Rod->BodyInstance.bLockZRotation=true;Rod->BodyInstance.SetDOFLock(EDOFMode::SixDOF);
 Rod->SetMassOverrideInKg(NAME_None,3);Rod->SetSimulatePhysics(true);Rod->SetAngularDamping(.15);
 for(auto* Joint:{AnchorJoint.Get(),PayloadJoint.Get()})
 {
  Joint->SetDisableCollision(true);Joint->SetLinearXLimit(LCM_Locked,0);Joint->SetLinearYLimit(LCM_Locked,0);Joint->SetLinearZLimit(LCM_Locked,0);
  Joint->SetAngularSwing1Limit(ACM_Free,0);Joint->SetAngularSwing2Limit(ACM_Free,0);Joint->SetAngularTwistLimit(ACM_Free,0);
 }
 AnchorJoint->SetConstrainedComponents(Rod,NAME_None,Cast<UPrimitiveComponent>(RootComponent),NAME_None);
 PayloadJoint->SetConstrainedComponents(Rod,NAME_None,Body,NAME_None);
 Rod->SetPhysicsLinearVelocity(FVector(InitialSpeed,0,0));Body->SetPhysicsLinearVelocity(FVector(InitialSpeed,0,0));
}
void APOIPendulum::Tick(float Dt)
{
 Super::Tick(Dt);
 auto* Body=Payload?Cast<UPrimitiveComponent>(Payload->GetRootComponent()):nullptr;
 if(HasAuthority())
 {
  RepTime+=Dt;if(RepTime>=.05f){RepTime=0;RodPose=Rod->GetComponentTransform();}
  if(Body && Body->IsSimulatingPhysics())
  {
   if(bTestMotor)Body->AddForce(FVector(FMath::Sin(GetWorld()->GetTimeSeconds()*1.5)*TestForceNewtons*100,0,0));
   for(TActorIterator<APawn> It(GetWorld());It;++It)if(auto* Pawn=*It;FVector::DistSquared(Pawn->GetActorLocation(),Body->GetComponentLocation())<1000000)
   {
    bool Loaded=false;if(auto* Track=Pawn->FindComponentByClass<UPOIMovingTraversalComponent>())Loaded=Track->IsTracking(Body);
    const FVector P=Pawn->GetActorLocation();FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(PendulumLoad),false,Pawn);
    if(!Loaded && GetWorld()->LineTraceSingleByChannel(Hit,P,P-FVector(0,0,140),ECC_Visibility,Q))Loaded=Hit.GetComponent()==Body;
    if(Loaded){FVector Contact=Body->Bounds.GetBox().GetClosestPointTo(P);Body->AddForceAtLocation(FVector(0,0,-80*980),Contact);}
   }
  }
 }
 else
 {
  FTransform Smoothed=RodPose;
  Smoothed.SetLocation(FMath::VInterpTo(Rod->GetComponentLocation(),RodPose.GetLocation(),Dt,20));
  Smoothed.SetRotation(FQuat::Slerp(Rod->GetComponentQuat(),RodPose.GetRotation(),FMath::Clamp(Dt*20.f,0.f,1.f)));
  Rod->SetWorldTransform(Smoothed,false,nullptr,ETeleportType::TeleportPhysics);
 }
 const FTransform T=Rod->GetComponentTransform();
 for(int I=0;I<Links.Num();++I){Links[I]->SetWorldLocation(T.TransformPosition(FVector(0,0,50.-100.*(I+.5)/Links.Num())));Links[I]->SetWorldRotation(T.GetRotation());}
}
void APOIPendulum::EndPlay(const EEndPlayReason::Type Reason)
{
 AnchorJoint->BreakConstraint();PayloadJoint->BreakConstraint();if(HasAuthority() && bSpawnedPayload && IsValid(Payload))Payload->Destroy();Super::EndPlay(Reason);
}
void APOIPendulum::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{Super::GetLifetimeReplicatedProps(OutLifetimeProps);DOREPLIFETIME(APOIPendulum,Payload);DOREPLIFETIME(APOIPendulum,RodPose);}
