#include "Items/SirrohCarryProps.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "NiagaraComponent.h"
#include "ProceduralMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "GameFramework/Character.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "ASTFireReactiveComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "PhysicsEngine/BodySetup.h"

bool USirrohCarryPoseComponent::PrepareImportedMesh(UStaticMesh* Mesh)
{
#if WITH_EDITOR
    if(!Mesh)return false;
    Mesh->CreateBodySetup();
    UBodySetup* Body=Mesh->GetBodySetup();
    if(!Body)return false;
    if(Body->AggGeom.GetElementCount()==0)
    {
        FKBoxElem Box;const FBox Bounds=Mesh->GetBoundingBox();
        Box.Center=Bounds.GetCenter();const FVector Size=Bounds.GetSize();
        Box.X=Size.X;Box.Y=Size.Y;Box.Z=Size.Z;Body->AggGeom.BoxElems.Add(Box);
    }
    Body->CollisionTraceFlag=CTF_UseSimpleAndComplex;
    Body->InvalidatePhysicsData();Body->CreatePhysicsMeshes();Mesh->MarkPackageDirty();
    return true;
#else
    return false;
#endif
}

bool USirrohCarryPoseComponent::ConfigureImportedPose(const FString& Kind)
{
    if(Kind!=TEXT("Torch") && Kind!=TEXT("Box") && Kind!=TEXT("Barrel"))return false;
    const FString Root=TEXT("/GASPALS/Systems/OverlaySystem/OverlayPose/")+Kind+TEXT("/Pose_")+Kind;
    const bool Torch=Kind==TEXT("Torch");
    StandingIdle=LoadObject<UAnimSequence>(nullptr,*(Root+(Torch?TEXT("_Stand_Relax_Idle"):TEXT("_Stand_Idle"))));
    StandingMove=LoadObject<UAnimSequence>(nullptr,*(Root+(Torch?TEXT("_Stand_Relax_Move"):TEXT("_Stand_Move"))));
    Crouching=LoadObject<UAnimSequence>(nullptr,*(Root+(Torch?TEXT("_Crouch_Relax"):TEXT("_Crouch"))));
    if(!StandingIdle || !StandingMove || !Crouching)return false;
    USkeleton* Skeleton=StandingIdle->GetSkeleton();
    const USkeletalMeshSocket* Socket=Skeleton?Skeleton->FindSocket(FName(*(TEXT("overlay_")+Kind.ToLower()))):nullptr;
    if(!Socket)return false;
    const auto& Ref=Skeleton->GetReferenceSkeleton();
    GripBone=Socket->BoneName.ToString().EndsWith(TEXT("_l"))?TEXT("hand_l"):TEXT("hand_r");
    GripOffset=Socket->GetSocketLocalTransform();
    for(int32 I=Ref.FindBoneIndex(Socket->BoneName);I!=INDEX_NONE && Ref.GetBoneName(I)!=GripBone;I=Ref.GetParentIndex(I))
    {
        FTransform SocketParent;
        StandingIdle->GetBoneTransform(SocketParent,FSkeletonPoseBoneIndex(I),FAnimExtractContext(0.0),false);
        GripOffset=GripOffset*SocketParent;
    }
    bTwoHanded=Kind==TEXT("Box");bMirrorPose=Torch;
    if(bMirrorPose){GripBone=TEXT("hand_r");GripOffset.Mirror(EAxis::Y,EAxis::Y);}
    return true;
}

void USirrohCarryPoseComponent::SetTorchHand(int32 Slot)
{
    const bool Mirror=Slot==0;
    if(bMirrorPose!=Mirror){GripOffset.Mirror(EAxis::Y,EAxis::Y);Alpha=0;}
    bMirrorPose=Mirror;GripBone=Mirror?TEXT("hand_r"):TEXT("hand_l");
}

