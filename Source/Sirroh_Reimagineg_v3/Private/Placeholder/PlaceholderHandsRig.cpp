#include "Placeholder/PlaceholderHandsRig.h"
#include "Components/SkeletalMeshComponent.h"
#include "Rigs/RigHierarchy.h"
#include "Rigs/RigHierarchyController.h"
#include "TwoBoneIK.h"

namespace PlaceholderRig
{
    const FName Bones[2][3] = {{TEXT("upperarm_r"),TEXT("lowerarm_r"),TEXT("hand_r")},
                             {TEXT("upperarm_l"),TEXT("lowerarm_l"),TEXT("hand_l")}};
    FRigElementKey Key(FName Name) { return FRigElementKey(Name,ERigElementType::Bone); }
}

void UPlaceholderHandsRig::Solve(USkeletalMeshComponent* Mesh, const FVector* Targets,
    const float* Weights, TMap<FName,FTransform>& OutPose)
{
    OutPose.Reset();
    if (!Mesh || !GetHierarchy()) return;
    URigHierarchy* H=GetHierarchy();
    for (int32 Side=0;Side<2;++Side)
    {
        Alphas[Side]=0;
        bool bValid=true;
        for (const FName Bone : PlaceholderRig::Bones[Side]) bValid &= Mesh->GetBoneIndex(Bone)!=INDEX_NONE;
        if (!bValid || Weights[Side]<=KINDA_SMALL_NUMBER) continue;
        for (int32 Joint=0;Joint<3;++Joint)
        {
            const FName Bone=PlaceholderRig::Bones[Side][Joint];
            const FRigElementKey Key=PlaceholderRig::Key(Bone);
            const FTransform Pose=Mesh->GetSocketTransform(Bone);
            if (!H->Contains(Key))
                H->GetController()->AddBone(Bone,Joint?PlaceholderRig::Key(PlaceholderRig::Bones[Side][Joint-1]):FRigElementKey(),Pose);
            H->SetGlobalTransform(Key,Pose,false,false);
        }
        Goals[Side]=Targets[Side];
        Alphas[Side]=FMath::Clamp(Weights[Side],0.0f,1.0f);
        const FVector Shoulder=Mesh->GetSocketLocation(PlaceholderRig::Bones[Side][0]);
        // Downward pole keeps elbows readable in the side-on sprite view.
        Poles[Side]=Shoulder+FVector(0,Side==0?-12:12,-100);
    }
    Execute_Internal(TEXT("Forwards Solve"));
    // Include finger/twist descendants: hand artwork can use finger endpoints for its fit.
    for (int32 Index=0;Index<Mesh->GetNumBones();++Index)
    {
        const FName Bone=Mesh->GetBoneName(Index);
        for (int32 Side=0;Side<2;++Side)
        {
            if (Alphas[Side]<=KINDA_SMALL_NUMBER) continue;
            FName Ancestor=Bone;
            bool bFound=false;
            for (int32 Depth=0;Depth<16 && !Ancestor.IsNone() && !bFound;++Depth)
            {
                for (const FName ArmBone : PlaceholderRig::Bones[Side])
                    if (Ancestor==ArmBone)
                    {
                        const FTransform Relative=Mesh->GetSocketTransform(Bone).GetRelativeTransform(Mesh->GetSocketTransform(ArmBone));
                        OutPose.Add(Bone,Relative*H->GetGlobalTransform(PlaceholderRig::Key(ArmBone)));
                        bFound=true;
                        break;
                    }
                Ancestor=Mesh->GetParentBone(Ancestor);
            }
        }
    }
}

bool UPlaceholderHandsRig::Execute_Internal(const FName& EventName)
{
    URigHierarchy* H=GetHierarchy();
    if (!H) return false;
    for (int32 Side=0;Side<2;++Side)
    {
        if (Alphas[Side]<=KINDA_SMALL_NUMBER) continue;
        FTransform A=H->GetGlobalTransform(PlaceholderRig::Key(PlaceholderRig::Bones[Side][0]));
        FTransform B=H->GetGlobalTransform(PlaceholderRig::Key(PlaceholderRig::Bones[Side][1]));
        FTransform C=H->GetGlobalTransform(PlaceholderRig::Key(PlaceholderRig::Bones[Side][2]));
        const FVector Target=FMath::Lerp(C.GetLocation(),Goals[Side],static_cast<double>(Alphas[Side]));
        AnimationCore::SolveTwoBoneIK(A,B,C,Poles[Side],Target,false,1.0,1.0);
        H->SetGlobalTransform(PlaceholderRig::Key(PlaceholderRig::Bones[Side][0]),A,false,false);
        H->SetGlobalTransform(PlaceholderRig::Key(PlaceholderRig::Bones[Side][1]),B,false,false);
        H->SetGlobalTransform(PlaceholderRig::Key(PlaceholderRig::Bones[Side][2]),C,false,false);
    }
    return true;
}
