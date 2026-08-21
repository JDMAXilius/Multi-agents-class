#include "UI/BNHUDLayout.h"
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

	Refresh();
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
