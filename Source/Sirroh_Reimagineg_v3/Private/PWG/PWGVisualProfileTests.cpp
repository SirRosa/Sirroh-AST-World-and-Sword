#include "PWG/PWGEnvironmentV2.h"
#include "Procedural_World_Generator/PWGv2Generator.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWGVisualProfileTest,"Sirroh.PWG.VisualProfile",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

bool FPWGVisualProfileTest::RunTest(const FString&)
{
    FPWGVisualProfile Profile;
    TestTrue(TEXT("Stylized default"),Profile.ResolveMode(true,true)==EPWGVisualMode::Underground_Stylized);
    Profile.Mode=EPWGVisualMode::Legacy;
    TestTrue(TEXT("Explicit Legacy"),Profile.ResolveMode(true,true)==EPWGVisualMode::Legacy);
    Profile.Mode=EPWGVisualMode::Underground_Stylized;
    TestTrue(TEXT("Underground V2 available"),Profile.ResolveMode(true,true)==EPWGVisualMode::Underground_Stylized);
    TestTrue(TEXT("Surface exact Legacy"),Profile.ResolveMode(false,true)==EPWGVisualMode::Legacy);
    TestTrue(TEXT("Missing asset falls back"),Profile.ResolveMode(true,false)==EPWGVisualMode::Legacy);
    TestTrue(TEXT("All lane banks covered at spacing 100"),Profile.ProtectedHalfWidth(100.f)>=150.f);
    TestTrue(TEXT("All lane banks covered at spacing 300"),Profile.ProtectedHalfWidth(300.f)>=350.f);
    Profile.Mode=static_cast<EPWGVisualMode>(255);
    Profile.Strength=std::numeric_limits<float>::quiet_NaN();
    Profile.FrontRange=-1.f;
    Profile.FogStrength=10.f;
    Profile.NeutralPadding=-50.f;
    const FPWGVisualProfile Safe=Profile.Sanitized();
    TestTrue(TEXT("Invalid mode falls back"),Safe.ResolveMode(true,true)==EPWGVisualMode::Legacy);
    TestEqual(TEXT("NaN strength reset"),Safe.Strength,1.f);
    TestEqual(TEXT("Front range clamped"),Safe.FrontRange,20.f);
    TestEqual(TEXT("Fog clamped"),Safe.FogStrength,1.f);
    TestEqual(TEXT("Minimum neutral protection"),Safe.ProtectedHalfWidth(100.f),150.f);
    TestEqual(TEXT("Rear cave section"),APWGv2Generator::VisualRoleForSection(5),APWGv2Generator::VisualRear);
    TestEqual(TEXT("Shaft rear section"),APWGv2Generator::VisualRoleForSection(9),APWGv2Generator::VisualRear);
    TestEqual(TEXT("Authored rear section"),APWGv2Generator::VisualRoleForSection(33),APWGv2Generator::VisualRear);
    TestEqual(TEXT("Cutaway"),APWGv2Generator::VisualRoleForSection(29),APWGv2Generator::VisualCutaway);
    TestEqual(TEXT("Gameplay floor separate"),APWGv2Generator::VisualRoleForSection(1),APWGv2Generator::VisualGameplay);
    TestEqual(TEXT("Far solid earth backing"),APWGv2Generator::VisualRoleForSection(25),APWGv2Generator::VisualRear);
    TestEqual(TEXT("Shaft front"),APWGv2Generator::VisualRoleForSection(21),APWGv2Generator::VisualCutaway);
    FPWGVisualProfile Depth;
    TestTrue(TEXT("Stage 2 default"),Depth.bEnableDepthGrading && Depth.bEnableDepthLayers && !Depth.bCompareStage1);
    FPWGVisualProfile Compare=Depth; Compare.bCompareStage1=true;
    TestFalse(TEXT("A/B invalidates uniform cache"),Depth.SameShaderValues(Compare));
    Depth.DepthRange=0.f; Depth.DepthBrightness=99.f; Depth.DepthFormationCount=99;
    const auto SafeDepth=Depth.Sanitized();
    TestEqual(TEXT("Depth range clamped"),SafeDepth.DepthRange,20.f);
    TestEqual(TEXT("Depth brightness clamped"),SafeDepth.DepthBrightness,3.f);
    TestEqual(TEXT("Depth count clamped"),SafeDepth.DepthFormationCount,2);
    const auto Dark=FPWGVisualProfile::DemoPreset(0).Sanitized();
    const auto Atmospheric=FPWGVisualProfile::DemoPreset(1).Sanitized();
    const auto Warm=FPWGVisualProfile::DemoPreset(2).Sanitized();
    TestTrue(TEXT("Dark no artistic ambient"),Dark.bDisableBackgroundAmbient && Dark.BackgroundAmbient==0.f);
    TestEqual(TEXT("Dark physical fill off"),Dark.PixelAmbientLux,0.f);
    TestFalse(TEXT("Raw is opt-in, not a game preset"),Atmospheric.bPixelRawPalette);
    FPWGVisualProfile Pixel=Atmospheric;
    Pixel.bPixelRawPalette=true;Pixel.PixelAmbientLux=128.f;
    TestTrue(TEXT("Raw survives sanitization"),Pixel.Sanitized().bPixelRawPalette);
    TestEqual(TEXT("Physical light audit can exceed previous 8 lux cap"),Pixel.Sanitized().PixelAmbientLux,128.f);
    Pixel.PixelAmbientLux=999.f;
    TestEqual(TEXT("Physical fill upper audit bound"),Pixel.Sanitized().PixelAmbientLux,512.f);
    TestTrue(TEXT("Dark no depth haze/lift"),Dark.DepthFog==0.f && Dark.DepthBrightness==1.f && Dark.DepthContrast==1.f);
    TestTrue(TEXT("Atmospheric purple"),Atmospheric.AtmosphereColor.B>Atmospheric.AtmosphereColor.G);
    TestTrue(TEXT("Warm red"),Warm.AtmosphereColor.R>Warm.AtmosphereColor.B);
    TestTrue(TEXT("Preset gameplay grades distinct"),!Dark.GameplayTint.Equals(Warm.GameplayTint));
    return true;
}
#endif
