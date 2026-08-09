// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Layers/OSPauseLayerWidget.h"

#include "Core/OSGameInstance.h"
#include "Core/OSPlayerController.h"
#include "UI/OSHUDWidget.h"
#include "UI/Layers/OSSettingsLayerWidget.h"

void UOSPauseLayerWidget::OpenSettings()
{
	// if (HUD)
	// {
	// 	HUD->HideLayer(GetClass());
	// 	HUD->PushLayer(SettingsLayerClass, true);
	// }
}

void UOSPauseLayerWidget::ClosePauseMenu()
{
	if (auto PC = GetOSPC())
	{
		PC->CloseMenuDomain();
	}
}

void UOSPauseLayerWidget::Quit()
{
	UOSGameInstance* GI = GetGameInstance<UOSGameInstance>();
	if (!GI) return;

	// Host leaving mid-match: end the session cleanly and send all clients to menu together.
	// Client leaving: destroy their session slot and ClientTravel locally.
	APlayerController* PC = GetOwningPlayer();
	const bool bIsHost = PC && PC->HasAuthority();

	if (bIsHost)
	{ 
		UE_LOG(LogTemp, Log, TEXT("[OSPauseMenu] Host quit — EndMatchAndReturnToMenu (all clients travel together)"));
		GI->EndMatchAndReturnToMenu();
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[OSPauseMenu] Client quit — LeaveServer"));
		GI->LeaveServer();
	}
}
