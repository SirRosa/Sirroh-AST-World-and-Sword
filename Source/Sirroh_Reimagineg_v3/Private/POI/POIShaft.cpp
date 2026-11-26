#include "POI/POIShaft.h"
#include "POIImageLighting.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UnrealType.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/Level.h"
#include "POI/POIArtPool.h"
#include "POI/POICorridorTools.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "Items/SirrohWorldItem.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarPOIDormancy(TEXT("poi.Dormancy"),1,TEXT("Pause distant POI actor/component ticks and rendering without discarding state."));

static UPOIArtTheme* ShaftArt(const FPOIShaftRow& Recipe)
{
    return Recipe.ArtTheme.IsNull()?UPOIArtTheme::ForLayer(Recipe.UpperLayer):Recipe.ArtTheme.LoadSynchronous();
}

bool FPOIShaftRow::Validate(FString& Error) const
{
    if (UpperLayer<0 || UpperLayer>3 || !FMath::IsFinite(Weight) || Weight<0 || !FMath::IsFinite(Width) || Width<400 || Width>1200 ||
        !FMath::IsFinite(StepRise) || StepRise<75 || StepRise>200 || !FMath::IsFinite(SectionHeight) || SectionHeight<150 || SectionHeight>3000 ||
        (Climbing!=EPOIKitKind::GASP && Climbing!=EPOIKitKind::AST && Climbing!=EPOIKitKind::Both))
    { Error=TEXT("Invalid layer, weight, width, step rise, section height or climbing type."); return false; }
    if (bUseCustomSection && SectionLevel.IsNull())
    {
        if (Pieces.Num()+NativePieces.Num()==0 || Pieces.Num()+NativePieces.Num()>100) { Error=TEXT("Custom section needs 1..100 kit blocks."); return false; }
        bool Bottom=false,Top=false;
        for (const auto& P:Pieces)
        {
            const FVector Pos=P.Transform.GetLocation();
            if (P.Transform.ContainsNaN() || !P.Transform.GetScale3D().Equals(FVector::OneVector) || !P.Transform.GetRotation().IsIdentity() ||
                P.Block.Size.ContainsNaN() || P.Block.Size.GetMin()<=0 || P.Block.Size.GetMax()>3000 ||
                Pos.Z<0 || Pos.Z>SectionHeight || FMath::Abs(Pos.Y)>50 || FMath::Abs(Pos.X)+P.Block.Size.X*.5>Width*.5+1)
            { Error=TEXT("Section blocks require finite positive Size, unit scale, zero rotation, Y within +/-50 and X/Z within section bounds."); return false; }
            if (P.Block.Kind==EPOIKitKind::GASP || P.Block.Kind==EPOIKitKind::AST || P.Block.Kind==EPOIKitKind::Both)
            { Bottom|=Pos.Z<=StepRise; Top|=Pos.Z>=SectionHeight-2; }
        }
        for (const auto& Native:NativePieces)
        {
            if (Native.ActorClass.IsNull() || Native.Transform.ContainsNaN() || Native.Points.Num()<2)
            { Error=TEXT("AST actor requires a class and at least two spline points."); return false; }
            for (const auto& Point:Native.Points)
            {
                const FVector Pos=Native.Transform.TransformPosition(Point.Position);
                if (Pos.ContainsNaN() || Pos.Z<-.01 || Pos.Z>SectionHeight+.01 || FMath::Abs(Pos.X)>Width*.5+1)
                { Error=FString::Printf(TEXT("AST spline point %s must remain inside width %.0f and height %.0f."),*Pos.ToString(),Width,SectionHeight); return false; }
                Bottom|=Pos.Z<=StepRise;
                Top|=Pos.Z>=SectionHeight-2;
            }
        }
        if (!Bottom || !Top) { Error=TEXT("Section must have a climb block near its bottom and exactly at its top. Test intermediate jumps manually."); return false; }
    }
    Error.Reset(); return true;
}

