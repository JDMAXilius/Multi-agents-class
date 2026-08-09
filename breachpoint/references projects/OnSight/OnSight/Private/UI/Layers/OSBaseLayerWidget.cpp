// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Layers/OSBaseLayerWidget.h"

#include "Core/OSPlayerController.h"
#include "UI/OSHUDWidget.h"

UOSHUDWidget* UOSBaseLayerWidget::GetHUD()
{
	return HUD;
}

void UOSBaseLayerWidget::Back()
{
	// if (IsValid(HUD) && HUD->PopLayer())
	// {
	// 	return;
	// }

	if (AOSPlayerController* PC = GetOSPC())
	{
		//PC->CloseMenuDomain();
	}
}

void UOSBaseLayerWidget::Init( UOSHUDWidget* HUDWidget )
{
	HUD = HUDWidget;
}

void UOSBaseLayerWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (!GetWorld()) return;
	if (auto gs = GetWorld()->GetGameState())
		_gameState = Cast<AOSGameState>(gs);
	RebuildFromGameState();
}

void UOSBaseLayerWidget::NativeDestruct()
{
	Super::NativeDestruct();
}

AOSPlayerController* UOSBaseLayerWidget::GetOSPC()
{
	if (auto PC = GetOwningPlayer())
		return Cast<AOSPlayerController>(PC);
	return nullptr;
}