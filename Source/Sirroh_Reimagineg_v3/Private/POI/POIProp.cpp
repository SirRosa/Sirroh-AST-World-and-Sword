#include "POI/POIProp.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture2D.h"

APOIProp::APOIProp()
{
    PrimaryActorTick.bCanEverTick=false;
    Body=CreateDefaultSubobject<UBoxComponent>(TEXT("Body")); RootComponent=Body;
    Body->SetMobility(EComponentMobility::Movable);
    Visual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual")); Visual->SetupAttachment(Body); Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Light=CreateDefaultSubobject<UPointLightComponent>(TEXT("Light")); Light->SetupAttachment(Body); Light->SetVisibility(false); Light->SetCastShadows(false);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube")); VisualMesh=Cube.Object;
}

void APOIProp::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    Body->SetBoxExtent(CollisionHalfExtent.GetAbs().ComponentMax(FVector(1)));
    Body->SetCollisionProfileName(CollisionMode==EPOIPropCollision::Decoration?TEXT("NoCollision"):TEXT("BlockAllDynamic"));
    Visual->SetStaticMesh(VisualMesh); Visual->SetMaterial(0,VisualMaterial); Visual->SetRelativeTransform(VisualTransform);
    if (ImageTexture && VisualMaterial)
        if (auto* Material=Visual->CreateDynamicMaterialInstance(0,VisualMaterial)) Material->SetTextureParameterValue(TEXT("ImageTexture"),ImageTexture);
    Light->SetRelativeLocation(LightOffset); Light->SetLightColor(LightColor); Light->SetIntensity(FMath::Max(0.f,LightIntensity));
    Light->SetAttenuationRadius(FMath::Max(1.f,LightRadius)); Light->SetCastShadows(bCastLightShadows); SetLightEnabled(bLightEnabled);
}

void APOIProp::BeginPlay()
{
    Super::BeginPlay();
    if (CollisionMode==EPOIPropCollision::Physics) { Body->SetSimulatePhysics(true); Body->SetMassOverrideInKg(NAME_None,FMath::Max(0.1f,MassKg)); }
}

void APOIProp::SetLightEnabled(bool Enabled)
{
    bLightEnabled=Enabled; Light->SetVisibility(Enabled);
}
