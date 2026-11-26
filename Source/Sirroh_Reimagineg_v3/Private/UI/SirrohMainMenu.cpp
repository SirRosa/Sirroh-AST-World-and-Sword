#include "UI/SirrohMainMenu.h"








#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/Reply.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "UI/SirrohUIResources.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"








namespace SirrohUI
{
        constexpr int32 FontColumns = 10;
        constexpr int32 FontRows = 7;
        constexpr float FontEmPixels = 147.0f;
        constexpr float GlyphSpacingPixels = 10.0f;
        constexpr float SpaceAdvanceFactor = 0.32f;
        constexpr float PanoramaAspect = 3.0f;








        // The order exactly matches the rows in Sirro_Font.png.
        const FString GlyphOrder = TEXT("aąbcćdeęfghijklłmnńoópqrsśtuvwxyzźżAĄBCĆDEĘFGHIJKLŁMNŃOÓPQRŚSTUVWXYZŹŻ");








        const int32 FontRowTops[FontRows] = { 17, 164, 307, 452, 600, 751, 899 };








        // Tight alpha bounds measured from Sirro_Font.png. Unlike an even 10x7
        // division, these regions do not contain the atlas' decorative cell lines.
        const FIntRect GlyphRegions[] =
        {
                FIntRect(58, 56, 115, 139), FIntRect(203, 57, 260, 156), FIntRect(344, 32, 402, 149),
                FIntRect(491, 59, 544, 146), FIntRect(636, 33, 684, 148), FIntRect(776, 31, 829, 149),
                FIntRect(921, 59, 971, 146), FIntRect(1068, 58, 1118, 156), FIntRect(1215, 32, 1273, 149),
                FIntRect(1367, 47, 1430, 155),
                FIntRect(55, 179, 113, 292), FIntRect(209, 186, 249, 291), FIntRect(350, 181, 388, 299),
                FIntRect(480, 179, 546, 293), FIntRect(636, 181, 675, 292), FIntRect(776, 179, 826, 294),
                FIntRect(899, 200, 984, 291), FIntRect(1058, 199, 1124, 289), FIntRect(1206, 184, 1272, 296),
                FIntRect(1369, 197, 1421, 289),
                FIntRect(62, 322, 114, 436), FIntRect(196, 332, 260, 437), FIntRect(345, 329, 401, 437),
                FIntRect(485, 340, 545, 430), FIntRect(627, 338, 685, 433), FIntRect(767, 321, 823, 434),
                FIntRect(920, 329, 970, 437), FIntRect(1058, 341, 1125, 431), FIntRect(1210, 340, 1276, 432),
                FIntRect(1354, 341, 1442, 433),
                FIntRect(53, 486, 120, 576), FIntRect(198, 477, 259, 590), FIntRect(344, 488, 398, 576),
                FIntRect(486, 467, 538, 581), FIntRect(629, 472, 680, 581), FIntRect(755, 473, 843, 586),
                FIntRect(903, 473, 989, 590), FIntRect(1050, 470, 1128, 586), FIntRect(1200, 470, 1273, 589),
                FIntRect(1358, 463, 1431, 589),
                FIntRect(49, 619, 129, 734), FIntRect(193, 622, 271, 734), FIntRect(338, 622, 411, 742),
                FIntRect(479, 621, 554, 740), FIntRect(614, 615, 693, 736), FIntRect(755, 619, 844, 737),
                FIntRect(919, 621, 976, 733), FIntRect(1061, 623, 1125, 738), FIntRect(1198, 621, 1288, 739),
                FIntRect(1361, 621, 1440, 738),
                FIntRect(57, 768, 125, 881), FIntRect(184, 775, 276, 882), FIntRect(330, 773, 415, 882),
                FIntRect(472, 760, 558, 882), FIntRect(617, 771, 694, 884), FIntRect(765, 762, 836, 883),
                FIntRect(911, 772, 983, 884), FIntRect(1048, 769, 1126, 884), FIntRect(1198, 770, 1288, 883),
                FIntRect(1361, 768, 1432, 880),
                FIntRect(55, 905, 117, 1024), FIntRect(192, 908, 269, 1023), FIntRect(334, 912, 410, 1020),
                FIntRect(477, 915, 550, 1020), FIntRect(608, 918, 706, 1022), FIntRect(761, 919, 839, 1021),
                FIntRect(906, 911, 973, 1024), FIntRect(1054, 915, 1125, 1022), FIntRect(1209, 904, 1273, 1022),
                FIntRect(1364, 905, 1426, 1023)
        };








