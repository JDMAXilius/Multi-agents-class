#include "UI/HUD/BREquipmentTray.h"

#include "CommonTextBlock.h"
#include "Engine/World.h"
#include "INotifyFieldValueChanged.h"
#include "TimerManager.h"
#include "UI/BRUIManagerSubsystem.h"
#include "UI/BRViewModels.h"
#include "UI/Components/BRComponentTokens.h"
#include "UI/Components/BRProgressBar.h"

#define LOCTEXT_NAMESPACE "Breachpoint"

UBREquipmentTray::UBREquipmentTray()
{
	GrappleReadyText = LOCTEXT("GrappleReady", "READY");
}

void UBREquipmentTray::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (CooldownBar)
	{
		// UBRProgressBar's own doc maps the uses: "cooldowns -> Amber (a clock is running)".
		// Pinned from C++ so a tray WBP cannot restyle a cooldown into a shield.
		CooldownBar->SetTreatment(EBRProgressTreatment::CombatVISR);
		CooldownBar->SetChannel(EBRProgressChannel::Amber);
	}

	// Construct unknown, never at a confident value (ue5-ui-architecture Sec 7).
	ApplyCooldownUnknown();
	RefreshGrenades();
}

void UBREquipmentTray::NativeConstruct()
{
	Super::NativeConstruct();

	BindViewModel();
}

void UBREquipmentTray::NativeDestruct()
{
	UnbindViewModel();
	StopCooldownTimer();

	Super::NativeDestruct();
}

void UBREquipmentTray::BindViewModel()
{
	UnbindViewModel();

	const UBRUIManagerSubsystem* Manager = UBRUIManagerSubsystem::Get(this);
	UBRVM_Combat* Combat = Manager ? Manager->GetCombatViewModel(GetOwningLocalPlayer()) : nullptr;
	BoundCombat = Combat;

	if (!Combat)
	{
		// The local player's ViewModel is not registered yet. Nothing is dereferenced and the
		// tray stays in its honest unknown state rather than printing a confident zero.
		ApplyCooldownUnknown();
		RefreshGrenades();
		return;
	}

	const INotifyFieldValueChanged::FFieldValueChangedDelegate Delegate =
		INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateUObject(
			this, &UBREquipmentTray::HandleCombatFieldChanged);

	GrenadeCountHandle = Combat->AddFieldValueChangedDelegate(
		UBRVM_Combat::FFieldNotificationClassDescriptor::GrenadeCount, Delegate);
	EquipmentStateHandle = Combat->AddFieldValueChangedDelegate(
		UBRVM_Combat::FFieldNotificationClassDescriptor::EquipmentState, Delegate);
	CooldownStartedHandle = Combat->AddFieldValueChangedDelegate(
		UBRVM_Combat::FFieldNotificationClassDescriptor::GrappleCooldownStarted, Delegate);
	GrappleReadyHandle = Combat->AddFieldValueChangedDelegate(
		UBRVM_Combat::FFieldNotificationClassDescriptor::GrappleReady, Delegate);

	RefreshGrenades();

	if (Combat->IsGrappleReady())
	{
		ApplyGrappleReady();
	}
	else
	{
		// Cooling, but this widget never saw the start event -- a join in progress, or a HUD
		// rebuilt mid-cooldown. The ViewModel carries the DURATION but no start/end instant, so
		// remaining time is genuinely unknown: dashes, not a guessed sweep. (Contract gap.)
		ApplyCooldownUnknown();
		BP_OnGrappleReadyChanged(false);
	}
}

void UBREquipmentTray::UnbindViewModel()
{
	if (UBRVM_Combat* Combat = BoundCombat.Get())
	{
		if (GrenadeCountHandle.IsValid())
		{
			Combat->RemoveFieldValueChangedDelegate(
				UBRVM_Combat::FFieldNotificationClassDescriptor::GrenadeCount, GrenadeCountHandle);
		}
		if (EquipmentStateHandle.IsValid())
		{
			Combat->RemoveFieldValueChangedDelegate(
				UBRVM_Combat::FFieldNotificationClassDescriptor::EquipmentState, EquipmentStateHandle);
		}
		if (CooldownStartedHandle.IsValid())
		{
			Combat->RemoveFieldValueChangedDelegate(
				UBRVM_Combat::FFieldNotificationClassDescriptor::GrappleCooldownStarted, CooldownStartedHandle);
		}
		if (GrappleReadyHandle.IsValid())
		{
			Combat->RemoveFieldValueChangedDelegate(
				UBRVM_Combat::FFieldNotificationClassDescriptor::GrappleReady, GrappleReadyHandle);
		}
	}

	GrenadeCountHandle.Reset();
	EquipmentStateHandle.Reset();
	CooldownStartedHandle.Reset();
	GrappleReadyHandle.Reset();
	BoundCombat.Reset();
}