void USirrohCarryPoseComponent::ApplyPose(USkeletalMeshComponent* Mesh,TMap<FName,FTransform>& Pose,float Delta)
{
    if(!Mesh || !Mesh->GetSkeletalMeshAsset())return;
    const ACharacter* Character=Cast<ACharacter>(Mesh->GetOwner());
    const auto* Mover=Mesh->GetOwner()->FindComponentByClass<UCharacterMoverComponent>();
    const bool Crouched=Mover?Mover->IsCrouching():(Character && Character->bIsCrouched);
    const FVector Movement=Mover?Mover->GetVelocity():Mesh->GetOwner()->GetVelocity();
    UAnimSequence* Sequence=Crouched?Crouching.Get():
        (Movement.SizeSquared2D()>100?StandingMove.Get():StandingIdle.Get());
    if(!Sequence || !Sequence->GetSkeleton())return;
    Alpha=FMath::FInterpConstantTo(Alpha,1.f,Delta,1.f/FMath::Max(.01f,BlendSeconds));
    const auto& Ref=Mesh->GetSkeletalMeshAsset()->GetRefSkeleton();
    const auto& SourceRef=Sequence->GetSkeleton()->GetReferenceSkeleton();
    const auto& Locals=Mesh->GetBoneSpaceTransforms();
    if(Locals.Num()!=Ref.GetNum())return;
    TArray<FTransform> World;World.SetNum(Ref.GetNum());
    TArray<FTransform> TargetRef;TargetRef.SetNum(Ref.GetNum());
    if(bMirrorPose && CachedMirrorSequence.Get()!=Sequence)
    {
        CachedMirrorSequence=Sequence;
        TArray<FTransform> SampleWorld,RefWorld;
        SampleWorld.SetNum(SourceRef.GetNum());RefWorld.SetNum(SourceRef.GetNum());
        MirrorPoseWorld.SetNum(SourceRef.GetNum());MirrorRefWorld.SetNum(SourceRef.GetNum());
        for(int32 I=0;I<SourceRef.GetNum();++I)
        {
            FTransform Sample;Sequence->GetBoneTransform(Sample,FSkeletonPoseBoneIndex(I),FAnimExtractContext(0.0),false);
            const int32 Parent=SourceRef.GetParentIndex(I);
            SampleWorld[I]=Parent==INDEX_NONE?Sample:Sample*SampleWorld[Parent];
            RefWorld[I]=Parent==INDEX_NONE?SourceRef.GetRefBonePose()[I]:SourceRef.GetRefBonePose()[I]*RefWorld[Parent];
            MirrorPoseWorld[I]=SampleWorld[I];MirrorPoseWorld[I].Mirror(EAxis::Y,EAxis::Y);
            MirrorRefWorld[I]=RefWorld[I];MirrorRefWorld[I].Mirror(EAxis::Y,EAxis::Y);
        }
    }
    const bool Left=GripBone.ToString().EndsWith(TEXT("_l"));
    const int32 RightRoot=Ref.FindBoneIndex(TEXT("clavicle_r"));
    const int32 LeftRoot=Ref.FindBoneIndex(TEXT("clavicle_l"));
    for(int32 I=0;I<Ref.GetNum();++I)
    {
        const FName Bone=Ref.GetBoneName(I);
        const int32 Parent=Ref.GetParentIndex(I);
        const bool RightBranch=RightRoot!=INDEX_NONE && (I==RightRoot || Ref.BoneIsChildOf(I,RightRoot));
        const bool LeftBranch=LeftRoot!=INDEX_NONE && (I==LeftRoot || Ref.BoneIsChildOf(I,LeftRoot));
        const bool Mask=(RightBranch && (bTwoHanded || !Left)) || (LeftBranch && (bTwoHanded || Left));
        FTransform Local=Locals[I];
        FString SourceName=Bone.ToString();
        if(bMirrorPose)
        {
            if(SourceName.EndsWith(TEXT("_r")))SourceName=SourceName.LeftChop(2)+TEXT("_l");
            else if(SourceName.EndsWith(TEXT("_l")))SourceName=SourceName.LeftChop(2)+TEXT("_r");
        }
        const int32 SourceIndex=SourceRef.FindBoneIndex(FName(*SourceName));
        if(Mask && SourceIndex!=INDEX_NONE)
        {
            FTransform Sample;
            Sequence->GetBoneTransform(Sample,FSkeletonPoseBoneIndex(SourceIndex),FAnimExtractContext(0.0),false);
            // Preserve target proportions: these skeletons share bone names, not necessarily lengths.
            Local.SetRotation(FQuat::Slerp(Local.GetRotation(),Sample.GetRotation(),Alpha).GetNormalized());
        }
        TargetRef[I]=Parent==INDEX_NONE?Ref.GetRefBonePose()[I]:Ref.GetRefBonePose()[I]*TargetRef[Parent];
        World[I]=Local*(Parent==INDEX_NONE?Mesh->GetComponentTransform():World[Parent]);
        if(Mask && bMirrorPose && SourceIndex!=INDEX_NONE)
        {
            // Mirror in component space: mirroring local wrist/elbow frames separately
            // introduces extra rotations and sends the torch arm behind the body.
            const FQuat Desired=(MirrorPoseWorld[SourceIndex].GetRotation()*MirrorRefWorld[SourceIndex].GetRotation().Inverse()*TargetRef[I].GetRotation()).GetNormalized();
            const FQuat DesiredWorld=Mesh->GetComponentQuat()*Desired;
            World[I].SetRotation(FQuat::Slerp(Mesh->GetSocketQuaternion(Bone),DesiredWorld,Alpha).GetNormalized());
        }
        if(Mask)Pose.Add(Bone,World[I]);
    }
}

