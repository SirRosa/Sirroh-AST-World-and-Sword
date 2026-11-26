#include "POI/POIGenerator.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/Level.h"
#include "EngineUtils.h"
#include "POI/POIInteractiveProp.h"
#include "GameFramework/PlayerController.h"
#include "Components/InputComponent.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "UObject/ConstructorHelpers.h"

APOIGenerator::APOIGenerator()
{
    PrimaryActorTick.bCanEverTick=true; PrimaryActorTick.TickInterval=0.25f;
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    PreviewMesh=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ExampleFloors"));
    PreviewMesh->SetupAttachment(RootComponent);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    PreviewMesh->SetStaticMesh(Cube.Object);
    PreviewMesh->SetCollisionProfileName(TEXT("BlockAll"));
    PreviewMesh->SetMobility(EComponentMobility::Movable);
}

void APOIGenerator::GeneratePreview()
{
    TArray<FPOIModuleRow> Rows;
    if (bUseExampleCatalog)
    {
        Rows=UPOIPlanner::ExampleCatalog();
        for (auto& Row:Rows) { Row.CellWidth=Settings.CellWidth; Row.FloorHeight=Settings.FloorHeight; Row.LaneWidth=Settings.LaneWidth; }
    }
    else if (Catalog && Catalog->GetRowStruct()==FPOIModuleRow::StaticStruct())
    {
        TArray<FPOIModuleRow*> TableRows; Catalog->GetAllRows(TEXT("POIGenerator"),TableRows);
        for (const auto* Row:TableRows) Rows.Add(*Row);
    }
    TArray<FPOIPlacement> NewPlan;
    if (!UPOIPlanner::BuildPlan(Rows,Settings,NewPlan,Status)) return;
#if WITH_EDITOR
    if (!GetWorld()->IsGameWorld()) Modify();
#endif
    ReleaseMaps(); ObjectStates.Reset(); Plan=MoveTemp(NewPlan); BuiltSettings=Settings;
    RebuildGeometry();
    Status=FString::Printf(TEXT("Built %d modules, seed %d. Preview geometry is illustrative; custom maps load in Play."),Plan.Num(),BuiltSettings.Seed);
    if (GetWorld()->IsGameWorld()) UpdateStreaming();
#if WITH_EDITOR
    else MarkPackageDirty();
#endif
}

void APOIGenerator::ReleaseMaps()
{
    for (auto& Pair:LoadedMaps) if (Pair.Value)
    {
        CaptureMapState(Pair.Key);
        Pair.Value->OnLevelShown.RemoveDynamic(this,&APOIGenerator::RestoreVisibleMapStates);
        Pair.Value->SetIsRequestingUnloadAndRemoval(true);
    }
    LoadedMaps.Reset(); FailedMaps.Reset(); RestoredMaps.Reset(); PersistentMaps.Reset();
}

void APOIGenerator::ClearPreview()
{
    ReleaseMaps(); ObjectStates.Reset(); Plan.Reset(); PreviewMesh->ClearInstances(); Status=TEXT("Cleared.");
}

void APOIGenerator::RebuildGeometry()
{
    PreviewMesh->ClearInstances();
    const bool Game=GetWorld()->IsGameWorld();
    for (const auto& P:Plan)
    {
        if (Game && !P.Module.Level.IsNull()) continue;
        const double Length=P.Module.WidthCells*BuiltSettings.CellWidth;
        const double Rise=(P.Module.ExitFloor-P.Module.EntryFloor)*BuiltSettings.FloorHeight;
        const int32 Steps=FMath::Clamp(FMath::CeilToInt(FMath::Min(FMath::Abs(Rise)/15.0,1024.0)),1,1024);
        for (int32 I=0;I<Steps;++I)
        {
            const double T=Steps==1?0:double(I)/(Steps-1);
            const FVector Pos(P.Cell.X*BuiltSettings.CellWidth+(I+0.5)*Length/Steps,0,
                (P.Cell.Y+P.Module.EntryFloor)*BuiltSettings.FloorHeight+T*Rise-10);
            PreviewMesh->AddInstance(FTransform(FQuat::Identity,Pos,FVector(Length/Steps/100,BuiltSettings.LaneWidth/100,0.2)));
        }
    }
}

void APOIGenerator::BeginPlay()
{
    Super::BeginPlay();
    if (bGenerateOnBeginPlay) GeneratePreview();
    else { RebuildGeometry(); }
}

void APOIGenerator::EndPlay(const EEndPlayReason::Type Reason)
{
    ReleaseMaps(); Super::EndPlay(Reason);
}

