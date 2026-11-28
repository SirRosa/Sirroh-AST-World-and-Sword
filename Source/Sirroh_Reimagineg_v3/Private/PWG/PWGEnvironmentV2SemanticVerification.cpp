#include "PWG/PWGEnvironmentV2.h"
#if !UE_BUILD_SHIPPING
#include "Procedural_World_Generator/PWGv2Generator.h"
#include "PWG/PWGPlayerController.h"
#include "PWG/PWGSidescrollerInputComponent.h"
#include "Items/SirrohCarryProps.h"
#include "ASTFireReactiveComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/LightComponentBase.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Camera/PlayerCameraManager.h"
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
    static const bool bPixelVisibilityQA=FParse::Param(FCommandLine::Get(),TEXT("PWGPixelVisibilityQA"));
    static const bool bPixelReadabilityQA=FParse::Param(FCommandLine::Get(),TEXT("PWGPixelReadabilityQA"));
    static const bool bPixelReadabilityAfterOnly=bPixelReadabilityQA && FParse::Param(FCommandLine::Get(),TEXT("PWGPixelReadabilityAfterOnly"));
    static const bool bPixelQA=bPixelReadabilityQA || bPixelVisibilityQA || FParse::Param(FCommandLine::Get(),TEXT("PWGPixel2DQA"));
    static const bool bDepthQA=FParse::Param(FCommandLine::Get(),TEXT("PWGDepthLayersQA"));
    static const bool bCaveQA=FParse::Param(FCommandLine::Get(),TEXT("PWGCaveReferenceQA"));
    if (!bRequested || VisualProbeStep<0 || !GetWorld()->IsGameWorld()) return;
    static const bool bGeoQA=bCaveQA && FParse::Param(FCommandLine::Get(),TEXT("PWGGeologicalArtQA"));
    if(bGeoQA && VisualProbeStep==0 && VisualProbeFrames==0)VisualProbeScene=7;
    auto* G=Cast<APWGv2Generator>(Generator);
    auto* PC=GetWorld()->GetFirstPlayerController();
    APawn* Pawn=PC?PC->GetPawn():nullptr;
    if (!G || !Pawn || !PC) return;
    auto Fail=[this](const TCHAR* Why)
    {
        UE_LOG(LogTemp,Error,TEXT("VF_RENDER_FAIL %s"),Why);
        VisualProbeStep=-1; FPlatformMisc::RequestExit(false);
    };
    if(G->WorldName!=(bPixelReadabilityQA?TEXT("VF_PixelReadability_20261128"):bPixelVisibilityQA?TEXT("VF_PixelVisibility_20261128"):bPixelQA?TEXT("VF_Pixel2D_20261126"):bCaveQA?TEXT("VF_CaveReference_20261126"):bDepthQA?TEXT("VF_DepthProbe_20261126"):TEXT("VF_POIProbe_20261126"))) {Fail(TEXT("wrong sandbox slot"));return;}
    auto* Mover=Pawn->FindComponentByClass<UMoverComponent>();
    auto* Input=Pawn->FindComponentByClass<UPWGSidescrollerInputComponent>();
    auto FreezePawn=[&](bool bFreeze)
    {
        Pawn->SetActorTickEnabled(!bFreeze);
        if(Mover) Mover->SetComponentTickEnabled(!bFreeze);
        if(Input) Input->SetComponentTickEnabled(!bFreeze);
    };
    auto SetPreset=[this](int32 Preset,bool bMasks=false,bool bLegacy=false,bool bStage1=false)
    {
        bPixelVisibilityReverseFill=false;bPixelVisibilityManualExposure=false;
        VisualProfile=FPWGVisualProfile::DemoPreset(bCaveQA && Preset==1?3:Preset);
        VisualProfile.bUseReliefCheckpoint=bDepthQA && !bCaveQA;
        if(bCaveQA){VisualProfile.bCaveReference=true; VisualProfile.bCaveGeological=bGeoQA; if(bGeoQA)VisualProfile.CaveVisualDepthMetres=30.f; else if(VisualProbeScene==8)VisualProfile.CaveVisualDepthMetres=35.f;}
        VisualProfile.BlendSeconds=0.f; VisualProfile.bVisualizeMasks=bMasks && !bDepthQA;
        VisualProfile.bVisualizeDepthLayers=bMasks && bDepthQA;
        VisualProfile.bCompareStage1=bPixelQA?bStage1:(!bDepthQA || bStage1);
        if(bPixelQA)
        {
            VisualProfile.bProceduralPixel=true;VisualProfile.bUseReliefCheckpoint=false;VisualProfile.bCaveReference=false;
            VisualProfile.bVisualizeDepthLayers=bMasks;VisualProfile.bVisualizeMasks=false;
        }
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
        if(bPixelReadabilityQA)
            VisualProbeScene=VisualProbeScene==0?1:VisualProbeScene==1?8:9;
        else if(bPixelVisibilityQA)
            VisualProbeScene=VisualProbeScene==0?8:9;
        else if(bPixelQA)
            VisualProbeScene=VisualProbeScene==0?1:VisualProbeScene==1?2:VisualProbeScene==2?8:9;
        else if(bCaveQA)
        {
            VisualProfile.bCaveReference=false; RefreshVisualProfile();
            G->ResetCaveReference();
            // Existing tunnel, shaft, POI, larger natural chamber, deeper cave.
            VisualProbeScene=bGeoQA?9:VisualProbeScene==0?1:VisualProbeScene==1?2:VisualProbeScene==2?7:VisualProbeScene==7?8:9;
        }
        else ++VisualProbeScene;
        if((bPixelQA && VisualProbeScene>8) || (!bPixelQA && ((bCaveQA && VisualProbeScene>8) || (!bCaveQA && VisualProbeScene>(bIsolation||bEnclosed?3:6)))))
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
            bVisualProbeDisableMaskPass=!bDepthQA && (VisualProbeStep%2)==0;
            if(bDepthQA) SetPreset(1,false,false,(VisualProbeStep%2)==0);
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
            if(bDepthQA)
                UE_LOG(LogTemp,Display,TEXT("VF_DEPTH_GPU scene=%s stage2=%d run=%d samples=%d gpu_median_ms=%.3f gpu_p95_ms=%.3f wall_median_ms=%.3f wall_p95_ms=%.3f"),
                    *VisualProbeLabel,!VisualProfile.bCompareStage1,VisualProbeStep-20,VisualProbeGPU.Num(),GPU50,GPU95,CPU50,CPU95);
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
        if(bPixelQA && (!bHasPixelBackgroundAsset || !G->IsPixelBackgroundActive())){Fail(TEXT("global Pixel2D not active"));return false;}
        if(bPixelReadabilityQA && (!G->HasPixelReadabilityMaterial() || G->IsPixelReadabilityActive()!=VisualProfile.bPixelReadability))
        {Fail(TEXT("Readability A/B material mismatch"));return false;}
        if((bPixelReadabilityQA || bPixelVisibilityQA) && G->IsPixelRawPaletteActive()!=VisualProfile.bPixelRawPalette){Fail(TEXT("Raw Palette toggle mismatch"));return false;}
        if(bDepthQA && !bHasDepthPrototypeAsset){Fail(TEXT("Depth prototype asset not loaded"));return false;}
        if(bCaveQA && (!bHasIndependentCaveAsset || G->IsCaveReferenceActive()==VisualProfile.bCompareStage1)){Fail(TEXT("Cave reference A/B state mismatch"));return false;}
        if(bDepthQA && !bCaveQA && G->IsDepthPrototypeActive()==VisualProfile.bCompareStage1){Fail(TEXT("Depth prototype A/B state mismatch"));return false;}
        const FString Name=VisualProbeLabel+TEXT("_")+Suffix;
        const FString Folder=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/(bPixelReadabilityQA?(bPixelReadabilityAfterOnly?TEXT("ChatGPT/PixelReadability_20261128/RenderTuned"):TEXT("ChatGPT/PixelReadability_20261128/RenderVerified")):bPixelVisibilityQA?TEXT("ChatGPT/PixelVisibility_20261128/RenderCorrected"):bPixelQA?TEXT("ChatGPT/ProceduralPixel2D_20261126/Render"):bCaveQA?(bGeoQA?TEXT("ChatGPT/GeologicalCave_20261126/Render"):TEXT("ChatGPT/StratifiedCave_20261126/RenderTexturedAmbient")):bDepthQA?(bEnclosed?TEXT("ChatGPT/DepthLayersPrototype_20261126/Enclosed"):TEXT("ChatGPT/DepthLayersPrototype_20261126/Render")):bIsolation?TEXT("ChatGPT/UnifiedBackground_20261126/Audit"):bEnclosed?TEXT("ChatGPT/UnifiedBackground_20261126/Enclosed"):TEXT("ChatGPT/UnifiedBackground_20261126/Render")));
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
        if(bPixelReadabilityQA || bPixelVisibilityQA)
        {
            const auto* Fill=CameraFillLight.Get();
            if(Fill) UE_LOG(LogTemp,Display,TEXT("PIXEL_FILL capture=%s lux=%.3f visible=%d forward=%s channels=%d%d%d raw=%d strength=%.2f"),*Name,Fill->Intensity,Fill->IsVisible(),*Fill->GetForwardVector().ToCompactString(),Fill->LightingChannels.bChannel0,Fill->LightingChannels.bChannel1,Fill->LightingChannels.bChannel2,G->IsPixelRawPaletteActive(),VisualProfile.Strength);
            if(PC->PlayerCameraManager)
            {
                const auto& PP=PC->PlayerCameraManager->GetCameraCacheView().PostProcessSettings;
                UE_LOG(LogTemp,Display,TEXT("PIXEL_CAMERA_PP capture=%s method=%d bias=%.3f min=%.3f max=%.3f manual_raw_override=%d"),*Name,int32(PP.AutoExposureMethod),PP.AutoExposureBias,PP.AutoExposureMinBrightness,PP.AutoExposureMaxBrightness,DepthPostProcess->Settings.bOverride_AutoExposureMethod);
            }
            if(bPixelReadabilityQA || VisualProfile.bPixelRawPalette || FCString::Strcmp(Suffix,TEXT("Atmospheric"))==0)
            {
                G->LogPixelVisibilityAudit(VisualProbeFocus);
                G->DebugVisualCoverage(FTransform(R,P),W,H);
            }
        }
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
            if(bPixelQA)
            {
                // In the disposable QA world only, isolate the existing physical
                // fill from unrelated scene lights. The actual torch is spawned later.
                for(TActorIterator<AActor> It(GetWorld());It;++It)
                    if(*It!=this)
                    {
                        TArray<ULightComponentBase*> Lights;It->GetComponents<ULightComponentBase>(Lights);
                        for(auto* L:Lights)if(L)L->SetVisibility(false,true);
                    }
                SetPreset(1,false,false,true);
            }
            else SetPreset(1,false,!bDepthQA,bDepthQA);
            break;
        }
        case 2:
        {
            float H=0;if(!GetView(VisualProbeCamera,VisualProbeRotation,VisualProbeWidth,H)){Fail(TEXT("camera not cached"));return;}
            // Recompute culling for the actual settled, locked view (not the old follow target).
            G->UpdateGameplayView(FTransform(VisualProbeRotation,VisualProbeCamera),VisualProbeWidth,VisualProbeWidth/FMath::Max(H,1.f));
            VisualProbePawn=Pawn->GetActorLocation();
            if(bPixelQA)
            {
                if(G->IsPixelBackgroundActive()){Fail(TEXT("Stage 1 toggle failed"));return;}
                SetPreset(1);
                if(bPixelReadabilityQA)
                {
                    if(bPixelReadabilityAfterOnly){Advance(81);return;}
                    VisualProfile.bPixelReadability=false;
                    RefreshVisualProfile();
                    Advance(80);return;
                }
                if(bPixelVisibilityQA)
                {
                    VisualProfile.bPixelRawPalette=true;
                    RefreshVisualProfile();
                    Advance(71);return;
                }
                Advance(4);return;
            }
            if(!Capture(bDepthQA?TEXT("Stage1"):TEXT("Legacy")))return;
            break;
        }
        case 3: SetPreset(1);break;
        case 4:
            if(bPixelQA)
            {
                if(!Capture(TEXT("Composition")))return;
                Advance(VisualProbeScene==0?7:70);return;
            }
            if(!Capture(bCaveQA?TEXT("Composition"):bDepthQA?TEXT("Stage2"):TEXT("Atmospheric")))return;break;
        case 5: SetPreset(1,true);break;
        case 6: if(!Capture(bDepthQA?TEXT("DepthMasks"):TEXT("Masks")))return;break;
        case 7:
            if(bPixelQA){SetPreset(0);break;}
            if(bGeoQA){SetPreset(1);VisualProfile.CaveTextureAmbient=0.f;RefreshVisualProfile();Advance(60);return;}
            if(bCaveQA){SetPreset(1);VisualProfile.bShowCaveMid=false;VisualProfile.bShowCaveFar=false;VisualProfile.bShowCaveClosure=false;RefreshVisualProfile();Advance(40);return;}
            if(bIsolation)
            {
                SetPreset(1);G->SetBackgroundAuditIsolation(1);Advance(30);return;
            }
            if(bEnclosed || (VisualProbeScene!=0 && VisualProbeScene!=2)) {NextScene();return;}
            SetPreset(0);break;
        case 8: if(!Capture(TEXT("Dark")))return;if(bPixelQA){Advance(11);return;}break;
        case 9: SetPreset(2);break;
        case 10: if(!Capture(TEXT("Warm")))return;break;
        case 11:
        {
            if(!bCaveQA && VisualProbeScene==2)
            {
                SetPreset(1);
                PC->ConsoleCommand(TEXT("t.MaxFPS 0"));
                Advance(20);return;
            }
            SetPreset(0);
            auto* TorchClass=LoadClass<ASirrohTorch>(nullptr,TEXT("/Game/POIKit/CarryProps/BP_AST_Torch.BP_AST_Torch_C"));
            if(!TorchClass){Fail(TEXT("torch class unavailable"));return;}
            FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            const FVector Loc=Pawn->GetActorLocation()+(bCaveQA?FVector(-350,180,40):FVector(130,-30,-20));
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
        case 13: if(!Capture(TEXT("TorchDark")))return;if(bPixelQA){Advance(70);return;}break;
        case 70: NextScene();return;
        // FScreenshotRequest is consumed at end of frame. Never alter the
        // profile in the same frame as the request; capture and switch in
        // distinct settled steps.
        case 80:
            if(!Capture(TEXT("Before")))return;
            Advance(84);return;
        case 84:
            VisualProfile.bPixelReadability=true;RefreshVisualProfile();Advance(81);return;
        case 81:
            if(!Capture(TEXT("After")))return;
            Advance(VisualProbeScene==0?85:70);return;
        case 85:
            VisualProfile.bPixelRawPalette=true;RefreshVisualProfile();Advance(82);return;
        case 82:
            if(!Capture(TEXT("RawPalette")))return;
            Advance(bPixelReadabilityAfterOnly?70:86);return;
        case 86:
            VisualProfile.bPixelRawPalette=false;VisualProfile.bVisualizeDepthLayers=true;RefreshVisualProfile();Advance(83);return;
        case 83:
            if(!Capture(TEXT("Plans")))return;
            Advance(87);return;
        case 87:
            SetPreset(0);Advance(8);return;
        // Exact same camera/seed/masks for raw, ambient sweep and dark/torch.
        case 71:
            if(!Capture(TEXT("RawPalette")))return;
            if(VisualProbeScene==8){Advance(70);return;}
            SetPreset(1);Advance(72);return;
        case 72:
            if(!Capture(TEXT("Atmospheric")))return;
            VisualProfile.PixelAmbientLux=8.f;RefreshVisualProfile();Advance(73);return;
        case 73:
            if(!Capture(TEXT("Atmospheric_8lux")))return;
            VisualProfile.PixelAmbientLux=32.f;RefreshVisualProfile();Advance(74);return;
        case 74:
            if(!Capture(TEXT("Atmospheric_32lux")))return;
            VisualProfile.PixelAmbientLux=128.f;RefreshVisualProfile();Advance(75);return;
        case 75:
            if(!Capture(TEXT("Atmospheric_128lux")))return;
            VisualProfile.Strength=0.f;RefreshVisualProfile();Advance(76);return;
        case 76:
            if(!Capture(TEXT("Lit128_NoDepthPP")))return;
            bPixelVisibilityManualExposure=true;RefreshVisualProfile();Advance(78);return;
        case 77:
            if(!Capture(TEXT("Atmospheric_512lux")))return;
            SetPreset(0);Advance(8);return;
        case 78:
            if(!Capture(TEXT("Lit128_ManualExposure")))return;
            bPixelVisibilityReverseFill=true;RefreshVisualProfile();Advance(79);return;
        case 79:
            if(!Capture(TEXT("Lit128_ReverseFill")))return;
            SetPreset(1);VisualProfile.PixelAmbientLux=512.f;RefreshVisualProfile();Advance(77);return;
        case 14: if(bGeoQA){NextScene();return;} SetPreset(1);break;
        case 15: if(!Capture(TEXT("TorchAtmospheric")))return;break;
        case 16:
            if(bGeoQA){NextScene();return;}
            if(auto* Torch=VisualProbeTorch.Get())Torch->Destroy();
            VisualProbeTorch.Reset(); SetPreset(1);
            PC->ConsoleCommand(TEXT("r.VSync 0"));PC->ConsoleCommand(TEXT("t.MaxFPS 0"));
            Advance(20);return;
        case 24:
        {
            if(bCaveQA || VisualProbeScene!=0){NextScene();return;}
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
        // FScreenshotRequest is serviced later by the renderer. Keep the
        // selected layer state unchanged until the next QA step.
        case 30: if(!Capture(TEXT("NoNativeCards")))return;break;
        case 31: G->SetBackgroundAuditIsolation(2);break;
        case 32: if(!Capture(TEXT("NoProjectedArt")))return;break;
        case 33: G->SetBackgroundAuditIsolation(3);break;
        case 34: if(!Capture(TEXT("NoShaftWalls")))return;break;
        case 35: G->SetBackgroundAuditIsolation(4);break;
        case 36: if(!Capture(TEXT("NoCaveBackdrop")))return;break;
        case 37: G->SetBackgroundAuditIsolation(5);break;
        case 38: if(!Capture(TEXT("NativeOnly")))return;break;
        case 39: NextScene();return;
        case 40: if(!Capture(TEXT("NearOnly")))return;break;
        case 41: SetPreset(1);VisualProfile.bShowCaveNear=false;VisualProfile.bShowCaveFar=false;VisualProfile.bShowCaveClosure=false;RefreshVisualProfile();break;
        case 42: if(!Capture(TEXT("MidOnly")))return;break;
        case 43: SetPreset(1);VisualProfile.bShowCaveNear=false;VisualProfile.bShowCaveMid=false;VisualProfile.bShowCaveClosure=false;RefreshVisualProfile();break;
        case 44: if(!Capture(TEXT("FarOnly")))return;break;
        case 45: SetPreset(1);VisualProfile.bShowCaveNear=false;VisualProfile.bShowCaveMid=false;VisualProfile.bShowCaveFar=false;RefreshVisualProfile();break;
        case 46: if(!Capture(TEXT("ClosureOnly")))return;break;
        case 47: SetPreset(0);break;
        case 48: if(!Capture(TEXT("Dark")))return;break;
        case 49: SetPreset(1);VisualProfile.Strength=0.f;RefreshVisualProfile();break;
        case 50: if(!Capture(TEXT("RawLit")))return;break;
        case 51: SetPreset(1);VisualProfile.CaveMaterialDebug=1;RefreshVisualProfile();break;
        case 52: if(!Capture(TEXT("BaseColor")))return;break;
        case 53: SetPreset(1);VisualProfile.CaveMaterialDebug=2;RefreshVisualProfile();break;
        case 54: if(!Capture(TEXT("Normals")))return;break;
        case 55: SetPreset(1);VisualProfile.CaveMaterialDebug=3;RefreshVisualProfile();break;
        case 56: if(!Capture(TEXT("UV")))return;break;
        case 57:
            SetPreset(1);
            if(VisualProbeScene==0 || VisualProbeScene==7){Advance(11);return;}
            NextScene();return;
        // One locked chamber, no multi-scene or performance sweep.
        case 60: if(!Capture(TEXT("AmbientOff")))return;break;
        case 61: SetPreset(1);VisualProfile.Strength=0.f;RefreshVisualProfile();break;
        case 62: if(!Capture(TEXT("RawLit")))return;break;
        case 63: SetPreset(1);VisualProfile.CaveMaterialDebug=1;RefreshVisualProfile();break;
        case 64: if(!Capture(TEXT("BaseColor")))return;break;
        case 65: SetPreset(1);VisualProfile.CaveMaterialDebug=2;RefreshVisualProfile();break;
        case 66: if(!Capture(TEXT("Geometry")))return;break;
        case 67: SetPreset(0);break;
        case 68: if(!Capture(TEXT("Dark")))return;break;
        case 69: Advance(11);return;
        case 25:
            UE_LOG(LogTemp,Display,TEXT("VF_LANE_RESULT targetY=%.2f actualY=%.2f active=%d reached=%d"),
                VisualProbeLaneY,Pawn->GetActorLocation().Y,Input?Input->IsLaneMoveActive():false,FMath::Abs(VisualProbeLaneY-Pawn->GetActorLocation().Y)<5.f);
            if(!Capture(TEXT("LaneNear")))return;
            NextScene();return;
    }
    Advance(VisualProbeStep+1);
}
#endif