        static_assert(UE_ARRAY_COUNT(GlyphRegions) == FontColumns * FontRows);








        float MeasureBitmapTextWidth(const FString& String, const float GlyphHeight)
        {
                float Width = 0.0f;
                const float PixelScale = GlyphHeight / FontEmPixels;
                for (const TCHAR Character : String)
                {
                        if (FChar::IsWhitespace(Character))
                        {
                                Width += GlyphHeight * SpaceAdvanceFactor;
                                continue;
                        }








                        const int32 GlyphIndex = GlyphOrder.Find(FString::Chr(Character), ESearchCase::CaseSensitive);
                        Width += GlyphIndex != INDEX_NONE
                                ? (GlyphRegions[GlyphIndex].Width() + GlyphSpacingPixels) * PixelScale
                                : GlyphHeight * SpaceAdvanceFactor;
                }
                return Width;
        }
}








void SSirrohBitmapText::Construct(const FArguments& InArgs)
{
        Text = InArgs._Text;
        FontTexture = InArgs._FontTexture;
        GlyphHeight = InArgs._GlyphHeight;
        Scale = InArgs._Scale;
        ColorAndOpacity = InArgs._ColorAndOpacity;
        FontBrush = FSirrohUIResources::MakeWholeTextureBrush(FontTexture);
}








void SSirrohBitmapText::SetText(const FText& InText)
{
        Text = InText;
        Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
}








bool SSirrohBitmapText::FindGlyph(TCHAR Character, int32& OutIndex) const
{
        OutIndex = SirrohUI::GlyphOrder.Find(FString::Chr(Character), ESearchCase::CaseSensitive);
        return OutIndex != INDEX_NONE;
}








float SSirrohBitmapText::GetAdvance(TCHAR Character) const
{
        if (FChar::IsWhitespace(Character))
        {
                return GlyphHeight * SirrohUI::SpaceAdvanceFactor;
        }








        int32 GlyphIndex = INDEX_NONE;
        if (FindGlyph(Character, GlyphIndex))
        {
                const float PixelScale = GlyphHeight / SirrohUI::FontEmPixels;
                return (SirrohUI::GlyphRegions[GlyphIndex].Width() + SirrohUI::GlyphSpacingPixels) * PixelScale;
        }
        return GlyphHeight * .65f;
}








FVector2D SSirrohBitmapText::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
        float Width = 0.0f;
        for (const TCHAR Character : Text.Get(FText::GetEmpty()).ToString())
        {
                Width += GetAdvance(Character);
        }
        return FVector2D(Width, GlyphHeight);
}








