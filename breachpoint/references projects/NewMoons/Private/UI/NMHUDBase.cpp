// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/NMHUDBase.h"
#include "Blueprint/UserWidget.h"

void ANMHUDBase::BeginPlay()
{
	Super::BeginPlay();
}

void ANMHUDBase::ShowWidget(TObjectPtr<UUserWidget>& Widget, const TSubclassOf<UUserWidget>& WidgetClass)
{
	if (!Widget.IsNull() || !IsValid(WidgetClass)) return;

	if (APlayerController* PlayerController = GetOwningPlayerController())
	{
		// Make widget owned by our PlayerController
		Widget = CreateWidget<UUserWidget>(PlayerController, WidgetClass);
		Widget->AddToViewport();
	}
}

void ANMHUDBase::HideWidget(TObjectPtr<UUserWidget>& Widget)
{
	if (!Widget.IsNull())
	{
		Widget->RemoveFromViewport();
		Widget = nullptr;
	}
}

void ANMHUDBase::ShowInGameMenu()
{
	ShowWidget(InGameMenu, InGameMenuClass);
}

void ANMHUDBase::HideInGameMenu()
{
	HideWidget(InGameMenu);
}

void ANMHUDBase::ShowMainMenu()
{
	ShowWidget(MainMenu, MainMenuClass);
}

void ANMHUDBase::HideMainMenu()
{
	HideWidget(MainMenu);
}

void ANMHUDBase::ToggleMainMenu()
{
	MainMenu.IsNull() ? ShowMainMenu() : HideMainMenu();
}

void ANMHUDBase::ShowVictoryUI()
{
	ShowWidget(VictoryScreen, VictoryScreenClass);
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("Victory Screen!"));
}

void ANMHUDBase::ShowDefeatUI()
{
	ShowWidget(DefeatScreen, DefeatScreenClass);
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("Defeat Screen!"));
}
