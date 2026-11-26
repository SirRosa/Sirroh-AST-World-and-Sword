#include "CombatMotionContactSubsystem.h"
#include "POI/POICorridorTools.h"
#include "Combat/CombatVitalityComponent.h"
#include "Combat/DirectionalCombatComponent.h"
#include "Character/SirrohPoseBridgeComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Sword/ProceduralSword.h"
#include "Sword/SwordWorldSubsystem.h"
#include "Procedural_World_Generator/PWGProceduralTree.h"
#include "Combat/CombatDefenseProjectile.h"
#include "GameFramework/ProjectileMovementComponent.h"

namespace
{
    // Rigid interpolation of MEASURED endpoints. No planned attack trajectory is consulted.
    void BladeAt(const FCombatWeaponSample& A,const FCombatWeaponSample& B,double T,FVector& Base,FVector& Tip)
    {
        const FTransform Frame(FQuat::Slerp(A.WeaponWorld.GetRotation(),B.WeaponWorld.GetRotation(),T).GetNormalized(),
            FMath::Lerp(A.WeaponWorld.GetLocation(),B.WeaponWorld.GetLocation(),T));
        Base=Frame.TransformPosition(A.WeaponWorld.InverseTransformPosition(A.BladeBase));
        Tip=Frame.TransformPosition(A.WeaponWorld.InverseTransformPosition(A.BladeTip));
    }
    double TravelBound(const FCombatWeaponSample& A,const FCombatWeaponSample& B)
    {
        const double Radius=FMath::Max(FVector::Distance(A.BladeBase,A.WeaponWorld.GetLocation()),FVector::Distance(A.BladeTip,A.WeaponWorld.GetLocation()));
        return FVector::Distance(A.WeaponWorld.GetLocation(),B.WeaponWorld.GetLocation())+
            Radius*A.WeaponWorld.GetRotation().AngularDistance(B.WeaponWorld.GetRotation());
    }
    bool Sweep(const FCombatWeaponSample& A,const FCombatWeaponSample& B,
        TFunctionRef<void(double,FVector&,FVector&)> Other,double OtherTravel,double Radius,double& Time,FVector& Point)
    {
        const double Speed=TravelBound(A,B)+OtherTravel;
        Time=0;
        for (int32 Iteration=0; Iteration<96; ++Iteration)
        {
            FVector BA,BT,OA,OB,P,Q;
            BladeAt(A,B,Time,BA,BT); Other(Time,OA,OB);
            FMath::SegmentDistToSegmentSafe(BA,BT,OA,OB,P,Q);
            const double Gap=FVector::Distance(P,Q)-Radius;
            Point=(P+Q)*0.5;
            if (Gap<=0.02) return true;
            if (Speed<=UE_SMALL_NUMBER || Gap>Speed*(1-Time)) return false;
            Time+=FMath::Max(Gap/Speed,1e-7);
            if (Time>1) return false;
        }
        // Do not invent a hit when grazing convergence is inconclusive.
        return false;
    }
    bool Continuous(const FCombatMotionContactSnapshot& A,const FCombatMotionContactSnapshot& B)
    {
        return A.LifeId==B.LifeId && B.Time>A.Time && B.Time-A.Time<=0.1 && A.Root.GetScale3D().Equals(B.Root.GetScale3D()) &&
            FVector::DistSquared(A.Root.GetLocation(),B.Root.GetLocation())<FMath::Square(150.0) &&
            A.Root.GetRotation().AngularDistance(B.Root.GetRotation())<FMath::DegreesToRadians(120.0);
    }
}

void UCombatMotionContactSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    TickHandle=FWorldDelegates::OnWorldPostActorTick.AddUObject(this,&UCombatMotionContactSubsystem::AfterActors);
}
void UCombatMotionContactSubsystem::Deinitialize()
{
    FWorldDelegates::OnWorldPostActorTick.Remove(TickHandle);
    Tracks.Reset(); Super::Deinitialize();
}
void UCombatMotionContactSubsystem::RegisterCombatant(UDirectionalCombatComponent* Combat)
{
    if (!Combat || !Combat->UsesMotionCoreV3() || !Combat->GetOwner()->HasAuthority()) return;
    for (const auto& Track:Tracks) if (Track.Combat==Combat) return;
    Tracks.AddDefaulted_GetRef().Combat=Combat;
}
void UCombatMotionContactSubsystem::UnregisterCombatant(UDirectionalCombatComponent* Combat)
{
    for (auto& Track:Tracks) if (Track.Combat==Combat) Track.Combat.Reset();
}
void UCombatMotionContactSubsystem::RegisterProjectile(ACombatDefenseProjectile* P)
{
    if (!P || !P->HasAuthority()) return;
    Projectiles.AddUnique(P);
    const double Now=GetWorld()->GetTimeSeconds();
    FCombatDefenseThreat T; T.Id=P->ThreatId; T.Attacker=P->GetOwner()?P->GetOwner():P; T.Projectile=P;
    T.Origin=P->GetActorLocation(); T.Velocity=P->Movement->Velocity; T.Acceleration=FVector(0,0,GetWorld()->GetGravityZ()*P->Movement->ProjectileGravityScale);
    T.StartTime=Now; T.ExpireTime=Now+P->Lifetime; T.bPierce=P->bPierce; T.bRanged=true;
    for (auto& Track:Tracks) if (auto* D=Track.Combat.Get(); D && D->GetOwner()!=T.Attacker && D->UsesIndicatorDefense())
    {
        const auto F=D->GetDefenseFrame();
        FVector Previous=T.Origin;
        for (double Time=Now+0.025;Time<=T.ExpireTime+0.001;Time+=0.025)
        {
            const FVector Current=T.Position(Time);
            const FVector Closest=FMath::ClosestPointOnSegment(F.Centre,Previous,Current);
            const FVector Delta=Closest-F.Centre;
            if (FMath::Abs(Delta.Y)<=F.HalfDepth && FVector2D(Delta.X,Delta.Z).SizeSquared()<=FMath::Square(F.Radius))
            { T.PredictedContactTime=Time; D->AddDefenseThreat(T); break; }
            Previous=Current;
        }
    }
}
void UCombatMotionContactSubsystem::QueueProjectileHit(ACombatDefenseProjectile* P,const FCombatMotionBodyHit& Hit)
{ ProjectileHits.Add({P,Hit}); }
void UCombatMotionContactSubsystem::CancelProjectile(ACombatDefenseProjectile* P)
{
    if (!P) return;
    for (auto& Track:Tracks) if (auto* D=Track.Combat.Get())
        if (const auto* R=D->DefenseResolver.FindResult(P->ThreatId); R && R->Outcome==ECombatDefenseOutcome::Pending)
        { D->DefenseResolver.Cancel(P->ThreatId,GetWorld()->GetTimeSeconds()); D->HandleDefenseResult(*D->DefenseResolver.FindResult(P->ThreatId)); }
}

