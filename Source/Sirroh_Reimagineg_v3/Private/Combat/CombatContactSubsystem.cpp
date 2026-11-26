#include "Combat/CombatContactSubsystem.h"
#include "POI/POICorridorTools.h"
#include "Sword/ProceduralSword.h"
#include "Combat/DirectionalCombatComponent.h"
#include "Sword/SwordWorldSubsystem.h"
#include "Items/SirrohWorldItem.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/GameStateBase.h"
#include "PWG/PWGGameMode.h"
#include "Procedural_World_Generator/PWGv2Generator.h"
#include "Engine/World.h"

namespace CombatCollision
{
    constexpr double CellSize=256;
    void Cells(const FBox& Box,TFunctionRef<void(FIntPoint)> Visit)
    {
        for (int32 X=FMath::FloorToInt(Box.Min.X/CellSize);X<=FMath::FloorToInt(Box.Max.X/CellSize);++X)
            for (int32 Z=FMath::FloorToInt(Box.Min.Z/CellSize);Z<=FMath::FloorToInt(Box.Max.Z/CellSize);++Z) Visit(FIntPoint(X,Z));
    }

    // Continuous collision between two linearly moving capsules, including both moving endpoints.
    // The distance function is Lipschitz bounded by the sum of maximum endpoint speeds.
    bool SweepSegments(FVector A0,FVector B0,FVector A1,FVector B1,
        FVector C0,FVector D0,FVector C1,FVector D1,double Radius,double& T,FVector& Point)
    {
        const double Speed=FMath::Max((A1-A0).Size(),(B1-B0).Size())+FMath::Max((C1-C0).Size(),(D1-D0).Size());
        T=0;
        for (int32 I=0;I<48;++I)
        {
            FVector P,Q;
            FMath::SegmentDistToSegmentSafe(FMath::Lerp(A0,A1,T),FMath::Lerp(B0,B1,T),
                FMath::Lerp(C0,C1,T),FMath::Lerp(D0,D1,T),P,Q);
            const double Separation=(P-Q).Size()-Radius;
            Point=(P+Q)*0.5;
            if (Separation<=0.05) return true;
            if (Speed<=UE_SMALL_NUMBER || Separation>Speed*(1-T)) return false;
            T+=Separation/Speed;
            if (T>1) return false;
        }
        // Conservative contact on a grazing convergence limit; never silently skip a fast blade.
        return true;
    }

    double RotationPadding(double Length,double Angle)
    {
        return FMath::Abs(Angle)>=PI?2*Length:Length*(1-FMath::Cos(FMath::Abs(Angle)*0.5));
    }
    double MotionPadding(const FDirectionalAttackMotion& Motion,double Length,double Dt)
    {
        if (!Motion.bReleased) return 0;
        const double Duration=FMath::Max(0.08f,Motion.Duration);
        // Bounds also cover the nested easing of the two cut orientation segments.
        const double Factor=4;
        const double Omega=1.5*FMath::Abs(Motion.SweepAngle)*Factor/Duration;
        const double AngularAccel=6*FMath::Abs(Motion.SweepAngle)*Factor*Factor/(Duration*Duration);
        const double Reach=FMath::Max3(Motion.StartGrip.Size(),Motion.EndGrip.Size(),Motion.ControlGrip.Size());
        return (48*Reach/(Duration*Duration)+Length*(Omega*Omega+AngularAccel))*Dt*Dt/8;
    }
}

void UCombatContactSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Collection.InitializeDependency<USwordWorldSubsystem>();
    TickHandle=FWorldDelegates::OnWorldPostActorTick.AddUObject(this,&UCombatContactSubsystem::AfterActors);
}
void UCombatContactSubsystem::Deinitialize()
{
    FWorldDelegates::OnWorldPostActorTick.Remove(TickHandle);
    Tracks.Reset(); ItemHistory.Reset(); Pending.Reset();
    Super::Deinitialize();
}
void UCombatContactSubsystem::RegisterCombatant(UDirectionalCombatComponent* Combat)
{
    if (!Combat || !Combat->GetOwner()->HasAuthority() || Combat->UsesMotionCoreV3()) return;
    for (const auto& Track:Tracks) if (Track.Combat==Combat) return;
    Tracks.AddDefaulted_GetRef().Combat=Combat;
}
void UCombatContactSubsystem::UnregisterCombatant(UDirectionalCombatComponent* Combat)
{
    // Preserve indices during contact callbacks; dead entries are removed before the next batch.
    for (auto& Track:Tracks) if (Track.Combat==Combat) Track.Combat.Reset();
}

