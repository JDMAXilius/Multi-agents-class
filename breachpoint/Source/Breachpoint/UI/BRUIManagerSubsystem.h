// Breachpoint. The screen-management spine: one entry point for pushing and popping screens.
#pragma once

#include "Subsystems/GameInstanceSubsystem.h"
#include "Templates/SubclassOf.h"
#include "UObject/SoftObjectPtr.h"
#include "UI/BRUITypes.h"

#include "BRUIManagerSubsystem.generated.h"

class UBRActivatableWidget;
class UBRRootLayout;
class UBRVM_Combat;
class UBRVM_Match;
class ULocalPlayer;

USTRUCT()
struct FBRLocalPlayerUI
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UBRRootLayout> RootLayout = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UBRVM_Combat> CombatViewModel = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UBRVM_Match> MatchViewModel = nullptr;
};

UCLASS()
class BREACHPOINT_API UBRUIManagerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UBRUIManagerSubsystem* Get(const UObject* WorldContextObject);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UBRVM_Combat* GetCombatViewModel(const ULocalPlayer* LocalPlayer) const;
	UBRVM_Match* GetMatchViewModel(const ULocalPlayer* LocalPlayer) const;

	UBRVM_Combat* GetPrimaryCombatViewModel() const;
	UBRVM_Match* GetPrimaryMatchViewModel() const;

	UBRRootLayout* CreateLayoutForLocalPlayer(ULocalPlayer* LocalPlayer);

	void RemoveLayoutForLocalPlayer(ULocalPlayer* LocalPlayer);

	UBRRootLayout* GetRootLayout(const ULocalPlayer* LocalPlayer) const;

	UBRActivatableWidget* PushWidgetToLayer(ULocalPlayer* LocalPlayer, FUITag LayerTag, const TSoftClassPtr<UBRActivatableWidget>& WidgetClass);

	UBRActivatableWidget* PushWidgetClassToLayer(ULocalPlayer* LocalPlayer, FUITag LayerTag, TSubclassOf<UBRActivatableWidget> WidgetClass);

	void RemoveWidgetFromLayer(ULocalPlayer* LocalPlayer, FUITag LayerTag, UBRActivatableWidget* Widget);

	void ClearLayer(ULocalPlayer* LocalPlayer, FUITag LayerTag);

	UBRActivatableWidget* ShowHUD(ULocalPlayer* LocalPlayer);

	UBRActivatableWidget* ShowMainMenu(ULocalPlayer* LocalPlayer);

	UBRActivatableWidget* ShowDeathOverlay(ULocalPlayer* LocalPlayer);

	UBRActivatableWidget* ShowCarnageReport(ULocalPlayer* LocalPlayer);

private:
	void HandleLocalPlayerAdded(ULocalPlayer* LocalPlayer);
	void HandleLocalPlayerRemoved(ULocalPlayer* LocalPlayer);

	FBRLocalPlayerUI& FindOrAddPlayerUI(ULocalPlayer* LocalPlayer);
	const FBRLocalPlayerUI* FindPlayerUI(const ULocalPlayer* LocalPlayer) const;

	void PublishViewModelsToGlobalCollection(const FBRLocalPlayerUI& PlayerUI);
	void UnpublishViewModelsFromGlobalCollection(const FBRLocalPlayerUI& PlayerUI);

	UPROPERTY(Transient)
	TMap<TObjectPtr<ULocalPlayer>, FBRLocalPlayerUI> PlayerUIs;

	UPROPERTY(Transient)
	TObjectPtr<ULocalPlayer> PublishedLocalPlayer = nullptr;

	FDelegateHandle LocalPlayerAddedHandle;
	FDelegateHandle LocalPlayerRemovedHandle;
};
