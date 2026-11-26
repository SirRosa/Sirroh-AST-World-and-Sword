#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SirrohDebugSubsystem.generated.h"
class SSirrohDebugPanel;
class APlayerController;
UCLASS()
class SIRROH_REIMAGINEG_V3_API USirrohDebugSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    void Toggle(APlayerController* PC);
    void Close();
    bool IsOpen() const { return bOpen; }
    bool IsPointerOverMenu() const;
    bool IsEditingText() const;
    virtual void Deinitialize() override;
    UObject* KeepCopy(UObject* Source);
private:
    bool bOpen=false;
    FDelegateHandle MapHandle;
    TWeakObjectPtr<class UGameViewportClient> Viewport;
    TWeakObjectPtr<APlayerController> Controller;
    TSharedPtr<SSirrohDebugPanel> Panel;
    UPROPERTY(Transient) TArray<TObjectPtr<UObject>> Copies;
};
