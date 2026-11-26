#include "Placeholder/PlaceholderBerry.h"
#include "Placeholder/PlaceholderBerryWorld.h"
#include "Components/SphereComponent.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "Math/RotationMatrix.h"
#include "Sword/SwordWorldSubsystem.h"
#include "Engine/Texture2D.h"
#include "Net/UnrealNetwork.h"

APlaceholderBerry::APlaceholderBerry()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    Collision = CreateDefaultSubobject<USphereComponent>(TEXT("BerryCollision"));
    SetRootComponent(Collision);
    Collision->InitSphereRadius(8.0f);
    Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Collision->SetCollisionObjectType(ECC_WorldDynamic);
    Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
    Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
    Collision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
    Visual = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("BerrySprite"));
    Visual->SetupAttachment(Collision);
    Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Visual->SetCastShadow(false);
}

void APlaceholderBerry::BeginPlay()
{
    Super::BeginPlay();
    OnRep_Appearance();
    GetWorld()->GetSubsystem<UPlaceholderBerryWorld>()->Berries.Add(this);
}
void APlaceholderBerry::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);DOREPLIFETIME(APlaceholderBerry,AtlasFrame);DOREPLIFETIME(APlaceholderBerry,Size);
}
void APlaceholderBerry::OnRep_Appearance()
{
    IconTexture=LoadObject<UTexture2D>(nullptr,TEXT("/Game/Systems/Inventory/Icons/Berry_Atlas.Berry_Atlas"));
    // Apply the enlargement here so existing serialized generator settings also grow by 2x.
    const float S = Size * 2.0f * 0.65f;
    Collision->SetSphereRadius(8.0f*FMath::Max(1.0f,Size)/14.0f);
    TArray<FVector> V = { {-S,0,-S},{S,0,-S},{S,0,S},{-S,0,S} };
    TArray<int32> T = {0,1,2,0,2,3};
    TArray<FVector> N; N.Init(FVector(0,-1,0),4);
    TArray<FVector2D> UV = {{0,1},{1,1},{1,0},{0,0}};
    TArray<FColor> C; C.Init(FColor::White,4);
    TArray<FProcMeshTangent> Tangents; Tangents.Init(FProcMeshTangent(1,0,0),4);
    Visual->CreateMeshSection(0,V,T,N,UV,C,Tangents,false);
    UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/Systems/Inventory/Placeholder/M_PlaceholderBerry.M_PlaceholderBerry"));
    if (Base)
    {
        Material = UMaterialInstanceDynamic::Create(Base,this);
        Material->SetScalarParameterValue(TEXT("AtlasColumn"),FMath::Clamp(AtlasFrame,0,63)%8);
        Material->SetScalarParameterValue(TEXT("AtlasRow"),FMath::Clamp(AtlasFrame,0,63)/8);
        Visual->SetMaterial(0,Material);
    }
    else UE_LOG(LogTemp, Error, TEXT("Placeholder berry material missing: run placeholder_setup.py."));
}

void APlaceholderBerry::EndPlay(const EEndPlayReason::Type Reason)
{
    if (UPlaceholderBerryWorld* State = GetWorld()->GetSubsystem<UPlaceholderBerryWorld>())
        State->Berries.RemoveAll([this](const TWeakObjectPtr<APlaceholderBerry>& B) { return !B.IsValid() || B.Get()==this; });
    Super::EndPlay(Reason);
}