void UBREquipmentTray::HandleCombatFieldChanged(UObject* Source, UE::FieldNotification::FFieldId FieldId)
{
	const UBRVM_Combat* Combat = Cast<UBRVM_Combat>(Source);
	if (!Combat)
	{
		return;
	}

	if (FieldId == UBRVM_Combat::FFieldNotificationClassDescriptor::GrenadeCount
		|| FieldId == UBRVM_Combat::FFieldNotificationClassDescriptor::EquipmentState)
	{
		RefreshGrenades();
		return;
	}

	if (FieldId == UBRVM_Combat::FFieldNotificationClassDescriptor::GrappleCooldownStarted)
	{
		StartCooldownVisual(Combat->GetGrappleCooldownDuration());
		return;
	}

	if (FieldId == UBRVM_Combat::FFieldNotificationClassDescriptor::GrappleReady)
	{
		ApplyGrappleReady();
	}
}

void UBREquipmentTray::RefreshGrenades()
{
	if (!GrenadeCountText)
	{
		return;
	}

	const UBRVM_Combat* Combat = BoundCombat.Get();
	const bool bKnown = Combat && Combat->GetEquipmentState() != EBRUIDataState::Unknown;

	// ONE number: the sim has one untyped grenade count (BP69). When a type axis lands on the
	// ViewModel, the glyph swap hangs off this one function.
	GrenadeCountText->SetText(
		bKnown ? FText::AsNumber(Combat->GetGrenadeCount()) : BRUI::UnknownValueText());
}

void UBREquipmentTray::StartCooldownVisual(float DurationSeconds)
{
	UWorld* World = GetWorld();
	if (!World || DurationSeconds <= 0.0f)
	{
		// A cooldown with no duration is not a cooldown this widget can draw honestly.
		ApplyCooldownUnknown();
		BP_OnGrappleReadyChanged(false);
		return;
	}

	CooldownDurationSeconds = DurationSeconds;
	CooldownEndTimeSeconds = World->GetTimeSeconds() + DurationSeconds;

	BP_OnGrappleReadyChanged(false);
	AdvanceCooldownVisual();

	// A timer, not a Tick (law 4). It interpolates between two instants already known at this
	// line; it never asks the ability system anything.
	World->GetTimerManager().SetTimer(
		CooldownTimerHandle, this, &UBREquipmentTray::AdvanceCooldownVisual, CooldownRefreshSeconds, true);
}

void UBREquipmentTray::AdvanceCooldownVisual()
{
	const UWorld* World = GetWorld();
	if (!World || !CooldownBar || CooldownDurationSeconds <= 0.0f)
	{
		StopCooldownTimer();
		return;
	}

	const float Remaining = FMath::Max(0.0f, CooldownEndTimeSeconds - World->GetTimeSeconds());
	CooldownBar->SetPercent(1.0f - (Remaining / CooldownDurationSeconds));

	if (Remaining > 0.0f)
	{
		CooldownBar->SetValueText(FText::AsNumber(FMath::CeilToInt(Remaining)));
		return;
	}

	// The prediction ran out. HOLD -- do not report ready. Readiness is a GE-applied tag and only
	// UBRVM_Combat::SetGrappleReady may announce it; an empty push prints the bar's own dash,
	// which is the true statement in the gap between prediction and confirmation.
	StopCooldownTimer();
	CooldownBar->SetValueText(FText());
}

void UBREquipmentTray::ApplyGrappleReady()
{
	StopCooldownTimer();

	CooldownDurationSeconds = 0.0f;
	CooldownEndTimeSeconds = 0.0f;

	if (CooldownBar)
	{
		CooldownBar->SetPercent(UBRProgressBar::FullPercent);
		CooldownBar->SetValueText(GrappleReadyText);
	}

	BP_OnGrappleReadyChanged(true);
}

void UBREquipmentTray::ApplyCooldownUnknown()
{
	StopCooldownTimer();

	CooldownDurationSeconds = 0.0f;
	CooldownEndTimeSeconds = 0.0f;

	if (CooldownBar)
	{
		CooldownBar->SetPercentUnknown();
	}
}

void UBREquipmentTray::StopCooldownTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CooldownTimerHandle);
	}
	CooldownTimerHandle.Invalidate();
}

#undef LOCTEXT_NAMESPACE
