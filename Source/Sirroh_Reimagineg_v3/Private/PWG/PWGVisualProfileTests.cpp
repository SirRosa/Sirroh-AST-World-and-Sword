#include "PWG/PWGEnvironmentV2.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWGVisualProfileTest,"Sirroh.PWG.VisualProfile",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

bool FPWGVisualProfileTest::RunTest(const FString&)
{
    FPWGVisualProfile Profile;
    TestTrue(TEXT("Legacy default"),Profile.ResolveMode(true,true)==EPWGVisualMode::Legacy);
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
    return true;
}
#endif
