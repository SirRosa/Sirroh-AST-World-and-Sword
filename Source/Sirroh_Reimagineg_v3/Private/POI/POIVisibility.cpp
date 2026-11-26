#include "POI/POIVisibility.h"
#include "ProceduralMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInterface.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarPOISight(TEXT("poi.Sight"),1,TEXT("Side-scroller sight occlusion: 0 off, 1 on."));
APOIVisibility::APOIVisibility()
{
    PrimaryActorTick.bCanEverTick=true; PrimaryActorTick.TickInterval=.1f;
    Fog=CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("SightCurtain")); RootComponent=Fog;
    Fog->SetCollisionEnabled(ECollisionEnabled::NoCollision); Fog->SetCastShadow(false);
    Fog->SetCanEverAffectNavigation(false);
}
void APOIVisibility::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    auto* PC=UGameplayStatics::GetPlayerController(this,0);
    APawn* Player=PC?PC->GetPawn():nullptr;
    if (!Player || !CVarPOISight.GetValueOnGameThread()) { SetActorHiddenInGame(true); return; }
    if (!Fog->GetMaterial(0)) Fog->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/POIKit/M_POISight.M_POISight")));
    FVector Camera; FRotator Rotation; PC->GetPlayerViewPoint(Camera,Rotation);
    // Free debug cameras are diagnostic views, not the player's side-scroller view.
    if (FMath::Abs(Rotation.Vector().Y)<.95f) { SetActorHiddenInGame(true); return; }
    SetActorHiddenInGame(false);
    FVector Eye=Player->GetActorLocation()+FVector(0,0,35);
    SetActorLocation(FVector(Eye.X,Camera.Y+FMath::Sign(Eye.Y-Camera.Y)*100,Eye.Z));
    FCollisionQueryParams Query(SCENE_QUERY_STAT(POISight),false,Player);
    Query.AddIgnoredActor(this);
    TArray<AActor*> Attached; Player->GetAttachedActors(Attached,true,true);
    Query.AddIgnoredActors(Attached);
    constexpr int32 Rays=192;
    TArray<FVector> Vertices,Normals; TArray<int32> Triangles; TArray<FVector2D> UV;
    TArray<FLinearColor> Colors;
    for (int32 I=0;I<=Rays;++I)
    {
        const float Angle=2*PI*I/Rays;
        const FVector Direction(FMath::Cos(Angle),0,FMath::Sin(Angle));
        FHitResult Hit;
        const bool Blocked=GetWorld()->LineTraceSingleByChannel(Hit,Eye,Eye+Direction*3500,ECC_Visibility,Query);
        // A small depth margin shows the surface of the occluder itself.
        const float Distance=Blocked?FMath::Max(35.f,Hit.Distance+12.f):3500.f;
        Vertices.Add(Direction*Distance); Vertices.Add(Direction*100000);
        Normals.Add(FVector(0,-1,0)); Normals.Add(FVector(0,-1,0));
        UV.Add(FVector2D::ZeroVector); UV.Add(FVector2D::ZeroVector);
        Colors.Add(FLinearColor::White); Colors.Add(FLinearColor::White);
        if (I<Rays) { const int32 A=I*2; Triangles.Append({A,A+1,A+3,A,A+3,A+2}); }
    }
    if (!Fog->GetProcMeshSection(0))
        Fog->CreateMeshSection_LinearColor(0,Vertices,Triangles,Normals,UV,Colors,TArray<FProcMeshTangent>(),false);
    else
        Fog->UpdateMeshSection_LinearColor(0,Vertices,Normals,UV,Colors,TArray<FProcMeshTangent>());
}
