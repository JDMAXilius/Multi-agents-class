#include "UI/OSHUDWidget.h"
#include "Components/ProgressBar.h"
#include "Components/CanvasPanelSlot.h"
#include "Core/OSGameState.h"
#include "Core/OSPlayerState.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
#include "OSLogCategories.h"
#include "Interfaces/OSLayerLayoutInterface.h"
#include "UI/Components/OSWinConditionComponentWidget.h"
#include "UI/Components/OSWinConditionInfoComponentWidget.h"
#include "UI/Layers/OSBaseLayerWidget.h"
#include "CommonActivatableWidget.h"
#include "CommonUI/Public/Widgets/CommonActivatableWidgetContainer.h"
#include "UI/OSAttributeWidget.h"
#include "UI/Layers/OSPauseLayerWidget.h"
#include "UI/Layers/OSScoreboardLayerWidget.h"

void UOSHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// WIP bridge: auto-resolve the root widget as any panel (Overlay, CanvasPanel, etc.).
	// If the root isn't a panel, call InitLayerRoot(Panel) explicitly from the BP.
	if (!LayerRootCached)
		LayerRootCached = Cast<UPanelWidget>(GetRootWidget());
	RefreshModeLayers();
}

void UOSHUDWidget::NativeDestruct()
{
	Super::NativeDestruct();

	// Unbind PlayerState delegates
	if (const UWorld* World = GetWorld())
	{
		// Unbind GameState delegates so they do not fire after widget is destroyed
		if (AOSGameState* GS = World->GetGameState<AOSGameState>())
		{
			GS->OnWinConditionChanged.RemoveAll(this);
			GS->OnMatchLeaderChanged.RemoveAll(this);
			GS->OnMatchResultChanged.RemoveAll(this);
		}
	}
}

void UOSHUDWidget::OnGASReady_Implementation(UAbilitySystemComponent* ASC, UOSAttributeSet* AS)
{
	if (AttributeWidget)
	{
		AttributeWidget->NotifyGASReady();
	}

	if (GetOwningGameState())
	{
		OwningGameState->OnMatchResultChanged.AddUObject(this, &UOSHUDWidget::OnMatchResultChanged);
	}
}

void UOSHUDWidget::OnMatchLeaderChanged( const FOSMatchLeader& leader, const int32 targetValue )
{
	if (!LeaderWinConditionInfoComponentWidget) return;
	LeaderWinConditionInfoComponentWidget->UpdateSelf(leader.LeaderValue, targetValue, leader.GetPlayerLabel());
}

void UOSHUDWidget::ShowPause_Implementation()
{
	if (PauseWidget)
	{
		PauseWidget->SetVisibility(ESlateVisibility::Visible);
	}
}

