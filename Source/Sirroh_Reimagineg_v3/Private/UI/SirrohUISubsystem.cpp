#include "UI/SirrohUISubsystem.h"
















#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "Engine/SkeletalMesh.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Camera/PlayerCameraManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Modules/ModuleManager.h"
#include "Sound/SoundWave.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UI/SirrohMainMenu.h"
#include "UI/SirrohUIResources.h"
#include "UObject/UObjectGlobals.h"
#include "PWG/PWGGameMode.h"
#include "Procedural_World_Generator/PWGv2Generator.h"
#include "Character/SirrohCharacterIdentityComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
















DEFINE_LOG_CATEGORY_STATIC(LogSirrohUI, Log, All);
















class SSirrohPauseMenu final : public SCompoundWidget
{
public:
        SLATE_BEGIN_ARGS(SSirrohPauseMenu) {}
                SLATE_ARGUMENT(const FSirrohUIResources*, Resources)
                SLATE_EVENT(FSimpleDelegate, OnResume)
                SLATE_EVENT(FSimpleDelegate, OnRespawn)
                SLATE_EVENT(FSimpleDelegate, OnMainMenu)
                SLATE_EVENT(FSimpleDelegate, OnQuit)
        SLATE_END_ARGS()
















        void Construct(const FArguments& Args)
        {
                OnResume = Args._OnResume;
                auto Button = [R=Args._Resources](const TCHAR* Text, FSimpleDelegate Action)
                {
                        return SNew(SSirrohMenuButton).Text(FText::FromString(Text)).Resources(R)
                                .FixedWidth(560.0f).Height(105.0f).GlyphHeight(36.0f).HorizontalPadding(80.0f)
                                .OnClicked_Lambda([Action]() { Action.ExecuteIfBound(); return FReply::Handled(); });
                };
                ChildSlot[SNew(SOverlay)
                        + SOverlay::Slot()[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                                .BorderBackgroundColor(FLinearColor(0,0,0,0.82f))]
                        + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
                        [SNew(SVerticalBox)
                                + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0,0,0,24)
                                [SNew(SSirrohBitmapText).Text(FText::FromString(TEXT("Pauza"))).FontTexture(Args._Resources->FontTexture).GlyphHeight(52.0f)]
                                + SVerticalBox::Slot().AutoHeight().Padding(0,5)[Button(TEXT("Wznów"), Args._OnResume)]
                                + SVerticalBox::Slot().AutoHeight().Padding(0,5)[Button(TEXT("Respawn"), Args._OnRespawn)]
                                + SVerticalBox::Slot().AutoHeight().Padding(0,5)[Button(TEXT("Wyjdź - Do Menu Głównego"), Args._OnMainMenu)]
                                + SVerticalBox::Slot().AutoHeight().Padding(0,5)[Button(TEXT("Wyjdź z Gry"), Args._OnQuit)]]];
        }
        virtual bool SupportsKeyboardFocus() const override { return true; }
        virtual FReply OnKeyDown(const FGeometry&, const FKeyEvent& Event) override
        {
                if (Event.GetKey() == EKeys::Escape) { OnResume.ExecuteIfBound(); return FReply::Handled(); }
                return FReply::Unhandled();
        }
private:
        FSimpleDelegate OnResume;
};
















