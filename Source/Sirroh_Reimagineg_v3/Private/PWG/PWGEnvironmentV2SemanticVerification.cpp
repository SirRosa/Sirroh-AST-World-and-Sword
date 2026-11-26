#include "PWG/PWGEnvironmentV2.h"
#if !UE_BUILD_SHIPPING
#include "Procedural_World_Generator/PWGv2Generator.h"
#include "PWG/PWGPlayerController.h"
#include "PWG/PWGSidescrollerInputComponent.h"
#include "Items/SirrohCarryProps.h"
#include "ASTFireReactiveComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
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

void APWGEnvironmentV2::VerifySemanticVisuals()
{
    static const bool bRequested=FParse::Param(FCommandLine::Get(),TEXT("PWGVisualSemanticQA"));
    if (!bRequested || VisualProbeStep<0 || !GetWorld()->IsGameWorld()) return;
    auto* G=Cast<APWGv2Generator>(Generator);
    auto* PC=GetWorld()->GetFirstPlayerController();
    APawn* Pawn=PC?PC->GetPawn():nullptr;
    if (!G || !Pawn || !PC) return;
    // Hard guard against touching the player's live world.
    if (G->WorldName!=TEXT("VF_SemanticProbe_20261126"))
    {
        UE_LOG(LogTemp,Error,TEXT("VF_RENDER_FAIL wrong world slot; aborting probe"));
        VisualProbeStep=-1;
        FPlatformMisc::RequestExit(false);
        return;
    }
    auto Fail=[this](const TCHAR* Why)
    {
        UE_LOG(LogTemp,Error,TEXT("VF_RENDER_FAIL %s"),Why);
        VisualProbeStep=-1;
        FPlatformMisc::RequestExit(false);
    };
    if (++VisualProbeFrames < (VisualProbeStep==0?120:VisualProbeStep==1?150:VisualProbeStep==6?120:45)) return;
    VisualProbeFrames=0;

    auto SetMode=[this](EPWGVisualMode Mode,bool bMask=false)
    {
        VisualProfile=FPWGVisualProfile();
        VisualProfile.Mode=Mode;
        VisualProfile.BlendSeconds=0.f;
        VisualProfile.bVisualizeMasks=bMask;
        RefreshVisualProfile();
    };
    auto Capture=[&](const TCHAR* Name)->bool
    {
        FVector P; FRotator R; float W=0,H=0;
        if (!GetView(P,R,W,H) || !P.Equals(VisualProbeCamera,0.1f) ||
            !R.Equals(VisualProbeRotation,0.01f) || !FMath::IsNearlyEqual(W,VisualProbeWidth,0.01f))
        { Fail(TEXT("camera drift")); return false; }
        if (VisualProfile.Mode==EPWGVisualMode::Underground_Stylized &&
            (!StylizedDepthMaterial || ActiveDepthMaterial!=StylizedDepthMaterial || ProfileMix<0.999f))
        { Fail(TEXT("semantic material not active")); return false; }
        const FString Folder=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("ChatGPT/VisualFoundation_Semantic_20261126/Render"));
        IFileManager::Get().MakeDirectory(*Folder,true);
        FString Lights;
        for (TActorIterator<AActor> It(GetWorld());It;++It)
        {
            TArray<UPointLightComponent*> Components;
            It->GetComponents<UPointLightComponent>(Components);
            for (auto* L:Components)
                if (L && L->IsVisible() && L->Intensity>0 && FVector::DistSquared(L->GetComponentLocation(),Pawn->GetActorLocation())<2000.0*2000.0)
                    Lights+=FString::Printf(TEXT(" %s=%.3f@%s"),*L->GetName(),L->Intensity,*L->GetComponentLocation().ToCompactString());
        }
        UE_LOG(LogTemp,Display,TEXT("VF_RENDER_CAPTURE %s camera=%s rotation=%s width=%.3f pawn=%s point_lights=%s"),
            Name,*P.ToCompactString(),*R.ToCompactString(),W,*Pawn->GetActorLocation().ToCompactString(),*Lights);
        FScreenshotRequest::RequestScreenshot(Folder/(FString(Name)+TEXT(".png")),false,false);
        return true;
    };
    switch (VisualProbeStep)
    {
        case 0:
        {
            if (!G->BeginDebugSession()) { Fail(TEXT("no sandbox world")); return; }
            FVector Target; FString Error;
            if (!G->DebugFindDestination(TEXT("Mountains"),TEXT("Shininglands"),1,Pawn,Target,Error)
                && !G->DebugFindDestination(NAME_None,NAME_None,1,Pawn,Target,Error))
            { Fail(*Error); return; }
            PC->SetIgnoreMoveInput(true); PC->SetIgnoreLookInput(true);
            if (auto* Input=Pawn->FindComponentByClass<UPWGSidescrollerInputComponent>()) Input->CancelLaneMove();
            if (auto* Mover=Pawn->FindComponentByClass<UMoverComponent>())
            {
                auto Constraint=Mover->GetPlanarConstraint();
                Constraint.bConstrainToPlane=true; Constraint.PlaneConstraintNormal=FVector::YAxisVector;
                Constraint.PlaneConstraintOrigin=Target; Mover->SetPlanarConstraint(Constraint);
                auto Teleport=MakeShared<FTeleportEffect>(); Teleport->TargetLocation=Target;
                Teleport->bUseActorRotation=true; Mover->QueueInstantMovementEffect(Teleport);
                auto Stop=MakeShared<FApplyVelocityEffect>(); Stop->VelocityToApply=FVector::ZeroVector;
                Stop->bAdditiveVelocity=false; Mover->QueueInstantMovementEffect(Stop);
            }
            else if (!Pawn->TeleportTo(Target,Pawn->GetActorRotation())) { Fail(TEXT("teleport failed")); return; }
            UE_LOG(LogTemp,Display,TEXT("VF_RENDER_TARGET %s"),*Target.ToCompactString());
            break;
        }
        case 1:
        {
            if (G->LayerAt(Pawn->GetActorLocation())!=1 || !bUndergroundView || !PC->GetViewTarget() || PC->GetViewTarget()==Pawn)
            { Fail(TEXT("underground / camera not ready")); return; }
            PC->GetViewTarget()->SetActorTickEnabled(false);
            SetMode(EPWGVisualMode::Legacy);
            break;
        }
        case 2:
        {
            float H=0;
            if (!GetView(VisualProbeCamera,VisualProbeRotation,VisualProbeWidth,H))
            { Fail(TEXT("no camera")); return; }
            if (!Capture(TEXT("01_legacy_dark"))) return;
            break;
        }
        case 3: SetMode(EPWGVisualMode::Underground_Stylized); break;
        case 4: if (!Capture(TEXT("02_semantic_dark"))) return; break;
        case 5: SetMode(EPWGVisualMode::Underground_Stylized,true); break;
        case 6:
        {
            if (!Capture(TEXT("03_masks_dark"))) return;
            break;
        }
        case 7:
        {
            // Extra no-ambient screenshot in the same scene.
            SetMode(EPWGVisualMode::Underground_Stylized);
            VisualProfile.bDisableBackgroundAmbient=true;
            RefreshVisualProfile();
            break;
        }
        case 8: if (!Capture(TEXT("04_no_ambient_dark"))) return; break;
        case 9:
        {
            SetMode(EPWGVisualMode::Legacy);
            UClass* TorchClass=LoadClass<ASirrohTorch>(nullptr,TEXT("/Game/POIKit/CarryProps/BP_AST_Torch.BP_AST_Torch_C"));
            if (!TorchClass) { Fail(TEXT("actual torch BP missing")); return; }
            FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            FVector Loc=Pawn->GetActorLocation()+FVector(200,-30,-20);
            auto* Torch=GetWorld()->SpawnActor<ASirrohTorch>(TorchClass,Loc,FRotator::ZeroRotator,Params);
            if (!Torch || !Torch->Fire || !Torch->TorchMesh) { Fail(TEXT("actual torch missing components")); return; }
            Torch->TorchMesh->SetSimulatePhysics(false);
            Torch->TorchMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Torch->SetActorLocation(Loc,false,nullptr,ETeleportType::TeleportPhysics);
            Torch->Fire->Ignite(1.f);
            VisualProbeTorch=Torch;
            break;
        }
        case 10:
        {
            auto* Torch=Cast<ASirrohTorch>(VisualProbeTorch.Get());
            if (!Torch) { Fail(TEXT("torch vanished")); return; }
            TArray<UPointLightComponent*> Lights; Torch->GetComponents<UPointLightComponent>(Lights);
            bool bLit=false;
            for (auto* L:Lights) if (L && L->IsVisible() && L->Intensity>0)
            {
                bLit=true;
                UE_LOG(LogTemp,Display,TEXT("VF_RENDER_TORCH_LIGHT intensity=%.3f radius=%.3f shadows=%d position=%s"),
                    L->Intensity,L->AttenuationRadius,L->CastShadows,*L->GetComponentLocation().ToCompactString());
            }
            if (!bLit) { Fail(TEXT("actual torch has no active point light")); return; }
            // Freeze fire envelope/flicker for the A/B, not for normal gameplay.
            Torch->SetActorTickEnabled(false);
            for (auto* Component:Torch->GetComponents()) if (Component) Component->SetComponentTickEnabled(false);
            break;
        }
        case 11: if (!Capture(TEXT("05_legacy_torch"))) return; break;
        case 12: SetMode(EPWGVisualMode::Underground_Stylized); break;
        case 13: if (!Capture(TEXT("06_semantic_torch"))) return; break;
        case 14:
            UE_LOG(LogTemp,Display,TEXT("VF_RENDER_COMPLETE fixed-camera capture sequence"));
            VisualProbeStep=-1; FPlatformMisc::RequestExit(false); return;
    }
    ++VisualProbeStep;
}
#endif
