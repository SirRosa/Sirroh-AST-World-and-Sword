#include "POI/POIModule.h"

bool UPOIPlanner::ValidateCatalog(const TArray<FPOIModuleRow>& Modules, FString& Error)
{
    Error.Reset();
    if (Modules.IsEmpty()) { Error=TEXT("Catalog is empty."); return false; }
    TSet<FName> Ids;
    for (const auto& M:Modules)
    {
        if (M.ModuleId.IsNone() || Ids.Contains(M.ModuleId)) { Error=TEXT("Module IDs must be non-empty and unique."); return false; }
        Ids.Add(M.ModuleId);
        if (M.WidthCells<1 || M.WidthCells>32 || M.HeightCells<1 || M.HeightCells>16 ||
            M.EntryFloor<0 || M.ExitFloor<0 || M.EntryFloor>=M.HeightCells || M.ExitFloor>=M.HeightCells ||
            M.MinimumBaseFloor<0 || M.EntryType.IsNone() || M.ExitType.IsNone() || M.Environment.IsNone() ||
            !FMath::IsFinite(M.Weight) || M.Weight<0 || M.Weight>100000 ||
            !FMath::IsFinite(M.CellWidth) || !FMath::IsFinite(M.FloorHeight) || !FMath::IsFinite(M.LaneWidth) ||
            M.CellWidth<100 || M.FloorHeight<100 || M.LaneWidth<100)
        { Error=FString::Printf(TEXT("Invalid dimensions, connections or weight: %s"),*M.ModuleId.ToString()); return false; }
    }
    return true;
}

bool UPOIPlanner::ValidateSettings(const FPOIPlanSettings& S, FString& Error)
{
    if (S.ModuleCount<1 || S.ModuleCount>64 || S.Floors<1 || S.Floors>16 || S.StartFloor<0 || S.StartFloor>=S.Floors ||
        S.EndFloor<0 || S.EndFloor>=S.Floors || S.SearchBudget<1 || S.SearchBudget>100000 || S.BoundaryType.IsNone() ||
        !FMath::IsFinite(S.CellWidth) || !FMath::IsFinite(S.FloorHeight) || !FMath::IsFinite(S.LaneWidth) ||
        S.CellWidth<100 || S.FloorHeight<100 || S.LaneWidth<100)
    { Error=TEXT("Invalid plan settings (counts, floors, dimensions or search budget)."); return false; }
    return true;
}

bool UPOIPlanner::BuildPlan(const TArray<FPOIModuleRow>& Modules, const FPOIPlanSettings& S, TArray<FPOIPlacement>& Plan, FString& Error)
{
    Plan.Reset();
    if (!ValidateCatalog(Modules,Error) || !ValidateSettings(S,Error)) return false;
    TArray<FPOIModuleRow> Sorted=Modules;
    for (const auto& M:Sorted) if (M.Weight>0 && M.Environment==S.Environment &&
        (!FMath::IsNearlyEqual(M.CellWidth,S.CellWidth) || !FMath::IsNearlyEqual(M.FloorHeight,S.FloorHeight) || !FMath::IsNearlyEqual(M.LaneWidth,S.LaneWidth)))
    { Error=FString::Printf(TEXT("Module dimensions differ from generator: %s"),*M.ModuleId.ToString()); return false; }
    Sorted.Sort([](const auto& A,const auto& B){return A.ModuleId.LexicalLess(B.ModuleId);});
    FRandomStream Random(S.Seed);
    int32 Remaining=S.SearchBudget;
    TFunction<bool(int32,int32,FName,bool)> Search;
    Search=[&](int32 X,int32 Floor,FName Type,bool Top)->bool
    {
        if (Plan.Num()==S.ModuleCount) return Floor==S.EndFloor && Type==S.BoundaryType && (!S.bRequireTopFloor || Top);
        if (--Remaining<0) return false;
        TArray<int32> Candidates;
        for (int32 I=0;I<Sorted.Num();++I)
        {
            const auto& M=Sorted[I]; const int32 Base=Floor-M.EntryFloor;
            if (M.Weight>0 && M.Environment==S.Environment && M.EntryType==Type && Base>=M.MinimumBaseFloor && Base+M.HeightCells<=S.Floors)
                Candidates.Add(I);
        }
        while (!Candidates.IsEmpty() && Remaining>=0)
        {
            double Total=0; for (int32 I:Candidates) Total+=Sorted[I].Weight;
            double Draw=Random.FRand()*Total; int32 Pick=Candidates.Num()-1;
            for (int32 I=0;I<Candidates.Num();++I) { Draw-=Sorted[Candidates[I]].Weight; if (Draw<=0) { Pick=I; break; } }
            const auto& M=Sorted[Candidates[Pick]]; Candidates.RemoveAt(Pick);
            FPOIPlacement P; P.Module=M; P.Cell=FIntPoint(X,Floor-M.EntryFloor);
            P.InstanceId=FName(*FString::Printf(TEXT("Module_%03d"),Plan.Num())); Plan.Add(P);
            const int32 NextFloor=P.Cell.Y+M.ExitFloor;
            if (Search(X+M.WidthCells,NextFloor,M.ExitType,Top || NextFloor==S.Floors-1)) return true;
            Plan.Pop();
        }
        return false;
    };
    if (Search(0,S.StartFloor,S.BoundaryType,S.StartFloor==S.Floors-1)) return true;
    Plan.Reset(); Error=Remaining<0?TEXT("Search budget exhausted. Reduce constraints or add matching modules."):TEXT("No connected layout satisfies these settings."); return false;
}

TArray<FPOIModuleRow> UPOIPlanner::ExampleCatalog()
{
    TArray<FPOIModuleRow> Result;
    FPOIModuleRow M; M.ModuleId=TEXT("Corridor_Example"); M.Weight=3; Result.Add(M);
    M.ModuleId=TEXT("Up_Example"); M.Category=TEXT("Stairs"); M.WidthCells=3; M.HeightCells=2; M.ExitFloor=1; M.Weight=2; Result.Add(M);
    M.ModuleId=TEXT("Down_Example"); M.EntryFloor=1; M.ExitFloor=0; Result.Add(M);
    return Result;
}
