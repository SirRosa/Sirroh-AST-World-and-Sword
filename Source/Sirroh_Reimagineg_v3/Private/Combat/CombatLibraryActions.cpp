#include "Combat/DirectionalCombatComponent.h"
#include "CombatInertiaMove.h"
#include "Components/SkeletalMeshComponent.h"
#include "MoverComponent.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "GameFramework/Actor.h"

bool UDirectionalCombatComponent::IsLibraryActionPlaying() const
{
    return MotionRig.Motions.IsValidIndex(LibraryActionSource) && MotionRig.Motions[LibraryActionSource].IsValid()
        && CombatTime()>=LibraryActionStart && CombatTime()<LibraryActionStart+MotionRig.Motions[LibraryActionSource].Length;
}

void UDirectionalCombatComponent::RequestLibraryAction(FName Action,FVector Direction)
{
    if (!IsLocal() || !bUseMotionCoreV3 || !bCombatActive) return;
    if (Direction.IsNearlyZero()) Direction=GetOwner()->GetVelocity().GetSafeNormal2D();
    if (Direction.IsNearlyZero()) Direction=GetOwner()->GetActorForwardVector();
    ServerLibraryAction(Action,Direction.GetSafeNormal());
}

void UDirectionalCombatComponent::ServerLibraryAction_Implementation(FName Action,FVector_NetQuantizeNormal Direction)
{
    if (!CanFight() || !bUseMotionCoreV3 || (!bCombatActive && Action!=TEXT("Holster")) || !bMotionRigReady || Reaction.bDisarmed || FVector(Direction).ContainsNaN()
        || CombatTime()<MotionReaction.GuardAt || (IsLibraryActionPlaying() && Action!=TEXT("Holster") && Action!=TEXT("Equip"))) return;
    if (Action!=TEXT("Dodge") && Action!=TEXT("Taunt") && Action!=TEXT("Kick") && Action!=TEXT("Equip") && Action!=TEXT("Holster") && Action!=TEXT("Turn")) return;
    auto* Mover=GetOwner()->FindComponentByClass<UMoverComponent>();
    if (Action!=TEXT("Taunt") && Mover && !Mover->HasGameplayTag(Mover_IsOnGround,false)) return;
    const auto* CharacterMover=Cast<UCharacterMoverComponent>(Mover);
    if (Action==TEXT("Dodge") && ((CharacterMover && CharacterMover->IsCrouching()) || GetOwner()->GetVelocity().Size2D()>350)) Action=TEXT("Roll");
    if (Action==TEXT("Dodge") || Action==TEXT("Roll"))
        Direction=FVector(FVector(Direction).X<0?-1:1,0,0); // gameplay plane: never roll into another terrain lane
    const FVector Local=GetOwner()->GetActorQuat().UnrotateVector(Direction);
    double Best=TNumericLimits<double>::Max(); int32 Selected=INDEX_NONE;
    for (int32 I=0; I<MotionRig.Sources.Num(); ++I)
    {
        const auto& S=MotionRig.Sources[I];
        if (S.Action!=Action || !MotionRig.Motions[I].IsValid()) continue;
        double Cost=-S.Priority;
        if (Action==TEXT("Dodge") || Action==TEXT("Roll"))
            Cost+=2*(1-FVector2D::DotProduct(FVector2D(Local.X,Local.Y).GetSafeNormal(),S.Direction.GetSafeNormal()));
        else if (Action==TEXT("Turn")) Cost+=S.Direction.Y*(Local.Y<0?1:-1);
        else if (Action==TEXT("Taunt")) Cost+=FMath::FRand(); // chosen once on authority, replicated index
        else if (Action==TEXT("Kick")) Cost+=S.bMoving!=(GetOwner()->GetVelocity().Size2D()>20)?1:0;
        if (Cost<Best) { Best=Cost; Selected=I; }
    }
    if (MotionRig.Catalog)
        Selected=SelectCatalogAction(Action,(Action==TEXT("Dodge") || Action==TEXT("Roll") || Action==TEXT("Turn"))?
            FVector2D(Local.X,Local.Y):FVector2D::ZeroVector,GetOwner()->GetVelocity().Size2D()>20,int32(LibraryActionSerial+1));
    if (Selected==INDEX_NONE) return;
    BodyAttack.bPrepared=false; if (Action!=TEXT("Turn")) BufferedBodyAttack=FBufferedBodyAttack();
    CancelInertiaMovement(); bMotionSubmittedAttackActive=false;
    LibraryActionSource=Selected; LibraryActionVariantId=MotionRig.Sources[Selected].CatalogVariantId;
    LibraryActionStart=CombatTime(); ++LibraryActionSerial;
    LibraryActionDirection=Direction;
    const auto& Motion=MotionRig.Motions[Selected];
    if (Mover && Mover->HasGameplayTag(Mover_IsOnGround,false) && Action!=TEXT("Taunt") && Action!=TEXT("Equip") && Action!=TEXT("Holster") && Action!=TEXT("Turn"))
    {
        auto Move=MakeShared<FCombatInertiaMove>();
        Move->GroundMode=Mover->GetMovementModeName(); Move->bLocalPath=true;
        Move->TravelMs=Motion.Length*1000; Move->DurationMs=Move->TravelMs;
        const FVector Start=Motion.Frames[0].RootTranslation;
        const FVector SourceTravel=Mesh->GetComponentTransform().TransformVector(Motion.Frames.Last().RootTranslation-Start).GetSafeNormal2D();
        const bool Evade=Action==TEXT("Dodge") || Action==TEXT("Roll");
        Move->bLocalPath=!Evade;
        const FQuat Alignment=Evade && !SourceTravel.IsNearlyZero()?FQuat::FindBetweenNormals(SourceTravel,FVector(Direction)):FQuat::Identity;
        bool Travel=false;
        for (const auto& Frame:Motion.Frames)
        {
            FVector Point=Mesh->GetComponentTransform().TransformVector(Frame.RootTranslation-Start);
            Point=Alignment.RotateVector(Point); Point.Z=0;
            if (Evade) Point.Y=0;
            Point=Point.GetClampedToMaxSize(400);
            if (!Evade) Point=GetOwner()->GetActorQuat().UnrotateVector(Point);
            Move->MotionPath.Add(Point); Travel|=!Point.IsNearlyZero();
        }
        if (Travel)
        {
            int32 End=Move->MotionPath.Num()-1;
            while (End>1 && FVector::DistSquared(Move->MotionPath[End],Move->MotionPath[End-1])<0.0025) --End;
            Move->TravelMs*=float(End)/(Move->MotionPath.Num()-1);
            Move->DurationMs=Move->TravelMs;
            Move->MotionPath.SetNum(End+1);
            Mover->QueueLayeredMove(Move);
        }
    }
    GetOwner()->ForceNetUpdate();
}

