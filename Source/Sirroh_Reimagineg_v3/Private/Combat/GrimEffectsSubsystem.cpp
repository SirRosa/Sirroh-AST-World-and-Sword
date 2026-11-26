#include "Combat/GrimEffectsSubsystem.h"
#include "ProceduralMeshComponent.h"
#include "Procedural_World_Generator/PWGProceduralTree.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundConcurrency.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

namespace
{
    TAutoConsoleVariable<int32> Enabled(TEXT("Combat.GrimFX"),1,TEXT("Enable local grim combat VFX and SFX."));
    TAutoConsoleVariable<float> Range(TEXT("Combat.GrimFX.Range"),3200.f,TEXT("Maximum cosmetic distance in cm."));
    TAutoConsoleVariable<int32> Budget(TEXT("Combat.GrimFX.Particles"),512,TEXT("Global particle/ribbon quad budget, 16..512."));
    const FLinearColor Palette[]={FLinearColor(0.13f,0.10f,0.065f),FLinearColor(0.22f,0.012f,0.02f),
        FLinearColor(0.20f,0.21f,0.19f),FLinearColor(0.65f,0.35f,0.10f),FLinearColor(0.22f,0.12f,0.055f)};
}
bool UGrimEffectsSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    const auto* World=Cast<UWorld>(Outer);
    return World && World->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
}
bool UGrimEffectsSubsystem::IsTickable() const { return bReady && !IsTemplate(); }
TStatId UGrimEffectsSubsystem::GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(UGrimEffectsSubsystem,STATGROUP_Tickables); }
void UGrimEffectsSubsystem::OnWorldBeginPlay(UWorld& World)
{
    Super::OnWorldBeginPlay(World);
    if (World.GetNetMode()==NM_DedicatedServer) return;
    RenderActor=World.SpawnActor<AActor>();
    if (!RenderActor) return;
    RenderActor->SetActorEnableCollision(false);
    Mesh=NewObject<UProceduralMeshComponent>(RenderActor);
    RenderActor->SetRootComponent(Mesh); RenderActor->AddInstanceComponent(Mesh);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); Mesh->SetCastShadow(false);
    Mesh->SetCanEverAffectNavigation(false); Mesh->bReceivesDecals=false;
    Mesh->SetComponentTickEnabled(false); Mesh->RegisterComponent();
    auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Systems/Procedural_Combat/GrimFX/M_GrimPixels.M_GrimPixels"));
    if (!Material) UE_LOG(LogTemp,Error,TEXT("GrimFX: M_GrimPixels missing; run Tools/BuildGrimEffects.py in Unreal Editor."));
    Mesh->SetMaterial(0,Material);
    auto* SwingMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Systems/Procedural_Combat/GrimFX/M_GrimSwing.M_GrimSwing"));
    Mesh->SetMaterial(1,SwingMaterial?SwingMaterial:Material);
    Particles.Reserve(Capacity); Vertices.SetNumZeroed(Capacity*4); Normals.Init(FVector(0,-1,0),Capacity*4);
    UVs.SetNumZeroed(Capacity*4); Colors.Init(FLinearColor::Black,Capacity*4); Indices.Reserve(Capacity*6);
    for (int32 I=0;I<Capacity;++I)
    {
        const int32 V=I*4; Indices.Append({V,V+1,V+2,V,V+2,V+3});
        UVs[V]=FVector2D(0,0); UVs[V+1]=FVector2D(1,0); UVs[V+2]=FVector2D(1,1); UVs[V+3]=FVector2D(0,1);
    }
    Mesh->CreateMeshSection_LinearColor(0,Vertices,Indices,Normals,UVs,Colors,TArray<FProcMeshTangent>(),false,false);
    RibbonVertices=Vertices;
    Mesh->CreateMeshSection_LinearColor(1,RibbonVertices,Indices,Normals,UVs,Colors,TArray<FProcMeshTangent>(),false,false);
    Mesh->SetVisibility(false);
    Attenuation=NewObject<USoundAttenuation>(this);
    Attenuation->Attenuation.bAttenuate=true; Attenuation->Attenuation.bSpatialize=true;
    Attenuation->Attenuation.AttenuationShapeExtents=FVector(200);
    Attenuation->Attenuation.FalloffDistance=3000;
    Concurrency=NewObject<USoundConcurrency>(this);
    Concurrency->Concurrency.MaxCount=16;
    Concurrency->Concurrency.ResolutionRule=EMaxConcurrentResolutionRule::StopQuietest;
    Concurrency->Concurrency.VoiceStealReleaseTime=0.025f;
    const TCHAR* Groups[]={TEXT("Swing"),TEXT("Flesh"),TEXT("Earth"),TEXT("Stone"),TEXT("Metal"),TEXT("Wood")};
    for (const TCHAR* Group:Groups) for (int32 Variant=1;Variant<=3;++Variant)
    {
        const FString Name=FString::Printf(TEXT("S_Grim_%s_%02d"),Group,Variant);
        auto* Asset=LoadObject<USoundBase>(nullptr,*(TEXT("/Game/Systems/Procedural_Combat/GrimFX/Audio/")+Name+TEXT(".")+Name));
        Sounds.Add(Asset);
        if (!Asset) UE_LOG(LogTemp,Error,TEXT("GrimFX: missing sound %s"),*Name);
    }
    // Existing GASP recordings: separate walk/run banks, loaded once and shared by all characters.
    for (const TCHAR* Gait:{TEXT("walk"),TEXT("run")}) for (int32 Variant=1;Variant<=8;++Variant)
    {
        const FString Name=FString::Printf(TEXT("Foley_fs_1p_sneaker_concrete_%s_%02d"),Gait,Variant);
        auto* Asset=LoadObject<USoundBase>(nullptr,*(TEXT("/Game/Audio/Foley/SoundWaves/Sneaker/Concrete/")+Name+TEXT(".")+Name));
        Sounds.Add(Asset);
        if (!Asset) UE_LOG(LogTemp,Error,TEXT("GrimFX: missing GASP footstep %s"),*Name);
    }
    bReady=true;
}
bool UGrimEffectsSubsystem::IsNear(const FVector& Point) const
{
    if (!bReady || !Enabled.GetValueOnGameThread() || Point.ContainsNaN()) return false;
    const double Radius=FMath::Max(0.f,Range.GetValueOnGameThread());
    for (auto It=GetWorld()->GetPlayerControllerIterator();It;++It)
        if (const auto* PC=It->Get(); PC && PC->IsLocalController())
        {
            const AActor* View=PC->GetPawn()?static_cast<const AActor*>(PC->GetPawn()):PC->GetViewTarget();
            if (View && FVector::DistSquared(Point,View->GetActorLocation())<=Radius*Radius) return true;
        }
    return false;
}
EGrimSurface UGrimEffectsSubsystem::Surface(const AActor* Actor,const UPrimitiveComponent* Component,const UPhysicalMaterial* Material)
{
    const auto Tagged=[&](FName Tag){return (Actor && Actor->ActorHasTag(Tag)) || (Component && Component->ComponentHasTag(Tag));};
    if (Tagged(TEXT("FX.Metal")) || Tagged(TEXT("CombatWeaponBody"))) return EGrimSurface::Metal;
    if (Tagged(TEXT("FX.Wood")) || (Actor && Actor->IsA<APWGProceduralTree>())) return EGrimSurface::Wood;
    if (Tagged(TEXT("FX.Stone"))) return EGrimSurface::Stone;
    if (Tagged(TEXT("FX.Flesh"))) return EGrimSurface::Flesh;
    if (Tagged(TEXT("FX.Earth")) || Tagged(TEXT("CombatTerrain"))) return EGrimSurface::Earth;
    if (!Material && Component)
        if (const FBodyInstance* Body=Component->GetBodyInstance()) Material=Body->GetSimplePhysicalMaterial();
    FString Name=Material?Material->GetName():FString();
    if (Material) for (const auto& Entry:UPhysicsSettings::Get()->PhysicalSurfaces)
        if (Entry.Type==Material->SurfaceType) { Name+=Entry.Name.ToString(); break; }
    if (Name.Contains(TEXT("Metal"),ESearchCase::IgnoreCase)) return EGrimSurface::Metal;
    if (Name.Contains(TEXT("Stone"),ESearchCase::IgnoreCase) || Name.Contains(TEXT("Rock"),ESearchCase::IgnoreCase)) return EGrimSurface::Stone;
    if (Name.Contains(TEXT("Wood"),ESearchCase::IgnoreCase)) return EGrimSurface::Wood;
    return EGrimSurface::Earth;
}
void UGrimEffectsSubsystem::Sound(int32 Group,const FVector& Point,float Volume,float Pitch)
{
    if (!IsNear(Point)) return;
    const float Now=GetWorld()->GetTimeSeconds();
    if (Now-AudioWindow>=0.1f) { AudioWindow=Now; AudioCount=0; }
    if (AudioCount>=8) return;
    const int32 Index=Group<6?Group*3+Random.RandRange(0,2):18+(Group-6)*8+Random.RandRange(0,7);
    if (!Sounds.IsValidIndex(Index) || !Sounds[Index]) return;
    ++AudioCount;
    UGameplayStatics::PlaySoundAtLocation(this,Sounds[Index],Point,FRotator::ZeroRotator,Volume,
        Pitch*(Group<6?Random.FRandRange(0.94f,1.06f):1.f),0,Attenuation,Concurrency);
}
void UGrimEffectsSubsystem::Burst(EGrimSurface Kind,const FVector& Point,const FVector& Direction,int32 Count,float Strength,bool Drip)
{
    if (!IsNear(Point)) return;
    const int32 Limit=FMath::Clamp(Budget.GetValueOnGameThread(),16,Capacity);
    const int32 K=FMath::Clamp(int32(Kind),0,4);
    const FVector Axis=Direction.GetSafeNormal(FVector::FReal(0.001),FVector::UpVector);
    for (int32 I=0;I<Count && Particles.Num()<Limit;++I)
    {
        auto& P=Particles.AddDefaulted_GetRef();
        P.Position=Point+Random.VRand()*2;
        P.Color=Palette[K]*(0.65f+0.25f*Random.RandRange(0,2)); P.Color.A=1;
        P.Life=Drip?0.6f:Random.FRandRange(0.18f,Kind==EGrimSurface::Earth?0.55f:0.42f);
        P.Size=Drip?Random.FRandRange(0.8f,1.5f):Random.FRandRange(1.f,Kind==EGrimSurface::Earth?4.5f:2.4f);
        P.Velocity=Drip?FVector(0,0,-35):(Axis*Random.FRandRange(30,110)+Random.VRand()*70+FVector(0,0,45))*Strength;
        P.Gravity=Kind==EGrimSurface::Earth?-70:-650;
    }
}
void UGrimEffectsSubsystem::Impact(EGrimSurface Kind,const FVector& Point,const FVector& Direction,float Strength)
{
    Strength=FMath::Clamp(Strength,0.6f,1.7f);
    Burst(Kind,Point,Direction,FMath::RoundToInt((Kind==EGrimSurface::Flesh?18:11)*Strength),Strength);
    const int32 Groups[]={2,1,3,4,5};
    Sound(Groups[FMath::Clamp(int32(Kind),0,4)],Point,0.65f*Strength);
}
void UGrimEffectsSubsystem::Step(EGrimSurface Kind,const FVector& Point,bool Sprint)
{
    if (Sprint && Kind==EGrimSurface::Earth) Burst(Kind,Point,FVector::UpVector,5,0.35f);
    Sound(Sprint?7:6,Point,Sprint?0.55f:0.36f);
}
void UGrimEffectsSubsystem::Bleed(const FVector& Point,float Strength) { Burst(EGrimSurface::Flesh,Point,-FVector::UpVector,FMath::Clamp(FMath::CeilToInt(Strength),1,3),1,true); }
void UGrimEffectsSubsystem::Swing(const FVector& Point,float Strength) { Sound(0,Point,0.48f*Strength,1.1f-0.12f*Strength); }
void UGrimEffectsSubsystem::Trail(const FVector& OldBase,const FVector& OldTip,const FVector& Base,const FVector& Tip)
{
    if (!IsNear(Tip)) return;
    const float Travel=FVector::Distance(OldTip,Tip);
    if(Travel<2.f || Travel>80.f || FVector::DistSquared(OldBase,Base)>FMath::Square(60.f))return;
    const FVector From=(OldTip-OldBase).GetSafeNormal(),To=(Tip-Base).GetSafeNormal();
    if(From.IsNearlyZero() || To.IsNearlyZero() || FVector::DotProduct(From,To)<0)return;
    const FQuat Turn=FQuat::FindBetweenNormals(From,To);
    const float OldLength=FVector::Distance(OldBase,OldTip),Length=FVector::Distance(Base,Tip);
    const int32 Segments=FMath::Clamp(FMath::CeilToInt(Travel/4.f),1,12);
    auto Edge=[&](float T,FVector& Inner,FVector& Outer){
        const FVector Direction=FQuat::Slerp(FQuat::Identity,Turn,T).RotateVector(From);
        const float L=FMath::Lerp(OldLength,Length,T);
        Outer=FMath::Lerp(OldBase,Base,T)+Direction*L;
        Inner=Outer-Direction*FMath::Min(7.f,L*.16f);
    };
    for(int32 I=0;I<Segments && Particles.Num()<FMath::Clamp(Budget.GetValueOnGameThread(),16,Capacity);++I){
        auto& P=Particles.AddDefaulted_GetRef();P.Ribbon=true;P.Life=.075f;P.Position=Tip;
        Edge(float(I)/Segments,P.Corners[0],P.Corners[1]);
        Edge(float(I+1)/Segments,P.Corners[3],P.Corners[2]);
        P.Color=FLinearColor(.42f,.45f,.46f,.18f);
    }
}
void UGrimEffectsSubsystem::Tick(float Delta)
{
    if (!Enabled.GetValueOnGameThread()) Particles.Reset();
    Clock+=Delta; if (Clock<1.f/60.f) return;
    const float Step=Clock; Clock=0;
    for (int32 I=Particles.Num()-1;I>=0;--I)
    {
        auto& P=Particles[I]; P.Age+=Step;
        if (P.Age>=P.Life) { Particles.RemoveAtSwap(I,1,EAllowShrinking::No); continue; }
        if (!P.Ribbon) { P.Velocity.Z+=P.Gravity*Step; P.Position+=P.Velocity*Step; }
    }
    if (Particles.IsEmpty()) { if (bVisible) Mesh->SetVisibility(false); bVisible=false; return; }
    // Fixed topology: update vertex buffers only. No particle actors, collision, shadows, decals or per-hit components.
    const FVector Origin=Particles[0].Position;
    Mesh->SetWorldLocation(Origin);
    for (int32 I=0;I<Capacity;++I)
    {
        const int32 V=I*4;
        if (!Particles.IsValidIndex(I)) { for (int32 J=0;J<4;++J) Vertices[V+J]=FVector::ZeroVector; continue; }
        const auto& P=Particles[I];
        const float Fade=P.Ribbon?FMath::Square(FMath::Clamp(1.f-P.Age/P.Life,0.f,1.f)):FMath::Max(0.25f,1.f-FMath::FloorToFloat(P.Age/P.Life*4)*0.25f);
        if (P.Ribbon)
        {
            const FVector A=FMath::Lerp(P.Corners[1],P.Corners[0],double(Fade));
            const FVector B=FMath::Lerp(P.Corners[2],P.Corners[3],double(Fade));
            Vertices[V]=A-Origin; Vertices[V+1]=P.Corners[1]-Origin; Vertices[V+2]=P.Corners[2]-Origin; Vertices[V+3]=B-Origin;
        }
        else
        {
            // Square cells, hard silhouette and four-step disappearance suit the world's pseudo pixel art.
            const FVector C=FVector(FMath::GridSnap(P.Position.X,1.0),P.Position.Y,FMath::GridSnap(P.Position.Z,1.0))-Origin;
            const double S=FMath::Max(0.5f,P.Size*Fade);
            Vertices[V]=C+FVector(-S,0,-S); Vertices[V+1]=C+FVector(S,0,-S);
            Vertices[V+2]=C+FVector(S,0,S); Vertices[V+3]=C+FVector(-S,0,S);
        }
        for (int32 J=0;J<4;++J) {Colors[V+J]=P.Color;if(P.Ribbon)Colors[V+J].A*=Fade;}
    }
    for(int32 I=0;I<Capacity;++I)for(int32 J=0;J<4;++J){
        const int32 V=I*4+J;const bool Ribbon=Particles.IsValidIndex(I) && Particles[I].Ribbon;
        RibbonVertices[V]=Ribbon?Vertices[V]:FVector::ZeroVector;
        if(Ribbon)Vertices[V]=FVector::ZeroVector;
    }
    Mesh->UpdateMeshSection_LinearColor(0,Vertices,Normals,UVs,Colors,TArray<FProcMeshTangent>(),false);
    Mesh->UpdateMeshSection_LinearColor(1,RibbonVertices,Normals,UVs,Colors,TArray<FProcMeshTangent>(),false);
    if (!bVisible) Mesh->SetVisibility(true); bVisible=true;
}
void UGrimEffectsSubsystem::Deinitialize()
{
    bReady=false; Particles.Reset(); Sounds.Reset();
    if (IsValid(RenderActor)) RenderActor->Destroy();
    Mesh=nullptr; RenderActor=nullptr;
    Super::Deinitialize();
}

