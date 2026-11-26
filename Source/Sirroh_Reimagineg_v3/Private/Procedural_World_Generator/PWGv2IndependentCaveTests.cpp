#include "PWGv2IndependentCave.h"
#include "PWG/PWGEnvironmentV2.h"
#include "Procedural_World_Generator/PWGv2Generator.h"
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
    for(int32 K=0;K<6;++K)
    {
        FPWGv2MeshBuffers A,B;FRandomStream R(123),S(123);
        PWGIndependentCave::Formation(A,R,static_cast<PWGIndependentCave::EFamily>(K),FVector(0,0,0),150,60,500);
        PWGIndependentCave::Formation(B,S,static_cast<PWGIndependentCave::EFamily>(K),FVector(0,0,0),150,60,500);
        TestTrue(TEXT("Each family produces deterministic closed solids"),!A.T.IsEmpty() && A.V==B.V && A.T==B.T);
        TestTrue(TEXT("UV normals and material masks per vertex"),A.V.Num()==A.UV.Num() && A.V.Num()==A.N.Num() && A.V.Num()==A.C.Num());
        bool bPlane=false,bSlope=false;
        for(const FVector& N:A.N){bPlane|=FMath::Abs(N.Y)>.9;bSlope|=FMath::Abs(N.Y)<.7;}
        TestTrue(TEXT("Planar and bevel faces coexist"),bPlane && bSlope);
    }
    {
        FPWGv2MeshBuffers A[4],B[4]; TArray<PWGIndependentCave::FHybridInstance> X,Y;
        PWGIndependentCave::Build(A,123,0,0,800,1520,400,900,5000,165,2200,PWGIndependentCave::FRecipe(),&X);
        PWGIndependentCave::Build(B,123,0,0,800,1520,400,900,5000,165,2200,PWGIndependentCave::FRecipe(),&Y);
        TestTrue(TEXT("Hybrid transform recipe is deterministic"),!X.IsEmpty() && X==Y);
        const double Min[3]={0,.23,.51},Max[3]={.23,.51,.80};
        for(const auto& I:X)
        {
            TestTrue(TEXT("Instance recipe stays within depth plan"),(I.Centre.Y-I.Extent.Y-165.)/2200.>=Min[I.Plan] && (I.Centre.Y+I.Extent.Y-165.)/2200.<Max[I.Plan]);
        }
    }
    {
        TArray<FPWGDepthFormationProfile> Rows;
        FPWGDepthFormationProfile L;L.Layer=2;L.VisualDepthMetres=24;Rows.Add(L);
        L.Biome=TEXT("TestBiome");L.VisualDepthMetres=99;Rows.Add(L);
        TestEqual(TEXT("Layer default"),FPWGDepthFormationProfile::Resolve(Rows,2,NAME_None).VisualDepthMetres,24.f);
        TestEqual(TEXT("Biome overrides layer and clamps"),FPWGDepthFormationProfile::Resolve(Rows,2,TEXT("TestBiome")).VisualDepthMetres,50.f);
        TestTrue(TEXT("Deep and shallow profiles differ"),FPWGDepthFormationProfile::Resolve(Rows,4,NAME_None).VisualDepthMetres>FPWGDepthFormationProfile::Resolve(Rows,1,NAME_None).VisualDepthMetres);
        FPWGv2MeshBuffers A[4];PWGIndependentCave::FRecipe R;R.Families=16;R.PlanCount=1;R.Density=.1;
        PWGIndependentCave::Build(A,123,0,0,800,1520,400,900,5000,165,2200,R);
        TestTrue(TEXT("Profile plan and family restriction"),!A[0].V.IsEmpty() && A[1].V.IsEmpty() && A[2].V.IsEmpty() && !A[3].V.IsEmpty());
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
