#pragma once


#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PWGGameMode.generated.h"


class APawn;
class APWGv2Generator;
class UDataTable;
class ACombatTrainingNPC;


UCLASS(Blueprintable)
class SIRROH_REIMAGINEG_V3_API APWGGameMode : public AGameModeBase
{
        GENERATED_BODY()


public:
        APWGGameMode();
        UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="PWG v2")
        bool bUsePWGv2 = true;
        UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="PWG v2")
        FString PWGv2WorldName = TEXT("FirstIsland");
        UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="PWG v2")
        int32 PWGv2Seed = 0;


        UFUNCTION(BlueprintCallable, Category="PWG")
        bool StartGameAfterCharacterCreation();


        UFUNCTION(BlueprintCallable, Category="PWG")
        bool RespawnPlayer(APlayerController* PlayerController);
        void ResetTrainingNPC();


        UFUNCTION(BlueprintPure, Category="PWG")
        APWGv2Generator* GetWorldGenerator() const { return WorldGenerator; }


        UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="PWG")
        TSubclassOf<APWGv2Generator> GeneratorClass;


        UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="PWG")
        TObjectPtr<UDataTable> BiomeTable;


        UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="PWG")
        FName BiomeRowName = TEXT("Meadow");

        /** Player Pawn class spawned after PWG world generation. */
        UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="PWG")
        TSubclassOf<APawn> PlayerPawnClass;


        UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="PWG")
        bool bStartAutomatically = true;

        /** Test an authored POI from PlayerStart with the configured PWG pawn and controller. */
        UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="PWG|POI Testing")
        bool bPOITestMode = false;

        virtual UClass* GetDefaultPawnClassForController_Implementation(AController* Controller) override;
        virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* Controller, const FTransform& Transform) override;


        UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="PWG", meta=(ClampMin="0.0"))
    float SpawnFadeDuration = 0.6f;


    UPROPERTY(EditDefaultsOnly, Category="PWG|Training")
    bool bSpawnCombatTrainingNPC = true;

    /** Temporary integration props beside the PWG spawn; disable after acceptance testing. */
    UPROPERTY(EditDefaultsOnly, Category="PWG|Integration")
    bool bSpawnASTIntegrationTestObjects = true;




    UPROPERTY(EditDefaultsOnly, Category="PWG|Training")
    float CombatTrainingSpawnOffset = 300;
    UPROPERTY(EditDefaultsOnly, Category="PWG|Training")
    TSoftClassPtr<APawn> ASTCombatNPCClass = TSoftClassPtr<APawn>(FSoftObjectPath(TEXT("/Game/Sirroh/Integration/BP_SirrohASTNPC.BP_SirrohASTNPC_C")));


protected:
        virtual void BeginPlay() override;


private:
    APawn* SpawnConfiguredPlayer(const FTransform& Transform);
        UPROPERTY(Transient)
    TObjectPtr<APWGv2Generator> WorldGenerator;


    UPROPERTY(Transient)
    TObjectPtr<APawn> CombatTrainingNPC;
    UPROPERTY(Transient) TObjectPtr<class APWGNavigationBounds> TrainingNavigationBounds;


        bool bGameStarted = false;
private:
    void SpawnASTIntegrationObjects(const FVector& PlayerLocation);
    UPROPERTY(Transient) TArray<TObjectPtr<AActor>> ASTIntegrationTestActors;
};

