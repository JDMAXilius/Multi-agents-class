// Breachpoint. The HUD C++ base: binding points, killfeed pooling, hit-marker dispatch.

#pragma once

#include "Blueprint/UserWidgetPool.h"
#include "CommonUserWidget.h"
#include "FieldNotificationId.h"
#include "UI/BRActivatableWidget.h"
#include "UI/BRUITypes.h"

#include "BRHUDLayout.generated.h"

class UPanelWidget;

/**
 * UBRKillfeedEntryWidget -- one killfeed row.
 *
 * The C++ class holds the DATA and the WBP subclass holds the LOOK. SetEntry() writes the
 * fields and fires one BlueprintImplementableEvent; the WBP's job is to read the getters and
 * play an animation. There is no branch in that graph, because there is no decision left to make
 * by the time it runs.
 *
 * NOT poolable via ICommonPoolableWidgetInterface on purpose: that interface is consumed by
 * CommonUI's own containers/WidgetFactory, and this row is pooled by an FUserWidgetPool owned by
 * UBRHUDLayout, which does not call it. Implementing it here would be decorative.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINT_API UBRKillfeedEntryWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** Called every time the row is claimed from the pool. Assume nothing persisted. */
	void SetEntry(const FBRKillfeedViewEntry& InEntry);

	/** By value, not by const ref: UHT does not accept reference return types on a UFUNCTION. */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Killfeed")
	FBRKillfeedViewEntry GetEntry() const { return Entry; }

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Killfeed")
	FText GetKillerName() const { return Entry.KillerName; }

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Killfeed")
	FText GetVictimName() const { return Entry.VictimName; }

	/** Empty is a supported, permanent state. Render the slot empty; never collapse the layout. */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Killfeed")
	FText GetSpotterLine() const { return Entry.SpotterLine; }

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Killfeed")
	bool IsLocalPlayerInvolved() const { return Entry.bLocalPlayerInvolved; }

