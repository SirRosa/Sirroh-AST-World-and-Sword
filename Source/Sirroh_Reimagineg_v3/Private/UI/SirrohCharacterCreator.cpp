#include "UI/SirrohCharacterCreator.h"
#include "UI/SirrohMainMenu.h"
#include "UI/SirrohUIResources.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/CoreStyle.h"

void SSirrohCharacterCreator::Construct(const FArguments& Args)
{
    OnView=Args._OnView; OnPreview=Args._OnPreview; OnSave=Args._OnSave; Resources=Args._Resources;
    HairSliderStyle.SetNormalBarImage(Resources->SliderBarBrush).SetHoveredBarImage(Resources->SliderBarBrush).SetDisabledBarImage(Resources->SliderBarBrush)
        .SetNormalThumbImage(Resources->SliderThumbBrush).SetHoveredThumbImage(Resources->SliderThumbBrush).SetDisabledThumbImage(Resources->SliderThumbBrush).SetBarThickness(8.f);
    auto Tabs=SNew(SHorizontalBox);
    const TCHAR* Categories[]={TEXT("Włosy"),TEXT("Twarz"),TEXT("Oczy"),TEXT("Ciało")};
    for(int32 I=0;I<4;++I)Tabs->AddSlot().FillWidth(1).Padding(2)
        [SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SSirrohMenuButton).Resources(Resources).FixedWidth(145).Height(48).GlyphHeight(20).HorizontalPadding(8)
            .Text_Lambda([this,I,Label=FString(Categories[I])](){return FText::FromString((ActiveCategory==I?TEXT("• "):TEXT(""))+Label);})
            .OnClicked_Lambda([this,I,View=Args._OnView](){ActiveCategory=I;View.ExecuteIfBound(I!=3);return FReply::Handled();})]];
    ChildSlot[
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(.52f).Padding(24)
        [SNew(SVerticalBox)
            + SVerticalBox::Slot().FillHeight(1)
            [SNew(SScaleBox).Stretch(EStretch::ScaleToFit)
                [SNew(SBox).WidthOverride(720).HeightOverride(900)[SNew(SImage).Image(Args._Preview)]]]
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
            [SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().Padding(8)
                [SNew(SSirrohMenuButton).Resources(Resources).Height(48).GlyphHeight(20).HorizontalPadding(12).Text(FText::FromString(TEXT("Cała postać")))
                    .OnClicked_Lambda([View=Args._OnView](){View.ExecuteIfBound(false);return FReply::Handled();})]
                + SHorizontalBox::Slot().AutoWidth().Padding(8)
                [SNew(SSirrohMenuButton).Resources(Resources).Height(48).GlyphHeight(20).HorizontalPadding(12).Text(FText::FromString(TEXT("Twarz")))
                    .OnClicked_Lambda([View=Args._OnView](){View.ExecuteIfBound(true);return FReply::Handled();})]
            ]
        ]
        + SHorizontalBox::Slot().FillWidth(.48f).Padding(24).VAlign(VAlign_Fill)
        [SNew(SBorder).BorderImage(&Args._Resources->CreatorPanelBrush).Padding(32)
            [SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(0,12)
                [SNew(SSirrohBitmapText).FontTexture(Resources->FontTexture).GlyphHeight(20).Text(FText::FromString(TEXT("Stwórz postać"))).GlyphHeight(30)]
                + SVerticalBox::Slot().AutoHeight().Padding(0,12)
                [SNew(SSirrohBitmapText).FontTexture(Resources->FontTexture).GlyphHeight(20).Text(FText::FromString(TEXT("Imię postaci")))]
                + SVerticalBox::Slot().AutoHeight().Padding(0,8)
                [SNew(SBox).HeightOverride(64)[SAssignNew(Name,SSirrohNameEntry).Resources(Resources).MaxLength(24).Hint(TEXT("Od 2 do 24 znaków"))]]
                + SVerticalBox::Slot().AutoHeight().Padding(0,8)[Tabs]
                + SVerticalBox::Slot().FillHeight(1).Padding(0,4)
                [SNew(SWidgetSwitcher).WidgetIndex_Lambda([this](){return ActiveCategory;})
                    + SWidgetSwitcher::Slot()[SNew(SScrollBox)+SScrollBox::Slot()[HairPicker()]]
                    + SWidgetSwitcher::Slot()[SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight().Padding(0,16)[SNew(SSirrohBitmapText).FontTexture(Resources->FontTexture).GlyphHeight(18).Text(FText::FromString(TEXT("Twarz zależy od typu ciała.")))]
                        + SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNew(SSirrohBitmapText).FontTexture(Resources->FontTexture).GlyphHeight(18).Text(FText::FromString(TEXT("Brak osobnych wariantów twarzy.")))]]
                    + SWidgetSwitcher::Slot()[SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight().Padding(0,12)[EyePicker(true)]
                        + SVerticalBox::Slot().AutoHeight().Padding(0,12)[EyePicker(false)]]
                    + SWidgetSwitcher::Slot()[SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight().Padding(0,12)[SexPicker()]
                        + SVerticalBox::Slot().AutoHeight().Padding(0,12)[BodyPicker()]]
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0,12)
                [SAssignNew(Error,SSirrohBitmapText).FontTexture(Resources->FontTexture).GlyphHeight(16).ColorAndOpacity(FLinearColor(1.f,.35f,.25f))]
                + SVerticalBox::Slot().AutoHeight().Padding(0,12)
                [SNew(SSirrohMenuButton).Resources(Args._Resources).Text(FText::FromString(TEXT("Zapisz i wybierz świat")))
                    .Height(60).GlyphHeight(22).HorizontalPadding(15).OnClicked(this,&SSirrohCharacterCreator::Save)]
                + SVerticalBox::Slot().AutoHeight().Padding(0,12)
                [SNew(SSirrohMenuButton).Resources(Args._Resources).Text(FText::FromString(TEXT("Wróć")))
                    .Height(52).GlyphHeight(22).HorizontalPadding(15)
                    .OnClicked_Lambda([Back=Args._OnBack](){Back.ExecuteIfBound();return FReply::Handled();})]
            ]
        ]
    ];
}

