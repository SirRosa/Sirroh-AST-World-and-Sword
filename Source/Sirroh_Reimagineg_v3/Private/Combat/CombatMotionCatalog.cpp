#include "Combat/CombatMotionCatalog.h"
#include "Animation/AnimSequence.h"
#include "UObject/UnrealType.h"
#include "HAL/IConsoleManager.h"

namespace
{
    TAutoConsoleVariable<int32> CVarCatalog(TEXT("Combat.MotionCatalogV3"),0,
        TEXT("Opt into DT_Combat_Motion_v3 at combat initialization; legacy remains available during migration."));
    void Categories(const FCombatMotionProfile& P, TArray<FCombatNamedMotionCategory>& Out)
    {
        auto Add=[&](FName Name,const FCombatMotionCategory& Data)
        { auto& Entry=Out.AddDefaulted_GetRef(); Entry.Name=Name; Entry.Data=Data; };
#define CAT(Path) Add(TEXT(#Path),P.Path)
        CAT(LookAim); CAT(GuardAim);
        CAT(Offense.Heavy); CAT(Offense.Light); CAT(Offense.Pierce); CAT(Offense.Kick); CAT(Offense.InAir); CAT(Offense.OnGround);
        CAT(Received.HitReaction); CAT(Received.Block); CAT(Received.PerfectBlock); CAT(Received.Parry);
        CAT(Received.InAir); CAT(Received.OnGround); CAT(Received.Knockdown); CAT(Received.GetUp); CAT(Received.Death);
        CAT(Movement.Roll); CAT(Movement.Dodge);
        CAT(Movement.Combat.Base); CAT(Movement.Combat.Walk); CAT(Movement.Combat.Transition); CAT(Movement.Combat.Chill);
        CAT(Movement.OutOfCombat.Base); CAT(Movement.OutOfCombat.Walk); CAT(Movement.OutOfCombat.Transition); CAT(Movement.OutOfCombat.Chill);
        CAT(Other.Taunt); CAT(Other.Guarding);
#undef CAT
        for (const auto& E:P.Offense.Specials) Add(FName(TEXT("Offense.Specials.")+E.Name.ToString()),E.Data);
        for (const auto& E:P.Movement.Additional) Add(FName(TEXT("Movement.")+E.Name.ToString()),E.Data);
        for (const auto& E:P.Other.Additional) Add(FName(TEXT("Other.")+E.Name.ToString()),E.Data);
    }
    FString SlotKey(const FCombatMotionSlot& S)
    { return S.Direction.ToString()+TEXT("/")+S.Context.ToString()+TEXT("/")+S.Region.ToString(); }
    bool SameSlot(const FCombatMotionSlot& A,const FCombatMotionSlot& B)
    { return A.Direction==B.Direction && A.Context==B.Context && A.Region==B.Region; }
    const FCombatMotionCategory* Find(const FResolvedCombatMotionProfile& P,FName Name)
    {
        const auto* Entry=P.Categories.FindByPredicate([&](const auto& E){return E.Name==Name;});
        return Entry?&Entry->Data:nullptr;
    }
    int32 DirectionIndex(FName Name)
    {
        static const FName Names[]={TEXT("Top"),TEXT("TopRight"),TEXT("Right"),TEXT("LowRight"),
            TEXT("Bottom"),TEXT("LowLeft"),TEXT("Left"),TEXT("TopLeft")};
        for (int32 I=0;I<8;++I) if (Names[I]==Name) return I;
        return INDEX_NONE;
    }
    void Merge(FCombatMotionCategory& Target,const FCombatMotionCategory& Input)
    {
        if (Input.Mode==ECombatCatalogOverride::Inherit) return;
        if (Input.Mode==ECombatCatalogOverride::Disabled) { Target=Input; Target.Slots.Reset(); return; }
        if (Target.Mode==ECombatCatalogOverride::Disabled) Target=FCombatMotionCategory();
        Target.Mode=ECombatCatalogOverride::Override;
        if (Input.bOverrideReuse) { Target.UseOtherAnimations=Input.UseOtherAnimations; Target.bOverrideReuse=true; }
        if (Input.bOverrideLayering) { Target.bUseLayeredAnimation=Input.bUseLayeredAnimation; Target.bOverrideLayering=true; }
        for (const auto& Slot:Input.Slots)
        {
            if (Slot.Mode==ECombatCatalogOverride::Inherit) continue;
            auto* Existing=Target.Slots.FindByPredicate([&](const auto& E){return SameSlot(E,Slot);});
            if (Existing) *Existing=Slot; else Target.Slots.Add(Slot);
        }
    }
}

