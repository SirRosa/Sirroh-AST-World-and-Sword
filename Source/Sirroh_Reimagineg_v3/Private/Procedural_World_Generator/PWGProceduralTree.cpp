#include "Procedural_World_Generator/PWGProceduralTree.h"
#include "Procedural_World_Generator/PWGv2Generator.h"
#include "ProceduralMeshComponent.h"
#include "Sword/SwordWorldSubsystem.h"
#include "Engine/World.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"

namespace
{
    constexpr float StepTime = 1.f / 60.f;
    FVector Axis(float Angle) { return FVector(FMath::Sin(Angle),0,FMath::Cos(Angle)); }
    float AngleOf(const FVector& V) { return FMath::Atan2(V.X,V.Z); }
    float Turn(float A,float B,float T) { return A+FMath::FindDeltaAngleRadians(A,B)*T; }
}

APWGProceduralTree::APWGProceduralTree()
{
    PrimaryActorTick.bCanEverTick=true;
    PrimaryActorTick.TickGroup=TG_PostPhysics;
    Mesh=CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Tree"));
    SetRootComponent(Mesh);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetCanEverAffectNavigation(false);
    Mesh->SetCastShadow(false);
    Mesh->SetLightingChannels(true,true,false);
}

EPWGTreeShape APWGProceduralTree::SelectShape(const TArray<FPWGTreeShapeWeight>& Weights,FName Region,FName Biome,FRandomStream& R)
{
    float Sum=0;
    for (const auto& W:Weights) if (FMath::IsFinite(W.Weight)) Sum+=FMath::Max(0.f,W.Weight);
    if (Sum>0)
    {
        float Pick=R.FRand()*Sum;
        for (const auto& W:Weights) if (W.Weight>0 && FMath::IsFinite(W.Weight) && (Pick-=W.Weight)<=0) return W.Shape;
    }
    return SelectShape(DefaultShapeWeights(Region,Biome),Region,Biome,R);
}
TArray<FPWGTreeShapeWeight> APWGProceduralTree::DefaultShapeWeights(FName Region,FName Biome)
{
    using S=EPWGTreeShape;
    TArray<S> Choices;
    const FString Context=(Region.ToString()+TEXT(" ")+Biome.ToString()).ToLower();
    if (Context.Contains(TEXT("mountain")) || Context.Contains(TEXT("pine")) || Context.Contains(TEXT("cold")) || Context.Contains(TEXT("tundra")))
        Choices={S::Conical,S::Pyramidal,S::Columnar,S::Tiered};
    else if (Context.Contains(TEXT("rock")) || Context.Contains(TEXT("granite"))) Choices={S::Tiered,S::Irregular,S::Columnar,S::MultiStem};
    else if (Context.Contains(TEXT("swamp"))) Choices={S::Weeping,S::MultiStem,S::Spreading,S::Vase};
    else if (Context.Contains(TEXT("desert")) || Context.Contains(TEXT("sunland"))) Choices={S::Umbrella,S::Vase,S::Irregular};
    else if (Context.Contains(TEXT("dead")) || Context.Contains(TEXT("scourge"))) Choices={S::Irregular,S::Weeping,S::Tiered};
    else Choices={S::Ovoid,S::Spherical,S::Spreading,S::Vase,S::Umbrella,S::MultiStem};
    TArray<FPWGTreeShapeWeight> Result;
    for (int32 I=0;I<12;++I)
    { FPWGTreeShapeWeight W; W.Shape=static_cast<S>(I); W.Weight=Choices.Contains(W.Shape)?1.f:0.f; Result.Add(W); }
    return Result;
}

void APWGProceduralTree::Initialize(APWGv2Generator* Generator,FName Id,const FPWGv2TreeRow& Row,
    EPWGTreeShape Shape,int32 Seed,float Height,float Wind,UMaterialInterface* Bark,UMaterialInterface* Crown,UMaterialInterface* Fruit,float FruitMultiplier,const FPWGTreeSave* Saved)
{
    Terrain=Generator; SaveWorldData=Generator->GetDebugWorld(); PersistentId=Id;
    Definition=Saved?Saved->Definition:Row; ShapeType=Saved?Saved->Shape:Shape;
    TreeSeed=Saved?Saved->Seed:Seed; TreeHeight=FMath::Clamp(Saved?Saved->Height:Height,200.f,1500.f);
    WindStrength=FMath::Clamp(Wind,0.f,2.f);
    Definition.BarkCells=FMath::Clamp(Definition.BarkCells,2,32);
    Definition.SegmentBudget=FMath::Clamp(Definition.SegmentBudget,64,512);
    Definition.MinimumSizeRatio=FMath::Clamp(Definition.MinimumSizeRatio,0.05f,0.5f);
    Definition.DepthStep=FMath::Clamp(Definition.DepthStep,0.01f,5.f);
    Definition.SleepDelay=FMath::Clamp(Definition.SleepDelay,1.f,20.f);
    Definition.BaseSegmentHealth=FMath::Max(1.f,Definition.BaseSegmentHealth);
    FRandomStream FruitRandom(TreeSeed^0xF817);
    HasFruit=Fruit && FruitRandom.FRand()<FMath::Clamp(Definition.FruitChance*FruitMultiplier,0.f,1.f);
    GroundBreakCount=Saved?Saved->GroundBreakCount:0;
    Generate();
    if (Saved && Saved->Segments.Num()==Segments.Num())
    {
        Dirty=true;
        for (int32 I=0;I<Segments.Num();++I)
        {
            auto& S=Segments[I]; const auto& V=Saved->Segments[I];
            if (V.A.ContainsNaN() || V.B.ContainsNaN()) continue;
            S.A=V.A; S.B=V.B; S.PreviousA=S.A-V.VelocityA*StepTime; S.PreviousB=S.B-V.VelocityB*StepTime;
            const float OldMaximum=Definition.BaseSegmentHealth*FMath::Max(0.08f,FMath::Square(S.Size/BaseSize));
            S.Health=Saved->DamageRevision<2?S.Health*FMath::Clamp(V.Health/FMath::Max(1.f,OldMaximum),0.f,1.f):FMath::Max(0.f,V.Health); S.Broken=V.Broken; S.Sleeping=V.Sleeping;
            S.Dynamic=S.Broken || (S.Parent!=INDEX_NONE && Segments[S.Parent].Dynamic);
        }
    }
    int32 Section=0;
    for (UMaterialInterface* Parent:{Bark,Crown,Fruit})
    {
        auto* Source=Cast<UMaterialInstanceDynamic>(Parent);
        auto* MID=Parent?UMaterialInstanceDynamic::Create(Source?Source->Parent.Get():Parent,this):nullptr;
        if (MID && Source) MID->CopyInterpParameters(Source);
        TreeMaterials.Add(MID); Mesh->SetMaterial(Section++,MID);
    }
    UpdateDepthLighting();
    PoseWind(GetWorld()->GetTimeSeconds());
    for (auto& S:Segments) { S.ContactA=GetActorTransform().TransformPosition(S.A); S.ContactB=GetActorTransform().TransformPosition(S.B); }
    UpdateMesh(); UpdateSurfaces();
}

