#pragma once


#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Character/SirrohCharacterSaveGame.h"
#include "Styling/SlateBrush.h"
#include "SirrohUISubsystem.generated.h"


class SWidget;
class SSirrohUIRoot;
class UAudioComponent;
class USoundWave;
class UTexture2D;
class UTextureRenderTarget2D;
class UMaterialInstanceDynamic;
class UWorld;
class USceneCaptureComponent2D;
class USirrohCharacterIdentityComponent;
class AActor;
struct FSirrohUIResources;


/**
 * Owns the game's top-level Slate UI. The subsystem is created automatically
 * for every game instance, so no Blueprint GameInstance is required.
 */
UCLASS()
class SIRROH_REIMAGINEG_V3_API USirrohUISubsystem : public UGameInstanceSubsystem
{
        GENERATED_BODY()


public:
        virtual void Initialize(FSubsystemCollectionBase& Collection) override;
        virtual void Deinitialize() override;
        void TogglePauseMenu(APlayerController* PlayerController);


private:
        void HandlePostLoadMap(UWorld* LoadedWorld);
        void ShowMainMenu(UWorld* MenuWorld);
        void HideMainMenu();
        void ApplyMenuInputMode(UWorld* MenuWorld);
        void OpenDefaultLevel();
        bool CreateCharacter(const FSirrohCharacterProfile& Profile);
        void LoadCharacter();
        void ShowWorldPicker();
        void HideWorldPicker();
        void UpdatePreviewProfile(const FSirrohCharacterProfile& Profile);
        void SetCreatorView(bool bFace);
        void CreateCharacterPreview(UWorld* MenuWorld);
        void DestroyCharacterPreview();
        void ApplySavedCharacter(UWorld* World);
        void ScheduleSavedCharacterApply(UWorld* World);
        int32 CountHeads() const;
        void QuitGame();
        bool LoadUITextures();
        void StartMenuMusic(UWorld* MenuWorld);
        void ShowPauseMenu(APlayerController* PlayerController);
        void HidePauseMenu(bool bUnpauseGame = true);
        void RespawnFromPause();
        void ReturnToMainMenuFromPause();


        UPROPERTY(Transient)
        TObjectPtr<UTexture2D> FontTexture;


        UPROPERTY(Transient)
        TObjectPtr<UTexture2D> BattlefieldTexture;


        UPROPERTY(Transient)
        TObjectPtr<UTexture2D> BloodMoonTexture;


        UPROPERTY(Transient)
        TObjectPtr<UTexture2D> TitleTexture;


        UPROPERTY(Transient)
        TObjectPtr<UTexture2D> NormalFrameTexture;


        UPROPERTY(Transient)
        TObjectPtr<UTexture2D> RedFrameTexture;


        UPROPERTY(Transient)
        TObjectPtr<UTexture2D> PanoramaTexture;


        UPROPERTY(Transient)
        TObjectPtr<UTexture2D> DecoTexture;


        UPROPERTY(Transient)
        TObjectPtr<UTexture2D> OtherDecoTexture;


        UPROPERTY(Transient)
        TObjectPtr<UTexture2D> SliderHeadTexture;


        UPROPERTY(Transient)
        TObjectPtr<UTexture2D> SliderLineTexture;


        UPROPERTY(Transient)
        TObjectPtr<UTexture2D> CharacterFrameTexture;


        UPROPERTY(Transient)
        TObjectPtr<UTexture2D> SmallSquareTexture;


        UPROPERTY(Transient)
        TObjectPtr<UTexture2D> SmallSquareSelectedTexture;

        UPROPERTY(Transient)
        TObjectPtr<UTexture2D> WorldDeleteTexture;


        UPROPERTY(Transient)
        TObjectPtr<UTextureRenderTarget2D> CharacterRenderTarget;
        UPROPERTY(Transient)
        TObjectPtr<UMaterialInstanceDynamic> CharacterPreviewMaterial;


        UPROPERTY(Transient)
        TObjectPtr<AActor> PreviewCharacter;


        UPROPERTY(Transient)
        TObjectPtr<AActor> PreviewCaptureActor;


        UPROPERTY(Transient)
        TObjectPtr<USceneCaptureComponent2D> PreviewCaptureComponent;


        UPROPERTY(Transient)
        TObjectPtr<USirrohCharacterIdentityComponent> PreviewIdentityComponent;


        UPROPERTY(Transient)
        TObjectPtr<USoundWave> MainMenuMusic;


        UPROPERTY(Transient)
        TObjectPtr<UAudioComponent> MenuMusicComponent;


        TSharedPtr<FSirrohUIResources> Resources;
        TSharedPtr<SSirrohUIRoot> RootWidget;
        TSharedPtr<SWidget> ViewportWidget;
        TSharedPtr<SWidget> PauseWidget;
        TSharedPtr<SWidget> WorldPickerWidget;
        TWeakObjectPtr<APlayerController> PausedPlayerController;
        FSlateBrush CharacterPreviewBrush;
        FDelegateHandle PostLoadMapHandle;
        FTimerHandle SavedCharacterApplyTimer;
        int32 SavedCharacterApplyAttempts = 0;
        bool bGameplayLaunchAuthorized = false;
        bool bRedirectingToMainMenu = false;
        FVector PreviewFullBodyLocation = FVector::ZeroVector;
};