void UCombatContactSubsystem::Capture(FCombatContactTrack& Track,double Now)
{
    auto* C=Track.Combat.Get();
    FCombatHistorySample S;
    S.Time=Now; S.Origin=C->GetOwner()->GetActorLocation(); S.Weapon=C->GetWeaponFrame();
    S.Rest=C->RestWeapon; S.Motion=C->AttackMotion;
    S.InterruptedAt=C->Reaction.bInterrupt?C->Reaction.StartTime:-DBL_MAX;
    S.bWeapon=C->bCombatActive && !C->Reaction.bDisarmed;
    S.bBlocking=C->GetCombatPhase()==EDirectionalCombatPhase::Blocking;
    S.Guard=C->CurrentGuard;
    S.bAttacking=C->GetCombatPhase()==EDirectionalCombatPhase::Attacking;
    S.PixelScale=C->Rig.PixelScale; S.AttackId=C->AttackMotion.Id;
    S.BodyA=S.Origin-FVector(0,0,40); S.BodyB=S.Origin+FVector(0,0,40);
    if (const auto* Capsule=C->GetOwner()->FindComponentByClass<UCapsuleComponent>())
    {
        S.BodyRadius=Capsule->GetScaledCapsuleRadius();
        const FVector Extent=Capsule->GetUpVector()*FMath::Max(0.0f,Capsule->GetScaledCapsuleHalfHeight()-S.BodyRadius);
        S.BodyA=Capsule->GetComponentLocation()-Extent; S.BodyB=Capsule->GetComponentLocation()+Extent;
    }
    if (!Track.History.IsEmpty() && FVector::DistSquared(Track.History.Last().Origin,S.Origin)>FMath::Square(1500.0))
    { Track.History.Reset(); Track.CheckedUntil=Now; } // Teleports are not sword sweeps.
    Track.History.Add(S);
    while (Track.History.Num()>2 && (Track.History[1].Time<Now-HistorySeconds || Track.History.Num()>128)) Track.History.RemoveAt(0);
}

FCombatHistorySample UCombatContactSubsystem::Sample(const FCombatContactTrack& Track,double Time) const
{
    if (Track.History.IsEmpty()) return {};
    if (Time<=Track.History[0].Time) return Track.History[0];
    for (int32 I=1;I<Track.History.Num();++I)
    {
        const auto& B=Track.History[I];
        if (Time>B.Time) continue;
        const auto& A=Track.History[I-1];
        const double T=FMath::Clamp((Time-A.Time)/FMath::Max(0.000001,B.Time-A.Time),0.0,1.0);
        FCombatHistorySample S=A;
        S.Time=Time; S.Origin=FMath::Lerp(A.Origin,B.Origin,T);
        S.BodyA=FMath::Lerp(A.BodyA,B.BodyA,T); S.BodyB=FMath::Lerp(A.BodyB,B.BodyB,T);
        S.Weapon.Grip=FMath::Lerp(A.Weapon.Grip,B.Weapon.Grip,T);
        // Preserve unwrapped attack angles, including rotations greater than 180 degrees.
        const float Delta=A.bAttacking && B.bAttacking && A.AttackId==B.AttackId?B.Weapon.Angle-A.Weapon.Angle:FMath::FindDeltaAngleRadians(A.Weapon.Angle,B.Weapon.Angle);
        S.Weapon.Angle=A.Weapon.Angle+Delta*T;
        S.Weapon.Rotation=FQuat::Slerp(A.Weapon.Rotation,B.Weapon.Rotation,T).GetNormalized();
        // Resample the actual attack curve rather than approximating a fast moving opponent
        // by one straight chord between server frames. A newly received release can be rewound.
        auto IsLive=[&](const FCombatHistorySample& H)
        { return H.Motion.bPrepared && H.Motion.bReleased && Time>=H.Motion.StartTime &&
            Time<=H.Motion.StartTime+H.Motion.Duration && (B.InterruptedAt<H.Motion.StartTime || Time<B.InterruptedAt); };
        const FCombatHistorySample* Curve=IsLive(B)?&B:(IsLive(A)?&A:nullptr);
        if (Curve)
        {
            S.Weapon=UDirectionalCombatComponent::EvaluateAttackFrame(Curve->Motion,Curve->Rest,Time,S.Origin);
            S.Motion=Curve->Motion;
        }
        if (T>=1-UE_SMALL_NUMBER) { S.bWeapon=B.bWeapon; S.bBlocking=B.bBlocking; S.Guard=B.Guard; }
        return S;
    }
    return Track.History.Last();
}

