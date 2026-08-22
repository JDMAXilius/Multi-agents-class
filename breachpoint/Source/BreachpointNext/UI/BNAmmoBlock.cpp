#include "UI/BNAmmoBlock.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
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
	BindCombatField(Combat, UBNVM_Combat::FFieldNotificationClassDescriptor::WeaponIcon);
	BindCombatField(Combat, UBNVM_Combat::FFieldNotificationClassDescriptor::StowedWeaponName);
	BindCombatField(Combat, UBNVM_Combat::FFieldNotificationClassDescriptor::StowedWeaponIcon);

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

	// R7.1 — the silhouette. SetBrushFromSoftTexture takes the SOFT pointer and loads it itself,
	// so no sync load lands on a weapon swap; an unset row column hides the slot rather than
	// drawing the previous weapon's gun.
	if (WeaponIcon)
	{
		const TSoftObjectPtr<UTexture2D> Icon = Combat ? Combat->GetWeaponIcon() : TSoftObjectPtr<UTexture2D>();
		const bool bHasIcon = bLive && !Icon.IsNull();

		// ONLY on a real change (critic): Refresh is the handler for every bound field, and
		// MagAmmo fires per shot — re-issuing the brush each round cancels and restarts the
		// streaming request, so a gun picked up and fired immediately would never finish
		// loading its own icon.
		if (bHasIcon && Icon != AppliedIcon)
		{
			WeaponIcon->SetBrushFromSoftTexture(Icon, /*bMatchSize=*/false);
			AppliedIcon = Icon;
		}
		WeaponIcon->SetVisibility(bHasIcon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}

	// R7.3 — the stowed slot. Its own emptiness test: the stowed name is empty whenever there is
	// nothing to swap to, which is a different question from whether the HAND is known, so it is
	// not gated on bLive.
	const FText StowedName = Combat ? Combat->GetStowedWeaponName() : FText::GetEmpty();
	const bool bHasStowed = !StowedName.IsEmpty();
	if (StowedNameText)
	{
		StowedNameText->SetText(StowedName);
		StowedNameText->SetVisibility(bHasStowed ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	if (StowedIcon)
	{
		const TSoftObjectPtr<UTexture2D> Icon = Combat ? Combat->GetStowedWeaponIcon() : TSoftObjectPtr<UTexture2D>();
		const bool bHasStowedIcon = bHasStowed && !Icon.IsNull();
		// The same load-once guard as the hand's icon, for the same reason.
		if (bHasStowedIcon && Icon != AppliedStowedIcon)
		{
			StowedIcon->SetBrushFromSoftTexture(Icon, /*bMatchSize=*/false);
			AppliedStowedIcon = Icon;
		}
		StowedIcon->SetVisibility(bHasStowedIcon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
}
