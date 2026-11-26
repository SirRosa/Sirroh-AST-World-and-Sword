#include "PWGv2IndependentCave.h"
#include "PWG/PWGEnvironmentV2.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWGIndependentCaveTest,"Sirroh.PWG.IndependentCave",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPWGIndependentCaveTest::RunTest(const FString&)
{
    for(double Depth:{500.,2200.,5000.})
    {
        FPWGv2MeshBuffers A[4],B[4],C[4];
        PWGIndependentCave::Build(A,123,0,0,500,1600,250,900,5000,165,Depth);
        PWGIndependentCave::Build(B,123,0,0,500,1600,250,900,5000,165,Depth);
        PWGIndependentCave::Build(C,847,0,0,500,1600,250,900,5000,165,Depth);
        const double Min[4]={0,.23,.51,.80},Max[4]={.23,.51,.80,1.03};
        for(int32 I=0;I<4;++I)
        {
            TestTrue(TEXT("Nonempty depth plan"),!A[I].V.IsEmpty() && !A[I].T.IsEmpty());
            TestTrue(TEXT("Deterministic positions and topology"),A[I].V==B[I].V && A[I].T==B[I].T);
            if(I<3) TestTrue(TEXT("Seed affects rock shape"),A[I].V!=C[I].V);
            for(const FVector& V:A[I].V)
            {
                const double Q=(V.Y-165.)/Depth;
                if(Q<Min[I] || Q>=Max[I]){AddError(TEXT("Geometry crosses physical grading band"));break;}
            }
        }
    }
    FPWGVisualProfile P=FPWGVisualProfile::DemoPreset(3);
    TestTrue(TEXT("Demo opts into independent scene"),P.bCaveReference && !P.bUseReliefCheckpoint);
    TestTrue(TEXT("Texture detail attenuates"),P.CaveNear.TextureIntensity>P.CaveMid.TextureIntensity && P.CaveMid.TextureIntensity>P.CaveFar.TextureIntensity && P.CaveFar.TextureIntensity>P.CaveClosure.TextureIntensity);
    P.CaveVisualDepthMetres=100.f; P.CaveNear.TextureIntensity=-2.f;
    const auto Safe=P.Sanitized();
    TestEqual(TEXT("Depth clamped"),Safe.CaveVisualDepthMetres,50.f);
    TestEqual(TEXT("Texture intensity clamped"),Safe.CaveNear.TextureIntensity,0.f);
    return true;
}
#endif
