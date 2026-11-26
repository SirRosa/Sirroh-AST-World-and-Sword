#include "Sword/ProceduralSword.h"
#include "Sword/SwordWorldSubsystem.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "Math/RotationMatrix.h"
#include "Net/UnrealNetwork.h"
#include "Components/BoxComponent.h"

AProceduralSword::AProceduralSword()
{
    PrimaryActorTick.bCanEverTick=true;
    PrimaryActorTick.bStartWithTickEnabled=false;
    Visual=CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("SwordCard"));
    SetRootComponent(Visual);
    Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Visual->SetCastShadow(false);
    CombatPhysicsProxy=CreateDefaultSubobject<UBoxComponent>(TEXT("CombatPhysicsProxy"));
    CombatPhysicsProxy->SetupAttachment(Visual);
    CombatPhysicsProxy->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    CombatPhysicsProxy->SetCollisionResponseToAllChannels(ECR_Ignore);
    CombatPhysicsProxy->SetGenerateOverlapEvents(false);
    CombatPhysicsProxy->SetCanEverAffectNavigation(false);
    CombatPhysicsProxy->SetHiddenInGame(true);
    RefreshCombatProxy();
}

FCombatWeaponDefinition AProceduralSword::GetCombatDefinition() const
{
    return BuildCombatDefinition(Definition,WorldUnitsPerPixel);
}

FCombatWeaponDefinition AProceduralSword::BuildCombatDefinition(const FSwordInstance& Instance, float PixelScale)
{
    FCombatWeaponDefinition Result;
    Result.WeaponInstanceId=Instance.InstanceId;
    const auto Stat=[](float V) { return FMath::IsFinite(V)?FMath::Clamp(V,0.1f,50.0f):1.0f; };
    Result.WeaponWeight=FMath::IsFinite(Instance.WeaponWeight) && Instance.WeaponWeight>0?Stat(Instance.WeaponWeight):1.0f; // kg fallback for absent/invalid legacy data
    Result.WeaponPower=Stat(Instance.WeaponPower);
    Result.WeaponAngle=Stat(Instance.WeaponAngle);
    const float Scale=FMath::IsFinite(PixelScale)?FMath::Max(0.05f,PixelScale):0.30f;
    // Same authored card dimensions as BeginPlay/legacy blade contact; not the alpha silhouette.
    Result.BladeBaseLocal=FVector(0,0,16*Scale);
    Result.BladeTipLocal=FVector(0,0,-368*Scale);
    Result.WeaponReach=FVector::Distance(Result.PrimaryGripLocal.GetLocation(),Result.BladeTipLocal);
    // Make the logical grip +X point along the blade while keeping anchors actor-local.
    Result.PrimaryGripLocal.SetRotation(FRotationMatrix::MakeFromXY(-FVector::ZAxisVector,FVector::YAxisVector).ToQuat());
    // Image-derived handle anchors for the same atlas used by the card compositor.
    // Recipe indices, not style/animation selection, determine the contact geometry.
    static constexpr float HiltSamples[64][3]={
#include "SwordHiltGripSamples.inl"
    };
    const auto& Hilt=HiltSamples[FMath::Clamp(Instance.Hilt.Index(),0,63)];
    Result.SecondaryGripLocal=Result.PrimaryGripLocal;
    Result.SecondaryGripLocal.SetLocation(FVector((Hilt[1]-64)*Scale,0,(64-Hilt[2])*Scale));
    Result.bHasSecondaryGrip=true;
    Result.GeometryVersion=2;
    Result.HandleLength=(115-Hilt[0])*Scale; // crossguard centre in the image compositor
    return Result;
}

void AProceduralSword::RefreshCombatProxy()
{
    if (!CombatPhysicsProxy) return;
    const FCombatWeaponDefinition Geometry=GetCombatDefinition();
    CombatPhysicsProxy->SetRelativeLocation((Geometry.BladeBaseLocal+Geometry.BladeTipLocal)*0.5);
    CombatPhysicsProxy->SetBoxExtent(FVector(Geometry.BladeRadius,Geometry.BladeRadius,
        FVector::Distance(Geometry.BladeBaseLocal,Geometry.BladeTipLocal)*0.5+Geometry.BladeRadius),false);
}

