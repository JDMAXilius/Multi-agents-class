// Copyright Zollpa LLC


#include "ZoransUserInterfaceWidget.h"
#include "AbilitySystemGlobals.h"
#include "ZoransResistance/Player/ZoransPlayerState.h"

void UZoransUserInterfaceWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwningPlayerState()))
	{
		if (UZoransAbilitySystemComponent* ZASC = Cast<UZoransAbilitySystemComponent>(ASC))
		{
			OwnerAbilitySystemComponent = ZASC;
		}
	}
}

UZoransAbilitySystemComponent* UZoransUserInterfaceWidget::GetOwningAbilitySystemComponent()
{
	if (!OwnerAbilitySystemComponent.Get() && GetOwningPlayerState())
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwningPlayerState()))
		{
			if (UZoransAbilitySystemComponent* ZASC = Cast<UZoransAbilitySystemComponent>(ASC))
			{
				OwnerAbilitySystemComponent = ZASC;
			}
		}
	}
	
	return OwnerAbilitySystemComponent.Get();
}

void UZoransUserInterfaceWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	
	OnWidgetTick(InDeltaTime);

	WidgetTickDelegate.Broadcast(InDeltaTime);
}