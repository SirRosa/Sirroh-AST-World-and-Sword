#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "POIPushTraversalComponent.generated.h"

UCLASS(ClassGroup=(POI),meta=(BlueprintSpawnableComponent))
class SIRROH_REIMAGINEG_V3_API UPOIPushTraversalComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UPOIPushTraversalComponent();
 virtual void BeginPlay() override;
 virtual void TickComponent(float Dt,ELevelTick Type,FActorComponentTickFunction* Tick) override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 static void ProtectTarget(AActor* Climber);
 void RefreshEdges();
private:
 void Release();
 TWeakObjectPtr<AActor> Climber;
 FQuat LastRotation=FQuat::Identity;
 bool bEdgesReady=false;
};
