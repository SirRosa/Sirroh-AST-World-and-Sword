#include "Procedural_World_Generator/PWGv2Generator.h"
#include "Components/CapsuleComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"

UPWGv2WorldSave* APWGv2Generator::BeginDebugSession()
{
    if (!WorldData || !HasAuthority()) return nullptr;
    if (!DebugOriginalWorld)
    {
        DebugOriginalWorld=WorldData;
        DebugOriginalSettings=Settings;
        for(auto* Table:{&BiomesTable,&TreesTable,&UndergroundTable,&TerrainTexturesTable})
            { DebugOriginalTables.Add(*Table); if(*Table) { *Table=DuplicateObject<UDataTable>(Table->Get(),this); (*Table)->SetFlags(RF_Transient); } }

        WorldData=DuplicateObject<UPWGv2WorldSave>(WorldData,this);
        DebugWorkingWorld=DuplicateObject<UPWGv2WorldSave>(WorldData,this);
    }
    return DebugWorkingWorld;
}

FString APWGv2Generator::DebugLocation(const FVector& Position) const
{
    const FVector P=GetActorTransform().InverseTransformPosition(Position);
    FName Region=NAME_None,Biome=NAME_None;
    if (WorldData)
    {
        for (const auto& R:WorldData->Regions) if (P.X>=R.StartX && P.X<=R.EndX) { Region=R.Id; break; }
        for (const auto& B:WorldData->Biomes) if (P.X>=B.StartX && P.X<=B.EndX) { Biome=B.Id; break; }
    }
    return FString::Printf(TEXT("X: %.2f m   Y: %.2f m   Z: %.2f m\nRegion: %s\nBiom: %s\nWarstwa: %d | Seed: %d\n%s"),
        P.X/100,P.Y/100,P.Z/100,*Region.ToString(),*Biome.ToString(),LayerAt(Position),WorldSeed,
        IsDebugSession()?TEXT("SANDBOX - zapis wyspy zablokowany"):TEXT("Oryginalna wyspa"));
}

bool APWGv2Generator::DebugFindDestination(FName Region,FName Biome,int32 Layer,APawn* Pawn,FVector& Out,FString& Error)
{
    auto* Capsule=Pawn?Pawn->FindComponentByClass<UCapsuleComponent>():nullptr;
    if (!WorldData || !Capsule || WorldData->Regions.IsEmpty() || Layer<0 || Layer>4 ||
        (Layer>0 && (!Settings.bEnableUnderground || WorldData->Layers.Num()!=4)))
    { Error=TEXT("Brak wyspy, kapsuly lub wygenerowanej warstwy."); return false; }
    const float Radius=Capsule->GetScaledCapsuleRadius(),Half=Capsule->GetScaledCapsuleHalfHeight();
    FCollisionQueryParams Params(SCENE_QUERY_STAT(DebugTeleport),false,Pawn);
    const float CurrentX=GetActorTransform().InverseTransformPosition(Pawn->GetActorLocation()).X;
    TArray<float> Candidates;
    for (const auto& R:WorldData->Regions)
    {
        if (!Region.IsNone() && R.Id!=Region) continue;
        for (const auto& B:WorldData->Biomes)
        {
            if (!Biome.IsNone() && B.Id!=Biome) continue;
            const float A=FMath::Max(R.StartX,B.StartX)+200, Z=FMath::Min(R.EndX,B.EndX)-200;
            if (A>=Z) continue;
            for (int32 I=0;I<9;++I) Candidates.Add(FMath::Lerp(A,Z,(I+0.5f)/9));
        }
    }
    Candidates.Sort([CurrentX](float A,float B){ return FMath::Abs(A-CurrentX)<FMath::Abs(B-CurrentX); });
    int32 Attempts=0;
    for (float X:Candidates)
    {
        if (++Attempts>80) break;
        const float Floor=LayerFloor(Layer,X);
        if (Layer>0 && LayerCeiling(Layer,X)-Floor<Half*2+20) continue;
        const FVector Expected=GetActorTransform().TransformPosition(FVector(X,0,Floor));
        StreamAt(X,true); // Synchronize nearby collision before any query.
        FHitResult Hit;
        if (!GetWorld()->LineTraceSingleByChannel(Hit,Expected+FVector(0,0,80),Expected-FVector(0,0,80),ECC_Pawn,Params) ||
            Hit.GetActor()!=this || Hit.ImpactNormal.Z<0.7f) continue;
        const FVector Centre=Hit.ImpactPoint+FVector(0,0,Half+5);
        if (GetWorld()->OverlapBlockingTestByChannel(Centre,Capsule->GetComponentQuat(),ECC_Pawn,
            FCollisionShape::MakeCapsule(Radius,Half),Params)) continue;
        Out=Centre-(Capsule->GetComponentLocation()-Pawn->GetActorLocation());
        if (LayerAt(Out)!=Layer) continue;
        DebugTeleportPinX=X; DebugTeleportPinUntil=GetWorld()->GetTimeSeconds()+1.0;
        return true;
    }
    StreamAt(CurrentX,true);
    Error=TEXT("Brak bezpiecznego miejsca w tej kombinacji region/biom/warstwa. Teleport anulowany.");
    return false;
}

