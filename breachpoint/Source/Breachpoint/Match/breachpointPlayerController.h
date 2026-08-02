#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "breachpointPlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;

UCLASS(abstract, config="Game")
class BREACHPOINT_API AbreachpointPlayerController : public APlayerController
{
	GENERATED_BODY()

public:

	AbreachpointPlayerController();

protected:

	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	virtual void BeginPlay() override;

	virtual void SetupInputComponent() override;

	bool ShouldUseTouchControls() const;
};