bool APlaceholderBerry::CanPickUp() const { return !bHeld && !bFlying && PickupDelay <= 0.0f; }
void APlaceholderBerry::UpdateInventoryBrush(FSlateBrush& Brush) const
{
    Brush.SetResourceObject(IconTexture);
    const int32 Frame=FMath::Clamp(AtlasFrame,0,63);
    const FVector2f UV(Frame%8/8.0f,Frame/8/8.0f);
    Brush.SetUVRegion(FBox2f(UV,UV+FVector2f(0.125f,0.125f)));
}
void APlaceholderBerry::SetHighlighted(bool bEnabled)
{
    if (Material) Material->SetScalarParameterValue(TEXT("Highlight"),bEnabled?1.0f:0.0f);
}
void APlaceholderBerry::Take(AActor* Holder)
{
    if (!CanPickUpBy(Holder)) return;
    ReleasePickup(Holder);
    MarkDisplaced();
    if (!PersistentId.IsEmpty()) GetWorld()->GetSubsystem<UPlaceholderBerryWorld>()->Collected.Add(PersistentId);
    bHeld=true;
    bFlying=false;
    SetOwner(Holder); // Chunk eviction must not destroy a berry in a hand.
    SetHighlighted(false);
    SetActorTickEnabled(false);
    Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
void APlaceholderBerry::SetHeldLocation(const FVector& Location)
{
    SetActorLocation(Location);
}
void APlaceholderBerry::Throw(const FVector& Origin, const FVector& InitialVelocity, AActor* Thrower)
{
    DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    MarkDisplaced();
    if (!PersistentId.IsEmpty()) GetWorld()->GetSubsystem<UPlaceholderBerryWorld>()->Collected.Add(PersistentId);
    SetHighlighted(false);
    HitFruit.Reset();
    if (ASirrohWorldItem* Source=Cast<ASirrohWorldItem>(Thrower)) HitFruit.Add(Source);
    bHeld=false;
    bFlying=true;
    SetOwner(nullptr);
    SetActorLocation(Origin);
    LaneY=Origin.Y;
    Velocity=InitialVelocity;
    Velocity.Y=0.0;
    FlightTime=0.0f;
    PickupDelay=0.25f;
    IgnoredThrower=Thrower;
    SetActorTickEnabled(true);
}
void APlaceholderBerry::KnockDown(const FVector& Impulse,AActor* Source)
{
    if (bHeld) return;
    FVector Origin=GetActorLocation();
    Origin.Y=LaneY; // The picture can sit in front of its crown; physics belongs to the terrain lane.
    Throw(Origin,Impulse,Source);
}
void APlaceholderBerry::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if(GetIsReplicated() && !HasAuthority())return;
    PickupDelay=FMath::Max(0.0f,PickupDelay-DeltaSeconds);
    if (!bFlying) { if (PickupDelay<=0) SetActorTickEnabled(false); return; }
    FlightTime+=DeltaSeconds;
    float Remaining=FMath::Min(DeltaSeconds,0.25f);
    // The sidescroller uses an orthographic camera: dropping camera-depth gives screen-space
    // overlap in world units, without deprojecting pixels or tracing separately through each lane.
    FVector ViewRight=FVector::XAxisVector, ViewUp=FVector::ZAxisVector;
    if (const APlayerCameraManager* Camera=UGameplayStatics::GetPlayerCameraManager(this,0))
    {
        const FRotationMatrix ViewRotation(Camera->GetCameraRotation());
        ViewRight=ViewRotation.GetUnitAxis(EAxis::Y);
        ViewUp=ViewRotation.GetUnitAxis(EAxis::Z);
    }
    auto Project=[&](const FVector& P) { return FVector2D(FVector::DotProduct(P,ViewRight),FVector::DotProduct(P,ViewUp)); };
    const FVector2D FrameStart=Project(GetActorLocation());
    const float TravelBound=Velocity.Size()*Remaining+490.0f*Remaining*Remaining;
    const float OwnRadius=GetFruitHitRadius();
    // One cheap broad-phase pass per flying fruit per frame; the 120 Hz substeps only
    // examine nearby candidates. Stationary fruit performs no collision scans.
    TArray<TWeakObjectPtr<ASirrohWorldItem>,TInlineAllocator<32>> Candidates;
    for (const auto& Entry:GetWorld()->GetSubsystem<USwordWorldSubsystem>()->Items)
    {
        const ASirrohWorldItem* Other=Entry.Get();
        if (!Other || Other==this || Other->IsHeld() || HitFruit.Contains(Entry)) continue;
        const FVector2D Distance=Project(Other->GetActorLocation())-FrameStart;
        const float Extent=TravelBound+OwnRadius+Other->GetItemHitRadius();
        if (FMath::Abs(Distance.X)<=Extent && FMath::Abs(Distance.Y)<=Extent) Candidates.Add(Entry);
    }
    // Swept substeps prevent tiny sprites tunnelling through the terrain.
    while (Remaining>SMALL_NUMBER && bFlying)
    {
        const float Dt=FMath::Min(Remaining,1.0f/120.0f);
        Remaining-=Dt;
        const FVector Start=GetActorLocation();
        const FVector End=Start+Velocity*Dt+FVector(0,0,-490.0f*Dt*Dt);
        FCollisionQueryParams Params(SCENE_QUERY_STAT(PlaceholderBerryFlight),false,this);
        if (IgnoredThrower.IsValid()) Params.AddIgnoredActor(IgnoredThrower.Get());
        FCollisionObjectQueryParams Objects;
        Objects.AddObjectTypesToQuery(ECC_WorldStatic);
        Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
        FHitResult Hit;
        const bool bTerrainHit=GetWorld()->SweepSingleByObjectType(Hit,Start,End,FQuat::Identity,Objects,
            FCollisionShape::MakeSphere(Collision->GetScaledSphereRadius()),Params);
        // Swept circle intersection in the camera plane includes fruit from all three lanes.
        ASirrohWorldItem* FruitHit=nullptr;
        double FruitTime=bTerrainHit?Hit.Time:1.0;
        const FVector2D ProjectedStart=Project(Start);
        const FVector2D Segment=Project(End)-ProjectedStart;
        const double A=Segment.SizeSquared();
        if (A>SMALL_NUMBER)
        for (const auto& Entry:Candidates)
        {
            ASirrohWorldItem* Other=Entry.Get();
            if (!Other || Other->IsHeld() || HitFruit.Contains(Entry)) continue;
            const FVector2D Offset=ProjectedStart-Project(Other->GetActorLocation());
            const double Radius=OwnRadius+Other->GetItemHitRadius();
            const double B=FVector2D::DotProduct(Offset,Segment);
            const double C=Offset.SizeSquared()-Radius*Radius;
            const double Discriminant=B*B-A*C;
            if (Discriminant<0) continue;
            const double T=C<=0?0.0:(-B-FMath::Sqrt(Discriminant))/A;
            if (T>=0 && T<=FruitTime) { FruitTime=T; FruitHit=Other; }
        }
        if (FruitHit)
        {
            HitFruit.Add(FruitHit);
            FruitHit->ReceiveProjectileImpact(Velocity*0.18f+FVector(0,0,-60),this);
            SetActorLocation(FMath::Lerp(Start,End,FruitTime));
            Velocity*=0.65f;
            Velocity.Z-=980.0f*Dt;
            continue;
        }
        if (bTerrainHit)
        {
            SetActorLocation(Hit.Location+Hit.Normal*0.5f);
            bFlying=false;
            Velocity=FVector::ZeroVector;
            PickupDelay=0.15f;
        }
        else
        {
            SetActorLocation(End);
            Velocity.Z-=980.0f*Dt;
        }
    }
    if (FlightTime>20.0f || GetActorLocation().Z < -100000.0f) Destroy();
}
