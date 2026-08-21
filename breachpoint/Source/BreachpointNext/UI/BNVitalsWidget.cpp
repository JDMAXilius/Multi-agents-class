#include "UI/BNVitalsWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/LocalPlayer.h"
#include "INotifyFieldValueChanged.h"
#include "UI/BNUIManager.h"
#include "UI/BNUITypes.h"
#include "UI/BNViewModels.h"

void UBNVitalsWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::HitTestInvisible);

	// The tokens, set from C++ so the WBP never types a hex.
	if (ShieldBar) { ShieldBar->SetFillColorAndOpacity(BNUIColors::Shield); }
	if (HealthBar) { HealthBar->SetFillColorAndOpacity(BNUIColors::Health); }
	if (ShieldText) { ShieldText->SetColorAndOpacity(FSlateColor(BNUIColors::Shield)); }
	if (HealthText) { HealthText->SetColorAndOpacity(FSlateColor(BNUIColors::Health)); }

	Refresh();
}

void UBNVitalsWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UBNUIManager* Manager = UBNUIManager::Get(this);
	UBNVM_Combat* Combat = Manager ? Manager->GetCombatViewModel(GetOwningLocalPlayer()) : nullptr;
	if (!Combat)
	{
		return;
	}
	BoundViewModel = Combat;

	BindCombatField(Combat, UBNVM_Combat::FFieldNotificationClassDescriptor::HealthPercent);
	BindCombatField(Combat, UBNVM_Combat::FFieldNotificationClassDescriptor::ShieldPercent);
	BindCombatField(Combat, UBNVM_Combat::FFieldNotificationClassDescriptor::HealthValue);
	BindCombatField(Combat, UBNVM_Combat::FFieldNotificationClassDescriptor::ShieldValue);
	BindCombatField(Combat, UBNVM_Combat::FFieldNotificationClassDescriptor::VitalsState);

	Refresh();
}

void UBNVitalsWidget::NativeDestruct()
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

	Super::NativeDestruct();
}

void UBNVitalsWidget::BindCombatField(UBNVM_Combat* Combat, UE::FieldNotification::FFieldId FieldId)
{
	if (FieldId.IsValid())
	{
		BoundFields.Emplace(FieldId, Combat->AddFieldValueChangedDelegate(FieldId,
			INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateUObject(this, &UBNVitalsWidget::HandleCombatFieldChanged)));
	}
}

void UBNVitalsWidget::HandleCombatFieldChanged(UObject* Source, UE::FieldNotification::FFieldId FieldId)
{
	Refresh();
}

void UBNVitalsWidget::Refresh()
{
	const UBNVM_Combat* Combat = BoundViewModel.Get();
	const bool bLive = Combat && Combat->GetVitalsState() == EBNUIDataState::Live;

	if (ShieldBar)
	{
		ShieldBar->SetPercent(bLive ? Combat->GetShieldPercent() : 0.f);
		ShieldBar->SetFillColorAndOpacity(bLive ? BNUIColors::Shield : BNUIColors::Dead);
	}
	if (ShieldText)
	{
		ShieldText->SetText(bLive ? FText::AsNumber(Combat->GetShieldValue()) : FText::FromString(TEXT("—")));
	}

	// Hidden-until-damaged, on the WHOLE element: a full health bar says nothing, and the old
	// module's version hid only the fill and left the frame drawing. Unknown also hides it —
	// an empty frame before the ASC binds would read as "no health", which is a lie.
	const bool bHealthVisible = bLive && Combat->GetHealthPercent() < 1.f;
	if (HealthBar)
	{
		HealthBar->SetVisibility(bHealthVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
		HealthBar->SetPercent(bLive ? Combat->GetHealthPercent() : 0.f);
	}
	if (HealthText)
	{
		HealthText->SetVisibility(bHealthVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
		HealthText->SetText(bLive ? FText::AsNumber(Combat->GetHealthValue()) : FText::GetEmpty());
	}
}