int32 SSirrohBitmapText::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
        int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
        if (!FontTexture)
        {
                return LayerId;
        }








        const FString String = Text.Get(FText::GetEmpty()).ToString();
        const float DrawScale = FMath::Max(0.01f, Scale.Get(1.0f));
        const FVector2D Desired = ComputeDesiredSize(1.0f) * DrawScale;
        float CursorX = FMath::Max(0.0f, (AllottedGeometry.GetLocalSize().X - Desired.X) * 0.5f);
        const float PixelScale = GlyphHeight * DrawScale / SirrohUI::FontEmPixels;
        const float EmTop = FMath::Max(0.0f, (AllottedGeometry.GetLocalSize().Y - GlyphHeight * DrawScale) * 0.5f);
        const ESlateDrawEffect DrawEffects = bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
        const FLinearColor FinalTint = InWidgetStyle.GetColorAndOpacityTint() * ColorAndOpacity;








        for (const TCHAR Character : String)
        {
                int32 GlyphIndex = INDEX_NONE;
                if (FindGlyph(Character, GlyphIndex))
                {
                        const int32 Row = GlyphIndex / SirrohUI::FontColumns;
                        const FIntRect& Region = SirrohUI::GlyphRegions[GlyphIndex];
                        const FVector2f TextureSize(FontTexture->GetSizeX(), FontTexture->GetSizeY());
                        const FVector2f UVMin(Region.Min.X / TextureSize.X, Region.Min.Y / TextureSize.Y);
                        const FVector2f UVMax(Region.Max.X / TextureSize.X, Region.Max.Y / TextureSize.Y);
                        const FVector2f DrawSize(Region.Width() * PixelScale, Region.Height() * PixelScale);
                        const float DrawY = EmTop + (Region.Min.Y - SirrohUI::FontRowTops[Row]) * PixelScale;








                        FSlateBrush GlyphBrush = FontBrush;
                        GlyphBrush.SetUVRegion(FBox2f(UVMin, UVMax));








                        FSlateDrawElement::MakeBox(
                                OutDrawElements,
                                LayerId,
                                AllottedGeometry.ToPaintGeometry(
                                        DrawSize,
                                        FSlateLayoutTransform(FVector2f(CursorX, DrawY))),
                                &GlyphBrush,
                                DrawEffects,
                                FinalTint);
                }
                else if (!FChar::IsWhitespace(Character))
                {
                        // The Sirro atlas contains letters only. Keep measurements and punctuation readable.
                        const FSlateFontInfo Fallback = FCoreStyle::GetDefaultFontStyle("Regular", FMath::Max(1,FMath::RoundToInt(GlyphHeight*.72f)));
                        FSlateDrawElement::MakeText(OutDrawElements,LayerId,
                                AllottedGeometry.ToPaintGeometry(FVector2f(GetAdvance(Character),GlyphHeight),
                                    FSlateLayoutTransform(DrawScale,FVector2f(CursorX,EmTop))),
                                FString::Chr(Character),Fallback,DrawEffects,FinalTint);
                }
                CursorX += GetAdvance(Character) * DrawScale;
        }








        return LayerId;
}








void SSirrohScrollingPanorama::Construct(const FArguments& InArgs)
{
        PanoramaBrush = InArgs._PanoramaBrush;
        ScrollSpeed = InArgs._ScrollSpeed;
        SetCanTick(true);
}








void SSirrohScrollingPanorama::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
        const float TileWidth = AllottedGeometry.GetLocalSize().Y * SirrohUI::PanoramaAspect;
        if (TileWidth > 0.0f)
        {
                ScrollOffset = FMath::Fmod(ScrollOffset + static_cast<double>(ScrollSpeed) * InDeltaTime, TileWidth);
                Invalidate(EInvalidateWidgetReason::Paint);
        }
}








FVector2D SSirrohScrollingPanorama::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
        return FVector2D::ZeroVector;
}








int32 SSirrohScrollingPanorama::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
        int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
        if (!PanoramaBrush)
        {
                return LayerId;
        }








        const FVector2f LocalSize = AllottedGeometry.GetLocalSize();
        const float TileWidth = LocalSize.Y * SirrohUI::PanoramaAspect;
        if (TileWidth <= 0.0f)
        {
                return LayerId;
        }








        const ESlateDrawEffect DrawEffects = bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
        for (float X = static_cast<float>(ScrollOffset) - TileWidth; X < LocalSize.X; X += TileWidth)
        {
                const FVector2f TileSize(TileWidth, LocalSize.Y);
                FSlateDrawElement::MakeBox(
                        OutDrawElements,
                        LayerId,
                        AllottedGeometry.ToPaintGeometry(TileSize, FSlateLayoutTransform(FVector2f(X, 0.0f))),
                        PanoramaBrush,
                        DrawEffects,
                        InWidgetStyle.GetColorAndOpacityTint());
        }
        return LayerId;
}








void SSirrohAnimatedLogo::Construct(const FArguments& InArgs)
{
        Resources = InArgs._Resources;
        SetCanTick(true);
}








void SSirrohAnimatedLogo::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
        AnimationTime += InDeltaTime;
        Invalidate(EInvalidateWidgetReason::Paint);
}