void APOIGenerator::Tick(float DeltaSeconds)
{
    if (bEnableDemoInput && !InputComponent)
        if (auto* PC=UGameplayStatics::GetPlayerController(this,0))
        {
            EnableInput(PC);
            InputComponent->BindKey(EKeys::E,IE_Pressed,this,&APOIGenerator::InteractNearby);
            InputComponent->BindKey(EKeys::F5,IE_Pressed,this,&APOIGenerator::SaveLayoutToSlot);
            InputComponent->BindKey(EKeys::F9,IE_Pressed,this,&APOIGenerator::LoadLayoutFromSlot);
        }
    Super::Tick(DeltaSeconds); UpdateStreaming();
}

void APOIGenerator::InteractNearby()
{
    APawn* Pawn=UGameplayStatics::GetPlayerPawn(this,0); if (!Pawn) return;
    APOIInteractiveProp* Best=nullptr; double Distance=MAX_dbl;
    for (TActorIterator<APOIInteractiveProp> It(GetWorld());It;++It) if (It->CanInteract(Pawn))
    {
        const double D=FVector::DistSquared(Pawn->GetActorLocation(),It->GetActorLocation());
        if (D<Distance) { Distance=D; Best=*It; }
    }
    if (Best) { Best->Interact(Pawn); Status=FString::Printf(TEXT("Interacted with %s"),*Best->GetName()); }
}

void APOIGenerator::UpdateStreaming()
{
    APawn* Pawn=UGameplayStatics::GetPlayerPawn(this,0);
    if (!Pawn) return;
    if (!GetActorScale3D().Equals(FVector::OneVector)) { Status=TEXT("Generator scale must be 1,1,1 for map streaming."); return; }
    const FVector Local=GetActorTransform().InverseTransformPosition(Pawn->GetActorLocation());
    int32 Requests=0;
    for (int32 I=0;I<Plan.Num();++I)
    {
        const auto& P=Plan[I]; if (P.Module.Level.IsNull()) continue;
        const FVector Origin(P.Cell.X*BuiltSettings.CellWidth,0,P.Cell.Y*BuiltSettings.FloorHeight);
        const FBox Bounds(Origin+FVector(0,-BuiltSettings.LaneWidth/2,0),Origin+FVector(P.Module.WidthCells*BuiltSettings.CellWidth,BuiltSettings.LaneWidth/2,P.Module.HeightCells*BuiltSettings.FloorHeight));
        const double Distance=FMath::Sqrt(Bounds.ComputeSquaredDistanceToPoint(Local));
        if (LoadedMaps.Contains(I))
        {
            if (Distance>FMath::Max(1000.f,LoadDistance)+FMath::Max(0.f,UnloadMargin))
            {
                CaptureMapState(I);
                LoadedMaps[I]->OnLevelShown.RemoveDynamic(this,&APOIGenerator::RestoreVisibleMapStates);
                LoadedMaps[I]->SetIsRequestingUnloadAndRemoval(true); LoadedMaps.Remove(I); RestoredMaps.Remove(I); PersistentMaps.Remove(I);
            }
        }
        else if (Distance<=FMath::Max(1000.f,LoadDistance) && Requests<2 && !FailedMaps.Contains(I))
        {
            ++Requests; bool Success=false;
            auto* Stream=ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr(this,P.Module.Level,GetActorTransform().TransformPosition(Origin),GetActorRotation(),Success);
            if (Success && Stream)
            {
                LoadedMaps.Add(I,Stream);
                Stream->OnLevelShown.AddDynamic(this,&APOIGenerator::RestoreVisibleMapStates);
            }
            else { FailedMaps.Add(I); Status=FString::Printf(TEXT("Failed to request map: %s"),*P.Module.Level.ToString()); }
        }
    }
    RestoreVisibleMapStates();
}

void APOIGenerator::RecordRemovedObject(const FString& Key)
{
    ObjectStates.FindOrAdd(Key).bRemoved=true;
}

void APOIGenerator::CaptureMapState(int32 Index)
{
    const auto* Stream=LoadedMaps.Find(Index);
    if (!Stream || !*Stream || !PersistentMaps.Contains(Index) || !Plan.IsValidIndex(Index)) return;
    ULevel* Level=(*Stream)->GetLoadedLevel(); if (!Level) return;
    for (AActor* Actor:Level->Actors) if (IsValid(Actor))
    {
        TArray<UPOIStateComponent*> Components; Actor->GetComponents(Components);
        for (auto* C:Components) if (!C->LocalId.IsNone())
        {
            const FString Key=Plan[Index].InstanceId.ToString()+TEXT("/")+C->LocalId.ToString();
            ObjectStates.Add(Key,C->Capture((*Stream)->LevelTransform));
        }
    }
}