void AProceduralSword::OnRep_CombatScale()
{
    RefreshCombatProxy();
}
void AProceduralSword::ApplyRecipe(UMaterialInstanceDynamic* MID)
{
    if (!MID) return;
    MID->SetScalarParameterValue(TEXT("BladePart"),Definition.Blade.Index());
    MID->SetScalarParameterValue(TEXT("GuardPart"),Definition.Guard.Index());
    MID->SetScalarParameterValue(TEXT("HiltPart"),Definition.Hilt.Index());
    MID->SetScalarParameterValue(TEXT("BladeVariant"),FMath::Clamp(Definition.BladeVariant,0,2));
}
void AProceduralSword::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AProceduralSword,Definition);
    DOREPLIFETIME(AProceduralSword,WorldUnitsPerPixel);
}
void AProceduralSword::OnRep_Definition()
{
    ApplyRecipe(Material); ApplyRecipe(InventoryMaterial);
    RefreshCombatProxy();
}
void AProceduralSword::BeginPlay()
{
    Super::BeginPlay();
    RefreshCombatProxy();
    // Coordinates in the composed 128x432 image: grip=(64,64), blade top=(64,48).
    const float W=64*WorldUnitsPerPixel,Top=64*WorldUnitsPerPixel,Bottom=-368*WorldUnitsPerPixel;
    TArray<FVector> V={{-W,0,Bottom},{W,0,Bottom},{W,0,Top},{-W,0,Top}};
    TArray<int32> T={0,1,2,0,2,3};
    TArray<FVector> N; N.Init(FVector(0,-1,0),4);
    TArray<FVector2D> UV={{0,1},{1,1},{1,0},{0,0}};
    TArray<FColor> C; C.Init(FColor::White,4);
    TArray<FProcMeshTangent> Tangents; Tangents.Init(FProcMeshTangent(1,0,0),4);
    Visual->CreateMeshSection(0,V,T,N,UV,C,Tangents,false);
    auto* State=GetWorld()->GetSubsystem<USwordWorldSubsystem>();
    if (auto* Base=State->WorldMaterial())
    { Material=UMaterialInstanceDynamic::Create(Base,this); ApplyRecipe(Material); Visual->SetMaterial(0,Material); }
    if (auto* Base=State->IconMaterial())
    { InventoryMaterial=UMaterialInstanceDynamic::Create(Base,this); ApplyRecipe(InventoryMaterial); }
}
void AProceduralSword::SetHighlighted(bool bEnabled)
{ if (Material) Material->SetScalarParameterValue(TEXT("Highlight"),bEnabled?1.0f:0.0f); }
void AProceduralSword::Take(AActor* Holder)
{
    if (!CanPickUpBy(Holder)) return;
    Super::Take(Holder);
}
void AProceduralSword::SetHeldTransform(const FTransform& Transform)
{
    FVector Normal=FVector::YAxisVector;
    if (const auto* Camera=UGameplayStatics::GetPlayerCameraManager(this,0)) Normal=Camera->GetCameraRotation().Vector();
    const FQuat SocketRotation=Transform.GetRotation()*SocketRotationOffset.Quaternion();
    FVector Direction=FVector::VectorPlaneProject(SocketRotation.GetAxisX(),Normal).GetSafeNormal();
    if (Direction.IsNearlyZero()) Direction=-FVector::ZAxisVector;
    SetActorLocationAndRotation(Transform.GetLocation(),FRotationMatrix::MakeFromYZ(Normal,-Direction).ToQuat());
}
void AProceduralSword::SetCombatTransform(const FTransform& Transform)
{
    const FQuat Rotation=Transform.GetRotation().GetNormalized();
    SetActorLocationAndRotation(Transform.GetLocation(),FRotationMatrix::MakeFromYZ(Rotation.GetAxisY(),-Rotation.GetAxisX()).ToQuat());
}
void AProceduralSword::UpdateInventoryBrush(FSlateBrush& Brush) const
{
    Brush.SetResourceObject(InventoryMaterial);
    Brush.SetUVRegion(FBox2f(FVector2f(0,0),FVector2f(1,1)));
}
void AProceduralSword::Throw(const FVector& Origin,const FVector& InitialVelocity,AActor* Thrower)
{
    MarkDisplaced();
    DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    SetOwner(nullptr); bHeld=false; bFlying=true;
    Velocity=InitialVelocity; Velocity.Y=0; LaneY=Origin.Y;
    IgnoredActor=Thrower; FlightTime=0;
    // Keep the projected current orientation when releasing the socket.
    const FVector Up=GetActorQuat().GetAxisZ(); Angle=FMath::RadiansToDegrees(FMath::Atan2(Up.X,Up.Z));
    SetActorLocationAndRotation(Origin,FQuat(FVector::YAxisVector,FMath::DegreesToRadians(Angle)));
    SetHighlighted(false); SetActorTickEnabled(true);
}
void AProceduralSword::ReceiveProjectileImpact(const FVector& Impulse,AActor* Source)
{
    if (bHeld) return;
    FVector P=GetActorLocation(); P.Y=LaneY;
    Throw(P,Impulse,Source);
}
void AProceduralSword::Embed(const FVector& Point,const FVector& Direction,AActor* Support)
{
    FVector D=Direction; D.Y=0; D=D.GetSafeNormal();
    if (D.IsNearlyZero()) D=-FVector::ZAxisVector;
    const FQuat Rotation=FRotationMatrix::MakeFromYZ(FVector::YAxisVector,-D).ToQuat();
    const FVector Tip=Point+D*FMath::Max(0.0f,EmbedDepth);
    SetActorLocationAndRotation(Tip-D*(368*WorldUnitsPerPixel),Rotation);
    bHeld=false; bFlying=false; Velocity=FVector::ZeroVector;
    SetActorTickEnabled(false);
    if (Support && Support!=this && Support->GetRootComponent())
        AttachToActor(Support,FAttachmentTransformRules::KeepWorldTransform);
}
void AProceduralSword::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (GetIsReplicated() && !HasAuthority()) return;
    if (!bFlying) return;
    FlightTime+=DeltaSeconds;
    float Remaining=FMath::Min(DeltaSeconds,0.1f);
    auto* State=GetWorld()->GetSubsystem<USwordWorldSubsystem>();
    const float BladeLength=384*WorldUnitsPerPixel;
    TArray<TWeakObjectPtr<ASirrohWorldItem>,TInlineAllocator<32>> NearItems;
    const float Travel=Velocity.Size()*Remaining+490*Remaining*Remaining+BladeLength;
    for (auto Entry:State->Items)
    {
        auto* Item=Entry.Get();
        if (!Item || Item==this || Item->IsHeld() || Item==IgnoredActor.Get()) continue;
        const FVector Difference=Item->GetActorLocation()-GetActorLocation();
        if (FMath::Abs(Difference.X)<Travel+Item->GetItemHitRadius() && FMath::Abs(Difference.Z)<Travel+Item->GetItemHitRadius()) NearItems.Add(Entry);
    }
    while (Remaining>SMALL_NUMBER && bFlying)
    {
        // Bound tip displacement due to both translation and rotation, not just frame time.
        const float TipSpeed=Velocity.Size()+FMath::Abs(FMath::DegreesToRadians(SpinDegreesPerSecond))*BladeLength;
        const float Dt=FMath::Min(Remaining,FMath::Min(1.0f/120.0f,4.0f/FMath::Max(1.0f,TipSpeed)));
        Remaining-=Dt;
        const FVector Start=GetActorLocation(); const FQuat OldRotation=GetActorQuat();
        const FVector End=Start+Velocity*Dt+FVector(0,0,-490*Dt*Dt);
        Angle=FMath::Fmod(Angle+SpinDegreesPerSecond*Dt,360.0f);
        const FQuat Rotation(FVector::YAxisVector,FMath::DegreesToRadians(Angle));
        const FVector A=BladeTop(End,Rotation),B=BladeTip(End,Rotation);
        const FVector PreviousCentre=(BladeTop(Start,OldRotation)+BladeTip(Start,OldRotation))*0.5f;
        const FVector Centre=(A+B)*0.5f;
        constexpr float Radius=4.0f;
        FCollisionQueryParams Params(SCENE_QUERY_STAT(SwordFlight),false,this);
        if (IgnoredActor.IsValid()) Params.AddIgnoredActor(IgnoredActor.Get());
        FCollisionObjectQueryParams Objects; Objects.AddObjectTypesToQuery(ECC_WorldStatic); Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
        FHitResult Hit;
        const bool bWorldHit=GetWorld()->SweepSingleByObjectType(Hit,PreviousCentre,Centre,Rotation,Objects,
            FCollisionShape::MakeCapsule(Radius,BladeLength*0.5f+Radius),Params);
        FVector SurfacePoint; AActor* SurfaceOwner=nullptr;
        float SurfaceLane=LaneY;
        const bool bSurfaceHit=!bWorldHit && State->FindSurface(A,B,Radius,SurfacePoint,SurfaceOwner,SurfaceLane);
        if (bWorldHit || bSurfaceHit)
        {
            MarkDisplaced(); SetOwner(nullptr);
            // Proxy depth is retained visually. Interaction belongs to the support's closest lane below.
            if (bSurfaceHit) LaneY=SurfaceLane;
            FVector Point=bWorldHit?Hit.ImpactPoint:SurfacePoint;
            if (bSurfaceHit) Point.Y-=4.0f;
            Embed(Point,Velocity,bWorldHit?Hit.GetActor():nullptr);
            break;
        }
        FVector FlatA=A,FlatB=B; FlatA.Y=FlatB.Y=0;
        ASirrohWorldItem* ItemHit=nullptr; double Best=DBL_MAX;
        for (auto Entry:NearItems)
        {
            auto* Item=Entry.Get(); if (!Item || Item->IsHeld()) continue;
            FVector P=Item->GetActorLocation(); P.Y=0;
            const FVector Closest=FMath::ClosestPointOnSegment(P,FlatA,FlatB);
            if (FVector::DistSquared(Closest,P)>FMath::Square(Item->GetItemHitRadius()+Radius)) continue;
            const double Distance=FVector::DistSquared(Closest,FlatB);
            if (Distance<Best) { Best=Distance; ItemHit=Item; }
        }
        if (ItemHit)
        {
            ItemHit->ReceiveProjectileImpact(Velocity*0.15f+FVector(0,0,-60),this);
            LaneY=ItemHit->LaneY;
            Embed(ItemHit->GetActorLocation(),Velocity,ItemHit);
            break;
        }
        SetActorLocationAndRotation(End,Rotation);
        Velocity.Z-=980*Dt;
    }
    if (FlightTime>30 || GetActorLocation().Z < -100000) Destroy();
}
