// Breachpoint. The HUD C++ base.

#include "UI/BRHUDLayout.h"

#include "Components/PanelWidget.h"
#include "Core/BRCore.h"
#include "Engine/World.h"
#include "INotifyFieldValueChanged.h"
#include "MVVMSubsystem.h"
#include "UI/BRUISettings.h"
#include "UI/BRViewModels.h"
#include "View/MVVMView.h"

// ===========================================================================
// UBRKillfeedEntryWidget
// ===========================================================================

void UBRKillfeedEntryWidget::SetEntry(const FBRKillfeedViewEntry& InEntry)
{
	Entry = InEntry;
	BP_OnEntrySet();
}

// ===========================================================================
// UBRHUDLayout
// ===========================================================================

UBRHUDLayout::UBRHUDLayout(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, KillfeedPool(*this)
{
	// The HUD is the one screen that must not steal input: the player is aiming through it.
	InputMode = EBRWidgetInputMode::Game;
}

void UBRHUDLayout::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	KillfeedPool.SetWorld(GetWorld());
	KillfeedPool.SetDefaultPlayerController(GetOwningPlayer());

	// Resolve the SOFT killfeed row class ONCE, here, at HUD construction - not per kill. This is
	// the only synchronous load on this class's path and it happens before the first shot.
	const UBRUISettings& Settings = UBRUISettings::Get();
	if (!Settings.KillfeedEntryClass.IsNull())
	{
		ResolvedKillfeedEntryClass = Settings.KillfeedEntryClass.LoadSynchronous();
	}

	if (!ResolvedKillfeedEntryClass)
	{
		// Honest degradation: the rest of the HUD works; the killfeed is empty and says why once.
		UE_LOG(LogBRUI, Warning,
			TEXT("BRUISettings.KillfeedEntryClass is unset or failed to load; the killfeed will ")
			TEXT("render no rows."));
	}
}

void UBRHUDLayout::ReleaseSlateResources(bool bReleaseChildren)
{
	// UserWidgetPool.h's own warning: release the pool's Slate widgets from the owning widget's
	// ReleaseSlateResources or the circular reference leaks. This is not optional bookkeeping.
	KillfeedPool.ReleaseAllSlateResources();

	Super::ReleaseSlateResources(bReleaseChildren);
}

void UBRHUDLayout::BindViewModels()
{
	Super::BindViewModels();

	// Hand this local player's ViewModels to the WBP's MVVM view before subscribing, so the very
	// first broadcast below lands on bindings that already exist.
	PushViewModelsIntoMVVMView();

	if (UBRVM_Combat* Combat = GetCombatViewModel())
	{
		Combat->OnHitMarker().AddUObject(this, &UBRHUDLayout::HandleHitMarker);

		VitalsStateFieldHandle = Combat->AddFieldValueChangedDelegate(
			UBRVM_Combat::FFieldNotificationClassDescriptor::VitalsState,
			INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateUObject(
				this, &UBRHUDLayout::HandleViewModelFieldChanged));

		// Publish the CURRENT state immediately. Subscriptions only deliver future changes, and a
		// HUD that activates after the state settled would sit on its Unknown placeholder forever.
		BP_OnVitalsStateChanged(Combat->GetVitalsState());
	}
	else
	{
		// No ViewModel yet is a normal frame, not a bug. Say Unknown out loud so the WBP renders
		// dashes rather than whatever its designer left in the default text field.
		BP_OnVitalsStateChanged(EBRUIDataState::Unknown);
	}

	if (UBRVM_Match* Match = GetMatchViewModel())
	{
		Match->OnKillfeedChanged().AddUObject(this, &UBRHUDLayout::HandleKillfeedChanged);

		MatchStateFieldHandle = Match->AddFieldValueChangedDelegate(
			UBRVM_Match::FFieldNotificationClassDescriptor::MatchState,
			INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateUObject(
				this, &UBRHUDLayout::HandleViewModelFieldChanged));

		BP_OnMatchStateChanged(Match->GetMatchState());
		RebuildKillfeed();
	}
	else
	{
		BP_OnMatchStateChanged(EBRUIDataState::Unknown);
	}
}

