#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Procedural_World_Generator/PWGv2Generator.h"
#include "PWGPlayerController.generated.h"

class APWGSidescrollerCamera;
class UInputMappingContext;

UCLASS(Blueprintable)
class SIRROH_REIMAGINEG_V3_API APWGPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
    UFUNCTION(Exec)
    void SirrohInteract();

    UFUNCTION(Exec, BlueprintCallable, Category="PWG|Movement")
    void SirrohParkour(bool bDrop = false);

	APWGPlayerController();

    /** Standalone integration comparison: 1 = native AST pawn input, 0 = Sirroh side-scroll. */
    UFUNCTION(Exec)
    void SirrohASTControls(bool bEnabled);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="PWG|Integration")
    bool bUseASTControls = false;
    UFUNCTION(Exec) void SirrohGroundCompare();
    UFUNCTION(Exec) void SirrohGroundReport();
    UFUNCTION(Exec) void SirrohPOIChain(int32 Index=0);
	virtual void PlayerTick(float DeltaTime) override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void AcknowledgePossession(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	UFUNCTION(BlueprintCallable, Category="PWG")
	void StartGameAfterCharacterCreation();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="PWG|Sidescroller")
	TSubclassOf<APWGSidescrollerCamera> SidescrollerCameraClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="PWG|Sidescroller", meta=(ClampMin="0.0"))
	float CameraBlendTime = 0.25f;

	/** Legacy offsets retained for existing Blueprint compatibility. Runtime framing is driven by VisibleGroundBelowCharacter. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="PWG|Sidescroller|Camera", meta=(Units="cm"))
	float SidescrollerCameraHeight = 230.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="PWG|Sidescroller|Camera", meta=(Units="cm"))
	float SidescrollerViewCentreHeight = 190.0f;

	/** Vertical screen position of the character's collision feet: 0 = top, 1 = bottom. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="PWG|Sidescroller|Camera",
		meta=(ClampMin="0.50", ClampMax="0.98", UIMin="0.50", UIMax="0.98"))
	float CharacterFeetScreenY = 0.82f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="PWG|Sidescroller|Camera", meta=(ClampMin="100.0", Units="cm"))
	float SidescrollerCameraDistance = 1840.0f;

	/** Horizontal width of the orthographic view. 1520 cm is roughly fifteen one-metre tiles. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="PWG|Sidescroller|Camera", meta=(ClampMin="400.0", Units="cm"))
	float SidescrollerOrthoWidth = 1520.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="PWG|Sidescroller|Camera", meta=(ClampMin="-30.0", ClampMax="30.0", Units="deg"))
	float SidescrollerCameraAngle = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="PWG|Terrain Lanes", meta=(ClampMin="0.05", ClampMax="2.0", Units="s"))
	float LaneSwitchDuration = 0.35f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="PWG|Terrain Lanes")
	EPWGTerrainLane CurrentTerrainLane = EPWGTerrainLane::Middle;

    /** Interaction reach in screen direction, depth and height (centimetres). */
    UPROPERTY(EditDefaultsOnly, Category="PWG|Integration", meta=(ClampMin="1.0", Units="cm"))
    FVector ASTInteractionReach = FVector(165.f,350.f,150.f);

private:
    bool bWasASTInteracting = false;
    bool TryProjectedASTLedge(bool Hop, bool ProbeOnly=false);
    double NextASTAirProbe=0;
    double LastASTDrop=-1000;
    double LastPlatformDownTap=-1000;
    double NextPlayerDepthUpdate=0;
    bool bManualDropNeedsGrab=false;
    bool bProjectedHopStarted=false;
    bool bProjectedHopSawBusy=false;
    void FinishProjectedHop();
    void TryDropThroughPlatform();

    void SyncASTInputContexts();
    bool TryASTSideInteraction();
    void HandleASTCombatInput(bool bAllowStart);
    AActor* FindASTSideInteraction();
    void UpdateASTInteractionPrompt();
    void SetASTInteractionPrompt(AActor* Target);
    TWeakObjectPtr<AActor> ASTPromptActor;
    double NextASTPromptUpdate=0;
    bool bOwnsASTPrompt=false;
    UPROPERTY(Transient) TArray<TObjectPtr<UInputMappingContext>> ASTOriginalContexts;
    UPROPERTY(Transient) TArray<TObjectPtr<UInputMappingContext>> PWGInteractionContexts;
    UPROPERTY(Transient) TArray<TObjectPtr<UInputMappingContext>> PWGClimbContexts;

    UPROPERTY(Transient) TObjectPtr<AActor> GroundComparisonPlatform;
    FVector GroundComparisonReturnLocation = FVector::ZeroVector;
    UFUNCTION(Server,Reliable) void ServerResetTrainingNPC();
    double PendingCombatKeyTime = -1;
    double LastDodgeLeft=-1000, LastDodgeRight=-1000;
    TWeakObjectPtr<APawn> CombatKeyPawn;
	UFUNCTION(Server, Reliable)
	void ServerStartGameAfterCharacterCreation();

	UPROPERTY(Transient)
	TObjectPtr<APWGSidescrollerCamera> SidescrollerCamera;

	void ConfigureSidescrollerPawn(APawn* InPawn);
	void SetPawnLaneConstraintY(APawn* InPawn, float WorldY) const;
	bool bLaneSwitching = false;
	float LaneSwitchElapsed = 0.0f;
	float LaneSwitchStartY = 0.0f;
	float LaneSwitchTargetY = 0.0f;
	EPWGTerrainLane PendingTerrainLane = EPWGTerrainLane::Middle;
};