namespace SirrohUIPaths
{
        const TCHAR* Font = TEXT("/Game/Systems/Sprite2Bone/UI/Sirro_Font.Sirro_Font");
        const TCHAR* Battlefield = TEXT("/Game/Systems/Sprite2Bone/UI/BattleField_Background.BattleField_Background");
        const TCHAR* BloodMoon = TEXT("/Game/Systems/Sprite2Bone/UI/Blood_Moon.Blood_Moon");
        const TCHAR* Title = TEXT("/Game/Systems/Sprite2Bone/UI/SirroH.SirroH");
        const TCHAR* NormalFrame = TEXT("/Game/Systems/Sprite2Bone/UI/Frame_Normal_UI.Frame_Normal_UI");
        const TCHAR* RedFrame = TEXT("/Game/Systems/Sprite2Bone/UI/Frame_Red_UI.Frame_Red_UI");
        const TCHAR* Panorama = TEXT("/Game/Systems/Sprite2Bone/UI/UI_Panorama.UI_Panorama");
        const TCHAR* Deco = TEXT("/Game/Systems/Sprite2Bone/UI/UI_Deco.UI_Deco");
        const TCHAR* OtherDeco = TEXT("/Game/Systems/Sprite2Bone/UI/UI_OtherDeco.UI_OtherDeco");
        const TCHAR* SliderHead = TEXT("/Game/Systems/Sprite2Bone/UI/UI_SliderHead.UI_SliderHead");
        const TCHAR* SliderLine = TEXT("/Game/Systems/Sprite2Bone/UI/UI_SliderLine.UI_SliderLine");
        const TCHAR* CharacterFrame = TEXT("/Game/Systems/Sprite2Bone/UI/UI_CharacterFrame.UI_CharacterFrame");
        const TCHAR* SmallSquare = TEXT("/Game/Systems/Sprite2Bone/UI/UI_SmallSquare_A.UI_SmallSquare_A");
        const TCHAR* SmallSquareSelected = TEXT("/Game/Systems/Sprite2Bone/UI/UI_SmallSquare_Red.UI_SmallSquare_Red");
        const TCHAR* PreviewSourceMesh = TEXT("/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin.SKM_UEFN_Mannequin");
        const TCHAR* Music = TEXT("/Game/Systems/Sprite2Bone/Music/Main_Menu_Song_OST.Main_Menu_Song_OST");
        const FString MainMenuMapName = TEXT("L_MainMenu");
        const FString GameplayMapName = TEXT("L_SirrohGameplay");
        const FString CharacterSaveSlot = TEXT("SirrohCharacter");
}
















void USirrohUISubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
        Super::Initialize(Collection);
        PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
                this, &USirrohUISubsystem::HandlePostLoadMap);
}
















void USirrohUISubsystem::Deinitialize()
{
        HidePauseMenu(false);
        HideMainMenu();
        if (PostLoadMapHandle.IsValid())
        {
                FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
                PostLoadMapHandle.Reset();
        }
        Resources.Reset();
        FontTexture = nullptr;
        BattlefieldTexture = nullptr;
        BloodMoonTexture = nullptr;
        TitleTexture = nullptr;
        NormalFrameTexture = nullptr;
        RedFrameTexture = nullptr;
        PanoramaTexture = nullptr;
        DecoTexture = nullptr;
        OtherDecoTexture = nullptr;
        SliderHeadTexture = nullptr;
        SliderLineTexture = nullptr;
        CharacterFrameTexture = nullptr;
        SmallSquareTexture = nullptr;
        SmallSquareSelectedTexture = nullptr;
        MainMenuMusic = nullptr;
        MenuMusicComponent = nullptr;
        Super::Deinitialize();
}
















void USirrohUISubsystem::TogglePauseMenu(APlayerController* PlayerController)
{
        if (PauseWidget.IsValid()) HidePauseMenu();
        else ShowPauseMenu(PlayerController);
}
















void USirrohUISubsystem::ShowPauseMenu(APlayerController* PlayerController)
{
        if (!PlayerController || !GEngine || !GEngine->GameViewport || !LoadUITextures()) return;
        if (!Resources.IsValid())
        {
                Resources = MakeShared<FSirrohUIResources>(FontTexture, BattlefieldTexture, BloodMoonTexture, TitleTexture,
                        NormalFrameTexture, RedFrameTexture, PanoramaTexture, DecoTexture, OtherDecoTexture, SliderHeadTexture,
                        SliderLineTexture, CharacterFrameTexture, SmallSquareTexture, SmallSquareSelectedTexture);
        }
        PausedPlayerController = PlayerController;
        PauseWidget = SNew(SSirrohPauseMenu).Resources(Resources.Get())
                .OnResume(FSimpleDelegate::CreateUObject(this, &USirrohUISubsystem::HidePauseMenu, true))
                .OnRespawn(FSimpleDelegate::CreateUObject(this, &USirrohUISubsystem::RespawnFromPause))
                .OnMainMenu(FSimpleDelegate::CreateUObject(this, &USirrohUISubsystem::ReturnToMainMenuFromPause))
                .OnQuit(FSimpleDelegate::CreateUObject(this, &USirrohUISubsystem::QuitGame));
        GEngine->GameViewport->AddViewportWidgetContent(PauseWidget.ToSharedRef(), 2000);
        UGameplayStatics::SetGamePaused(PlayerController, true);
        PlayerController->bShowMouseCursor = true;
        FInputModeUIOnly Mode; Mode.SetWidgetToFocus(PauseWidget); Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        PlayerController->SetInputMode(Mode);
}
















