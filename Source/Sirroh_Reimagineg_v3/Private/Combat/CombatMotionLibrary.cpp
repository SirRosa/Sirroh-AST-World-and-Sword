#include "Combat/DirectionalCombatComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Combat/CombatMotionCatalog.h"
#include "Animation/AnimSequence.h"
#include "MoverComponent.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "GameFramework/Actor.h"

TArray<FCombatMotionSource> CombatBodySolver::LibrarySources(const FCombatStyleDefinition& Style)
{
    TArray<FCombatMotionSource> Result;
    const UDataTable* Table=Style.MotionLibrary.LoadSynchronous();
    if (!Table) return Result; // Isolated hint/reaction sampling intentionally clears the library.
    if (!Style.CatalogProfile.IsNone())
    {
        FResolvedCombatMotionProfile Profile; FString Error;
        if (!UCombatMotionCatalog::ResolveProfile(const_cast<UDataTable*>(Table),Style.CatalogProfile,Profile,Error))
        { UE_LOG(LogTemp,Error,TEXT("Motion catalog rejected: %s"),*Error); return Result; }
        return UCombatMotionCatalog::RuntimeSources(Profile);
    }
    if (!Table || Table->GetRowStruct()!=FCombatMotionSource::StaticStruct()) return Result;
    TArray<FName> Names=Table->GetRowNames(); Names.Sort(FNameLexicalLess());
    for (FName Name:Names)
    {
        const auto* Row=Table->FindRow<FCombatMotionSource>(Name,TEXT("Motion library"));
        if (!Row || !Row->bEnabled || (Row->Hands!=0 && Row->Hands!=(Style.bTwoHanded?2:1))
            || (!Row->WeaponFamily.IsNone() && Row->WeaponFamily!=Style.MotionFamily)) continue;
        // Catalog remains complete; cache only the actions currently evaluated by this solver.
        if (Row->Action!=TEXT("Attack") && Row->Action!=TEXT("Guard") && Row->Action!=TEXT("Walk")
            && Row->Action!=TEXT("GuardAim") && Row->Action!=TEXT("LookAim")
            && Row->Action!=TEXT("Dodge") && Row->Action!=TEXT("Roll") && Row->Action!=TEXT("Taunt")
            && Row->Action!=TEXT("Kick") && Row->Action!=TEXT("Equip") && Row->Action!=TEXT("Holster")
            && Row->Action!=TEXT("Carry") && Row->Action!=TEXT("CarryWalk") && Row->Action!=TEXT("Run")
            && Row->Action!=TEXT("Jump") && Row->Action!=TEXT("Fall") && Row->Action!=TEXT("Land")
            && Row->Action!=TEXT("Sprint") && Row->Action!=TEXT("Crouch")
            && Row->Action!=TEXT("CrouchStart") && Row->Action!=TEXT("CrouchEnd") && Row->Action!=TEXT("Turn")) continue;
        FCombatMotionSource Source=*Row;
        TSet<FName> Visited; FName Fallback=Source.FallbackRow;
        while (!Source.Animation.LoadSynchronous() && !Fallback.IsNone() && !Visited.Contains(Fallback))
        {
            Visited.Add(Fallback);
            const auto* Other=Table->FindRow<FCombatMotionSource>(Fallback,TEXT("Motion fallback"));
            if (!Other || !Other->bEnabled || (Other->Hands!=0 && Other->Hands!=Source.Hands)) break;
            Source.Animation=Other->Animation; Fallback=Other->FallbackRow;
        }
        if (!Source.Animation.IsNull()) Result.Add(MoveTemp(Source));
    }
    return Result;
}