void APOIGenerator::RestoreVisibleMapStates()
{
    for (const auto& Pair:LoadedMaps)
    {
        if (!Pair.Value || !Pair.Value->IsLevelVisible() || RestoredMaps.Contains(Pair.Key)) continue;
        ULevel* Level=Pair.Value->GetLoadedLevel(); if (!Level || !Plan.IsValidIndex(Pair.Key)) continue;
        TArray<UPOIStateComponent*> All; TSet<FName> Ids; bool Valid=true;
        for (AActor* Actor:Level->Actors) if (IsValid(Actor))
        {
            TArray<UPOIStateComponent*> Components; Actor->GetComponents(Components);
            for (auto* C:Components)
            {
                if (C->LocalId.IsNone() || Ids.Contains(C->LocalId)) { Valid=false; break; }
                Ids.Add(C->LocalId); All.Add(C);
            }
        }
        RestoredMaps.Add(Pair.Key);
        if (!Valid) { Status=TEXT("Persistence disabled for a map: missing or duplicate LocalId."); continue; }
        PersistentMaps.Add(Pair.Key);
        for (auto* C:All)
        {
            const FString Key=Plan[Pair.Key].InstanceId.ToString()+TEXT("/")+C->LocalId.ToString();
            C->BindToGenerator(this,Key);
            if (const auto* State=ObjectStates.Find(Key)) { const FPOIObjectState Copy=*State; C->Restore(Copy,Pair.Value->LevelTransform); }
        }
    }
}

bool APOIGenerator::SaveLayout()
{
    if (Plan.IsEmpty() || LayoutSlot.IsEmpty()) { Status=TEXT("No layout or save slot."); return false; }
    auto* Save=Cast<UPOILayoutSave>(UGameplayStatics::CreateSaveGameObject(UPOILayoutSave::StaticClass()));
    for (const auto& Pair:LoadedMaps) CaptureMapState(Pair.Key);
    Save->Settings=BuiltSettings; Save->Placements=Plan;
    Save->ObjectStates=ObjectStates;
    const bool OK=UGameplayStatics::SaveGameToSlot(Save,LayoutSlot,0);
    Status=OK?TEXT("Layout and registered object states saved."):TEXT("Save failed."); return OK;
}

bool APOIGenerator::LoadLayout()
{
    auto* Save=Cast<UPOILayoutSave>(UGameplayStatics::LoadGameFromSlot(LayoutSlot,0));
    if (!Save || Save->Version!=1 || Save->Placements.IsEmpty() || Save->Placements.Num()!=Save->Settings.ModuleCount)
    { Status=TEXT("Missing or unsupported layout save."); return false; }
    // Validate saved snapshots and dimensions without re-rolling the catalog.
    TArray<FPOIModuleRow> Rows; TSet<FName> Seen;
    for (const auto& P:Save->Placements) if (!Seen.Contains(P.Module.ModuleId)) { Seen.Add(P.Module.ModuleId); Rows.Add(P.Module); }
    if (!UPOIPlanner::ValidateCatalog(Rows,Status) || !UPOIPlanner::ValidateSettings(Save->Settings,Status)) return false;
    int32 X=0,Floor=Save->Settings.StartFloor; FName Type=Save->Settings.BoundaryType;
    bool Top=Floor==Save->Settings.Floors-1; TSet<FName> Instances;
    for (const auto& P:Save->Placements)
    {
        TArray<FPOIModuleRow> Single{P.Module};
        if (!UPOIPlanner::ValidateCatalog(Single,Status)) return false;
        if (P.Cell.X!=X || P.Cell.Y+P.Module.EntryFloor!=Floor || P.Cell.Y<P.Module.MinimumBaseFloor ||
            P.Cell.Y+P.Module.HeightCells>Save->Settings.Floors || P.Module.EntryType!=Type || P.Module.Environment!=Save->Settings.Environment ||
            P.InstanceId.IsNone() || Instances.Contains(P.InstanceId) ||
            !FMath::IsNearlyEqual(P.Module.CellWidth,Save->Settings.CellWidth) || !FMath::IsNearlyEqual(P.Module.FloorHeight,Save->Settings.FloorHeight) || !FMath::IsNearlyEqual(P.Module.LaneWidth,Save->Settings.LaneWidth))
        { Status=TEXT("Invalid saved connections, bounds or instance IDs."); return false; }
        Instances.Add(P.InstanceId); X+=P.Module.WidthCells; Floor=P.Cell.Y+P.Module.ExitFloor; Type=P.Module.ExitType; Top|=Floor==Save->Settings.Floors-1;
    }
    if (Floor!=Save->Settings.EndFloor || Type!=Save->Settings.BoundaryType || (Save->Settings.bRequireTopFloor && !Top))
    { Status=TEXT("Saved layout does not reach required exit or top floor."); return false; }
    ReleaseMaps(); ObjectStates=Save->ObjectStates; Settings=Save->Settings; BuiltSettings=Settings; Plan=Save->Placements; RebuildGeometry();
    Status=TEXT("Loaded saved layout snapshot."); return true;
}
