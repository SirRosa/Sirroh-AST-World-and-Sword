#include "PWG/PWGEnvironmentV2.h"

#if !UE_BUILD_SHIPPING
#include "HighResScreenshot.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "MoverComponent.h"
#include "Combat/CombatVitalityComponent.h"
#include "Misc/CommandLine.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

void APWGEnvironmentV2::VerifySkySequence()
{
    static const bool bRequested = FParse::Param(FCommandLine::Get(), TEXT("PWGSkyQA"));
    if (!bRequested || !DynamicSky || !DynamicWeather) return;

    const int32 Frame = ++SkyVerificationFrame;
    if (Frame % 30 == 0)
    {
        FVector Location;
        FRotator Rotation;
        float Width, Height;
        if (GetView(Location, Rotation, Width, Height))
            UE_LOG(LogTemp, Display, TEXT("PWGSkyQA frame=%d camera=%s width=%.1f"), Frame, *Location.ToCompactString(), Width);
        auto* Controller = GetWorld()->GetFirstPlayerController();
        APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
        auto* Mesh = Pawn ? Pawn->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
        auto* Mover = Pawn ? Pawn->FindComponentByClass<UMoverComponent>() : nullptr;
        if (Mover) UE_LOG(LogTemp, Display, TEXT("PWGSkyQA mode=%s plane=%d"),
            *Mover->GetMovementModeName().ToString(), Mover->GetPlanarConstraint().bConstrainToPlane);
        if (Pawn) UE_LOG(LogTemp, Display, TEXT("PWGSkyQA target=%s velocity=%s mesh=%s"),
            *Pawn->GetActorLocation().ToCompactString(), *Pawn->GetVelocity().ToCompactString(),
            Mesh ? *Mesh->GetComponentLocation().ToCompactString() : TEXT("none"));
    }
    if (Frame == 1400 && FParse::Param(FCommandLine::Get(), TEXT("PWGSkyQARecovery")))
    {
        auto* Controller = GetWorld()->GetFirstPlayerController();
        APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
        if (auto* Trigger = Pawn ? Pawn->FindFunction(TEXT("TriggerRagdoll")) : nullptr)
        {
            FStructOnScope Args(Trigger);
            if (auto* Stop = FindFProperty<FBoolProperty>(Trigger, TEXT("StopActiveMontages")))
                Stop->SetPropertyValue_InContainer(Args.GetStructMemory(), true);
            Pawn->ProcessEvent(Trigger, Args.GetStructMemory());
            UE_LOG(LogTemp, Display, TEXT("PWGSkyQA triggered ragdoll"));
        }
    }
    if (Frame == 1700 && FParse::Param(FCommandLine::Get(), TEXT("PWGSkyQARecovery")))
    {
        auto* Controller = GetWorld()->GetFirstPlayerController();
        APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
        if (auto* Health = Pawn ? Pawn->FindComponentByClass<UCombatVitalityComponent>() : nullptr)
        {
            Health->ResetVitality();
            if (auto* Exit = Pawn->FindFunction(TEXT("ExitRagdoll")); Exit && Exit->ParmsSize == 0)
                Pawn->ProcessEvent(Exit, nullptr);
            UE_LOG(LogTemp, Display, TEXT("PWGSkyQA requested ragdoll recovery"));
        }
    }
    if (Frame == 60 || Frame == 210 || Frame == 360)
    {
        const TCHAR* Preset = Frame == 360 ? TEXT("Rain_Thunderstorm") : TEXT("Clear_Skies");
        const FString Path = FString::Printf(TEXT("/Game/UltraDynamicSky/Blueprints/Weather_Effects/Weather_Presets/%s.%s"), Preset, Preset);
        UFunction* Change = DynamicWeather->FindFunction(TEXT("Change Weather"));
        auto* WeatherType = Change ? FindFProperty<FObjectPropertyBase>(Change, TEXT("New Weather Type")) : nullptr;
        UObject* Settings = LoadObject<UObject>(nullptr, *Path);
        if (WeatherType && Settings && Settings->IsA(WeatherType->PropertyClass))
        {
            FStructOnScope Args(Change);
            WeatherType->SetObjectPropertyValue_InContainer(Args.GetStructMemory(), Settings);
            DynamicWeather->ProcessEvent(Change, Args.GetStructMemory());
        }
        if (auto* Time = CastField<FNumericProperty>(SkyTime))
            Time->SetFloatingPointPropertyValue(Time->ContainerPtrToValuePtr<void>(DynamicSky.Get()), Frame == 210 ? 100.0 : 1050.0);
    }
    // Exercise the same environment switch called by the generator at a cave entrance.
    if (Frame >= 550 && Frame <= 590) SetUndergroundView(true);
    if (Frame == 591) SetUndergroundView(false);

    const TCHAR* Capture = nullptr;
    switch (Frame)
    {
        case 190: Capture = TEXT("PWG_Sky_ClearDay"); break;
        case 340: Capture = TEXT("PWG_Sky_ClearNight"); break;
        case 490: Capture = TEXT("PWG_Sky_Storm"); break;
        case 585: Capture = TEXT("PWG_Sky_UndergroundSwitch"); break;
        case 650: Capture = TEXT("PWG_Sky_SurfaceReturn"); break;
        case 2200:
            UE_LOG(LogTemp, Display, TEXT("PWGSkyQA completed 2200 frames"));
            FPlatformMisc::RequestExit(false);
            return;
        default: return;
    }
    UE_LOG(LogTemp, Display, TEXT("PWGSkyQA %s coverage=%.3f underground=%d"), Capture, WeatherCloudAmount, bUndergroundView);
    FScreenshotRequest::RequestScreenshot(Capture, false, false);
}
#endif
