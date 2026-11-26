#include "PWG/PWGGameMode.h"
#include "UObject/UnrealType.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Engine/StaticMesh.h"
#include "Character/SirrohCharacterIdentityComponent.h"
#include "Procedural_World_Generator/PWGv2Generator.h"
#include "Procedural_World_Generator/PWGv2WorldSession.h"
#include "MoveLibrary/ConstrainedMoveUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"




#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Engine/DataTable.h"
#include "EngineUtils.h"
#include "GameFramework/MovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "MoverComponent.h"
#include "DefaultMovementSet/InstantMovementEffects/BasicInstantMovementEffects.h"
#include "Procedural_World_Generator/PWGv2Generator.h"
#include "PWG/PWGSidescrollerInputComponent.h"
#include "Combat/CombatTrainingNPC.h"
#include "PWG/PWGNavigationBounds.h"
#include "NavigationSystem.h"
#include "TimerManager.h"




DEFINE_LOG_CATEGORY_STATIC(LogPWGGameMode, Log, All);

APWGGameMode::APWGGameMode()
{
        DefaultPawnClass = nullptr;
        GeneratorClass = APWGv2Generator::StaticClass();

}




void APWGGameMode::BeginPlay()
{
        Super::BeginPlay();
        if (bStartAutomatically)
        {
                StartGameAfterCharacterCreation();
        }
}




