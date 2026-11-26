#include "CombatDragonBodyAdapter.h"
#include "Combat/DirectionalCombatComponent.h"
#include "PhysicsControlComponent.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/ConstraintInstance.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "Engine/SkeletalMesh.h"

UCombatDragonBodyAdapter::UCombatDragonBodyAdapter()
{
    SetIsReplicatedByDefault(false);
    PrimaryComponentTick.bStartWithTickEnabled=false;
    individual_bone_selection.Reset();
}

bool UCombatDragonBodyAdapter::Initialize(USkeletalMeshComponent& Source, const FCombatFinalPose& Pose)
{
    if (Proxy) return Proxy->GetSkeletalMeshAsset()==Source.GetSkeletalMeshAsset();
    if (!Source.GetPhysicsAsset())
    {
        if (!bMissingAssetLogged) UE_LOG(LogTemp,Warning,TEXT("Combat hit physics on %s requires a PhysicsAsset for %s."),
            *GetOwner()->GetName(),*GetNameSafe(Source.GetSkeletalMeshAsset()));
        bMissingAssetLogged=true;
        return false;
    }
    Proxy=NewObject<USkeletalMeshComponent>(GetOwner());
    GetOwner()->AddInstanceComponent(Proxy);
    Proxy->SetSkeletalMesh(Source.GetSkeletalMeshAsset());
    Proxy->SetPhysicsAsset(Source.GetPhysicsAsset());
    Proxy->bEnablePhysicsOnDedicatedServer=true;
    Proxy->SetDisablePostProcessBlueprint(true);
    Proxy->SetWorldTransform(Source.GetComponentTransform());
    Proxy->SetHiddenInGame(true); Proxy->SetVisibility(false); Proxy->SetCastShadow(false);
    Proxy->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    // Contacts are already server swept against the exact final pose. This proxy only
    // integrates the reaction; it must not collide a second time with its owner/weapon.
    Proxy->SetCollisionResponseToAllChannels(ECR_Ignore);
    Proxy->SetEnableGravity(false);
    Proxy->RegisterComponent(); Proxy->SetComponentTickEnabled(false);
    Proxy->SetAllBodiesSimulatePhysics(false);
    for (const auto& Setup:Source.GetPhysicsAsset()->SkeletalBodySetups)
        if (Setup) individual_bone_selection.Add(Setup->BoneName);
    use_auto_blending_logic=false; modify_physics_states=false;
    Linear_Strength=1200; Angular_Strength=400; Linear_Damp=70; Angular_Damp=40;
    if (CombatBodySolver::UsesPhysicsAssetBiomechanics())
    {
        NativeControls=NewObject<UPhysicsControlComponent>(GetOwner());
        GetOwner()->AddInstanceComponent(NativeControls); NativeControls->RegisterComponent();
        NativeControls->SetComponentTickEnabled(false);
        Proxy->SetCollisionObjectType(ECC_WorldDynamic);
        Proxy->SetCollisionResponseToChannel(ECC_WorldStatic,ECR_Block);
        Proxy->SetCollisionResponseToChannel(ECC_WorldDynamic,ECR_Block);
    }
    else FirstTimeInitialization(Proxy);
    SetComponentTickEnabled(false); // the combat post-physics step supplies the complete target map
    if (handler_holder) handler_holder->SetReplicates(false);
    PrepareTargets(Pose);
    return true;
}

void UCombatDragonBodyAdapter::PrepareTargets(const FCombatFinalPose& Pose)
{
    Targets=Pose.BonesWorld;
    if (!Proxy) return;
    for (int32 I=0; I<Proxy->GetNumBones(); ++I)
    {
        const FName Name=Proxy->GetBoneName(I);
        const FTransform* Target=Targets.Find(Name);
        if (!Target) continue;
        if (Raw_Animation_Transforms.IsValidIndex(I)) Raw_Animation_Transforms[I]=*Target;
        if (auto* Body=Proxy->GetBodyInstance(Name); Body && !Body->IsInstanceSimulatingPhysics())
            Body->SetBodyTransform(*Target,ETeleportType::TeleportPhysics);
    }
}