int32 UDirectionalCombatComponent::SelectCatalogAction(FName Action,FVector2D Direction,bool Moving,int32 Seed) const
{
    if (!MotionRig.Catalog) return INDEX_NONE;
    static const TMap<FName,FName> Categories={
        {TEXT("Guard"),TEXT("Movement.Combat.Base")},{TEXT("Carry"),TEXT("Movement.OutOfCombat.Base")},
        {TEXT("Walk"),TEXT("Movement.Combat.Walk")},{TEXT("CarryWalk"),TEXT("Movement.OutOfCombat.Walk")},
        {TEXT("Equip"),TEXT("Movement.OutOfCombat.Transition")},{TEXT("Holster"),TEXT("Movement.Combat.Transition")},
        {TEXT("Dodge"),TEXT("Movement.Dodge")},{TEXT("Roll"),TEXT("Movement.Roll")},
        {TEXT("Taunt"),TEXT("Other.Taunt")},{TEXT("Kick"),TEXT("Offense.Kick")}};
    FCombatCatalogQuery Q;
    const auto* Category=Categories.Find(Action);
    Q.Category=Category?*Category:FName(TEXT("Movement.")+Action.ToString());
    Q.Direction=UCombatMotionCatalog::DirectionName(Direction); Q.Seed=Seed;
    // Legacy locomotion rows use InPlace as their slot context even for authored root travel.
    Q.Context=Action==TEXT("Kick") && Moving?TEXT("Moving"):TEXT("InPlace");
    Q.Guard=bCombatActive?1:0; Q.bRestrictVariants=true;
    for (int32 I=0;I<MotionRig.Sources.Num();++I)
        if (MotionRig.Motions.IsValidIndex(I) && MotionRig.Motions[I].IsValid()) Q.AllowedVariants.Add(MotionRig.Sources[I].CatalogVariantId);
    const auto Choice=UCombatMotionCatalog::ChooseForState(*MotionRig.Catalog,Q);
    return Choice.bFound?MotionRig.Sources.IndexOfByPredicate([&](const auto& S){return S.CatalogVariantId==Choice.Variant.Id;}):INDEX_NONE;
}

int32 UDirectionalCombatComponent::SelectLibraryAttack(const TArray<int32>& Candidates,EDirectionalAttack Type,const FVector& Target)
{
    if (MotionCoreStyle.MotionLibrary.IsNull()) return INDEX_NONE;
    const UMoverComponent* Mover=GetOwner()->FindComponentByClass<UMoverComponent>();
    const auto* CharacterMover=Cast<UCharacterMoverComponent>(Mover);
    const bool Air=Mover && !Mover->HasGameplayTag(Mover_IsOnGround,false);
    const bool Crouch=CharacterMover && CharacterMover->IsCrouching();
    const FVector Travel=GetOwner()->GetActorQuat().UnrotateVector(Target-BodyAttack.StartWorkingPoint);
    FVector2D Direction(Travel.Y,Travel.Z);
    if (Direction.SizeSquared()<225)
    {
        const FTransform* Pelvis=MotionFinalPose.BonesWorld.Find(TEXT("pelvis"));
        const FVector Centre=Pelvis?Pelvis->GetLocation():GetOwner()->GetActorLocation();
        const FVector Side=GetOwner()->GetActorQuat().UnrotateVector(BodyAttack.StartWorkingPoint-Centre);
        Direction=FVector2D(-Side.Y,-Side.Z);
    }
    Direction.Normalize();
    const bool Move=GetOwner()->GetVelocity().Size2D()>30 || FVector::Dist(Target,GetOwner()->GetActorLocation())>180;
    const bool Sprint=GetOwner()->GetVelocity().Size2D()>350;
    if (MotionRig.Catalog)
    {
        FCombatCatalogQuery Query;
        Query.Category=Air?TEXT("Offense.InAir"):Type==EDirectionalAttack::Heavy?TEXT("Offense.Heavy"):
            Type==EDirectionalAttack::Pierce?TEXT("Offense.Pierce"):TEXT("Offense.Light");
        Query.Direction=UCombatMotionCatalog::DirectionName(Direction);
        Query.Context=Move?TEXT("Moving"):TEXT("InPlace"); Query.Seed=int32(BodyAttack.Id);
        Query.Airborne=Air; Query.Sprint=Sprint; Query.Crouch=Crouch; Query.Guard=1; Query.bRestrictVariants=true;
        for (int32 I:Candidates) Query.AllowedVariants.Add(MotionRig.Sources[I].CatalogVariantId);
        const auto Choice=UCombatMotionCatalog::ChooseForState(*MotionRig.Catalog,Query);
        if (!Choice.bFound) return INDEX_NONE;
        BodyAttack.bLayeredAnimation=Choice.bUseLayeredAnimation;
        return MotionRig.Sources.IndexOfByPredicate([&](const auto& S){return S.CatalogVariantId==Choice.Variant.Id;});
    }
    double Best=TNumericLimits<double>::Max(); int32 Selected=INDEX_NONE;
    for (int32 Index:Candidates)
    {
        const auto& Source=MotionRig.Sources[Index];
        if (Source.Action!=TEXT("Attack") || (Source.Airborne>=0 && bool(Source.Airborne)!=Air)
            || (Source.Crouch>=0 && bool(Source.Crouch)!=Crouch) || (Source.Sprint>=0 && bool(Source.Sprint)!=Sprint)
            || Source.Guard==0) continue;
        double Cost=(Source.bMoving!=Move?2.0:0.0)-Source.Priority;
        Cost+=2*(1-FVector2D::DotProduct(Direction,Source.Direction.GetSafeNormal()));
        Cost+=FMath::Abs(Source.Intensity-(Type==EDirectionalAttack::Heavy?0.9:0.25));
        const FTransform* Pelvis=MotionFinalPose.BonesWorld.Find(TEXT("pelvis"));
        if (Pelvis) Cost+=0.2*FMath::Abs(Source.Height-FMath::Clamp((Target.Z-Pelvis->GetLocation().Z)/80.,-1.,1.));
        Cost+=0.04*FMath::Abs(Source.Variant-BodyAttack.ComboIndex%3);
        if (!Source.WeaponFamily.IsNone()) Cost-=4; // explicit family overrides the shared base bank
        if (MotionSolverState.AttackStartRotations.Num()==MotionRig.Names.Num())
        {
            const auto& Frames=MotionRig.Motions[Index].Frames;
            double Fit=TNumericLimits<double>::Max();
            for (int32 Sample=0; Sample<3; ++Sample)
            {
                const int32 Frame=FMath::Clamp(FMath::RoundToInt(Source.LoadTime*Sample*0.5*(Frames.Num()-1)),0,Frames.Num()-1);
                double Error=0;
                for (int32 Bone=0; Bone<MotionRig.Names.Num(); ++Bone)
                    if (MotionRig.StyleBones[Bone])
                    {
                        const double Weight=Bone==MotionRig.Pelvis || MotionRig.Spine.Contains(Bone)?2.0:1.0;
                        Error+=Weight*FMath::Square(Frames[Frame].LocalRotations[Bone].AngularDistance(MotionSolverState.AttackStartRotations[Bone]));
                    }
                Fit=FMath::Min(Fit,Error);
            }
            Cost+=Fit*0.3;
        }
        if (Cost<Best) { Best=Cost; Selected=Index; }
    }
    return Selected;
}

