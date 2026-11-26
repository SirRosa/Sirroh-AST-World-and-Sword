#include "PWGv2PixelField.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWGPixelFieldTest,"Sirroh.PWG.PixelField",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPWGPixelFieldTest::RunTest(const FString& Parameters)
{
    using namespace PWGPixelField;
    FContext C; for(int32 I=0;I<4;++I){C.Layers[I]=FVector2D(-I*1000,-I*1000+600);C.Openings.Add(C.Layers[I]);}
    FSettings S;
    bool bSeedDiff=false,bPlanDiff=false,bOpen=false,bSolid=false;
    TSet<uint8> Indices;
    for(int32 X=-600;X<600;X+=4)
    {
        const auto A=Compose(C,X,0,123,S),B=Compose(C,X,0,123,S);
        const auto Other=Compose(C,X,0,847,S),Far=Compose(C,X,2,123,S),Closure=Compose(C,X,3,123,S);
        for(int32 Z=-100;Z<700;Z+=8)
        {
            const FColor V=Paint(A,X,Z,0,123,S);
            if(V!=Paint(B,X,Z,0,123,S)){AddError(TEXT("Non-deterministic lattice"));return false;}
            const FColor Last=Paint(Closure,X,Z,3,123,S);
            if(Last.A!=255 || (V.A!=0 && V.A!=255) || V.R%36!=0 || V.R>252){AddError(TEXT("Invalid mask/index/closure"));return false;}
            bOpen|=V.A==0;bSolid|=V.A==255;
            bSeedDiff|=V!=Paint(Other,X,Z,0,847,S);
            bPlanDiff|=V.A!=Paint(Far,X,Z,2,123,S).A;
            if(V.A)Indices.Add(V.R);
        }
    }
    TestTrue(TEXT("Independent seeded fields and plans"),bSeedDiff && bPlanDiff);
    TestTrue(TEXT("Both void and occupied samples"),bOpen && bSolid);
    TestTrue(TEXT("Indexed coherent colour variation"),Indices.Num()>=4);
    return true;
}
#endif
