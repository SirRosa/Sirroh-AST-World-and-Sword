#include "Character/SirrohPoseBridgeComponent.h"
#include "Items/SirrohCarryProps.h"
#include "Combat/CombatVitalityComponent.h"
#include "Combat/CombatTrainingNPC.h"
#include "PhysicsEngine/BodyInstance.h"




#include "Combat/DirectionalCombatComponent.h"
#include "Placeholder/PlaceholderHandsComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "UObject/UnrealType.h"
#include "GameFramework/Actor.h"

namespace
{
    bool NormalizePoseTransform(FTransform& Transform)
    {
        const double Norm=Transform.GetRotation().SizeSquared();
        if (Transform.ContainsNaN() || !FMath::IsFinite(Norm) || Norm<=UE_SMALL_NUMBER) return false;
        Transform.NormalizeRotation();
        return Transform.IsRotationNormalized();
    }

    bool AcceptPose(const TMap<FName,FTransform>& Input,TMap<FName,FTransform>& Output,
        bool& bReported,const UObject* Owner,const TCHAR* Layer)
    {
        TMap<FName,FTransform> Normalized=Input;
        for (auto& Bone:Normalized)
        {
            if (NormalizePoseTransform(Bone.Value)) continue;
            if (!bReported)
                UE_LOG(LogTemp,Error,TEXT("PoseBridge %s rejected %s pose: invalid transform on bone %s (rotation norm squared %.9g)."),
                    *GetNameSafe(Owner),Layer,*Bone.Key.ToString(),Bone.Value.GetRotation().SizeSquared());
            bReported=true;
            return false; // Keep the last accepted layer; never publish a partial skeleton.
        }
        bReported=false;
        Output=MoveTemp(Normalized);
        return true;
    }
}




USirrohPoseBridgeComponent::USirrohPoseBridgeComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostPhysics;
}




USirrohPoseBridgeComponent* USirrohPoseBridgeComponent::FindOrCreate(AActor* Owner)
{
    if (!Owner)
    {
        return nullptr;
    }




    if (USirrohPoseBridgeComponent* Existing = Owner->FindComponentByClass<USirrohPoseBridgeComponent>())
    {
        Existing->InitializeFromOwner();
        return Existing;
    }




    USirrohPoseBridgeComponent* Bridge = NewObject<USirrohPoseBridgeComponent>(Owner, TEXT("SirrohPoseBridge"));
    if (!Bridge)
    {
        return nullptr;
    }




    Owner->AddInstanceComponent(Bridge);
    Bridge->RegisterComponent();
    Bridge->InitializeFromOwner();
    return Bridge;
}




void USirrohPoseBridgeComponent::BeginPlay()
{
    Super::BeginPlay();
    InitializeFromOwner();
}




void USirrohPoseBridgeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    HandPose.Reset();
    CombatPose.Reset();




    // Runtime visibility changes do not alter the Blueprint defaults, but restoring the source
    // also keeps hot-reload / component re-registration predictable while PIE is shutting down.
    if (SourceMesh)
    {
        SourceMesh->SetHiddenInGame(false, false);
        SourceMesh->SetVisibility(true, false);
    }




    Super::EndPlay(EndPlayReason);
}




void USirrohPoseBridgeComponent::InitializeFromOwner()
{
    if (!GetOwner())
    {
        return;
    }




    if (!SourceMesh)
    {
        SourceMesh = GetOwner()->FindComponentByClass<USkeletalMeshComponent>();
        if (SourceMesh)
        {
            AddTickPrerequisiteComponent(SourceMesh);
            // The source may be hidden because the poseable mesh is what we render, but it must
            // still evaluate the AnimBP so locomotion remains the base pose every frame.
            SourceMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
        }
    }




    if (UPlaceholderHandsComponent* Hands = GetOwner()->FindComponentByClass<UPlaceholderHandsComponent>())
    {
        AddTickPrerequisiteComponent(Hands);
    }
    if (UDirectionalCombatComponent* Combat = GetOwner()->FindComponentByClass<UDirectionalCombatComponent>())
    {
        AddTickPrerequisiteComponent(Combat);
    }




    EnsureVisualMesh();
}




