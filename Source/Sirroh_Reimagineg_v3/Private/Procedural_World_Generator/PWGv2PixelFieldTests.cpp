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
    int64 Opaque[4]={},Accent[4]={},Base[4]={};
    bool bPaintDiff=false;
    for(int32 X=-600;X<600;X+=4)
    {
        const auto A=Compose(C,X,0,123,S),B=Compose(C,X,0,123,S);
        const auto Other=Compose(C,X,0,847,S),Far=Compose(C,X,2,123,S),Closure=Compose(C,X,3,123,S);
        const auto Left=Compose(C,X-4,0,123,S),Right=Compose(C,X+4,0,123,S);
        for(int32 Z=-100;Z<700;Z+=8)
        {
            const FColor V=Paint(A,X,Z,0,123,S,&Left,&Right,4,128);
            if(V!=Paint(B,X,Z,0,123,S,&Left,&Right,4,128)){AddError(TEXT("Non-deterministic lattice"));return false;}
            const FColor Last=Paint(Closure,X,Z,3,123,S);
            if(Last.A!=255 || (V.A!=0 && V.A!=255) || V.R%36!=0 || V.R>252){AddError(TEXT("Invalid mask/index/closure"));return false;}
            bOpen|=V.A==0;bSolid|=V.A==255;
            bSeedDiff|=V!=Paint(Other,X,Z,0,847,S);
            bPlanDiff|=V.A!=Paint(Far,X,Z,2,123,S).A;
            if(V.A)Indices.Add(V.R);
            for(int32 P=0;P<4;++P)
            {
                const auto CP=Compose(C,X,P,123,S),LP=Compose(C,X-4,P,123,S),RP=Compose(C,X+4,P,123,S);
                const auto Old=PaintLegacy(CP,X,Z,P,123,S),New=Paint(CP,X,Z,P,123,S,&LP,&RP,4,128);
                if(Old.A!=New.A || Old.G!=New.G || (New.A && (New.R%36 || New.R>252)))
                {AddError(TEXT("Readability changed mask, palette family or index encoding"));return false;}
                bPaintDiff|=Old.R!=New.R;
                if(New.A)
                {
                    ++Opaque[P];
                    if(New.R>=180)++Accent[P];
                    if(New.R>=72 && New.R<=144)++Base[P];
                    if(P==3 && (New.R<72 || New.R>144))
                    {AddError(TEXT("Closure must use only calm material body indices"));return false;}
                }
            }
        }
    }
    TestTrue(TEXT("Independent seeded fields and plans"),bSeedDiff && bPlanDiff);
    TestTrue(TEXT("Both void and occupied samples"),bOpen && bSolid);
    TestTrue(TEXT("Indexed coherent colour variation"),Indices.Num()>=3 && bPaintDiff);
    for(int32 P=0;P<4;++P)
    {
        TestTrue(TEXT("Material body dominates"),Opaque[P]>0 && Base[P]*100>=Opaque[P]*80);
        TestTrue(TEXT("Mineral accents rare"),Accent[P]*100<=Opaque[P]*3);
        if(P>=2)TestEqual(TEXT("No distant mineral accents"),Accent[P],int64(0));
    }
    TestTrue(TEXT("Biome endpoints vary without changing indices"),BiomeTint(0,0,0)!=BiomeTint(1,1,0));
    return true;
}
#endif
