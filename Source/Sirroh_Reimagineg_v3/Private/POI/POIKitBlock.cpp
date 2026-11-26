#include "POI/POIKitBlock.h"
#include "POIImageLighting.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SplineComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "MoverComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture2D.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"

APOIKitBlock::APOIKitBlock()
{
    PrimaryActorTick.bCanEverTick=true; PrimaryActorTick.bStartWithTickEnabled=false; PrimaryActorTick.TickGroup=TG_PrePhysics;
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    Body=CreateDefaultSubobject<UBoxComponent>(TEXT("PhysicalBody")); Body->SetupAttachment(RootComponent); Body->SetGenerateOverlapEvents(false);
    Visual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ImageOrBlock")); Visual->SetupAttachment(RootComponent); Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    TopVisual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TopImage")); TopVisual->SetupAttachment(RootComponent); TopVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BottomVisual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BottomImage")); BottomVisual->SetupAttachment(RootComponent); BottomVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Proximity=CreateDefaultSubobject<UBoxComponent>(TEXT("OneWayProximity")); Proximity->SetupAttachment(RootComponent);
    Proximity->SetCollisionEnabled(ECollisionEnabled::NoCollision); Proximity->SetCollisionResponseToAllChannels(ECR_Ignore); Proximity->SetCollisionResponseToChannel(ECC_Pawn,ECR_Overlap);
    Proximity->OnComponentBeginOverlap.AddDynamic(this,&APOIKitBlock::Enter); Proximity->OnComponentEndOverlap.AddDynamic(this,&APOIKitBlock::Leave);
    GASP=CreateDefaultSubobject<UChildActorComponent>(TEXT("GASPAdapter")); GASP->SetupAttachment(RootComponent);
    AST=CreateDefaultSubobject<UChildActorComponent>(TEXT("ASTAdapter")); AST->SetupAttachment(RootComponent);
    ASTOpposite=CreateDefaultSubobject<UChildActorComponent>(TEXT("ASTOppositeAdapter")); ASTOpposite->SetupAttachment(RootComponent);
    ASTFront=CreateDefaultSubobject<UChildActorComponent>(TEXT("ASTFrontAdapter")); ASTFront->SetupAttachment(RootComponent);
    ASTBack=CreateDefaultSubobject<UChildActorComponent>(TEXT("ASTBackAdapter")); ASTBack->SetupAttachment(RootComponent);
    GASPClass=TSoftClassPtr<AActor>(FSoftObjectPath(TEXT("/Game/Levels/LevelPrototyping/LevelBlock_Traversable.LevelBlock_Traversable_C")));
    ASTClass=TSoftClassPtr<AActor>(FSoftObjectPath(TEXT("/AST_ParkourSystem/Blueprints/Actor/ParkourActor.ParkourActor_C")));
}

AActor* APOIKitBlock::GetGASPActor() const { return GASP->GetChildActor(); }
AActor* APOIKitBlock::GetASTActor() const { return AST->GetChildActor(); }
void APOIKitBlock::OnConstruction(const FTransform& Transform) { Super::OnConstruction(Transform); RefreshBlock(); }

FVector4 APOIKitBlock::GetImageAlphaBounds(UTexture2D* Texture)
{
#if WITH_EDITORONLY_DATA
    if (Texture && Texture->Source.IsValid())
    {
        // Cache by source GUID: reimport invalidates the result; no pixel scan on construction repeats.
        static TMap<FString,FVector4> Cache;
        const FString Key=Texture->GetPathName()+Texture->Source.GetId().ToString();
        if (const auto* Found=Cache.Find(Key)) return *Found;
        FVector4 Rect(0,0,1,1);
        const int32 W=Texture->Source.GetSizeX(), H=Texture->Source.GetSizeY();
        TArray64<uint8> Pixels;
        const auto Format=Texture->Source.GetFormat();
        if ((Format==TSF_BGRA8) && Texture->Source.GetMipData(Pixels,0) && Pixels.Num()>=int64(W)*H*4)
        {
            int32 MinX=W,MinY=H,MaxX=-1,MaxY=-1;
            for (int32 Y=0;Y<H;++Y) for (int32 X=0;X<W;++X)
                if (Pixels[(int64(Y)*W+X)*4+3]>8)
                { MinX=FMath::Min(MinX,X); MinY=FMath::Min(MinY,Y); MaxX=FMath::Max(MaxX,X); MaxY=FMath::Max(MaxY,Y); }
            if (MaxX>=MinX && MaxY>=MinY) Rect=FVector4(double(MinX)/W,double(MinY)/H,double(MaxX+1-MinX)/W,double(MaxY+1-MinY)/H);
        }
        Cache.Add(Key,Rect); return Rect;
    }
#endif
    return FVector4(0,0,1,1);
}

