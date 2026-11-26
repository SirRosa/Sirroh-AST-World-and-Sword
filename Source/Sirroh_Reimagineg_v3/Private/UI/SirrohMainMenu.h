#pragma once


#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/Input/SSlider.h"
#include "UI/SirrohCharacterCreator.h"


class SButton;
class SWidgetSwitcher;
class UTexture2D;
struct FSirrohUIResources;




/** Draws text directly from the custom 10x7 Sirro_Font texture atlas. */
class SSirrohBitmapText : public SLeafWidget
{
public:
        SLATE_BEGIN_ARGS(SSirrohBitmapText)
                : _Text()
                , _FontTexture(nullptr)
                , _GlyphHeight(46.0f)
                , _Scale(1.0f)
                , _ColorAndOpacity(FLinearColor::White)
        {}
                SLATE_ATTRIBUTE(FText, Text)
                SLATE_ARGUMENT(UTexture2D*, FontTexture)
                SLATE_ARGUMENT(float, GlyphHeight)
                SLATE_ATTRIBUTE(float, Scale)
                SLATE_ARGUMENT(FLinearColor, ColorAndOpacity)
        SLATE_END_ARGS()


        void Construct(const FArguments& InArgs);
        void SetText(const FText& InText);
        virtual bool ComputeVolatility() const override { return SLeafWidget::ComputeVolatility() || Text.IsBound() || Scale.IsBound(); }


        virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
                const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
                int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
        virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;


private:
        bool FindGlyph(TCHAR Character, int32& OutIndex) const;
        float GetAdvance(TCHAR Character) const;


        TAttribute<FText> Text;
        UTexture2D* FontTexture = nullptr;
        FSlateBrush FontBrush;
        float GlyphHeight = 46.0f;
        TAttribute<float> Scale;
        FLinearColor ColorAndOpacity = FLinearColor::White;
};


/** Paints repeated copies of the panorama and moves them at a frame-rate-independent speed. */
class SSirrohScrollingPanorama : public SLeafWidget
{
public:
        SLATE_BEGIN_ARGS(SSirrohScrollingPanorama)
                : _PanoramaBrush(nullptr)
                , _ScrollSpeed(14.0f)
        {}
                SLATE_ARGUMENT(const FSlateBrush*, PanoramaBrush)
                SLATE_ARGUMENT(float, ScrollSpeed)
        SLATE_END_ARGS()


        void Construct(const FArguments& InArgs);


        virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
        virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
                const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
                int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
        virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;


private:
        const FSlateBrush* PanoramaBrush = nullptr;
        float ScrollSpeed = 14.0f;
        double ScrollOffset = 0.0;
};


/** Composes and animates the three transparent layers of the main-menu logo. */
class SSirrohAnimatedLogo : public SLeafWidget
{
public:
        SLATE_BEGIN_ARGS(SSirrohAnimatedLogo)
                : _Resources(nullptr)
        {}
                SLATE_ARGUMENT(const FSirrohUIResources*, Resources)
        SLATE_END_ARGS()


        void Construct(const FArguments& InArgs);


        virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
        virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
                const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
                int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
        virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;


private:
        const FSirrohUIResources* Resources = nullptr;
        double AnimationTime = 0.0;
};


/** Reusable atlas-backed menu button. */
class SSirrohMenuButton : public SCompoundWidget
{
public:
        SLATE_BEGIN_ARGS(SSirrohMenuButton)
                : _Text()
                , _Resources(nullptr)
                , _IsEnabled(true)
                , _FixedWidth(0.0f)
                , _Height(157.0f)
                , _GlyphHeight(0.0f)
                , _HorizontalPadding(300.0f)
        {}
                SLATE_ATTRIBUTE(FText, Text)
                SLATE_ARGUMENT(const FSirrohUIResources*, Resources)
                SLATE_EVENT(FOnClicked, OnClicked)
                SLATE_ARGUMENT(bool, IsEnabled)
                SLATE_ARGUMENT(float, FixedWidth)
                SLATE_ARGUMENT(float, Height)
                SLATE_ARGUMENT(float, GlyphHeight)
                SLATE_ARGUMENT(float, HorizontalPadding)
        SLATE_END_ARGS()


        void Construct(const FArguments& InArgs);
        TSharedPtr<SWidget> GetFocusableWidget() const;
        virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;


private:
        const FSlateBrush* GetCurrentBrush() const;