void APWGProceduralTree::Generate()
{
    FRandomStream R(TreeSeed);
    Phase=R.FRand()*UE_TWO_PI;
    BaseSize=TreeHeight*FMath::Clamp(Definition.TrunkWidthRatio,0.09f,0.22f)*R.FRandRange(0.9f,1.1f);
    const float Minimum=BaseSize*Definition.MinimumSizeRatio;
    const int32 Budget=Definition.SegmentBudget;
    using S=EPWGTreeShape;
    // Every path inherits its parent's angle. Interpolated targets steer growth, never reset rotation.
    auto Path=[&](int32 Parent,float Length,float Width,float TargetAngle,float Droop,int32 MaxCount)->TArray<int32>
    {
        TArray<int32> Result;
        Width=FMath::Clamp(Width,Minimum,Parent==INDEX_NONE?BaseSize:Segments[Parent].Size);
        const int32 Available=Budget-Segments.Num();
        if (Available<2) return Result;
        const int32 Count=FMath::Clamp(FMath::RoundToInt(Length/(0.78f*(Width+Minimum)*0.5f)),2,FMath::Min(MaxCount,Available));
        FVector Current=Parent==INDEX_NONE?FVector::ZeroVector:Segments[Parent].RestB;
        float Angle=Parent==INDEX_NONE?TargetAngle:Segments[Parent].RestAngle;
        const float InitialAngle=Angle;
        const float Curl=R.FRandRange(-0.12f,0.12f);
        for (int32 I=0;I<Count;++I)
        {
            const float T=float(I)/FMath::Max(1,Count-1);
            float Desired=TargetAngle+Curl*T+Droop*FMath::SmoothStep(0.3f,1.f,T);
            Angle=Turn(Angle,Desired,I==0 && Parent!=INDEX_NONE?0.6f:0.32f)+R.FRandRange(-0.035f,0.035f);
            FSegment P; P.Parent=Parent;
            P.Size=FMath::Lerp(Width,Minimum,T); P.Length=P.Size*0.78f;
            P.RestAngle=Angle; P.RelativeAngle=FMath::FindDeltaAngleRadians(Parent==INDEX_NONE?InitialAngle:Segments[Parent].RestAngle,Angle);
            P.RestA=P.A=P.PreviousA=Current;
            P.RestB=P.B=P.PreviousB=Current+Axis(Angle)*P.Length;
            P.Cell=R.RandRange(1,Definition.BarkCells-1);
            P.Health=Definition.BaseSegmentHealth*FMath::Clamp(Definition.DurabilityMultiplier,1.f,20.f)*FMath::Max(0.22f,FMath::Square(P.Size/BaseSize));
            P.ContactA=GetActorTransform().TransformPosition(P.A); P.ContactB=GetActorTransform().TransformPosition(P.B);
            Parent=Segments.Add(P); Result.Add(Parent); Current=P.RestB;
        }
        Tips.Add(Result.Last());
        return Result;
    };
    const bool Multi=ShapeType==S::MultiStem;
    const float StemFraction=(ShapeType==S::Umbrella?0.68f:ShapeType==S::Vase?0.46f:ShapeType==S::Spreading?0.78f:1.f);
    TArray<int32> Main=Path(INDEX_NONE,TreeHeight*StemFraction,BaseSize,R.FRandRange(-0.06f,0.06f),ShapeType==S::Irregular?0.3f:0.f,48);
    TArray<TArray<int32>> Stems; Stems.Add(Main);
    if (Multi && Main.Num()>2)
    {
        for (float Side:{-1.f,1.f})
            Stems.Add(Path(Main[0],TreeHeight*0.92f,BaseSize*0.82f,Side*R.FRandRange(0.25f,0.4f),0,38));
    }
    const float Breadth=FMath::Clamp(Definition.CrownWidthRatio/0.6f,0.5f,1.7f)*R.FRandRange(0.87f,1.13f);
    TArray<TArray<int32>> Limbs;
    for (const auto& Stem:Stems)
    {
        if (Stem.Num()<3) continue;
        const int32 Levels=Multi?3:R.RandRange(4,6);
        const float Start=(ShapeType==S::Umbrella?0.86f:ShapeType==S::Vase?0.78f:ShapeType==S::Spreading?0.35f:0.22f);
        for (int32 L=0;L<Levels;++L)
        {
            const float T=FMath::Lerp(Start,0.93f,float(L)/FMath::Max(1,Levels-1));
            const int32 Parent=Stem[FMath::Clamp(FMath::RoundToInt(T*(Stem.Num()-1)),1,Stem.Num()-2)];
            const float U=FMath::Clamp((T-Start)/(1-Start),0.f,1.f);
            float Extent=0.34f,Angle=0.95f,Droop=0;
            switch (ShapeType)
            {
            case S::Conical: Extent=0.34f*(1-T)+0.04f; Angle=0.95f; break;
            case S::Pyramidal: Extent=0.50f*(1-T)+0.03f; Angle=1.28f; break;
            case S::Columnar: Extent=0.08f+0.04f*FMath::Sin(U*UE_PI); Angle=0.45f; break;
            case S::Ovoid: Extent=0.10f+0.25f*FMath::Pow(FMath::Sin(U*UE_PI),0.8f); Angle=0.8f; break;
            case S::Spherical: Extent=0.13f+0.30f*FMath::Sin(U*UE_PI); Angle=1.2f-U*0.55f; break;
            case S::Spreading: Extent=0.53f*(1-0.25f*U); Angle=1.35f; break;
            case S::Vase: Extent=0.43f+0.12f*U; Angle=0.6f+U*0.45f; break;
            case S::Umbrella: Extent=0.56f-0.16f*U; Angle=1.35f-U*0.35f; break;
            case S::Weeping: Extent=0.43f; Angle=0.7f; Droop=2.05f; break;
            case S::Tiered: Extent=0.45f*(1-T)+0.1f; Angle=1.48f; break;
            case S::Irregular: Extent=R.FRandRange(0.16f,0.48f); Angle=R.FRandRange(0.45f,1.6f); break;
            case S::MultiStem: Extent=0.18f; Angle=0.7f; break;
            }
            for (float Side:{-1.f,1.f})
            {
                if (Segments.Num()>Budget-12) break;
                const float Width=FMath::Max(Minimum,Segments[Parent].Size*R.FRandRange(0.72f,0.94f));
                auto Limb=Path(Parent,TreeHeight*Extent*Breadth*R.FRandRange(0.87f,1.13f),Width,
                    Segments[Parent].RestAngle+Side*Angle,Side*Droop,Multi?14:23);
                if (!Limb.IsEmpty()) Limbs.Add(MoveTemp(Limb));
            }
        }
    }
    // Secondary forks fill the crown from real wood instead of spawning detached foliage.
    const int32 PrimaryLimbCount=Limbs.Num();
    for (int32 LimbIndex=0;LimbIndex<PrimaryLimbCount;++LimbIndex)
    {
        const auto& Limb=Limbs[LimbIndex];
        if (Limb.Num()<4 || Segments.Num()>Budget-6) continue;
        const int32 Parent=Limb[Limb.Num()/2];
        const float Side=Segments[Parent].RestB.X>=0?1.f:-1.f;
        auto Fork=Path(Parent,TreeHeight*R.FRandRange(0.09f,0.16f),Segments[Parent].Size*0.78f,
            Segments[Parent].RestAngle-Side*0.65f,ShapeType==S::Weeping?Side*1.4f:0,10);
        if (!Fork.IsEmpty()) Limbs.Add(MoveTemp(Fork));
    }
    // Actual wood owns the crown. Shape only biases optional growth around those seeds.
    const float LeafSize=TreeHeight*R.FRandRange(0.14f,0.17f);
    auto GrowthPreference=[&](const FVector& Direction,const FVector& Position) -> float
    {
        const float Outward=Direction.X*(Position.X>=0?1.f:-1.f);
        switch (ShapeType)
        {
        case S::Conical: case S::Pyramidal: return 0.65f-0.25f*Direction.Z;
        case S::Columnar: return 0.85f-0.55f*FMath::Abs(Direction.X);
        case S::Umbrella: case S::Spreading: case S::Tiered: return 0.85f-0.55f*FMath::Abs(Direction.Z);
        case S::Weeping: return 0.60f-0.35f*Direction.Z;
        case S::Vase: return 0.55f+0.20f*Direction.Z+0.20f*Outward;
        default: return 0.65f;
        }
    };
    // Measure clearance in the same XZ plane as the rendered bark and foliage.
    // Bark cards are squares centred on their segments, including the overlapping joins.
    auto StemClearance=[&](const FVector& Centre)
    {
        float Clearance=MAX_flt;
        for (const auto& Stem:Stems)
        {
            for (const int32 Index:Stem)
            {
                const auto& Wood=Segments[Index];
                const FVector Delta=Centre-(Wood.RestA+Wood.RestB)*0.5;
                const FVector Up=Axis(Wood.RestAngle),Right(Up.Z,0,-Up.X);
                const float Half=Wood.Size*0.5f;
                const FVector2D Outside(
                    FMath::Max(0.0,FMath::Abs(FVector::DotProduct(Delta,Right))-Half),
                    FMath::Max(0.0,FMath::Abs(FVector::DotProduct(Delta,Up))-Half));
                Clearance=FMath::Min(Clearance,static_cast<float>(Outside.Size()));
            }
        }
        return Clearance;
    };
    const int32 CrownBudget=FMath::Clamp(Definition.CrownBudget,32,300);
    // Work inward from all tips together so early branches cannot spend the seed budget.
    // Keep the inner half of each limb clear around its junction with the stem.
    float CrownFloor=MAX_flt;
    int32 LongestLimb=0;
    for (const auto& Limb:Limbs)
    {
        LongestLimb=FMath::Max(LongestLimb,Limb.Num());
        CrownFloor=FMath::Min(CrownFloor,static_cast<float>(Segments[Limb[0]].RestA.Z));
    }
    for (int32 FromTip=0;FromTip<LongestLimb;FromTip+=2)
    {
        for (const auto& Limb:Limbs)
        {
            const int32 Index=Limb.Num()-1-FromTip;
            if (Index<Limb.Num()/2) continue;
            const int32 SeedLimit=FromTip==0?CrownBudget:CrownBudget/2;
            if (Leaves.Num()>=SeedLimit) break;
            FLeaf L;
            L.Segment=Limb[Index];
            L.Along=0.75f;
            const auto& Wood=Segments[L.Segment];
            L.RestCentre=FMath::Lerp(Wood.RestA,Wood.RestB,L.Along);
            if (Leaves.ContainsByPredicate([&](const FLeaf& Other){
                return FVector::DistSquared(Other.RestCentre,L.RestCentre)<FMath::Square(Other.Size*0.45f);
            })) continue;
            // A drooping branch may end below its own fork. Keep that tip covered.
            L.Size=FMath::Min(LeafSize*R.FRandRange(0.94f,1.04f),static_cast<float>(L.RestCentre.Z)*1.8f);
            const float Clearance=StemClearance(L.RestCentre);
            // Short upright twigs still get a tuft. Draw it behind bark when the
            // twig itself lies within the projected stem.
            L.Size=FMath::Min(L.Size,FMath::Max(Wood.Size,2.f*Clearance));
            if (L.Size<=UE_KINDA_SMALL_NUMBER) continue;
            CrownFloor=FMath::Min(CrownFloor,static_cast<float>(L.RestCentre.Z)-L.Size*0.5f);
            L.Phase=R.FRand()*UE_TWO_PI; L.Influence=0.7f; L.BehindWood=R.FRand()<0.4f || Clearance<=L.Size*0.5f;
            Leaves.Add(L);
        }
    }
    CrownFloor=FMath::Max(0.f,CrownFloor);
    for (int32 Parent=0;Parent<Leaves.Num() && Leaves.Num()<CrownBudget;++Parent)
    {
        const FLeaf Source=Leaves[Parent];
        if (Source.Generation>=3) continue;
        const float RestAngle=Segments[Source.Segment].RestAngle;
        const FVector Up=Axis(RestAngle),Right(Up.Z,0,-Up.X);
        const float StartAngle=R.FRand()*UE_TWO_PI;
        for (int32 K=0;K<8 && Leaves.Num()<CrownBudget;++K)
        {
            const float Angle=StartAngle+UE_TWO_PI*K/8;
            FLeaf L; L.ParentLeaf=Parent; L.Segment=Source.Segment; L.Generation=Source.Generation+1;
            L.Size=FMath::Min(Source.Size,LeafSize*R.FRandRange(0.88f,0.98f)*FMath::Pow(0.94f,float(L.Generation)));
            const FVector Direction(FMath::Cos(Angle),0,FMath::Sin(Angle));
            // Substantial overlap guarantees connected foliage even while the parent bends.
            const FVector Offset=Direction*(Source.Size+L.Size)*0.29f;
            L.RestCentre=Source.RestCentre+Offset;
            if (L.RestCentre.Z-L.Size*0.5f<CrownFloor) continue;
            // Keep a local, irregular lobe around its actual supporting branch, never a template fill.
            const auto& Wood=Segments[L.Segment];
            const FVector Anchor=FMath::Lerp(Wood.RestA,Wood.RestB,0.75f);
            if (FVector::DistSquared(L.RestCentre,Anchor)>FMath::Square(LeafSize*1.15f)) continue;
            if (R.FRand()>GrowthPreference(Direction,L.RestCentre)/(1.f+Source.Generation*0.35f)) continue;
            if (Leaves.ContainsByPredicate([&](const FLeaf& Other){return FVector::DistSquared(Other.RestCentre,L.RestCentre)<FMath::Square((Other.Size+L.Size)*0.23f);})) continue;
            if (StemClearance(L.RestCentre)<L.Size*0.5f) continue;
            L.ParentOffset=FVector2D(FVector::DotProduct(Offset,Right),FVector::DotProduct(Offset,Up));
            L.Influence=FMath::Min(0.9f,0.72f+L.Generation*0.045f); L.Phase=R.FRand()*UE_TWO_PI;
            L.BehindWood=Source.BehindWood; // coherent patches behind bark, not random pixel holes
            Leaves.Add(L);
        }
    }
    for (int32 I=0;I<Leaves.Num();++I)
    {
        // Both stacks have unique non-wrapping depths; no crown shares the bark plane.
        Leaves[I].Depth=Leaves[I].BehindWood?(I+1)*Definition.DepthStep:
            -(Segments.Num()+Tips.Num()+I+1)*Definition.DepthStep;
    }
}

