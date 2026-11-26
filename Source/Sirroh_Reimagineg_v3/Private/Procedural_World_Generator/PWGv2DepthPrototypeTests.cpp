#include "PWGv2DepthPrototype.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWGDepthPrototypeTest,"Sirroh.PWG.DepthPrototype",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

bool FPWGDepthPrototypeTest::RunTest(const FString&)
{
    FProcMeshSection Src;
    Src.bEnableCollision=true;
    for (int32 X=-2000;X<2000;X+=100)
        for (int32 Z=-1000;Z<1000;Z+=100)
        {
            const int32 I=Src.ProcVertexBuffer.Num();
            for (const FVector P:{FVector(X,185,Z),FVector(X+100,185,Z),FVector(X+100,185,Z+100),FVector(X,185,Z+100)})
            {
                FProcMeshVertex V; V.Position=P; V.UV0=FVector2D(P.X/100,-P.Z/100);
                Src.ProcVertexBuffer.Add(V);
            }
            for (int32 Offset:{0,1,2,0,2,3}) Src.ProcIndexBuffer.Add(I+Offset);
        }
    const double Span=290.;
    const auto Back=PWGDepthPrototype::BuildBacking(Src,Span);
    TestFalse(TEXT("Backing no collision"),Back.bEnableCollision);
    TestTrue(TEXT("Source unchanged"),Src.bEnableCollision && Src.ProcVertexBuffer[0].Position.Y==185.);
    TestEqual(TEXT("Backing preserves topology"),Back.ProcIndexBuffer.Num(),Src.ProcIndexBuffer.Num());
    FPWGv2MeshBuffers Near,Repeat,Mid,OtherSeed;
    PWGDepthPrototype::BuildFormation(Src,847,0,Span,Near);
    PWGDepthPrototype::BuildFormation(Src,847,0,Span,Repeat);
    PWGDepthPrototype::BuildFormation(Src,847,1,Span,Mid);
    PWGDepthPrototype::BuildFormation(Src,123,0,Span,OtherSeed);
    TestTrue(TEXT("Both formation plans nonempty"),!Near.V.IsEmpty() && !Mid.V.IsEmpty());
    TestTrue(TEXT("Deterministic same seed"),Near.V==Repeat.V && Near.T==Repeat.T);
    TestTrue(TEXT("Different seed changes geometry"),Near.V!=OtherSeed.V);
    TestTrue(TEXT("Independent geometry across plans"),Near.V!=Mid.V);
    for (const FVector& V:Near.V)
        if (V.Y<=250. || V.Y>=250.+Span*.30) {AddError(TEXT("Near geometry outside plan"));break;}
    for (const FVector& V:Mid.V)
        if (V.Y<=250.+Span*.30 || V.Y>=250.+Span*.72) {AddError(TEXT("Mid geometry outside plan"));break;}
    for (const auto& V:Back.ProcVertexBuffer)
        if (V.Position.Y<=250.+Span*.72 || V.Position.Y>=550.) {AddError(TEXT("Backing outside safe depth"));break;}
    return true;
}
#endif
