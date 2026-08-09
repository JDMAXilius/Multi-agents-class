// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "Interfaces/OSLayerLayoutInterface.h"
#include "UI/OSUserInterfaceWidget.h"
#include "OSBaseLayerWidget.generated.h"

class AOSGameState;
class AOSPlayerController;
class UOSHUDWidget;
/** Base for HUD layers (killfeed, scoreboard, etc.). LayerLayout defines layout; UpdateStats() refreshes from game state. */
UCLASS()
class ONSIGHT_API UOSBaseLayerWidget : public UOSUserInterfaceWidget, public IOSLayerLayoutInterface
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	bool bAutoHide = false; //checks if the layer should be automatically shown when it's added to ActiveLayers.
	UFUNCTION(BlueprintCallable, BlueprintPure)
	UOSHUDWidget* GetHUD();
	
	// Used with HUD's LayerStack. If possible, will back out into a previous widget within the stack.
	// Will close the widget either way.
	UFUNCTION(BlueprintCallable)
	void Back();
	
	UFUNCTION()
	virtual void Init(UOSHUDWidget* HUDWidget);
	
	virtual void RebuildFromGameState() {}

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	
	
	AOSPlayerController* GetOSPC();

	UPROPERTY(Transient)
	UOSHUDWidget* HUD;
	
	virtual FOSLayerLayout GetLayerLayout_Implementation() const override {return LayerLayout;}
	
	UPROPERTY(EditDefaultsOnly)
	FOSLayerLayout LayerLayout;
	
	UPROPERTY()
	AOSGameState* _gameState;
	
};
