#include "POI/POIModuleAuthor.h"
#include "Components/BoxComponent.h"
#include "Components/ArrowComponent.h"
#include "Engine/World.h"

APOIModuleAuthor::APOIModuleAuthor()
{
    PrimaryActorTick.bCanEverTick=false;
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    Bounds=CreateDefaultSubobject<UBoxComponent>(TEXT("ReservedSpace")); Bounds->SetupAttachment(RootComponent);
    Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision); Bounds->SetHiddenInGame(true);
    Entry=CreateDefaultSubobject<UArrowComponent>(TEXT("Entry")); Entry->SetupAttachment(RootComponent); Entry->SetArrowColor(FColor::Green);
    Exit=CreateDefaultSubobject<UArrowComponent>(TEXT("Exit")); Exit->SetupAttachment(RootComponent); Exit->SetArrowColor(FColor::Cyan);
}

void APOIModuleAuthor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    const FVector Half(FMath::Max(1,Definition.WidthCells)*Definition.CellWidth/2,Definition.LaneWidth/2,FMath::Max(1,Definition.HeightCells)*Definition.FloorHeight/2);
    Bounds->SetBoxExtent(Half); Bounds->SetRelativeLocation(FVector(Half.X,0,Half.Z));
    Entry->SetRelativeLocation(FVector(0,0,Definition.EntryFloor*Definition.FloorHeight));
    Exit->SetRelativeLocation(FVector(2*Half.X,0,Definition.ExitFloor*Definition.FloorHeight));
}

void APOIModuleAuthor::ValidateModule()
{
    TArray<FPOIModuleRow> Rows{Definition};
    if (!UPOIPlanner::ValidateCatalog(Rows,Status)) return;
    if (!GetActorTransform().Equals(FTransform::Identity)) { Status=TEXT("Author marker must be at origin, with zero rotation and unit scale."); return; }
    Status=TEXT("Metadata valid. Manually test traversal, headroom and geometry bounds; these are not checked automatically yet.");
}

void APOIModuleAuthor::ExportDefinition()
{
#if WITH_EDITOR
    TArray<FPOIModuleRow> Rows{Definition};
    if (!UPOIPlanner::ValidateCatalog(Rows,Status)) return;
    if (!GetActorTransform().Equals(FTransform::Identity)) { Status=TEXT("Move author marker to map origin before export."); return; }
    if (!TargetCatalog || TargetCatalog->GetRowStruct()!=FPOIModuleRow::StaticStruct()) { Status=TEXT("Select a DataTable using POIModuleRow."); return; }
    if (GetWorld()->WorldType!=EWorldType::Editor) { Status=TEXT("Export is available only in the level editor."); return; }
    if (Definition.Level.IsNull()) { Status=TEXT("Assign the saved source map to Definition.Level first."); return; }
    TargetCatalog->Modify(); TargetCatalog->AddRow(Definition.ModuleId,Definition); TargetCatalog->MarkPackageDirty();
    Status=TEXT("Row updated. Save the DataTable asset. Generator dimensions must match author dimensions.");
#endif
}