void USirrohPoseBridgeComponent::EnsureVisualMesh()
{
    if ((GetOwner() && GetOwner()->GetNetMode() == NM_DedicatedServer) || !bCreateRuntime3DVisual || !SourceMesh || !SourceMesh->GetSkeletalMeshAsset())
    {
        return;
    }




    if (!VisualMesh)
    {
        VisualMesh = NewObject<UPoseableMeshComponent>(GetOwner(), TEXT("Sirroh3DVisualMesh"));
        if (!VisualMesh)
        {
            return;
        }




        GetOwner()->AddInstanceComponent(VisualMesh);




        if (USceneComponent* Parent = SourceMesh->GetAttachParent())
        {
            VisualMesh->SetupAttachment(Parent, SourceMesh->GetAttachSocketName());
            VisualMesh->SetRelativeTransform(SourceMesh->GetRelativeTransform());
        }
        else
        {
            // A root skeletal mesh cannot be attached beside itself. Attaching to it with identity
            // keeps the same component/world transform and does not participate in bone hierarchy.
            VisualMesh->SetupAttachment(SourceMesh);
            VisualMesh->SetRelativeTransform(FTransform::Identity);
        }




        VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        VisualMesh->SetGenerateOverlapEvents(false);
        VisualMesh->RegisterComponent();
    }




    if (VisualMesh->GetSkinnedAsset() != SourceMesh->GetSkeletalMeshAsset())
    {
        CachedSkeletalMesh = SourceMesh->GetSkeletalMeshAsset();
        VisualMesh->SetSkinnedAssetAndUpdate(CachedSkeletalMesh, true);
    }




    SyncMaterials();




    VisualMesh->SetHiddenInGame(false, false);
    VisualMesh->SetVisibility(true, false);




    if (bHideAnimatedSourceMesh)
    {
        SourceMesh->SetHiddenInGame(true, false);
        SourceMesh->SetVisibility(false, false);
    }
}




void USirrohPoseBridgeComponent::SyncMaterials()
{
    if (!bMirrorMaterials || !SourceMesh || !VisualMesh)
    {
        return;
    }




    const int32 NumMaterials = SourceMesh->GetNumMaterials();
    for (int32 Index = 0; Index < NumMaterials; ++Index)
    {
        if (VisualMesh->GetMaterial(Index) != SourceMesh->GetMaterial(Index))
        {
            VisualMesh->SetMaterial(Index, SourceMesh->GetMaterial(Index));
        }
    }
}




void USirrohPoseBridgeComponent::PublishHandPose(const TMap<FName, FTransform>& InPose)
{
    AcceptPose(InPose,HandPose,bReportedInvalidHandPose,GetOwner(),TEXT("hand"));
}




void USirrohPoseBridgeComponent::ClearHandPose()
{
    HandPose.Reset();
}




void USirrohPoseBridgeComponent::PublishCombatPose(const TMap<FName, FTransform>& InPose)
{
    AcceptPose(InPose,CombatPose,bReportedInvalidCombatPose,GetOwner(),TEXT("combat"));
}




void USirrohPoseBridgeComponent::ClearCombatPose()
{
    CombatPose.Reset();
}




bool USirrohPoseBridgeComponent::TryGetHandBoneTransform(FName BoneName, FTransform& OutTransform) const
{
    if (const FTransform* Override = HandPose.Find(BoneName))
    {
        OutTransform = *Override;
        return true;
    }
    return false;
}




bool USirrohPoseBridgeComponent::TryGetFinalBoneTransform(FName BoneName, FTransform& OutTransform) const
{
    if (const FTransform* Override = CombatPose.Find(BoneName))
    {
        OutTransform = *Override;
        return true;
    }
    if (CombatPose.IsEmpty())
        if (const FTransform* Override = HandPose.Find(BoneName))
        { OutTransform = *Override; return true; }
    if (SourceMesh && SourceMesh->GetBoneIndex(BoneName) != INDEX_NONE)
    {
        // A bone without its own override still inherits an overridden spine/root.
        // Resolve the current solver layer, not last frame's rendered socket cache.
        const auto& Layer=CombatPose.IsEmpty()?HandPose:CombatPose;
        const auto& Skeleton=SourceMesh->GetSkeletalMeshAsset()->GetRefSkeleton();
        const auto& Locals=SourceMesh->GetBoneSpaceTransforms();
        TArray<int32,TInlineAllocator<16>> Chain;
        for (int32 Bone=SourceMesh->GetBoneIndex(BoneName);Bone!=INDEX_NONE;Bone=Skeleton.GetParentIndex(Bone))
            Chain.Add(Bone);
        OutTransform=SourceMesh->GetComponentTransform();
        for (int32 I=Chain.Num()-1;I>=0;--I)
        {
            const int32 Bone=Chain[I];
            if (const auto* Override=Layer.Find(Skeleton.GetBoneName(Bone))) OutTransform=*Override;
            else if (Locals.IsValidIndex(Bone)) OutTransform=Locals[Bone]*OutTransform;
            else {OutTransform=SourceMesh->GetSocketTransform(BoneName);break;}
        }
        return true;
    }
    return false;
}