void UCombatMotionContactSubsystem::CommitAttack(UDirectionalCombatComponent* Combat)
{
    if (!Combat || !Combat->GetOwner()->HasAuthority() || !Combat->UsesIndicatorDefense()) return;
    const auto& Attack=Combat->BodyAttack; if (!Attack.bPrepared || !Attack.bReleased) return;
    const double Now=Combat->CombatTime();
    TArray<double> ContactTimes;
    TArray<float> ContactPhases;
    if (Combat->MotionRig.Sources.IsValidIndex(Attack.MotionSourceIndex))
    {
        const auto& Source=Combat->MotionRig.Sources[Attack.MotionSourceIndex];
        const double Length=Combat->MotionRig.Motions[Attack.MotionSourceIndex].Length;
        for (int32 I=0;I<Source.StrikeWindows.Num();++I)
        {
            const auto& Window=Source.StrikeWindows[I];
            // The beginning of a damage window is not its contact pose (often still the load).
            const float Contact=(Window.X+Window.Y)*0.5f;
            ContactTimes.Add(Attack.StrikeAt+(FMath::Max(double(Contact),double(Attack.SourceStrikeTime))-Attack.SourceStrikeTime)*Length);
            ContactPhases.Add(Contact);
        }
        if (ContactTimes.IsEmpty())
        { ContactTimes.Add(Attack.StrikeAt+(Source.ContactTime-Attack.SourceStrikeTime)*Length); ContactPhases.Add(Source.ContactTime); }
    }
    if (ContactTimes.IsEmpty()) { ContactTimes.Add(Attack.StrikeAt+Attack.Duration*0.45); ContactPhases.Add(0.5f); }
    const auto Weapon=Combat->GetEquippedWeaponGeometry();
    for (int32 Phase=0;Phase<ContactTimes.Num();++Phase)
    {
        const FGuid Id=FGuid::NewGuid();
        FVector BladeTravel=FVector::ZeroVector;
        if (Combat->MotionRig.Motions.IsValidIndex(Attack.MotionSourceIndex) && Combat->Mesh)
        {
            const auto& Motion=Combat->MotionRig.Motions[Attack.MotionSourceIndex];
            const auto& Frames=Motion.Frames;
            const int32 B=FMath::Clamp(FMath::RoundToInt(ContactPhases[Phase]*(Frames.Num()-1)),1,Frames.Num()-1);
            const FVector Working=FMath::Lerp(Weapon.BladeBaseLocal,Weapon.BladeTipLocal,Attack.Type==EDirectionalAttack::Pierce?1.0:0.75);
            const auto Point=[&](int32 I)
            { return (Weapon.PrimaryGripLocal.Inverse()*Frames[I].Grip*Combat->Mesh->GetComponentTransform()).TransformPosition(Working); };
            BladeTravel=Point(FMath::Min(B+1,Frames.Num()-1))-Point(B-1);
            const FVector Neutral=(FVector(Attack.NeutralTargetWorld)-Attack.StartOrigin).GetSafeNormal();
            const FVector Requested=(FVector(Attack.Target)-Attack.StartOrigin).GetSafeNormal();
            if (!Neutral.IsNearlyZero() && !Requested.IsNearlyZero())
            {
                FQuat Guide=FQuat::FindBetweenNormals(Neutral,Requested);
                const double Angle=Guide.AngularDistance(FQuat::Identity);
                const double Limit=FMath::DegreesToRadians(Combat->MotionRig.Sources[Attack.MotionSourceIndex].AdaptationDegrees);
                if (Angle>Limit && Angle>UE_SMALL_NUMBER) Guide=FQuat::Slerp(FQuat::Identity,Guide,Limit/Angle).GetNormalized();
                BladeTravel=Guide.RotateVector(BladeTravel);
            }
            BladeTravel.Y=0;
        }
        for (const auto& Track:Tracks)
        {
            auto* D=Track.Combat.Get(); if (!D || D==Combat || !D->UsesIndicatorDefense() || !D->CanFight()) continue;
            const auto Frame=D->GetDefenseFrame();
            const FVector Target=FMath::ClosestPointOnSegment(Frame.Centre,Attack.StartWorkingPoint,FVector(Attack.Target));
            if (FVector::DistSquared(Frame.Centre,Target)>FMath::Square(Frame.Radius+80.f)
                || FVector::DistSquared(Frame.Centre,Combat->GetOwner()->GetActorLocation())>FMath::Square(Weapon.WeaponReach+Attack.ApproachDisplacement.Size()+220.f)) continue;
            FVector Travel=Target-Attack.StartWorkingPoint; Travel.Y=0;
            if (Attack.Type!=EDirectionalAttack::Pierce && !BladeTravel.IsNearlyZero()) Travel=BladeTravel;
            Travel=Travel.GetSafeNormal(UE_SMALL_NUMBER,FVector(FMath::Sign(Target.X-Combat->GetOwner()->GetActorLocation().X),0,0));
            FCombatDefenseThreat T; T.Id=Id; T.Attacker=Combat->GetOwner(); T.WeaponId=Attack.WeaponId;
            T.AttackId=Attack.Id; T.StrikePhase=Phase; T.Epoch=Combat->MotionEpoch; T.bPierce=Attack.Type==EDirectionalAttack::Pierce;
            T.StartTime=Now; T.PredictedContactTime=FMath::Max(Now+0.02,ContactTimes[Phase]);
            // Forecast is committed once, in world space. It never follows a dodging target.
            FVector Offset=Target-Frame.Centre; Offset.Y=0;
            const double Along=FVector::DotProduct(Offset,Travel);
            const double PerpendicularSq=FMath::Max(0.,Offset.SizeSquared()-Along*Along);
            if (PerpendicularSq>FMath::Square(Frame.Radius)) continue;
            const FVector Entry=Target-Travel*(Along+FMath::Sqrt(FMath::Square(Frame.Radius)-PerpendicularSq));
            constexpr double Lead=40.; // Short incoming cue outside the guard, not a projectile fired by the attacker.
            T.Origin=Entry-Travel*Lead; T.Velocity=Travel*Lead/(T.PredictedContactTime-Now);
            T.ExpireTime=T.PredictedContactTime+FMath::Max(0.5,2*Frame.Radius/FMath::Max(1.,T.Velocity.Size()));
            D->AddDefenseThreat(T);
        }
    }
}