void APWGProceduralTree::PoseWind(float Time)
{
    const float Gust=(0.7f+0.3f*FMath::Sin(Time*0.23f+Phase))*WindStrength;
    const float HitAge=Time-HitTime;
    const float Shake=HitAge>=0 && HitAge<1.5f?HitAmplitude*FMath::Exp(-HitAge*4.5f)*FMath::Sin(HitAge*27):0.f;
    TArray<float,TInlineAllocator<512>> Deflections;
    for (int32 I=0;I<Segments.Num();++I)
    {
        auto& S=Segments[I];
        const float Influence=FMath::Clamp(1-S.Size/BaseSize,0.f,1.f);
        const float HeightAlpha=FMath::Clamp(float(S.RestB.Z/TreeHeight),0.f,1.f);
        const float Wind=FMath::DegreesToRadians(FMath::Clamp(Definition.WindBendDegrees,0.f,20.f)*0.25f)*Gust*Influence*HeightAlpha*FMath::Sin(Time*0.75f+Phase-HeightAlpha*0.5f);
        const float Response=Influence*FMath::Exp(-float(FVector::Distance(S.RestB,HitPosition))/TreeHeight);
        Deflections.Add(S.Parent==INDEX_NONE?0.f:Wind+Shake*Response);
        if (S.Dynamic) continue;
        const float ParentAngle=S.Parent==INDEX_NONE?S.RestAngle:AngleOf(Segments[S.Parent].B-Segments[S.Parent].A)+S.RelativeAngle;
        // Difference of absolute deflections: a 30-segment limb must not bend 30 times farther.
        const float Bend=Deflections[I]-(S.Parent==INDEX_NONE?0.f:Deflections[S.Parent]);
        S.A=S.Parent==INDEX_NONE?S.RestA:Segments[S.Parent].B;
        S.B=S.A+Axis(ParentAngle+Bend)*S.Length;
        S.PreviousA=S.A; S.PreviousB=S.B;
    }
}