FString UCombatDragonBodyAdapter::DescribePhysicsAsset(USkeletalMesh* Mesh,UPhysicsAsset* Asset)
{
    if (!Mesh || !Asset) return TEXT("Missing mesh/physics asset");
    FString Report;
    for (const auto& Setup:Asset->SkeletalBodySetups) if (Setup)
        Report+=FString::Printf(TEXT("%s bone=%d type=%d shapes=%d collision=%d\n"),*Setup->BoneName.ToString(),
            Mesh->GetRefSkeleton().FindBoneIndex(Setup->BoneName),int32(Setup->PhysicsType),Setup->AggGeom.GetElementCount(),int32(Setup->DefaultInstance.GetCollisionEnabled()));
    return Report;
}

void UCombatDragonBodyAdapter::Hit(FName Bone,const FVector& Point,const FVector& Impulse,float Strength,double Now)
{
    const auto* Debug=GetOwner()->FindComponentByClass<UDirectionalCombatComponent>();
    if (!Proxy || (Debug && !Debug->bDebugBodyPhysics)) return;
    const float DebugImpulse=Debug?Debug->DebugHitImpulse:1.f;
    if (NativeControls)
    {
        while (!Bone.IsNone() && !Proxy->GetBodyInstance(Bone)) Bone=Proxy->GetParentBone(Bone);
        if (auto* Body=Proxy->GetBodyInstance(Bone); Body && Targets.Contains(Bone) && Bone!=TEXT("pelvis") && Bone!=TEXT("root"))
        {
            // The hit clip supplies the main reaction; physics contributes a short secondary response.
            Body->SetBodyTransform(Targets.FindChecked(Bone),ETeleportType::TeleportPhysics);
            Body->SetInstanceSimulatePhysics(true); Body->SetEnableGravity(false);
            Body->AddImpulseAtPosition(Impulse.GetClampedToMaxSize(1800)*0.08f*DebugImpulse,Point);
            auto* Entry=Active.FindByPredicate([&](const auto& A){return A.Bone==Bone;});
            if (!Entry) { Entry=&Active.AddDefaulted_GetRef(); Entry->Bone=Bone; }
            Entry->Start=Now; Entry->Strength=Strength;
        }
        return;
    }
    if (!handler_holder) return;
    // Capsule regions can name a bone without a PhysicsAsset body. Select its nearest ancestor.
    while (!Bone.IsNone() && !Proxy->GetBodyInstance(Bone)) Bone=Proxy->GetParentBone(Bone);
    if (Bone==TEXT("pelvis"))
    {
        // Keep pelvis/capsule support kinematic. A skeleton whose torso capsule starts
        // above pelvis may have its first actual torso body further up the spine.
        Bone=NAME_None;
        for (const FName Candidate:individual_bone_selection)
            if (Candidate.ToString().StartsWith(TEXT("spine"))) { Bone=Candidate; break; }
        if (Bone.IsNone()) return;
    }
    auto* Body=Proxy->GetBodyInstance(Bone);
    const FTransform* Target=Targets.Find(Bone);
    const int32 Index=Proxy->GetBoneIndex(Bone);
    if (!Body || !Target || !handler_holder->physics_handle_array.IsValidIndex(Index)) return;
    auto* Handle=handler_holder->physics_handle_array[Index].Get();
    if (!Handle) return;
    // Preserve authored PhysicsAsset constraint frames and limits.
    // A torso must be able to carry its child bodies; kinematic children would pin
    // both ends of its joints and suppress the physical response almost completely.
    for (const FName Child:individual_bone_selection)
    {
        if (Child!=Bone && !Proxy->BoneIsChildOf(Child,Bone)) continue;
        auto* ChildBody=Proxy->GetBodyInstance(Child);
        const FTransform* ChildTarget=Targets.Find(Child);
        const int32 ChildIndex=Proxy->GetBoneIndex(Child);
        if (!ChildBody || !ChildTarget || !handler_holder->physics_handle_array.IsValidIndex(ChildIndex)) continue;
        auto* ChildHandle=handler_holder->physics_handle_array[ChildIndex].Get();
        if (!ChildHandle) continue;
        FActiveBody* Existing=Active.FindByPredicate([&](const auto& B){return B.Bone==Child;});
        if (!Existing)
        {
            ChildBody->SetBodyTransform(*ChildTarget,ETeleportType::TeleportPhysics);
            ChildBody->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
            ChildBody->SetInstanceSimulatePhysics(true); ChildBody->SetEnableGravity(false);
            ChildBody->WakeInstance();
            if (!ChildBody->IsValidBodyInstance() || !ChildBody->IsInstanceSimulatingPhysics())
            {
                UE_LOG(LogTemp,Warning,TEXT("Combat physical reaction cannot simulate %s on %s"),*Child.ToString(),*GetOwner()->GetName());
                continue;
            }
            ChildHandle->ReleaseComponent();
            ChildHandle->GrabComponentAtLocationWithRotation(Proxy,Child,ChildTarget->GetLocation(),ChildTarget->Rotator());
            ChildHandle->bInterpolateTarget=false;
            Existing=&Active.AddDefaulted_GetRef(); Existing->Bone=Child;
        }
        Existing->Start=Now; Existing->Strength=FMath::Clamp(Strength,0.1f,1.0f);
        ChildHandle->SetAngularStiffness(FMath::Lerp(400.0f,60.0f,Existing->Strength));
        ChildHandle->SetAngularDamping(FMath::Lerp(40.0f,12.0f,Existing->Strength));
        ChildHandle->SetLinearStiffness(FMath::Lerp(1200.0f,250.0f,Existing->Strength));
        ChildHandle->SetLinearDamping(FMath::Lerp(70.0f,25.0f,Existing->Strength));
    }
    Body->WakeInstance();
    Body->AddImpulseAtPosition(Impulse.GetClampedToMaxSize(6500),Point);
    // A region sweep can hit near the centre of mass. Model the torque about the
    // supporting joint as well, rather than discarding a purely translational strike.
    const FTransform* Support=Targets.Find(Proxy->GetParentBone(Bone));
    if (Support)
    {
        const FVector Lever=(Point-Support->GetLocation()).GetClampedToMaxSize(35);
        Body->AddAngularImpulseInRadians(FVector::CrossProduct(Lever,Impulse)*0.35,false);
    }
}