void UCombatContactSubsystem::RebuildGrid()
{
    DynamicGrid.Reset(); ItemGrid.Reset(); Items.Reset();
    for (int32 I=0;I<Tracks.Num();++I)
    {
        FBox Bounds(ForceInit);
        for (const auto& S:Tracks[I].History)
        {
            Bounds+=S.BodyA; Bounds+=S.BodyB;
            // A sphere around every grip covers the full rotational path, not just endpoint tips.
            const FVector R(368*S.PixelScale+S.BodyRadius);
            Bounds+=S.Weapon.Grip-R; Bounds+=S.Weapon.Grip+R;
        }
        if (Bounds.IsValid) CombatCollision::Cells(Bounds,[&](FIntPoint Cell){DynamicGrid.FindOrAdd(Cell).Add(I);});
    }
    const double Now=GetWorld()->GetGameState()?GetWorld()->GetGameState()->GetServerWorldTimeSeconds():GetWorld()->GetTimeSeconds();
    for (auto It=ItemHistory.CreateIterator();It;++It)
        if (!It.Key().IsValid() || It.Key()->IsHeld()) It.RemoveCurrent();
    for (const auto& Weak:GetWorld()->GetSubsystem<USwordWorldSubsystem>()->Items)
    {
        auto* Item=Weak.Get();
        if (!Item || Item->IsHeld()) continue;
        auto* Existing=ItemHistory.Find(Weak);
        const FVector Previous=Existing && Now-Existing->Time<=0.1?Existing->Current:Item->GetActorLocation();
        const FVector Current=Item->GetActorLocation();
        ItemHistory.Add(Weak,{Previous,Current,Now});
        FBox Bounds(ForceInit); Bounds+=Previous; Bounds+=Current;
        Bounds=Bounds.ExpandBy(FMath::Max(1.0f,Item->GetItemHitRadius()));
        const int32 Index=Items.Add(Weak);
        CombatCollision::Cells(Bounds,[&](FIntPoint Cell){ItemGrid.FindOrAdd(Cell).Add(Index);});
    }
}

void UCombatContactSubsystem::AfterActors(UWorld* World,ELevelTick Tick,float Delta)
{
    if (World!=GetWorld() || World->GetNetMode()==NM_Client || Tick==LEVELTICK_ViewportsOnly || World->IsPaused() || Delta<=0) return;
    Tracks.RemoveAll([](const auto& Track){return !Track.Combat.IsValid() || Track.Combat->UsesMotionCoreV3();});
    if (Tracks.IsEmpty()) return;
    const double Now=Tracks[0].Combat->CombatTime();
    for (auto& Track:Tracks) Capture(Track,Now);
    bool bNeedsContacts=false;
    for (const auto& Track:Tracks)
    {
        const auto* C=Track.Combat.Get();
        const auto& Motion=C->AttackMotion;
        bNeedsContacts|=C->bCombatActive && Motion.bPrepared && Motion.bReleased && Now>Motion.StartTime &&
            (Track.AttackId!=Motion.Id || Track.CheckedUntil<Motion.StartTime+Motion.Duration);
    }
    if (!bNeedsContacts) return;
    RebuildGrid(); Pending.Reset();
    for (int32 I=0;I<Tracks.Num();++I) CollectAttack(I,Now);
    Pending.Sort([](const FPendingContact& A,const FPendingContact& B)
    {
        if (A.Time!=B.Time) return A.Time<B.Time;
        if (A.bWeapon!=B.bWeapon) return A.bWeapon; // An exact tie belongs to the weapon before the body.
        if (A.Attacker!=B.Attacker) return A.Attacker<B.Attacker;
        return A.Defender<B.Defender;
    });
    for (const auto& Contact:Pending) Resolve(Contact,Now);
}