bool UCombatMotionCatalog::ResolveProfile(UDataTable* Table,FName Profile,FResolvedCombatMotionProfile& Result,FString& Error)
{
    Result=FResolvedCombatMotionProfile(); Error.Reset();
    if (!Table || Table->GetRowStruct()!=FCombatMotionProfile::StaticStruct())
    { Error=TEXT("Expected CombatMotionProfile DataTable"); return false; }
    TArray<const FCombatMotionProfile*> Chain; TSet<FName> Visited;
    for (FName Name=Profile;!Name.IsNone();)
    {
        if (Visited.Contains(Name)) { Error=TEXT("Profile inheritance cycle: ")+Name.ToString(); return false; }
        Visited.Add(Name);
        const auto* Row=Table->FindRow<FCombatMotionProfile>(Name,TEXT("Resolve motion profile"),false);
        if (!Row) { Error=TEXT("Missing motion profile: ")+Name.ToString(); return false; }
        Chain.Add(Row); Name=Row->ParentProfile;
    }
    if (Chain.IsEmpty()) { Error=TEXT("Empty profile ID"); return false; }
    for (int32 I=Chain.Num()-1;I>=0;--I)
    {
        const auto& Row=*Chain[I];
        for (FName Name:Row.OverrideStyleFields)
        {
            const FProperty* Property=FCombatStyleDefinition::StaticStruct()->FindPropertyByName(Name);
            if (!Property) { Error=TEXT("Unknown style override field: ")+Name.ToString(); return false; }
            Property->CopyCompleteValue_InContainer(&Result.Style,&Row.Style);
        }
        if (Row.Effects.Mode!=ECombatCatalogOverride::Inherit) Result.Effects=Row.Effects;
        TArray<FCombatNamedMotionCategory> Entries; Categories(Row,Entries);
        TSet<FName> Names;
        for (const auto& Entry:Entries)
        {
            if (Names.Contains(Entry.Name)) { Error=TEXT("Duplicate category: ")+Entry.Name.ToString(); return false; }
            Names.Add(Entry.Name);
            auto* Existing=Result.Categories.FindByPredicate([&](const auto& E){return E.Name==Entry.Name;});
            if (!Existing) { Existing=&Result.Categories.AddDefaulted_GetRef(); Existing->Name=Entry.Name; }
            Merge(Existing->Data,Entry.Data);
        }
    }
    Result.ProfileId=Profile;
    Result.Categories.Sort([](const auto& A,const auto& B){return A.Name.LexicalLess(B.Name);});
    return true;
}

