#include "Procedural_World_Generator/PWGv2Generator.h"

void APWGv2Generator::PlanShaftTerrain()
{
    // Plan from the surface down so each upper endpoint uses the previously adapted floor.
    for (int32 Layer=0;Layer<4;++Layer)
    for (auto& Portal:WorldData->Portals)
    {
        if (!Portal.bKitShaft || Portal.UpperLayer!=Layer || Portal.LowerLayer!=Layer+1) continue;
        FRandomStream Random(Portal.ShaftSeed);
        const int32 Count=Random.RandRange(4,6);
        auto Pick=[&](EPOIShaftRole DesiredRole)->const FPOIShaftRow*
        {
            float Total=0;
            for (const auto& Row:Portal.ShaftModules) if (Row.Role==DesiredRole) Total+=Row.Weight;
            if (Total<=0) return nullptr;
            float Draw=Random.FRand()*Total;
            for (const auto& Row:Portal.ShaftModules) if (Row.Role==DesiredRole)
            { Draw-=Row.Weight; if (Draw<=0) return &Row; }
            return nullptr;
        };
        Portal.ShaftSectionPlan.Reset();
        float Height=0;
        for (int32 I=0;I<Count;++I)
        {
            const auto* Row=Pick(I==0?EPOIShaftRole::Exit:(I==Count-1?EPOIShaftRole::Entrance:EPOIShaftRole::Middle));
            if (!Row) { Portal.ShaftSectionPlan.Reset(); break; }
            Portal.ShaftSectionPlan.Add(*Row); Height+=Row->SectionHeight;
        }
        if (Portal.ShaftSectionPlan.IsEmpty()) Height=Count*300.f;
        Portal.TopZ=LayerFloor(Layer,Portal.X); Portal.BottomZ=Portal.TopZ-Height;
        Portal.bPOIFirst=true;
        if (Layer==0)
        {
            FPOITerrainStamp Stamp;
            Stamp.Id=FName(*FString::Printf(TEXT("ShaftMouth_%d"),Portal.ShaftSeed));
            Stamp.Center=FVector2D(Portal.X,-Settings.LaneSpacing*.5f);
            Stamp.HalfExtent=FVector2D(Portal.Width*.5f+100.f,Settings.LaneSpacing*.5f+100.f);
            Stamp.Height=Portal.TopZ; Stamp.BlendDistance=400; Stamp.Priority=90;
            WorldData->POITerrainStamps.Add(Stamp);
        }
    }
    FString Error; FPOITerrainStamp::ValidateAndSort(WorldData->POITerrainStamps,Error);
}

bool APWGv2Generator::GetShaftLaneDestination(const FVector& Location,float Radius,float HalfHeight,FVector& Destination) const
{
    if (!WorldData || !Settings.bEnableUnderground) return false;
    const FVector Local=GetActorTransform().InverseTransformPosition(Location);
    if (Local.Y>=-10 || Local.Y < -Settings.LaneSpacing-50) return false;
    for (const auto& P:WorldData->Portals)
    {
        if (!P.bKitShaft || FMath::Abs(Local.X-P.X)>P.Width*.5f-Radius-5.f) continue;
        const float Top=LayerFloor(P.UpperLayer,P.X),Bottom=LayerFloor(P.LowerLayer,P.X);
        if (Local.Z-HalfHeight>Top+30 || Local.Z-HalfHeight<Bottom-10) continue;
        Destination=GetActorTransform().TransformPosition(FVector(Local.X,0,Local.Z));
        return true;
    }
    return false;
}