APOIShaft::APOIShaft()
{
    PrimaryActorTick.bCanEverTick=true; PrimaryActorTick.TickInterval=.25f; RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}
void APOIShaft::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    ProcessPendingBuild();
    if (!HasPendingBuild()) UpdateDormancy();
}
void APOIShaft::ProcessPendingBuild()
{
    if (PendingBlocks.IsEmpty()) return;
    const APawn* Player=UGameplayStatics::GetPlayerPawn(this,0);
    const FVector Delta=Player?Player->GetActorLocation()-GetActorLocation():FVector::ZeroVector;
    // Teleports/rapid approaches must never expose an incomplete climb route.
    const bool Immediate=Player && FMath::Abs(Delta.X)<Recipe.Width*.5f+600 && Delta.Z>-600 && Delta.Z<Height+600;
    const double Deadline=FPlatformTime::Seconds()+.002;
    bProcessingPending=true;
    do
    {
        const FPendingBlock Block=PendingBlocks[0];
        PendingBlocks.RemoveAt(0);
        AddBlock(Block.Settings,Block.Local,Block.Boundary);
    } while(!PendingBlocks.IsEmpty() && (Immediate || FPlatformTime::Seconds()<Deadline));
    bProcessingPending=false;
    if(PendingBlocks.IsEmpty())
    {
        SetActorTickInterval(.25f);
        Status=FString::Printf(TEXT("Built %d blocks."),GeneratedBlocks.Num());
    }
}
void APOIShaft::HideLayerActor(AActor* Actor)
{
    if (!bLayerRenderHidden || !IsValid(Actor) || !Actor->GetRootComponent() || Actor->IsA<APawn>() || Actor->IsA<ASirrohWorldItem>()) return;
    if ((Actor->GetOwner() && Actor->GetOwner()->IsA<APawn>()) || (Actor->GetAttachParentActor() && Actor->GetAttachParentActor()->IsA<APawn>())) return;
    const TWeakObjectPtr<AActor> Key(Actor);
    if (LayerHiddenActors.Contains(Key)) return;
    // A dormant actor may already be hidden; remember its original state, not the dormancy mask.
    const FPausedActor* Paused=PausedSet.Contains(Key)?PausedActors.FindByPredicate([&](const FPausedActor& State){return State.Actor==Key;}):nullptr;
    if (Paused?Paused->Hidden:Actor->IsHidden()) return;
    LayerHiddenActors.Add(Key);
    Actor->SetActorHiddenInGame(true);
}
void APOIShaft::RefreshLayerRenderVisibility()
{
    if (!bLayerRenderHidden) return;
    for (APOIKitBlock* Block:GeneratedBlocks) HideLayerActor(Block);
    for (AActor* Actor:GeneratedNativeActors) if (IsValid(Actor) && Actor->GetOwner()==this) HideLayerActor(Actor);
    for (ULevelStreamingDynamic* Stream:GeneratedLevels) if (Stream && Stream->GetLoadedLevel())
        for (AActor* Actor:Stream->GetLoadedLevel()->Actors) HideLayerActor(Actor);
}
void APOIShaft::SetLayerRenderHidden(bool bHideLayer)
{
    if (bLayerRenderHidden==bHideLayer) return;
    bLayerRenderHidden=bHideLayer;
    if (bHideLayer) RefreshLayerRenderVisibility();
    else
    {
        for (const auto& Key:LayerHiddenActors)
            if (auto* Actor=Key.Get(); Actor && !PausedSet.Contains(Key)) Actor->SetActorHiddenInGame(false);
        LayerHiddenActors.Reset();
    }
}
void APOIShaft::UpdateDormancy()
{
    APawn* Player=UGameplayStatics::GetPlayerPawn(this,0);
    if (!Player || !GetWorld()->IsGameWorld()) return;
    const FVector Delta=Player->GetActorLocation()-GetActorLocation();
    const bool Sleep=CVarPOIDormancy.GetValueOnGameThread()!=0 &&
        (FMath::Abs(Delta.X)>Recipe.Width*.5f+2400 || Delta.Z < -900 || Delta.Z>Height+900);
    if (!Sleep && bDormant)
    {
        for (const auto& State:PausedActors) if (auto* A=State.Actor.Get())
        {
            A->SetActorHiddenInGame(State.Hidden || LayerHiddenActors.Contains(State.Actor)); A->SetActorTickEnabled(State.Tick);
            for (const auto& C:State.TickingComponents) if (C.IsValid()) C->SetComponentTickEnabled(true);
        }
        PausedActors.Reset(); PausedSet.Reset(); bDormant=false;
    }
    if (!Sleep) { DormantActorCount=-1; return; }
    int32 ActorCount=0;
    for(ULevelStreamingDynamic* Stream:GeneratedLevels)if(Stream && Stream->GetLoadedLevel())ActorCount+=Stream->GetLoadedLevel()->Actors.Num();
    // Sleeping levels normally have an unchanged actor list. Do not rescan every
    // component of every POI actor four times per second while travelling.
    if(bDormant && DormantActorCount==ActorCount)return;
    DormantActorCount=ActorCount;
    for (ULevelStreamingDynamic* Stream:GeneratedLevels) if (Stream && Stream->GetLoadedLevel())
        for (AActor* A:Stream->GetLoadedLevel()->Actors)
        {
            if (!IsValid(A) || A->IsA<APawn>() || !A->GetRootComponent()) continue;
            if (A->GetAttachParentActor() && A->GetAttachParentActor()->IsA<APawn>()) continue;
            if (A->GetOwner() && A->GetOwner()->IsA<APawn>()) continue;
            if (PausedSet.Contains(A)) continue;
            FPausedActor State; State.Actor=A; State.Tick=A->IsActorTickEnabled(); State.Hidden=A->IsHidden() && !LayerHiddenActors.Contains(TWeakObjectPtr<AActor>(A));
            for (UActorComponent* C:A->GetComponents()) if (C && C->IsComponentTickEnabled()) { State.TickingComponents.Add(C); C->SetComponentTickEnabled(false); }
            A->SetActorTickEnabled(false); A->SetActorHiddenInGame(true); PausedActors.Add(MoveTemp(State)); PausedSet.Add(A);
        }
    bDormant=true;
}
APOIKitBlock* APOIShaft::AddBlock(const FPOIKitSettings& Settings,const FTransform& Local,bool Boundary)
{
    if(bIncrementalBuild && !bProcessingPending && GetWorld()->IsGameWorld())
    {
        PendingBlocks.Add({Settings,Local,Boundary});
        SetActorTickInterval(0.f);
        return nullptr;
    }
    FActorSpawnParameters Params; Params.Owner=this; Params.OverrideLevel=GetLevel(); Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Params.bDeferConstruction=true;
    auto* Block=GetWorld()->SpawnActor<APOIKitBlock>(APOIKitBlock::StaticClass(),Local*GetActorTransform(),Params);
    if (!Block) return nullptr;
    Block->Settings=Settings;
    if (GetWorld()->IsGameWorld())
        if (const auto* Theme=ShaftArt(Recipe)) Theme->Apply(Block->Settings,ArtRandom);
    UGameplayStatics::FinishSpawningActor(Block,Local*GetActorTransform());
    Block->AttachToActor(this,FAttachmentTransformRules::KeepWorldTransform);
    if (Boundary) Block->Tags.Add(TEXT("POIAutoBoundary"));
    GeneratedBlocks.Add(Block); HideLayerActor(Block); return Block;
}
void APOIShaft::ClearGenerated()
{
    PendingBlocks.Reset();
    SetActorTickInterval(.25f);
    for (ULevelStreamingDynamic* Level:GeneratedLevels) if (Level)
    {
        Level->OnLevelShown.RemoveDynamic(this,&APOIShaft::ApplyLoadedArt);
        Level->SetIsRequestingUnloadAndRemoval(true);
    }
    GeneratedLevels.Reset(); DecoratedLevels.Reset(); DormantActorCount=-1;
    PausedActors.Reset(); PausedSet.Reset(); bDormant=false;
    LayerHiddenActors.Reset();
    for (APOIKitBlock* Block:GeneratedBlocks) if (IsValid(Block) && Block->GetOwner()==this) Block->Destroy();
    GeneratedBlocks.Reset();
    for (AActor* Actor:GeneratedNativeActors) if (IsValid(Actor) && Actor->GetOwner()==this) Actor->Destroy();
    GeneratedNativeActors.Reset();
}
void APOIShaft::AddSection(const FPOIShaftRow& Section,float Offset)
{
    if (!Section.SectionLevel.IsNull())
    {
        bool Success=false;
        const FVector Position=GetActorTransform().TransformPosition(FVector(0,0,Offset));
        auto* Level=ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr(this,Section.SectionLevel,Position,GetActorRotation(),Success);
        if (Success && Level)
        {
            GeneratedLevels.Add(Level);
            Level->OnLevelShown.AddDynamic(this,&APOIShaft::ApplyLoadedArt);
        }
        else UE_LOG(LogTemp,Error,TEXT("Cannot load POI section %s"),*Section.SectionLevel.ToString());
        return;
    }
    for (const auto& P:Section.Pieces)
    { FTransform T=P.Transform; T.AddToTranslation(FVector(0,0,Offset)); AddBlock(P.Block,T); }
    for (const auto& P:Section.NativePieces)
    {
        UClass* Class=P.ActorClass.LoadSynchronous();
        if (!Class) { Status=TEXT("Missing AST actor class."); continue; }
        FTransform Local=P.Transform; Local.AddToTranslation(FVector(0,0,Offset));
        const FTransform Transform=Local*GetActorTransform();
        auto* Actor=GetWorld()->SpawnActorDeferred<AActor>(Class,Transform,this,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (!Actor) continue;
        for (const auto& Value:P.Settings)
            if (auto* Property=FindFProperty<FProperty>(Class,Value.Key))
                Property->ImportText_Direct(*Value.Value,Property->ContainerPtrToValuePtr<void>(Actor),Actor,PPF_None);
        UGameplayStatics::FinishSpawningActor(Actor,Transform);
        if (auto* Spline=Actor->FindComponentByClass<USplineComponent>())
        {
            Spline->ClearSplinePoints(false);
            Spline->AddPoints(P.Points,false);
            Spline->SetClosedLoop(P.bClosedLoop,true);
            Spline->bSplineHasBeenEdited=true;
            Spline->bInputSplinePointsToConstructionScript=true;
            Actor->RerunConstructionScripts();
        }
        Actor->AttachToActor(this,FAttachmentTransformRules::KeepWorldTransform);
        POIImageLighting::ApplyToActor(Actor); // native AST platforms can carry independent PNG mesh slots
        GeneratedNativeActors.Add(Actor); HideLayerActor(Actor);
    }
}

void APOIShaft::BuildShaft()
{
    if (!Recipe.Validate(Status)) return;
    if (!FMath::IsFinite(Height) || Height<150 || Height>30000 || !GetActorScale3D().Equals(FVector::OneVector) || !GetActorRotation().IsNearlyZero())
    { Status=TEXT("Height must be 150..30000 cm; shaft requires zero rotation and unit scale."); return; }
    const int32 Sections=Recipe.bUseCustomSection?FMath::FloorToInt(Height/Recipe.SectionHeight):0;
    if (Sections*(Recipe.Pieces.Num()+Recipe.NativePieces.Num())>500) { Status=TEXT("Too many blocks (>500). Simplify section or reduce shaft height."); return; }
    TArray<FPOIShaftRow> SectionPlan;
    TArray<float> Offsets;
    float FillStart=Sections*Recipe.SectionHeight, FillEnd=Height;
    if (!PlannedSections.IsEmpty())
    {
        FillStart=0; FillEnd=Height;
        for (const auto& Section:PlannedSections)
        {
            if (!Section.Validate(Status) || FillStart+Section.SectionHeight>Height+.1f) return;
            SectionPlan.Add(Section); Offsets.Add(FillStart); FillStart+=Section.SectionHeight;
        }
        if (!FMath::IsNearlyEqual(FillStart,Height,.1f))
        { Status=TEXT("Saved module plan must match shaft height."); return; }
        FillEnd=FillStart; // Floating-point subtraction must not create a tiny filler platform.
    }
    else if (bAssembleModules)
    {
        TArray<FPOIShaftRow> Pool=SavedModules;
        if (ModuleTable && ModuleTable->GetRowStruct()==FPOIShaftRow::StaticStruct())
        {
            Pool.Reset();
            TArray<FName> Names=ModuleTable->GetRowNames();
            Names.Sort([](FName A,FName B){return A.LexicalLess(B);});
            for (FName Name:Names) Pool.Add(*ModuleTable->FindRow<FPOIShaftRow>(Name,TEXT("Shaft modules")));
        }
        Pool.RemoveAll([this](const FPOIShaftRow& Row)
        {
            FString Error;
            return Row.UpperLayer!=Recipe.UpperLayer || Row.Weight<=0 || !Row.bUseCustomSection ||
                !FMath::IsNearlyEqual(Row.Width,Recipe.Width) || !Row.Validate(Error);
        });
        FRandomStream Random(Seed);
        auto Pick=[&Pool,&Random](EPOIShaftRole DesiredRole,float Available)->int32
        {
            float Total=0;
            for (const auto& Row:Pool) if (Row.Role==DesiredRole && Row.SectionHeight<=Available) Total+=Row.Weight;
            if (Total<=0) return INDEX_NONE;
            float Draw=Random.FRand()*Total;
            for (int32 I=0;I<Pool.Num();++I) if (Pool[I].Role==DesiredRole && Pool[I].SectionHeight<=Available)
            { Draw-=Pool[I].Weight; if (Draw<=0) return I; }
            return INDEX_NONE;
        };
        float MinEntrance=MAX_flt;
        for (const auto& Row:Pool) if (Row.Role==EPOIShaftRole::Entrance) MinEntrance=FMath::Min(MinEntrance,Row.SectionHeight);
        const int32 Bottom=Pick(EPOIShaftRole::Exit,Height-MinEntrance);
        const int32 Top=Bottom==INDEX_NONE?INDEX_NONE:Pick(EPOIShaftRole::Entrance,Height-Pool[Bottom].SectionHeight);
        if (Bottom==INDEX_NONE || Top==INDEX_NONE)
        { Status=TEXT("Need compatible Entrance and Exit modules that fit shaft height and width."); return; }
        SectionPlan.Add(Pool[Bottom]); Offsets.Add(0);
        FillStart=Pool[Bottom].SectionHeight;
        FillEnd=Height-Pool[Top].SectionHeight;
        while (FillStart<FillEnd)
        {
            const int32 Middle=Pick(EPOIShaftRole::Middle,FillEnd-FillStart);
            if (Middle==INDEX_NONE) break;
            SectionPlan.Add(Pool[Middle]); Offsets.Add(FillStart);
            FillStart+=Pool[Middle].SectionHeight;
        }
        if (FillEnd-Pool[Bottom].SectionHeight>=150 && SectionPlan.Num()==1)
        { Status=TEXT("Need a compatible Middle module for the space between Entrance and Exit."); return; }
        SectionPlan.Add(Pool[Top]); Offsets.Add(FillEnd);
    }
    else for (int32 I=0;I<Sections;++I) { SectionPlan.Add(Recipe); Offsets.Add(I*Recipe.SectionHeight); }
    int32 PlannedCount=0;
    for (const auto& Row:SectionPlan) PlannedCount+=Row.Pieces.Num()+Row.NativePieces.Num();
    if (PlannedCount>500) { Status=TEXT("Assembly exceeds 500 authored actors."); return; }
#if WITH_EDITOR
    if (!GetWorld()->IsGameWorld()) Modify();
#endif
    ClearGenerated();
    ArtRandom.Initialize(HashCombineFast(Seed,0xA471));
    BuiltRoles.Reset();
    for (int32 I=0;I<SectionPlan.Num();++I)
    {
        AddSection(SectionPlan[I],Offsets[I]);
        BuiltRoles.Add(SectionPlan[I].Role);
    }
    const float Start=FillStart;
    const int32 Steps=FMath::CeilToInt((FillEnd-Start)/Recipe.StepRise);
    for (int32 I=1;I<=Steps;++I)
    {
        const float Z=Start+(FillEnd-Start)*I/FMath::Max(1,Steps);
        const bool Left=(I%2)==1;
        FPOIKitSettings S; S.Kind=Recipe.Climbing; S.Collision=Recipe.Collision; S.Size=FVector(Recipe.Width*.5-60,100,100); S.bFacePositiveX=Left; S.Image=Recipe.PlatformImage;
        AddBlock(S,FTransform(FVector((Left?-1:1)*(Recipe.Width*.25+30),0,Z)));
    }
    // Background at +Y is behind the playable lane. Side walls leave the lower-room exit clear.
    FPOIKitSettings Back; Back.Kind=EPOIKitKind::Background; Back.Collision=EPOIKitCollision::NoPhysicalCollision;
    Back.Collision=EPOIKitCollision::Solid;
    Back.Size=FVector(Recipe.Width,1,Height); Back.Image=Recipe.BackImage;
    // Authored sections already contain their background; only fill uncovered height.
    if (SectionPlan.IsEmpty()) AddBlock(Back,FTransform(FVector(0,65,Height)),true);
    else if (FillEnd>FillStart)
    { Back.Size.Z=FillEnd-FillStart; AddBlock(Back,FTransform(FVector(0,65,FillEnd)),true); }
    if (bBuildBoundaryWalls && Height>BottomExitClearance)
        for (float Side:{-1.f,1.f})
        {
            FPOIKitSettings S; S.Kind=EPOIKitKind::SideWall; S.Size=FVector(25,100,Height-FMath::Max(0.f,BottomExitClearance)); S.Image=Recipe.SideImage;
            AddBlock(S,FTransform(FVector(Side*(Recipe.Width*.5+12.5),0,Height)),true);
        }
    Status=FString::Printf(TEXT("Built %d blocks. Test the route with the actual character before export."),GeneratedBlocks.Num());
#if WITH_EDITOR
    if (!GetWorld()->IsGameWorld()) MarkPackageDirty();
#endif
}
void APOIShaft::ExportSection()
{
#if WITH_EDITOR
    if (GetWorld()->IsGameWorld() || !TargetTable || TargetTable->GetRowStruct()!=FPOIShaftRow::StaticStruct() || RowName.IsNone())
    { Status=TEXT("Select a POIShaftRow DataTable and row name in the level editor."); return; }
    if (bAssembleModules) { Status=TEXT("Export individual modules, not the assembly preview."); return; }
    FPOIShaftRow Row=Recipe; Row.SectionHeight=Height; Row.Pieces.Reset(); Row.NativePieces.Reset(); Row.bUseCustomSection=true;
    TArray<AActor*> Attached; GetAttachedActors(Attached);
    for (auto* A:Attached) if (auto* B=Cast<APOIKitBlock>(A)) if (!B->ActorHasTag(TEXT("POIAutoBoundary")))
    { FPOIShaftPiece P; P.Block=B->Settings; P.Transform=B->GetActorTransform().GetRelativeTransform(GetActorTransform()); Row.Pieces.Add(P); }
    for (auto* Actor:Attached)
    {
        UClass* BaseClass=Actor->GetClass();
        while (BaseClass && !BaseClass->GetPathName().StartsWith(TEXT("/AST_ParkourSystem/Blueprints/Actor/"))) BaseClass=BaseClass->GetSuperClass();
        if (!BaseClass) continue;
        auto* Spline=Actor->FindComponentByClass<USplineComponent>();
        if (!Spline) continue;
        FPOIShaftNativePiece Piece;
        Piece.ActorClass=Actor->GetClass();
        Piece.Transform=Actor->GetActorTransform().GetRelativeTransform(GetActorTransform());
        Piece.bClosedLoop=Spline->IsClosedLoop();
        for (int32 I=0;I<Spline->GetNumberOfSplinePoints();++I)
            Piece.Points.Add(Spline->GetSplinePointAt(I,ESplineCoordinateSpace::Local));
        for (const FName Name:{FName(TEXT("SplineLength")),FName(TEXT("Length")),FName(TEXT("Height")),FName(TEXT("LedgeType")),FName(TEXT("LedgeDepth")),FName(TEXT("ClimbStyle"))})
            if (auto* Property=FindFProperty<FProperty>(Actor->GetClass(),Name))
            {
                FString Value;
                Property->ExportText_InContainer(0,Value,Actor,nullptr,Actor,PPF_None);
                Piece.Settings.Add(Name,Value);
            }
        Row.NativePieces.Add(MoveTemp(Piece));
    }
    Row.Pieces.Sort([](const auto& A,const auto& B){return A.Transform.GetLocation().Z<B.Transform.GetLocation().Z;});
    if (!Row.Validate(Status)) return;
    TargetTable->Modify(); TargetTable->AddRow(RowName,Row); TargetTable->MarkPackageDirty();
    Status=TEXT("Section exported. Save the DataTable, then use it in PWG's Shaft Variants table for a new world.");
#endif
}
void APOIShaft::EndPlay(const EEndPlayReason::Type Reason)
{ ClearGenerated(); Super::EndPlay(Reason); }

void APOIShaft::ApplyLoadedArt()
{
    RefreshLayerRenderVisibility();
    const auto* Theme=ShaftArt(Recipe);
    // Saved authored rooms without an ArtTheme still need their PNG materials lit.
    for (int32 I=0;I<GeneratedLevels.Num();++I)
    {
        ULevelStreamingDynamic* Stream=GeneratedLevels[I];
        if (!Stream || !Stream->IsLevelVisible() || DecoratedLevels.Contains(Stream)) continue;
        auto* Level=Stream->GetLoadedLevel();
        if (!Level) continue;
        if (Theme) for (AActor* Actor:Level->Actors) if (Actor && Actor->FindFunction(TEXT("Get PullPush IK Location")))
            UPOICorridorTools::ConfigurePushPlane(Actor);
        FRandomStream Random(HashCombineFast(Seed,I+1));
        TArray<APOIKitBlock*> Blocks;
        for (AActor* A:Level->Actors) if (auto* B=Cast<APOIKitBlock>(A)) Blocks.Add(B);
        Blocks.Sort([](const APOIKitBlock& A,const APOIKitBlock& B){return A.GetFName().LexicalLess(B.GetFName());});
        for (auto* Block:Blocks)
        {
            if (Theme) Theme->Apply(Block->Settings,Random);
            Block->RefreshAppearance();
            if (bProjectBackground && Block->Settings.Kind==EPOIKitKind::Background)
                for (auto* C:Block->GetComponents()) if (auto* Mesh=Cast<UStaticMeshComponent>(C)) Mesh->SetVisibility(false);
        }
        int32 LitSlots=0;
        for (AActor* Actor:Level->Actors)
            if (IsValid(Actor) && !Actor->IsA<APOIKitBlock>() && !Actor->IsA<APawn>() && !Actor->IsA<ASirrohWorldItem>())
                LitSlots+=POIImageLighting::ApplyToActor(Actor);
        if (LitSlots>0) UE_LOG(LogTemp,Display,TEXT("POI lit native PNG slots=%d level=%s"),LitSlots,*Stream->GetWorldAssetPackageName());
        DecoratedLevels.Add(Stream);
    }
}