bool APWGGameMode::StartGameAfterCharacterCreation()
{
        if (bGameStarted) return true;
        UWorld* World = GetWorld();
        if (!World)
        {
                UE_LOG(LogPWGGameMode, Error, TEXT("PWG start failed: no World."));
                return false;
        }

        UClass* ResolvedPlayerPawnClass = PlayerPawnClass.Get();
        if (!ResolvedPlayerPawnClass)
        {
                UE_LOG(LogPWGGameMode, Error, TEXT("PWG start failed: PlayerPawnClass is not configured."));
                return false;
        }




        if (bPOITestMode)
        {
            APlayerController* Controller = World->GetFirstPlayerController();
            if (!Controller) return false;
            if (!Controller->GetPawn()) RestartPlayer(Controller);
            bGameStarted = IsValid(Controller->GetPawn());
            if (bGameStarted) USirrohCharacterIdentityComponent::ApplySaved(Controller->GetPawn());
            return bGameStarted;
        }

        for (TActorIterator<APWGv2Generator> It(World); It; ++It) { WorldGenerator=*It; break; }
        if (!WorldGenerator)
        {
            UClass* ClassToSpawn=GeneratorClass.Get();
            if (!ClassToSpawn || !ClassToSpawn->IsChildOf(APWGv2Generator::StaticClass())) ClassToSpawn=APWGv2Generator::StaticClass();
            WorldGenerator=World->SpawnActor<APWGv2Generator>(ClassToSpawn,FTransform::Identity);
        }
        if (!WorldGenerator)
        {
                UE_LOG(LogPWGGameMode, Error, TEXT("PWG start failed: generator could not be spawned."));
                return false;
        }
        if (auto* V2=Cast<APWGv2Generator>(WorldGenerator))
        {
            V2->WorldName=PWGv2WorldName;
            V2->SeedOverride=PWGv2Seed;
            if (const auto* Session=GetGameInstance()->GetSubsystem<UPWGv2WorldSession>(); Session && !Session->SelectedWorld.IsEmpty())
            { V2->WorldName=Session->SelectedWorld; V2->SeedOverride=Session->RequestedSeed; }
        }
        if (!WorldGenerator->GenerateWorld(BiomeRowName))
        {
            if (auto* Session=GetGameInstance()->GetSubsystem<UPWGv2WorldSession>()) Session->LastError=TEXT("Nie udalo sie wczytac/generowac swiata. Zapis nie zostal zastapiony. Sprawdz log PWGv2.");
            UGameplayStatics::OpenLevel(this,TEXT("L_MainMenu")); return false;
        }




        APlayerController* PC = World->GetFirstPlayerController();
        if (!PC)
        {
                UE_LOG(LogPWGGameMode, Error, TEXT("PWG start failed: no PlayerController exists."));
                return false;
        }




        if (APawn* OldPawn = PC->GetPawn())
        {
                PC->UnPossess();
                OldPawn->Destroy();
        }




        FVector SpawnLocation = WorldGenerator->GetWorldCenterSurfaceLocation();
        int32 ArrivalLane=1;
        if (auto* V2=Cast<APWGv2Generator>(WorldGenerator)) V2->GetSavedArrival(SpawnLocation,ArrivalLane);
        if (SpawnLocation.Z <= -BIG_NUMBER * 0.5f)
        {
                UE_LOG(LogPWGGameMode, Error, TEXT("PWG start failed: centre surface height is invalid."));
                return false;
        }




        const float SurfaceZ = SpawnLocation.Z;
        // Finish construction safely above the terrain first. Blueprint construction
        // can change capsule size/offset, so measuring it before FinishSpawning is unreliable.
        SpawnLocation.Z = SurfaceZ + 1000.0f;
        FTransform SpawnTransform(FRotator::ZeroRotator, SpawnLocation);
        APawn* NewPawn = SpawnConfiguredPlayer(SpawnTransform);
        if (!NewPawn) return false;

        float CollisionBottomZ = NewPawn->GetActorLocation().Z;
        if (const UCapsuleComponent* Capsule = NewPawn->FindComponentByClass<UCapsuleComponent>())
        {
                CollisionBottomZ = Capsule->GetComponentLocation().Z - Capsule->GetScaledCapsuleHalfHeight();
        }
        else
        {
                const FBox Bounds = NewPawn->GetComponentsBoundingBox(true);
                if (Bounds.IsValid) CollisionBottomZ = Bounds.Min.Z;
        }




        constexpr float FloorClearance = 3.0f;
        const float VerticalCorrection = SurfaceZ + FloorClearance - CollisionBottomZ;
        SpawnLocation = NewPawn->GetActorLocation() + FVector(0.0f, 0.0f, VerticalCorrection);
        // Height() describes the rendered surface; convex collision and capsule radius
        // can put its actual support point higher, especially on slopes and seams.
        if (const UCapsuleComponent* Capsule = NewPawn->FindComponentByClass<UCapsuleComponent>())
        {
            FCollisionQueryParams Query(SCENE_QUERY_STAT(PWGSpawnFloor), false, NewPawn);
            FHitResult FloorHit;
            const FVector CapsuleOffset = Capsule->GetComponentLocation() - NewPawn->GetActorLocation();
            const FVector SweepStart = NewPawn->GetActorLocation() + CapsuleOffset;
            const FVector SweepEnd = SpawnLocation + CapsuleOffset - FVector(0, 0, 200);
            if (World->SweepSingleByChannel(FloorHit, SweepStart, SweepEnd, Capsule->GetComponentQuat(),
                Capsule->GetCollisionObjectType(), Capsule->GetCollisionShape(), Query,
                FCollisionResponseParams(Capsule->GetCollisionResponseToChannels())) && !FloorHit.bStartPenetrating)
            {
                SpawnLocation = FloorHit.Location - CapsuleOffset + FVector(0, 0, FloorClearance);
            }
            else
            {
                // Keep the safe elevated spawn and let Mover find the floor normally.
                SpawnLocation = NewPawn->GetActorLocation();
                UE_LOG(LogPWGGameMode, Warning, TEXT("PWG spawn floor sweep failed or began in penetration; using elevated spawn."));
            }
        }
        if (UMoverComponent* Mover = NewPawn->FindComponentByClass<UMoverComponent>())
        {
            auto Teleport = MakeShared<FTeleportEffect>();
            Teleport->TargetLocation = SpawnLocation;
            Teleport->bUseActorRotation = true;
            Mover->QueueInstantMovementEffect(Teleport);
        }
        else NewPawn->SetActorLocation(SpawnLocation, false, nullptr, ETeleportType::TeleportPhysics);
        if (UMovementComponent* Movement = NewPawn->FindComponentByClass<UMovementComponent>())
        {
                Movement->StopMovementImmediately();
        }
        PC->Possess(NewPawn);
        USirrohCharacterIdentityComponent::ApplySaved(NewPawn);
    WorldGenerator->EnsureStarterSwordNear(SpawnLocation);
    SpawnASTIntegrationObjects(SpawnLocation);
    if (bSpawnCombatTrainingNPC && !IsValid(TrainingNavigationBounds))
    {
        TrainingNavigationBounds=World->SpawnActor<APWGNavigationBounds>(SpawnLocation,FRotator::ZeroRotator);
        if (auto* Navigation=FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
        {
            Navigation->OnNavigationBoundsUpdated(TrainingNavigationBounds);
            // Bounds updates are queued. Build after the navigation system has
            // consumed them instead of building an empty tile set in this frame.
            TWeakObjectPtr<UNavigationSystemV1> WeakNavigation=Navigation;
            GetWorldTimerManager().SetTimerForNextTick([WeakNavigation]()
            {
                if (WeakNavigation.IsValid()) WeakNavigation->Build();
            });
        }
    }




    if (bSpawnCombatTrainingNPC && !IsValid(CombatTrainingNPC)) ResetTrainingNPC();

        if (PC->PlayerCameraManager && SpawnFadeDuration > 0.0f)
        {
                PC->PlayerCameraManager->SetManualCameraFade(1.0f, FLinearColor::Black, false);
                PC->PlayerCameraManager->StartCameraFade(1.0f, 0.0f, SpawnFadeDuration, FLinearColor::Black, false, false);
        }




        bGameStarted = true;
        UE_LOG(LogPWGGameMode, Display, TEXT("Spawned player at world centre X=%.2f, middle Y=%.2f; surface Z=%.2f, actor Z=%.2f."),
                SpawnLocation.X, SpawnLocation.Y, SurfaceZ, SpawnLocation.Z);
        return true;
}




APawn* APWGGameMode::SpawnConfiguredPlayer(const FTransform& Transform)
{
    if (!PlayerPawnClass) return nullptr;
    APawn* Pawn = GetWorld()->SpawnActorDeferred<APawn>(PlayerPawnClass, Transform, nullptr, nullptr,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!Pawn) return nullptr;
    auto* Input = NewObject<UPWGSidescrollerInputComponent>(Pawn, TEXT("PWGSidescrollerInput"));
    Pawn->AddInstanceComponent(Input);
    Input->RegisterComponent();
    if (auto* Mover = Pawn->FindComponentByClass<UMoverComponent>())
    {
        Mover->InputProducer = Input;
        Mover->bGatherInputFromAllInputProducerComponents = false;
    }
    UGameplayStatics::FinishSpawningActor(Pawn, Transform);
    return Pawn;
}

UClass* APWGGameMode::GetDefaultPawnClassForController_Implementation(AController* Controller)
{
    if (bPOITestMode) return PlayerPawnClass.Get();
    return Super::GetDefaultPawnClassForController_Implementation(Controller);
}

APawn* APWGGameMode::SpawnDefaultPawnAtTransform_Implementation(AController* Controller, const FTransform& Transform)
{
    if (bPOITestMode) return SpawnConfiguredPlayer(Transform);
    return Super::SpawnDefaultPawnAtTransform_Implementation(Controller, Transform);
}

bool APWGGameMode::RespawnPlayer(APlayerController* PlayerController)
{
        APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
        if (bPOITestMode && PlayerController)
        {
            if (Pawn) { PlayerController->UnPossess(); Pawn->Destroy(); }
            RestartPlayer(PlayerController);
            if (APawn* Respawned = PlayerController->GetPawn())
            {
                USirrohCharacterIdentityComponent::ApplySaved(Respawned);
                return true;
            }
            return false;
        }
        if (!Pawn || !WorldGenerator || !WorldGenerator->IsWorldGenerated()) return false;
        USirrohCharacterIdentityComponent::ApplySaved(Pawn);




        FVector Target = WorldGenerator->GetWorldCenterSurfaceLocation();
        if (auto* V2=Cast<APWGv2Generator>(WorldGenerator))
        {
            V2->EnsureLocationLoaded(Target);
            if (auto* Producer=Pawn->FindComponentByClass<UPWGSidescrollerInputComponent>()) Producer->CancelLaneMove();
            if (auto* Mover=Pawn->FindComponentByClass<UMoverComponent>())
            {
                auto Constraint=Mover->GetPlanarConstraint(); Constraint.PlaneConstraintOrigin.Y=Target.Y; Mover->SetPlanarConstraint(Constraint);
            }
        }
        const UCapsuleComponent* Capsule = Pawn->FindComponentByClass<UCapsuleComponent>();
        const float BottomOffset = Capsule
                ? Capsule->GetComponentLocation().Z - Pawn->GetActorLocation().Z - Capsule->GetScaledCapsuleHalfHeight()
                : 0.0f;
        Target.Z += 3.0f - BottomOffset;




        if (UMoverComponent* Mover = Pawn->FindComponentByClass<UMoverComponent>())
        {
                TSharedPtr<FTeleportEffect> Teleport = MakeShared<FTeleportEffect>();
                Teleport->TargetLocation = Target;
                Teleport->bUseActorRotation = true;
                Mover->QueueInstantMovementEffect(Teleport);
        }
        else
        {
                Pawn->TeleportTo(Target, Pawn->GetActorRotation());
        }
        return true;
}
void APWGGameMode::ResetTrainingNPC()
{
    if (!HasAuthority() || !WorldGenerator) return;
    auto* PC=GetWorld()->GetFirstPlayerController();
    APawn* Player=PC?PC->GetPawn():nullptr;
    if (!Player) return;
    FVector Location=Player->GetActorLocation()+FVector(CombatTrainingSpawnOffset,0,0);
    const float Surface=WorldGenerator->GetSurfaceHeightAt(Location.X,Location.Y);
    if (Surface<=-BIG_NUMBER*0.5f) return;
    if (IsValid(CombatTrainingNPC)) CombatTrainingNPC->Destroy();
    UClass* NPCClass=ASTCombatNPCClass.LoadSynchronous();
    if (!NPCClass) { UE_LOG(LogTemp,Error,TEXT("PWG: integrated AST NPC class is missing"));return; }
    Location.Z=Surface+95;
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    CombatTrainingNPC=GetWorld()->SpawnActor<APawn>(NPCClass,Location,FRotator(0,180,0),Params);

}







void APWGGameMode::SpawnASTIntegrationObjects(const FVector& PlayerLocation)
{
    if (!bSpawnASTIntegrationTestObjects || !HasAuthority() || !WorldGenerator || !ASTIntegrationTestActors.IsEmpty()) return;
    UStaticMesh* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!Cube) return;
    // Test stations use the original AST actors and their own interaction children.
    // They are runtime fixtures for PWG, not a room-generation system.
    auto Box=[&](FVector Center,FVector Size)->AActor*
    {
        AActor* A=GetWorld()->SpawnActor<AActor>(AActor::StaticClass(),Center,FRotator::ZeroRotator);
        if(!A)return nullptr;
        auto* M=NewObject<UStaticMeshComponent>(A,TEXT("ASTTestGeometry"));
        A->SetRootComponent(M);A->AddInstanceComponent(M);M->SetMobility(EComponentMobility::Static);
        M->SetStaticMesh(Cube);M->SetCollisionProfileName(TEXT("BlockAll"));
        M->SetWorldLocation(Center);M->SetWorldScale3D(Size/100.f);M->RegisterComponent();
        WorldGenerator->RegisterTraversalVisual(M);
        A->Tags.Add(TEXT("PWG_AST_IntegrationTest"));ASTIntegrationTestActors.Add(A);return A;
    };
    auto Station=[&](float Offset,FVector2D Size,FVector& Floor)->bool
    {
        Floor=PlayerLocation+FVector(Offset,0,0);WorldGenerator->EnsureLocationLoaded(Floor);
        float Highest=-BIG_NUMBER;
        for(double X:{-Size.X*.5,0.,Size.X*.5})for(double Y:{-Size.Y*.5,0.,Size.Y*.5})
            Highest=FMath::Max(Highest,WorldGenerator->GetSurfaceHeightAt(Floor.X+X,Floor.Y+Y));
        if(Highest<=-BIG_NUMBER*.5f)return false;
        Floor.Z=Highest+5.f;
        return Box(Floor-FVector(0,0,10),FVector(Size.X,Size.Y,20))!=nullptr;
    };
    auto ASTActor=[&](const TCHAR* Path,FVector Position,FRotator Rotation,FVector Scale,int32 LedgeType=INDEX_NONE)->AActor*
    {
        UClass* C=LoadClass<AActor>(nullptr,Path);
        if(!C){UE_LOG(LogPWGGameMode,Error,TEXT("AST test fixture class missing: %s"),Path);return nullptr;}
        const FTransform T(Rotation,Position,Scale);
        AActor* A=GetWorld()->SpawnActorDeferred<AActor>(C,T,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if(!A)return nullptr;
        if(auto* P=FindFProperty<FNumericProperty>(C,TEXT("SplineLength")))P->SetFloatingPointPropertyValue(P->ContainerPtrToValuePtr<void>(A),100.);
        if(LedgeType!=INDEX_NONE)
            if(auto* Property=FindFProperty<FByteProperty>(C,TEXT("LedgeType")))
                Property->SetPropertyValue_InContainer(A,static_cast<uint8>(LedgeType));
        UGameplayStatics::FinishSpawningActor(A,T);
        if (LedgeType != INDEX_NONE)
        {
            // AST measures reach and end margins in spline-local centimetres.
            // Bake the length into spline points instead of scaling those distances.
            if (auto* Spline = A->FindComponentByClass<USplineComponent>())
            {
                A->SetActorScale3D(FVector::OneVector);
                Spline->SetSplinePoints({FVector::ZeroVector, FVector(100.f * Scale.X, 0, 0)},
                    ESplineCoordinateSpace::Local);
                Spline->SetSplinePointType(0, ESplinePointType::Linear, false);
                Spline->SetSplinePointType(1, ESplinePointType::Linear, true);
                Spline->bSplineHasBeenEdited = true;
                A->RerunConstructionScripts();
            }
        }
        A->Tags.Add(TEXT("PWG_AST_IntegrationTest"));ASTIntegrationTestActors.Add(A);return A;
    };
    const TCHAR* LedgeClass=TEXT("/AST_ParkourSystem/Blueprints/Actor/ParkourActor.ParkourActor_C");
    auto Ledge=[&](FVector Edge,float Width,bool bFront,uint8 Type)
    {
        const float Sign=bFront?1.f:-1.f;
        return ASTActor(LedgeClass,Edge+FVector(0,Sign*Width*.5f,0),FRotator(0,-Sign*90.f,0),FVector(Width/100.f,1,1),Type);
    };
    FVector Floor;
    // Connected side and front ledges expose corners, shimmy and separated hop targets.
    if (Station(-1550, FVector2D(2900, 900), Floor))
    {
        for (int32 Index = 0; Index < 3; ++Index)
        {
            const float Height = 220.f + Index * 60.f;
            const FVector Base = Floor + FVector(800.f - Index * 680.f, 0, 0);
            Box(Base + FVector(0, 0, Height * .5f), FVector(600, 350, Height));
            Ledge(Base + FVector(300, 0, Height), 350, true, 0);
            Ledge(Base + FVector(-300, 0, Height), 350, false, 0);
            // The visible long face connects both corners. Neighbours are 80 cm apart.
            ASTActor(LedgeClass, Base + FVector(-300, 175, Height),
                FRotator::ZeroRotator, FVector(6, 1, 1), 0);
            ASTActor(LedgeClass, Base + FVector(300, -175, Height),
                FRotator(0, 180, 0), FVector(6, 1, 1), 0);
        }
    }
    if(Station(-3700,FVector2D(1000,700),Floor))
    {
        Box(Floor+FVector(-100,0,65),FVector(180,350,130));
        Box(Floor+FVector(-110,0,195),FVector(160,350,130));
        Ledge(Floor+FVector(-10,0,130),350,true,0);
        Ledge(Floor+FVector(-30,0,260),350,true,0);
    }
    if(Station(-5200,FVector2D(1700,700),Floor))
    {
        // Low hurdle, mantle, and higher ledge: AST traces see real blocking geometry.
        for(int32 I=0;I<3;++I)
        {
            const float Height=70.f+55.f*I;
            const FVector Base=Floor+FVector(450.f-I*450.f,0,0);
            Box(Base+FVector(0,0,Height*.5f),FVector(120,350,Height));
            const uint8 Type=I==0?1:2; // E_LedgeType: Ledge, Vault, Mantle.
            Ledge(Base+FVector(60,0,Height),350,true,Type);
            Ledge(Base+FVector(-60,0,Height),350,false,Type);
        }
    }
}