#if !UE_BUILD_SHIPPING
namespace
{
    // User-invoked only; never executed by asset generation or initialization.
    FAutoConsoleCommandWithWorldAndArgs Preview(TEXT("Combat.GrimFX.Preview"),
        TEXT("Preview one effect: swing, flesh, earth, stone, metal, wood, step or bleed."),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args,UWorld* World)
        {
            if (!World || Args.IsEmpty()) return;
            auto* PC=World->GetFirstPlayerController(); auto* FX=World->GetSubsystem<UGrimEffectsSubsystem>();
            if (!PC || !PC->GetPawn() || !FX) return;
            const FVector P=PC->GetPawn()->GetActorLocation()+FVector(-70,-20,20);
            const FString Name=Args[0].ToLower();
            if (Name==TEXT("swing"))
            {
                FX->Swing(P,1);
                for (int32 I=0;I<5;++I) FX->Trail(P,P+FVector(-70+I*9,0,60-I*12),P,P+FVector(-61+I*9,0,48-I*12));
            }
            else if (Name==TEXT("bleed")) FX->Bleed(P,3);
            else if (Name==TEXT("step")) FX->Step(EGrimSurface::Earth,P,true);
            else
            {
                const TArray<FString> Names={TEXT("earth"),TEXT("flesh"),TEXT("stone"),TEXT("metal"),TEXT("wood")};
                const int32 Index=Names.IndexOfByKey(Name);
                if (Index!=INDEX_NONE) FX->Impact(static_cast<EGrimSurface>(Index),P,FVector(-1,0,1),1);
            }
        }));
    FAutoConsoleCommandWithWorldAndArgs Surfaces(TEXT("Combat.GrimFX.TestSurfaces"),
        TEXT("Spawn assigned earth/stone/metal/wood contact targets near the player. 'clear' removes these targets."),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args,UWorld* World)
        {
            if (!World || !World->IsGameWorld()) return;
            TArray<AActor*> Existing; UGameplayStatics::GetAllActorsWithTag(World,TEXT("GrimFX.Test"),Existing);
            if (!Args.IsEmpty() && Args[0]==TEXT("clear")) { for (auto* A:Existing) A->Destroy(); return; }
            if (!Existing.IsEmpty()) return;
            auto* PC=World->GetFirstPlayerController(); if (!PC || !PC->GetPawn()) return;
            const FVector Origin=PC->GetPawn()->GetActorLocation();
            auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
            if (!Cube) return;
            const FName Tags[]={TEXT("FX.Earth"),TEXT("FX.Stone"),TEXT("FX.Metal"),TEXT("FX.Wood")};
            for (int32 I=0;I<4;++I)
            {
                FVector P=Origin+FVector(-250-I*230,0,0);
                FHitResult Floor; FCollisionQueryParams Params(SCENE_QUERY_STAT(GrimTestStand),false,PC->GetPawn());
                if (!World->LineTraceSingleByChannel(Floor,P+FVector(0,0,200),P-FVector(0,0,600),ECC_Visibility,Params)) continue;
                P.Z=Floor.ImpactPoint.Z+(I==0?5:45);
                auto* A=World->SpawnActor<AStaticMeshActor>(P,FRotator::ZeroRotator);
                if (!A) continue;
                A->Tags.Add(TEXT("GrimFX.Test")); A->Tags.Add(Tags[I]);
                auto* C=A->GetStaticMeshComponent(); C->SetMobility(EComponentMobility::Movable); C->SetStaticMesh(Cube);
                C->SetCollisionProfileName(TEXT("BlockAll")); C->SetWorldScale3D(I==0?FVector(1.5,1.5,0.1):FVector(0.65,0.65,0.9));
                C->SetCastShadow(false);
#if WITH_EDITOR
                A->SetActorLabel(Tags[I].ToString());
#endif
            }
        }));
}
#endif