bool UCombatMotionCatalog::ValidateCatalog(UDataTable* Table,TArray<FString>& Errors)
{
    Errors.Reset();
    if (!Table || Table->GetRowStruct()!=FCombatMotionProfile::StaticStruct())
    { Errors.Add(TEXT("Expected CombatMotionProfile DataTable")); return false; }
    TArray<FName> Names=Table->GetRowNames(); Names.Sort(FNameLexicalLess());
    for (FName Name:Names)
    {
        const auto* Row=Table->FindRow<FCombatMotionProfile>(Name,TEXT("Validate motion catalog"));
        auto Fail=[&](const FString& Message){Errors.Add(Name.ToString()+TEXT(": ")+Message);};
        if (!Row->OtherGripProfile.IsNone() && !Names.Contains(Row->OtherGripProfile)) Fail(TEXT("Missing other grip profile"));
        TArray<FCombatNamedMotionCategory> Raw; Categories(*Row,Raw);
        for (const auto& Category:Raw)
        {
            TSet<FString> Slots;
            for (const auto& Slot:Category.Data.Slots)
            {
                const FString Key=SlotKey(Slot);
                if (Slots.Contains(Key)) Fail(TEXT("Duplicate slot ")+Category.Name.ToString()+TEXT("/")+Key);
                Slots.Add(Key);
                if (Slot.Mode==ECombatCatalogOverride::Override && Slot.Sets.IsEmpty()) Fail(TEXT("Override slot has no variant sets: ")+Key);
                TSet<FName> Sets;
                for (const auto& Set:Slot.Sets)
                {
                    if (Sets.Contains(Set.Name)) Fail(TEXT("Duplicate variant set: ")+Key);
                    Sets.Add(Set.Name);
                    if (Set.Variants.Num()<1 || Set.Variants.Num()>10) Fail(TEXT("Variant set must contain 1-10 entries: ")+Key);
                    for (const auto& V:Set.Variants)
                    {
                        if (V.Id.IsNone()) Fail(TEXT("Variant has no stable ID: ")+Key);
                        auto* Animation=V.Motion.Animation.LoadSynchronous();
                        if (!Animation) Fail(TEXT("Missing animation: ")+V.Id.ToString());
                        const auto& M=V.Motion;
                        const auto Normalized=[](float X){ return FMath::IsFinite(X) && X>=0 && X<=1; };
                        if (!FMath::IsFinite(M.Speed) || M.Speed<=0 || !Normalized(M.LoadTime) || !Normalized(M.ContactTime)
                            || !Normalized(M.FinishTime) || M.LoadTime>M.ContactTime || M.ContactTime>M.FinishTime
                            || !Normalized(M.PoseWeight) || M.Direction.ContainsNaN() || M.NeutralTarget.ContainsNaN()
                            || M.Hands<0 || M.Hands>2) Fail(TEXT("Invalid motion parameters: ")+V.Id.ToString());
                        for (int32 Flag:{M.Airborne,M.Sprint,M.Crouch,M.Guard})
                            if (Flag < -1 || Flag > 1) Fail(TEXT("Invalid context flag: ")+V.Id.ToString());
                        double PreviousEnd=-1;
                        for (const auto& Window:M.StrikeWindows)
                        {
                            if (Window.ContainsNaN() || Window.X<0 || Window.X>=Window.Y || Window.Y>1 || Window.X<PreviousEnd)
                                Fail(TEXT("Invalid or overlapping strike window: ")+V.Id.ToString());
                            PreviousEnd=Window.Y;
                        }
                        const bool Aim=Category.Name==TEXT("LookAim") || Category.Name==TEXT("GuardAim");
                        if (Animation && Category.Data.Mode!=ECombatCatalogOverride::Disabled && Animation->IsValidAdditive()!=Aim)
                            Fail(TEXT("Wrong additive mode: ")+V.Id.ToString());
                        if (!FMath::IsFinite(V.ContactPoseTime) || V.ContactPoseTime<0 || V.ContactPoseTime>1)
                            Fail(TEXT("Invalid contact pose time: ")+V.Id.ToString());
                    }
                }
            }
        }
        FResolvedCombatMotionProfile Resolved; FString Error;
        if (!ResolveProfile(Table,Name,Resolved,Error)) { Fail(Error); continue; }
        TSet<FName> VariantIds;
        for (const auto& Category:Resolved.Categories)
        {
            for (const auto& Slot:Category.Data.Slots) for (const auto& Set:Slot.Sets) for (const auto& V:Set.Variants)
            {
                if (VariantIds.Contains(V.Id)) Fail(TEXT("Duplicate effective variant ID: ")+V.Id.ToString());
                VariantIds.Add(V.Id);
            }
            TSet<FString> Seen; FName Context=TEXT("Moving");
            for (FName Next=Category.Name;!Next.IsNone();)
            {
                const FString Key=Next.ToString()+TEXT("/")+Context.ToString();
                if (Seen.Contains(Key)) { Fail(TEXT("Animation reuse cycle: ")+Key); break; }
                Seen.Add(Key);
                const auto* C=Find(Resolved,Next);
                if (!C) { Fail(TEXT("Missing reuse category: ")+Next.ToString()); break; }
                if (C->Mode==ECombatCatalogOverride::Disabled) break;
                const FName NextContext=C->UseOtherAnimations.Context.IsNone()?Context:C->UseOtherAnimations.Context;
                if (C->UseOtherAnimations.Category==Next && NextContext==Context)
                {
                    if (C->UseOtherAnimations.Context.IsNone()) Fail(TEXT("Self reuse requires a destination context: ")+Next.ToString());
                    break;
                }
                Next=C->UseOtherAnimations.Category; Context=NextContext;
            }
        }
    }
    return Errors.IsEmpty();
}