int32 APWGProceduralTree::FragmentRoot(int32 I) const
{
    while (Segments[I].Parent!=INDEX_NONE && !Segments[I].Broken) I=Segments[I].Parent;
    return I;
}
void APWGProceduralTree::WakeFragment(int32 Index,const FVector& Impulse)
{
    const int32 Root=FragmentRoot(Index);
    FVector V=GetActorTransform().InverseTransformVectorNoScale(Impulse); V.Y=0; V=V.GetClampedToMaxSize(90);
    for (int32 I=0;I<Segments.Num();++I) if (FragmentRoot(I)==Root && Segments[I].Dynamic)
    {
        auto& S=Segments[I]; S.Sleeping=false; S.QuietTime=0;
        S.PreviousA=S.A-((S.A-S.PreviousA)/StepTime+V).GetClampedToMaxSize(1200)*StepTime;
        S.PreviousB=S.B-((S.B-S.PreviousB)/StepTime+V).GetClampedToMaxSize(1200)*StepTime;
    }
}
void APWGProceduralTree::BreakJoint(int32 Index,const FVector& Impulse,bool GroundImpact)
{
    Segments[Index].Broken=true;
    for (int32 I=Index;I<Segments.Num();++I)
    {
        auto& S=Segments[I]; S.Dynamic=S.Broken || (S.Parent!=INDEX_NONE && Segments[S.Parent].Dynamic);
    }
    WakeFragment(Index,Impulse);
    // A coherent angular impulse tips the whole connected body, not just its first block.
    if (!GroundImpact)
    {
        const FVector Pivot=Segments[Index].A;
        const float Spin=Impulse.X>=0?0.12f:-0.12f;
        for (int32 I=Index;I<Segments.Num();++I) if (FragmentRoot(I)==Index)
        {
            auto& S=Segments[I];
            const FVector A=S.A-Pivot,B=S.B-Pivot;
            S.PreviousA-=FVector(A.Z,0,-A.X)*Spin*StepTime;
            S.PreviousB-=FVector(B.Z,0,-B.X)*Spin*StepTime;
        }
    }
    Dirty=true;
}
void APWGProceduralTree::DamageAt(FVector WorldPoint,float Damage,FVector Impulse)
{
    if (!HasAuthority() || !FMath::IsFinite(Damage) || Damage<=0 || WorldPoint.ContainsNaN() || Impulse.ContainsNaN()) return;
    const FVector P=GetActorTransform().InverseTransformPosition(WorldPoint);
    int32 Nearest=INDEX_NONE; double Best=DBL_MAX;
    for (int32 I=0;I<Segments.Num();++I)
    {
        const auto& S=Segments[I];
        const double Distance=FVector::DistSquared(P,FMath::ClosestPointOnSegment(P,S.A,S.B));
        if (Distance<Best) { Best=Distance; Nearest=I; }
    }
    if (Nearest==INDEX_NONE || Best>FMath::Square(double(Segments[Nearest].Size+40))) return;
    // A severed block does not grant a free cut of a healthy neighbouring joint.
    if (Segments[Nearest].Broken)
        for (int32 I=Nearest+1;I<Segments.Num();++I)
            if (Segments[I].Parent==Nearest && !Segments[I].Broken) { Nearest=I; break; }
    auto& S=Segments[Nearest]; S.Health=FMath::Max(0.f,S.Health-Damage); Dirty=true;
    HitTime=GetWorld()->GetTimeSeconds(); HitPosition=S.RestB;
    HitAmplitude=FMath::DegreesToRadians(FMath::Clamp(Damage*0.045f,0.6f,2.4f))*(Impulse.X>=0?1.f:-1.f);
    if (S.Dynamic) WakeFragment(Nearest,Impulse*0.12f);
    SetActorTickInterval(0.f);
    if (S.Health<=0 && !S.Broken) BreakJoint(Nearest,Impulse*0.12f);
    CaptureState();
}
float APWGProceduralTree::TakeDamage(float Damage,const FDamageEvent& Event,AController* EventInstigator,AActor* Causer)
{
    if (!HasAuthority()) return 0;
    FVector Point=GetActorLocation(),Direction=Causer?(Point-Causer->GetActorLocation()).GetSafeNormal():FVector::XAxisVector;
    if (Event.IsOfType(FPointDamageEvent::ClassID))
    {
        const auto& Hit=static_cast<const FPointDamageEvent&>(Event); Point=Hit.HitInfo.ImpactPoint; Direction=Hit.ShotDirection;
    }
    DamageAt(Point,Damage,Direction*Damage*3);
    return Damage;
}

