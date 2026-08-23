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

	// MaxShield == 0 means this mode has no shields, not that yours are down. A 273x20 bar
	// pinned at empty for the whole match is not honest-unknown, it is furniture — and it is
	// what makes the health bar below it look missing.
	const bool bShields = Combat && Combat->HasShields();
	if (ShieldBar)
	{
		ShieldBar->SetVisibility(bShields ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		ShieldBar->SetPercent(bLive ? Combat->GetShieldPercent() : 0.f);
		ShieldBar->SetFillColorAndOpacity(bLive ? BNUIColors::Shield : BNUIColors::Dead);
	}
	if (ShieldText)
	{
		ShieldText->SetVisibility(bShields ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		ShieldText->SetText(bLive ? FText::AsNumber(Combat->GetShieldValue()) : FText::FromString(TEXT("—")));
		// The dash dims too (critic): a confident live-cyan "—" half-honours honest-unknown.
		ShieldText->SetColorAndOpacity(FSlateColor(bLive ? BNUIColors::Shield : BNUIColors::Dead));
	}

	// Hidden-until-damaged, on the WHOLE element: a full health bar says nothing, and the old
	// module's version hid only the fill and left the frame drawing. Unknown also hides it —
	// an empty frame before the ASC binds would read as "no health", which is a lie.
	// Hidden-until-damaged is a rule about a bar that sits UNDER a shield: a full health bar
	// says nothing when a shield is the thing actually protecting you. With shields off, health
	// IS the vitals, and hiding it leaves the player with no health readout at all — which is
	// exactly what "I do not see the healthbar" was.
	const bool bHealthVisible = bLive && (!bShields || Combat->GetHealthPercent() < 1.f);
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