void USirrohUISubsystem::HidePauseMenu(const bool bUnpauseGame)
{
        if (PauseWidget.IsValid() && GEngine && GEngine->GameViewport)
                GEngine->GameViewport->RemoveViewportWidgetContent(PauseWidget.ToSharedRef());
        PauseWidget.Reset();
        if (APlayerController* PC = PausedPlayerController.Get())
        {
                if (bUnpauseGame) UGameplayStatics::SetGamePaused(PC, false);
                PC->bShowMouseCursor = false;
                PC->SetInputMode(FInputModeGameOnly());
        }
        PausedPlayerController.Reset();
}
















void USirrohUISubsystem::RespawnFromPause()
{
        APlayerController* PC = PausedPlayerController.Get();
        HidePauseMenu();
        if (PC) if (APWGGameMode* GM = PC->GetWorld()->GetAuthGameMode<APWGGameMode>()) GM->RespawnPlayer(PC);
}
















void USirrohUISubsystem::ReturnToMainMenuFromPause()
{
        if (GetWorld()) if (auto* Mode=GetWorld()->GetAuthGameMode<APWGGameMode>())
            if (auto* V2=Cast<APWGv2Generator>(Mode->GetWorldGenerator())) V2->SaveWorld();
        UWorld* World = GetWorld();
        HidePauseMenu();
        if (World) UGameplayStatics::OpenLevel(World, FName(TEXT("L_MainMenu")));
}
















void USirrohUISubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
        if (!LoadedWorld || !LoadedWorld->IsGameWorld())
        {
                return;
        }
















        const FString CleanMapName = UWorld::RemovePIEPrefix(LoadedWorld->GetMapName());
        const bool bForcePreview = FParse::Param(FCommandLine::Get(), TEXT("SirrohMainMenuPreview"));
        // Developer-only rendering/automation switch. Normal launches still enforce
        // Main Menu -> character select/creation -> DefaultLevel.
        const bool bSkipMainMenu = FParse::Param(FCommandLine::Get(), TEXT("SirrohSkipMainMenu"));
        if (CleanMapName == SirrohUIPaths::MainMenuMapName || bForcePreview)
        {
                bRedirectingToMainMenu = false;
                bGameplayLaunchAuthorized = false;
                ShowMainMenu(LoadedWorld);
        }
        else if (CleanMapName == SirrohUIPaths::GameplayMapName && !bGameplayLaunchAuthorized && !bSkipMainMenu)
        {
                // PIE started directly from DefaultLevel should still follow the intended
                // Main Menu -> Character Creation/Load -> generated world flow.
                if (!bRedirectingToMainMenu)
                {
                        bRedirectingToMainMenu = true;
                        TWeakObjectPtr<UWorld> WeakWorld(LoadedWorld);
                        LoadedWorld->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakWorld]()
                        {
                                if (WeakWorld.IsValid())
                                {
                                        UGameplayStatics::OpenLevel(WeakWorld.Get(), FName(TEXT("L_MainMenu")));
                                }
                        }));
                }
                return;
        }
        else
        {
                bGameplayLaunchAuthorized = false;
                bRedirectingToMainMenu = false;
                HideMainMenu();
                ScheduleSavedCharacterApply(LoadedWorld);        }
}
