void UCombatContactSubsystem::CollectAttack(int32 TrackIndex,double Now)
{
    auto& Track=Tracks[TrackIndex];
    auto* C=Track.Combat.Get();
    const auto& Motion=C->AttackMotion;
    if (!C->bCombatActive || !Motion.bPrepared || !Motion.bReleased || C->Reaction.bDisarmed) return;
    if (Track.AttackId!=Motion.Id)
    {
        Track.AttackId=Motion.Id; Track.CheckedUntil=Motion.StartTime;
        Track.HitBodies.Reset(); Track.HitWeapons.Reset();
    }
    // RPC receipt already bounds rewind to 250 ms. Never truncate a known attack after a
    // long server frame: sweep its whole unprocessed interval, even if it has already ended.
    const double From=FMath::Max(Track.CheckedUntil,Motion.StartTime);
    const double To=FMath::Min(Now,Motion.StartTime+Motion.Duration);
    if (To<=From) return;
    Track.CheckedUntil=To;
    const double Reach=FMath::Max3(Motion.StartGrip.Size(),Motion.EndGrip.Size(),Motion.ControlGrip.Size())+368*C->Rig.PixelScale+10;
    FBox Bounds(ForceInit);
    for (const auto& S:Track.History) { Bounds+=S.Origin-FVector(Reach); Bounds+=S.Origin+FVector(Reach); }
    TSet<int32> Candidates,ItemCandidates;
    CombatCollision::Cells(Bounds,[&](FIntPoint Cell)
    {
        if (const auto* Bucket=DynamicGrid.Find(Cell)) for (int32 Id:*Bucket) if (Id!=TrackIndex) Candidates.Add(Id);
        if (const auto* Bucket=ItemGrid.Find(Cell)) for (int32 Id:*Bucket) ItemCandidates.Add(Id);
    });
    TArray<FSwordSurface> Surfaces;
    GetWorld()->GetSubsystem<USwordWorldSubsystem>()->QueryCombatSurfaces(Bounds.Min,Bounds.Max,Surfaces);
    const double Duration=FMath::Max(0.08f,Motion.Duration);
    // Pierce aligns in the first quarter; smoothstep reaches 1.5 times average angular speed.
    const double Omega=6*FMath::Abs(Motion.SweepAngle)/Duration;
    const int32 Steps=FMath::Clamp(FMath::CeilToInt(FMath::Max((To-From)*120,(To-From)*Omega/FMath::DegreesToRadians(4.0))),1,128);
    const double Dt=(To-From)/Steps;
    const double AccelAngle=96*FMath::Abs(Motion.SweepAngle)/(Duration*Duration);
    const double AccelGrip=48*Reach/(Duration*Duration);
    // Conservative chord error also grows when the subdivision budget is exhausted.
    const double Padding=(AccelGrip+368*C->Rig.PixelScale*(Omega*Omega+AccelAngle))*Dt*Dt/8;
    const double Radius=FMath::Max(0.5f,C->ContactSettings.BladeRadius)+Padding;
    TSet<int32> FoundBodies,FoundWeapons,FoundSurfaces,FoundItems;
    bool bFoundPhysics=false;
    TArray<float> Lanes;
    if (const auto* GM=GetWorld()->GetAuthGameMode<APWGGameMode>())
        if (const auto* Generator=GM->GetWorldGenerator())
            for (EPWGTerrainLane Lane:{EPWGTerrainLane::Front,EPWGTerrainLane::Middle,EPWGTerrainLane::Back}) Lanes.Add(Generator->GetLaneWorldY(Lane));
    if (Lanes.IsEmpty()) Lanes.Add(C->GetOwner()->GetActorLocation().Y);
    for (int32 Step=0;Step<Steps;++Step)
    {
        const double T0=From+Step*Dt,T1=T0+Dt;
        const auto Self0=Sample(Track,T0),Self1=Sample(Track,T1);
        const auto W0=C->EvaluateWeapon(T0,Self0.Origin),W1=C->EvaluateWeapon(T1,Self1.Origin);
        const FVector A0=CombatSpace::InLane(W0.Base(C->Rig.PixelScale),Self0.Origin.Y),B0=CombatSpace::InLane(W0.Tip(C->Rig.PixelScale),Self0.Origin.Y);
        const FVector A1=CombatSpace::InLane(W1.Base(C->Rig.PixelScale),Self1.Origin.Y),B1=CombatSpace::InLane(W1.Tip(C->Rig.PixelScale),Self1.Origin.Y);
        auto Add=[&](AActor* Target,int32 Defender,bool bWeapon,bool bBlock,bool bBody,double Fraction,FVector Point,const FVector& TargetDelta)
        {
            FPendingContact Hit;
            Hit.Attacker=TrackIndex; Hit.Defender=Defender; Hit.Target=Target;
            Hit.Time=T0+Dt*Fraction; Hit.Point=Point; Hit.Point.Y+=FMath::Lerp(Self0.Origin.Y,Self1.Origin.Y,Fraction);
            // Relative velocity at the contacted section, not the speed of an unrelated blade tip.
            const FVector Blade=B0-A0;
            const double Along=FMath::Clamp(FVector::DotProduct(Point-A0,Blade)/FMath::Max(1.0,Blade.SizeSquared()),0.0,1.0);
            const FVector Travel=FMath::Lerp(A1-A0,B1-B0,Along)-TargetDelta;
            Hit.Speed=Travel.Size()/FMath::Max(0.00001,Dt);
            Hit.Direction=Travel.GetSafeNormal();
            if (Hit.Direction.IsNearlyZero()) Hit.Direction=W1.Direction();
            Hit.bWeapon=bWeapon; Hit.bBlocking=bBlock; Hit.bBody=bBody; Hit.AttackId=Motion.Id;
            Pending.Add(Hit);
        };
        for (int32 Other:Candidates)
        {
            auto* Defender=Tracks[Other].Combat.Get();
            if (!Defender) continue;
            const auto D0=Sample(Tracks[Other],T0),D1=Sample(Tracks[Other],T1);
            double Fraction; FVector Point;
            if (!FoundWeapons.Contains(Other) && !Track.HitWeapons.Contains(Defender->GetOwner()) && D0.bWeapon)
            {
                const double OtherPad=CombatCollision::RotationPadding(368*D0.PixelScale,D1.Weapon.Rotation.AngularDistance(D0.Weapon.Rotation))+
                    FMath::Max(CombatCollision::MotionPadding(D0.Motion,368*D0.PixelScale,Dt),CombatCollision::MotionPadding(D1.Motion,368*D1.PixelScale,Dt));
                if (CombatCollision::SweepSegments(A0,B0,A1,B1,CombatSpace::InLane(D0.Weapon.Base(D0.PixelScale),D0.Origin.Y),CombatSpace::InLane(D0.Weapon.Tip(D0.PixelScale),D0.Origin.Y),
                    CombatSpace::InLane(D1.Weapon.Base(D1.PixelScale),D1.Origin.Y),CombatSpace::InLane(D1.Weapon.Tip(D1.PixelScale),D1.Origin.Y),Radius+FMath::Max(0.5f,Defender->ContactSettings.BladeRadius)+OtherPad,Fraction,Point))
                {
                    Add(Defender->GetOwner(),Other,true,D0.bBlocking && D0.Guard==Motion.Guard,false,Fraction,Point,D1.Weapon.Grip-D0.Weapon.Grip);
                    FoundWeapons.Add(Other);
                }
            }
            if (!FoundBodies.Contains(Other) && !Track.HitBodies.Contains(Defender->GetOwner()) &&
                CombatCollision::SweepSegments(A0,B0,A1,B1,CombatSpace::InLane(D0.BodyA,D0.Origin.Y),CombatSpace::InLane(D0.BodyB,D0.Origin.Y),
                    CombatSpace::InLane(D1.BodyA,D1.Origin.Y),CombatSpace::InLane(D1.BodyB,D1.Origin.Y),Radius+D0.BodyRadius,Fraction,Point))
            {
                Add(Defender->GetOwner(),Other,false,false,true,Fraction,Point,D1.Origin-D0.Origin);
                FoundBodies.Add(Other);
            }
        }
        for (int32 I=0;I<Surfaces.Num();++I)
        {
            const auto& S=Surfaces[I];
            if (FoundSurfaces.Contains(I) || S.Owner==C->GetOwner()) continue;
            double Fraction; FVector Point;
            const FVector SA=CombatSpace::InLane(S.A,S.LaneY),SB=CombatSpace::InLane(S.B,S.LaneY);
            if (CombatCollision::SweepSegments(A0,B0,A1,B1,SA,SB,SA,SB,Radius+S.Radius,Fraction,Point))
            { Add(S.Owner.Get(),INDEX_NONE,false,false,false,Fraction,Point,FVector::ZeroVector); FoundSurfaces.Add(I); }
        }
        for (int32 I:ItemCandidates)
        {
            auto* Item=Items[I].Get();
            if (!Item || FoundItems.Contains(I)) continue;
            const auto* History=ItemHistory.Find(Items[I]);
            if (!History) continue;
            // Items have current-frame motion only; combatant history is the compensated path.
            const double FrameStart=Track.History.Num()>1?Track.History[Track.History.Num()-2].Time:From;
            const double FrameDuration=FMath::Max(0.00001,Now-FrameStart);
            const FVector P0=CombatSpace::InLane(FMath::Lerp(History->Previous,History->Current,FMath::Clamp((T0-FrameStart)/FrameDuration,0.0,1.0)),Item->LaneY);
            const FVector P1=CombatSpace::InLane(FMath::Lerp(History->Previous,History->Current,FMath::Clamp((T1-FrameStart)/FrameDuration,0.0,1.0)),Item->LaneY);
            double Fraction; FVector Point;
            if (CombatCollision::SweepSegments(A0,B0,A1,B1,P0,P0,P1,P1,Radius+Item->GetItemHitRadius(),Fraction,Point))
            { Add(Item,INDEX_NONE,false,false,false,Fraction,Point,P1-P0); FoundItems.Add(I); }
        }
        // Physical terrain/props supplement the weapon-only PWG surfaces, on each gameplay lane.
        if (!bFoundPhysics)
        {
            FCollisionQueryParams Params(SCENE_QUERY_STAT(CombatTerrain),false,C->GetOwner());
            FCollisionObjectQueryParams Objects(ECC_WorldStatic);
            Objects.AddObjectTypesToQuery(ECC_PhysicsBody);
            const FVector MidDirection=FQuat::Slerp(W0.Rotation,W1.Rotation,0.5f).GetAxisX();
            const FQuat Rotation=FRotationMatrix::MakeFromZ(MidDirection).ToQuat();
            const float HalfLength=192*C->Rig.PixelScale;
            const float SweepRadius=Radius+HalfLength*FMath::Min(2.0f,static_cast<float>(W1.Rotation.AngularDistance(W0.Rotation))*0.5f);
            for (float LaneY:Lanes)
            {
                FVector Start=(A0+B0)*0.5,End=(A1+B1)*0.5; Start.Y+=LaneY; End.Y+=LaneY;
                FHitResult Hit;
                if (GetWorld()->SweepSingleByObjectType(Hit,Start,End,Rotation,Objects,FCollisionShape::MakeCapsule(SweepRadius,HalfLength+SweepRadius),Params))
                { Add(Hit.GetActor(),INDEX_NONE,false,false,false,Hit.Time,CombatSpace::InLane(Hit.ImpactPoint,LaneY),FVector::ZeroVector); bFoundPhysics=true; }
            }
        }
    }
}