FVector2D SSirrohAnimatedLogo::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
        return FVector2D(780.0f, 439.0f);
}








int32 SSirrohAnimatedLogo::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
        int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
        if (!Resources)
        {
                return LayerId;
        }








        const FVector2f LocalSize = AllottedGeometry.GetLocalSize();
        const ESlateDrawEffect DrawEffects = bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
        const FLinearColor Tint = InWidgetStyle.GetColorAndOpacityTint();








        // One full journey from left to right and back takes 18 seconds. The cosine
        // eases naturally at both ends; the parabola adds a gentle upward arc.
        const double Phase = FMath::Fmod(AnimationTime, 18.0) / 18.0 * 2.0 * UE_DOUBLE_PI;
        const float TravelAlpha = 0.5f - 0.5f * static_cast<float>(FMath::Cos(Phase));
        const float ArcAlpha = 4.0f * TravelAlpha * (1.0f - TravelAlpha);
        const float MoonSize = LocalSize.Y * 0.48f;
        const float MoonX = FMath::Lerp(LocalSize.X * 0.06f, LocalSize.X * 0.94f - MoonSize, TravelAlpha);
        const float MoonY = LocalSize.Y * 0.13f - LocalSize.Y * 0.055f * ArcAlpha;








        FSlateDrawElement::MakeBox(
                OutDrawElements,
                LayerId,
                AllottedGeometry.ToPaintGeometry(
                        FVector2f(MoonSize, MoonSize),
                        FSlateLayoutTransform(FVector2f(MoonX, MoonY))),
                &Resources->BloodMoonBrush,
                DrawEffects,
                Tint);








        FSlateDrawElement::MakeBox(
                OutDrawElements,
                LayerId + 1,
                AllottedGeometry.ToPaintGeometry(LocalSize, FSlateLayoutTransform()),
                &Resources->BattlefieldBrush,
                DrawEffects,
                Tint);








        const float TitleBob = static_cast<float>(FMath::Sin(AnimationTime * 1.15)) * 5.0f;
        FSlateDrawElement::MakeBox(
                OutDrawElements,
                LayerId + 2,
                AllottedGeometry.ToPaintGeometry(
                        LocalSize,
                        FSlateLayoutTransform(FVector2f(0.0f, TitleBob))),
                &Resources->TitleBrush,
                DrawEffects,
                Tint);








        return LayerId + 2;
}








void SSirrohMenuButton::Construct(const FArguments& InArgs)
{
        Resources = InArgs._Resources;
        const FString InitialText = InArgs._Text.Get(FText::GetEmpty()).ToString();
        const float ButtonFontHeight = InArgs._GlyphHeight > 0.0f ? InArgs._GlyphHeight : (InitialText.Len() > 12 ? 58.0f : 76.0f);
        const float TextWidthAtHover = SirrohUI::MeasureBitmapTextWidth(InitialText, ButtonFontHeight) * 1.10f;
        const float ButtonWidth = InArgs._FixedWidth > 0.0f
                ? InArgs._FixedWidth
                : FMath::Clamp(TextWidthAtHover + InArgs._HorizontalPadding, 260.0f, 1040.0f);
        SetCanTick(true);








        ChildSlot
        [
                SNew(SBox)
                .WidthOverride(ButtonWidth)
                .HeightOverride(InArgs._Height)
                [
                        SAssignNew(Button, SButton)
                        .ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder"))
                        .ContentPadding(0.0f)
                        .IsEnabled(InArgs._IsEnabled)
                        .OnClicked(InArgs._OnClicked)
                        [
                                SNew(SOverlay)
                                + SOverlay::Slot()
                                [
                                        SNew(SImage)
                                        .Image_Lambda([this]() { return GetCurrentBrush(); })
                                        .ColorAndOpacity_Lambda([this]()
                                        {
                                                if (!Button.IsValid() || !Button->IsEnabled())
                                                {
                                                        return FLinearColor(0.32f, 0.32f, 0.32f, 0.9f);
                                                }
                                                return Button->IsPressed() ? FLinearColor(0.72f, 0.72f, 0.72f, 1.0f) : FLinearColor::White;
                                        })
                                ]
                                + SOverlay::Slot()
                                .HAlign(HAlign_Center)
                                .VAlign(VAlign_Center)
                                [
                                        SNew(SSirrohBitmapText)
                                        .Text(InArgs._Text)
                                        .FontTexture(Resources ? Resources->FontTexture : nullptr)
                                        .GlyphHeight(ButtonFontHeight)
                                        .Scale_Lambda([this]() { return CurrentTextScale; })
                                ]
                        ]
                ]
        ];
}