bool USirrohUISubsystem::LoadUITextures()
{
        FontTexture = LoadObject<UTexture2D>(nullptr, SirrohUIPaths::Font);
        BattlefieldTexture = LoadObject<UTexture2D>(nullptr, SirrohUIPaths::Battlefield);
        BloodMoonTexture = LoadObject<UTexture2D>(nullptr, SirrohUIPaths::BloodMoon);
        TitleTexture = LoadObject<UTexture2D>(nullptr, SirrohUIPaths::Title);
        NormalFrameTexture = LoadObject<UTexture2D>(nullptr, SirrohUIPaths::NormalFrame);
        RedFrameTexture = LoadObject<UTexture2D>(nullptr, SirrohUIPaths::RedFrame);
        PanoramaTexture = LoadObject<UTexture2D>(nullptr, SirrohUIPaths::Panorama);
        DecoTexture = LoadObject<UTexture2D>(nullptr, SirrohUIPaths::Deco);
        OtherDecoTexture = LoadObject<UTexture2D>(nullptr, SirrohUIPaths::OtherDeco);
        SliderHeadTexture = LoadObject<UTexture2D>(nullptr, SirrohUIPaths::SliderHead);
        SliderLineTexture = LoadObject<UTexture2D>(nullptr, SirrohUIPaths::SliderLine);
        CharacterFrameTexture = LoadObject<UTexture2D>(nullptr, SirrohUIPaths::CharacterFrame);
        SmallSquareTexture = LoadObject<UTexture2D>(nullptr, SirrohUIPaths::SmallSquare);
        SmallSquareSelectedTexture = LoadObject<UTexture2D>(nullptr, SirrohUIPaths::SmallSquareSelected);
        MainMenuMusic = LoadObject<USoundWave>(nullptr, SirrohUIPaths::Music);
















        const bool bLoadedAll = FontTexture && BattlefieldTexture && BloodMoonTexture && TitleTexture
                && NormalFrameTexture && RedFrameTexture && PanoramaTexture && DecoTexture
                && OtherDecoTexture && SliderHeadTexture && SliderLineTexture && CharacterFrameTexture
                && SmallSquareTexture && SmallSquareSelectedTexture;
        if (!bLoadedAll)
        {
                UE_LOG(LogSirrohUI, Error,
                        TEXT("Main menu textures are missing. Expected them in /Game/Systems/Sprite2Bone/UI."));
        }
        if (!MainMenuMusic)
        {
                UE_LOG(LogSirrohUI, Warning, TEXT("Main-menu music is missing at %s."), SirrohUIPaths::Music);
        }
        return bLoadedAll;
}
















void USirrohUISubsystem::ShowMainMenu(UWorld* MenuWorld)
{
        HideMainMenu();
        if (!GEngine || !GEngine->GameViewport || !LoadUITextures())
        {
                return;
        }
















        Resources = MakeShared<FSirrohUIResources>(
                FontTexture,
                BattlefieldTexture,
                BloodMoonTexture,
                TitleTexture,
                NormalFrameTexture,
                RedFrameTexture,
                PanoramaTexture,
                DecoTexture,
                OtherDecoTexture,
                SliderHeadTexture,
                SliderLineTexture,
                CharacterFrameTexture,
                SmallSquareTexture,
                SmallSquareSelectedTexture);
        CreateCharacterPreview(MenuWorld);
















        const bool bHasSavedCharacter = UGameplayStatics::DoesSaveGameExist(SirrohUIPaths::CharacterSaveSlot, 0);
        FString SavedCharacterName;
        if (bHasSavedCharacter)
        {
                if (const USirrohCharacterSaveGame* Save = Cast<USirrohCharacterSaveGame>(
                        UGameplayStatics::LoadGameFromSlot(SirrohUIPaths::CharacterSaveSlot, 0)))
                {
                        SavedCharacterName = Save->CharacterName;
                }
        }
        RootWidget = SNew(SSirrohUIRoot)
                .Resources(Resources)
                .CharacterPreviewBrush(CharacterRenderTarget ? &CharacterPreviewBrush : nullptr)
                .HasSavedCharacter(bHasSavedCharacter)
                .SavedCharacterName(SavedCharacterName)
                .OnProfilePreview(FOnSirrohProfilePreview::CreateUObject(this, &USirrohUISubsystem::UpdatePreviewProfile))
                .OnCreatorView(FOnSirrohCreatorView::CreateUObject(this, &USirrohUISubsystem::SetCreatorView))
                .OnCharacterCreated(FOnSirrohProfileSave::CreateUObject(this, &USirrohUISubsystem::CreateCharacter))
                .OnLoadCharacter(FSimpleDelegate::CreateUObject(this, &USirrohUISubsystem::LoadCharacter))
                .OnQuitConfirmed(FSimpleDelegate::CreateUObject(this, &USirrohUISubsystem::QuitGame));
        ViewportWidget = RootWidget;
        GEngine->GameViewport->AddViewportWidgetContent(RootWidget.ToSharedRef(), 1000);
        StartMenuMusic(MenuWorld);
        UE_LOG(LogSirrohUI, Display, TEXT("C++ main menu added to the viewport on map %s."), *MenuWorld->GetMapName());
















        TWeakObjectPtr<USirrohUISubsystem> WeakThis(this);
        MenuWorld->GetTimerManager().SetTimerForNextTick([WeakThis, MenuWorld]()
        {
                if (WeakThis.IsValid())
                {
                        WeakThis->ApplyMenuInputMode(MenuWorld);
                }
        });
}
















