#include "UI/BNScreen_Death.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Engine/Texture2D.h"
#include "Components/TextBlock.h"
#include "INotifyFieldValueChanged.h"
#include "UI/BNUITypes.h"
#include "UI/BNViewModels.h"

#define LOCTEXT_NAMESPACE "BreachpointNextUI"

UBNScreen_Death::UBNScreen_Death()
{
	// EXPLICIT Game (critic + the compiled reference, which carries a paragraph on why Inherit
	// is dangerous here): the dead player keeps looking around, and input must not be left to
	// whatever config happened to be applied last.
	InputMode = EBNWidgetInputMode::Game;
}

void UBNScreen_Death::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	if (KilledByText) { KilledByText->SetColorAndOpacity(FSlateColor(BNUIColors::Threat)); }
	if (RespawnText) { RespawnText->SetColorAndOpacity(FSlateColor(BNUIColors::Amber)); }
}

void UBNScreen_Death::BindViewModels()
{
	UBNVM_Combat* Combat = GetCombatViewModel();
	if (!Combat)
	{
		return;
	}
	BoundViewModel = Combat;

	for (const UE::FieldNotification::FFieldId FieldId : {
		UBNVM_Combat::FFieldNotificationClassDescriptor::KilledByLine,
		UBNVM_Combat::FFieldNotificationClassDescriptor::KilledByWeapon,
		UBNVM_Combat::FFieldNotificationClassDescriptor::KilledByWeaponIcon,
		UBNVM_Combat::FFieldNotificationClassDescriptor::RespawnFraction,
		UBNVM_Combat::FFieldNotificationClassDescriptor::RespawnSecondsRemaining })
	{
		if (FieldId.IsValid())
		{
			BoundFields.Emplace(FieldId, Combat->AddFieldValueChangedDelegate(FieldId,
				INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateUObject(this, &UBNScreen_Death::HandleCombatFieldChanged)));
		}
	}

	Refresh();
}

void UBNScreen_Death::UnbindViewModels()
{
	if (UBNVM_Combat* Combat = BoundViewModel.Get())
	{
		for (const TPair<UE::FieldNotification::FFieldId, FDelegateHandle>& Bound : BoundFields)
		{
			if (Bound.Value.IsValid())
			{
				Combat->RemoveFieldValueChangedDelegate(Bound.Key, Bound.Value);
			}
		}
	}
	BoundFields.Reset();
	BoundViewModel.Reset();
}

void UBNScreen_Death::HandleCombatFieldChanged(UObject* Source, UE::FieldNotification::FFieldId FieldId)
{
	Refresh();
}

void UBNScreen_Death::Refresh()
{
	const UBNVM_Combat* Combat = BoundViewModel.Get();
	if (!Combat)
	{
		return;
	}

	if (KilledByText)
	{
		// Empty while the killfeed bunch is in flight — an honest blank the ring catch-up fills
		// a moment later, never a guessed name.
		KilledByText->SetText(Combat->GetKilledByLine());
	}
	if (WeaponText)
	{
		// R7.3 — the weapon under the name. COLLAPSED when unnamed rather than blanked: the two
		// lines are a vertical box, and an empty row would push the respawn clock down a line
		// for exactly the deaths the door could not explain.
		const FText Weapon = Combat->GetKilledByWeapon();
		WeaponText->SetText(Weapon);
		WeaponText->SetVisibility(Weapon.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	const int32 Seconds = Combat->GetRespawnSecondsRemaining();
	const bool bRespawnPending = Seconds >= 0;

	if (RespawnText)
	{
		RespawnText->SetText(bRespawnPending
			? FText::Format(LOCTEXT("RespawnIn", "RESPAWNING IN {0}"), FText::AsNumber(Seconds))
			: FText::GetEmpty());
	}

	// R7.7 — the bare numeral the design draws large. Same value, different weight: the sentence
	// above it is for reading once, this is for glancing at.
	if (CountdownText)
	{
		CountdownText->SetText(bRespawnPending ? FText::AsNumber(Seconds) : FText::GetEmpty());
		CountdownText->SetVisibility(bRespawnPending ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}

	// The ring fills as the wait runs down. Hidden — not zeroed — when nothing is pending: an
	// empty ring on a living player is a UI element with no subject.
	if (RespawnRing)
	{
		RespawnRing->SetPercent(Combat->GetRespawnFraction());
		RespawnRing->SetVisibility(bRespawnPending ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}

	// The killer's weapon, as art rather than words — the same soft icon the killfeed glyph and
	// the tray silhouette use, so one filled DT column lights all three.
	if (WeaponIcon)
	{
		const TSoftObjectPtr<UTexture2D> Icon = Combat->GetKilledByWeaponIcon();
		const bool bHasIcon = !Icon.IsNull();
		if (bHasIcon && Icon != AppliedWeaponIcon)
		{
			WeaponIcon->SetBrushFromSoftTexture(Icon, /*bMatchSize=*/false);
			AppliedWeaponIcon = Icon;
		}
		WeaponIcon->SetVisibility(bHasIcon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

#undef LOCTEXT_NAMESPACE