TSharedRef<SWidget> SSirrohCharacterCreator::BodyPicker()
{
    return SNew(SSirrohMenuButton).Resources(Resources).Height(48).GlyphHeight(20).HorizontalPadding(12)
        .Visibility_Lambda([this](){return Draft.Sex==ESirrohSex::Male?EVisibility::Visible:EVisibility::Collapsed;})
        .Text_Lambda([this](){static const TCHAR* Names[]={TEXT("Grim V1"),TEXT("Grim V7"),TEXT("Grim V10"),TEXT("SimpleMan")};return FText::FromString(FString(TEXT("Typ ciała: "))+Names[uint8(Draft.BodyType)]);})
        .OnClicked_Lambda([this](){Draft.BodyType=ESirrohBodyType((uint8(Draft.BodyType)+1)%4);RefreshPreview();return FReply::Handled();});
}
TSharedRef<SWidget> SSirrohCharacterCreator::EyePicker(bool bLeft)
{
    return SNew(SSirrohMenuButton).Resources(Resources).Height(48).GlyphHeight(20).HorizontalPadding(12)
        .Text_Lambda([this,bLeft](){return FText::FromString(FString(bLeft?TEXT("Lewe oko: "):TEXT("Prawe oko: "))+((bLeft?Draft.LeftEye:Draft.RightEye)==ESirrohIrisColor::Lime?TEXT("Limonkowe"):TEXT("Turkusowe")));})
        .OnClicked_Lambda([this,bLeft](){auto& Eye=bLeft?Draft.LeftEye:Draft.RightEye;Eye=ESirrohIrisColor(1-uint8(Eye));RefreshPreview();return FReply::Handled();});
}

void SSirrohCharacterCreator::RefreshPreview()
{
    FSirrohCharacterProfile Preview=Draft; Preview.Name=TEXT("Podgląd");
    OnPreview.ExecuteIfBound(Preview);
    OnView.ExecuteIfBound(ActiveCategory!=3);
}

FReply SSirrohCharacterCreator::Save()
{
    Draft.Name=Name->GetValue().TrimStartAndEnd();
    if (!Draft.IsValid())
        Error->SetText(FText::FromString(TEXT("Imię: 2–24 znaki; litery, spacje, - lub apostrof.")));
    else if (!OnSave.IsBound() || !OnSave.Execute(Draft))
        Error->SetText(FText::FromString(TEXT("Nie udało się zapisać postaci. Spróbuj ponownie.")));
    return FReply::Handled();
}