TSharedPtr<const FResolvedCombatMotionProfile> UCombatMotionCatalog::GetProfile(const FCombatStyleDefinition& Style)
{
    if (Style.CatalogProfile.IsNone() || Style.MotionLibrary.IsNull()) return nullptr;
    auto* Table=Style.MotionLibrary.LoadSynchronous(); if (!Table) return nullptr;
    struct FCache { TMap<FName,TSharedPtr<const FResolvedCombatMotionProfile>> Profiles; bool bBound=false; };
    static TMap<TWeakObjectPtr<UDataTable>,FCache> Cache;
    for (auto It=Cache.CreateIterator();It;++It) if (!It.Key().IsValid()) It.RemoveCurrent();
    const TWeakObjectPtr<UDataTable> Key(Table);
    auto& Entry=Cache.FindOrAdd(Key);
    if (!Entry.bBound)
    {
        Entry.bBound=true;
        Table->OnDataTableChanged().AddLambda([Key]() { if (auto* E=Cache.Find(Key)) E->Profiles.Reset(); });
    }
    if (const auto* Found=Entry.Profiles.Find(Style.CatalogProfile)) return *Found;
    auto Resolved=MakeShared<FResolvedCombatMotionProfile>(); FString Error;
    if (!ResolveProfile(Table,Style.CatalogProfile,*Resolved,Error))
    { UE_LOG(LogTemp,Error,TEXT("Motion profile: %s"),*Error); return nullptr; }
    Entry.Profiles.Add(Style.CatalogProfile,Resolved); return Resolved;
}

FName UCombatMotionCatalog::DirectionName(FVector2D Direction)
{
    static const FName Names[]={TEXT("Top"),TEXT("TopRight"),TEXT("Right"),TEXT("LowRight"),TEXT("Bottom"),TEXT("LowLeft"),TEXT("Left"),TEXT("TopLeft")};
    if (Direction.IsNearlyZero()) return TEXT("Mid");
    const int32 I=FMath::RoundToInt(FMath::Atan2(Direction.X,Direction.Y)/(PI/4));
    return Names[(I+8)%8];
}

FCombatCatalogChoice UCombatMotionCatalog::Choose(const FResolvedCombatMotionProfile& Profile,FName Category,
    FName Direction,FName Context,FName Region,int32 Seed)
{
    FCombatCatalogQuery Query; Query.Category=Category; Query.Direction=Direction; Query.Context=Context; Query.Region=Region; Query.Seed=Seed;
    return ChooseForState(Profile,Query);
}

