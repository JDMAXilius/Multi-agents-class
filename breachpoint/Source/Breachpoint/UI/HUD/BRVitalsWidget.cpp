#include "UI/HUD/BRVitalsWidget.h"

#include "Animation/WidgetAnimation.h"
#include "Components/SizeBox.h"
#include "Components/Widget.h"
#include "Engine/LocalPlayer.h"
#include "INotifyFieldValueChanged.h"
#include "UI/BRUIManagerSubsystem.h"
#include "UI/BRViewModels.h"

void UBRVitalsWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Vitals are a readout, never an input surface (HUD-CPP-AUDIT H7). Propagates to children.
	SetVisibility(ESlateVisibility::HitTestInvisible);

	if (RootSizeBox)
	{
		RootSizeBox->SetWidthOverride(VitalsWidth);
		RootSizeBox->SetHeightOverride(VitalsHeight);
	}

	// Pinned here, not in the WBP: the tier rule (ui-presentation Sec 4) and "health is yellow,
	// never green" both stop being enforceable the moment a details panel can pick a channel.
	if (ShieldBar)
	{
		ShieldBar->SetTreatment(EBRProgressTreatment::CombatVISR);
		ShieldBar->SetChannel(EBRProgressChannel::Shield);
	}

	if (HealthBar)
	{
		HealthBar->SetTreatment(EBRProgressTreatment::CombatVISR);
		// Channel Health is also what arms hidden-until-damaged inside UBRProgressBar.
		HealthBar->SetChannel(EBRProgressChannel::Health);
	}

	// Construct unknown. A late joiner has no attributes for one or more frames and a confident
	// full shield bar would be a lie about a fight they are already in (ue5-ui-architecture Sec 7).
	ApplyUnknown();
}

void UBRVitalsWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BindViewModel();
	Refresh();
}

void UBRVitalsWidget::NativeDestruct()
{
	// UNBIND. A FieldNotify delegate outliving its widget is the crash the death cam finds.
	UnbindViewModel();

	// HUD-CPP-AUDIT H10: the state latch must not survive the tree. Left set, a second
	// construct saw an "unchanged" pair, skipped the state push, and the WBP re-entered the
	// tree without its shield-break treatment.
	bStatePushed = false;
	bLastShieldsBroken = false;
	bLastKnown = false;

	Super::NativeDestruct();
}

void UBRVitalsWidget::BindViewModel()
{
	UnbindViewModel();

	const UBRUIManagerSubsystem* UIManager = UBRUIManagerSubsystem::Get(this);
	if (!UIManager)
	{
		return;
	}

	UBRVM_Combat* ViewModel = UIManager->GetCombatViewModel(GetOwningLocalPlayer());
	if (!ViewModel)
	{
		// Legal: the ViewModel may not exist yet. The widget stays in its unknown state rather
		// than inventing numbers, and picks the data up when the HUD is next constructed.
		return;
	}

	BoundViewModel = ViewModel;

	// Every field this surface reads. Any of them changing re-reads all of them -- four getters
	// is cheaper than a per-field switch and cannot drift out of sync with the display.
	const UE::FieldNotification::FFieldId ObservedFields[] = {
		UBRVM_Combat::FFieldNotificationClassDescriptor::VitalsState,
		UBRVM_Combat::FFieldNotificationClassDescriptor::ShieldPercent,
		UBRVM_Combat::FFieldNotificationClassDescriptor::HealthPercent,
		UBRVM_Combat::FFieldNotificationClassDescriptor::bShieldsBroken
	};

	for (const UE::FieldNotification::FFieldId& FieldId : ObservedFields)
	{
		ViewModel->AddFieldValueChangedDelegate(
			FieldId,
			INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateUObject(
				this, &UBRVitalsWidget::HandleFieldChanged));
	}
}

void UBRVitalsWidget::UnbindViewModel()
{
	if (UBRVM_Combat* ViewModel = BoundViewModel.Get())
	{
		// Removing by delegate object covers every field in one call and cannot leave a handle
		// bound to a field the list above stopped naming.
		ViewModel->RemoveAllFieldValueChangedDelegates(this);
	}

	BoundViewModel.Reset();
}

void UBRVitalsWidget::HandleFieldChanged(UObject* Source, UE::FieldNotification::FFieldId FieldId)
{
	Refresh();
}

void UBRVitalsWidget::Refresh()
{
	const UBRVM_Combat* ViewModel = BoundViewModel.Get();
	if (!ViewModel || ViewModel->GetVitalsState() == EBRUIDataState::Unknown)
	{
		ApplyUnknown();
		return;
	}

	// Stale is still the last TRUE thing the server said, so it renders. Unknown never does.
	const bool bBroken = ViewModel->AreShieldsBroken();

	if (ShieldBar)
	{
		ShieldBar->SetPercent(ViewModel->GetShieldPercent());

		// The only shield colour change this class can honestly make: `bShieldsBroken` is the one
		// signal the ViewModel offers, and Enemy on a shield break is a sanctioned use of the
		// threat channel. `visr/shield-low` needs a threshold nobody has authored -- see the
		// contract gaps; a literal 0.25f here would be a law 3 violation.
		ShieldBar->SetChannel(bBroken ? EBRProgressChannel::Enemy : EBRProgressChannel::Shield);
	}

	if (HealthBar)
	{
		// Hidden-until-damaged is applied inside UBRProgressBar; we never toggle it from here.
		HealthBar->SetPercent(ViewModel->GetHealthPercent());
	}

	if (CentreTick)
	{
		// Hidden, not Collapsed: the vitals block must not reflow under the player's eyes.
		CentreTick->SetVisibility(bBroken ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible);
	}

	NotifyStateChanged(bBroken, true);
}

void UBRVitalsWidget::ApplyUnknown()
{
	if (ShieldBar)
	{
		ShieldBar->SetChannel(EBRProgressChannel::Shield);
		ShieldBar->SetPercentUnknown();
	}

	if (HealthBar)
	{
		HealthBar->SetPercentUnknown();
	}

	if (CentreTick)
	{
		CentreTick->SetVisibility(ESlateVisibility::Hidden);
	}

	NotifyStateChanged(false, false);
}

void UBRVitalsWidget::NotifyStateChanged(bool bInShieldsBroken, bool bInKnown)
{
	if (bStatePushed && bLastShieldsBroken == bInShieldsBroken && bLastKnown == bInKnown)
	{
		return;
	}

	bLastShieldsBroken = bInShieldsBroken;
	bLastKnown = bInKnown;
	bStatePushed = true;

	// The lawful hook: a WBP-authored animation played from C++ (the old BIE could never be
	// implemented under R18). Forward on break, reverse on restore; the change-gate above is
	// what stops a regen tick from strobing it.
	if (ShieldBreakAnim)
	{
		if (bInShieldsBroken)
		{
			PlayAnimationForward(ShieldBreakAnim);
		}
		else
		{
			PlayAnimationReverse(ShieldBreakAnim);
		}
	}
}