void APOIKitBlock::RefreshBlock()
{
    for (const auto& Pair:OriginalIgnores) if (Pair.Key.IsValid()) RestoreCapsule(Pair.Key.Get(),Pair.Value);
    OriginalIgnores.Reset(); SetActorTickEnabled(false);
    const FVector Size=Settings.Size.GetAbs().ComponentMax(FVector(1));
    Body->SetBoxExtent(Size*.5); Body->SetRelativeLocation(FVector(0,0,-Size.Z*.5));
    const bool Physical=Settings.Collision!=EPOIKitCollision::NoPhysicalCollision;
    Body->SetCollisionObjectType(ECC_WorldStatic); Body->SetCollisionResponseToAllChannels(ECR_Block);
    Body->SetCollisionResponseToChannel(ECC_GameTraceChannel1,ECR_Ignore);
    Body->SetCollisionEnabled(Physical?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);
    Proximity->SetVisibility(false);
    Proximity->SetHiddenInGame(true);
    Proximity->SetBoxExtent(Size*.5+FVector(250,250,800)); Proximity->SetRelativeLocation(FVector(0,0,-Size.Z*.5));
    Proximity->SetCollisionEnabled(Physical && Settings.Collision==EPOIKitCollision::OneWay?ECollisionEnabled::QueryOnly:ECollisionEnabled::NoCollision);
    RefreshAppearance();
    const FVector ActorScale=GetActorScale3D().GetAbs();
    const bool WantGASP=Settings.Kind==EPOIKitKind::GASP || Settings.Kind==EPOIKitKind::Both;
    const bool WantAST=Settings.Kind==EPOIKitKind::AST || Settings.Kind==EPOIKitKind::Both;
    // SetChildActorClass destroys/recreates even an unchanged class in UE 5.8.
    // Loaded POI levels already own their adapters; BeginPlay must retain them.
    auto EnsureAdapter=[](UChildActorComponent* Component,UClass* Desired)
    {
        if(Component->GetChildActorClass()!=Desired)
            Component->SetChildActorClass(Desired);
        else if(Desired && !Component->GetChildActor())
            Component->CreateChildActor();
    };
    UClass* GASPType=WantGASP?GASPClass.LoadSynchronous():nullptr;
    UClass* ASTType=WantAST?ASTClass.LoadSynchronous():nullptr;
    EnsureAdapter(GASP,GASPType);
    EnsureAdapter(AST,ASTType);
    EnsureAdapter(ASTOpposite,Settings.bBothASTSides?ASTType:nullptr);
    EnsureAdapter(ASTFront,ASTType);
    EnsureAdapter(ASTBack,nullptr); // Rear edge is deliberately not climbable.
    // Verified source SM_Cube bounds: (0,0,0)..(100,100,100), bottom CORNER origin.
    GASP->SetRelativeTransform(FTransform(FRotator::ZeroRotator,FVector(-Size.X*.5,-Size.Y*.5,-Size.Z),Size/100));
    for (auto* Adapter:{AST.Get(),ASTOpposite.Get(),ASTFront.Get(),ASTBack.Get()})
    {
        const float Side=(Settings.bFacePositiveX?1.f:-1.f)*(Adapter==AST.Get()?1.f:-1.f);
        const bool FrontBack=Adapter==ASTFront.Get() || Adapter==ASTBack.Get();
        const bool Front=Adapter==ASTFront.Get();
        Adapter->SetAbsolute(false,false,true);
        Adapter->SetRelativeTransform(FrontBack?
            FTransform(FRotator(0,Front?180:0,0),FVector((Front?1:-1)*Size.X*.5,(Front?-1:1)*Size.Y*.5,0)):
            FTransform(FRotator(0,-Side*90,0),FVector(Side*Size.X*.5,Side*Size.Y*.5,0)));
        const float Length=FrontBack?Size.X*ActorScale.X:Size.Y*ActorScale.Y;
        if (auto* Edge=Adapter->GetChildActor())
        {
            if (auto* Spline=Edge->FindComponentByClass<USplineComponent>())
            {
                Spline->SetSplinePoints({FVector::ZeroVector,FVector(Length,0,0)},ESplineCoordinateSpace::Local);
                Spline->SetSplinePointType(0,ESplinePointType::Linear,false);
                Spline->SetSplinePointType(1,ESplinePointType::Linear,true);
                Spline->bSplineHasBeenEdited=true;
            }
            if (auto* P=FindFProperty<FByteProperty>(Edge->GetClass(),TEXT("LedgeType")))
                P->SetPropertyValue_InContainer(Edge,static_cast<uint8>(Settings.ASTLedge));
            const bool FreeHang=Settings.ASTHang==EPOIASTHang::Free ||
                (Settings.ASTHang==EPOIASTHang::Automatic && (Size.Z*GetActorScale3D().Z<120 || Settings.Collision==EPOIKitCollision::NoPhysicalCollision));
            if (auto* P=FindFProperty<FByteProperty>(Edge->GetClass(),TEXT("ClimbStyle")))
                P->SetPropertyValue_InContainer(Edge,FreeHang?1:0);
        }
    }
    for (auto* Adapter:{GASP.Get(),AST.Get(),ASTOpposite.Get(),ASTFront.Get(),ASTBack.Get()}) if (auto* A=Adapter->GetChildActor())
    {
        TArray<UPrimitiveComponent*> Components; A->GetComponents(Components);
        for (auto* C:Components)
        {
            C->SetVisibility(false); C->SetHiddenInGame(true); C->SetGenerateOverlapEvents(false);
            C->SetCollisionResponseToAllChannels(ECR_Ignore);
            if (C->IsA<UStaticMeshComponent>())
            {
                C->SetCollisionEnabled(Adapter==GASP.Get()?ECollisionEnabled::QueryOnly:ECollisionEnabled::NoCollision);
                C->SetCollisionObjectType(Adapter!=GASP.Get()?ECC_GameTraceChannel4:ECC_WorldDynamic);
                if (Adapter==GASP.Get()) C->SetCollisionResponseToChannel(ECC_GameTraceChannel1,ECR_Block);
            }
            else C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        }
    }
    for (auto* Adapter:{AST.Get(),ASTOpposite.Get(),ASTFront.Get(),ASTBack.Get()}) if (AActor* Ledge=Adapter->GetChildActor())
    {
        // AST object queries must hit the actor which implements BPI_ParkourActor.
        auto* Grip=FindObjectFast<UBoxComponent>(Ledge,TEXT("POIGripQuery"));
        if (!Grip)
        {
            Grip=NewObject<UBoxComponent>(Ledge,TEXT("POIGripQuery"));
            Ledge->AddInstanceComponent(Grip);
            Grip->SetupAttachment(Ledge->GetRootComponent());
            Grip->SetRelativeLocation(FVector(50,0,0));
            Grip->SetBoxExtent(FVector(50,4,5));
            Grip->RegisterComponent();
        }
        const bool FrontBack=Adapter==ASTFront.Get() || Adapter==ASTBack.Get();
        const float Length=FrontBack?Size.X*ActorScale.X:Size.Y*ActorScale.Y;
        Grip->SetRelativeLocation(FVector(Length*.5,0,0));
        Grip->SetBoxExtent(FVector(Length*.5,4,5));
        Grip->SetCollisionObjectType(ECC_GameTraceChannel4);
        Grip->SetCollisionResponseToAllChannels(ECR_Ignore);
        Grip->SetCollisionResponseToChannel(ECC_GameTraceChannel4,ECR_Block);
        Grip->SetGenerateOverlapEvents(false);
        Grip->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    }
    Status=TEXT("Ready. Origin = top centre. NoPhysicalCollision keeps traversal queries enabled.");
    if ((WantGASP && !GetGASPActor()) || (WantAST && !GetASTActor())) Status=TEXT("ERROR: required GASP/AST source Blueprint is missing.");
    if (Settings.Collision==EPOIKitCollision::OneWay && (!FMath::IsNearlyZero(GetActorRotation().Pitch) || !FMath::IsNearlyZero(GetActorRotation().Roll))) Status=TEXT("ERROR: OneWay requires an upright block (yaw allowed).");
}

