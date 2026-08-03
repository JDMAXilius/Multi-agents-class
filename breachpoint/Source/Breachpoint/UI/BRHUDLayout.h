#pragma once

#include "CommonUserWidget.h"
#include "FieldNotificationId.h"
#include "UI/BRActivatableWidget.h"
#include "UI/BRUITypes.h"

#include "BRHUDLayout.generated.h"

/**
 * `UBRKillfeedEntryWidget` -- ONE killfeed row. Declared here for history; it is driven
 * exclusively by `UBRKillfeed` (`UI/HUD/BRKillfeed.h`), which owns the pool that holds it.
 * This layout no longer projects the feed -- see the `UBRHUDLayout` comment.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINT_API UBRKillfeedEntryWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	void SetEntry(const FBRKillfeedViewEntry& InEntry);

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Killfeed")
	FBRKillfeedViewEntry GetEntry() const { return Entry; }

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Killfeed")
	FText GetKillerName() const { return Entry.KillerName; }

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Killfeed")
	FText GetVictimName() const { return Entry.VictimName; }

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Killfeed")
	FText GetSpotterLine() const { return Entry.SpotterLine; }

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Killfeed")
	bool IsLocalPlayerInvolved() const { return Entry.bLocalPlayerInvolved; }

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Breachpoint|Killfeed", meta = (DisplayName = "On Entry Set"))
	void BP_OnEntrySet();

private:
	UPROPERTY(Transient)
	FBRKillfeedViewEntry Entry;
};

/**
 * `UBRHUDLayout` -- the in-game HUD frame. It hosts the surfaces and routes ViewModels to
 * them; it renders no feed, bar or number itself.
 *
 * THE KILLFEED IS NOT HERE, DELIBERATELY. This class used to carry a second killfeed
 * (`KillfeedContainer` + an inline `FUserWidgetPool` + `RebuildKillfeed`) that projected the
 * SAME `UBRVM_Match` ring as `UBRKillfeed` (`UI/HUD/BRKillfeed.h`). Two projections of one
 * array is one feed drawn twice the moment a WBP hosts both, and `WBP_HUDLayout` hosts the
 * `Killfeed` child today. The inline one is gone: it rebuilt every row on every change
 * (re-parenting mid-firefight, and re-firing enter animations), it capped the feed silently,
 * and it had no index-stable slot for a late Spotter line to land in.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINT_API UBRHUDLayout : public UBRActivatableWidget
{
	GENERATED_BODY()

public:
	UBRHUDLayout();

protected:
	virtual void BindViewModels() override;
	virtual void UnbindViewModels() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Breachpoint|HUD|Hit Markers", meta = (DisplayName = "On Shield Hit"))
	void BP_OnShieldHit();

	UFUNCTION(BlueprintImplementableEvent, Category = "Breachpoint|HUD|Hit Markers", meta = (DisplayName = "On Flesh Hit"))
	void BP_OnFleshHit();

	UFUNCTION(BlueprintImplementableEvent, Category = "Breachpoint|HUD|Hit Markers", meta = (DisplayName = "On Headshot Hit"))
	void BP_OnHeadshotHit();

	UFUNCTION(BlueprintImplementableEvent, Category = "Breachpoint|HUD|Hit Markers", meta = (DisplayName = "On Kill Confirmed"))
	void BP_OnKillConfirmed();

	UFUNCTION(BlueprintImplementableEvent, Category = "Breachpoint|HUD", meta = (DisplayName = "On Vitals State Changed"))
	void BP_OnVitalsStateChanged(EBRUIDataState NewState);

	UFUNCTION(BlueprintImplementableEvent, Category = "Breachpoint|HUD", meta = (DisplayName = "On Match State Changed"))
	void BP_OnMatchStateChanged(EBRUIDataState NewState);

private:
	void HandleHitMarker(EBRHitMarkerKind Kind);
	void HandleViewModelFieldChanged(UObject* Source, UE::FieldNotification::FFieldId FieldId);

	void PushViewModelsIntoMVVMView();

	FDelegateHandle VitalsStateFieldHandle;
	FDelegateHandle MatchStateFieldHandle;
};
