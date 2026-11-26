#include "Sword/SwordWorldSubsystem.h"
#include "Sword/ProceduralSword.h"
#include "Materials/MaterialInterface.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

namespace SwordGrid
{
    constexpr double CellSize=256.0;
    void Cells(FVector A,FVector B,float Radius,TFunctionRef<void(FIntPoint)> Visit)
    {
        const int32 MinX=FMath::FloorToInt((FMath::Min(A.X,B.X)-Radius)/CellSize);
        const int32 MaxX=FMath::FloorToInt((FMath::Max(A.X,B.X)+Radius)/CellSize);
        const int32 MinZ=FMath::FloorToInt((FMath::Min(A.Z,B.Z)-Radius)/CellSize);
        const int32 MaxZ=FMath::FloorToInt((FMath::Max(A.Z,B.Z)+Radius)/CellSize);
        for (int32 X=MinX;X<=MaxX;++X) for (int32 Z=MinZ;Z<=MaxZ;++Z) Visit(FIntPoint(X,Z));
    }
}
void USwordWorldSubsystem::RegisterSurface(AActor* Owner,FVector A,FVector B,float Radius,float LaneY,const FVector* PreviousA,const FVector* PreviousB)
{
    if (!Owner) return;
    // Moving trees refresh their surfaces every frame. Reuse IDs instead of eventually overflowing.
    const int32 Id=FreeSurfaceIds.IsEmpty()?NextSurfaceId++:FreeSurfaceIds.Pop(EAllowShrinking::No);
    const FVector PA=PreviousA?*PreviousA:A,PB=PreviousB?*PreviousB:B;
    Surfaces.Add(Id,{Owner,A,B,FMath::Max(0.5f,Radius),LaneY,PA,PB});
    OwnerSurfaces.FindOrAdd(Owner).Add(Id);
    SwordGrid::Cells(A.ComponentMin(B).ComponentMin(PA).ComponentMin(PB),A.ComponentMax(B).ComponentMax(PA).ComponentMax(PB),Radius,[&](FIntPoint Cell){Grid.FindOrAdd(Cell).Add(Id);});
}
void USwordWorldSubsystem::RemoveSurfaces(AActor* Owner)
{
    if (TArray<int32>* Ids=OwnerSurfaces.Find(Owner))
    {
        for (int32 Id:*Ids) if (const FSwordSurface* S=Surfaces.Find(Id))
        {
            SwordGrid::Cells(S->A.ComponentMin(S->B).ComponentMin(S->PreviousA).ComponentMin(S->PreviousB),S->A.ComponentMax(S->B).ComponentMax(S->PreviousA).ComponentMax(S->PreviousB),S->Radius,[&](FIntPoint Cell)
            {
                if (auto* Bucket=Grid.Find(Cell)) { Bucket->RemoveSwap(Id); if (Bucket->IsEmpty()) Grid.Remove(Cell); }
            });
            Surfaces.Remove(Id);
            FreeSurfaceIds.Add(Id);
        }
        OwnerSurfaces.Remove(Owner);
    }
}
bool USwordWorldSubsystem::FindSurface(const FVector& BladeStart,const FVector& BladeEnd,float Radius,FVector& Point,AActor*& Owner,float& LaneY)
{
    TSet<int32> Candidates;
    SwordGrid::Cells(BladeStart,BladeEnd,Radius,[&](FIntPoint Cell)
    { if (const auto* Bucket=Grid.Find(Cell)) for(int32 Id:*Bucket) Candidates.Add(Id); });
    bool bHit=false;
    double Best=DBL_MAX;
    FVector A=BladeStart,B=BladeEnd; A.Y=B.Y=0;
    for (int32 Id:Candidates)
    {
        const FSwordSurface* S=Surfaces.Find(Id);
        if (!S || !S->Owner.IsValid()) continue;
        FVector C=S->A,D=S->B; C.Y=D.Y=0;
        FVector OnBlade,OnSurface;
        FMath::SegmentDistToSegmentSafe(A,B,C,D,OnBlade,OnSurface);
        const double Distance=FVector::DistSquared(OnBlade,OnSurface);
        if (Distance>FMath::Square(Radius+S->Radius)) continue;
        const double TipDistance=FVector::DistSquared(OnBlade,B);
        if (TipDistance<Best)
        {
            Best=TipDistance; bHit=true; Owner=S->Owner.Get();
            Point=OnSurface+(OnBlade-OnSurface).GetSafeNormal()*S->Radius;
            Point.Y=FMath::Lerp(S->A.Y,S->B.Y,0.5);
            LaneY=S->LaneY;
        }
    }
    return bHit;
}
void USwordWorldSubsystem::QueryCombatSurfaces(const FVector& Min,const FVector& Max,TArray<FSwordSurface>& Out) const
{
    TSet<int32> Candidates;
    SwordGrid::Cells(Min,Max,0,[&](FIntPoint Cell)
    { if (const auto* Bucket=Grid.Find(Cell)) for (int32 Id:*Bucket) Candidates.Add(Id); });
    for (int32 Id:Candidates)
        if (const FSwordSurface* Surface=Surfaces.Find(Id))
            if (Surface->Owner.IsValid()) Out.Add(*Surface);
}
void USwordWorldSubsystem::LoadData()
{
    if (bLoaded) return;
    bLoaded=true;
    SpawnRules=LoadObject<UDataTable>(nullptr,TEXT("/Game/Systems/Procedural_Sword_Generator/DT_SwordSpawnRules.DT_SwordSpawnRules"));
    CombinationRules=LoadObject<UDataTable>(nullptr,TEXT("/Game/Systems/Procedural_Sword_Generator/DT_SwordCombinations.DT_SwordCombinations"));
}
FSwordSpawnRule USwordWorldSubsystem::RuleFor(int32 SurfaceStyle,FName Context)
{
    LoadData();
    if (SpawnRules)
    {
        TArray<FName> Names=SpawnRules->GetRowNames(); Names.Sort(FNameLexicalLess());
        for (FName Name:Names)
        {
            const auto* Row=SpawnRules->FindRow<FSwordSpawnRule>(Name,TEXT("Sword rule"));
            if (Row && Row->SurfaceStyle==SurfaceStyle && Row->SpawnContext==Context) return *Row;
        }
    }
    const int32 Mapping[]={0,1,2,3,7,5,4,6}; // PWG: Mini1..4,Rock,Ground,Mini5,Mini6.
    FSwordSpawnRule Rule;
    Rule.SurfaceStyle=SurfaceStyle; Rule.SpawnContext=Context;
    Rule.Culture=static_cast<ESwordCulture>(Mapping[FMath::Clamp(SurfaceStyle,0,7)]);
    Rule.Style=static_cast<ESwordStyle>(static_cast<uint8>(Rule.Culture));
    return Rule;
}
FSwordInstance USwordWorldSubsystem::Generate(int32 SurfaceStyle,int32 Seed,bool bUnrestricted)
{
    LoadData();
    FRandomStream Random(Seed);
    FSwordSpawnRule Rule=RuleFor(SurfaceStyle);
    auto Select=[&](int32 Part)->FSwordPart
    {
        if (bUnrestricted) return FSwordPart::FromIndex(Random.RandRange(0,63));
        FSwordSpawnRule Source=Rule;
        if (Random.FRand()<FMath::Clamp(Rule.ForeignPartChance,0.0f,1.0f))
        {
            TArray<FSwordSpawnRule,TInlineAllocator<8>> Foreign;
            for (int32 Mini=0;Mini<8;++Mini) if (Mini!=SurfaceStyle)
            { auto Other=RuleFor(Mini); if (Other.bEnabled) Foreign.Add(Other); }
            if (!Foreign.IsEmpty()) Source=Foreign[Random.RandRange(0,Foreign.Num()-1)];
        }
        const TArray<FSwordWeightedPart>& Options=Part==0?Source.Blades:(Part==1?Source.Guards:Source.Hilts);
        double Sum=0; for (const auto& Entry:Options) Sum+=FMath::Max(0.0f,Entry.Weight);
        double Pick=Random.FRand()*Sum;
        if (Sum>0) for (const auto& Entry:Options)
        {
            if (Entry.Weight<=0) continue;
            Pick-=Entry.Weight;
            if (Pick<=0) return Entry.Part;
        }
        FSwordPart P; P.Culture=Source.Culture; P.Style=Source.Style; return P;
    };
    FSwordInstance Result; Result.Seed=Seed; Result.InstanceId=FGuid::NewGuid();
    Result.Blade=Select(0); Result.Guard=Select(1); Result.Hilt=Select(2); Result.BladeVariant=Random.RandRange(0,2);
    int32 Priority=MIN_int32;
    if (CombinationRules)
    {
        TArray<FName> Names=CombinationRules->GetRowNames(); Names.Sort(FNameLexicalLess());
        for (FName Name:Names)
        {
            const auto* R=CombinationRules->FindRow<FSwordCombinationRule>(Name,TEXT("Sword combination"));
            if (!R || R->Priority<=Priority) continue;
            if ((R->BladeIndex<0 || R->BladeIndex==Result.Blade.Index()) && (R->GuardIndex<0 || R->GuardIndex==Result.Guard.Index()) &&
                (R->HiltIndex<0 || R->HiltIndex==Result.Hilt.Index()) && (R->BladeVariant<0 || R->BladeVariant==Result.BladeVariant))
            {
                Priority=R->Priority; Result.CombinationRule=Name;
                const auto Value=[&](FName Key) { const float* V=R->Stats.Find(Key); return V && FMath::IsFinite(*V)?FMath::Clamp(*V,0.1f,50.0f):1.0f; };
                const float* Mass=R->Stats.Find(TEXT("Weapon_Weight"));
                Result.WeaponWeight=Mass && FMath::IsFinite(*Mass) && *Mass>0?FMath::Clamp(*Mass,0.1f,50.0f):1.0f;
                Result.WeaponPower=Value(TEXT("Weapon_Power"));
                Result.WeaponAngle=Value(TEXT("Weapon_Angle"));
            }
        }
    }
    return Result;
}
AProceduralSword* USwordWorldSubsystem::SpawnSword(const FVector& Position,float Lane,const FString& SpawnId,int32 Mini,int32 Seed,AActor* Owner,bool bUnrestricted)
{
    if (RemovedFromSpawn.Contains(SpawnId)) return nullptr;
    const FTransform T(FRotator::ZeroRotator,Position);
    auto* Sword=GetWorld()->SpawnActorDeferred<AProceduralSword>(AProceduralSword::StaticClass(),T,Owner,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!Sword) return nullptr;
    Sword->Definition=Generate(Mini,Seed,bUnrestricted); Sword->LaneY=Lane; Sword->PersistentId=SpawnId;
    Sword->Definition.InstanceId=FGuid::NewDeterministicGuid(SpawnId,static_cast<uint64>(Seed));
    UGameplayStatics::FinishSpawningActor(Sword,T);
    return Sword;
}
UMaterialInterface* USwordWorldSubsystem::WorldMaterial()
{
    if (!SwordMaterial) SwordMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Systems/Procedural_Sword_Generator/M_ProceduralSword.M_ProceduralSword"));
    return SwordMaterial;
}
UMaterialInterface* USwordWorldSubsystem::IconMaterial()
{
    if (!SwordIconMaterial) SwordIconMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Systems/Procedural_Sword_Generator/M_ProceduralSword_UI.M_ProceduralSword_UI"));
    return SwordIconMaterial;
}
TArray<FSwordSaveRecord> USwordWorldSubsystem::CaptureSwordRecords() const
{
    TArray<FSwordSaveRecord> Records;
    for (auto Item:Items) if (const auto* Sword=Cast<AProceduralSword>(Item.Get()))
    {
        FSwordSaveRecord& R=Records.AddDefaulted_GetRef();
        R.Sword=Sword->Definition; R.SpawnId=Sword->PersistentId; R.Transform=Sword->GetActorTransform();
        R.LaneY=Sword->LaneY; R.bHeld=Sword->IsHeld();
    }
    return Records;
}
