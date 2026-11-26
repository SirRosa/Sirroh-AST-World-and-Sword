#include "POI/POICorridorTools.h"
#include "Character/SirrohASTCombatBridgeComponent.h"
#include "POI/POIPendulum.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "PWG/PWGPhysicsContactComponent.h"
#include "POI/POIPushTraversalComponent.h"
#include "POI/POIPhysicalPushComponent.h"
#include "POI/POIMovingTraversalComponent.h"
#include "PWG/PWGSidescrollerInputComponent.h"
#include "MotionWarpingComponent.h"
#include "MoverDataModelTypes.h"
#include "KinematicActorSimulation.h"
#include "MoveLibrary/BasedMovementUtils.h"
#include "Components/ChildActorComponent.h"
#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Misc/AutomationTest.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"
#include "UObject/Script.h"
#if WITH_EDITOR
#include "StaticMeshCompiler.h"
#endif

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPOIPushSupportTest,"Sirroh.POI.PushSupport",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPOIPushSupportTest::RunTest(const FString& Parameters)
{
    for (double Mass:{10.,80.,500.})
    {
        const double J=UPWGPhysicsContactComponent::InelasticImpulse(600,80,Mass);
        const double PlayerAfter=600-J/80,BodyAfter=J/Mass;
        TestTrue(TEXT("Contact conserves momentum"),FMath::IsNearlyEqual(80*PlayerAfter+Mass*BodyAfter,80*600.,.01));
        TestTrue(TEXT("Contact dissipates rather than creates energy"),80*PlayerAfter*PlayerAfter+Mass*BodyAfter*BodyAfter<=80*600.*600.);
        TestTrue(TEXT("Both bodies reach same normal velocity"),FMath::IsNearlyEqual(PlayerAfter,BodyAfter,.01));
    }
    TestEqual(TEXT("Separating bodies get no impulse"),UPWGPhysicsContactComponent::InelasticImpulse(-100,80,10),0.);
    for(double Mass:{25.,40.,60.,250.,600.})
    {
        const double J=UPWGPhysicsContactComponent::NudgeImpulse(1200,80,Mass,.08);
        TestTrue(TEXT("Running contact cannot add more than 90 cm/s"),J/Mass<=90.);
        TestEqual(TEXT("No additional impulse without elapsed budget"),UPWGPhysicsContactComponent::NudgeImpulse(1200,80,Mass,0),0.);
    }
    for(double FPS:{30.,60.,120.})
        TestTrue(TEXT("Sustained contact force is frame-rate independent"),FMath::IsNearlyEqual(UPWGPhysicsContactComponent::NudgeImpulse(1200,80,600,1./FPS)*FPS,10000.,.01));
    TestTrue(TEXT("Heavy prop receives smaller sprint velocity than light prop"),UPWGPhysicsContactComponent::NudgeImpulse(1200,80,25,.08)/25 > UPWGPhysicsContactComponent::NudgeImpulse(1200,80,200,.08)/200);
    for(float Axis:{-1.f,1.f})
    {
        FVector Position(Axis*40,0,100),Velocity(Axis*600,0,0);
        UPOIPhysicalPushComponent::LimitMotion(0,0,0,Axis,Position,Velocity);
        TestTrue(TEXT("Blocked push clamps progress in either world direction"),FMath::IsNearlyEqual(Position.X,double(Axis)*12));
        TestEqual(TEXT("Blocked push cancels residual pawn speed"),Velocity.X,0.);
        Position=FVector(Axis*40,0,100);Velocity=FVector(-Axis*150,0,0);
        UPOIPhysicalPushComponent::LimitMotion(0,0,0,-Axis,Position,Velocity);
        TestEqual(TEXT("Reversing away from obstruction stays possible"),Velocity.X,double(-Axis)*150);
        Position=FVector(Axis*40,0,100);Velocity=FVector(Axis*600,0,0);
        UPOIPhysicalPushComponent::LimitMotion(0,Axis*30,0,Axis,Position,Velocity);
        TestEqual(TEXT("Pawn cannot outrun slow held prop"),Velocity.X,double(Axis)*30);
    }
    {
        FVector P(40,0,100),V(600,0,0);
        UPOIPhysicalPushComponent::LimitMotion(0,0,0,0,P,V);
        TestEqual(TEXT("Residual/root-motion speed is blocked without directional input"),V.X,0.);
        TestEqual(TEXT("Grip positional limit remains active without directional input"),P.X,12.);
    }
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(true);
    auto CreateFixture=[&]()
    {
        auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    auto* Floor=World->SpawnActor<AActor>();
    auto* Box=NewObject<UBoxComponent>(Floor); Floor->SetRootComponent(Box); Floor->AddInstanceComponent(Box);
    Box->SetBoxExtent(FVector(1000,1000,25)); Box->SetCollisionProfileName(TEXT("BlockAll"));
    Box->RegisterComponent(); Box->SetWorldLocation(FVector(0,0,-25));
        return World;
    };
    auto* World=CreateFixture();
    for (float StartZ:{20.f,50.f,100.f})
    {
        FHitResult Hit;
        const bool Found=UPOICorridorTools::PushFloorTrace(World,FVector(0,0,StartZ),FVector(0,0,-85),FVector(46,46,48),FRotator::ZeroRotator,UEngineTypes::ConvertToTraceType(ECC_Visibility),false,{},EDrawDebugTrace::None,Hit,true,FLinearColor::Red,FLinearColor::Green,0);
        TestTrue(TEXT("Support found even from overlapping box start"),Found);
        TestTrue(TEXT("Support centre is floor plus half extent, not penetrating start"),FMath::IsNearlyEqual(Hit.Location.Z,48.f,.2f));
        TestFalse(TEXT("No penetrating hit passed to AST"),Hit.bStartPenetrating);
    }
    for (const TCHAR* Name:{TEXT("Barrel"),TEXT("Crate"),TEXT("Bench")})
    {
        // Each variant needs its own scene: synchronous automation runs all variants in one engine frame.
        GEngine->DestroyWorldContext(World); World->DestroyWorld(false);
        World=CreateFixture();
        auto* Mesh=LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("/Game/POIKit/Underground/Meshes/SM_Pushable_%s.SM_Pushable_%s"),Name,Name));
        if (TestNotNull(TEXT("Pushable mesh exists"),Mesh))
        {
#if WITH_EDITOR
            TArray<UStaticMesh*> ReadyMeshes{Mesh};
            FStaticMeshCompilingManager::Get().FinishCompilation(ReadyMeshes);
#endif
            TestTrue(TEXT("Mesh pivot is at its centre"),Mesh->GetBoundingBox().GetCenter().IsNearlyZero(.01f));
            auto* Class=LoadClass<AActor>(nullptr,*FString::Printf(TEXT("/Game/POIKit/Underground/Actors/BP_Pushable_U_%s.BP_Pushable_U_%s_C"),Name,Name));
            if (!TestNotNull(TEXT("Pushable class"),Class)) continue;
            auto* Object=World->SpawnActor<AActor>(Class);
            Object->RegisterAllComponents();
            for (float Scale:{1.f,1.5f})
            {
                Object->SetActorScale3D(FVector(Scale));
                const float HalfZ=Mesh->GetBoundingBox().GetExtent().Z*Scale;
                const FVector Start(0,0,HalfZ+10);
                const FVector End=UPOICorridorTools::PushSupportEnd(Object,Start);
                FHitResult Support;
                TestTrue(TEXT("Release probe reaches floor for every pushable size"),UKismetSystemLibrary::SphereTraceSingle(World,Start,End,5,UEngineTypes::ConvertToTraceType(ECC_Visibility),false,{Object},EDrawDebugTrace::None,Support,true));
                TestTrue(TEXT("Release probe preserves 35cm clearance rather than unlimited reach"),FMath::IsNearlyEqual(End.Z,-25.f,.1f));
            }
            if (auto* Traversal=Object->FindComponentByClass<UPOIPushTraversalComponent>())
            {
                for (const FRotator Rotation:{FRotator::ZeroRotator,FRotator(90,0,0),FRotator(-90,0,0),FRotator(180,0,0),FRotator(0,0,90),FRotator(0,0,-90)})
                {
                    Object->SetActorRotation(Rotation);Traversal->RefreshEdges();
                    const auto* Root=Cast<UStaticMeshComponent>(Object->GetRootComponent());
                    const double Top=Root->CalcBounds(Root->GetComponentTransform()).GetBox().Max.Z;
                    int Count=0;TArray<UChildActorComponent*> Edges;Object->GetComponents(Edges);
                    for (auto* Edge:Edges) if(Edge->GetName().StartsWith(TEXT("ParkourActor")) && Edge->GetChildActor())
                    {
                        auto* Spline=Edge->GetChildActor()->FindComponentByClass<USplineComponent>();
                        if (!TestNotNull(TEXT("Original AST spline"),Spline)) continue;
                        ++Count;
                        TestTrue(TEXT("Ledge start follows upper face after tipping"),FMath::IsNearlyEqual(Spline->GetLocationAtSplinePoint(0,ESplineCoordinateSpace::World).Z,Top,.2));
                        TestTrue(TEXT("Ledge end follows upper face after tipping"),FMath::IsNearlyEqual(Spline->GetLocationAtSplinePoint(1,ESplineCoordinateSpace::World).Z,Top,.2));
                    }
                    TestEqual(TEXT("Four native edges reused for each exposed face"),Count,4);
                }
                Object->SetActorRotation(FRotator::ZeroRotator);Traversal->RefreshEdges();
            }
            else AddError(TEXT("Missing pushable traversal adapter"));
            auto* Body=Cast<UPrimitiveComponent>(Object->GetRootComponent());
            auto* State=Object->FindFunction(TEXT("Set Pull/PushObje State"));
            if (TestNotNull(TEXT("AST state event"),State) && TestNotNull(TEXT("Physics root"),Body))
            {
#if WITH_EDITOR
                if (auto* Static=Cast<UStaticMeshComponent>(Body);Static && Static->GetStaticMesh())
                { TArray<UStaticMesh*> Ready{Static->GetStaticMesh()};FStaticMeshCompilingManager::Get().FinishCompilation(Ready); }
#endif
                Body->RecreatePhysicsState();
                if (auto* Traversal=Object->FindComponentByClass<UPOIPushTraversalComponent>()) Traversal->BeginPlay();
                TestTrue(TEXT("Pushable simulates before any interaction"),Body->IsSimulatingPhysics());
                AddInfo(FString::Printf(TEXT("%s physical mass %.2f kg"),Name,Body->GetMass()));
                FEditorScriptExecutionGuard ScriptGuard;
                FStructOnScope Args(State);
                auto* Param=FindFProperty<FByteProperty>(State,TEXT("PullPushState"));
                if (TestNotNull(TEXT("AST state parameter"),Param))
                {
                    auto SetState=[&](uint8 Value){Param->SetPropertyValue_InContainer(Args.GetStructMemory(),Value);Object->ProcessEvent(State,Args.GetStructMemory());};
                    SetState(1);
                    AddInfo(FString::Printf(TEXT("%s: enum=%s state=%s simulate=%d validbody=%d registered=%d collision=%d"),Name,*Param->Enum->GetNameStringByValue(1),*FindFProperty<FByteProperty>(Object->GetClass(),TEXT("PullpushState"))->Enum->GetNameStringByValue(FindFProperty<FByteProperty>(Object->GetClass(),TEXT("PullpushState"))->GetPropertyValue_InContainer(Object)),Body->BodyInstance.bSimulatePhysics,Body->BodyInstance.IsValidBodyInstance(),Body->IsRegistered(),int32(Body->GetCollisionEnabled())));
                    TestTrue(TEXT("Released object retains physics"),Body->IsSimulatingPhysics());
                    // Isolate impulse response from the floor: the trace fixture spawned the centred mesh at Z=0.
                    Object->SetActorLocation(FVector(0,0,1000),false,nullptr,ETeleportType::TeleportPhysics);
                    Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
                    Body->PutRigidBodyToSleep();
                    TestFalse(TEXT("Sleeping body is at rest"),Body->IsAnyRigidBodyAwake());
                    TestTrue(TEXT("Sleep does not disable simulation"),Body->IsSimulatingPhysics());
                    TestFalse(TEXT("Resting weapon overlap does not strike"),UPOICorridorTools::ApplyPropStrike(Body,FVector::ZeroVector,4));
                    TestTrue(TEXT("Measured weapon strike reaches physical prop"),UPOICorridorTools::ApplyPropStrike(Body,FVector(600,0,0),4));
                    for(int Step=0;Step<4;++Step) World->Tick(LEVELTICK_All,1.f/60.f);
                    AddInfo(FString::Printf(TEXT("Strike %s awake=%d velocity=%s mass=%f"),Name,Body->IsAnyRigidBodyAwake(),*Body->GetPhysicsLinearVelocity().ToString(),Body->GetMass()));
                    TestTrue(TEXT("Strike wakes prop and changes velocity"),Body->IsAnyRigidBodyAwake() && Body->GetPhysicsLinearVelocity().X>0);
                    TestTrue(TEXT("Strike velocity is bounded"),Body->GetPhysicsLinearVelocity().X<=100.01);
                    Body->AddImpulse(FVector(10000,0,0));
                    TestTrue(TEXT("Impulse wakes sleeping pushable"),Body->IsAnyRigidBodyAwake());
                    auto* Puncher=World->SpawnActor<AActor>();
                    for(double Side:{-1.,1.})
                    {
                        Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
                        const FBox Bounds=Body->Bounds.GetBox();
                        const double Face=Side<0?Bounds.Min.X:Bounds.Max.X;
                        const FVector Start(Face+Side*20,Body->GetComponentLocation().Y,Body->GetComponentLocation().Z);
                        const FVector End(Face-Side*10,Start.Y,Start.Z);
                        TSet<TWeakObjectPtr<AActor>> Targets;
                        TestTrue(TEXT("Fist sweep reaches physical prop from either side"),USirrohASTCombatBridgeComponent::TracePropFist(Puncher,Start,End,1.f/60.f,Targets));
                        TestFalse(TEXT("Same fist attack cannot impulse target twice"),USirrohASTCombatBridgeComponent::TracePropFist(Puncher,Start,End,1.f/60.f,Targets));
                        Targets.Reset();
                        TestFalse(TEXT("Stationary fist does not impulse prop"),USirrohASTCombatBridgeComponent::TracePropFist(Puncher,Start,Start,1.f/60.f,Targets));
                    }
                    Puncher->Destroy();
                    const FVector Before=Object->GetActorLocation();
                    UPOICorridorTools::PreparePhysicalPush(Object);
                    SetState(0);
                    TestFalse(TEXT("Held state disables simulation before attachment"),Body->IsSimulatingPhysics());
                    auto* Driver=World->SpawnActor<AActor>();
                    auto* DriverRoot=NewObject<USceneComponent>(Driver);Driver->AddInstanceComponent(DriverRoot);Driver->SetRootComponent(DriverRoot);DriverRoot->RegisterComponent();
                    auto* DriverMover=NewObject<UMoverComponent>(Driver);Driver->AddInstanceComponent(DriverMover);DriverMover->RegisterComponent();
                    auto* Proxy=NewObject<UBoxComponent>(Driver);Driver->AddInstanceComponent(Proxy);Proxy->SetupAttachment(DriverRoot);Proxy->RegisterComponent();
                    Driver->SetActorLocation(Object->GetActorLocation()-FVector(120,0,0));
                    Object->AttachToComponent(Proxy,FAttachmentTransformRules::KeepWorldTransform);
                    // Reproduce a displaced proxy during AST setup, not just a
                    // trivial attach/detach at the same world transform.
                    Proxy->AddWorldOffset(FVector(500,0,0));
                    UPOICorridorTools::BeginPhysicalPush(Object);
                    auto* Drive=Object->FindComponentByClass<UPOIPhysicalPushComponent>();
                    TestNotNull(TEXT("Physical push drive created"),Drive);
                    TestTrue(TEXT("Driven object remains dynamic"),Body->IsSimulatingPhysics());
                    TestNull(TEXT("Physics body is not attached to character"),Body->GetAttachParent());
                    TestTrue(TEXT("Starting push does not teleport body"),Before.Equals(Object->GetActorLocation(),.01));
                    TestEqual(TEXT("AST proxy cannot double physical contacts"),Proxy->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
                    if(Drive)
                    {
                        TestTrue(TEXT("Driver recognized without attachment"),Drive->IsDrivenBy(Driver));
                        Drive->LimitInput(0);
                        FMoverSyncState PushState;FMoverAuxStateContext PushAux;FMoverTimeStep PushStep;
                        auto& PushSync=PushState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();
                        PushSync.SetTransforms_WorldSpace(Driver->GetActorLocation()+FVector(50,0,0),FRotator::ZeroRotator,FVector(500,0,0),FVector::ZeroVector);
                        Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
                        DriverMover->OnPostMovement.Broadcast(PushStep,PushState,PushAux);
                        TestTrue(TEXT("Registered push callback clamps actual Mover state without input"),FMath::IsNearlyEqual(PushSync.GetLocation_WorldSpace().X,Driver->GetActorLocation().X+12.));
                        TestEqual(TEXT("Registered push callback cancels actual Mover state speed"),PushSync.GetVelocity_WorldSpace().X,0.);

                        Driver->AddActorWorldOffset(FVector(15,0,0));
                        TestEqual(TEXT("Blocked body stops forward input"),Drive->LimitInput(1),0.f);
                        TestEqual(TEXT("Can reverse away from blocked body"),Drive->LimitInput(-1),-1.f);
                        TestFalse(TEXT("Blocked but reachable object must not be released by missing floor"),UPOICorridorTools::ShouldReleasePhysicalPush(Object));
                        Driver->AddActorWorldOffset(FVector(10000,0,0));
                        TestTrue(TEXT("Out-of-reach grip releases"),UPOICorridorTools::ShouldReleasePhysicalPush(Object));
                    }
                    SetState(1);
                    TestTrue(TEXT("Release enables physics again"),Body->IsSimulatingPhysics());
                    if(Drive)TestFalse(TEXT("AST release ends physical drive immediately"),Drive->IsDrivenBy(Driver));
                    Driver->Destroy();
                }
            }
            Object->Destroy();
        }
    }
    GEngine->DestroyWorldContext(World); World->DestroyWorld(false);World=CreateFixture();
    auto* Payload=World->SpawnActor<AActor>();
    auto* PayloadBody=NewObject<UStaticMeshComponent>(Payload);Payload->SetRootComponent(PayloadBody);Payload->AddInstanceComponent(PayloadBody);
    PayloadBody->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));PayloadBody->SetCollisionProfileName(TEXT("PhysicsActor"));PayloadBody->RegisterComponent();Payload->SetActorLocation(FVector(0,0,150));
    auto* Pendulum=World->SpawnActor<APOIPendulum>();Pendulum->SetActorLocation(FVector(0,0,500));Pendulum->Payload=Payload;Pendulum->DispatchBeginPlay();
    TestTrue(TEXT("Pendulum rod simulates"),Pendulum->Rod->IsSimulatingPhysics());
    TestTrue(TEXT("Pendulum payload simulates"),PayloadBody->IsSimulatingPhysics());
    TestTrue(TEXT("Anchor constraint is initialized"),Pendulum->AnchorJoint->ConstraintInstance.IsValidConstraintInstance());
    TestTrue(TEXT("Payload constraint is initialized"),Pendulum->PayloadJoint->ConstraintInstance.IsValidConstraintInstance());
    Pendulum->Destroy();
    GEngine->DestroyWorldContext(World); World->DestroyWorld(false); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPOIMovingTraversalTest,"Sirroh.POI.MovingTraversal",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPOIMovingTraversalTest::RunTest(const FString& Parameters)
{
 const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(true);
 auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
 FEditorScriptExecutionGuard Guard;
 auto* Class=LoadClass<AActor>(nullptr,TEXT("/Game/SirrohIntegration/BP_SirrohCharacter.BP_SirrohCharacter_C"));
 auto* Pawn=Class?World->SpawnActor<AActor>(Class):nullptr;
 if(TestNotNull(TEXT("Actual Sirroh pawn"),Pawn))
 {
  auto* Input=NewObject<UPWGSidescrollerInputComponent>(Pawn);Pawn->AddInstanceComponent(Input);Input->RegisterComponent();
  if(!Input->HasBeenInitialized())Input->InitializeComponent();
  auto* Contact=Pawn->FindComponentByClass<UPWGPhysicsContactComponent>();
  auto* AnimatedBody=NewObject<UBoxComponent>(Pawn);Pawn->AddInstanceComponent(AnimatedBody);
  AnimatedBody->SetBoxExtent(FVector(10));AnimatedBody->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);AnimatedBody->RegisterComponent();AnimatedBody->SetSimulatePhysics(true);
  const auto OldResponse=AnimatedBody->GetCollisionResponseToChannel(ECC_PhysicsBody);
  if(TestNotNull(TEXT("Contact policy on Sirroh pawn"),Contact))
  {
   Contact->BeginPlay();
   TestTrue(TEXT("Animated physical body retains simulation"),AnimatedBody->IsSimulatingPhysics());
   TestEqual(TEXT("Animated body cannot launch props"),AnimatedBody->GetCollisionResponseToChannel(ECC_PhysicsBody),ECR_Ignore);
   TestEqual(TEXT("Capsule retains query but not solver contact"),CastChecked<UPrimitiveComponent>(Pawn->GetRootComponent())->GetCollisionEnabled(),ECollisionEnabled::QueryOnly);
   Contact->EndPlay(EEndPlayReason::RemovedFromWorld);
   TestEqual(TEXT("Contact policy restores body response"),AnimatedBody->GetCollisionResponseToChannel(ECC_PhysicsBody),OldResponse);
  }
  AnimatedBody->DestroyComponent();
  UActorComponent* StateManager=nullptr;FByteProperty* StateProperty=nullptr;
  for(auto* Part:Pawn->GetComponents())if(Part && Part->ComponentHasTag(TEXT("StateManager")))
   if(auto* P=FindFProperty<FByteProperty>(Part->GetClass(),TEXT("ParkourState"))){StateManager=Part;StateProperty=P;break;}
  auto* Warp=Pawn->FindComponentByClass<UMotionWarpingComponent>();auto* Mover=Pawn->FindComponentByClass<UMoverComponent>();
  if(TestNotNull(TEXT("Native AST state"),StateProperty) && TestNotNull(TEXT("Native MotionWarping"),Warp) && TestNotNull(TEXT("Native Mover"),Mover))
  {
   StateProperty->SetPropertyValue_InContainer(StateManager,0);
   auto* Platform=World->SpawnActor<AActor>();auto* Body=NewObject<UBoxComponent>(Platform);
   Platform->AddInstanceComponent(Body);Platform->SetRootComponent(Body);Body->SetBoxExtent(FVector(100));Body->SetMobility(EComponentMobility::Movable);Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);Body->RegisterComponent();Body->SetSimulatePhysics(true);
   const FVector Grip(30,0,100),Landing(500,0,-100);
   UPOIMovingTraversalComponent::Track(Pawn,Body,true); // Capture selection before delayed AST animation starts.
   const FTransform SelectionMotion(FRotator::ZeroRotator,FVector(80,0,0));
   Body->SetWorldTransform(SelectionMotion,false,nullptr,ETeleportType::TeleportPhysics);
   StateProperty->SetPropertyValue_InContainer(StateManager,1);
   TestTrue(TEXT("AST traversal test state active"),Input->IsTraversalActive());
   Warp->AddOrUpdateWarpTargetFromLocation(TEXT("FrontLedge"),Grip);
   Warp->AddOrUpdateWarpTargetFromLocation(TEXT("BackFloor"),Landing);
   UPOIMovingTraversalComponent::Track(Pawn,Body);
   TestTrue(TEXT("Delayed AST target is rebased from selection time"),Warp->FindWarpTarget(TEXT("FrontLedge"))->GetLocation().Equals(SelectionMotion.TransformPosition(Grip),.1));
   TestTrue(TEXT("Traversal does not freeze physical support"),Body->IsSimulatingPhysics());
   TestTrue(TEXT("Ledge target follows component"),Warp->FindWarpTarget(TEXT("FrontLedge"))->bFollowComponent);
   const FTransform Motion(FRotator(0,0,25),FVector(100,0,-50));
   Body->SetWorldTransform(Motion,false,nullptr,ETeleportType::TeleportPhysics);
   TestTrue(TEXT("Grip follows translation and rotation"),Warp->FindWarpTarget(TEXT("FrontLedge"))->GetLocation().Equals(Motion.TransformPosition(Grip),.1));
   TestTrue(TEXT("Landing floor is not dragged with obstacle"),Warp->FindWarpTarget(TEXT("BackFloor"))->GetLocation().Equals(Landing,.1));
   FMoverSyncState State;FMoverAuxStateContext Aux;FMoverTimeStep Step;
   Mover->SetSimulation(*NewObject<UKinematicActorSimulation>(Mover));
   State.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>().SetTransforms_WorldSpace(FVector(0,0,200),FRotator::ZeroRotator,FVector::ZeroVector);
   Mover->OnPostMovement.Broadcast(Step,State,Aux);
   TestEqual(TEXT("Hang uses Mover dynamic base"),State.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>()->GetMovementBase(),static_cast<UPrimitiveComponent*>(Body));
   FRelativeBaseInfo BaseInfo;
   TestTrue(TEXT("Post-physics base update has its required snapshot"),Mover->GetSimBlackboard_Mutable()->TryGet(CommonBlackboard::LastFoundDynamicMovementBase,BaseInfo));
   TestTrue(TEXT("Base snapshot identifies support"),BaseInfo.UsesSameBase(Body));
   StateProperty->SetPropertyValue_InContainer(StateManager,0);Mover->OnPostMovement.Broadcast(Step,State,Aux);
   TestFalse(TEXT("Leaving traversal releases warp anchor"),Warp->FindWarpTarget(TEXT("FrontLedge"))->Component.IsValid());
   TestTrue(TEXT("Support remains dynamic after release"),Body->IsSimulatingPhysics());
  }
 }
 GEngine->DestroyWorldContext(World);World->DestroyWorld(false);return true;
}
#endif




