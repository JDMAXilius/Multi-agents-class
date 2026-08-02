// Breachpoint. The HUD C++ base: binding points, killfeed pooling, hit-marker dispatch.
#pragma once

#include "Blueprint/UserWidgetPool.h"
#include "CommonUserWidget.h"
#include "FieldNotificationId.h"
#include "UI/BRActivatableWidget.h"
#include "UI/BRUITypes.h"

#include "BRHUDLayout.generated.h"

class UPanelWidget;

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

UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINT_API UBRHUDLayout : public UBRActivatableWidget
{
	GENERATED_BODY()

public:
	UBRHUDLayout(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeOnInitialized() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

	virtual void BindViewModels() override;
	virtual void UnbindViewModels() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|HUD")
	TObjectPtr<UPanelWidget> KillfeedContainer;

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

	UFUNCTION(BlueprintImplementableEvent, Category = "Breachpoint|HUD", meta = (DisplayName = "On Killfeed Rebuilt"))
	void BP_OnKillfeedRebuilt(int32 NumRows);

private:
	void HandleHitMarker(EBRHitMarkerKind Kind);
	void HandleKillfeedChanged();
	void HandleViewModelFieldChanged(UObject* Source, UE::FieldNotification::FFieldId FieldId);

	void RebuildKillfeed();
	void PushViewModelsIntoMVVMView();

	UPROPERTY(Transient)
	FUserWidgetPool KillfeedPool;

	UPROPERTY(Transient)
	TSubclassOf<UBRKillfeedEntryWidget> ResolvedKillfeedEntryClass;

	FDelegateHandle VitalsStateFieldHandle;
	FDelegateHandle MatchStateFieldHandle;
};