        const FSirrohUIResources* Resources = nullptr;
        TSharedPtr<SButton> Button;
        float CurrentTextScale = 1.0f;
};


/** Focusable text input that paints every entered character from Sirro_Font. */
class SSirrohNameEntry : public SCompoundWidget
{
public:
        SLATE_BEGIN_ARGS(SSirrohNameEntry) : _Resources(nullptr), _Hint(TEXT("Nazwij Postać")), _Identifier(false), _MaxLength(20) {}
                SLATE_ARGUMENT(FString, Hint)
                SLATE_ARGUMENT(bool, Identifier)
                SLATE_ARGUMENT(int32, MaxLength)
                SLATE_ARGUMENT(const FSirrohUIResources*, Resources)
        SLATE_END_ARGS()


        void Construct(const FArguments& InArgs);
        const FString& GetValue() const { return Value; }
        virtual bool SupportsKeyboardFocus() const override { return true; }
        virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
        virtual FReply OnKeyChar(const FGeometry& MyGeometry, const FCharacterEvent& InCharacterEvent) override;


private:
        FString Value;
        FString Hint;
        bool bIdentifier=false;
        int32 MaxLength=20;
        const FSirrohUIResources* Resources = nullptr;
        TSharedPtr<SButton> Button;
        TSharedPtr<SSirrohBitmapText> ValueText;
        float CurrentTextScale = 1.0f;
        virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
        const FSlateBrush* GetCurrentBrush() const;
};


/** The complete first UI screen, including page routing and quit confirmation. */
class SSirrohUIRoot : public SCompoundWidget
{
public:
        SLATE_BEGIN_ARGS(SSirrohUIRoot) {}
                SLATE_ARGUMENT(TSharedPtr<FSirrohUIResources>, Resources)
                SLATE_ARGUMENT(const FSlateBrush*, CharacterPreviewBrush)
                SLATE_ARGUMENT(bool, HasSavedCharacter)
                SLATE_ARGUMENT(FString, SavedCharacterName)
                SLATE_EVENT(FOnSirrohProfilePreview, OnProfilePreview)
                SLATE_EVENT(FOnSirrohCreatorView, OnCreatorView)
                SLATE_EVENT(FOnSirrohProfileSave, OnCharacterCreated)
                SLATE_EVENT(FSimpleDelegate, OnLoadCharacter)
                SLATE_EVENT(FSimpleDelegate, OnQuitConfirmed)
        SLATE_END_ARGS()


        void Construct(const FArguments& InArgs);
        TSharedPtr<SWidget> GetInitialFocusWidget() const;


        virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
        virtual TOptional<bool> OnQueryShowFocus(const EFocusCause InFocusCause) const override { return false; }


private:
        TSharedRef<SWidget> BuildMainMenu();
        TSharedRef<SWidget> BuildPlayPage();
        TSharedRef<SWidget> BuildInfoPage();
        TSharedRef<SWidget> BuildCharacterCreationPage();
        TSharedRef<SWidget> BuildQuitDialog();
        TSharedRef<SWidget> BuildMenuButton(const FText& Text, const FOnClicked& OnClicked, bool bEnabled = true);
        FReply ShowNewGamePage();
        FReply ShowCharacterCreationPage();
        FReply ShowSettingsPage();
        FReply ReturnToMainMenu();
        FReply RequestQuit();
        FReply CancelQuit();
        FReply ConfirmQuit();
        void ShowInfo(const FText& Heading);


        TSharedPtr<FSirrohUIResources> Resources;
        TSharedPtr<SWidgetSwitcher> PageSwitcher;
        TSharedPtr<SSirrohBitmapText> InfoHeading;
        TSharedPtr<SSirrohMenuButton> NewGameButton;
        TSharedPtr<SSirrohMenuButton> BackButton;
        TSharedPtr<SSirrohMenuButton> CharacterCreationButton;
        TSharedPtr<SSirrohMenuButton> QuitNoButton;
        const FSlateBrush* CharacterPreviewBrush = nullptr;
        bool bHasSavedCharacter = false;
        FString SavedCharacterName;
        FOnSirrohProfilePreview OnProfilePreview;
        FOnSirrohCreatorView OnCreatorView;
        FOnSirrohProfileSave OnCharacterCreated;
        TSharedPtr<SSirrohCharacterCreator> CharacterCreator;
        FSimpleDelegate OnLoadCharacter;
        FSimpleDelegate OnQuitConfirmed;
        bool bQuitDialogVisible = false;
};