bool APOIKitBlock::ShouldPassFromBelow(float Bottom,float Top,bool Ignoring)
{
    // AST motion warping may finish with feet a few cm below the nominal surface.
    // Requiring feet ABOVE the surface leaves OneWay permanently ignored after a mantle.
    return Bottom<Top-6 || (Ignoring && Bottom<Top-4);
}

void APOIKitBlock::BeginPlay()
{
    Super::BeginPlay(); RefreshBlock();
    TArray<AActor*> Overlaps; Proximity->GetOverlappingActors(Overlaps,APawn::StaticClass());
    for (auto* A:Overlaps) if (auto* C=A->FindComponentByClass<UCapsuleComponent>()) UpdateCapsule(C);
}
bool APOIKitBlock::BeginDropThrough(UCapsuleComponent* Capsule)
{
    if (!Capsule || Settings.Collision!=EPOIKitCollision::OneWay) return false;
    DropThroughUntil.Add(Capsule,GetWorld()->GetTimeSeconds()+.45);
    UpdateCapsule(Capsule);
    return true;
}

void APOIKitBlock::UpdateCapsule(UCapsuleComponent* C)
{
    if (!C || Settings.Collision!=EPOIKitCollision::OneWay) return;
    if (!OriginalIgnores.Contains(C))
    {
        OriginalIgnores.Add(C,C->GetMoveIgnoreComponents().Contains(Body));
        if (auto* P=Cast<APawn>(C->GetOwner())) if (auto* M=P->GetMovementComponent()) M->AddTickPrerequisiteActor(this);
        if (auto* M=C->GetOwner()->FindComponentByClass<UMoverComponent>()) M->AddTickPrerequisiteActor(this);
    }
    const bool Original=OriginalIgnores.FindChecked(C);
    const float Bottom=C->GetComponentLocation().Z-C->GetScaledCapsuleHalfHeight();
    const double* DropEnd=DropThroughUntil.Find(C);
    const bool Dropping=DropEnd && GetWorld()->GetTimeSeconds()<*DropEnd;
    if (DropEnd && !Dropping) DropThroughUntil.Remove(C);
    C->IgnoreComponentWhenMoving(Body,Original || Dropping || ShouldPassFromBelow(Bottom,GetActorLocation().Z,C->GetMoveIgnoreComponents().Contains(Body)));
    SetActorTickEnabled(true);
}
void APOIKitBlock::RestoreCapsule(UCapsuleComponent* C,bool Original)
{
    C->IgnoreComponentWhenMoving(Body,Original);
    if (auto* P=Cast<APawn>(C->GetOwner())) if (auto* M=P->GetMovementComponent()) M->RemoveTickPrerequisiteActor(this);
    if (auto* M=C->GetOwner()->FindComponentByClass<UMoverComponent>()) M->RemoveTickPrerequisiteActor(this);
}
void APOIKitBlock::Enter(UPrimitiveComponent*,AActor* Other,UPrimitiveComponent* OtherComp,int32,bool,const FHitResult&)
{ if (Cast<APawn>(Other)) if (auto* C=Cast<UCapsuleComponent>(OtherComp)) UpdateCapsule(C); }
void APOIKitBlock::Leave(UPrimitiveComponent*,AActor*,UPrimitiveComponent* OtherComp,int32)
{
    if (auto* C=Cast<UCapsuleComponent>(OtherComp)) if (const bool* Original=OriginalIgnores.Find(C))
    { RestoreCapsule(C,*Original); OriginalIgnores.Remove(C); }
    if (OriginalIgnores.IsEmpty()) SetActorTickEnabled(false);
}
void APOIKitBlock::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    for (auto It=OriginalIgnores.CreateIterator();It;++It) if (It.Key().IsValid()) UpdateCapsule(It.Key().Get()); else It.RemoveCurrent();
    if (OriginalIgnores.IsEmpty()) SetActorTickEnabled(false);
}
void APOIKitBlock::EndPlay(const EEndPlayReason::Type Reason)
{
    for (const auto& Pair:OriginalIgnores) if (Pair.Key.IsValid()) RestoreCapsule(Pair.Key.Get(),Pair.Value);
    OriginalIgnores.Reset(); Super::EndPlay(Reason);
}