TSharedRef<SWidget> SSirrohCharacterCreator::SexPicker()
{
    return SNew(SSirrohMenuButton).Resources(Resources).Height(48).GlyphHeight(20).HorizontalPadding(12)
        .Text_Lambda([this](){return FText::FromString(Draft.Sex==ESirrohSex::Male?TEXT("Płeć: Mężczyzna"):TEXT("Płeć: Kobieta"));})
        .OnClicked_Lambda([this](){Draft.Sex=ESirrohSex(1-uint8(Draft.Sex));RefreshPreview();return FReply::Handled();});
}
TSharedRef<SWidget> SSirrohCharacterCreator::HairPicker()
{
    auto Box=SNew(SVerticalBox).Visibility_Lambda([this](){return Draft.Sex==ESirrohSex::Female || Draft.BodyType==ESirrohBodyType::SimpleMan ? EVisibility::Visible:EVisibility::Collapsed;});
    Box->AddSlot().AutoHeight().Padding(0,6)
    [SNew(SSirrohMenuButton).Resources(Resources).Height(48).GlyphHeight(22).HorizontalPadding(12)
        .Text_Lambda([this](){return FText::FromString(Draft.bHairEnabled?TEXT("Włosy: włączone"):TEXT("Włosy: brak"));})
        .OnClicked_Lambda([this](){Draft.bHairEnabled=!Draft.bHairEnabled;RefreshPreview();return FReply::Handled();})];
    auto Options=SNew(SVerticalBox).Visibility_Lambda([this](){return Draft.bHairEnabled?EVisibility::Visible:EVisibility::Collapsed;});
    Options->AddSlot().AutoHeight().Padding(0,6)
    [SNew(SSirrohMenuButton).Resources(Resources).Height(48).GlyphHeight(22).HorizontalPadding(12)
        .Text_Lambda([this](){static const TCHAR* Styles[]={TEXT("Proste"),TEXT("Falowane"),TEXT("Kręcone"),TEXT("Sterczące")};return FText::FromString(FString(TEXT("Styl: "))+Styles[uint8(Draft.HairStyle)]);})
        .OnClicked_Lambda([this](){Draft.HairStyle=ESirrohHairStyle((uint8(Draft.HairStyle)+1)%4);RefreshPreview();return FReply::Handled();})];
    auto AddLength=[this,&Options](const TCHAR* Label,float FSirrohCharacterProfile::* Field)
    {
        const FString Caption(Label);
        Options->AddSlot().AutoHeight().Padding(8,8,8,2)
        [SNew(SSirrohBitmapText).FontTexture(Resources->FontTexture).GlyphHeight(20)
            .Text_Lambda([this,Caption,Field](){float V=Draft.*Field;return FText::FromString(FString::Printf(TEXT("%s: %.0f cm"),*Caption,V>0?V:Draft.HairLength));})];
        Options->AddSlot().AutoHeight().Padding(10,4,10,8)
        [SNew(SBox).HeightOverride(28)[SNew(SSlider).Style(&HairSliderStyle)
            .Value_Lambda([this,Field](){float V=Draft.*Field;return ((V>0?V:Draft.HairLength)-1.f)/39.f;})
            .OnValueChanged_Lambda([this,Field](float V){Draft.*Field=FMath::RoundToFloat(1.f+39.f*V);RefreshPreview();})]];
    };
    AddLength(TEXT("Grzywka"),&FSirrohCharacterProfile::HairFrontLength);
    AddLength(TEXT("Boki"),&FSirrohCharacterProfile::HairSideLength);
    AddLength(TEXT("Tył"),&FSirrohCharacterProfile::HairBackLength);
    AddLength(TEXT("Góra"),&FSirrohCharacterProfile::HairTopLength);
    Options->AddSlot().AutoHeight().Padding(8,8,8,2)
    [SNew(SSirrohBitmapText).FontTexture(Resources->FontTexture).GlyphHeight(20)
        .Text_Lambda([this](){return FText::FromString(FString::Printf(TEXT("Gęstość: %.0f%%"),Draft.HairDensity*100));})];
    Options->AddSlot().AutoHeight().Padding(10,4,10,8)
    [SNew(SBox).HeightOverride(28)[SNew(SSlider).Style(&HairSliderStyle)
        .Value_Lambda([this](){return (Draft.HairDensity-.35f)/1.15f;})
        .OnValueChanged_Lambda([this](float V){Draft.HairDensity=.35f+1.15f*V;RefreshPreview();})]];
    for(int32 I=0;I<2;++I)
        Options->AddSlot().AutoHeight().Padding(0,4)[SNew(SSirrohMenuButton).Resources(Resources).Height(48).GlyphHeight(20).HorizontalPadding(12)
            .Text_Lambda([this,I](){static const TCHAR* Names[]={TEXT("Czarny"),TEXT("Brązowy"),TEXT("Popielaty"),TEXT("Jasny"),TEXT("Rudy")};return FText::FromString(FString(I?TEXT("Pasemka: "):TEXT("Kolor: "))+Names[I?Draft.HairHighlightColor:Draft.HairColor]);})
            .OnClicked_Lambda([this,I](){if(I)Draft.HairHighlightColor=(Draft.HairHighlightColor+1)%5;else Draft.HairColor=(Draft.HairColor+1)%5;RefreshPreview();return FReply::Handled();})];
    Box->AddSlot().AutoHeight()[Options];return Box;
}
