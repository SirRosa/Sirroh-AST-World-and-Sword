#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Styling/SlateTypes.h"
#include "Character/SirrohCharacterProfile.h"
class SSirrohNameEntry;
class SSirrohBitmapText;
class STextBlock;
struct FSirrohUIResources;
DECLARE_DELEGATE_OneParam(FOnSirrohProfilePreview, const FSirrohCharacterProfile&);
DECLARE_DELEGATE_RetVal_OneParam(bool, FOnSirrohProfileSave, const FSirrohCharacterProfile&);
DECLARE_DELEGATE_OneParam(FOnSirrohCreatorView, bool);

class SSirrohCharacterCreator : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SSirrohCharacterCreator) {}
        SLATE_ARGUMENT(const FSirrohUIResources*, Resources)
        SLATE_ARGUMENT(const FSlateBrush*, Preview)
        SLATE_EVENT(FOnSirrohProfilePreview, OnPreview)
        SLATE_EVENT(FOnSirrohProfileSave, OnSave)
        SLATE_EVENT(FSimpleDelegate, OnBack)
        SLATE_EVENT(FOnSirrohCreatorView, OnView)
    SLATE_END_ARGS()
    void Construct(const FArguments& Args);
    void RefreshPreview();
private:
    TSharedRef<SWidget> EyePicker(bool bLeft);
    TSharedRef<SWidget> BodyPicker();
    TSharedRef<SWidget> SexPicker();
    TSharedRef<SWidget> HairPicker();
    FReply Save();
    int32 ActiveCategory=0;
    FSirrohCharacterProfile Draft;
    const FSirrohUIResources* Resources=nullptr;
    FSliderStyle HairSliderStyle;
    TSharedPtr<SSirrohNameEntry> Name;
    TSharedPtr<SSirrohBitmapText> Error;
    FOnSirrohCreatorView OnView;
    FOnSirrohProfilePreview OnPreview;
    FOnSirrohProfileSave OnSave;
};