float APWGProceduralTree::GroundHeight(const FVector& P) const
{
    if (!Terrain) return 0;
    const FVector W=GetActorTransform().TransformPosition(P);
    return GetActorTransform().InverseTransformPosition(FVector(W.X,W.Y,Terrain->GetSurfaceHeightAt(W.X,W.Y))).Z;
}
void APWGProceduralTree::Simulate(float Dt)
{
    // Each connected component is one rigid body. Internal joints cannot drift apart,
    // and terrain impulses act on the component rather than separating its blocks.
    TMap<int32,TArray<int32>> Groups;
    for (int32 I=0;I<Segments.Num();++I)
        if (Segments[I].Dynamic && !Segments[I].Sleeping) Groups.FindOrAdd(FragmentRoot(I)).Add(I);
    int32 ImpactBreak=INDEX_NONE; double StrongestImpact=0;
    const float Time=GetWorld()->GetTimeSeconds();
    for (const auto& Group:Groups)
    {
        auto& Root=Segments[Group.Key];
        double Mass=0,Inertia=0;
        FVector Centre=FVector::ZeroVector,Velocity=FVector::ZeroVector;
        for (int32 I:Group.Value)
        {
            const auto& S=Segments[I]; const double M=FMath::Square(S.Size);
            Mass+=M; Centre+=(S.A+S.B)*0.5*M;
            Velocity+=((S.A-S.PreviousA)+(S.B-S.PreviousB))*(0.5*M/Dt);
        }
        Centre/=Mass; Velocity/=Mass;
        double Spin=0;
        for (int32 I:Group.Value)
        {
            const auto& S=Segments[I]; const double M=FMath::Square(S.Size)*0.5;
            for (int32 End=0;End<2;++End)
            {
                const FVector R=(End?S.B:S.A)-Centre;
                const FVector V=((End?S.B:S.A)-(End?S.PreviousB:S.PreviousA))/Dt-Velocity;
                Inertia+=M*(R.X*R.X+R.Z*R.Z);
                Spin+=M*(R.Z*V.X-R.X*V.Z);
            }
        }
        Inertia=FMath::Max(Inertia,Mass);
        Spin=FMath::Clamp(Spin/Inertia,-3.0,3.0)*0.997;
        Velocity*=0.997; Velocity.Z-=980*Dt;
        const float Angle=AngleOf(Root.B-Root.A)-Root.RestAngle+float(Spin*Dt);
        const double C=FMath::Cos(Angle),N=FMath::Sin(Angle);
        auto Rotate=[&](const FVector& P){return FVector(C*P.X+N*P.Z,0,-N*P.X+C*P.Z);};
        FVector RestCentre=FVector::ZeroVector;
        for (int32 I:Group.Value)
        {
            const auto& S=Segments[I];
            RestCentre+=(S.RestA+S.RestB)*(0.5*FMath::Square(S.Size)/Mass);
        }
        Centre+=Velocity*Dt;
        for (int32 I:Group.Value)
        {
            auto& S=Segments[I];
            S.A=Centre+Rotate(S.RestA-RestCentre); S.B=Centre+Rotate(S.RestB-RestCentre);
            S.Grounded=false;
        }
        // Resolve the deepest supporting contact for the entire body. Multiple passes
        // distribute impulses across a lying trunk without changing any internal joint.
        double Lift=0;
        TArray<FVector> Contacts;
        for (int32 I:Group.Value)
        {
            auto& S=Segments[I];
            for (const FVector& P:{S.A,S.B})
            {
                const double Penetration=GroundHeight(P)+S.Size*0.5-P.Z;
                if (Penetration>=-1.0)
                {
                    Contacts.Add(P); S.Grounded=true;
                    const double Speed=FMath::Max(0.0,-Velocity.Z+Spin*(P.X-Centre.X));
                    if (GroundBreakCount<2 && Time>=NextGroundBreakTime && !S.Broken &&
                        Tips.Contains(I) && S.Size<BaseSize*0.28f && Speed>StrongestImpact &&
                        Speed>FMath::Max(50.f,Definition.ImpactBreakSpeed)*FMath::Clamp(Definition.GroundImpactResistance,1.f,10.f))
                    { ImpactBreak=I; StrongestImpact=Speed; }
                }
                Lift=FMath::Max(Lift,Penetration);
            }
        }
        Centre.Z+=Lift;
        for (int32 Pass=0;Pass<8;++Pass) for (const FVector& P:Contacts)
        {
            const FVector R=P+FVector(0,0,Lift)-Centre;
            const double NormalSpeed=Velocity.Z-Spin*R.X;
            if (NormalSpeed>=0) continue;
            const double J=-NormalSpeed/(1.0/Mass+R.X*R.X/Inertia);
            Velocity.Z+=J/Mass; Spin-=R.X*J/Inertia;
            const double TangentSpeed=Velocity.X+Spin*R.Z;
            const double Friction=FMath::Clamp(-TangentSpeed/(1.0/Mass+R.Z*R.Z/Inertia),-J*0.65,J*0.65);
            Velocity.X+=Friction/Mass; Spin+=R.Z*Friction/Inertia;
        }
        float MaxSpeed=0;
        for (int32 I:Group.Value)
        {
            auto& S=Segments[I]; S.A.Z+=Lift; S.B.Z+=Lift;
            const FVector A=S.A-Centre,B=S.B-Centre;
            const FVector VA=Velocity+FVector(A.Z,0,-A.X)*Spin,VB=Velocity+FVector(B.Z,0,-B.X)*Spin;
            S.PreviousA=S.A-VA*Dt; S.PreviousB=S.B-VB*Dt;
            MaxSpeed=FMath::Max(MaxSpeed,float(FMath::Max(VA.Size(),VB.Size())));
        }
        Root.QuietTime=!Contacts.IsEmpty() && MaxSpeed<16?Root.QuietTime+Dt:0;
        if (Root.QuietTime>=Definition.SleepDelay)
        {
            for (int32 I:Group.Value)
            { auto& S=Segments[I]; S.Sleeping=true; S.PreviousA=S.A; S.PreviousB=S.B; }
            CaptureState();
        }
    }
    if (ImpactBreak!=INDEX_NONE)
    {
        ++GroundBreakCount; NextGroundBreakTime=Time+0.75f;
        BreakJoint(ImpactBreak,FVector::ZeroVector,true);
    }
}