void USirrohUISubsystem::HideMainMenu()
{
        HideWorldPicker();
        DestroyCharacterPreview();
        if (MenuMusicComponent)
        {
                MenuMusicComponent->Stop();
                MenuMusicComponent->DestroyComponent();
                MenuMusicComponent = nullptr;
        }
















        if (ViewportWidget.IsValid() && GEngine && GEngine->GameViewport)
        {
                GEngine->GameViewport->RemoveViewportWidgetContent(ViewportWidget.ToSharedRef());
        }
        ViewportWidget.Reset();
        RootWidget.Reset();
        Resources.Reset();
}
















void USirrohUISubsystem::StartMenuMusic(UWorld* MenuWorld)
{
        if (!MenuWorld || !MainMenuMusic)
        {
                return;
        }
















        MainMenuMusic->bLooping = true;
        MenuMusicComponent = UGameplayStatics::SpawnSound2D(
                MenuWorld,
                MainMenuMusic,
                1.0f,
                1.0f,
                0.0f,
                nullptr,
                false,
                false);
}
















void USirrohUISubsystem::ApplyMenuInputMode(UWorld* MenuWorld)
{
        APlayerController* PlayerController = MenuWorld ? MenuWorld->GetFirstPlayerController() : nullptr;
        if (!PlayerController || !RootWidget.IsValid())
        {
                return;
        }
















        PlayerController->bShowMouseCursor = true;
        FInputModeUIOnly InputMode;
        InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        InputMode.SetWidgetToFocus(RootWidget->GetInitialFocusWidget());
        PlayerController->SetInputMode(InputMode);
}
















void USirrohUISubsystem::QuitGame()
{
        if (GetWorld()) if (auto* Mode=GetWorld()->GetAuthGameMode<APWGGameMode>())
            if (auto* V2=Cast<APWGv2Generator>(Mode->GetWorldGenerator())) V2->SaveWorld();
        UWorld* World = GetWorld();
        APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
        UKismetSystemLibrary::QuitGame(World, PlayerController, EQuitPreference::Quit, false);
}
















void USirrohUISubsystem::OpenDefaultLevel()
{
        if (UWorld* World = GetWorld())
        {
                bGameplayLaunchAuthorized = true;
                UGameplayStatics::OpenLevel(World, FName(TEXT("L_SirrohGameplay")));
        }
}
















int32 USirrohUISubsystem::CountHeads() const
{
        FAssetRegistryModule& Module = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
        TArray<FString> PathsToScan;
        PathsToScan.Add(TEXT("/Game/Systems/Sprite2Bone/Textures/Atlases/Heads"));
        Module.Get().ScanPathsSynchronous(PathsToScan, true);
        TArray<FAssetData> Assets;
        Module.Get().GetAssetsByPath(FName(TEXT("/Game/Systems/Sprite2Bone/Textures/Atlases/Heads")), Assets, false);
        Assets.RemoveAll([](const FAssetData& Asset) { return Asset.AssetClassPath != UTexture2D::StaticClass()->GetClassPathName(); });
        const bool bHasDirectional38Heads = Assets.ContainsByPredicate([](const FAssetData& Asset)
        {
                return Asset.AssetName.ToString().EndsWith(TEXT("_38VT"), ESearchCase::IgnoreCase);
        });
        if (bHasDirectional38Heads)
        {
                Assets.RemoveAll([](const FAssetData& Asset)
                {
                        return !Asset.AssetName.ToString().EndsWith(TEXT("_38VT"), ESearchCase::IgnoreCase);
                });
        }
        return FMath::Max(1, Assets.Num());
}
















