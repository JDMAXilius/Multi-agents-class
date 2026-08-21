#include "UI/BNAmmoBlock.h"
#include "Components/TextBlock.h"
#include "Engine/LocalPlayer.h"
#include "INotifyFieldValueChanged.h"
#include "UI/BNUIManager.h"
#include "UI/BNUITypes.h"
#include "UI/BNViewModels.h"

void UBNAmmoBlock::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	if (WeaponNameText) { WeaponNameText->SetColorAndOpacity(FSlateColor(BNUIColors::InkDim)); }
	if (ReserveAmmoText) { ReserveAmmoText->SetColorAndOpacity(FSlateColor(BNUIColors::InkDim)); }
	Refresh();
}

void UBNAmmoBlock::NativeConstruct()
{
	Super::NativeConstruct();

	UBNUIManager* Manager = UBNUIManager::Get(this);
	UBNVM_Combat* Combat = Manager ? Manager->GetCombatViewModel(GetOwningLocalPlayer()) : nullptr;
	if (!Combat)
	{
		return;
	}
	BoundViewModel = Combat;

	BindCombatField(Combat, UBNVM_Combat::FFieldNotificationClassDescriptor::WeaponName);
	BindCombatField(Combat, UBNVM_Combat::FFieldNotificationClassDescriptor::MagAmmo);
	BindCombatField(Combat, UBNVM_Combat::FFieldNotificationClassDescriptor::ReserveAmmo);
	BindCombatField(Combat, UBNVM_Combat::FFieldNotificationClassDescriptor::EquipmentState);

	Refresh();
}

void UBNAmmoBlock::NativeDestruct()
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

void UBNAmmoBlock::BindCombatField(UBNVM_Combat* Combat, UE::FieldNotification::FFieldId FieldId)
{
	if (FieldId.IsValid())
	{
		BoundFields.Emplace(FieldId, Combat->AddFieldValueChangedDelegate(FieldId,
			INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateUObject(this, &UBNAmmoBlock::HandleCombatFieldChanged)));
	}
}

void UBNAmmoBlock::HandleCombatFieldChanged(UObject* Source, UE::FieldNotification::FFieldId FieldId)
{
	Refresh();
}

void UBNAmmoBlock::Refresh()
{
	const UBNVM_Combat* Combat = BoundViewModel.Get();
	const bool bLive = Combat && Combat->GetEquipmentState() == EBNUIDataState::Live;
	const FText Dash = FText::FromString(TEXT("—"));

	if (WeaponNameText)
	{
		WeaponNameText->SetText(bLive ? Combat->GetWeaponName() : Dash);
	}
	if (MagAmmoText)
	{
		const bool bHasMag = bLive && Combat->GetMagAmmo() != INDEX_NONE;
		MagAmmoText->SetText(bHasMag ? FText::AsNumber(Combat->GetMagAmmo()) : Dash);
	}
	if (ReserveAmmoText)
	{
		const bool bHasReserve = bLive && Combat->GetReserveAmmo() != INDEX_NONE;
		ReserveAmmoText->SetText(bHasReserve ? FText::AsNumber(Combat->GetReserveAmmo()) : Dash);
	}
}
