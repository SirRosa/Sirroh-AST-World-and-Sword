#include "Combat/DirectionalCombatComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Sword/ProceduralSword.h"

namespace
{
    float Smooth(float T) { T=FMath::Clamp(T,0.0f,1.0f); return T*T*(3-2*T); }
    FVector Curve(const FVector& A,const FVector& B,const FVector& C,const FVector& D,float T)
    {
        const float S=1-T;
        return A*(S*S*S)+B*(3*S*S*T)+C*(3*S*T*T)+D*(T*T*T);
    }
}

void UDirectionalCombatComponent::PlanDirectionalAttack(const FCombatIntent& Intent,FCombatBodyDrive& Drive) const
{
    const auto Weapon=GetEquippedWeaponGeometry();
    const FQuat Frame=GetOwner()->GetActorQuat();
    const FVector Up=Frame.GetAxisZ();
    const bool bThrust=BodyAttack.Type==EDirectionalAttack::Pierce;
    const bool bHeavy=BodyAttack.Type==EDirectionalAttack::Heavy;
    const bool bSource=MotionRig.Motions.IsValidIndex(Drive.MotionSourceIndex)
        && MotionRig.Motions[Drive.MotionSourceIndex].IsValid();
    FVector Target=Intent.AttackTarget;
    // Freeze the aiming reference at admission. Moving the sword during wind-up must
    // not change an upward command into a downward command on the next frame.
    FVector Origin=BodyAttack.StartOrigin;
    Origin.Z=bSource?BodyAttack.NeutralTargetWorld.Z:BodyAttack.StartWorkingPoint.Z;
    if (BodyAttack.bChained && !bThrust && FVector::DistSquared(Target,BodyAttack.StartWorkingPoint)<FMath::Square(FMath::Max(15.f,Weapon.WeaponReach*0.2f)))
    {
        // A target already occupied by the blade requests a continuation, not a
        // zero-length curve. Mirror its transverse position about the body.
        const FTransform* Pelvis=MotionFinalPose.BonesWorld.Find(TEXT("pelvis"));
        const FVector Centre=Pelvis?Pelvis->GetLocation():Origin;
        FVector From=Frame.UnrotateVector(BodyAttack.StartWorkingPoint-Centre);
        FVector2D Sweep(-From.Y,-From.Z);
        if (!Sweep.Normalize()) Sweep=FVector2D(0,1);
        const double Radius=FMath::Clamp(FVector2D(From.Y,From.Z).Size(),35.,100.);
        Target=Centre+Frame.RotateVector(FVector(FMath::Max(45.,From.X),Sweep.X*Radius,Sweep.Y*Radius));
    }
    FVector Local=Frame.UnrotateVector(Target-Origin);
    Local.X=FMath::Max(30.0,Local.X);
    Local.Y=FMath::Clamp(Local.Y,-Local.X*1.73,Local.X*1.73);
    Local.Z=FMath::Clamp(Local.Z,-Local.Size2D()*1.73,Local.Size2D()*1.73); // continuous front hemisphere, no direction buckets
    Target=Origin+Frame.RotateVector(Local);
    const double Horizontal=FVector2D(Local.X,Local.Y).Size();
    const double Elevation=FMath::Atan2(Local.Z,FMath::Max(0.001,Horizontal));
    const FVector Forward=Frame.RotateVector(FVector(Local.X,Local.Y,0).GetSafeNormal(UE_SMALL_NUMBER,FVector::ForwardVector));
    const FVector Travel=(Forward*FMath::Cos(Elevation)+Up*FMath::Sin(Elevation)).GetSafeNormal();
    const FVector Axis=FVector::CrossProduct(Forward,Up).GetSafeNormal();
    const FVector BowDirection=FVector::CrossProduct(Axis,Travel).GetSafeNormal();
    const float Span=FMath::Clamp(Weapon.WeaponReach*(bThrust?0.30f:bHeavy?0.95f:0.45f)*
        FMath::Clamp(Weapon.WeaponAngle,0.35f,2.0f),12.0f,150.0f);
    const FVector BrakeTravel=(BodyAttack.StartWorkingVelocity*(BodyAttack.Preparation*0.5f)).GetClampedToMaxSize(Span*0.4f);
    const FVector Load=BodyAttack.bChained?BodyAttack.StartWorkingPoint+BrakeTravel:Target-Travel*Span;
    const float Bow=bThrust?0:bHeavy?Span*0.55f:Span*0.14f;
    const FVector EntryTangent=BodyAttack.bChained?
        (BodyAttack.StartWorkingVelocity*0.10).GetClampedToMaxSize(Span*0.5f):Travel*(Span*0.25f);
    const FVector ControlA=Load+EntryTangent+BowDirection*Bow;
    const FVector ControlB=Target-Travel*(Span*0.25f);
    const float Preparation=Smooth(float((Intent.SimulationTime-BodyAttack.PreparedAt)/FMath::Max(0.001f,BodyAttack.Preparation)));
    const bool bStriking=BodyAttack.bReleased && Intent.SimulationTime>=BodyAttack.StrikeAt;
    float U=bStriking?FMath::Clamp(float((Intent.SimulationTime-BodyAttack.StrikeAt)/FMath::Max(0.001f,BodyAttack.Duration))/0.7f,0.0f,1.428571f):0;
    float DirectionWeight=1,RangeWeight=1,FamilyWeight=1;
    if (bSource)
    {
        const auto& Source=MotionRig.Sources[Drive.MotionSourceIndex];
        const auto& Frames=MotionRig.Motions[Drive.MotionSourceIndex].Frames;
        DirectionWeight=Smooth(float(FMath::Abs(Elevation)/FMath::DegreesToRadians(45.0)));
        const float NeutralRange=FMath::Max(1.0f,float(FVector::VectorPlaneProject(BodyAttack.NeutralTargetWorld-Origin,Up).Size()));
        RangeWeight=FMath::Clamp(float(FMath::Abs(Horizontal-NeutralRange)/NeutralRange),0.0f,1.0f);
        FamilyWeight=Source.NativeFamily==BodyAttack.Type?0.0f:1.0f;
        // Source arc length supplies nonuniform acceleration, rather than linear time
        // or a constant-speed hand interpolator. Geometry is that of this actual weapon.
        const auto Point=[&](float Time)
        {
            const float P=FMath::Clamp(Time,0.0f,1.0f)*(Frames.Num()-1);
            const int32 I=FMath::FloorToInt(P);
            FTransform Grip; Grip.Blend(Frames[I].Grip,Frames[FMath::Min(I+1,Frames.Num()-1)].Grip,P-I);
            return (Weapon.PrimaryGripLocal.Inverse()*Grip).TransformPosition(
                FMath::Lerp(Weapon.BladeBaseLocal,Weapon.BladeTipLocal,bThrust?1.0:0.75));
        };
        double Total=0,Passed=0;
        FVector Previous=Point(Source.LoadTime);
        for (int32 I=1; I<=32; ++I)
        {
            const float A=FMath::Lerp(Source.LoadTime,Source.ContactTime,(I-1)/32.0f);
            const float B=FMath::Lerp(Source.LoadTime,Source.ContactTime,I/32.0f);
            const FVector Next=Point(B); const double Distance=FVector::Distance(Previous,Next);
            Total+=Distance; Passed+=Distance*FMath::Clamp((Drive.MotionTime-A)/FMath::Max(0.0001f,B-A),0.0f,1.0f);
            Previous=Next;
        }
        U=Drive.MotionTime<=Source.ContactTime?
            (Total>0.001?float(Passed/Total):FMath::Clamp((Drive.MotionTime-Source.LoadTime)/(Source.ContactTime-Source.LoadTime),0.0f,1.0f)):
            1+0.428571f*(Drive.MotionTime-Source.ContactTime)/(Source.FinishTime-Source.ContactTime);
    }
    Drive.bTrajectoryPlan=true;
    Drive.LoadPoint=Load;
    Drive.SwingAxis=Axis;
    Drive.StrikeDirection=Travel;
    Drive.AdaptationWeight=FMath::Max(FMath::Max3(DirectionWeight,RangeWeight,FamilyWeight),1-Drive.MovementFulfillment);
    if (BodyAttack.bChained) Drive.AdaptationWeight=1; // Actual start remains the trajectory origin throughout the continuation.
    const FVector Finish=Target+Travel*(Span*(bThrust?0.18f:bHeavy?0.55f:0.3f));
    const float BrakeTime=FMath::Clamp(float((Intent.SimulationTime-BodyAttack.PreparedAt)/FMath::Max(0.001f,BodyAttack.Preparation)),0.0f,1.0f);
    const FVector PreparationGoal=BodyAttack.bChained?BodyAttack.StartWorkingPoint+BrakeTravel*(2*BrakeTime-BrakeTime*BrakeTime):
        FMath::Lerp(BodyAttack.StartWorkingPoint,Load,Preparation);
    Drive.WorkingPointGoal=!bStriking?PreparationGoal:
        U<=1?Curve(Load,ControlA,ControlB,Target,U):FMath::Lerp(Target,Finish,FMath::Clamp((U-1)/0.428571f,0.0f,1.0f));
    if (!bStriking) Drive.WorkingTravelDirection=(Load-BodyAttack.StartWorkingPoint).GetSafeNormal(UE_SMALL_NUMBER,Travel);
    else if (U<1)
    {
        const float S=1-U;
        Drive.WorkingTravelDirection=((ControlA-Load)*(S*S)+(ControlB-ControlA)*(2*S*U)+(Target-ControlB)*(U*U)).GetSafeNormal(UE_SMALL_NUMBER,Travel);
    }
    else Drive.WorkingTravelDirection=Travel;
    Drive.WorkingPointWeight=Drive.Weight*Drive.AdaptationWeight*(1-Drive.RecoveryAlpha);
    // Multi-strike clips already contain their internal reversals. Do not pull
    // every strike toward the end of the first cubic; retain source biomechanics.
    if (bSource && MotionRig.Sources[Drive.MotionSourceIndex].StrikeWindows.Num()>1)
        Drive.WorkingPointWeight=0;

    const FTransform* Shoulder=MotionFinalPose.BonesWorld.Find(TEXT("upperarm_r"));
    const FVector BodyOrigin=Shoulder?Shoulder->GetLocation():Origin;
    Drive.Direction=(Drive.WorkingPointGoal-BodyOrigin).GetSafeNormal(UE_SMALL_NUMBER,Forward);
    Drive.CoreRotation=FRotator::ZeroRotator;
    if (bSource && Drive.AuthoredComponentPose.IsValidIndex(MotionRig.GripBone))
    {
        const FTransform MeshWorld=Mesh->GetComponentTransform();
        const FTransform Grip=MotionRig.GripLocal*Drive.AuthoredComponentPose[MotionRig.GripBone]*MeshWorld;
        const FTransform WeaponWorld=Weapon.PrimaryGripLocal.Inverse()*Grip;
        const FVector Working=WeaponWorld.TransformPosition(FMath::Lerp(Weapon.BladeBaseLocal,Weapon.BladeTipLocal,bThrust?1.0:0.75));
        const FVector SourceShoulder=MeshWorld.TransformPosition(Drive.AuthoredComponentPose[MotionRig.Shoulder].GetLocation());
        const FVector SourceHand=MeshWorld.TransformPosition(Drive.AuthoredComponentPose[MotionRig.Hand].GetLocation());
        if (MotionRig.Sources[Drive.MotionSourceIndex].StrikeWindows.Num()>1)
        {
            // Rotate the whole authored sequence toward the intent, preserving
            // every internal reversal instead of fitting it to one cubic swing.
            FQuat Guide=FQuat::FindBetweenNormals((BodyAttack.NeutralTargetWorld-Origin).GetSafeNormal(UE_SMALL_NUMBER,Forward),
                (Target-Origin).GetSafeNormal(UE_SMALL_NUMBER,Forward));
            const double Angle=Guide.AngularDistance(FQuat::Identity);
            const double Limit=FMath::DegreesToRadians(MotionRig.Sources[Drive.MotionSourceIndex].AdaptationDegrees);
            if (Angle>Limit && Angle>UE_SMALL_NUMBER) Guide=FQuat::Slerp(FQuat::Identity,Guide,Limit/Angle).GetNormalized();
            Drive.WorkingPointGoal=SourceShoulder+Guide.RotateVector(Working-SourceShoulder);
        }
        const FQuat Turn=FQuat::FindBetweenNormals((Working-SourceShoulder).GetSafeNormal(UE_SMALL_NUMBER,Forward),
            (Drive.WorkingPointGoal-SourceShoulder).GetSafeNormal(UE_SMALL_NUMBER,Forward));
        Drive.Direction=Turn.RotateVector((SourceHand-SourceShoulder).GetSafeNormal(UE_SMALL_NUMBER,Forward));
        const FQuat CurrentFrame=GetOwner()->GetActorQuat();
        Drive.TrajectoryCorrection=(CurrentFrame.Inverse()*Turn*CurrentFrame).Rotator();
        // Range is independent of direction; no arm extension beyond the anatomical budget.
        const double Length=FVector::Distance(Drive.AuthoredComponentPose[MotionRig.Shoulder].GetLocation(),Drive.AuthoredComponentPose[MotionRig.Elbow].GetLocation())+
            FVector::Distance(Drive.AuthoredComponentPose[MotionRig.Elbow].GetLocation(),Drive.AuthoredComponentPose[MotionRig.Hand].GetLocation());
        const float SourceExtension=float(FVector::Distance(SourceHand,SourceShoulder)/FMath::Max(1.0,Length*FMath::Abs(MeshWorld.GetScale3D().X)));
        Drive.Extension=FMath::Lerp(SourceExtension,Drive.Extension,RangeWeight);
    }
    else Drive.TrajectoryCorrection=Frame.UnrotateVector(Drive.Direction).Rotation();
}
