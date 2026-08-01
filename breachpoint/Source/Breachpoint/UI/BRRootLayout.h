// Breachpoint. The per-local-player widget that HOSTS the four CommonUI layer stacks.

#pragma once

#include "CommonUserWidget.h"
#include "Templates/SubclassOf.h"
#include "UI/BRUITypes.h"

#include "BRRootLayout.generated.h"

class UBRActivatableWidget;
class UCommonActivatableWidget;
class UCommonActivatableWidgetStack;

/**
 * UBRRootLayout -- the single widget that owns the four CommonUI layer stacks for one local
 * player. Everything else in the game's UI is pushed onto one of its stacks; nothing is ever
 * added to the viewport directly.
 *
 * ================= WHY THIS CLASS EXISTS AT ALL (skill correction) =================
 *
 * `ue5-ui-architecture` 1 says "a UGameInstanceSubsystem OWNING the CommonUI layer stack". That
 * is not possible as written in stock UE 5.8, and this is the single biggest correction this
 * packet makes to the skill:
 *
 *   - `UCommonActivatableWidgetStack` (CommonUI/Public/Widgets/CommonActivatableWidgetContainer.h)
 *     is a `UWidget`. A UWidget must live in a widget tree; a UObject subsystem cannot own or
 *     render one.
 *   - The class people remember owning layers - `UPrimaryGameLayout`, with
 *     `UGameUIManagerSubsystem` / `UGameUIPolicy` / `UCommonUIExtensions` - is NOT in the engine.
 *     It ships in Lyra's `CommonGame` plugin. Verified against this machine's engine:
 *     D:\Program Files\UE_5.8_Source\Engine\Plugins\ has no CommonGame directory, and the
 *     CommonUI plugin's entire Widgets/ folder is one header containing
 *     ContainerBase / Stack / Queue.
 *
 * So the spine is: BRUIManagerSubsystem (a UObject, per ARCHITECTURE 3.9) owns a UBRRootLayout
 * PER LOCAL PLAYER, the root layout hosts the stacks, and the subsystem pushes THROUGH it. The
 * subsystem is still the only public entry point, so callers see exactly the API the skill
 * describes - only the ownership underneath is real.
 *
 * ================= WHAT THE WBP SUBCLASS MUST SUPPLY =================
 *
 * Four `UCommonActivatableWidgetStack` widgets, named EXACTLY as the members below (BindWidget
 * is a hard requirement - the WBP will not compile without them), stacked in this Z order:
 *
 *     GameLayerStack      (bottom)  HUD
 *     GameMenuLayerStack            in-match overlays
 *     MenuLayerStack                front end
 *     ModalLayerStack     (top)     confirmations
 *
 * The WBP holds LAYOUT ONLY: an Overlay, four stacks, nothing else. No graph, no variables.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINT_API UBRRootLayout : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** The stack registered for a UI.Layer.* tag, or null if the tag is not a layer. */
	UCommonActivatableWidgetStack* GetLayerStack(FUITag LayerTag) const;

	/**
	 * Push a widget class onto a layer.
	 *
	 * The stack POOLS instances internally (FUserWidgetPool inside
	 * UCommonActivatableWidgetContainerBase), so the returned pointer must not be cached across
	 * pushes - the engine's own comment says the same thing. Establish state every time.
	 */
	UBRActivatableWidget* PushWidgetToLayer(FUITag LayerTag, TSubclassOf<UBRActivatableWidget> WidgetClass);

	/**
	 * Remove a widget from its layer.
	 *
	 * Prefer DeactivateWidget() on the widget itself: a stack automatically removes and pops to
	 * the widget beneath when its top entry deactivates, and THAT is the path that restores focus
	 * for gamepad back-navigation. This function exists for forced teardown (travel), not for
	 * normal navigation.
	 */
	void RemoveWidgetFromLayer(FUITag LayerTag, UBRActivatableWidget* Widget);

	/** Force-clear a layer. Travel and match-end only. */
	void ClearLayer(FUITag LayerTag);

	UCommonActivatableWidget* GetActiveWidgetOnLayer(FUITag LayerTag) const;

protected:
	//~ UUserWidget
	virtual void NativeOnInitialized() override;
	//~ End UUserWidget

private:
	void RegisterLayer(FUITag LayerTag, UCommonActivatableWidgetStack* Stack);

	/** UI.Layer.Game -- the HUD. Always the bottom of the pile. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
	TObjectPtr<UCommonActivatableWidgetStack> GameLayerStack;

	/** UI.Layer.GameMenu -- scoreboard, death overlay, carnage report. Game is still live. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
	TObjectPtr<UCommonActivatableWidgetStack> GameMenuLayerStack;

	/** UI.Layer.Menu -- front end. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
	TObjectPtr<UCommonActivatableWidgetStack> MenuLayerStack;

	/** UI.Layer.Modal -- confirmations and errors. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
	TObjectPtr<UCommonActivatableWidgetStack> ModalLayerStack;

	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<UCommonActivatableWidgetStack>> LayerStacks;
};