void UCombatDragonBodyAdapter::Solve(const FCombatBodyRig& Rig,FCombatFinalPose& Pose,double Now)
{
    if (!Proxy || !handler_holder) return;
    PrepareTargets(Pose);
    for (int32 I=Active.Num()-1; I>=0; --I)
    {
        const auto& A=Active[I];
        if (Now-A.Start<0.85) continue;
        const int32 Index=Proxy->GetBoneIndex(A.Bone);
        if (handler_holder->physics_handle_array.IsValidIndex(Index))
            if (auto* H=handler_holder->physics_handle_array[Index].Get()) H->ReleaseComponent();
        if (auto* B=Proxy->GetBodyInstance(A.Bone)) B->SetInstanceSimulatePhysics(false);
        Active.RemoveAt(I);
    }
    ModifyAllHandleTransforms();
    // Parent-first propagation preserves joint positions and limb lengths. Physical rotations
    // are bounded offsets, not absolute world poses that lag behind capsule movement.
    for (int32 I=0; I<Rig.Names.Num(); ++I)
    {
        const auto* A=Active.FindByPredicate([&](const auto& B){return B.Bone==Rig.Names[I];});
        if (!A) continue;
        const auto* Body=Proxy->GetBodyInstance(A->Bone);
        const auto* Target=Targets.Find(A->Bone);
        auto* Current=Pose.BonesWorld.Find(A->Bone);
        if (!Body || !Target || !Current) continue;
        const FTransform PhysicalTransform=Body->GetUnrealWorldTransform();
        FQuat Physical=PhysicalTransform.GetRotation();
        FVector PhysicalPosition=PhysicalTransform.GetLocation();
        FQuat Desired=Target->GetRotation();
        int32 Parent=Rig.Parents[I];
        while (Parent!=INDEX_NONE && !Proxy->GetBodyInstance(Rig.Names[Parent])) Parent=Rig.Parents[Parent];
        if (Parent!=INDEX_NONE)
        {
            const FName ParentName=Rig.Names[Parent];
            const auto* ParentBody=Proxy->GetBodyInstance(ParentName);
            const auto* ParentTarget=Targets.Find(ParentName);
            if (ParentBody && ParentTarget)
            {
                const FTransform PhysicalParent=ParentBody->GetUnrealWorldTransform();
                Physical=ParentTarget->GetRotation()*PhysicalParent.GetRotation().Inverse()*Physical;
                PhysicalPosition=ParentTarget->TransformPosition(PhysicalParent.InverseTransformPosition(PhysicalPosition));
            }
        }
        FQuat Delta=(Physical*Desired.Inverse()).GetNormalized();
        if (Delta.W<0) Delta=Delta*-1;
        const float Fade=1-FMath::Clamp(float((Now-A->Start-0.30)/0.55),0.0f,1.0f);
        const double Angle=Delta.GetAngle();
        Delta=FQuat::Slerp(FQuat::Identity,Delta,FMath::Min(1.0,FMath::DegreesToRadians(45.0)/FMath::Max(Angle,UE_SMALL_NUMBER))*FMath::Lerp(0.65f,1.0f,A->Strength)*Fade).GetNormalized();
        const FVector Pivot=Current->GetLocation();
        FVector Shift=(PhysicalPosition-Target->GetLocation()).GetClampedToMaxSize(
            FMath::Lerp(4.0f,14.0f,A->Strength))*Fade;
        // Transfer translation around the supporting joint, not into stretched bones.
        if (Rig.Parents[I]!=INDEX_NONE)
        {
            const FVector Anchor=Pose.BonesWorld.FindChecked(Rig.Names[Rig.Parents[I]]).GetLocation();
            const double Length=FVector::Distance(Pivot,Anchor);
            Shift=Anchor+(Pivot+Shift-Anchor).GetSafeNormal(UE_SMALL_NUMBER,
                (Pivot-Anchor).GetSafeNormal())*Length-Pivot;
        }
        for (int32 J=I; J<Rig.Names.Num(); ++J)
        {
            int32 Ancestor=J;
            while (Ancestor!=INDEX_NONE && Ancestor!=I) Ancestor=Rig.Parents[Ancestor];
            if (Ancestor!=I) continue;
            auto& Bone=Pose.BonesWorld.FindChecked(Rig.Names[J]);
            Bone.SetLocation(Pivot+Shift+Delta.RotateVector(Bone.GetLocation()-Pivot));
            Bone.SetRotation((Delta*Bone.GetRotation()).GetNormalized());
        }
    }
    Pose.PrimaryGripWorld=Rig.GripLocal*Pose.BonesWorld.FindChecked(Rig.Names[Rig.GripBone]);
    Pose.PrimaryGripWorld.SetScale3D(FVector::OneVector);
}