FCombatMotionContactSnapshot UCombatMotionContactSubsystem::Capture(UDirectionalCombatComponent& Combat) const
{
    FCombatMotionContactSnapshot S;
    S.Time=Combat.CombatTime(); S.Root=Combat.GetOwner()->GetActorTransform();
    S.ActionId=Combat.LibraryActionSerial;
    const auto* Health=Combat.GetOwner()->FindComponentByClass<UCombatVitalityComponent>();
    if (Health) S.LifeId=Health->LifeId;
    if (Combat.IsLibraryActionPlaying() && Combat.MotionRig.Sources[Combat.LibraryActionSource].Action==TEXT("Kick"))
    {
        const auto& Source=Combat.MotionRig.Sources[Combat.LibraryActionSource];
        const double T=(S.Time-Combat.LibraryActionStart)/Combat.MotionRig.Motions[Combat.LibraryActionSource].Length;
        S.bKicking=T>=Source.LoadTime && T<=Source.ContactTime;
    }
    if (const auto* Sample=Combat.GetMotionCoreWeaponSample())
    { S.Weapon=*Sample; S.bWeapon=Combat.CanFight() && Sample->bContactEnabled && FMath::Abs(Sample->SimulationTime-S.Time)<0.001; }
    struct FRegion { const TCHAR* A; const TCHAR* B; float Radius; };
    const FRegion Regions[]={{TEXT("pelvis"),TEXT("spine_03"),12},{TEXT("spine_03"),TEXT("neck_01"),10},{TEXT("head"),TEXT("head"),9},
        {TEXT("upperarm_r"),TEXT("lowerarm_r"),5},{TEXT("lowerarm_r"),TEXT("hand_r"),4},
        {TEXT("upperarm_l"),TEXT("lowerarm_l"),5},{TEXT("lowerarm_l"),TEXT("hand_l"),4},
        {TEXT("thigh_r"),TEXT("calf_r"),7},{TEXT("calf_r"),TEXT("foot_r"),5},
        {TEXT("thigh_l"),TEXT("calf_l"),7},{TEXT("calf_l"),TEXT("foot_l"),5},
        {TEXT("hand_l"),TEXT("hand_l"),4},{TEXT("hand_r"),TEXT("hand_r"),4},
        {TEXT("foot_l"),TEXT("foot_l"),5},{TEXT("foot_r"),TEXT("foot_r"),5}};
    for (const auto& R:Regions)
    {
        auto& Body=S.Body.AddDefaulted_GetRef(); Body.Region=R.A;
        FTransform A,B;
        Body.bValid=(!Health || (Health->LifeState!=ECombatLifeState::Dead && Health->IsRegionPresent(R.A))) && Combat.TryGetContactBone(R.A,A) && Combat.TryGetContactBone(R.B,B);
        if (Body.bValid) { Body.A=A.GetLocation(); Body.B=B.GetLocation(); Body.Radius=R.Radius*S.Root.GetScale3D().GetAbsMax(); }
    }
    return S;
}
void UCombatMotionContactSubsystem::AdvanceDefenses(double Time)
{
    for (;;)
    {
        double Next=Time;
        for (const auto& Track:Tracks) if (auto* C=Track.Combat.Get()) Next=FMath::Min(Next,C->NextDefenseEvent(Time));
        for (auto& Track:Tracks) if (auto* C=Track.Combat.Get()) C->AdvanceDefenseTo(Next);
        for (auto& Track:Tracks) if (auto* C=Track.Combat.Get()) C->CancelInvalidDefenseThreats(Next);
        if (Next>=Time) break;
    }
}
void UCombatMotionContactSubsystem::AfterActors(UWorld* World,ELevelTick Tick,float Delta)
{
    if (World!=GetWorld() || World->GetNetMode()==NM_Client || World->IsPaused()) return;
    Tracks.RemoveAll([](const auto& T){return !T.Combat.IsValid();});
    for (auto& Track:Tracks)
    {
        Track.Current=Capture(*Track.Combat.Get());
        Track.BodyBounds=FBox(ForceInit);
        Track.WeaponBounds=FBox(ForceInit);
        for (const auto* Snapshot:{&Track.Previous,&Track.Current})
        {
            for (const auto& Body:Snapshot->Body) if (Body.bValid)
            {
                FBox Box(ForceInit); Box+=Body.A; Box+=Body.B;
                Track.BodyBounds+=Box.ExpandBy(Body.Radius);
            }
            if (Snapshot->bWeapon)
            {
                const auto& Sample=Snapshot->Weapon;
                const double Radius=FMath::Max(FVector::Distance(Sample.BladeBase,Sample.WeaponWorld.GetLocation()),
                    FVector::Distance(Sample.BladeTip,Sample.WeaponWorld.GetLocation()))+Sample.BladeRadius;
                Track.WeaponBounds+=FBox(Sample.WeaponWorld.GetLocation()-FVector(Radius),Sample.WeaponWorld.GetLocation()+FVector(Radius));
            }
        }
        const auto& W=Track.Current.Weapon;
        if (W.AttackId!=Track.AttackId || W.Epoch!=Track.Epoch || W.WeaponInstanceId!=Track.WeaponId
            || Track.Previous.LifeId!=Track.Current.LifeId)
        { Track.HitTargets.Reset(); Track.AttackId=W.AttackId; Track.Epoch=W.Epoch; Track.WeaponId=W.WeaponInstanceId; Track.LastStrikePhase=W.StrikePhase; }
        else if (W.bAttackActive && W.StrikePhase>Track.LastStrikePhase)
        { Track.HitTargets.Reset(); Track.LastStrikePhase=W.StrikePhase; }
    }
    for (auto& Track:Tracks) if (Track.Combat.IsValid()) Track.Combat->BeginDefenseStep(Track.Current.Time);
    struct FDelivery { TWeakObjectPtr<UDirectionalCombatComponent> Combat; FCombatMotionBodyHit Hit; TWeakObjectPtr<ACombatDefenseProjectile> Projectile; };
    TArray<FDelivery> Deliveries;
    // Limb contacts share the same measured-body sweep and delivery path. A kick
    // never borrows blade geometry or fabricates a sword hit at the target.
    for (auto& Attacker:Tracks)
    {
        const auto& A=Attacker.Previous; const auto& B=Attacker.Current;
        if (A.ActionId!=B.ActionId) Attacker.KickTargets.Reset();
        if (!B.bKicking || !Attacker.Combat->CanFight() || !Attacker.bHistory || !Continuous(A,B) || !Attacker.Combat.IsValid()) continue;
        for (const auto& Defender:Tracks)
        {
            if (&Defender==&Attacker || !Defender.Combat.IsValid() || !Defender.bHistory || !Continuous(Defender.Previous,Defender.Current)) continue;
            AActor* Target=Defender.Combat->GetOwner();
            if (Attacker.KickTargets.Contains(Target)) continue;
            double Earliest=2; FVector Point=FVector::ZeroVector,Velocity=FVector::ZeroVector; FName Region;
            for (int32 Foot:{8,10})
            {
                if (!A.Body.IsValidIndex(Foot) || !B.Body[Foot].bValid || !A.Body[Foot].bValid) continue;
                const FVector From=A.Body[Foot].B,To=B.Body[Foot].B;
                const FVector Relative=(To-From)-(B.Root.GetLocation()-A.Root.GetLocation());
                if (Relative.Size()/(B.Time-A.Time)<100) continue;
                FCombatWeaponSample Start,End;
                Start.WeaponWorld=FTransform(From); End.WeaponWorld=FTransform(To);
                Start.BladeBase=Start.BladeTip=From; End.BladeBase=End.BladeTip=To;
                for (int32 I=0; I<Defender.Current.Body.Num(); ++I)
                {
                    const auto& C=Defender.Previous.Body[I]; const auto& D=Defender.Current.Body[I];
                    if (!C.bValid || !D.bValid) continue;
                    double T; FVector P;
                    if (Sweep(Start,End,[&](double X,FVector& U,FVector& V){U=FMath::Lerp(C.A,D.A,X);V=FMath::Lerp(C.B,D.B,X);},
                        FMath::Max(FVector::Dist(C.A,D.A),FVector::Dist(C.B,D.B)),D.Radius+7,T,P) && T<Earliest)
                    {
                        FHitResult Obstacle;
                        FCollisionQueryParams Params(SCENE_QUERY_STAT(CombatKickOcclusion),false,Attacker.Combat->GetOwner()); Params.AddIgnoredActor(Target);
                        if (World->SweepSingleByObjectType(Obstacle,From,P,FQuat::Identity,FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionShape::MakeSphere(7),Params)) continue;
                        Earliest=T; Point=P; Region=D.Region;
                        Velocity=((To-From)-((D.A-C.A)+(D.B-C.B))*0.5)/(B.Time-A.Time);
                    }
                }
            }
            if (Earliest>1) continue;
            Attacker.KickTargets.Add(Target);
            auto& Delivery=Deliveries.AddDefaulted_GetRef(); Delivery.Combat=Attacker.Combat;
            auto& Hit=Delivery.Hit; Hit.Action=TEXT("Kick"); Hit.Attacker=Attacker.Combat->GetOwner(); Hit.Target=Target;
            Hit.TargetLifeId=Defender.Current.LifeId;
            Hit.AttackId=B.ActionId; Hit.Point=Point; Hit.BodyRegion=Region;
            Hit.ContactVelocity=Velocity; Hit.RelativeSpeed=Velocity.Size(); Hit.Time=FMath::Lerp(A.Time,B.Time,Earliest);
        }
    }
    for (auto& Attacker:Tracks)
    {
        const auto& A=Attacker.Previous; const auto& B=Attacker.Current;
        if (!Attacker.bHistory || !Continuous(A,B) || !A.bWeapon || !B.bWeapon || !B.Weapon.bAttackActive
            || B.Weapon.bDiscontinuity || A.Weapon.Epoch!=B.Weapon.Epoch || A.Weapon.WeaponInstanceId!=B.Weapon.WeaponInstanceId) continue;
        // A real weapon intercept earlier in this interval takes priority over body candidates.
        double Intercept=2;
        for (const auto& Defender:Tracks)
        {
            if (Attacker.Combat->UsesIndicatorDefense()) break;
            if (&Defender==&Attacker || !Defender.bHistory || !Continuous(Defender.Previous,Defender.Current)
                || !Defender.Previous.bWeapon || !Defender.Current.bWeapon || Defender.Current.Weapon.bDiscontinuity) continue;
            if (!Attacker.WeaponBounds.Intersect(Defender.WeaponBounds)) continue;
            const auto& C=Defender.Previous.Weapon; const auto& D=Defender.Current.Weapon;
            if (C.Epoch!=D.Epoch || C.WeaponInstanceId!=D.WeaponInstanceId) continue;
            double T; FVector Point;
            if (Sweep(A.Weapon,B.Weapon,[&](double X,FVector& P,FVector& Q){BladeAt(C,D,X,P,Q);},
                TravelBound(C,D),B.Weapon.BladeRadius+D.BladeRadius,T,Point)) Intercept=FMath::Min(Intercept,T);
        }
        // Explicit weapon/prop contacts: animated pawn bodies intentionally do
        // not push props through the solver. Only a measured active strike does.
        if(Attacker.Combat.IsValid() && Attacker.Combat->GetOwner()->HasAuthority())
        {
            FCollisionQueryParams Params(SCENE_QUERY_STAT(CombatPhysicalProps),false,Attacker.Combat->GetOwner());
            TArray<AActor*> Attached;Attacker.Combat->GetOwner()->GetAttachedActors(Attached,true,true);Params.AddIgnoredActors(Attached);
            FCollisionObjectQueryParams Objects(ECC_PhysicsBody);Objects.AddObjectTypesToQuery(ECC_WorldStatic);
            const double Angle=A.Weapon.WeaponWorld.GetRotation().AngularDistance(B.Weapon.WeaponWorld.GetRotation());
            const int32 Steps=FMath::Clamp(FMath::CeilToInt(Angle/FMath::DegreesToRadians(10.)),1,12);
            for(int32 Step=0;Step<Steps;++Step)
            {
                FVector A0,A1,B0,B1;BladeAt(A.Weapon,B.Weapon,double(Step)/Steps,A0,A1);BladeAt(A.Weapon,B.Weapon,double(Step+1)/Steps,B0,B1);
                const double HalfLength=(B1-B0).Size()*.5;
                const double Radius=B.Weapon.BladeRadius+HalfLength*(1-FMath::Cos(Angle/(2*Steps)));
                const FQuat Rotation=FRotationMatrix::MakeFromZ(((A1-A0)+(B1-B0)).GetSafeNormal()).ToQuat();
                FHitResult Hit;
                if(!World->SweepSingleByObjectType(Hit,(A0+A1)*.5,(B0+B1)*.5,Rotation,Objects,FCollisionShape::MakeCapsule(Radius,HalfLength+Radius),Params))continue;
                if((Step+Hit.Time)/Steps>=Intercept)continue;
                auto* Body=Hit.GetComponent();auto* Target=Hit.GetActor();
                if(!Body || !Target || !Body->IsSimulatingPhysics() || Target->FindComponentByClass<UDirectionalCombatComponent>() || Attacker.HitTargets.Contains(Target))continue;
                FVector Velocity=B.Weapon.LinearVelocity+FVector::CrossProduct(B.Weapon.AngularVelocity,Hit.ImpactPoint-B.Weapon.WeaponWorld.GetLocation());Velocity.Y=0;
                if(Velocity.Size()<100)continue;
                const double Mass=Attacker.Combat->GetEquippedWeaponGeometry().WeaponWeight;
                if(UPOICorridorTools::ApplyPropStrike(Body,Velocity,Mass))Attacker.HitTargets.Add(Target);
            }
        }
        // Tree-only surfaces do not block locomotion. Narrow phase retains real XYZ and
        // sweeps the measured final blade against both previous and current segment poses.
        TArray<FSwordSurface> TreeSurfaces;
        World->GetSubsystem<USwordWorldSubsystem>()->QueryCombatSurfaces(Attacker.WeaponBounds.Min,Attacker.WeaponBounds.Max,TreeSurfaces);
        double TreeTime=2; FVector TreePoint=FVector::ZeroVector; APWGProceduralTree* TreeTarget=nullptr;
        for (const auto& Surface:TreeSurfaces)
        {
            auto* Tree=Cast<APWGProceduralTree>(Surface.Owner.Get());
            if (!Tree || Attacker.HitTargets.Contains(Tree)) continue;
            double T; FVector Point;
            const double Motion=FMath::Max(FVector::Distance(Surface.A,Surface.PreviousA),FVector::Distance(Surface.B,Surface.PreviousB));
            if (Sweep(A.Weapon,B.Weapon,[&](double X,FVector& P,FVector& Q)
                { P=FMath::Lerp(Surface.PreviousA,Surface.A,X); Q=FMath::Lerp(Surface.PreviousB,Surface.B,X); },
                Motion,B.Weapon.BladeRadius+Surface.Radius,T,Point) && T<TreeTime && T<Intercept)
            { TreeTime=T; TreePoint=Point; TreeTarget=Tree; }
        }
        if (TreeTarget)
        {
            const FVector Velocity=B.Weapon.LinearVelocity+FVector::CrossProduct(B.Weapon.AngularVelocity,TreePoint-B.Weapon.WeaponWorld.GetLocation());
            if (Velocity.Size()>=100)
            {
                FCollisionQueryParams Params(SCENE_QUERY_STAT(TreeStrikeOcclusion),false,Attacker.Combat->GetOwner());
                FHitResult Obstacle;
                if (!World->LineTraceSingleByObjectType(Obstacle,B.Weapon.WeaponWorld.GetLocation(),TreePoint,FCollisionObjectQueryParams(ECC_WorldStatic),Params))
                {
                    const float Power=FMath::Clamp(Attacker.Combat->EquippedSword.WeaponPower,0.1f,50.f);
                    TreeTarget->DamageAt(TreePoint,FMath::Clamp(float(Velocity.Size())*0.035f*Power,1.f,250.f),Velocity*0.2f);
                    FCombatContactEvent Effect;
                    Effect.Attacker=Attacker.Combat->GetOwner(); Effect.Target=TreeTarget;
                    Effect.AttackId=B.Weapon.AttackId; Effect.Point=TreePoint; Effect.Direction=-Velocity.GetSafeNormal();
                    Effect.EffectSurface=4; Effect.Intensity=Attacker.Combat->GetSwingIntensity(); Effect.Time=World->GetTimeSeconds();
                    Attacker.Combat->MulticastContact(Effect);
                    Attacker.HitTargets.Add(TreeTarget);
                    Intercept=FMath::Min(Intercept,TreeTime);
                }
            }
        }
        for (const auto& Defender:Tracks)
        {
            if (&Defender==&Attacker || !Defender.Combat.IsValid() || !Defender.bHistory || !Continuous(Defender.Previous,Defender.Current)) continue;
            if (!Attacker.WeaponBounds.Intersect(Defender.BodyBounds)) continue;
            AActor* Target=Defender.Combat->GetOwner();
            if (Attacker.HitTargets.Contains(Target)) continue;
            double Earliest=2; FVector HitPoint, TargetVelocity=FVector::ZeroVector; FName Region;
            for (int32 I=0; I<Defender.Current.Body.Num(); ++I)
            {
                const auto& C=Defender.Previous.Body[I]; const auto& D=Defender.Current.Body[I];
                if (!C.bValid || !D.bValid) continue;
                double T; FVector Point;
                if (Sweep(A.Weapon,B.Weapon,[&](double X,FVector& P,FVector& Q){P=FMath::Lerp(C.A,D.A,X);Q=FMath::Lerp(C.B,D.B,X);},
                    FMath::Max(FVector::Distance(C.A,D.A),FVector::Distance(C.B,D.B)),B.Weapon.BladeRadius+D.Radius,T,Point)
                    && T<Earliest)
                {
                    Earliest=T; HitPoint=Point; Region=D.Region;
                    const FVector Segment=D.B-D.A;
                    const double Along=FMath::Clamp(FVector::DotProduct(Point-D.A,Segment)/FMath::Max(Segment.SizeSquared(),UE_SMALL_NUMBER),0.0,1.0);
                    TargetVelocity=FMath::Lerp(D.A-C.A,D.B-C.B,Along)/(B.Time-A.Time);
                }
            }
            if (Earliest>1 || Earliest>=Intercept) continue;
            const FVector ContactVelocity=B.Weapon.LinearVelocity+FVector::CrossProduct(B.Weapon.AngularVelocity,
                HitPoint-B.Weapon.WeaponWorld.GetLocation())-TargetVelocity;
            if (ContactVelocity.Size()<100) continue; // resting overlap is not an attack hit
            FCollisionQueryParams Params(SCENE_QUERY_STAT(CombatBodyOcclusion),false,Attacker.Combat->GetOwner());
            Params.AddIgnoredActor(Target);
            FCollisionObjectQueryParams Objects;
            Objects.AddObjectTypesToQuery(ECC_WorldStatic); Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
            FHitResult Occlusion;
            Objects.AddObjectTypesToQuery(ECC_GameTraceChannel5);
            const FTransform ContactFrame(FQuat::Slerp(A.Weapon.WeaponWorld.GetRotation(),B.Weapon.WeaponWorld.GetRotation(),Earliest),
                FMath::Lerp(A.Weapon.WeaponWorld.GetLocation(),B.Weapon.WeaponWorld.GetLocation(),Earliest));
            const FVector LocalPoint=ContactFrame.InverseTransformPosition(HitPoint);
            const double Angle=A.Weapon.WeaponWorld.GetRotation().AngularDistance(B.Weapon.WeaponWorld.GetRotation())*Earliest;
            const int32 Steps=FMath::Clamp(FMath::CeilToInt(FMath::Max(Angle/FMath::DegreesToRadians(10.0),TravelBound(A.Weapon,B.Weapon)*Earliest/20)),1,32);
            FVector PreviousPoint=A.Weapon.WeaponWorld.TransformPosition(LocalPoint);
            bool bOccluded=false;
            const float Padding=LocalPoint.Size()*(1-FMath::Cos(Angle/(2*Steps)));
            for (int32 Step=1; Step<=Steps; ++Step)
            {
                const double T=Earliest*Step/Steps;
                const FTransform Frame(FQuat::Slerp(A.Weapon.WeaponWorld.GetRotation(),B.Weapon.WeaponWorld.GetRotation(),T),
                    FMath::Lerp(A.Weapon.WeaponWorld.GetLocation(),B.Weapon.WeaponWorld.GetLocation(),T));
                const FVector NextPoint=Frame.TransformPosition(LocalPoint);
                if (World->SweepSingleByObjectType(Occlusion,PreviousPoint,NextPoint,FQuat::Identity,Objects,
                    FCollisionShape::MakeSphere(B.Weapon.BladeRadius+Padding),Params)) { bOccluded=true; break; }
                PreviousPoint=NextPoint;
            }
            if (bOccluded) continue;
            Attacker.HitTargets.Add(Target);
            auto& Delivery=Deliveries.AddDefaulted_GetRef(); Delivery.Combat=Attacker.Combat;
            auto& Hit=Delivery.Hit; Hit.Attacker=Attacker.Combat->GetOwner(); Hit.Target=Target;
            Hit.TargetLifeId=Defender.Current.LifeId; Hit.AttackType=B.Weapon.AttackType;
            Hit.WeaponEpoch=B.Weapon.Epoch; Hit.StrikePhase=B.Weapon.StrikePhase;
            Hit.AttackId=static_cast<int32>(B.Weapon.AttackId); Hit.WeaponId=B.Weapon.WeaponInstanceId;
            Hit.Point=HitPoint; Hit.BodyRegion=Region; Hit.RelativeSpeed=ContactVelocity.Size(); Hit.ContactVelocity=ContactVelocity;
            Hit.Time=FMath::Lerp(A.Time,B.Time,Earliest);
        }
    }
    for (const auto& Pending:ProjectileHits)
    { auto& D=Deliveries.AddDefaulted_GetRef(); D.Projectile=Pending.Projectile; D.Hit=Pending.Hit; }
    ProjectileHits.Reset();
    Deliveries.Sort([](const auto& A,const auto& B){return A.Hit.Time==B.Hit.Time?A.Hit.AttackId<B.Hit.AttackId:A.Hit.Time<B.Hit.Time;});
    for (auto& Delivery:Deliveries)
    {
        // Resolve forecasts only up to this physical contact, never retroactively parry damage.
        AdvanceDefenses(Delivery.Hit.Time);
        if ((!Delivery.Combat.IsValid() && !Delivery.Projectile.IsValid()) || !IsValid(Delivery.Hit.Target)) continue;
        if (Delivery.Projectile.IsValid() && Delivery.Projectile->DefenseStopTime<=Delivery.Hit.Time) continue;
        auto* Attacker=Delivery.Combat.Get();
        auto* Defender=Delivery.Hit.Target->FindComponentByClass<UDirectionalCombatComponent>();
        if (!Defender) continue;
        if (Attacker && Delivery.Hit.Action!=TEXT("Kick") && Attacker->UsesIndicatorDefense()
            && (!Attacker->BodyAttack.bPrepared || Attacker->BodyAttack.Id!=uint32(Delivery.Hit.AttackId))) continue;
        if (Defender->UsesIndicatorDefense())
        {
            if (auto* T=Defender->DefenseThreats.FindByPredicate([&](const auto& Threat)
                { return Delivery.Hit.bRanged?Threat.Id==Delivery.Hit.ThreatId:Threat.Attacker==Delivery.Hit.Attacker && Threat.AttackId==Delivery.Hit.AttackId
                    && Threat.WeaponId==Delivery.Hit.WeaponId && Threat.Epoch==Delivery.Hit.WeaponEpoch && Threat.StrikePhase==Delivery.Hit.StrikePhase; }))
            {
                const auto R=Defender->DefenseResolver.FinalizeBodyHit(T->Id,Delivery.Hit.Time);
                Delivery.Hit.DamageScale=R.DamageScale; Delivery.Hit.ImpulseScale=R.ImpulseScale;
                T->Outcome=R.Outcome; Defender->GetOwner()->ForceNetUpdate();
                if (R.DamageScale<=0) continue;
            }
        }
        Delivery.Hit.Serial=Attacker?++Attacker->MotionBodyHitSerial:++Defender->MotionBodyHitSerial;
        Defender->ReceiveMotionBodyHit(Delivery.Hit);
        if (Delivery.Combat.IsValid()) Delivery.Combat->MulticastMotionBodyHit(Delivery.Hit);
        else Defender->MulticastMotionBodyHit(Delivery.Hit);
        if (Delivery.Projectile.IsValid()) Delivery.Projectile->Destroy();
    }
    AdvanceDefenses(World->GetTimeSeconds());
    for (auto& Track:Tracks) if (Track.Combat.IsValid())
    {
        Track.Previous=MoveTemp(Track.Current); Track.bHistory=true;
    }
    for (auto& P:Projectiles) if (P.IsValid() && P->DefenseStopTime<=World->GetTimeSeconds()) P->Destroy();
    Projectiles.RemoveAll([](const auto& P){return !P.IsValid();});
}

bool UDirectionalCombatComponent::TryGetContactBone(FName Name,FTransform& Out) const
{
    if (const auto* H=GetOwner()->FindComponentByClass<UCombatVitalityComponent>(); H && H->UsesRagdollPose())
    {
        if (!Mesh || Mesh->GetBoneIndex(Name)==INDEX_NONE) return false;
        Out=Mesh->GetSocketTransform(Name); return !Out.ContainsNaN();
    }
    if (PoseBridge && PoseBridge->TryGetFinalBoneTransform(Name,Out)) return !Out.ContainsNaN();
    if (!Mesh || Mesh->GetBoneIndex(Name)==INDEX_NONE) return false;
    Out=Mesh->GetSocketTransform(Name); return !Out.ContainsNaN();
}
void UDirectionalCombatComponent::MulticastMotionBodyHit_Implementation(FCombatMotionBodyHit Hit)
{
    OnMotionBodyHit.Broadcast(Hit);
}

