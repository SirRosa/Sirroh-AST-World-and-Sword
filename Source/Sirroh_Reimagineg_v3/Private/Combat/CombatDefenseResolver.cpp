#include "Combat/CombatDefenseResolver.h"

FVector FCombatDefenseThreat::Position(double Time) const
{
    const double T=FMath::Max(0.,Time-StartTime);
    return Origin+Velocity*T+Acceleration*(0.5*T*T);
}
bool FCombatDefenseThreat::IsValid() const
{
    return Id.IsValid() && !Origin.ContainsNaN() && !Velocity.ContainsNaN() && !Acceleration.ContainsNaN()
        && FMath::IsFinite(StartTime) && FMath::IsFinite(PredictedContactTime) && FMath::IsFinite(ExpireTime)
        && PredictedContactTime>=StartTime && ExpireTime>=PredictedContactTime;
}
bool FCombatDefenseThreat::IsFlashing(double Time) const
{ return Time>=FMath::Max(StartTime,PredictedContactTime-0.2) && Time<=PredictedContactTime; }
bool FCombatDefenseResult::IsDefense() const
{ return Outcome==ECombatDefenseOutcome::Block || Outcome==ECombatDefenseOutcome::PerfectBlock || Outcome==ECombatDefenseOutcome::Parry; }
float FCombatDefenseResolver::PaddleHalfAngle(const FCombatDefenseFrame& Frame)
{ return FMath::Clamp(0.75f*FMath::Max(0.f,Frame.WeaponLength)/(2*FMath::Max(1.f,Frame.Radius)),0.f,float(PI/4)); }
bool FCombatDefenseResolver::AddThreat(const FCombatDefenseThreat& Threat)
{
    if (!Threat.IsValid() || Ledger.Contains(Threat.Id)) return false;
    auto& Entry=Ledger.Add(Threat.Id); Entry.Result.ThreatId=Threat.Id;
    Entry.KeepUntil=Threat.ExpireTime+2;
    Threats.Add(Threat); return true;
}
void FCombatDefenseResolver::SetWeapon(FGuid Id,float Value)
{
    if (WeaponId!=Id) { OmniblockUntil=-DBL_MAX; for (auto& E:Ledger) E.Value.ParryAt=-DBL_MAX; }
    WeaponId=Id; Durability=FMath::IsFinite(Value)?FMath::Max(0.f,Value):0;
}
void FCombatDefenseResolver::Reset()
{ Threats.Reset(); Ledger.Reset(); WeaponId.Invalidate(); Durability=0; OmniblockUntil=LastAdvance=-DBL_MAX; }
void FCombatDefenseResolver::Cancel(FGuid Id,double Time)
{
    if (auto* E=Ledger.Find(Id); E && E->Result.Outcome==ECombatDefenseOutcome::Pending)
    { E->Result.Outcome=ECombatDefenseOutcome::Cancelled; E->Result.Time=Time; }
}
const FCombatDefenseResult* FCombatDefenseResolver::FindResult(FGuid Id) const
{ const auto* E=Ledger.Find(Id); return E?&E->Result:nullptr; }
bool FCombatDefenseResolver::TryParry(double Time,const FCombatDefenseFrame& Frame)
{
    if (!FMath::IsFinite(Time) || !Frame.bCanDefend || !WeaponId.IsValid() || Durability<=0) return false;
    // One press selects one pending incoming threat; it is not a persistent parry stance.
    FEntry* Selected=nullptr; double Earliest=DBL_MAX;
    for (const auto& Threat:Threats)
    {
        auto* E=Ledger.Find(Threat.Id);
        if (!E || E->bBodyFinalized || E->Result.Outcome!=ECombatDefenseOutcome::Pending || !Threat.IsFlashing(Time)) continue;
        if (Threat.PredictedContactTime<Earliest)
        { Selected=E; Earliest=Threat.PredictedContactTime; }
    }
    if (!Selected) return false;
    Selected->ParryAt=Time; return true;
}
void FCombatDefenseResolver::Advance(double From,double To,const FCombatDefenseFrame& Previous,const FCombatDefenseFrame& Current,TArray<FCombatDefenseResult>& Results)
{
    if (!FMath::IsFinite(From) || !FMath::IsFinite(To) || To<=From || From<LastAdvance || To<=LastAdvance) return;
    LastAdvance=To;
    if (!Current.bCanDefend || !WeaponId.IsValid()) OmniblockUntil=-DBL_MAX;
    struct FCandidate { int32 Index; double Time; FVector Point; FVector Centre; };
    TArray<FCandidate> Candidates;
    // Subdivide ballistic curves; solve segment-circle entry analytically within each step.
    for (int32 I=0;I<Threats.Num();++I)
    {
        const auto& T=Threats[I]; auto* E=Ledger.Find(T.Id);
        if (!E || E->bBodyFinalized || E->Result.Outcome!=ECombatDefenseOutcome::Pending || To<T.StartTime) continue;
        const double Begin=FMath::Max(From,T.StartTime), End=FMath::Min(To,T.ExpireTime);
        const int32 Steps=FMath::Max(1,FMath::CeilToInt((End-Begin)/0.008));
        for (int32 Step=0;Step<Steps && End>=Begin;++Step)
        {
            const double A=FMath::Lerp(Begin,End,double(Step)/Steps),B=FMath::Lerp(Begin,End,double(Step+1)/Steps);
            const FVector CA=FMath::Lerp(Previous.Centre,Current.Centre,(A-From)/(To-From));
            const FVector CB=FMath::Lerp(Previous.Centre,Current.Centre,(B-From)/(To-From));
            const FVector PA=T.Position(A),PB=T.Position(B), RA=PA-CA,RB=PB-CB;
            const FVector2D O(RA.X,RA.Z),D(RB.X-RA.X,RB.Z-RA.Z);
            const double Radius=FMath::Max(1.f,Current.Radius), C=O.SizeSquared()-Radius*Radius;
            double U=0;
            if (C>0)
            {
                const double AA=D.SizeSquared(),BB=2*FVector2D::DotProduct(O,D),Disc=BB*BB-4*AA*C;
                if (AA<=UE_SMALL_NUMBER || Disc<0) continue;
                U=(-BB-FMath::Sqrt(Disc))/(2*AA); if (U<0 || U>1) continue;
            }
            const FVector Point=FMath::Lerp(PA,PB,U),Centre=FMath::Lerp(CA,CB,U);
            if (FMath::Abs(Point.Y-Centre.Y)>Current.HalfDepth) continue;
            Candidates.Add({I,FMath::Lerp(A,B,U),Point,Centre}); break;
        }
    }
    Candidates.Sort([&](const auto& A,const auto& B){return A.Time==B.Time?Threats[A.Index].Id.ToString()<Threats[B.Index].Id.ToString():A.Time<B.Time;});
    for (const auto& C:Candidates)
    {
        const auto& T=Threats[C.Index]; auto& E=Ledger.FindChecked(T.Id); auto& R=E.Result;
        const FVector Delta=C.Point-C.Centre;
        const FVector2D Direction=FVector2D(Delta.X,Delta.Z).GetSafeNormal();
        const float Angle=FMath::Acos(FMath::Clamp(float(FVector2D::DotProduct(Direction,Current.Paddle.GetSafeNormal())),-1.f,1.f));
        const float Half=PaddleHalfAngle(Current);
        const bool Can=Current.bCanDefend && WeaponId.IsValid() && (Durability>0 || C.Time<OmniblockUntil);
        const bool Paddle=Can && !Current.Paddle.IsNearlyZero() && Angle<=Half;
        const bool Perfect=Paddle && Angle<=Half*FMath::Clamp(Current.PerfectFraction,0.f,1.f);
        const bool Parry=Paddle && E.ParryAt<=C.Time && E.ParryAt>=T.PredictedContactTime-0.2 && C.Time-E.ParryAt<=0.2;
        const bool Omni=Can && C.Time<OmniblockUntil;
        R.Time=C.Time; R.Point=C.Point; R.bOmniblock=Omni;
        if (!Paddle && !Omni) R.Outcome=ECombatDefenseOutcome::Miss;
        else
        {
            R.Outcome=Parry?ECombatDefenseOutcome::Parry:Perfect?ECombatDefenseOutcome::PerfectBlock:ECombatDefenseOutcome::Block;
            R.DamageScale=R.ImpulseScale=T.bPierce && R.Outcome==ECombatDefenseOutcome::Block?0.5f:0.f;
            if (!Omni)
            {
                OmniblockUntil=C.Time+0.3;
                R.DurabilityCost=R.Outcome==ECombatDefenseOutcome::Block?FMath::Min(1.f,Durability):0;
                Durability-=R.DurabilityCost;
            }
        }
        Results.Add(R);
    }
    for (const auto& T:Threats) if (To>=T.ExpireTime)
        if (auto* E=Ledger.Find(T.Id); E && E->Result.Outcome==ECombatDefenseOutcome::Pending)
        { E->Result.Outcome=ECombatDefenseOutcome::Miss; E->Result.Time=T.ExpireTime; Results.Add(E->Result); }
    Threats.RemoveAll([&](const auto& T){return To>T.ExpireTime;});
    for (auto It=Ledger.CreateIterator();It;++It) if (To>It.Value().KeepUntil) It.RemoveCurrent();
}
FCombatDefenseResult FCombatDefenseResolver::FinalizeBodyHit(FGuid Id,double Time)
{
    auto* E=Ledger.Find(Id);
    if (!E)
    {
        E=&Ledger.Add(Id); E->Result.ThreatId=Id; E->KeepUntil=Time+2;
    }
    if (E->bBodyFinalized) return E->Result;
    E->bBodyFinalized=true;
    if (!E->Result.IsDefense() || E->Result.Time>Time)
    {
        E->Result.Outcome=ECombatDefenseOutcome::BodyHit; E->Result.Time=Time;
        E->Result.DamageScale=E->Result.ImpulseScale=1;
    }
    return E->Result;
}
