// Copyright Zollpa LLC


#include "ZoransDurationWidget.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/PlayerState.h"

void UZoransDurationWidget::NativeConstruct()
{
	Super::NativeConstruct();

	OwnerAbilitySystemComponent = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwningPlayerState());
}

UAbilitySystemComponent* UZoransDurationWidget::GetOwningAbilitySystemComponent()
{
	return OwnerAbilitySystemComponent.Get() ? OwnerAbilitySystemComponent : OwnerAbilitySystemComponent = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwningPlayerState());
}

void UZoransDurationWidget::OnDurationExpired_Implementation()
{
	
}