void APOIKitBlock::RefreshAppearance()
{
    const FVector Size=Settings.Size.GetAbs().ComponentMax(FVector(1));
    const FVector ActorScale=GetActorScale3D().GetAbs();
    Visual->SetAbsolute(false,false,true);
    const bool Card=!Settings.Image.IsNull() || Settings.Kind==EPOIKitKind::Background;
    Visual->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,Card?TEXT("/Engine/BasicShapes/Plane.Plane"):TEXT("/Engine/BasicShapes/Cube.Cube")));
    Visual->SetRelativeLocation(FVector(0,Card?-Size.Y*.5-1:0,-Size.Z*.5));
    Visual->SetRelativeRotation(Card?FRotator(0,0,90):FRotator::ZeroRotator);
    Visual->SetRelativeScale3D(Card?FVector(Size.X*ActorScale.X/100,Size.Z*ActorScale.Z/100,1):Size*ActorScale/100);
    UMaterialInterface* Material=Settings.Material.LoadSynchronous();
    if (!Material && Card) Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/POIPrototype/M_POIImage.M_POIImage"));
    // Resolve per face below: both the front PNG and optional top/bottom PNG
    // must react to scene lighting. Do not replace a custom already-lit platform material.
    UTexture2D* FrontImage=Settings.Image.LoadSynchronous();
    UTexture2D* CapImage=Settings.TopImage.LoadSynchronous();