bool UDirectionalCombatComponent::EvaluateLibraryAction(FCombatIntent& Intent,FCombatBodyDrive& Drive)
{
    if (!IsLibraryActionPlaying()) return false;
    const float Time=FMath::Clamp(float((CombatTime()-LibraryActionStart)/MotionRig.Motions[LibraryActionSource].Length),0.f,1.f);
    Drive=FCombatBodyDrive();
    Drive.MotionSourceIndex=LibraryActionSource; Drive.MotionTime=Time; Drive.SupportMotionTime=Time;
    Drive.MotionWeight=1; Drive.LegMotionWeight=1; Drive.AdaptationWeight=0;
    const FName Action=MotionRig.Sources[LibraryActionSource].Action;
    if ((Action==TEXT("Taunt") || Action==TEXT("Equip") || Action==TEXT("Holster") || Time>0.8f)
        && GetOwner()->GetVelocity().Size2D()>8) Drive.LegMotionWeight=0;
    Drive.MotionAdaptationDegrees=0;
    Intent.Phase=ECombatMotionPhase::Combat; Intent.AttackId=0;
    if (!CombatBodySolver::SampleMotion(MotionRig,LibraryActionSource,Time,Drive.AuthoredLocalPose,Drive.AuthoredComponentPose)) return false;
    if (Action==TEXT("Turn"))
    {
        Drive.AuthoredLocalPose[0].SetRotation(MotionRig.ReferenceLocalPose[0].GetRotation());
        for (int32 I=0; I<MotionRig.Names.Num(); ++I)
            Drive.AuthoredComponentPose[I]=Drive.AuthoredLocalPose[I]*(MotionRig.Parents[I]==INDEX_NONE?FTransform::Identity:Drive.AuthoredComponentPose[MotionRig.Parents[I]]);
    }
    if (Action==TEXT("Dodge") || Action==TEXT("Roll"))
    {
        const auto& Frames=MotionRig.Motions[LibraryActionSource].Frames;
        const FVector Travel=Mesh->GetComponentTransform().TransformVector(Frames.Last().RootTranslation-Frames[0].RootTranslation).GetSafeNormal2D();
        if (!Travel.IsNearlyZero())
        {
            const FQuat MeshFrame=Mesh->GetComponentQuat();
            const FQuat Turn=MeshFrame.Inverse()*FQuat::FindBetweenNormals(Travel,FVector(LibraryActionDirection))*MeshFrame;
            Drive.AuthoredLocalPose[0].SetRotation((Turn*Drive.AuthoredLocalPose[0].GetRotation()).GetNormalized());
            for (int32 I=0; I<MotionRig.Names.Num(); ++I)
                Drive.AuthoredComponentPose[I]=Drive.AuthoredLocalPose[I]*(MotionRig.Parents[I]==INDEX_NONE?FTransform::Identity:Drive.AuthoredComponentPose[MotionRig.Parents[I]]);
        }
    }
    return true;
}