void APWGProceduralTree::UpdateMesh()
{
    struct FBuffer { TArray<FVector> V,N; TArray<int32> T; TArray<FVector2D> UV; TArray<FColor> C; TArray<FProcMeshTangent> Tangents; };
    FBuffer Bark,Foliage,Fruit;
    int32 Rank=0;
    auto Vertex=[&](FBuffer& M,FVector P,FVector2D UV)
    { M.V.Add(P); M.UV.Add(UV); M.N.Add(-FVector::YAxisVector); M.C.Add(FColor::White); M.Tangents.Add(FProcMeshTangent(FVector::XAxisVector,false)); };
    auto Square=[&](FVector Centre,FVector Up,float Size,int32 Cell)
    {
        Centre.Y-=++Rank*Definition.DepthStep;
        const FVector Right(Up.Z,0,-Up.X); const int32 Base=Bark.V.Num();
        const float V0=float(Cell)/Definition.BarkCells,V1=float(Cell+1)/Definition.BarkCells;
        Vertex(Bark,Centre-Right*Size*0.5f-Up*Size*0.5f,FVector2D(0,V1));
        Vertex(Bark,Centre+Right*Size*0.5f-Up*Size*0.5f,FVector2D(1,V1));
        Vertex(Bark,Centre+Right*Size*0.5f+Up*Size*0.5f,FVector2D(1,V0));
        Vertex(Bark,Centre-Right*Size*0.5f+Up*Size*0.5f,FVector2D(0,V0));
        Bark.T.Append({Base,Base+1,Base+2,Base,Base+2,Base+3});
    };
    for (const auto& S:Segments) Square((S.A+S.B)*0.5f,(S.B-S.A).GetSafeNormal(),S.Size,S.Cell);
    for (int32 I:Tips)
    {
        const auto& S=Segments[I]; const FVector Up=(S.B-S.A).GetSafeNormal();
        Square(S.B+Up*S.Size*0.20f,Up,S.Size,0);
    }
    const float Time=GetWorld()->GetTimeSeconds();
    TArray<FVector> CrownCentres; TArray<float> CrownAngles;
    CrownCentres.Reserve(Leaves.Num()); CrownAngles.Reserve(Leaves.Num());
    for (const auto& L:Leaves)
    {
        const auto& S=Segments[L.Segment]; const FVector Up=(S.B-S.A).GetSafeNormal();
        const float Flutter=S.Sleeping?0:FMath::Sin(Time*1.5f+L.Phase)*WindStrength*L.Influence*0.025f;
        float Angle=AngleOf(Up)+Flutter;
        FVector Centre=FMath::Lerp(S.A,S.B,L.Along);
        if (L.ParentLeaf!=INDEX_NONE)
        {
            const float ParentAngle=CrownAngles[L.ParentLeaf];
            const FVector ParentUp=Axis(ParentAngle),ParentRight(ParentUp.Z,0,-ParentUp.X);
            Centre=CrownCentres[L.ParentLeaf]+ParentRight*L.ParentOffset.X+ParentUp*L.ParentOffset.Y;
            Angle=ParentAngle+Flutter*0.2f;
        }
        CrownCentres.Add(Centre); CrownAngles.Add(Angle);
        const FVector LeafUp=Axis(Angle),LeafRight(LeafUp.Z,0,-LeafUp.X);
        Centre.Y+=L.Depth;
        const int32 Base=Foliage.V.Num(); Vertex(Foliage,Centre,FVector2D(0.5,0.5));
        constexpr int32 Sides=20;
        for (int32 K=0;K<Sides;++K)
        {
            const float A=UE_TWO_PI*K/Sides;
            Vertex(Foliage,Centre+(LeafRight*FMath::Cos(A)+LeafUp*FMath::Sin(A))*L.Size*0.5f,
                FVector2D(0.5f+0.5f*FMath::Cos(A),0.5f-0.5f*FMath::Sin(A)));
        }
        for (int32 K=0;K<Sides;++K) Foliage.T.Append({Base,Base+1+K,Base+1+(K+1)%Sides});
    }
    if (HasFruit && !Leaves.IsEmpty())
    {
        const auto& L=Leaves[0]; const auto& S=Segments[L.Segment];
        FVector Centre=FMath::Lerp(S.A,S.B,L.Along); Centre.Y=-(Segments.Num()+Tips.Num()+Leaves.Num()+1)*Definition.DepthStep;
        const FVector Up=(S.B-S.A).GetSafeNormal(),Right(Up.Z,0,-Up.X);
        const float Half=FMath::Clamp(L.Size*0.24f,15.f,45.f);
        Vertex(Fruit,Centre-Right*Half-Up*Half,FVector2D(0,1));
        Vertex(Fruit,Centre+Right*Half-Up*Half,FVector2D(1,1));
        Vertex(Fruit,Centre+Right*Half+Up*Half,FVector2D(1,0));
        Vertex(Fruit,Centre-Right*Half+Up*Half,FVector2D(0,0));
        Fruit.T.Append({0,1,2,0,2,3});
    }
    int32 Section=0;
    for (FBuffer* M:{&Bark,&Foliage,&Fruit})
    {
        if (M->V.IsEmpty()) { ++Section; continue; }
        if (!MeshCreated) Mesh->CreateMeshSection(Section,M->V,M->T,M->N,M->UV,M->C,M->Tangents,false);
        else Mesh->UpdateMeshSection(Section,M->V,M->N,M->UV,M->C,M->Tangents);
        ++Section;
    }
    MeshCreated=true;
}
void APWGProceduralTree::UpdateSurfaces()
{
    auto* World=GetWorld()->GetSubsystem<USwordWorldSubsystem>();
    World->RemoveSurfaces(this);
    for (auto& S:Segments)
    {
        const FVector A=GetActorTransform().TransformPosition(S.A),B=GetActorTransform().TransformPosition(S.B);
        World->RegisterSurface(this,A,B,S.Size*0.5f,GetActorLocation().Y,&S.ContactA,&S.ContactB);
        S.ContactA=A; S.ContactB=B;
    }
}
void APWGProceduralTree::UpdateDepthLighting()
{
    if (!Terrain) return;
    const APawn* Pawn=UGameplayStatics::GetPlayerPawn(this,0);
    const float PlayerY=Pawn?Terrain->GetActorTransform().InverseTransformPosition(Pawn->GetActorLocation()).Y:0;
    const float TreeY=Terrain->GetActorTransform().InverseTransformPosition(GetActorLocation()).Y;
    const float Depth=TreeY-PlayerY;
    const float Neutral=35.f;
    float Brightness=1,Fog=0,Desaturation=0;
    const bool GlobalDepth=Terrain->HasGlobalDepthAtmosphere();
    if (!GlobalDepth && Depth>Neutral)
    {
        const float T=FMath::Pow(FMath::Clamp((Depth-Neutral)/FMath::Max(200.f,Terrain->Settings.BackgroundDepth-Neutral),0.f,1.f),0.55f);
        Fog=T*0.82f; Desaturation=T*0.65f;
    }
    else if (!GlobalDepth && Depth<-Neutral)
    {
        const float T=FMath::Pow(FMath::Clamp((-Depth-Neutral)/FMath::Max(200.f,Terrain->Settings.ForegroundDepth-Neutral),0.f,1.f),0.45f);
        Brightness=FMath::Lerp(1.f,0.012f,T);
    }
    for (UMaterialInstanceDynamic* MID:TreeMaterials) if (MID)
    {
        MID->SetScalarParameterValue(TEXT("TreeBrightness"),Brightness);
        MID->SetScalarParameterValue(TEXT("TreeFogAmount"),Fog);
        MID->SetScalarParameterValue(TEXT("TreeDesaturation"),Desaturation);
    }
}
void APWGProceduralTree::Tick(float Delta)
{
    Super::Tick(Delta);
    if (Segments.IsEmpty()) return;
    if (!Mesh->IsVisible() && !Segments.ContainsByPredicate([](const FSegment& S){ return S.Dynamic && !S.Sleeping; }))
    {
        // Wind pose uses absolute world time. Resume on screen without replaying missed steps.
        SetActorTickInterval(0.12f);
        return;
    }
    UpdateDepthLighting();
    const bool Moving=Segments.ContainsByPredicate([](const FSegment& S){ return !S.Dynamic || !S.Sleeping; });
    if (!Moving) { SetActorTickInterval(0.15f); return; }
    const bool Physics=Segments.ContainsByPredicate([](const FSegment& S){ return S.Dynamic && !S.Sleeping; });
    const APawn* Pawn=UGameplayStatics::GetPlayerPawn(this,0);
    SetActorTickInterval(!Physics && Pawn && FVector::DistSquared(Pawn->GetActorLocation(),GetActorLocation())>FMath::Square(4000.f)?0.12f:0.f);
    PoseWind(GetWorld()->GetTimeSeconds());
    Accumulator=FMath::Min(Accumulator+Delta,0.1f);
    while (Accumulator>=StepTime) { if (Physics) Simulate(StepTime); Accumulator-=StepTime; }
    UpdateMesh(); UpdateSurfaces();
}
void APWGProceduralTree::CaptureState()
{
    if (!Dirty || !SaveWorldData) return;
    auto& Save=SaveWorldData->DamagedTrees.FindOrAdd(PersistentId);
    Save.DamageRevision=2; Save.GroundBreakCount=GroundBreakCount;
    Save.Seed=TreeSeed; Save.Shape=ShapeType; Save.Height=TreeHeight; Save.Definition=Definition;
    Save.Segments.Reset(Segments.Num());
    for (const auto& S:Segments)
    {
        auto& P=Save.Segments.AddDefaulted_GetRef(); P.A=S.A; P.B=S.B;
        P.VelocityA=(S.A-S.PreviousA)/StepTime; P.VelocityB=(S.B-S.PreviousB)/StepTime;
        P.Health=S.Health; P.Broken=S.Broken; P.Sleeping=S.Sleeping;
    }
}
void APWGProceduralTree::EndPlay(const EEndPlayReason::Type Reason)
{
    CaptureState();
    if (GetWorld()) GetWorld()->GetSubsystem<USwordWorldSubsystem>()->RemoveSurfaces(this);
    Super::EndPlay(Reason);
}