void USirrohUISubsystem::CreateCharacterPreview(UWorld* MenuWorld)
{
    DestroyCharacterPreview();
    if (!MenuWorld) return;
    USkeletalMesh* Asset = LoadObject<USkeletalMesh>(nullptr,
        TEXT("/Game/Characters/GrimModular/Meshes/SKM_Grim_Default.SKM_Grim_Default"));
    UMaterialInterface* Presentation = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/Characters/GrimModular/Materials/M_Grim_PreviewUI.M_Grim_PreviewUI"));
    if (!Asset || !Presentation)
    {
        UE_LOG(LogSirrohUI, Error, TEXT("Creator preview: missing mesh or UI material."));
        return;
    }
    CharacterRenderTarget = NewObject<UTextureRenderTarget2D>(this);
    CharacterRenderTarget->ClearColor = FLinearColor(0.025f,0.025f,0.03f,1.f);
    CharacterRenderTarget->InitCustomFormat(720,900,PF_B8G8R8A8,true);
    CharacterRenderTarget->UpdateResourceImmediate(true);
    // Slate receives opaque RGB; scene-capture alpha must never hide the image.
    CharacterPreviewMaterial = UMaterialInstanceDynamic::Create(Presentation,this);
    CharacterPreviewMaterial->SetTextureParameterValue(TEXT("PreviewTexture"),CharacterRenderTarget);
    CharacterPreviewBrush = FSlateBrush();
    CharacterPreviewBrush.SetResourceObject(CharacterPreviewMaterial);
    CharacterPreviewBrush.DrawAs = ESlateBrushDrawType::Image;
    CharacterPreviewBrush.ImageSize = FVector2D(720,900);

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    PreviewCharacter = MenuWorld->SpawnActor<AActor>(AActor::StaticClass(),FVector::ZeroVector,FRotator::ZeroRotator,Params);
    if (!PreviewCharacter) return;
    auto* Mesh = NewObject<USkeletalMeshComponent>(PreviewCharacter,TEXT("CreatorMesh"));
    PreviewCharacter->AddInstanceComponent(Mesh);
    PreviewCharacter->SetRootComponent(Mesh);
    Mesh->SetSkeletalMesh(Asset);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetGenerateOverlapEvents(false);
    Mesh->RegisterComponent();
    PreviewCharacter->SetActorLocation(FVector(0,0,-100000));
    PreviewCharacter->SetActorEnableCollision(false);
    PreviewIdentityComponent = USirrohCharacterIdentityComponent::FindOrCreate(PreviewCharacter);
    FSirrohCharacterProfile Draft; Draft.Name = TEXT("Podgl�d");
    if (!PreviewIdentityComponent->ApplyProfile(Draft))
        UE_LOG(LogSirrohUI, Error, TEXT("Creator preview: appearance application failed."));

    PreviewCaptureActor = MenuWorld->SpawnActor<AActor>(AActor::StaticClass(),FVector::ZeroVector,FRotator::ZeroRotator,Params);
    if (!PreviewCaptureActor) return;
    auto* Capture = NewObject<USceneCaptureComponent2D>(PreviewCaptureActor,TEXT("CreatorCamera"));
    PreviewCaptureActor->SetRootComponent(Capture);
    PreviewCaptureActor->AddInstanceComponent(Capture);
    Capture->TextureTarget = CharacterRenderTarget;
    Capture->ProjectionType = ECameraProjectionMode::Perspective;
    Capture->FOVAngle = 30.f;
    Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
    Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
    Capture->ShowOnlyComponents.Add(Mesh);
    Capture->PostProcessSettings.bOverride_AutoExposureMethod = true;
    Capture->PostProcessSettings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
    Capture->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
    Capture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
    Capture->bCaptureEveryFrame = true;
    Capture->bCaptureOnMovement = true;
    Capture->RegisterComponent();
    Capture->SetWorldRotation(FRotator(0,90,0));
    PreviewCaptureComponent = Capture;
    SetCreatorView(false);
}

void USirrohUISubsystem::DestroyCharacterPreview()
{
        if (PreviewCaptureActor) PreviewCaptureActor->Destroy();
        if (PreviewCharacter) PreviewCharacter->Destroy();
        PreviewCaptureActor = nullptr;
        PreviewCaptureComponent = nullptr;
        PreviewCharacter = nullptr;
        PreviewIdentityComponent = nullptr;
        CharacterRenderTarget = nullptr;
        CharacterPreviewMaterial = nullptr;
        CharacterPreviewBrush = FSlateBrush();
}
















void USirrohUISubsystem::UpdatePreviewProfile(const FSirrohCharacterProfile& Profile)
{
    if (PreviewIdentityComponent) PreviewIdentityComponent->ApplyProfile(Profile);
    if (PreviewCaptureComponent && PreviewCharacter) { PreviewCaptureComponent->ClearShowOnlyComponents(); PreviewCaptureComponent->ShowOnlyActorComponents(PreviewCharacter); }
    SetCreatorView(true);
}