bool APWGv2Generator::DebugRebuild(bool Restore,FString& Error,bool Replan)
{
    if (!BeginDebugSession()) { Error=TEXT("Brak wygenerowanej wyspy lub uprawnien hosta."); return false; }
    if (Restore)
    {
        int32 TableIndex=0;
        for(auto* Table:{&BiomesTable,&TreesTable,&UndergroundTable,&TerrainTexturesTable})
        {
            UDataTable* Original=DebugOriginalTables.IsValidIndex(TableIndex)?DebugOriginalTables[TableIndex].Get():nullptr;
            *Table=Original?DuplicateObject<UDataTable>(Original,this):nullptr;
            if(*Table)(*Table)->SetFlags(RF_Transient);
            ++TableIndex;
        }
        WorldData=DuplicateObject<UPWGv2WorldSave>(DebugOriginalWorld,this);
        Settings=DebugOriginalSettings;
        WorldData->Settings=Settings;
        DebugWorkingWorld=DuplicateObject<UPWGv2WorldSave>(WorldData,this);
    }
    else WorldData=DuplicateObject<UPWGv2WorldSave>(DebugWorkingWorld,this);
    // Topology (span boundaries, sample spacing and island size) remains fixed in this rebuild.
    Settings=WorldData->Settings;
    Settings.SampleSpacing=DebugOriginalSettings.SampleSpacing;
    Settings.ChunkLengthMetres=FMath::Clamp(Settings.ChunkLengthMetres,8.f,128.f);
    Settings.LaneSpacing=FMath::Clamp(Settings.LaneSpacing,100.f,300.f);
    Settings.LoadRadius=FMath::Clamp(Settings.LoadRadius,1,4);
    Settings.RetainRadius=FMath::Clamp(Settings.RetainRadius,Settings.LoadRadius,6);
    Settings.ForegroundDepth=FMath::Clamp(Settings.ForegroundDepth,500.f,10000.f);
    Settings.BackgroundDepth=FMath::Clamp(Settings.BackgroundDepth,500.f,20000.f);
    Settings.RegionBlendMetres=FMath::Clamp(Settings.RegionBlendMetres,1.f,100.f);
    Settings.MaxPlayableSlope=FMath::Clamp(Settings.MaxPlayableSlope,0.05f,0.5f);
    Settings.CoastLengthMetres=FMath::Clamp(Settings.CoastLengthMetres,20.f,500.f);
    WorldData->Settings=Settings;
    if (!Restore)
    {
        for (int32 I=0;I<WorldData->Layers.Num();++I)
        {
            auto& L=WorldData->Layers[I];
            L.Layer=I+1; L.ClearHeightMetres=FMath::Clamp(L.ClearHeightMetres,2.5f,15.f);
            L.RoomLengthMetres=FMath::Clamp(L.RoomLengthMetres,6.f,60.f);
            L.FloorVariationMetres=FMath::Clamp(L.FloorVariationMetres,0.f,2.f);
            const float Min=I==0?3.f:WorldData->Layers[I-1].FloorBelowSeaMetres+L.ClearHeightMetres+5;
            L.FloorBelowSeaMetres=FMath::Clamp(L.FloorBelowSeaMetres,Min,Min+100);
            L.RocksPer100Metres=FMath::Clamp(L.RocksPer100Metres,0.f,100.f);
        }
        for (auto& R:WorldData->Regions)
        {
            auto& D=R.Definition;
            D.MinHeightMetres=FMath::Clamp(D.MinHeightMetres,0.f,500.f);
            D.MaxHeightMetres=FMath::Clamp(D.MaxHeightMetres,D.MinHeightMetres,500.f);
            D.NoiseWavelengthMetres=FMath::Max(D.NoiseWavelengthMetres,20.f);
            D.GrassPerSquareMetre=FMath::Clamp(D.GrassPerSquareMetre,0.f,20.f);
            D.TreesPer100Metres=FMath::Clamp(D.TreesPer100Metres,0.f,100.f);
            D.BushesPer100Metres=FMath::Clamp(D.BushesPer100Metres,0.f,100.f);
            D.RocksPer100Metres=FMath::Clamp(D.RocksPer100Metres,0.f,100.f);
        }
        auto Eval=[&](const FPWGv2RegionSpan& R,float X)
        {
            const auto& D=R.Definition;
            const float N=FMath::PerlinNoise1D(X/(D.NoiseWavelengthMetres*100)+WorldData->Seed%8192);
            return Settings.SeaLevel+100*FMath::Lerp(D.MinHeightMetres,D.MaxHeightMetres,FMath::Clamp(0.5f+0.5f*N,0.f,1.f));
        };
        for (int32 I=0;I<WorldData->Heights.Num();++I)
        {
            const float X=WorldData->MinX+I*Settings.SampleSpacing;
            float Z=Eval(RegionAt(X),X);
            for (int32 R=1;R<WorldData->Regions.Num();++R)
            {
                const float Edge=WorldData->Regions[R].StartX,Half=Settings.RegionBlendMetres*50;
                if (FMath::Abs(X-Edge)<Half) { Z=FMath::Lerp(Eval(WorldData->Regions[R-1],X),Eval(WorldData->Regions[R],X),FMath::SmoothStep(0.f,1.f,(X-Edge+Half)/(2*Half))); break; }
            }
            const float Shore=FMath::Max(0.f,FMath::Abs(X)-WorldData->Regions.Last().EndX);
            WorldData->Heights[I]=FMath::Lerp(Z,Settings.SeaLevel-600,FMath::SmoothStep(0.f,Settings.CoastLengthMetres*100,Shore));
        }
        const int32 Centre=FMath::Clamp(FMath::RoundToInt(-WorldData->MinX/Settings.SampleSpacing),0,WorldData->Heights.Num()-1);
        const float SpawnZ=FMath::Max(Settings.SeaLevel+250,WorldData->Heights[Centre]);
        for(int32 I=0;I<WorldData->Heights.Num();++I)
            if(FMath::Abs(WorldData->MinX+I*Settings.SampleSpacing)<800) WorldData->Heights[I]=SpawnZ;
        const float Step=Settings.MaxPlayableSlope*Settings.SampleSpacing;
        for (int32 I=1;I<WorldData->Heights.Num();++I) WorldData->Heights[I]=FMath::Min(WorldData->Heights[I],WorldData->Heights[I-1]+Step);
        for (int32 I=WorldData->Heights.Num()-2;I>=0;--I) WorldData->Heights[I]=FMath::Min(WorldData->Heights[I],WorldData->Heights[I+1]+Step);
    }
    if (Replan && !Restore) MakeExtendedPlan();
    DebugWorkingWorld=DuplicateObject<UPWGv2WorldSave>(WorldData,this);
    for (auto& P:Chunks) if (P.Value) P.Value->DestroyComponent();
    for (auto& P:GrassChunks) if (P.Value) P.Value->DestroyComponent();
    for (auto& P:UndergroundChunks) if (P.Value) P.Value->DestroyComponent();
    for (auto& P:DecorationChunks)
    {
        for (auto C:P.Value.Components) if (C) C->DestroyComponent();
        for (auto A:P.Value.Actors) if (A && A->GetOwner()==this) A->Destroy();
    }
    Chunks.Reset(); GrassChunks.Reset(); UndergroundChunks.Reset(); DecorationChunks.Reset();
    PendingSurfaceCollision.Reset(); PendingUndergroundCollision.Reset(); AtlasMaterials.Reset();
    return true;
}

TArray<int32> APWGv2Generator::DebugAvailableLayers(FName Region,FName Biome,APawn* Pawn) const
{
    TArray<int32> Result;
    if(!WorldData)return Result;
    const auto* C=Pawn?Pawn->FindComponentByClass<UCapsuleComponent>():nullptr;
    const float Required=C?C->GetScaledCapsuleHalfHeight()*2+20:200;
    for(int32 L=0;L<=4;++L)
    {
        if(L>0 && (!Settings.bEnableUnderground || WorldData->Layers.Num()!=4))continue;
        bool Exists=false;
        for(const auto& R:WorldData->Regions)if(R.Id==Region)
            for(const auto& B:WorldData->Biomes)if(B.Id==Biome)
            {
                const float A=FMath::Max(R.StartX,B.StartX)+200,Z=FMath::Min(R.EndX,B.EndX)-200;
                if(A>=Z)continue;
                for(int32 I=0;I<9;++I){const float X=FMath::Lerp(A,Z,(I+0.5f)/9);if(L==0 || LayerCeiling(L,X)-LayerFloor(L,X)>=Required){Exists=true;break;}}
            }
        if(Exists)Result.Add(L);
    }
    return Result;
}