#if WITH_EDITOR
    if (!GetWorld() || !GetWorld()->IsGameWorld())
    {
        Settings.ImageUVRect=GetImageAlphaBounds(FrontImage);
        Settings.TopImageUVRect=GetImageAlphaBounds(CapImage);
    }
#endif
    auto ApplyImage=[&](UStaticMeshComponent* Face,UTexture2D* Image,const FVector4& Bounds)
    {
        UMaterialInterface* FaceMaterial=Material;
        if (Settings.Kind==EPOIKitKind::Background || (Image && (!Material || POIImageLighting::IsUnlit(Material))))
        {
            if (auto* Lit=POIImageLighting::LitImageMaterial()) FaceMaterial=Lit;
            else UE_LOG(LogTemp,Error,TEXT("POI: missing lit PNG material"));
        }
        Face->SetMaterial(0,FaceMaterial);
        if (FaceMaterial) if (auto* MID=Face->CreateDynamicMaterialInstance(0,FaceMaterial))
        {
            if (Image) MID->SetTextureParameterValue(TEXT("ImageTexture"),Image);
            MID->SetVectorParameterValue(TEXT("Tint"),Settings.Tint);
            const FVector4 R=Settings.bFitImageToAlpha?Bounds:FVector4(0,0,1,1);
            MID->SetVectorParameterValue(TEXT("UVRect"),FLinearColor(R.X,R.Y,R.Z,R.W));
        }
    };
    ApplyImage(Visual,FrontImage,Settings.ImageUVRect);
    for (auto* Face:{TopVisual.Get(),BottomVisual.Get()})
    {
        Face->SetVisibility(CapImage && Settings.Kind!=EPOIKitKind::Background);
        Face->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Plane.Plane")));
        Face->SetAbsolute(false,false,true);
        Face->SetRelativeScale3D(FVector(Size.X*ActorScale.X/100,Size.Y*ActorScale.Y/100,1));
        ApplyImage(Face,CapImage,Settings.TopImageUVRect);
    }
    TopVisual->SetRelativeLocation(FVector(0,0,.1));
    BottomVisual->SetRelativeLocation(FVector(0,0,-Size.Z-.1));
    BottomVisual->SetRelativeRotation(FRotator(0,0,180));
}

void APOIKitBlock::GetVisualMaskMeshes(TArray<UStaticMeshComponent*>& Out) const
{
    for(UStaticMeshComponent* Mesh:{Visual.Get(),TopVisual.Get(),BottomVisual.Get()})
        if(IsValid(Mesh) && Mesh->GetStaticMesh()) Out.Add(Mesh);
}