void UCombatContactSubsystem::Resolve(const FPendingContact& Hit,double Now)
{
    auto* Attacker=Tracks[Hit.Attacker].Combat.Get();
    if (!Attacker || !Attacker->bCombatActive || !Attacker->AttackMotion.bReleased || Attacker->AttackMotion.Id!=Hit.AttackId) return;
    auto* Defender=Hit.Defender!=INDEX_NONE?Tracks[Hit.Defender].Combat.Get():nullptr;
    if (Hit.Defender!=INDEX_NONE && !Defender) return;
    FCombatContactEvent Event;
    Event.AttackId=static_cast<int32>(Hit.AttackId); Event.Attacker=Attacker->GetOwner(); Event.Target=Hit.Target.Get();
    Event.Point=Hit.Point; Event.Direction=Hit.Direction; Event.Time=Hit.Time;
    Event.Intensity=Attacker->AttackMotion.Intensity; Event.bSwordCollision=Hit.bWeapon;
    Event.Impact=Attacker->AttackMotion.ImpactScore;
    const float PushSign=Defender?(Defender->GetOwner()->GetActorLocation().X>=Attacker->GetOwner()->GetActorLocation().X?1.0f:-1.0f):Attacker->AttackMotion.Facing;
    const FVector PushDirection(PushSign,0,0);
    auto& Track=Tracks[Hit.Attacker];
    if (Hit.bWeapon)
    {
        if (Defender->Reaction.bDisarmed || !Defender->bCombatActive || Track.HitWeapons.Contains(Event.Target.Get())) return;
        Track.HitWeapons.Add(Event.Target.Get());
        const auto& D=Defender->ContactSettings;
        if (Event.Impact>15 && Event.Impact<=20)
        {
            // A fully charged heavy can disarm on an actual sword contact, even outside a guard.
            Event.Result=ECombatContactResult::Disarm;
            Defender->React(Event.Result,Hit.Time,0.5f,true,true);
            if (Attacker->AttackMotion.bPush) Defender->ApplyKnockback(PushDirection*FMath::Max(0.0f,D.KnockbackSpeed)*0.4f);
            Attacker->React(ECombatContactResult::Weapon,Hit.Time,0.2f,true);
        }
        else if (!Hit.bBlocking || (Defender->Reaction.bInterrupt && Defender->Reaction.StartTime>=Hit.Time && Defender->Reaction.EndTime>Now))
        {
            // A wrong guard must not provide an accidental block through the general clash path.
            Event.Result=ECombatContactResult::Breakthrough;
        }
        else if (Event.Impact<=5)
        {
            Event.Result=ECombatContactResult::Rebound;
            Attacker->React(Event.Result,Hit.Time,0.3f,true);
            Defender->React(ECombatContactResult::Shake,Hit.Time,0.2f,false);
        }
        else if (Event.Impact<=8)
        {
            Event.Result=ECombatContactResult::Shake;
            Attacker->React(Event.Result,Hit.Time,0.22f,true);
            Defender->React(Event.Result,Hit.Time,0.22f,false);
        }
        else if (Event.Impact<=15)
        {
            Event.Result=ECombatContactResult::Stagger;
            Defender->React(Event.Result,Hit.Time,FMath::Max(1.0f,D.StaggerSeconds),true);
            if (Attacker->AttackMotion.bPush) Defender->ApplyKnockback(PushDirection*FMath::Max(0.0f,D.KnockbackSpeed)*0.4f);
            Attacker->React(ECombatContactResult::Weapon,Hit.Time,0.2f,true);
        }
        else
        {
            Event.Result=ECombatContactResult::Breakthrough;
            Defender->React(ECombatContactResult::Shake,Hit.Time,0.2f,false);
            // Continue resolving this attack. Only a separate body intersection can produce Hit.
        }
    }
    else if (Hit.bBody)
    {
        if (Track.HitBodies.Contains(Event.Target.Get())) return;
        Track.HitBodies.Add(Event.Target.Get());
        Event.Result=ECombatContactResult::Hit;
        const bool Stagger=Event.Impact>8;
        if (!Defender->Reaction.bDisarmed)
            Defender->React(Stagger?ECombatContactResult::Stagger:ECombatContactResult::Shake,Hit.Time,Stagger?FMath::Max(0.15f,Defender->ContactSettings.StaggerSeconds):0.15f,Stagger);
        if (Attacker->AttackMotion.bPush) Defender->ApplyKnockback(PushDirection*FMath::Max(0.0f,Defender->ContactSettings.KnockbackSpeed)*0.4f);
        Attacker->React(ECombatContactResult::Hit,Hit.Time,0.2f,true);
    }
    else
    {
        Event.Result=ECombatContactResult::World;
        if (auto* Item=Cast<ASirrohWorldItem>(Hit.Target.Get()))
            Item->ReceiveProjectileImpact(Hit.Direction*150,Attacker->GetOwner());
        else if(auto* Target=Hit.Target.Get();Target && Attacker->GetOwner()->HasAuthority() && !Track.HitBodies.Contains(Target))
            if(auto* Body=Cast<UPrimitiveComponent>(Target->GetRootComponent());Body && Body->IsSimulatingPhysics() && Hit.Speed>=100)
            {
                FVector Direction=Hit.Direction;Direction.Y=0;Direction.Normalize();
                const float Mass=Attacker->GetEquippedWeaponGeometry().WeaponWeight;
                if(UPOICorridorTools::ApplyPropStrike(Body,Direction*Hit.Speed,Mass))Track.HitBodies.Add(Target);
            }
        Attacker->React(Event.Result,Hit.Time,0.2f,true);
    }
    Attacker->MulticastContact(Event);
    if (Defender) Defender->MulticastContact(Event);
}