protected:
	/** Visual refresh only. Everything needed is already on Entry. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Breachpoint|Killfeed", meta = (DisplayName = "On Entry Set"))
	void BP_OnEntrySet();

private:
	UPROPERTY(Transient)
	FBRKillfeedViewEntry Entry;
};

/**
 * UBRHUDLayout -- BREACHPOINT-ARCHITECTURE.md 3.9: "binds BP visual subclasses to the
 * ViewModels; killfeed widget pool; shield-vs-flesh hit markers".
 *
 * ============================ THE DIVISION OF LABOUR ============================
 *
 * C++ (here)                              WBP subclass (Tier 4, layout only)
 * -----------------------------------     ---------------------------------------------
 * resolves the two ViewModels             places bars, blocks, reticle, killfeed panel
 * subscribes / unsubscribes                MVVM-binds text and percents to VM getters
 * dispatches hit-marker events             plays one animation per BP_On* event
 * owns and recycles the killfeed pool      styles a row
 *
 * A branch on gameplay state in that right-hand column is a finding. If a WBP needs to decide
 * something, the decision belongs on the ViewModel and the WBP gets a field to bind to.
 *
 * ============================ HOW A VALUE GETS HERE ============================
 *
 * Two paths, both push, neither polls:
 *
 *   A. MVVM (values). The WBP's MVVM view binds a widget property to a ViewModel getter. When
 *      the ViewModel calls UE_MVVM_SET_PROPERTY_VALUE the field broadcasts and MVVM writes the
 *      widget. NativeOnActivated hands the view its ViewModels via UMVVMView::SetViewModel, so
 *      the binding is correct for THIS local player rather than for a global.
 *
 *      In the MVVM editor panel, bindings must be set to execute on the field's change - NOT
 *      "Tick". UMVVMView has an ExecuteTickBindings() path and choosing it re-introduces the
 *      per-frame poll under a different name. That is the modern shape of the old UMG property
 *      binding and the rung-2 grep gate should be taught to look for it.
 *
 *   B. Native delegates (events). Hit markers and the killfeed arrive on plain C++ multicasts and
 *      FieldNotify subscriptions taken here, and are re-dispatched to the WBP as
 *      BlueprintImplementableEvents. Events do not belong in a value binding.
 *
 * ============================ JOIN-IN-PROGRESS ============================
 *
 * Everything below tolerates null. GetCombatViewModel() can be null, its ASC can be unbound, and
 * its VitalsState can be Unknown for many frames after a mid-match join. The WBP switches on
 * VitalsState / MatchState and renders dashes; it never renders a confident 0/100. A stale or
 * garbage first frame is a finding, because it tells the player something false about a fight
 * they are already in.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINT_API UBRHUDLayout : public UBRActivatableWidget
{
	GENERATED_BODY()

public:
	UBRHUDLayout(const FObjectInitializer& ObjectInitializer);

protected:
	//~ UUserWidget
	virtual void NativeOnInitialized() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	//~ End UUserWidget

	//~ UBRActivatableWidget
	virtual void BindViewModels() override;
	virtual void UnbindViewModels() override;
	//~ End UBRActivatableWidget

	// -----------------------------------------------------------------------
	// BINDING POINTS -- what the WBP subclass must supply
	// -----------------------------------------------------------------------

	/**
	 * The panel killfeed rows are parented to (a VerticalBox in practice).
	 *
	 * BindWidgetOptional, not BindWidget: a HUD variant without a killfeed is legitimate (a
	 * training range), and hard-failing a WBP compile over it would push someone toward a
	 * duplicate HUD class, which is worse.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|HUD")
	TObjectPtr<UPanelWidget> KillfeedContainer;

	// -----------------------------------------------------------------------
	// HIT MARKERS -- one event per kind, so the WBP picks a visual and nothing else
	// -----------------------------------------------------------------------

	UFUNCTION(BlueprintImplementableEvent, Category = "Breachpoint|HUD|Hit Markers", meta = (DisplayName = "On Shield Hit"))
	void BP_OnShieldHit();

	UFUNCTION(BlueprintImplementableEvent, Category = "Breachpoint|HUD|Hit Markers", meta = (DisplayName = "On Flesh Hit"))
	void BP_OnFleshHit();

	UFUNCTION(BlueprintImplementableEvent, Category = "Breachpoint|HUD|Hit Markers", meta = (DisplayName = "On Headshot Hit"))
	void BP_OnHeadshotHit();

	UFUNCTION(BlueprintImplementableEvent, Category = "Breachpoint|HUD|Hit Markers", meta = (DisplayName = "On Kill Confirmed"))
	void BP_OnKillConfirmed();

	// -----------------------------------------------------------------------
	// HONEST EMPTY STATES -- the WBP switches its whole vitals/match block on these
	// -----------------------------------------------------------------------

	UFUNCTION(BlueprintImplementableEvent, Category = "Breachpoint|HUD", meta = (DisplayName = "On Vitals State Changed"))
	void BP_OnVitalsStateChanged(EBRUIDataState NewState);

	UFUNCTION(BlueprintImplementableEvent, Category = "Breachpoint|HUD", meta = (DisplayName = "On Match State Changed"))
	void BP_OnMatchStateChanged(EBRUIDataState NewState);

	/** Fires after the killfeed rows have been rebuilt, in case the WBP animates the panel. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Breachpoint|HUD", meta = (DisplayName = "On Killfeed Rebuilt"))
	void BP_OnKillfeedRebuilt(int32 NumRows);

private:
	void HandleHitMarker(EBRHitMarkerKind Kind);
	void HandleKillfeedChanged();
	void HandleViewModelFieldChanged(UObject* Source, UE::FieldNotification::FFieldId FieldId);

	void RebuildKillfeed();
	void PushViewModelsIntoMVVMView();

	/**
	 * The killfeed widget pool. Sized to BRUISettings.KillfeedMaxVisibleEntries, claimed and
	 * released, never CreateWidget-per-kill.
	 *
	 * A firefight is exactly when the killfeed is busiest and exactly when a frame spike is least
	 * affordable, so the allocation happens once and the rows are recycled forever after. Held as
	 * a UPROPERTY because FUserWidgetPool is a reflected struct - that is what keeps the pooled
	 * widgets alive through GC (the same thing CommonUI's own containers do).
	 */
	UPROPERTY(Transient)
	FUserWidgetPool KillfeedPool;

	/** Resolved once from the SOFT class in BRUISettings. Not a hard reference in code. */
	UPROPERTY(Transient)
	TSubclassOf<UBRKillfeedEntryWidget> ResolvedKillfeedEntryClass;

	/** FieldNotify subscriptions, one release site each (UnbindViewModels). */
	FDelegateHandle VitalsStateFieldHandle;
	FDelegateHandle MatchStateFieldHandle;
};