void SSirrohMenuButton::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
        const float TargetScale = Button.IsValid() && Button->IsHovered() ? 1.10f : 1.0f;
        const float PreviousScale = CurrentTextScale;
        CurrentTextScale = FMath::FInterpTo(CurrentTextScale, TargetScale, InDeltaTime, 14.0f);








        if (!FMath::IsNearlyEqual(PreviousScale, CurrentTextScale, 0.0001f))
        {
                Invalidate(EInvalidateWidgetReason::Paint);
        }
}








const FSlateBrush* SSirrohMenuButton::GetCurrentBrush() const
{
        if (!Resources)
        {
                return FCoreStyle::Get().GetDefaultBrush();
        }
        if (Button.IsValid() && Button->IsPressed())
        {
                return &Resources->ButtonPressedBrush;
        }
        if (Button.IsValid() && Button->IsHovered())
        {
                return &Resources->ButtonHoveredBrush;
        }
        return &Resources->ButtonNormalBrush;
}








TSharedPtr<SWidget> SSirrohMenuButton::GetFocusableWidget() const
{
        return Button;
}

















void SSirrohNameEntry::Construct(const FArguments& InArgs)
{
        Hint=InArgs._Hint; bIdentifier=InArgs._Identifier; MaxLength=InArgs._MaxLength;
        Resources = InArgs._Resources;
        SetCanTick(true);
        ChildSlot
        [
                SAssignNew(Button, SButton)
                .ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder"))
                .ContentPadding(0.0f)
                .OnClicked_Lambda([this]()
                {
                        FSlateApplication::Get().SetKeyboardFocus(SharedThis(this), EFocusCause::Mouse);
                        return FReply::Handled();
                })
                [
                        SNew(SOverlay)
                        + SOverlay::Slot()
                        [
                                SNew(SImage).Image_Lambda([this]() { return GetCurrentBrush(); })
                        ]
                        + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(18.0f, 8.0f)
                        [
                                SAssignNew(ValueText, SSirrohBitmapText)
                                .Text(FText::FromString(Hint))
                                .FontTexture(InArgs._Resources ? InArgs._Resources->FontTexture : nullptr)
                                .GlyphHeight(37.0f)
                                .Scale_Lambda([this]() { return CurrentTextScale; })
                        ]
                ]
        ];
}








void SSirrohNameEntry::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
        const float Target = Button.IsValid() && Button->IsHovered() ? 1.10f : 1.0f;
        const float PreviousScale = CurrentTextScale;
        CurrentTextScale = FMath::FInterpTo(CurrentTextScale, Target, InDeltaTime, 14.0f);
        if (!FMath::IsNearlyEqual(PreviousScale, CurrentTextScale, 0.0001f))
        {
                Invalidate(EInvalidateWidgetReason::Paint);
        }
}








const FSlateBrush* SSirrohNameEntry::GetCurrentBrush() const
{
        if (!Resources) return FCoreStyle::Get().GetDefaultBrush();
        return Button.IsValid() && Button->IsHovered() ? &Resources->ButtonHoveredBrush : &Resources->ButtonNormalBrush;
}








FReply SSirrohNameEntry::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
        if (InKeyEvent.GetKey() == EKeys::BackSpace && !Value.IsEmpty())
        {
                Value.LeftChopInline(1);
                ValueText->SetText(FText::FromString(Value.IsEmpty() ? Hint : Value));
                return FReply::Handled();
        }
        return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}








