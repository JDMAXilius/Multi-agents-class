#pragma once

#include "CoreMinimal.h"
#include "OSCommonUserWidget.h"
#include "Components/CanvasPanel.h"
#include "UI/OSUserInterfaceWidget.h"
#include "Components/ProgressBar.h"
#include "Components/HorizontalBox.h"
#include "Components/TextBlock.h"
#include "Data/OSHitDamageContext.h"
#include "Layers/OSKillFeedLayerWidget.h"
#include "OSHUDWidget.generated.h"

class UOSAttributeWidget;
class UCommonActivatableWidget;
class UCommonActivatableWidgetStack;
class UAkAudioEvent;
class UOSWinConditionLayerWidget;
class UOSTargetMarkerLayerWidget;
class UOSBaseLayerWidget;
class UOSPauseLayerWidget;
class UOSWinConditionComponentWidget;
class UOSWinConditionInfoComponentWidget;
class AOSPlayerState;
class APlayerController;
class APawn;
class UOSScoreboardLayerWidget;
struct FOSStats;
/**
 * Main HUD Widget for displaying player attributes
 * Shows Health, Stamina, and Aura with progress bars
 */
struct FOnAttributeChangeData;
struct FGameplayEventData;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnTargetChanged, AActor*);
UCLASS()
class ONSIGHT_API UOSHUDWidget : public UOSCommonUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION()
	void OnMatchLeaderChanged( const FOSMatchLeader& Leader, const int32 targetValue );

	UFUNCTION(BlueprintNativeEvent)
	void ShowPause();
	void HidePause();

	void ShowScoreboard();
	void HideScoreboard();

	UPROPERTY()
	TObjectPtr<UCommonActivatableWidgetStack> MenuStack = nullptr;
	UCommonActivatableWidgetStack* GetMenuStack() const { return MenuStack; }
	
	UFUNCTION(BlueprintCallable, Category = "OSMenuStackrWidget")
	void OpenMenu(TSubclassOf<UCommonActivatableWidget> MenuClass);

	UPROPERTY()
	TObjectPtr<UOSAttributeWidget> AttributeWidget = nullptr;

	UPROPERTY()
	TObjectPtr<UOSPauseLayerWidget> PauseWidget = nullptr;

	UPROPERTY()
	TObjectPtr<UOSScoreboardLayerWidget> ScoreboardWidget_Teams = nullptr;

	UPROPERTY()
	TObjectPtr<UOSScoreboardLayerWidget> ScoreboardWidget_FFA = nullptr;

protected:
	virtual void OnGASReady_Implementation(UAbilitySystemComponent* ASC, UOSAttributeSet* AS) override;
	
	// Win Condition UI. Shows current progress to how to win a gamemode
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UOSWinConditionInfoComponentWidget> LeaderWinConditionInfoComponentWidget;
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UOSWinConditionComponentWidget> WinConditionScreen;

	UFUNCTION()
	virtual void OnMatchResultChanged(const FOSMatchResult& matchResult);

	UFUNCTION(BlueprintCallable)
	void InitWidget(UCommonActivatableWidgetStack* InMenuStack, UOSAttributeWidget* InAttributeWidget, UOSPauseLayerWidget* InPauseWidget, UOSScoreboardLayerWidget* InScoreboardWidget_Teams, UOSScoreboardLayerWidget* InScoreboardWidget_FFA);

	// --- WIP bridge: mode-layer system ---
	// Temporary consumer for ModeLayerClasses from the GameMode until Common UI exposes a dedicated mode-layer path.
	// NOTE: LayerRoot is resolved at runtime via GetRootWidget() cast rather than
	// a BindWidgetOptional — the W_OSHUD BP already declares its own LayerRoot
	// variable and the UPROPERTY name would collide on compile.
	// Typed as UPanelWidget (not UCanvasPanel) so Overlay, CanvasPanel, or any
	// other panel type can serve as the layer container. CanvasPanel-specific
	// layout overrides still apply when the slot permits.
	UPROPERTY(Transient)
	TObjectPtr<UPanelWidget> LayerRootCached;

	UPROPERTY(Transient)
	TMap<TSubclassOf<UUserWidget>, TObjectPtr<UUserWidget>> ActiveLayers;

	UUserWidget* AddLayer(TSubclassOf<UUserWidget> LayerClass);
	void RefreshModeLayers();

	// Called by the W_OSHUD BP from its Event Construct, passing its own LayerRoot
	// canvas. Caches the panel and triggers mode-layer population. Using an explicit
	// setter keeps the link refactor-safe (renaming the BP variable won't silently
	// break the binding, unlike a by-name lookup).
	UFUNCTION(BlueprintCallable, Category="HUD|Layers")
	void InitLayerRoot(UPanelWidget* InLayerRoot);

private:
	bool bInitializedStack = false;
};

