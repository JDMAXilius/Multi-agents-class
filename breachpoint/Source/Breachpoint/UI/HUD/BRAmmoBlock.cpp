#include "UI/HUD/BRAmmoBlock.h"

#include "CommonTextBlock.h"
#include "Components/SizeBox.h"
#include "Animation/WidgetAnimation.h"
#include "Engine/LocalPlayer.h"
#include "INotifyFieldValueChanged.h"
#include "UI/BRUIManagerSubsystem.h"
#include "UI/BRViewModels.h"
#include "UI/Components/BRComponentTokens.h"
#include "UI/Styles/BRUITokens.h"

void UBRAmmoBlock::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// A readout, never an input surface (HUD-CPP-AUDIT H7). Propagates to every child.
	SetVisibility(ESlateVisibility::HitTestInvisible);

	if (RootSizeBox)
	{
		RootSizeBox->SetWidthOverride(AmmoWidth);
		RootSizeBox->SetHeightOverride(AmmoHeight);
	}

	// Construct unknown. On join-in-progress the equipment ViewModel has not spoken yet, and a
	// confident "32 / 160" would tell the player something false about the fight they are in.
	ApplyState(EBRAmmoReadoutState::Unknown);
}

void UBRAmmoBlock::NativeConstruct()
{
	Super::NativeConstruct();

	BindViewModel();
	Refresh();
}

void UBRAmmoBlock::NativeDestruct()
{
	UnbindViewModel();

	// HUD-CPP-AUDIT H10: the latch must not survive the tree, or a second construct skips the
	// state push and the WBP re-enters stale.
	bStateApplied = false;
	State = EBRAmmoReadoutState::Unknown;

	Super::NativeDestruct();
}

void UBRAmmoBlock::BindViewModel()
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
		// The widget stays unknown rather than inventing numbers.
		return;
	}

	BoundViewModel = ViewModel;

	const UE::FieldNotification::FFieldId ObservedFields[] = {
		UBRVM_Combat::FFieldNotificationClassDescriptor::EquipmentState,
		UBRVM_Combat::FFieldNotificationClassDescriptor::MagazineAmmo,
		UBRVM_Combat::FFieldNotificationClassDescriptor::ReserveAmmo,
		UBRVM_Combat::FFieldNotificationClassDescriptor::ActiveWeaponName,
		UBRVM_Combat::FFieldNotificationClassDescriptor::StowedWeaponName
	};

	for (const UE::FieldNotification::FFieldId& FieldId : ObservedFields)
	{
		ViewModel->AddFieldValueChangedDelegate(
			FieldId,
			INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateUObject(
				this, &UBRAmmoBlock::HandleFieldChanged));
	}
}

void UBRAmmoBlock::UnbindViewModel()
{
	if (UBRVM_Combat* ViewModel = BoundViewModel.Get())
	{
		ViewModel->RemoveAllFieldValueChangedDelegates(this);
	}

	BoundViewModel.Reset();
}

void UBRAmmoBlock::HandleFieldChanged(UObject* Source, UE::FieldNotification::FFieldId FieldId)
{
	Refresh();
}

EBRAmmoReadoutState UBRAmmoBlock::ResolveState() const
{
	const UBRVM_Combat* ViewModel = BoundViewModel.Get();
	if (!ViewModel || ViewModel->GetEquipmentState() == EBRUIDataState::Unknown)
	{
		return EBRAmmoReadoutState::Unknown;
	}

	// Low and Battery are unreachable BY DESIGN until UBRVM_Combat gains the two fields named in
	// this packet's contract gaps (a low-ammo signal sourced from a CT_Combat / DT_Weapons row,
	// and an energy-weapon charge). Guessing either one here -- "< 25% of MagSize", "reserve is
	// zero" -- would put a gameplay number in a widget, which law 3 and ui-presentation Sec 7.4
	// both forbid. This function is where they arrive; nothing else changes when they do.
	return EBRAmmoReadoutState::Normal;
}

void UBRAmmoBlock::Refresh()
{
	const UBRVM_Combat* ViewModel = BoundViewModel.Get();

	ApplyState(ResolveState());

	if (State == EBRAmmoReadoutState::Unknown || !ViewModel)
	{
		// ApplyState already printed the dash into every field.
		return;
	}

	// Stale still renders: it is the last true thing the server said. Unknown never does.
	if (MagazineText)
	{
		MagazineText->SetText(FText::AsNumber(ViewModel->GetMagazineAmmo()));
	}

	if (ReserveText)
	{
		ReserveText->SetText(FText::AsNumber(ViewModel->GetReserveAmmo()));
	}

	if (ActiveWeaponText)
	{
		const FText ActiveName = ViewModel->GetActiveWeaponName();
		ActiveWeaponText->SetText(ActiveName.IsEmpty() ? BRUI::UnknownValueText() : ActiveName);
	}

	if (StowedWeaponText)
	{
		// The ghosted swap target. An empty name collapses rather than rendering a blank ghost:
		// a player carrying one weapon has no swap target, and a blank slot would imply one.
		const FText StowedName = ViewModel->GetStowedWeaponName();
		StowedWeaponText->SetText(StowedName);
		StowedWeaponText->SetVisibility(
			StowedName.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

void UBRAmmoBlock::ApplyState(EBRAmmoReadoutState InState)
{
	// The BP hook is for STATE animations, so it fires on a state CHANGE and not on every ammo
	// push -- otherwise firing a weapon would restart the animation once per round.
	const bool bChanged = (State != InState) || !bStateApplied;

	State = InState;
	bStateApplied = true;

	if (MagazineText)
	{
		// Two states, two colours. (Low/amber returns with BP69's ViewModel fields.)
		MagazineText->SetColorAndOpacity(
			State == EBRAmmoReadoutState::Unknown ? BR::Tokens::Dead() : BR::Tokens::Shield());
	}

	if (ReserveText)
	{
		// Reserve is secondary in every export -- it never carries the warning.
		ReserveText->SetColorAndOpacity(
			State == EBRAmmoReadoutState::Unknown ? BR::Tokens::Dead() : BR::Tokens::InkDim());
	}

	if (StowedWeaponText)
	{
		// Ghosting is a TOKEN (visr/ink-dim), not a render-opacity literal.
		StowedWeaponText->SetColorAndOpacity(BR::Tokens::InkDim());
	}

	if (State == EBRAmmoReadoutState::Unknown)
	{
		// Honest empty state: an em dash everywhere a number would go.
		const FText Unknown = BRUI::UnknownValueText();

		if (MagazineText)
		{
			MagazineText->SetText(Unknown);
		}

		if (ReserveText)
		{
			ReserveText->SetText(Unknown);
		}

		if (ActiveWeaponText)
		{
			ActiveWeaponText->SetText(Unknown);
		}

		if (StowedWeaponText)
		{
			StowedWeaponText->SetText(Unknown);
		}
	}

	if (bChanged && StateAnim)
	{
		if (State == EBRAmmoReadoutState::Normal)
		{
			PlayAnimationForward(StateAnim);
		}
		else
		{
			PlayAnimationReverse(StateAnim);
		}
	}
}