void UBRHUDLayout::UnbindViewModels()
{
	// Symmetric with BindViewModels, unconditionally. A HUD is deactivated on death, on
	// scoreboard, and on travel; a subscription that survives any one of those outlives the
	// widget on the next one.
	if (UBRVM_Combat* Combat = GetCombatViewModel())
	{
		Combat->OnHitMarker().RemoveAll(this);
		if (VitalsStateFieldHandle.IsValid())
		{
			Combat->RemoveFieldValueChangedDelegate(
				UBRVM_Combat::FFieldNotificationClassDescriptor::VitalsState, VitalsStateFieldHandle);
		}
	}
	VitalsStateFieldHandle.Reset();

	if (UBRVM_Match* Match = GetMatchViewModel())
	{
		Match->OnKillfeedChanged().RemoveAll(this);
		if (MatchStateFieldHandle.IsValid())
		{
			Match->RemoveFieldValueChangedDelegate(
				UBRVM_Match::FFieldNotificationClassDescriptor::MatchState, MatchStateFieldHandle);
		}
	}
	MatchStateFieldHandle.Reset();

	Super::UnbindViewModels();
}

void UBRHUDLayout::PushViewModelsIntoMVVMView()
{
	// GetViewFromUserWidget returns null for a WBP with no MVVM bindings authored, which is the
	// state of every screen until the WBP packet lands. Not an error; nothing to hand over.
	UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this);
	if (!View)
	{
		return;
	}

	const UBRUISettings& Settings = UBRUISettings::Get();

	// SetViewModel by NAME. This is the per-local-player-correct path: the MVVM global collection
	// can only hold one player's ViewModels, so a splitscreen second player bound through the
	// collection would silently read player one's health. Here it cannot.
	if (UBRVM_Combat* Combat = GetCombatViewModel())
	{
		View->SetViewModel(Settings.CombatViewModelContextName, Combat);
	}
	if (UBRVM_Match* Match = GetMatchViewModel())
	{
		View->SetViewModel(Settings.MatchViewModelContextName, Match);
	}
}

void UBRHUDLayout::HandleHitMarker(EBRHitMarkerKind Kind)
{
	// One event in, one event out. No decision is made here and none is made in the WBP - the
	// shield-vs-flesh distinction was decided by the damage pipeline, which is the only place that
	// actually knows.
	switch (Kind)
	{
	case EBRHitMarkerKind::Shield:
		BP_OnShieldHit();
		break;
	case EBRHitMarkerKind::Flesh:
		BP_OnFleshHit();
		break;
	case EBRHitMarkerKind::Headshot:
		BP_OnHeadshotHit();
		break;
	case EBRHitMarkerKind::Kill:
		BP_OnKillConfirmed();
		break;
	case EBRHitMarkerKind::None:
	default:
		break;
	}
}

void UBRHUDLayout::HandleViewModelFieldChanged(UObject* Source, UE::FieldNotification::FFieldId FieldId)
{
	if (const UBRVM_Combat* Combat = Cast<UBRVM_Combat>(Source))
	{
		if (FieldId == UBRVM_Combat::FFieldNotificationClassDescriptor::VitalsState)
		{
			BP_OnVitalsStateChanged(Combat->GetVitalsState());
		}
		return;
	}

	if (const UBRVM_Match* Match = Cast<UBRVM_Match>(Source))
	{
		if (FieldId == UBRVM_Match::FFieldNotificationClassDescriptor::MatchState)
		{
			BP_OnMatchStateChanged(Match->GetMatchState());
		}
	}
}

void UBRHUDLayout::HandleKillfeedChanged()
{
	RebuildKillfeed();
}

void UBRHUDLayout::RebuildKillfeed()
{
	if (!KillfeedContainer || !ResolvedKillfeedEntryClass)
	{
		return;
	}

	const UBRVM_Match* Match = GetMatchViewModel();
	if (!Match)
	{
		return;
	}

	// Release-then-reclaim in the same call. FUserWidgetPool hands back the SAME instances, so
	// after the first rebuild this loop allocates nothing: no CreateWidget, no RemoveFromParent
	// churn, no per-kill garbage during a firefight.
	KillfeedContainer->ClearChildren();
	KillfeedPool.ReleaseAll(/*bReleaseSlate=*/false);

	const TArray<FBRKillfeedViewEntry>& Entries = Match->GetKillfeedEntries();
	int32 NumRows = 0;

	for (const FBRKillfeedViewEntry& Entry : Entries)
	{
		UBRKillfeedEntryWidget* Row = KillfeedPool.GetOrCreateInstance<UBRKillfeedEntryWidget>(ResolvedKillfeedEntryClass);
		if (!Row)
		{
			UE_LOG(LogBRUI, Warning, TEXT("Killfeed pool could not produce a row widget; %d of %d rendered."),
				NumRows, Entries.Num());
			break;
		}

		Row->SetEntry(Entry);
		KillfeedContainer->AddChild(Row);
		++NumRows;
	}

	BP_OnKillfeedRebuilt(NumRows);
}