void USirrohUISubsystem::SetCreatorView(bool bFace)
{
    if (!PreviewCharacter || !PreviewCaptureComponent) return;
    const bool bGrimV7 = PreviewIdentityComponent &&
        (PreviewIdentityComponent->GetProfile().Sex == ESirrohSex::Female || PreviewIdentityComponent->GetProfile().BodyType != ESirrohBodyType::GrimV1);
    // Each body type keeps its imported reference orientation; only the preview camera changes.
    PreviewCaptureComponent->SetWorldRotation(FRotator(0, bGrimV7 ? -90.f : 90.f, 0));
    const FVector FaceTarget = bGrimV7 ? FVector(0, 3, 160) : FVector(0, -3, 153);
    const FVector Target = PreviewCharacter->GetActorTransform().TransformPosition(bFace ? FaceTarget : FVector(0,0,85));
    PreviewCaptureComponent->SetWorldLocation(Target - PreviewCaptureComponent->GetForwardVector() * 360.f);
    PreviewCaptureComponent->FOVAngle = bFace ? 7.f : 30.f;
}

























bool USirrohUISubsystem::CreateCharacter(const FSirrohCharacterProfile& Profile)
{
    if (!Profile.IsValid()) return false; // Saving data must not depend on the preview renderer.
    auto* Save = Cast<USirrohCharacterSaveGame>(UGameplayStatics::CreateSaveGameObject(USirrohCharacterSaveGame::StaticClass()));
    if (!Save) return false;
    Save->Profile = Profile;
    Save->ProfileVersion = 3;
    Save->CharacterName = Profile.Name;
    Save->CharacterId = FGuid::NewGuid();
    Save->BodyPresetId = Profile.Sex == ESirrohSex::Female ? TEXT("GrimGirl") : Profile.BodyType == ESirrohBodyType::SimpleMan ? TEXT("SimpleMan") : Profile.BodyType == ESirrohBodyType::GrimV10 ? TEXT("GrimV10") : (Profile.BodyType == ESirrohBodyType::GrimV7 ? TEXT("GrimV7") : TEXT("GrimV1"));
    if (!UGameplayStatics::SaveGameToSlot(Save, SirrohUIPaths::CharacterSaveSlot, 0)) return false;
    ShowWorldPicker();
    return true;
}








void USirrohUISubsystem::LoadCharacter()
{
        if (UGameplayStatics::DoesSaveGameExist(SirrohUIPaths::CharacterSaveSlot, 0)) ShowWorldPicker();
}
















void USirrohUISubsystem::ApplySavedCharacter(UWorld* World)
{
    if (!World) return;
    APawn* Pawn = World->GetFirstPlayerController() ? World->GetFirstPlayerController()->GetPawn() : nullptr;
    if (Pawn && USirrohCharacterIdentityComponent::ApplySaved(Pawn))
        World->GetTimerManager().ClearTimer(SavedCharacterApplyTimer);
}
















void USirrohUISubsystem::ScheduleSavedCharacterApply(UWorld* World)
{
        if (!World)
        {
                return;
        }
















        World->GetTimerManager().ClearTimer(SavedCharacterApplyTimer);
        SavedCharacterApplyAttempts = 0;
        TWeakObjectPtr<USirrohUISubsystem> WeakThis(this);
        TWeakObjectPtr<UWorld> WeakWorld(World);
        FTimerDelegate RetryDelegate;
        RetryDelegate.BindLambda([WeakThis, WeakWorld]()
        {
                if (!WeakThis.IsValid() || !WeakWorld.IsValid())
                {
                        return;
                }
















                USirrohUISubsystem* Subsystem = WeakThis.Get();
                ++Subsystem->SavedCharacterApplyAttempts;
                Subsystem->ApplySavedCharacter(WeakWorld.Get());
                if (Subsystem->SavedCharacterApplyAttempts >= 50
                        && WeakWorld->GetTimerManager().IsTimerActive(Subsystem->SavedCharacterApplyTimer))
                {
                        WeakWorld->GetTimerManager().ClearTimer(Subsystem->SavedCharacterApplyTimer);
                        UE_LOG(LogSirrohUI, Warning, TEXT("Could not find a playable Pawn for the saved character."));
                }
        });
        World->GetTimerManager().SetTimer(SavedCharacterApplyTimer, RetryDelegate, 0.1f, true, 0.0f);
}