FTransform USirrohPoseBridgeComponent::GetFinalBoneTransform(FName BoneName) const
{
    FTransform Result = FTransform::Identity;
    TryGetFinalBoneTransform(BoneName, Result);
    return Result;
}




void USirrohPoseBridgeComponent::SyncVisualPose()
{
    if (!SourceMesh || !VisualMesh || !SourceMesh->GetSkeletalMeshAsset())
    {
        return;
    }




    if (VisualMesh->GetSkinnedAsset() != SourceMesh->GetSkeletalMeshAsset())
    {
        EnsureVisualMesh();
        if (!VisualMesh || VisualMesh->GetSkinnedAsset() != SourceMesh->GetSkeletalMeshAsset())
        {
            return;
        }
    }




    if (auto* H=GetOwner()->FindComponentByClass<UCombatVitalityComponent>(); H && H->UsesRagdollPose())
    {
        VisualMesh->SetWorldTransform(SourceMesh->GetComponentTransform());
        // BoneSpaceTransforms are pre-physics animation; ComponentSpaceTransforms contain the physical result.
        const auto& Physical=SourceMesh->GetComponentSpaceTransforms();
        const auto& Ref=SourceMesh->GetSkeletalMeshAsset()->GetRefSkeleton();
        if (Physical.Num()==VisualMesh->BoneSpaceTransforms.Num())
        {
            TArray<FTransform> World;World.SetNum(Physical.Num());
            for (int32 I=0;I<Physical.Num();++I)
            {
                const int32 Parent=Ref.GetParentIndex(I);
                FTransform Local=Parent==INDEX_NONE?Physical[I]:Physical[I].GetRelativeTransform(Physical[Parent]);
                const FTransform ParentWorld=Parent==INDEX_NONE?VisualMesh->GetComponentTransform():World[Parent];
                World[I]=Local*ParentWorld;
                // Chaos bodies are authoritative even when a single-node animation source has
                // not blended its physical pose into ComponentSpaceTransforms this frame.
                if(auto* Body=SourceMesh->GetBodyInstance(Ref.GetBoneName(I));Body && Body->IsInstanceSimulatingPhysics())
                    World[I]=Body->GetUnrealWorldTransform();
                Local=World[I].GetRelativeTransform(ParentWorld);
                if (NormalizePoseTransform(Local)) VisualMesh->BoneSpaceTransforms[I]=Local;
            }
        }
        H->ApplyNetworkRagdoll(*VisualMesh);
        H->BlendDeathPose(*VisualMesh);
        VisualMesh->RefreshBoneTransforms();
        return;
    }
    // Keep component placement synchronized if the character Blueprint adjusts the source mesh.
    if (SourceMesh->GetAttachParent())
    {
        VisualMesh->SetWorldTransform(SourceMesh->GetComponentTransform());
    }




    // A disarmed training pawn has no locomotion AnimBP. Do not replace its last
    // valid composed idle with the reference A-pose when the combat layer is cleared.
    if(GetOwner()->IsA<ACombatTrainingNPC>() && bHasComposedNPCPose && CombatPose.IsEmpty() && HandPose.IsEmpty()
        && SourceMesh->GetAnimationMode()==EAnimationMode::AnimationSingleNode && !SourceMesh->AnimationData.AnimToPlay)
        return;
    // First mirror the normal AnimBP/locomotion pose. Then apply only the procedural
    // overrides. Skeleton bone arrays are parent-first, so world-space child overrides see
    // already-updated parents when several bones from the same chain are overridden.
    VisualMesh->CopyPoseFromSkeletalComponent(SourceMesh);




    const int32 BoneCount = SourceMesh->GetNumBones();
    if (VisualMesh->BoneSpaceTransforms.Num()!=BoneCount) return;
    const auto& Reference=SourceMesh->GetSkeletalMeshAsset()->GetRefSkeleton().GetRefBonePose();
    bool bInvalidVisual=false;
    // SetBoneTransformByName converts world -> component -> parent-local without
    // normalization. Its next call reconstructs the parent from these locals;
    // normalize each result BEFORE a descendant can consume it.
    for (int32 BoneIndex=0; BoneIndex<BoneCount; ++BoneIndex)
    {
        if (!NormalizePoseTransform(VisualMesh->BoneSpaceTransforms[BoneIndex]))
        {
            VisualMesh->BoneSpaceTransforms[BoneIndex]=Reference[BoneIndex];
            bInvalidVisual=true;
        }
    }
    // Assemble one complete pose parent-first, then commit once. A combat frame never
    // borrows a missing joint from the inventory solver's previous world-space frame.
    const auto& Layer=CombatPose.IsEmpty()?HandPose:CombatPose;
    TArray<FTransform> Locals=VisualMesh->BoneSpaceTransforms;
    TArray<FTransform> World;
    World.SetNum(BoneCount);
    const auto& Skeleton=SourceMesh->GetSkeletalMeshAsset()->GetRefSkeleton();
    for (int32 I=0;I<BoneCount;++I)
    {
        const int32 Parent=Skeleton.GetParentIndex(I);
        const FTransform ParentWorld=Parent==INDEX_NONE?VisualMesh->GetComponentTransform():World[Parent];
        World[I]=Locals[I]*ParentWorld;
        if (const FTransform* Override=Layer.Find(Skeleton.GetBoneName(I))) World[I]=*Override;
        Locals[I]=World[I].GetRelativeTransform(ParentWorld);
        if (!NormalizePoseTransform(Locals[I])) { bInvalidVisual=true; break; }
    }
    if (!bInvalidVisual) { VisualMesh->BoneSpaceTransforms=MoveTemp(Locals);if(!Layer.IsEmpty())bHasComposedNPCPose=true; }
    if (bInvalidVisual && !bReportedInvalidVisualPose)
        UE_LOG(LogTemp,Error,TEXT("PoseBridge %s rejected invalid local bone transforms before rendering."),*GetNameSafe(GetOwner()));
    bReportedInvalidVisualPose=bInvalidVisual;




    VisualMesh->RefreshBoneTransforms();
}




void USirrohPoseBridgeComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);




    if (!SourceMesh || SourceMesh->GetSkeletalMeshAsset() != CachedSkeletalMesh || !VisualMesh)
    {
        InitializeFromOwner();
    }




    if (!bCreateRuntime3DVisual)
    {
        if (SourceMesh)
        {
            SourceMesh->SetHiddenInGame(false, false);
            SourceMesh->SetVisibility(true, false);
        }
        if (VisualMesh)
        {
            VisualMesh->SetHiddenInGame(true, false);
            VisualMesh->SetVisibility(false, false);
        }
        return;
    }




    EnsureVisualMesh();
    SyncMaterials();
    SyncVisualPose();
    if(auto* Hands=GetOwner()->FindComponentByClass<UPlaceholderHandsComponent>();Hands && !Hands->IsASTPickupActive())
        for(int32 Slot=0;Slot<2;++Slot)
            if(auto* Item=Hands->GetItem(Slot))
                if(auto* Carry=Item->FindComponentByClass<USirrohCarryPoseComponent>())
                {
                    const auto* Combat=GetOwner()->FindComponentByClass<UDirectionalCombatComponent>();
                    if(Slot==0 && Combat && Combat->OwnsMotionCoreWeapon())continue;
                    FTransform Hand;
                    if(TryGetFinalBoneTransform(Carry->GripBone,Hand))
                    {
                        Item->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
                        Item->SetHeldTransform(Hand);
                        if(auto* Torch=Cast<ASirrohTorch>(Item))
                        {
                            // The model's handle grip is its local origin. Centre it in the
                            // rendered palm, not at the wrist/forearm bone origin.
                            FTransform Index,Middle,Pinky;
                            if(TryGetFinalBoneTransform(Slot==0?TEXT("index_01_r"):TEXT("index_01_l"),Index)
                                && TryGetFinalBoneTransform(Slot==0?TEXT("middle_02_r"):TEXT("middle_02_l"),Middle)
                                && TryGetFinalBoneTransform(Slot==0?TEXT("pinky_01_r"):TEXT("pinky_01_l"),Pinky))
                                Torch->SetActorLocation((Index.GetLocation()+Middle.GetLocation()+Pinky.GetLocation())/3.0);
                        }
                    }
                }

    // AST's backpack was attached to the hidden animation source, which has no
    // procedural combat layer. Keep its authored socket offset on the final pose.
    for (UActorComponent* Component : GetOwner()->GetComponents())
    {
        if (!Component || !Component->ComponentHasTag(TEXT("InventorySystem"))) continue;
        const auto* Property=FindFProperty<FObjectPropertyBase>(Component->GetClass(),TEXT("Backpack"));
        auto* Backpack=Property?Cast<USkeletalMeshComponent>(Property->GetObjectPropertyValue_InContainer(Component)):nullptr;
        if (!Backpack || Backpack->GetAttachParent()!=SourceMesh || Backpack->GetAttachSocketName().IsNone()) continue;
        const FName Socket=Backpack->GetAttachSocketName();
        if (!VisualMesh->DoesSocketExist(Socket)) continue;
        Backpack->AttachToComponent(VisualMesh,FAttachmentTransformRules::KeepRelativeTransform,Socket);
        Backpack->AddTickPrerequisiteComponent(this);
        Backpack->PrimaryComponentTick.TickGroup=TG_PostPhysics;
    }
}