FReply SSirrohNameEntry::OnKeyChar(const FGeometry& MyGeometry, const FCharacterEvent& InCharacterEvent)
{
        const TCHAR Character = InCharacterEvent.GetCharacter();
        if (Value.Len() < MaxLength && (bIdentifier ? (FChar::IsAlnum(Character) || Character==TEXT('_') || Character==TEXT('-')) : (FChar::IsAlpha(Character) || Character == TEXT(' ') || Character == TEXT('-') || Character == TEXT('\''))))
        {
                Value.AppendChar(Character);
                ValueText->SetText(FText::FromString(Value));
                return FReply::Handled();
        }
        return SCompoundWidget::OnKeyChar(MyGeometry, InCharacterEvent);
}








void SSirrohUIRoot::Construct(const FArguments& InArgs)
{
        Resources = InArgs._Resources;
        CharacterPreviewBrush = InArgs._CharacterPreviewBrush;
        bHasSavedCharacter = InArgs._HasSavedCharacter;
        SavedCharacterName = InArgs._SavedCharacterName;
        OnProfilePreview = InArgs._OnProfilePreview;
        OnCreatorView = InArgs._OnCreatorView;
        OnCharacterCreated = InArgs._OnCharacterCreated;
        OnLoadCharacter = InArgs._OnLoadCharacter;
        OnQuitConfirmed = InArgs._OnQuitConfirmed;








        ChildSlot
        [
                SNew(SOverlay)
                + SOverlay::Slot()
                [
                        SNew(SSirrohScrollingPanorama)
                        .PanoramaBrush(Resources.IsValid() ? &Resources->PanoramaBrush : nullptr)
                        .ScrollSpeed(36.0f)
                ]
                + SOverlay::Slot()
                [
                        SNew(SBorder)
                        .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                        .BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.34f))
                ]
                + SOverlay::Slot()
                .Padding(24.0f)
                [
                        SNew(SScaleBox)
                        .Stretch(EStretch::ScaleToFit)
                        .StretchDirection(EStretchDirection::DownOnly)
                        [
                        SNew(SBox)
                                .WidthOverride(1800.0f)
                                .HeightOverride(1020.0f)
                                [
                                        SAssignNew(PageSwitcher, SWidgetSwitcher)
                                        .WidgetIndex(0)
                                        + SWidgetSwitcher::Slot()
                                        [
                                                BuildMainMenu()
                                        ]
                                        + SWidgetSwitcher::Slot()
                                        [
                                                BuildPlayPage()
                                        ]
                                        + SWidgetSwitcher::Slot()
                                        [
                                                BuildCharacterCreationPage()
                                        ]
                                        + SWidgetSwitcher::Slot()
                                        [
                                                BuildInfoPage()
                                        ]
                                ]
                        ]
                ]
                + SOverlay::Slot()
                [
                        SNew(SBox)
                        .Visibility_Lambda([this]()
                        {
                                return bQuitDialogVisible ? EVisibility::Visible : EVisibility::Collapsed;
                        })
                        [
                                BuildQuitDialog()
                        ]
                ]
        ];
}








TSharedRef<SWidget> SSirrohUIRoot::BuildMainMenu()
{
        return SNew(SVerticalBox)
                + SVerticalBox::Slot()
                .AutoHeight()
                .HAlign(HAlign_Center)
                [
                        SNew(SBox)
                        .WidthOverride(780.0f)
                        .HeightOverride(439.0f)
                        [
                                SNew(SSirrohAnimatedLogo)
                                .Resources(Resources.Get())
                        ]
                ]
                + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 2.0f)
                [
                        SAssignNew(NewGameButton, SSirrohMenuButton)
                        .Text(FText::FromString(TEXT("Graj")))
                        .Resources(Resources.Get())
                        .OnClicked(this, &SSirrohUIRoot::ShowNewGamePage)
                ]
                + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 2.0f)
                [
                        BuildMenuButton(FText::FromString(TEXT("Ustawienia")), FOnClicked::CreateSP(this, &SSirrohUIRoot::ShowSettingsPage))
                ]
                + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 2.0f)
                [
                        BuildMenuButton(FText::FromString(TEXT("Opuść Grę")), FOnClicked::CreateSP(this, &SSirrohUIRoot::RequestQuit))
                ];
}



































