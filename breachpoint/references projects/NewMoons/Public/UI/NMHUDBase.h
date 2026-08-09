// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "NMHUDBase.generated.h"

class UUserWidget;

UCLASS(Abstract)
class NEWMOONS_API ANMHUDBase : public AHUD
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable)
	void ShowInGameMenu();

	UFUNCTION(BlueprintCallable)
	void HideInGameMenu();

	UFUNCTION(BlueprintCallable)
	void ShowMainMenu();

	UFUNCTION(BlueprintCallable)
	void HideMainMenu();

	UFUNCTION(BlueprintCallable)
	void ToggleMainMenu();

	UFUNCTION(BlueprintCallable)
	void ShowVictoryUI();
	UFUNCTION(BlueprintCallable)
	void ShowDefeatUI();

protected:
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UUserWidget> InGameMenuClass;

	UPROPERTY()
	TObjectPtr<UUserWidget> InGameMenu;

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UUserWidget> MainMenuClass;

	UPROPERTY()
	TObjectPtr<UUserWidget> MainMenu;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UUserWidget> VictoryScreenClass;

	UPROPERTY()
	TObjectPtr<UUserWidget> VictoryScreen;
	
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UUserWidget> DefeatScreenClass;

	UPROPERTY()
	TObjectPtr<UUserWidget> DefeatScreen;
	
	virtual void BeginPlay() override;
	void ShowWidget(TObjectPtr<UUserWidget>& Widget, const TSubclassOf<UUserWidget>& WidgetClass);
	void HideWidget(TObjectPtr <UUserWidget>& Widget);
};
