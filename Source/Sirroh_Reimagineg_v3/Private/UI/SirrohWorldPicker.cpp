#include "UI/SirrohUISubsystem.h"
#include "SirrohMainMenu.h"
#include "Widgets/Layout/SScaleBox.h"
#include "UI/SirrohUIResources.h"
#include "Procedural_World_Generator/PWGv2WorldSession.h"
#include "Procedural_World_Generator/PWGv2Data.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SOverlay.h"
#include "Engine/Texture2D.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void USirrohUISubsystem::HideWorldPicker()
{
    if (WorldPickerWidget.IsValid() && GEngine && GEngine->GameViewport)
        GEngine->GameViewport->RemoveViewportWidgetContent(WorldPickerWidget.ToSharedRef());
    WorldPickerWidget.Reset();
    if (RootWidget.IsValid()) RootWidget->SetVisibility(EVisibility::Visible);
}
void USirrohUISubsystem::ShowWorldPicker()
{
    if (!GEngine || !GEngine->GameViewport || !Resources.IsValid()) return;
    auto* Character=Cast<USirrohCharacterSaveGame>(UGameplayStatics::LoadGameFromSlot(TEXT("SirrohCharacter"),0));
    if (!Character) return;
    if (!Character->CharacterId.IsValid())
    {
        Character->CharacterId=FGuid::NewGuid();
        if (!UGameplayStatics::SaveGameToSlot(Character,TEXT("SirrohCharacter"),0)) return;
    }
    auto* Session=GetGameInstance()->GetSubsystem<UPWGv2WorldSession>();
    Session->CharacterId=Character->CharacterId;
    HideWorldPicker();
    if (RootWidget.IsValid()) RootWidget->SetVisibility(EVisibility::Hidden);
    auto PendingDelete=MakeShared<FString>();
    TSharedRef<SVerticalBox> Content=SNew(SVerticalBox).IsEnabled_Lambda([PendingDelete]() { return PendingDelete->IsEmpty(); });
    WorldDeleteTexture=LoadObject<UTexture2D>(nullptr,TEXT("/Game/Systems/Sprite2Bone/UI/UI_SmallSquare_Reaper.UI_SmallSquare_Reaper"));
    auto DeleteBrush=MakeShared<FSlateBrush>(); DeleteBrush->SetResourceObject(WorldDeleteTexture); DeleteBrush->ImageSize=FVector2D(48,48);
    auto Error=SNew(SSirrohBitmapText).FontTexture(Resources->FontTexture).GlyphHeight(24).ColorAndOpacity(FLinearColor(1,0.3,0.2));
    Error->SetText(FText::FromString(Session->LastError)); Session->LastError.Reset();
    Content->AddSlot().AutoHeight().Padding(8)[SNew(SSirrohBitmapText).FontTexture(Resources->FontTexture).GlyphHeight(24).Text(FText::FromString(TEXT("Wybierz swiat — ")+Character->CharacterName))];
    Content->AddSlot().AutoHeight().Padding(8)[Error];
    TSharedRef<SScrollBox> List=SNew(SScrollBox);
    TArray<FString> Files;
    IFileManager::Get().FindFiles(Files,*(FPaths::ProjectSavedDir()/TEXT("SaveGames/PWGv2_*.sav")),true,false);
    Files.Sort();
    for (const FString& File:Files)
    {
        const FString Name=FPaths::GetBaseFilename(File).RightChop(6);
        if (Name.EndsWith(TEXT("_v1_backup")) || Name.EndsWith(TEXT("_backup"))) continue;
        const auto* SavedWorld=Cast<UPWGv2WorldSave>(UGameplayStatics::LoadGameFromSlot(TEXT("PWGv2_")+Name,0));
        const bool Compatible=SavedWorld && SavedWorld->Version==2 && SavedWorld->AtlasLayoutRevision==1 && !SavedWorld->Biomes.IsEmpty();
        const FString Label=Compatible?Name:Name+TEXT(" (nieobslugiwany zapis)");
        List->AddSlot().Padding(4)[SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(1)[SNew(SSirrohMenuButton).Resources(Resources.Get()).Height(70).GlyphHeight(26).HorizontalPadding(40).IsEnabled(Compatible).Text(FText::FromString(Label)).OnClicked_Lambda([this,Name,Session]()
        {
            Session->SelectedWorld=Name; Session->RequestedSeed=0;
            HideWorldPicker(); OpenDefaultLevel(); return FReply::Handled();
        })]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8,0)[SNew(SButton)
            .ToolTipText(FText::FromString(TEXT("Usun swiat")))
            .OnClicked_Lambda([PendingDelete,Name,DeleteBrush]() { *PendingDelete=Name; return FReply::Handled(); })
            [SNew(SImage).Image(&DeleteBrush.Get())]]];
    }
    if (Files.IsEmpty()) List->AddSlot()[SNew(SSirrohBitmapText).FontTexture(Resources->FontTexture).GlyphHeight(24).Text(FText::FromString(TEXT("Brak zapisanych swiatow")))];
    Content->AddSlot().FillHeight(1).Padding(8)[List];
    auto NameBox=SNew(SSirrohNameEntry).Resources(Resources.Get()).Hint(TEXT("Nazwa swiata")).Identifier(true);
    auto SeedBox=SNew(SSirrohNameEntry).Resources(Resources.Get()).Hint(TEXT("Seed: puste = losowy")).Identifier(true);
    Content->AddSlot().AutoHeight().Padding(8)[SNew(SBox).HeightOverride(70)[NameBox]];
    Content->AddSlot().AutoHeight().Padding(8)[SNew(SBox).HeightOverride(70)[SeedBox]];
    Content->AddSlot().AutoHeight().Padding(8)[SNew(SSirrohMenuButton).Resources(Resources.Get()).Height(70).GlyphHeight(26).HorizontalPadding(40).Text(FText::FromString(TEXT("Utworz swiat"))).OnClicked_Lambda([this,NameBox,SeedBox,Error,Session]()
    {
        const FString Name=NameBox->GetValue().TrimStartAndEnd();
        bool Valid=!Name.IsEmpty() && Name.Len()<=64;
        for (TCHAR C:Name) Valid &= FChar::IsAlnum(C)||C==TEXT('_')||C==TEXT('-');
        int32 Seed=0; const FString SeedText=SeedBox->GetValue().TrimStartAndEnd();
        if (!Valid || Name.EndsWith(TEXT("_backup")) || (!SeedText.IsEmpty() && !LexTryParseString(Seed,*SeedText)))
        { Error->SetText(FText::FromString(TEXT("Sprawdz nazwe i seed (liczba calkowita)."))); return FReply::Handled(); }
        if (UGameplayStatics::DoesSaveGameExist(TEXT("PWGv2_")+Name,0))
        { Error->SetText(FText::FromString(TEXT("Ta nazwa juz istnieje."))); return FReply::Handled(); }
        Session->SelectedWorld=Name; Session->RequestedSeed=Seed;
        HideWorldPicker(); OpenDefaultLevel(); return FReply::Handled();
    })];
    Content->AddSlot().AutoHeight().Padding(8)[SNew(SSirrohMenuButton).Resources(Resources.Get()).Height(70).GlyphHeight(26).HorizontalPadding(40).Text(FText::FromString(TEXT("Wstecz"))).OnClicked_Lambda([this]()
    { HideWorldPicker(); ApplyMenuInputMode(GetWorld()); return FReply::Handled(); })];
    WorldPickerWidget=SNew(SBorder).BorderImage(&Resources->BattlefieldBrush).Padding(30).HAlign(HAlign_Center).VAlign(VAlign_Center)
        [SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SBox).WidthOverride(900).HeightOverride(820)
        [SNew(SOverlay)
        + SOverlay::Slot()[SNew(SBorder).BorderImage(&Resources->CreatorPanelBrush).Padding(36)[Content]]
        + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
        [SNew(SBorder).BorderImage(&Resources->CreatorPanelBrush).Padding(32)
        .Visibility_Lambda([PendingDelete]() { return PendingDelete->IsEmpty()?EVisibility::Collapsed:EVisibility::Visible; })
        [SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(8)[SNew(SSirrohBitmapText).FontTexture(Resources->FontTexture).GlyphHeight(26).Text(FText::FromString(TEXT("Na pewno usunac swiat?")))]
        + SVerticalBox::Slot().AutoHeight().Padding(8)[SNew(SSirrohBitmapText).FontTexture(Resources->FontTexture).GlyphHeight(24).Text_Lambda([PendingDelete]() { return FText::FromString(*PendingDelete); })]
        + SVerticalBox::Slot().AutoHeight().Padding(8)[SNew(SSirrohMenuButton).Resources(Resources.Get()).Height(70).GlyphHeight(26).HorizontalPadding(40).Text(FText::FromString(TEXT("Usun"))).OnClicked_Lambda([this,PendingDelete,Session,Error]()
        {
            const FString Name=*PendingDelete;
            if (Name.IsEmpty()) return FReply::Handled();
            if (!UGameplayStatics::DeleteGameInSlot(TEXT("PWGv2_")+Name,0))
            { PendingDelete->Reset(); Error->SetText(FText::FromString(TEXT("Nie udalo sie usunac swiata."))); return FReply::Handled(); }
            if (Session->SelectedWorld==Name) Session->SelectedWorld.Reset();
            ShowWorldPicker(); return FReply::Handled();
        })]
        + SVerticalBox::Slot().AutoHeight().Padding(8)[SNew(SSirrohMenuButton).Resources(Resources.Get()).Height(70).GlyphHeight(26).HorizontalPadding(40).Text(FText::FromString(TEXT("Anuluj"))).OnClicked_Lambda([PendingDelete]() { PendingDelete->Reset(); return FReply::Handled(); })]
        ]]
        ]]];
    GEngine->GameViewport->AddViewportWidgetContent(WorldPickerWidget.ToSharedRef(),2100);
    if (auto* PC=GetWorld()->GetFirstPlayerController())
    {
        PC->bShowMouseCursor=true;
        FInputModeUIOnly Mode; Mode.SetWidgetToFocus(WorldPickerWidget); PC->SetInputMode(Mode);
    }
}