FCombatCatalogChoice UCombatMotionCatalog::ChooseForState(const FResolvedCombatMotionProfile& Profile,const FCombatCatalogQuery& Q)
{
    FCombatCatalogChoice Result;
    struct FSearch { FName Category,Context; const FCombatMotionCategory* Data; bool bLayered; };
    TArray<FSearch> Chain; TSet<FString> Visited;
    FName Context=Q.Context;
    const auto* Requested=Find(Profile,Q.Category);
    const bool Layering=Requested && Requested->bUseLayeredAnimation;
    for (FName Name=Q.Category;!Name.IsNone();)
    {
        const FString Key=Name.ToString()+TEXT("/")+Context.ToString();
        if (Visited.Contains(Key)) { Result.Reason=TEXT("Animation reuse cycle"); return Result; }
        Visited.Add(Key);
        const auto* Data=Find(Profile,Name);
        if (!Data || Data->Mode==ECombatCatalogOverride::Disabled) break;
        const bool Layered=Q.Context==TEXT("Moving") && Context==TEXT("InPlace");
        if (!Layered || Layering) Chain.Add({Name,Context,Data,Layered});
        const FName NextContext=Data->UseOtherAnimations.Context.IsNone()?Context:Data->UseOtherAnimations.Context;
        const FName Next=Data->UseOtherAnimations.Category;
        if (Next==Name && NextContext==Context) break; // same-category context fallback has reached its destination
        Name=Next; Context=NextContext;
    }
    // Layered reuse is explicit and never supersedes an authored moving slot in the chain.
    if (Q.Context==TEXT("Moving") && Layering)
    {
        const int32 Count=Chain.Num();
        for (int32 I=0;I<Count;++I)
            if (Chain[I].Context==TEXT("Moving")) Chain.Add({Chain[I].Category,TEXT("InPlace"),Chain[I].Data,true});
    }
    const auto* Base=Profile.Style.BasePose.LoadSynchronous();
    const auto* Skeleton=Base?Base->GetSkeleton():nullptr;
    FRandomStream Random(Q.Seed);
    auto Select=[&](const FSearch& Search,const FCombatMotionSlot& Slot,const TCHAR* Reason)->bool
    {
        if (Slot.Mode!=ECombatCatalogOverride::Override || Slot.Context!=Search.Context
            || (Slot.Region!=Q.Region && Slot.Region!=TEXT("Any"))) return false;
        TArray<const FCombatCatalogVariant*> Candidates;
        for (const auto& Set:Slot.Sets) for (const auto& V:Set.Variants)
        {
            const auto& M=V.Motion;
            const auto Match=[](int32 Required,int32 State){return Required<0 || State<0 || Required==State;};
            if (!M.bEnabled || (M.Hands && M.Hands!=(Profile.Style.bTwoHanded?2:1))
                || (!M.WeaponFamily.IsNone() && M.WeaponFamily!=Profile.Style.MotionFamily)
                || !Match(M.Airborne,Q.Airborne) || !Match(M.Sprint,Q.Sprint) || !Match(M.Crouch,Q.Crouch) || !Match(M.Guard,Q.Guard)
                || (Q.bRestrictVariants && !Q.AllowedVariants.Contains(V.Id))) continue;
            auto* Animation=M.Animation.LoadSynchronous();
            if (!Animation || (Skeleton && Animation->GetSkeleton()!=Skeleton)) continue;
            const bool Aim=Search.Category==TEXT("LookAim") || Search.Category==TEXT("GuardAim");
            if (Animation->IsValidAdditive()!=Aim) continue;
            Candidates.Add(&V);
        }
        if (Candidates.IsEmpty()) return false;
        Candidates.Sort([](const auto& A,const auto& B){return A.Id.LexicalLess(B.Id);});
        Result.bFound=true; Result.Category=Search.Category; Result.Direction=Slot.Direction; Result.Context=Slot.Context;
        Result.Variant=*Candidates[Random.RandRange(0,Candidates.Num()-1)]; Result.Reason=Reason;
        Result.bUseLayeredAnimation=Search.bLayered; return true;
    };
    for (const auto& Search:Chain)
        for (int32 Pass=0;Pass<2;++Pass)
            for (const auto& Slot:Search.Data->Slots)
                if (Slot.Direction==Q.Direction && Slot.Context==Search.Context && (Pass==0?Slot.Region==Q.Region:Slot.Region==TEXT("Any")))
                {
                    if (Slot.Mode==ECombatCatalogOverride::Disabled)
                    { Result.Reason=TEXT("Slot explicitly disabled"); return Result; }
                    if (Select(Search,Slot,Search.Category==Q.Category?TEXT("Exact/inherited slot"):TEXT("Use Other Animations"))) return Result;
                }
    const int32 Index=DirectionIndex(Q.Direction);
    for (const auto& Search:Chain)
    {
        TArray<const FCombatMotionSlot*> Slots;
        TSet<FName> Directions;
        for (const auto& Slot:Search.Data->Slots)
            if (Slot.Mode==ECombatCatalogOverride::Override && Slot.Context==Search.Context && (Slot.Region==Q.Region || Slot.Region==TEXT("Any")))
            { Directions.Add(Slot.Direction); if (Index!=INDEX_NONE && DirectionIndex(Slot.Direction)!=INDEX_NONE) Slots.Add(&Slot); }
        Slots.Sort([&](const auto& A,const auto& B)
        {
            auto Cost=[&](const auto& S){const int32 D=FMath::Abs(DirectionIndex(S.Direction)-Index); return FMath::Min(D,8-D);};
            if (Cost(A)!=Cost(B)) return Cost(A)<Cost(B);
            if ((A.Region==Q.Region)!=(B.Region==Q.Region)) return A.Region==Q.Region;
            return DirectionIndex(A.Direction)<DirectionIndex(B.Direction);
        });
        if (Index==INDEX_NONE && (Q.Direction==TEXT("Front") || Q.Direction==TEXT("Back") || Q.Direction==TEXT("Mid")))
        {
            for (const auto& Slot:Search.Data->Slots)
                if (Slot.Context==Search.Context && (Slot.Region==Q.Region || Slot.Region==TEXT("Any"))) Slots.Add(&Slot);
            const FVector Incoming=Q.Direction==TEXT("Back")?-FVector::ForwardVector:FVector::ForwardVector;
            auto Cost=[&](const FCombatMotionSlot& Slot)
            {
                float Best=2;
                for (const auto& Set:Slot.Sets) for (const auto& V:Set.Variants)
                    if (!Q.bRestrictVariants || Q.AllowedVariants.Contains(V.Id))
                        Best=FMath::Min(Best,float(1-FVector::DotProduct(Incoming,V.Incoming.GetSafeNormal())));
                return Best;
            };
            Slots.Sort([&](const auto& A,const auto& B){ const float CA=Cost(A),CB=Cost(B);
                return CA==CB?A.Direction.LexicalLess(B.Direction):CA<CB; });
        }
        for (const auto* Slot:Slots) if (Select(Search,*Slot,TEXT("Nearest natural direction"))) return Result;
        if (Directions.Num()==1 && Q.Category!=TEXT("LookAim") && Q.Category!=TEXT("GuardAim"))
            for (const auto& Slot:Search.Data->Slots) if (Select(Search,Slot,TEXT("Only available direction"))) return Result;
    }
    Result.Reason=TEXT("No compatible animation"); return Result;
}