void UDirectionalCombatComponent::EvaluateLibraryGuard(FCombatIntent& Intent,FCombatBodyDrive& Drive)
{
    if (MotionCoreStyle.MotionLibrary.IsNull() || (bCombatActive && GetCombatPhase()!=EDirectionalCombatPhase::Combat)) return;
    const FVector Velocity=GetOwner()->GetActorQuat().UnrotateVector(GetOwner()->GetVelocity());
    const float Speed=Velocity.Size2D();
    bLibraryWalking=Speed>(bLibraryWalking?5:12);
    const bool Moving=bLibraryWalking;
    const auto* Mover=GetOwner()->FindComponentByClass<UMoverComponent>();
    const auto* CharacterMover=Cast<UCharacterMoverComponent>(Mover);
    const bool Air=Mover && !Mover->HasGameplayTag(Mover_IsOnGround,false);
    const bool Crouched=CharacterMover && CharacterMover->IsCrouching();
    const FName Previous=MotionRig.Sources.IsValidIndex(LibraryLoopSource)?MotionRig.Sources[LibraryLoopSource].Action:NAME_None;
    const bool PreviousEnded=MotionRig.Motions.IsValidIndex(LibraryLoopSource) && LibraryLoopTime>=MotionRig.Motions[LibraryLoopSource].Length;
    FName Wanted=bCombatActive?(Moving?TEXT("Walk"):TEXT("Guard")):(Moving?TEXT("CarryWalk"):TEXT("Carry"));
    // Carry animations own the weapon/arms even at running speed; Mover supplies the legs.
    if (Air) Wanted=GetOwner()->GetVelocity().Z>20?TEXT("Jump"):TEXT("Fall");
    else if (Crouched && MotionCoreStyle.bTwoHanded)
    {
        Wanted=TEXT("Crouch");
        if (Previous!=TEXT("Crouch") && (Previous!=TEXT("CrouchStart") || !PreviousEnded)) Wanted=TEXT("CrouchStart");
    }
    else if (Previous==TEXT("Crouch") || Previous==TEXT("CrouchStart") || (Previous==TEXT("CrouchEnd") && !PreviousEnded)) Wanted=TEXT("CrouchEnd");
    else if (!Moving && (Previous==TEXT("Fall") || Previous==TEXT("Jump") || (Previous==TEXT("Land") && !PreviousEnded))) Wanted=TEXT("Land");
    if (!Air && Crouched && !MotionCoreStyle.bTwoHanded) Wanted=bCombatActive?TEXT("Guard"):TEXT("Carry");
    int32 Selected=INDEX_NONE; double Best=TNumericLimits<double>::Max();
    for (int32 I=0; I<MotionRig.Sources.Num(); ++I)
    {
        const auto& S=MotionRig.Sources[I];
        if (!MotionRig.Motions[I].IsValid() || (bCombatActive && S.Guard==0) || S.Action!=Wanted) continue;
        const double Cost=-S.Priority+(Moving?1-FVector2D::DotProduct(FVector2D(Velocity.X,Velocity.Y).GetSafeNormal(),S.Direction.GetSafeNormal()):0)
            -(I==LibraryLoopSource?0.15:0); // hysteresis at directional boundaries
        if (Cost<Best) { Best=Cost; Selected=I; }
    }
    if (MotionRig.Catalog)
        Selected=SelectCatalogAction(Wanted,Moving?FVector2D(Velocity.X,Velocity.Y):FVector2D::ZeroVector,Moving,
            int32(GetTypeHash(EquippedSword.InstanceId)^FCrc::StrCrc32(*Wanted.ToString())));
    if (Selected==INDEX_NONE) return;
    const double Dt=LibraryLastTime>0?FMath::Clamp(Intent.SimulationTime-LibraryLastTime,0.,0.1):0;
    LibraryLastTime=Intent.SimulationTime;
    if (LibraryLoopSource!=Selected)
    {
        const bool Gait=(Wanted==TEXT("Walk") || Wanted==TEXT("CarryWalk") || Wanted==TEXT("Run") || Wanted==TEXT("Sprint"))
            && (Previous==TEXT("Walk") || Previous==TEXT("CarryWalk") || Previous==TEXT("Run") || Previous==TEXT("Sprint"));
        const double Phase=Gait && MotionRig.Motions.IsValidIndex(LibraryLoopSource)?
            FMath::Fmod(LibraryLoopTime/MotionRig.Motions[LibraryLoopSource].Length,1.):0;
        LibraryLoopTime=Phase*MotionRig.Motions[Selected].Length;
        LibraryLoopSource=Selected;
    }
    const bool Gait=Wanted==TEXT("Walk") || Wanted==TEXT("CarryWalk") || Wanted==TEXT("Run") || Wanted==TEXT("Sprint");
    const auto& GaitMotion=MotionRig.Motions[Selected];
    const FVector Travel=Mesh->GetComponentTransform().TransformVector(GaitMotion.Frames.Last().RootTranslation-GaitMotion.Frames[0].RootTranslation);
    const float NativeSpeed=Travel.Size2D()>5?float(Travel.Size2D()/GaitMotion.Length):(Wanted==TEXT("Run") || Wanted==TEXT("Sprint")?350.f:GuardWalkSpeed);
    LibraryLoopTime+=Dt*(Gait?FMath::Clamp(Speed/FMath::Max(20.f,NativeSpeed),0.2f,2.f):1.f);
    const float Alignment=FVector2D::DotProduct(FVector2D(Velocity.X,Velocity.Y).GetSafeNormal(),MotionRig.Sources[Selected].Direction.GetSafeNormal());
    const bool FullBody=Air || Wanted==TEXT("Land") || Wanted.ToString().StartsWith(TEXT("Crouch")) || Wanted==TEXT("Sprint") || (bCombatActive && Gait && Alignment>0.7f);
    const bool OneShot=Wanted==TEXT("Land") || Wanted==TEXT("Jump") || Wanted==TEXT("CrouchStart") || Wanted==TEXT("CrouchEnd");
    Drive.MotionSourceIndex=Selected; Drive.MotionWeight=1; Drive.LegMotionWeight=FullBody || (Moving && bCombatActive)?1:0;
    if (Crouched && !MotionCoreStyle.bTwoHanded) Drive.LegMotionWeight=0;
    Drive.MotionTime=OneShot?FMath::Min(float(LibraryLoopTime)/MotionRig.Motions[Selected].Length,1.f):FMath::Fmod(float(LibraryLoopTime)/MotionRig.Motions[Selected].Length,1.0f);
    Drive.SupportMotionTime=Drive.MotionTime; Drive.AdaptationWeight=bCombatActive?1.f:0.f;
    Drive.MotionAdaptationDegrees=bCombatActive?85.f:0.f;
    if (!CombatBodySolver::SampleMotion(MotionRig,Selected,Drive.MotionTime,Drive.AuthoredLocalPose,Drive.AuthoredComponentPose)) return;
    if (!bCombatActive || (FullBody && Wanted!=TEXT("Walk"))) return;
    const FVector Aim=GetOwner()->GetActorQuat().UnrotateVector(Intent.GuardDirectionWorld);
    const FVector2D Point(FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(Aim.Y,Aim.X))/70.0,-1.0,1.0),
        FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(Aim.Z,Aim.Size2D()))/60.0,-1.0,1.0));
    for (FName Group:{FName(TEXT("GuardAim")),FName(TEXT("LookAim"))})
    {
        TArray<FQuat> Delta; Delta.Init(FQuat::Identity,MotionRig.Names.Num()); float Sum=0;
        for (int32 Index=0; Index<MotionRig.Sources.Num(); ++Index)
        {
            const auto& S=MotionRig.Sources[Index]; const auto& Motion=MotionRig.Motions[Index];
            if (S.Action!=Group || !Motion.bAdditive || !Motion.Sequence.Get()) continue;
            FVector2D Query=Point;
            if (!MotionCoreStyle.bTwoHanded) { if (Group==TEXT("GuardAim")) Query.X=0; else Query.Y=0; }
            const float Weight=1/FMath::Max(0.0025f,float((S.Direction-Query).SizeSquared()));
            for (int32 Bone=0; Bone<Delta.Num(); ++Bone)
            {
                if (!MotionRig.StyleBones[Bone] || Motion.SourceBones[Bone]==INDEX_NONE) continue;
                FTransform Sample; Motion.Sequence->GetBoneTransform(Sample,FSkeletonPoseBoneIndex(Motion.SourceBones[Bone]),FAnimExtractContext(0.0),false);
                Delta[Bone]=FQuat::Slerp(Delta[Bone],Sample.GetRotation(),Weight/(Sum+Weight)).GetNormalized();
            }
            Sum+=Weight;
        }
        if (Sum<=0) continue;
        const auto Before=Drive.AuthoredComponentPose;
        for (int32 Bone=0; Bone<Delta.Num(); ++Bone)
        {
            const int32 Parent=MotionRig.Parents[Bone];
            const FQuat Rotation=(Delta[Bone]*Before[Bone].GetRotation()).GetNormalized();
            if (MotionRig.StyleBones[Bone]) Drive.AuthoredLocalPose[Bone].SetRotation(Parent==INDEX_NONE?Rotation:
                (Drive.AuthoredComponentPose[Parent].GetRotation().Inverse()*Rotation).GetNormalized());
            Drive.AuthoredComponentPose[Bone]=Drive.AuthoredLocalPose[Bone]*(Parent==INDEX_NONE?FTransform::Identity:Drive.AuthoredComponentPose[Parent]);
        }
    }
}