TSharedRef<SWidget> SSirrohUIRoot::BuildCharacterCreationPage()
{
    return SAssignNew(CharacterCreator, SSirrohCharacterCreator)
        .Resources(Resources.Get()).Preview(CharacterPreviewBrush)
        .OnPreview(OnProfilePreview)
        .OnSave(FOnSirrohProfileSave::CreateLambda([this](const FSirrohCharacterProfile& Profile)
        {
            if (!OnCharacterCreated.IsBound() || !OnCharacterCreated.Execute(Profile)) return false;
            bHasSavedCharacter = true;
            SavedCharacterName = Profile.Name;
            return true;
        }))
        .OnView(OnCreatorView)
        .OnBack(FSimpleDelegate::CreateLambda([this](){ShowNewGamePage();}));
}








TSharedRef<SWidget> SSirrohUIRoot::BuildPlayPage()
{
        TSharedRef<SVerticalBox> Options = SNew(SVerticalBox);
        Options->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 10.0f)
        [
                SNew(SSirrohMenuButton).Text(FText::FromString(TEXT("Wybierz Postać"))).Resources(Resources.Get())
                .FixedWidth(760.0f).Height(100.0f).GlyphHeight(54.0f).HorizontalPadding(40.0f)
        ];








                Options->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 5.0f)
                [
                        SNew(SSirrohMenuButton).Resources(Resources.Get())
                        .Text_Lambda([this](){return FText::FromString(SavedCharacterName);})
                        .Visibility_Lambda([this](){return bHasSavedCharacter ? EVisibility::Visible : EVisibility::Collapsed;})
                        .OnClicked_Lambda([this]()
                        {
                                OnLoadCharacter.ExecuteIfBound();
                                return FReply::Handled();
                        })
                ];








        Options->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 5.0f)
        [
                BuildMenuButton(FText::FromString(TEXT("Stwórz Postać")), FOnClicked::CreateSP(this, &SSirrohUIRoot::ShowCharacterCreationPage))
        ];
        Options->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 5.0f)
        [
                BuildMenuButton(FText::FromString(TEXT("Wróć")), FOnClicked::CreateSP(this, &SSirrohUIRoot::ReturnToMainMenu))
        ];








        return SNew(SBox).VAlign(VAlign_Center).HAlign(HAlign_Center)[Options];
}








TSharedRef<SWidget> SSirrohUIRoot::BuildInfoPage()
{
        return SNew(SVerticalBox)
                + SVerticalBox::Slot().FillHeight(1.0f).VAlign(VAlign_Center).HAlign(HAlign_Center)
                [
                        SAssignNew(InfoHeading, SSirrohBitmapText).Text(FText::FromString(TEXT("Ustawienia")))
                        .FontTexture(Resources.IsValid() ? Resources->FontTexture : nullptr).GlyphHeight(72.0f)
                ]
                + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 4.0f)
                [
                        SAssignNew(BackButton, SSirrohMenuButton)
                        .Text(FText::FromString(TEXT("Wróć")))
                        .Resources(Resources.Get())
                        .OnClicked(this, &SSirrohUIRoot::ReturnToMainMenu)
                ];
}








TSharedRef<SWidget> SSirrohUIRoot::BuildQuitDialog()
{
        return SNew(SOverlay)
                + SOverlay::Slot()
                [
                        SNew(SBorder)
                        .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                        .BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.82f))
                ]
                + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
                [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 20.0f)
                        [
                                SNew(SSirrohBitmapText)
                                .Text(FText::FromString(TEXT("Opuścić Grę")))
                                .FontTexture(Resources.IsValid() ? Resources->FontTexture : nullptr)
                                .GlyphHeight(64.0f)
                        ]
                        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 4.0f)
                        [
                                BuildMenuButton(FText::FromString(TEXT("Tak")), FOnClicked::CreateSP(this, &SSirrohUIRoot::ConfirmQuit))
                        ]
                        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 4.0f)
                        [
                                SAssignNew(QuitNoButton, SSirrohMenuButton)
                                .Text(FText::FromString(TEXT("Nie")))
                                .Resources(Resources.Get())
                                .OnClicked(this, &SSirrohUIRoot::CancelQuit)
                        ]
                ];
}