void UOSHUDWidget::HidePause()
{
	if (PauseWidget)
	{
		PauseWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UOSHUDWidget::ShowScoreboard()
{
	if (GetOwningGameState())
	{
		FString Name = OwningGameState->GameModeClass ? OwningGameState->GameModeClass->GetName() : TEXT("None");

		if (Name.Contains(TEXT("FFA")))
		{
			if (ScoreboardWidget_FFA)
			{
				ScoreboardWidget_FFA->SetVisibility(ESlateVisibility::Visible);
			}
		}
		else
		{
			if (ScoreboardWidget_Teams)
			{
				ScoreboardWidget_Teams->SetVisibility(ESlateVisibility::Visible);
			}
		}
	}
}

void UOSHUDWidget::HideScoreboard()
{
	if (GetOwningGameState())
	{
		FString Name = OwningGameState->GameModeClass ? OwningGameState->GameModeClass->GetName() : TEXT("None");

		if (Name.Contains(TEXT("FFA")))
		{
			if (ScoreboardWidget_FFA)
			{
				ScoreboardWidget_FFA->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
		else
		{
			if (ScoreboardWidget_Teams)
			{
				ScoreboardWidget_Teams->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
	}
}

void UOSHUDWidget::InitWidget(UCommonActivatableWidgetStack* InMenuStack, UOSAttributeWidget* InAttributeWidget, UOSPauseLayerWidget* InPauseWidget, UOSScoreboardLayerWidget* InScoreboardWidget_Teams, UOSScoreboardLayerWidget* InScoreboardWidget_FFA)
{
	if (!bInitializedStack)
	{
		MenuStack = InMenuStack;
		AttributeWidget = InAttributeWidget;
		PauseWidget = InPauseWidget;
		ScoreboardWidget_Teams = InScoreboardWidget_Teams;
		ScoreboardWidget_FFA = InScoreboardWidget_FFA;
		bInitializedStack = true;
	}
}

// --- WIP bridge: mode-layer system ---
// Consumes ModeLayerClasses from the GameMode for mode-specific overlays.

void UOSHUDWidget::InitLayerRoot(UPanelWidget* InLayerRoot)
{
	LayerRootCached = InLayerRoot;
	RefreshModeLayers();
}

UUserWidget* UOSHUDWidget::AddLayer(TSubclassOf<UUserWidget> LayerClass)
{
	if (!LayerClass || !LayerRootCached) return nullptr;

	if (const auto Found = ActiveLayers.Find(LayerClass))
		return Found->Get();

	APlayerController* PC = GetOwningPlayer();
	if (!PC) return nullptr;

	UOSBaseLayerWidget* Layer = CreateWidget<UOSBaseLayerWidget>(PC, LayerClass);
	if (!Layer) return nullptr;

	LayerRootCached->AddChild(Layer);
	ActiveLayers.Add(LayerClass, Layer);

	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Layer->Slot))
	{
		if (Layer->GetClass()->ImplementsInterface(UOSLayerLayoutInterface::StaticClass()))
		{
			const FOSLayerLayout L = IOSLayerLayoutInterface::Execute_GetLayerLayout(Layer);
			if (L.bOverride)
			{
				CanvasSlot->SetAnchors(L.Anchors);
				CanvasSlot->SetAlignment(L.Alignment);
				CanvasSlot->SetOffsets(L.Offsets);
				CanvasSlot->SetAutoSize(L.bAutoSize);
				CanvasSlot->SetZOrder(L.ZOrder);
			}
		}
	}

	Layer->Init(this);
	if (Layer->bAutoHide)
		Layer->SetVisibility(ESlateVisibility::Collapsed);

	return Layer;
}

void UOSHUDWidget::RefreshModeLayers()
{
	if (!LayerRootCached)
	{
		UE_LOG(LogOSDomination, Warning, TEXT("[HUD] RefreshModeLayers: LayerRootCached is null"));
		return;
	}

	if (const AOSGameState* GS = GetWorld() ? GetWorld()->GetGameState<AOSGameState>() : nullptr)
	{
		for (const TSubclassOf<UOSBaseLayerWidget>& LayerClass : GS->ModeLayerClasses)
		{
			if (LayerClass) AddLayer(LayerClass);
		}
	}
}

void UOSHUDWidget::OpenMenu(TSubclassOf<UCommonActivatableWidget> MenuClass)
{
	if (MenuStack && MenuClass)
	{
		if (UCommonActivatableWidget* MenuWidget = MenuStack->AddWidget(MenuClass))
		{
			MenuWidget->ActivateWidget();
			UE_LOG(LogTemp, Warning, TEXT("OS Widgets - Opening Menu: %s"), *MenuWidget->GetName());
		}
	}
}

void UOSHUDWidget::OnMatchResultChanged( const FOSMatchResult& matchResult )
{
	if (!matchResult.bGameEnded || !WinConditionScreen) return;
	WinConditionScreen->SetVisibility(ESlateVisibility::Visible);

	// TDM: Winner is null, WinnerValue = WinningTeamIndex (0/1) or -1 for draw.
	//AOSGameState* GS = Cast<AOSGameState>(GrabGameState());
	if (GetOwningGameState())
	{
		FString WinnerName;
		int32 DisplayValue = matchResult.WinnerValue;
		if (matchResult.Winner)
		{
			WinnerName = matchResult.Winner->GetPlayerName();
		}
		else if (OwningGameState && matchResult.WinnerValue >= 0 && matchResult.WinnerValue < AOSGameState::MaxTDMTeams)
		{
			WinnerName = FString::Printf(TEXT("Team %d"), matchResult.WinnerValue + 1);
			DisplayValue = OwningGameState->GetTeamKills(matchResult.WinnerValue);
		}
		else
		{
			WinnerName = TEXT("Draw");
		}
		WinConditionScreen->Print(WinnerName, DisplayValue);
	}
}