bool CombatBodySolver::ConstrainAnatomy(const FCombatBodyRig& Rig,FCombatFinalPose& Pose)
{
    // Bound the COMPOSED pose, including source transport, aim, IK and contact.
    // A per-layer budget allowed repeated follow-ups to accumulate spinal torsion.
    // Share a total angular budget across the spine, independent of bone count.
    auto Before=Pose.BonesWorld;
    // FTransform composition/relative conversion does not normalize quaternions.
    // In particular, reusing a held pose recursively amplifies norm error in
    // descendant bones. Validate every bone, not just the two weapon grips.
    for (auto& Bone:Before)
    {
        const double Norm=Bone.Value.GetRotation().SizeSquared();
        if (Bone.Value.ContainsNaN() || !FMath::IsFinite(Norm) || Norm<=UE_SMALL_NUMBER) return false;
        Bone.Value.NormalizeRotation();
    }
    double SpineAngle=0;
    for (int32 Bone:Rig.Spine)
    {
        const int32 Parent=Rig.Parents[Bone];
        const FTransform* World=Before.Find(Rig.Names[Bone]);
        const FTransform* ParentWorld=Parent==INDEX_NONE?nullptr:Before.Find(Rig.Names[Parent]);
        if (!World || !ParentWorld || World->ContainsNaN() || ParentWorld->ContainsNaN()) return false;
        SpineAngle+=Rig.ReferenceLocalPose[Bone].GetRotation().AngularDistance(
            World->GetRelativeTransform(*ParentWorld).GetRotation());
    }
    const double SpineFraction=FMath::Min(1.0,FMath::DegreesToRadians(75.0)/FMath::Max(SpineAngle,UE_SMALL_NUMBER));
    for (int32 I=0; I<Rig.Names.Num(); ++I)
    {
        const int32 Parent=Rig.Parents[I];
        const FTransform* World=Before.Find(Rig.Names[I]);
        if (!World || World->ContainsNaN()) return false;
        if (Parent==INDEX_NONE)
        {
            Pose.BonesWorld[Rig.Names[I]]=*World;
            continue;
        }
        const FTransform* ParentWorld=Before.Find(Rig.Names[Parent]);
        if (!ParentWorld) return false;
        FTransform Local=World->GetRelativeTransform(*ParentWorld);
        Local.NormalizeRotation();
        const FString Name=Rig.Names[I].ToString();
        // Wrist limits belong to the grip solver's source-relative envelope.
        // A second absolute clamp here can open an already closed two-hand grip.
        if (Rig.Spine.Contains(I))
        {
            const FQuat Reference=Rig.ReferenceLocalPose[I].GetRotation();
            const double Angle=Reference.AngularDistance(Local.GetRotation());
            const double Fraction=FMath::Min(SpineFraction,FMath::DegreesToRadians(30.0)/FMath::Max(Angle,UE_SMALL_NUMBER));
            Local.SetRotation(FQuat::Slerp(Reference,Local.GetRotation(),Fraction).GetNormalized());
        }
        const float Limit=Name.StartsWith(TEXT("neck"))?35.f:Name==TEXT("head")?55.f:0.f;
        if (Limit>0)
        {
            const FQuat Reference=Rig.ReferenceLocalPose[I].GetRotation();
            const float Angle=Reference.AngularDistance(Local.GetRotation());
            if (Angle>FMath::DegreesToRadians(Limit))
                Local.SetRotation(FQuat::Slerp(Reference,Local.GetRotation(),FMath::DegreesToRadians(Limit)/Angle).GetNormalized());
        }
        Pose.BonesWorld[Rig.Names[I]]=Local*Pose.BonesWorld.FindChecked(Rig.Names[Parent]);
        Pose.BonesWorld[Rig.Names[I]].NormalizeRotation();
    }
    // Consumers must use the corrected body, never cached pre-constraint grips.
    Pose.PrimaryGripWorld=Rig.GripLocal*Pose.BonesWorld.FindChecked(Rig.Names[Rig.GripBone]);
    Pose.PrimaryGripWorld.NormalizeRotation();
    Pose.PrimaryGripWorld.SetScale3D(FVector::OneVector);
    if (Pose.bHasSecondaryGrip && Rig.SupportGripBone!=INDEX_NONE)
    {
        Pose.SecondaryGripWorld=Rig.SupportGripLocal*Pose.BonesWorld.FindChecked(Rig.Names[Rig.SupportGripBone]);
        Pose.SecondaryGripWorld.NormalizeRotation();
        Pose.SecondaryGripWorld.SetScale3D(FVector::OneVector);
    }
    return true;
}