TSharedRef<SWidget> SSirrohUIRoot::BuildMenuButton(const FText& Text, const FOnClicked& OnClicked, bool bEnabled)
{
        return SNew(SSirrohMenuButton)
                .Text(Text)
                .Resources(Resources.Get())
                .IsEnabled(bEnabled)                .OnClicked(OnClicked);
}








void SSirrohUIRoot::ShowInfo(const FText& Heading)
{
        if (InfoHeading.IsValid())
        {
                InfoHeading->SetText(Heading);
        }
        if (PageSwitcher.IsValid())
        {
                PageSwitcher->SetActiveWidgetIndex(3);
        }
        if (BackButton.IsValid() && BackButton->GetFocusableWidget().IsValid())
        {
                FSlateApplication::Get().SetKeyboardFocus(BackButton->GetFocusableWidget(), EFocusCause::SetDirectly);
        }
}








FReply SSirrohUIRoot::ShowNewGamePage()
{
        if (PageSwitcher.IsValid())
        {
                PageSwitcher->SetActiveWidgetIndex(1);
        }
        return FReply::Handled();
}








FReply SSirrohUIRoot::ShowCharacterCreationPage()
{
        if (PageSwitcher.IsValid())
        {
                PageSwitcher->SetActiveWidgetIndex(2);
        }
        if (CharacterCreator) CharacterCreator->RefreshPreview();
        return FReply::Handled();
}








FReply SSirrohUIRoot::ShowSettingsPage()
{
        ShowInfo(FText::FromString(TEXT("Ustawienia")));
        return FReply::Handled();
}













































































































































FReply SSirrohUIRoot::ReturnToMainMenu()
{
        if (PageSwitcher.IsValid())
        {
                PageSwitcher->SetActiveWidgetIndex(0);
        }
        if (NewGameButton.IsValid() && NewGameButton->GetFocusableWidget().IsValid())
        {
                FSlateApplication::Get().SetKeyboardFocus(NewGameButton->GetFocusableWidget(), EFocusCause::Navigation);
        }
        return FReply::Handled();
}








FReply SSirrohUIRoot::RequestQuit()
{
        bQuitDialogVisible = true;
        Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
        if (QuitNoButton.IsValid() && QuitNoButton->GetFocusableWidget().IsValid())
        {
                FSlateApplication::Get().SetKeyboardFocus(QuitNoButton->GetFocusableWidget(), EFocusCause::SetDirectly);
        }
        return FReply::Handled();
}








FReply SSirrohUIRoot::CancelQuit()
{
        bQuitDialogVisible = false;
        Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
        if (NewGameButton.IsValid() && NewGameButton->GetFocusableWidget().IsValid())
        {
                FSlateApplication::Get().SetKeyboardFocus(NewGameButton->GetFocusableWidget(), EFocusCause::SetDirectly);
        }
        return FReply::Handled();
}








FReply SSirrohUIRoot::ConfirmQuit()
{
        OnQuitConfirmed.ExecuteIfBound();
        return FReply::Handled();
}








FReply SSirrohUIRoot::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
        if (InKeyEvent.GetKey() == EKeys::Escape)
        {
                if (bQuitDialogVisible)
                {
                        return CancelQuit();
                }
                if (PageSwitcher.IsValid() && PageSwitcher->GetActiveWidgetIndex() == 2)
                {
                        return ShowNewGamePage();
                }
                if (PageSwitcher.IsValid() && PageSwitcher->GetActiveWidgetIndex() != 0)
                {
                        return ReturnToMainMenu();
                }
                return RequestQuit();
        }
        return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}








TSharedPtr<SWidget> SSirrohUIRoot::GetInitialFocusWidget() const
{
        return NewGameButton.IsValid() ? NewGameButton->GetFocusableWidget() : nullptr;
}
