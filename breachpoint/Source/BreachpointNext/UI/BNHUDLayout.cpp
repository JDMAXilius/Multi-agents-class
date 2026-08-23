#include "UI/BNHUDLayout.h"
#include "BreachpointNext.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "INotifyFieldValueChanged.h"
#include "UI/BNViewModels.h"

UBNHUDLayout::UBNHUDLayout()
{
	// The game plays under this widget; nothing here takes input away from it.
	InputMode = EBNWidgetInputMode::Game;
}

void UBNHUDLayout::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Rule 1, applied once at the root — it propagates to every child, so no surface can
	// accidentally become a click-eater.
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UBNHUDLayout::BindViewModels()
{
	UBNVM_Match* Match = GetMatchViewModel();
	if (!Match)
	{
		return;
	}
	BoundViewModel = Match;

	const UE::FieldNotification::FFieldId FieldId = UBNVM_Match::FFieldNotificationClassDescriptor::PhaseBannerText;
	if (FieldId.IsValid())
	{
		BoundFields.Emplace(FieldId, Match->AddFieldValueChangedDelegate(FieldId,
			INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateUObject(this, &UBNHUDLayout::HandleMatchFieldChanged)));
	}

	// The reticle is the one thing on this layout the COMBAT model feeds — see the header for
	// why it lives here at all. A HUD whose match model is up but whose combat model is not
	// still gets a reticle, because RefreshReticle falls back.
	if (UBNVM_Combat* Combat = GetCombatViewModel())
	{
		BoundCombatViewModel = Combat;
		BindCombatField(Combat, UBNVM_Combat::FFieldNotificationClassDescriptor::WeaponReticle);
	}

	Refresh();
	RefreshReticle();
}

void UBNHUDLayout::BindCombatField(UBNVM_Combat* Combat, UE::FieldNotification::FFieldId FieldId)
{
	if (!Combat || !FieldId.IsValid())
	{
		return;
	}
	BoundCombatFields.Emplace(FieldId, Combat->AddFieldValueChangedDelegate(FieldId,
		INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateUObject(this, &UBNHUDLayout::HandleCombatFieldChanged)));
}

void UBNHUDLayout::HandleCombatFieldChanged(UObject* Source, UE::FieldNotification::FFieldId FieldId)
{
	RefreshReticle();
}

void UBNHUDLayout::RefreshReticle()
{
	if (!ReticleDot)
	{
		return;
	}

	const UBNVM_Combat* Combat = BoundCombatViewModel.Get();
	const TSoftObjectPtr<UTexture2D> FromWeapon = Combat ? Combat->GetWeaponReticle() : TSoftObjectPtr<UTexture2D>();
	// The weapon's mark wins; the default catches an unset column and the unarmed hand. Only a
	// project with NEITHER draws nothing, and that is announced once rather than silently.
	const TSoftObjectPtr<UTexture2D> Chosen = FromWeapon.IsNull() ? DefaultReticle : FromWeapon;

	if (Chosen.IsNull())
	{
		if (!bWarnedNoReticle)
		{
			bWarnedNoReticle = true;
			UE_LOG(LogBN, Warning, TEXT("BNUI: no reticle — the row's Reticle column is unset and "
				"DefaultReticle is unset on %s. The centre of the screen will be empty."),
				*GetClass()->GetName());
		}
		ReticleDot->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	ReticleDot->SetBrushFromSoftTexture(Chosen, /*bMatchSize=*/false);
	ReticleDot->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UBNHUDLayout::UnbindViewModels()
{
	// Against the STORED object (H9), never a fresh lookup.
	if (UBNVM_Match* Match = BoundViewModel.Get())
	{
		for (const TPair<UE::FieldNotification::FFieldId, FDelegateHandle>& Bound : BoundFields)
		{
			if (Bound.Value.IsValid())
			{
				Match->RemoveFieldValueChangedDelegate(Bound.Key, Bound.Value);
			}
		}
	}
	if (UBNVM_Combat* Combat = BoundCombatViewModel.Get())
	{
		for (const TPair<UE::FieldNotification::FFieldId, FDelegateHandle>& Bound : BoundCombatFields)
		{
			if (Bound.Value.IsValid())
			{
				Combat->RemoveFieldValueChangedDelegate(Bound.Key, Bound.Value);
			}
		}
	}
	BoundCombatFields.Reset();
	BoundCombatViewModel.Reset();

	BoundFields.Reset();
	BoundViewModel.Reset();
}

void UBNHUDLayout::HandleMatchFieldChanged(UObject* Source, UE::FieldNotification::FFieldId FieldId)
{
	Refresh();
}

void UBNHUDLayout::Refresh()
{
	const UBNVM_Match* Match = BoundViewModel.Get();
	if (!BannerText || !Match)
	{
		return;
	}

	const FText Banner = Match->GetPhaseBannerText();
	BannerText->SetText(Banner);
	// Hidden, not Collapsed: the banner is alone at its anchor, nothing reflows around it —
	// and a Collapse/restore cycle would re-layout for no reason.
	BannerText->SetVisibility(Banner.IsEmpty() ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible);
}
