#include "PWG/PWGEnvironmentV2.h"
#if !UE_BUILD_SHIPPING
#include "Procedural_World_Generator/PWGv2Generator.h"
#include "PWG/PWGPlayerController.h"
#include "PWG/PWGSidescrollerInputComponent.h"
#include "Items/SirrohCarryProps.h"
#include "ASTFireReactiveComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HighResScreenshot.h"
#include "MoverComponent.h"
#include "DefaultMovementSet/InstantMovementEffects/BasicInstantMovementEffects.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "DynamicRHI.h"

void APWGEnvironmentV2::VerifySemanticVisuals()
{
    static const bool bRequested=FParse::Param(FCommandLine::Get(),TEXT("PWGVisualPOIQA"));
    static const bool bIsolation=FParse::Param(FCommandLine::Get(),TEXT("PWGBackgroundAuditLegacy"));
    static const bool bEnclosed=FParse::Param(FCommandLine::Get(),TEXT("PWGBackgroundAuditEnclosed"));
    if (!bRequested || VisualProbeStep<0 || !GetWorld()->IsGameWorld()) return;
    auto* G=Cast<APWGv2Generator>(Generator);
    auto* PC=GetWorld()->GetFirstPlayerController();
    APawn* Pawn=PC?PC->GetPawn():nullptr;
    if (!G || !Pawn || !PC) return;
    auto Fail=[this](const TCHAR* Why)
    {
        UE_LOG(LogTemp,Error,TEXT("VF_RENDER_FAIL %s"),Why);
        VisualProbeStep=-1; FPlatformMisc::RequestExit(false);
    };
    if(G->WorldName!=TEXT("VF_POIProbe_20261126")) {Fail(TEXT("wrong sandbox slot"));return;}
    auto* Mover=Pawn->FindComponentByClass<UMoverComponent>();
    auto* Input=Pawn->FindComponentByClass<UPWGSidescrollerInputComponent>();
    auto FreezePawn=[&](bool bFreeze)
    {
        Pawn->SetActorTickEnabled(!bFreeze);
        if(Mover) Mover->SetComponentTickEnabled(!bFreeze);
        if(Input) Input->SetComponentTickEnabled(!bFreeze);
    };
    auto SetPreset=[this](int32 Preset,bool bMasks=false,bool bLegacy=false)
    {
        VisualProfile=FPWGVisualProfile::DemoPreset(Preset);
        VisualProfile.BlendSeconds=0.f; VisualProfile.bVisualizeMasks=bMasks;
        if(bLegacy)VisualProfile.Mode=EPWGVisualMode::Legacy;
        RefreshVisualProfile();
    };
    auto Advance=[this](int32 Step){VisualProbeStep=Step;VisualProbeFrames=0;};
    auto NextScene=[&]()
    {
        if(auto* Torch=VisualProbeTorch.Get())Torch->Destroy();
        VisualProbeTorch.Reset();
        FreezePawn(false);
        if(PC->GetViewTarget() && PC->GetViewTarget()!=Pawn)PC->GetViewTarget()->SetActorTickEnabled(true);
        bVisualProbeDisableMaskPass=false;
        PC->ConsoleCommand(TEXT("t.MaxFPS 60"));
        G->SetBackgroundAuditIsolation(0);
        ++VisualProbeScene;
        if(VisualProbeScene>(bIsolation||bEnclosed?3:6))
        {
            UE_LOG(LogTemp,Display,TEXT("VF_RENDER_COMPLETE all requested scene sequences"));
            VisualProbeStep=-1; FPlatformMisc::RequestExit(false); return;
        }
        Advance(0);
    };
    const int32 Frame=++VisualProbeFrames;
    // Explicit A/B/A/B mask-only toggle. Same camera, shader and geometry; full GPU
    // frame timing is an estimate of incremental mask cost, not an isolated pass timer.
    if(VisualProbeStep>=20 && VisualProbeStep<=23)
    {
        if(Frame==1)
        {
            bVisualProbeDisableMaskPass=(VisualProbeStep%2)==0;
            RefreshVisualProfile();
            VisualProbeGPU.Reset(); VisualProbeCPU.Reset();
            VisualProbeLastFrame=FPlatformTime::Seconds();
        }
        const double Now=FPlatformTime::Seconds();
        if(Frame>60)
        {
            const uint32 Cycles=RHIGetGPUFrameCycles(0);
            if(Cycles>0)VisualProbeGPU.Add(FPlatformTime::ToMilliseconds(Cycles));
            VisualProbeCPU.Add(float((Now-VisualProbeLastFrame)*1000.0));
        }
        VisualProbeLastFrame=Now;
        if(Frame>=240)
        {
            auto Percentile=[](TArray<float>& A,float Q)
            {A.Sort();return A.IsEmpty()?0.f:A[FMath::Clamp(FMath::RoundToInt((A.Num()-1)*Q),0,A.Num()-1)];};
            const float GPU50=Percentile(VisualProbeGPU,.5f),GPU95=Percentile(VisualProbeGPU,.95f);
            const float CPU50=Percentile(VisualProbeCPU,.5f),CPU95=Percentile(VisualProbeCPU,.95f);
            UE_LOG(LogTemp,Display,TEXT("VF_GPU scene=%s masks=%d actual=%d run=%d samples=%d gpu_median_ms=%.3f gpu_p95_ms=%.3f wall_median_ms=%.3f wall_p95_ms=%.3f"),
                *VisualProbeLabel,!bVisualProbeDisableMaskPass,G->IsVisualMaskPassEnabled(),VisualProbeStep-20,VisualProbeGPU.Num(),GPU50,GPU95,CPU50,CPU95);
            if(VisualProbeStep==23)
            {
                bVisualProbeDisableMaskPass=false;RefreshVisualProfile();
                PC->ConsoleCommand(TEXT("t.MaxFPS 60"));
                Advance(24);
            }
            else Advance(VisualProbeStep+1);
        }
        return;
    }
    const int32 Wait=VisualProbeStep==0?120:VisualProbeStep==1?180:VisualProbeStep==12?90:VisualProbeStep==25?120:45;
    if(Frame<Wait)return;

    auto Capture=[&](const TCHAR* Suffix,bool bLock=true)->bool
    {
        FVector P; FRotator R; float W=0,H=0;
        if(!GetView(P,R,W,H)) {Fail(TEXT("camera unavailable"));return false;}
        if(bLock && (!P.Equals(VisualProbeCamera,.2f) || !R.Equals(VisualProbeRotation,.02f) || !FMath::IsNearlyEqual(W,VisualProbeWidth,.02f)))
        {Fail(TEXT("camera drift"));return false;}
        if(VisualProfile.Mode==EPWGVisualMode::Underground_Stylized &&
            (!StylizedDepthMaterial || ActiveDepthMaterial!=StylizedDepthMaterial || ProfileMix<.999f))
        {Fail(TEXT("semantic material not active"));return false;}
        const FString Name=VisualProbeLabel+TEXT("_")+Suffix;
        const FString Folder=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/(bIsolation?TEXT("ChatGPT/UnifiedBackground_20261126/Audit"):bEnclosed?TEXT("ChatGPT/UnifiedBackground_20261126/Enclosed"):TEXT("ChatGPT/UnifiedBackground_20261126/Render")));
        IFileManager::Get().MakeDirectory(*Folder,true);
        FString Lights;
        for(TActorIterator<AActor> It(GetWorld());It;++It)
        {
            TArray<UPointLightComponent*> Components;It->GetComponents<UPointLightComponent>(Components);
            for(auto* L:Components)if(L && L->IsVisible() && L->Intensity>0 && FVector::DistSquared(L->GetComponentLocation(),VisualProbeFocus)<2000.0*2000.0)
                Lights+=FString::Printf(TEXT(" %s=%.3f@%s"),*L->GetName(),L->Intensity,*L->GetComponentLocation().ToCompactString());
        }
        UE_LOG(LogTemp,Display,TEXT("VF_RENDER_CAPTURE %s camera=%s rotation=%s width=%.3f pawn=%s layer=%d lights=%s"),
            *Name,*P.ToCompactString(),*R.ToCompactString(),W,*Pawn->GetActorLocation().ToCompactString(),G->LayerAt(Pawn->GetActorLocation()),*Lights);
        if(VisualProfile.bVisualizeMasks)G->DebugVisualCoverage(FTransform(R,P),W,H);
        FScreenshotRequest::RequestScreenshot(Folder/(Name+TEXT(".png")),false,false);
        return true;
    };
    switch(VisualProbeStep)
    {
        case 0:
        {
            if(!G->BeginDebugSession()) {Fail(TEXT("sandbox unavailable"));return;}
            if(!G->DebugVisualScene(VisualProbeScene,Pawn,VisualProbeTarget,VisualProbeFocus,VisualProbeLabel))
            {
                UE_LOG(LogTemp,Error,TEXT("VF_RENDER_SCENE_UNAVAILABLE index=%d"),VisualProbeScene);
                NextScene();return;
            }
            PC->SetIgnoreMoveInput(true);PC->SetIgnoreLookInput(true);
            FreezePawn(false);
            if(Input)Input->CancelLaneMove();
            if(auto* Controller=Cast<APWGPlayerController>(PC))Controller->CurrentTerrainLane=EPWGTerrainLane::Middle;
            if(Mover)
            {
                auto Constraint=Mover->GetPlanarConstraint();Constraint.bConstrainToPlane=true;
                Constraint.PlaneConstraintNormal=FVector::YAxisVector;Constraint.PlaneConstraintOrigin=VisualProbeTarget;
                Mover->SetPlanarConstraint(Constraint);
                auto Teleport=MakeShared<FTeleportEffect>();Teleport->TargetLocation=VisualProbeTarget;
                Teleport->bUseActorRotation=true;Mover->QueueInstantMovementEffect(Teleport);
                auto Stop=MakeShared<FApplyVelocityEffect>();Stop->VelocityToApply=FVector::ZeroVector;
                Stop->bAdditiveVelocity=false;Mover->QueueInstantMovementEffect(Stop);
            }
            else if(!Pawn->TeleportTo(VisualProbeTarget,Pawn->GetActorRotation())) {Fail(TEXT("teleport rejected"));return;}
            UE_LOG(LogTemp,Display,TEXT("VF_RENDER_TARGET %s %s"),*VisualProbeLabel,*VisualProbeTarget.ToCompactString());
            break;
        }
        case 1:
        {
            if(!bUndergroundView || G->LayerAt(Pawn->GetActorLocation())==0 || !PC->GetViewTarget() || PC->GetViewTarget()==Pawn)
            {Fail(TEXT("underground/camera not ready"));return;}
            // Camera centred on actual shaft/room boundary, not a menu or an artificial test map.
            FreezePawn(true);
            auto* Camera=PC->GetViewTarget();
            Camera->SetActorTickEnabled(false);
            Camera->SetActorLocationAndRotation(VisualProbeFocus+FVector(0,-1840,0),FRotator(0,90,0));
            float W=0,H=0;FVector P;FRotator R;
            if(GetView(P,R,W,H))
                G->UpdateGameplayView(FTransform(FRotator(0,90,0),VisualProbeFocus+FVector(0,-1840,0)),W,W/FMath::Max(H,1.f));
            SetPreset(1,false,true);
            break;
        }
        case 2:
        {
            float H=0;if(!GetView(VisualProbeCamera,VisualProbeRotation,VisualProbeWidth,H)){Fail(TEXT("camera not cached"));return;}
            // Recompute culling for the actual settled, locked view (not the old follow target).
            G->UpdateGameplayView(FTransform(VisualProbeRotation,VisualProbeCamera),VisualProbeWidth,VisualProbeWidth/FMath::Max(H,1.f));
            VisualProbePawn=Pawn->GetActorLocation();
            if(!Capture(TEXT("Legacy")))return;
            break;
        }
        case 3: SetPreset(1);break;
        case 4: if(!Capture(TEXT("Atmospheric")))return;break;
        case 5: SetPreset(1,true);break;
        case 6: if(!Capture(TEXT("Masks")))return;break;
        case 7:
            if(bIsolation)
            {
                SetPreset(1);G->SetBackgroundAuditIsolation(1);Advance(30);return;
            }
            if(bEnclosed || (VisualProbeScene!=0 && VisualProbeScene!=2)) {NextScene();return;}
            SetPreset(0);break;
        case 8: if(!Capture(TEXT("Dark")))return;break;
        case 9: SetPreset(2);break;
        case 10: if(!Capture(TEXT("Warm")))return;break;
        case 11:
        {
            if(VisualProbeScene==2)
            {
                SetPreset(1);
                PC->ConsoleCommand(TEXT("t.MaxFPS 0"));
                Advance(20);return;
            }
            SetPreset(0);
            auto* TorchClass=LoadClass<ASirrohTorch>(nullptr,TEXT("/Game/POIKit/CarryProps/BP_AST_Torch.BP_AST_Torch_C"));
            if(!TorchClass){Fail(TEXT("torch class unavailable"));return;}
            FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            const FVector Loc=Pawn->GetActorLocation()+FVector(130,-30,-20);
            auto* Torch=GetWorld()->SpawnActor<ASirrohTorch>(TorchClass,Loc,FRotator::ZeroRotator,Params);
            if(!Torch || !Torch->Fire || !Torch->TorchMesh){Fail(TEXT("torch unavailable"));return;}
            Torch->TorchMesh->SetSimulatePhysics(false);Torch->TorchMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Torch->SetActorLocation(Loc,false,nullptr,ETeleportType::TeleportPhysics);
            Torch->Fire->Ignite(1.f);VisualProbeTorch=Torch;
            break;
        }
        case 12:
        {
            auto* Torch=Cast<ASirrohTorch>(VisualProbeTorch.Get());
            if(!Torch){Fail(TEXT("torch missing"));return;}
            TArray<UPointLightComponent*> Lights;Torch->GetComponents<UPointLightComponent>(Lights);
            bool bLit=false;
            for(auto* L:Lights)if(L && L->IsVisible() && L->Intensity>0)
            {
                bLit=true;UE_LOG(LogTemp,Display,TEXT("VF_RENDER_TORCH intensity=%.3f radius=%.3f shadows=%d"),L->Intensity,L->AttenuationRadius,L->CastShadows);
            }
            if(!bLit){Fail(TEXT("torch no light"));return;}
            Torch->SetActorTickEnabled(false);
            for(UActorComponent* Component:Torch->GetComponents())if(Component)Component->SetComponentTickEnabled(false);
            break;
        }
        case 13: if(!Capture(TEXT("TorchDark")))return;break;
        case 14: SetPreset(1);break;
        case 15: if(!Capture(TEXT("TorchAtmospheric")))return;break;
        case 16:
            if(auto* Torch=VisualProbeTorch.Get())Torch->Destroy();
            VisualProbeTorch.Reset(); SetPreset(1);
            PC->ConsoleCommand(TEXT("r.VSync 0"));PC->ConsoleCommand(TEXT("t.MaxFPS 0"));
            Advance(20);return;
        case 24:
        {
            if(VisualProbeScene!=0){NextScene();return;}
            // Exercise the existing normal lane move producer, not a replacement collision path.
            FreezePawn(false);EPWGTerrainLane TargetLane;FVector Destination;
            if(Input && G->FindLaneSwitchDestination(Pawn->GetActorLocation(),-1,TargetLane,Destination))
            {
                VisualProbeLaneY=Destination.Y;Input->BeginLaneMove(Destination.Y);
                UE_LOG(LogTemp,Display,TEXT("VF_LANE_REQUEST targetY=%.2f"),VisualProbeLaneY);
            }
            else {UE_LOG(LogTemp,Error,TEXT("VF_LANE_UNAVAILABLE"));NextScene();return;}
            break;
        }
        case 30:
            if(!Capture(TEXT("NoNativeCards")))return;
            G->SetBackgroundAuditIsolation(2);break;
        case 31:
            if(!Capture(TEXT("NoProjectedArt")))return;
            G->SetBackgroundAuditIsolation(3);break;
        case 32:
            if(!Capture(TEXT("NoShaftWalls")))return;
            G->SetBackgroundAuditIsolation(4);break;
        case 33:
            if(!Capture(TEXT("NoCaveBackdrop")))return;
            G->SetBackgroundAuditIsolation(5);break;
        case 34:
            if(!Capture(TEXT("NativeOnly")))return;
            NextScene();return;
        case 25:
            UE_LOG(LogTemp,Display,TEXT("VF_LANE_RESULT targetY=%.2f actualY=%.2f active=%d reached=%d"),
                VisualProbeLaneY,Pawn->GetActorLocation().Y,Input?Input->IsLaneMoveActive():false,FMath::Abs(VisualProbeLaneY-Pawn->GetActorLocation().Y)<5.f);
            if(!Capture(TEXT("LaneNear")))return;
            NextScene();return;
    }
    Advance(VisualProbeStep+1);
}
#endif