namespace
{
void PositionTorchEffects(ASirrohTorch* Torch)
{
    for(UActorComponent* Component:Torch->GetComponents())
    {
        if(auto* Light=Cast<UPointLightComponent>(Component);Light && Light->ComponentHasTag(TEXT("ASTFireOwnedLight")))
            Light->SetRelativeLocation(FVector::ZeroVector);
        if(auto* FX=Cast<UNiagaraComponent>(Component))
            { // Source mesh scale limits ignition roots, not Niagara particle size.
                FX->SetAbsolute(false,true,true);
                FX->SetWorldScale3D(FVector(Torch->Fire->FlameSize));
            }
    }
}
void ConfigureMesh(UStaticMeshComponent* Mesh)
{
    Mesh->SetMobility(EComponentMobility::Movable);
    Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Mesh->SetGenerateOverlapEvents(false);
    Mesh->SetIsReplicated(true);
    Mesh->BodyInstance.bLockYTranslation=true;
    Mesh->BodyInstance.DOFMode=EDOFMode::SixDOF;
}
void ReleasePhysics(AActor* Item,UStaticMeshComponent* Mesh,const FVector& Origin,const FVector& Velocity,AActor* Thrower,float Mass)
{
    Item->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    Item->SetOwner(nullptr);
    Item->SetActorLocation(Origin,false,nullptr,ETeleportType::TeleportPhysics);
    Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Mesh->SetCollisionResponseToChannel(ECC_Pawn,ECR_Ignore);
    Mesh->SetMassOverrideInKg(NAME_None,Mass);
    Mesh->SetSimulatePhysics(true);
    FVector PlanarVelocity=Velocity.GetClampedToMaxSize(3000);PlanarVelocity.Y=0;
    Mesh->SetPhysicsLinearVelocity(PlanarVelocity);
    Mesh->SetPhysicsAngularVelocityInDegrees(FVector(0,180,0));
    Item->SetActorTickEnabled(true);
    Item->ForceNetUpdate();
}
}
ASirrohTorch::ASirrohTorch()
{
    bReplicates=true;SetReplicateMovement(true);
    TorchMesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TorchMesh"));
    SetRootComponent(TorchMesh);ConfigureMesh(TorchMesh);
    Visual->SetupAttachment(TorchMesh);Visual->SetVisibility(false);
    CombatPhysicsProxy->SetupAttachment(TorchMesh);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/GASPALS/Systems/EquipmentSystem/Assets/Meshes/Torch.Torch"));
    TorchMesh->SetStaticMesh(Mesh.Object);
    CarryPose=CreateDefaultSubobject<USirrohCarryPoseComponent>(TEXT("CarryPose"));
    FlameHead=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FlameHead"));
    FlameHead->SetupAttachment(TorchMesh);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> HeadMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    FlameHead->SetStaticMesh(HeadMesh.Object);
    FlameHead->SetRelativeLocation(FlameTip);FlameHead->SetRelativeScale3D(FVector(.06f));
    FlameHead->SetVisibility(false);FlameHead->SetHiddenInGame(true);
    FlameHead->SetCollisionEnabled(ECollisionEnabled::NoCollision);FlameHead->SetCastShadow(false);
    Fire=CreateDefaultSubobject<UASTFireReactiveComponent>(TEXT("TorchFire"));
    Fire->PreferredMeshComponentName=TEXT("FlameHead");
    Fire->Coverage=EASTFireCoverage::Point;Fire->FlamePoints=1;
    Fire->PointOffset=FVector::ZeroVector;
    Fire->bAutoExtinguish=false;Fire->bEnableNiagara=true;
    Fire->bEnableSurfaceFire=false;Fire->bEnableCharring=false;
    Fire->bEnableProceduralFlames=true;Fire->bEnableFlameCards=false;
    Fire->FlameSize=.25f;
}
void ASirrohTorch::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    Fire->PointOffset=FVector::ZeroVector;
    FlameHead->SetRelativeLocation(FlameTip);
    Fire->PreferredMeshComponentName=TEXT("FlameHead");
    Fire->bEnableProceduralFlames=true;
    Fire->RefreshFireVisuals();
    PositionTorchEffects(this);
}
void ASirrohTorch::BeginPlay()
{
    // The imported 3D torch replaces the procedural sword card, while retaining its combat contract.
    ASirrohWorldItem::BeginPlay();LaneY=GetActorLocation().Y;
    if(HasAuthority() && !Definition.InstanceId.IsValid())Definition.InstanceId=FGuid::NewGuid();
    Visual->SetVisibility(false);Visual->SetHiddenInGame(true);
    const auto Geometry=GetCombatDefinition();
    CombatPhysicsProxy->SetRelativeLocation((Geometry.BladeBaseLocal+Geometry.BladeTipLocal)*.5);
    CombatPhysicsProxy->SetBoxExtent(FVector(4,4,Geometry.WeaponReach*.5));
    PositionTorchEffects(this);
}
void ASirrohTorch::Tick(float Delta)
{
    // Physical throws do not use the sword's card-flight/embed simulation.
    if(HasAuthority() && bFlying && TorchMesh->GetPhysicsLinearVelocity().SizeSquared()<100)
    {bFlying=false;ForceNetUpdate();}
}
void ASirrohTorch::Take(AActor* Holder)
{
    if(!CanPickUpBy(Holder))return;
    TorchMesh->SetSimulatePhysics(false);TorchMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ASirrohWorldItem::Take(Holder);
}
void ASirrohTorch::OnRep_HeldState()
{
    if(bHeld)TorchMesh->SetSimulatePhysics(false);
    TorchMesh->SetCollisionEnabled(bHeld?ECollisionEnabled::NoCollision:ECollisionEnabled::QueryAndPhysics);
}
void ASirrohTorch::Throw(const FVector& Origin,const FVector& InitialVelocity,AActor* Thrower)
{
    if(!HasAuthority())return;
    MarkDisplaced();bHeld=false;bFlying=true;LaneY=Origin.Y;
    ReleasePhysics(this,TorchMesh,Origin,InitialVelocity,Thrower,1.2f);
}
void ASirrohTorch::SetHeldTransform(const FTransform& Transform)
{const FTransform Held=CarryPose->HeldTransform(Transform);SetActorLocationAndRotation(Held.GetLocation(),Held.GetRotation());}
void ASirrohTorch::SetCombatTransform(const FTransform& Transform)
{SetActorTransform(GetCombatDefinition().PrimaryGripLocal.Inverse()*Transform);}
FName ASirrohTorch::HandSocket(int32 Slot) const{return CarryPose->GripBone;}
void ASirrohTorch::SetHighlighted(bool Enabled){TorchMesh->SetRenderCustomDepth(Enabled);}
FCombatWeaponDefinition ASirrohTorch::GetCombatDefinition() const
{
    FCombatWeaponDefinition Result;
    Result.WeaponInstanceId=Definition.InstanceId;Result.GeometryVersion=3;
    Result.WeaponWeight=1.2f;Result.WeaponPower=.65f;Result.HandleLength=30;
    Result.PrimaryGripLocal.SetRotation(FRotationMatrix::MakeFromXY(FVector::ZAxisVector,FVector::YAxisVector).ToQuat());
    Result.BladeBaseLocal=FVector(0,0,10);Result.BladeTipLocal=FlameTip;
    Result.WeaponReach=FlameTip.Size();Result.BladeRadius=4;Result.bApproximateGeometry=false;
    return Result;
}
ASirrohCarryProp::ASirrohCarryProp()
{
    bReplicates=true;SetReplicateMovement(true);
    PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.bStartWithTickEnabled=false;
    PropMesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PropMesh"));SetRootComponent(PropMesh);ConfigureMesh(PropMesh);
    CarryPose=CreateDefaultSubobject<USirrohCarryPoseComponent>(TEXT("CarryPose"));CarryPose->bTwoHanded=true;
}
void ASirrohCarryProp::BeginPlay(){Super::BeginPlay();LaneY=GetActorLocation().Y;}
void ASirrohCarryProp::Tick(float Delta)
{if(HasAuthority() && bFlying && PropMesh->GetPhysicsLinearVelocity().SizeSquared()<100){bFlying=false;ForceNetUpdate();}}
void ASirrohCarryProp::Take(AActor* Holder)
{
    if(!CanPickUpBy(Holder))return;
    PropMesh->SetSimulatePhysics(false);PropMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);Super::Take(Holder);
}
void ASirrohCarryProp::OnRep_HeldState()
{
    if(bHeld)PropMesh->SetSimulatePhysics(false);
    PropMesh->SetCollisionEnabled(bHeld?ECollisionEnabled::NoCollision:ECollisionEnabled::QueryAndPhysics);
}
void ASirrohCarryProp::Throw(const FVector& Origin,const FVector& Velocity,AActor* Thrower)
{
    if(!HasAuthority())return;
    MarkDisplaced();bHeld=false;bFlying=true;LaneY=Origin.Y;
    ReleasePhysics(this,PropMesh,Origin,Velocity,Thrower,FMath::Max(.1f,MassKg));
}
void ASirrohCarryProp::SetHeldTransform(const FTransform& Transform)
{const FTransform Held=CarryPose->HeldTransform(Transform);SetActorLocationAndRotation(Held.GetLocation(),Held.GetRotation());}
FName ASirrohCarryProp::HandSocket(int32 Slot) const{return CarryPose->GripBone;}
void ASirrohCarryProp::SetHighlighted(bool Enabled){PropMesh->SetRenderCustomDepth(Enabled);}
