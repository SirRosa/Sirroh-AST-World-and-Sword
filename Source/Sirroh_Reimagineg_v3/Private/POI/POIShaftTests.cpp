#include "POI/POIShaft.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SplineComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/AutomationTest.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPOIShaftKitTest,"Sirroh.POI.ShaftKit",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPOIShaftKitTest::RunTest(const FString& Parameters)
{
    TestFalse(TEXT("Warped landing restores one-way support within floor tolerance"),APOIKitBlock::ShouldPassFromBelow(147.6f,150.f,true));
    TestTrue(TEXT("Hanging below one-way remains passable"),APOIKitBlock::ShouldPassFromBelow(60.f,150.f,true));
    const UWorld::InitializationValues Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    World->InitializeActorsForPlay(FURL()); World->GetWorldSettings()->NotifyBeginPlay();
    auto* Block=World->SpawnActor<APOIKitBlock>(); Block->Settings.Kind=EPOIKitKind::Both; Block->RefreshBlock();
    TestNotNull(TEXT("Original GASP Blueprint instantiated"),Block->GetGASPActor());
    TestNotNull(TEXT("Original AST Blueprint instantiated"),Block->GetASTActor());
    FHitResult Hit;
    TestTrue(TEXT("GASP channel finds a block"),World->LineTraceSingleByChannel(Hit,FVector(-300,0,-50),FVector(300,0,-50),ECC_GameTraceChannel1));
    TestEqual(TEXT("GASP cast sees original child, not wrapper"),Hit.GetActor(),Block->GetGASPActor());
    if (auto* G=Block->GetGASPActor())
    {
        TArray<USplineComponent*> Splines; G->GetComponents(Splines); TestEqual(TEXT("Four GASP ledges"),Splines.Num(),4);
        for (auto* S:Splines) for (int32 I=0;I<S->GetNumberOfSplinePoints();++I)
        { const auto P=S->GetLocationAtSplinePoint(I,ESplineCoordinateSpace::World); TestTrue(TEXT("Ledge aligned to wrapper top"),FMath::Abs(P.Z)<1); TestTrue(TEXT("Ledge within body width"),FMath::Abs(P.X)<=121 && FMath::Abs(P.Y)<=51); }
    }
    FCollisionObjectQueryParams ASTObjects; ASTObjects.AddObjectTypesToQuery(ECC_GameTraceChannel4);
    TestTrue(TEXT("AST object trace finds ledge"),World->LineTraceSingleByObjectType(Hit,FVector(80,0,.5),FVector(160,0,.5),ASTObjects));
    TestEqual(TEXT("AST trace owns original ParkourActor"),Hit.GetActor(),Block->GetASTActor());
    Block->Settings.Collision=EPOIKitCollision::NoPhysicalCollision; Block->RefreshBlock();
    TestFalse(TEXT("No physical collision does not block visibility sweep"),World->LineTraceSingleByChannel(Hit,FVector(-300,0,-50),FVector(300,0,-50),ECC_Visibility));
    TestTrue(TEXT("GASP detection survives no physical collision"),World->LineTraceSingleByChannel(Hit,FVector(-300,0,-50),FVector(300,0,-50),ECC_GameTraceChannel1));
    // Exercise AST's actual directional gate, not just the query collision.
    for (bool Positive : {true, false})
    {
        Block->Settings.bFacePositiveX=Positive; Block->RefreshBlock();
        AActor* Edge=Block->GetASTActor();
        UFunction* Gate=Edge?Edge->FindFunction(TEXT("CheckLedgeDirection")):nullptr;
        TestNotNull(TEXT("AST direction interface exists"),Gate);
        if (!Gate) continue;
        FStructOnScope Args(Gate);
        auto* Impact=FindFProperty<FStructProperty>(Gate,TEXT("ImpactPoint"));
        auto* Forward=FindFProperty<FStructProperty>(Gate,TEXT("TraceForwardDirection"));
        auto* Accepted=FindFProperty<FBoolProperty>(Gate,TEXT("ReturnValue"));
        if (!Impact || !Forward || !Accepted) { AddError(TEXT("AST direction interface changed")); continue; }
        const float Side=Positive?1.f:-1.f;
        *Impact->ContainerPtrToValuePtr<FVector>(Args.GetStructMemory())=FVector(Side*120,0,0);
        *Forward->ContainerPtrToValuePtr<FVector>(Args.GetStructMemory())=FVector(-Side,0,0);
        Edge->ProcessEvent(Gate,Args.GetStructMemory());
        TestTrue(TEXT("AST accepts approach from outside platform without physical collision"),Accepted->GetPropertyValue_InContainer(Args.GetStructMemory()));
        *Forward->ContainerPtrToValuePtr<FVector>(Args.GetStructMemory())=FVector(Side,0,0);
        Edge->ProcessEvent(Gate,Args.GetStructMemory());
        TestFalse(TEXT("AST rejects approach from behind ledge"),Accepted->GetPropertyValue_InContainer(Args.GetStructMemory()));
    }
    Block->Settings.bBothASTSides=true;
    Block->Settings.Image=TSoftObjectPtr<UTexture2D>(LoadObject<UTexture2D>(nullptr,TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture")));
    Block->SetActorScale3D(FVector(1.1,1.3,2.4)); Block->RefreshBlock();
    TArray<UStaticMeshComponent*> Faces;Block->GetComponents(Faces);
    for (auto* Face:Faces) if (Face->GetFName()==TEXT("ImageOrBlock"))
        TestTrue(TEXT("Vertical card fills scaled body height"),FMath::IsNearlyEqual(Face->Bounds.BoxExtent.Z,Block->Settings.Size.Z*2.4*.5,1.));
    TArray<UChildActorComponent*> Adapters;Block->GetComponents(Adapters);
    int32 ASTCount=0;
    for (auto* Adapter:Adapters)
    {
        AActor* Edge=Adapter->GetChildActor();
        UFunction* Gate=Edge?Edge->FindFunction(TEXT("CheckLedgeDirection")):nullptr;
        if (!Gate) continue;
        ++ASTCount;
        auto* Spline=Edge->FindComponentByClass<USplineComponent>();
        if (!Spline) {AddError(TEXT("AST perimeter missing original spline"));continue;}
        const FVector Point=Spline->GetLocationAtDistanceAlongSpline(Spline->GetSplineLength()*.5,ESplineCoordinateSpace::World);
        const FVector Inward=-Spline->GetRotationAtDistanceAlongSpline(Spline->GetSplineLength()*.5,ESplineCoordinateSpace::World).RotateVector(FVector::YAxisVector);
        FStructOnScope Args(Gate);
        auto* Impact=FindFProperty<FStructProperty>(Gate,TEXT("ImpactPoint"));
        auto* Forward=FindFProperty<FStructProperty>(Gate,TEXT("TraceForwardDirection"));
        auto* Accepted=FindFProperty<FBoolProperty>(Gate,TEXT("ReturnValue"));
        if (!Impact || !Forward || !Accepted) {AddError(TEXT("AST direction interface changed"));continue;}
        *Impact->ContainerPtrToValuePtr<FVector>(Args.GetStructMemory())=Point;
        *Forward->ContainerPtrToValuePtr<FVector>(Args.GetStructMemory())=Inward;
        Edge->ProcessEvent(Gate,Args.GetStructMemory());
        TestTrue(TEXT("Every perimeter edge accepts its outward approach"),Accepted->GetPropertyValue_InContainer(Args.GetStructMemory()));
    }
    TestEqual(TEXT("Front and two sides, no rear AST grip"),ASTCount,3);
    Block->SetActorScale3D(FVector::OneVector);
    Block->Settings.Kind=EPOIKitKind::SideWall; Block->Settings.Collision=EPOIKitCollision::OneWay; Block->RefreshBlock();
    auto* Pawn=World->SpawnActor<ACharacter>(FVector(0,0,-300),FRotator::ZeroRotator);
    Pawn->GetCapsuleComponent()->SetGenerateOverlapEvents(true); Pawn->GetCapsuleComponent()->UpdateOverlaps();
    static_cast<AActor*>(Block)->Tick(.016f);
    TestTrue(TEXT("Capsule below ignores only platform body"),Pawn->GetCapsuleComponent()->GetMoveIgnoreComponents().Contains(Block->GetPhysicalBody()));
    Pawn->SetActorLocation(FVector(0,0,250),true,&Hit);
    TestFalse(TEXT("Upward sweep passes through"),Hit.bBlockingHit);
    static_cast<AActor*>(Block)->Tick(.016f);
    TestFalse(TEXT("Capsule above reenables support"),Pawn->GetCapsuleComponent()->GetMoveIgnoreComponents().Contains(Block->GetPhysicalBody()));
    Pawn->SetActorLocation(FVector(0,0,-300),true,&Hit);
    TestTrue(TEXT("Downward sweep lands on platform"),Hit.bBlockingHit);
    Pawn->Destroy(); Block->Destroy();
    FString Error; FPOIShaftRow Row;
    for (int32 Layer=0;Layer<4;++Layer) { Row.UpperLayer=Layer; TestTrue(TEXT("All four transitions accepted"),Row.Validate(Error)); }
    Row.UpperLayer=4; TestFalse(TEXT("No nonexistent sixth layer"),Row.Validate(Error));
    auto* Shaft=World->SpawnActor<APOIShaft>(); Shaft->Recipe.Climbing=EPOIKitKind::GASP; Shaft->Height=450; Shaft->BuildShaft();
    TestEqual(TEXT("Three climb blocks + three boundaries"),Shaft->GeneratedBlocks.Num(),6);
    Shaft->BuildShaft(); TestEqual(TEXT("Rebuild does not duplicate"),Shaft->GeneratedBlocks.Num(),6);
    Row=FPOIShaftRow(); Row.bUseCustomSection=true;
    FPOIShaftPiece P; P.Block.Kind=EPOIKitKind::GASP; P.Transform.SetTranslation(FVector(-150,0,150)); Row.Pieces.Add(P);
    P.Transform.SetTranslation(FVector(150,0,300)); Row.Pieces.Add(P);
    TestTrue(TEXT("Valid authored section"),Row.Validate(Error));
    Shaft->Recipe=Row; Shaft->Height=750; Shaft->BuildShaft();
    TestEqual(TEXT("Two sections + remainder step + boundaries"),Shaft->GeneratedBlocks.Num(),8);
    Row.Pieces[1].Transform.SetTranslation(FVector(150,0,250)); TestFalse(TEXT("Missing section-top connection rejected"),Row.Validate(Error));
    Row.Pieces[1].Transform.SetTranslation(FVector(150,0,300));
    FPOIShaftNativePiece Beam;
    Beam.ActorClass=TSoftClassPtr<AActor>(FSoftObjectPath(TEXT("/AST_ParkourSystem/Blueprints/Actor/BeamActor.BeamActor_C")));
    Beam.Transform.SetTranslation(FVector(-100,0,100));
    Beam.Points.Add(FSplinePoint(0,FVector::ZeroVector));
    Beam.Points.Add(FSplinePoint(1,FVector(200,0,0)));
    Row.NativePieces.Add(Beam);
    Shaft->Recipe=Row; Shaft->Height=300; Shaft->BuildShaft();
    TestEqual(TEXT("Native beam instantiated"),Shaft->GeneratedNativeActors.Num(),1);
    if (!Shaft->GeneratedNativeActors.IsEmpty())
    {
        auto* Spline=Shaft->GeneratedNativeActors[0]->FindComponentByClass<USplineComponent>();
        TestNotNull(TEXT("Native beam retains spline"),Spline);
        if (Spline) TestTrue(TEXT("Authored beam length survives reconstruction"),FMath::IsNearlyEqual(Spline->GetSplineLength(),200.f,1.f));
        TArray<UPrimitiveComponent*> Components;
        Shaft->GeneratedNativeActors[0]->GetComponents(Components);
        bool HasBeamFloor=false;
        for (auto* Component:Components) HasBeamFloor|=Component->ComponentHasTag(TEXT("Beam"));
        TestTrue(TEXT("Original Beam tag survives for balance detection"),HasBeamFloor);
    }
    Row.NativePieces.Reset();
    Shaft->Recipe=FPOIShaftRow(); Shaft->Height=1500; Shaft->bAssembleModules=true;
    for (auto SectionRole:{EPOIShaftRole::Exit,EPOIShaftRole::Middle,EPOIShaftRole::Entrance})
    { Row.Role=SectionRole; Shaft->SavedModules.Add(Row); }
    Shaft->BuildShaft();
    TestEqual(TEXT("Bottom, three middles, top"),Shaft->BuiltRoles.Num(),5);
    if (Shaft->BuiltRoles.Num()==5)
    {
        TestTrue(TEXT("Bottom at lower end"),Shaft->BuiltRoles[0]==EPOIShaftRole::Exit);
        TestTrue(TEXT("Entrance at upper end"),Shaft->BuiltRoles.Last()==EPOIShaftRole::Entrance);
    }
    Shaft->BuildShaft();
    TestEqual(TEXT("Rebuild clears previous native actors"),Shaft->GeneratedNativeActors.Num(),0);
    Shaft->Destroy(); World->DestroyWorld(false);
    return true;
}
#endif