TArray<FCombatMotionSource> UCombatMotionCatalog::RuntimeSources(const FResolvedCombatMotionProfile& Profile)
{
    TArray<FCombatMotionSource> Result;
    for (const auto& Category:Profile.Categories)
    {
        if (Category.Data.Mode!=ECombatCatalogOverride::Override || Category.Name==TEXT("Other.Archive")) continue;
        for (const auto& Slot:Category.Data.Slots)
            if (Slot.Mode==ECombatCatalogOverride::Override)
                for (const auto& Set:Slot.Sets) for (const auto& V:Set.Variants)
                    if (V.Motion.bEnabled && (!V.Motion.Hands || V.Motion.Hands==(Profile.Style.bTwoHanded?2:1))
                        && (V.Motion.WeaponFamily.IsNone() || V.Motion.WeaponFamily==Profile.Style.MotionFamily))
                    {
                        static const TSet<FName> Supported={TEXT("Attack"),TEXT("Guard"),TEXT("Walk"),TEXT("GuardAim"),TEXT("LookAim"),
                            TEXT("Dodge"),TEXT("Roll"),TEXT("Taunt"),TEXT("Kick"),TEXT("Equip"),TEXT("Holster"),TEXT("Carry"),TEXT("CarryWalk"),
                            TEXT("Run"),TEXT("Jump"),TEXT("Fall"),TEXT("Land"),TEXT("Sprint"),TEXT("Crouch"),TEXT("CrouchStart"),TEXT("CrouchEnd"),TEXT("Turn")};
                        if (!Supported.Contains(V.Motion.Action) || (!V.Motion.WeaponFamily.IsNone() && V.Motion.WeaponFamily!=Profile.Style.MotionFamily)) continue;
                        auto& Source=Result.Add_GetRef(V.Motion); Source.CatalogVariantId=V.Id; Source.CatalogCategory=Category.Name;
                    }
    }
    Result.Sort([](const auto& A,const auto& B){return A.CatalogVariantId.LexicalLess(B.CatalogVariantId);});
    return Result;
}

bool UCombatMotionCatalog::IsEnabled() { return CVarCatalog.GetValueOnGameThread()!=0; }

bool UCombatMotionCatalog::StyleForProfile(UDataTable* Table,FName Profile,FCombatStyleDefinition& Style,FString& Error)
{
    FResolvedCombatMotionProfile Resolved;
    if (!ResolveProfile(Table,Profile,Resolved,Error)) return false;
    Style=MoveTemp(Resolved.Style); Style.CatalogProfile=Profile; Style.MotionLibrary=Table;
    return true;
}

bool UCombatMotionCatalog::StyleForGrip(const FCombatStyleDefinition& Current,bool bTwoHanded,FCombatStyleDefinition& Style,FString& Error)
{
    Style=Current; Error.Reset();
    if (Current.CatalogProfile.IsNone()) { Style.bTwoHanded=bTwoHanded; return true; }
    auto* Table=Current.MotionLibrary.LoadSynchronous();
    if (!Table || Table->GetRowStruct()!=FCombatMotionProfile::StaticStruct()) { Error=TEXT("Missing profile table"); return false; }
    FCombatStyleDefinition Resolved;
    if (!StyleForProfile(Table,Current.CatalogProfile,Resolved,Error)) return false;
    if (Resolved.bTwoHanded==bTwoHanded) { Style=MoveTemp(Resolved); return true; }
    const auto* Row=Table->FindRow<FCombatMotionProfile>(Current.CatalogProfile,TEXT("Switch catalog grip"));
    if (!Row || Row->OtherGripProfile.IsNone()) { Error=TEXT("Profile has no compatible other grip"); return false; }
    if (!StyleForProfile(Table,Row->OtherGripProfile,Style,Error)) return false;
    if (Style.bTwoHanded!=bTwoHanded) { Error=TEXT("Other grip profile has wrong handedness"); return false; }
    return true;
}