void UCombatDragonBodyAdapter::Stop()
{
    Active.Reset(); Targets.Reset();
    NativeNames.Reset(); LastNativeTime=-1;
    if (NativeControls) { NativeControls->DestroyComponent(); NativeControls=nullptr; }
    if (handler_holder)
    {
        for (const auto& Handle:handler_holder->physics_handle_array) if (Handle) Handle->ReleaseComponent();
        handler_holder->Destroy(); handler_holder=nullptr;
    }
    if (Proxy) { Proxy->DestroyComponent(); Proxy=nullptr; }
    owning_skeleton=nullptr; individual_bone_selection.Reset();
}
void UCombatDragonBodyAdapter::EndPlay(const EEndPlayReason::Type Reason)
{
    Stop(); Super::EndPlay(Reason);
}

bool UCombatDragonBodyAdapter::DrivePhysicsAsset(const FCombatBodyRig& Rig,FCombatFinalPose& Pose,double Now)
{
    if (!Proxy || !NativeControls) return false;
    // Authored/IK targets are already bounded upstream. Native PA joints constrain simulated reactions.
    PrepareTargets(Pose);
    const float Delta=LastNativeTime>=0?FMath::Clamp(float(Now-LastNativeTime),0.001f,0.1f):0.f;
    LastNativeTime=Now;
    Active.RemoveAll([Now](const FActiveBody& Hit){return Now-Hit.Start>=0.25;});
    for (FName Bone:individual_bone_selection)
    {
        auto* Body=Proxy->GetBodyInstance(Bone);
        const auto* Target=Targets.Find(Bone);
        const int32 Index=Proxy->GetBoneIndex(Bone);
        if (!Body || !Target || !Rig.Parents.IsValidIndex(Index)) continue;
        bool LowerBody=false;
        for (int32 Ancestor=Index;Ancestor!=INDEX_NONE;Ancestor=Rig.Parents[Ancestor])
            if (Ancestor==Rig.Hips[0] || Ancestor==Rig.Hips[1]) { LowerBody=true; break; }
        // Standing locomotion/foot support belongs to animation + Mover. Simulating it here
        // added a second ground support system against the kinematic pelvis.
        if (LowerBody || Bone==TEXT("pelvis") || Rig.Parents[Index]==INDEX_NONE ||
            !Active.ContainsByPredicate([Bone](const FActiveBody& Hit){return Hit.Bone==Bone;}))
        {
            if (Body->IsInstanceSimulatingPhysics()) Body->SetInstanceSimulatePhysics(false);
            Body->SetBodyTransform(*Target,ETeleportType::TeleportPhysics);
            if (const FName* Existing=NativeNames.Find(Bone)) NativeControls->SetControlEnabled(*Existing,false);
            continue;
        }
        FName Parent=Proxy->GetParentBone(Bone);
        while (!Parent.IsNone() && !Proxy->GetBodyInstance(Parent)) Parent=Proxy->GetParentBone(Parent);
        const auto* ParentTarget=Targets.Find(Parent); if (!ParentTarget) continue;
        if (!Body->IsInstanceSimulatingPhysics())
        {
            Body->SetBodyTransform(*Target,ETeleportType::TeleportPhysics);
            Body->SetInstanceSimulatePhysics(true); Body->SetEnableGravity(false);
            Body->SetPositionSolverIterationCount(12); Body->SetVelocitySolverIterationCount(4);
        }
        float Recovery=1.f;
        for (const auto& Hit:Active)
            if (Bone==Hit.Bone || Proxy->BoneIsChildOf(Bone,Hit.Bone))
                Recovery=FMath::Min(Recovery,FMath::Clamp(float((Now-Hit.Start)/0.3),0.f,1.f));
        FPhysicsControlData Data;
        Data.bUseSkeletalAnimation=false;
        const auto* Debug=GetOwner()->FindComponentByClass<UDirectionalCombatComponent>();
        Data.AngularStrength=FMath::Lerp(14.f,20.f,Recovery)*(Debug?Debug->DebugPhysicsStrength:1.f);
        Data.AngularDampingRatio=1.5f*(Debug?Debug->DebugPhysicsDamping:1.f);
        Data.AngularExtraDamping=4.f;
        // ClampExact limits the orientation, but unbounded target velocities could still
        // kick the drive against a limit. Do not differentiate these procedural targets.
        Data.AngularTargetVelocityMultiplier=0;
        Data.LinearTargetVelocityMultiplier=0;
        // PA joints own translations and lengths. Controls only drive authored joint orientation.
        Data.LinearStrength=0;
        FName* Name=NativeNames.Find(Bone);
        if (!Name)
        {
            const FName Created=NativeControls->CreateControl(Proxy,Parent,Proxy,Bone,Data,
                FPhysicsControlTarget(),TEXT("Combat"),TEXT("CombatPA_"));
            if (Created.IsNone()) return false;
            Name=&NativeNames.Add(Bone,Created);
            NativeControls->SetControlsInSetDriveLimitViolationResponse(TEXT("Combat"),EAngularDriveLimitViolationResponse::ClampExact);
        }
        else NativeControls->SetControlData(*Name,Data);
        NativeControls->SetControlEnabled(*Name,true);
        NativeControls->SetControlTargetPoses(*Name,ParentTarget->GetLocation(),ParentTarget->Rotator(),
            Target->GetLocation(),Target->Rotator(),0.f,true);
    }
    NativeControls->UpdateTargetCaches(Delta);
    NativeControls->UpdateControls(Delta);
    const auto Planned=Pose.BonesWorld;
    for (int32 I=0;I<Rig.Names.Num();++I)
    {
        const FName Bone=Rig.Names[I]; const int32 Parent=Rig.Parents[I];
        const auto* Target=Planned.Find(Bone); if (!Target) return false;
        auto* Body=Proxy->GetBodyInstance(Bone);
        FTransform Result=Parent!=INDEX_NONE?
            Target->GetRelativeTransform(Planned.FindChecked(Rig.Names[Parent]))*
                Pose.BonesWorld.FindChecked(Rig.Names[Parent]):*Target;
        if (Body && Body->IsInstanceSimulatingPhysics())
        {
            const auto* Hit=Active.FindByPredicate([Bone](const FActiveBody& H){return H.Bone==Bone;});
            const FTransform Physical=Body->GetUnrealWorldTransform();
            if (Hit && !Physical.ContainsNaN())
            {
                FQuat Offset=(Physical.GetRotation()*Target->GetRotation().Inverse()).GetNormalized();
                if (Offset.W<0) Offset=Offset*-1;
                const float Fade=FMath::Clamp(float((0.25-(Now-Hit->Start))/0.25),0.f,1.f);
                const double Weight=FMath::Min(0.08,FMath::DegreesToRadians(2.0)/FMath::Max(Offset.GetAngle(),UE_SMALL_NUMBER))*Fade;
                Result.SetRotation((FQuat::Slerp(FQuat::Identity,Offset,Weight)*Result.GetRotation()).GetNormalized());
            }
        }
        // Never copy simulated world translations: animation owns bone lengths and locomotion.
        Pose.BonesWorld[Bone]=Result;
    }
    Pose.PrimaryGripWorld=Rig.GripLocal*Pose.BonesWorld.FindChecked(Rig.Names[Rig.GripBone]);
    Pose.PrimaryGripWorld.SetScale3D(FVector::OneVector);
    Pose.PrimaryGripWorld.NormalizeRotation();
    Pose.bHasPrimaryGrip=!Pose.PrimaryGripWorld.ContainsNaN();
    if (Rig.SupportGripBone!=INDEX_NONE)
    {
        Pose.SecondaryGripWorld=Rig.SupportGripLocal*Pose.BonesWorld.FindChecked(Rig.Names[Rig.SupportGripBone]);
        Pose.SecondaryGripWorld.SetScale3D(FVector::OneVector);
        Pose.SecondaryGripWorld.NormalizeRotation();
        if (Pose.SecondaryGripWorld.ContainsNaN()) return false;
    }
    return Pose.bHasPrimaryGrip;
}
