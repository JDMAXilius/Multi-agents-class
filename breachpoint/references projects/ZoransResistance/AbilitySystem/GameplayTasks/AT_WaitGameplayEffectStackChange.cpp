// Copyright Zollpa LLC


#include "AT_WaitGameplayEffectStackChange.h"

UWaitGameplayEffectStackChange* UWaitGameplayEffectStackChange::WaitGameplayEffectStackChange(UAbilitySystemComponent* AbilitySystemComponent, TSubclassOf<UGameplayEffect> InEffectToTrack)
{
	UWaitGameplayEffectStackChange* WaitGameplayEffectStackChange = NewObject<UWaitGameplayEffectStackChange>();
	WaitGameplayEffectStackChange->ASC = AbilitySystemComponent;
	WaitGameplayEffectStackChange->EffectToTrack = InEffectToTrack;

	if (!IsValid(AbilitySystemComponent) || !IsValid(InEffectToTrack))
	{
		WaitGameplayEffectStackChange->EndTask();
		return nullptr;
	}

	AbilitySystemComponent->OnActiveGameplayEffectAddedDelegateToSelf.AddUObject(WaitGameplayEffectStackChange, &UWaitGameplayEffectStackChange::OnActiveGameplayEffectAddedCallback);
	AbilitySystemComponent->OnAnyGameplayEffectRemovedDelegate().AddUObject(WaitGameplayEffectStackChange, &UWaitGameplayEffectStackChange::OnRemoveGameplayEffectCallback);

	return WaitGameplayEffectStackChange;
}

void UWaitGameplayEffectStackChange::EndTask()
{
	if (IsValid(ASC))
	{
		ASC->OnActiveGameplayEffectAddedDelegateToSelf.RemoveAll(this);
		ASC->OnAnyGameplayEffectRemovedDelegate().RemoveAll(this);
	}

	SetReadyToDestroy();
	MarkAsGarbage();
}

void UWaitGameplayEffectStackChange::OnActiveGameplayEffectAddedCallback(UAbilitySystemComponent* Target, const FGameplayEffectSpec& SpecApplied, FActiveGameplayEffectHandle ActiveHandle)
{
	if (IsValid(SpecApplied.Def) && SpecApplied.Def.GetClass() == EffectToTrack)
	{
		ASC->OnGameplayEffectStackChangeDelegate(ActiveHandle)->AddUObject(this, &UWaitGameplayEffectStackChange::GameplayEffectStackChanged);
		const int32 NewStackCount = SpecApplied.StackCount;
		OnGameplayEffectStackChange.Broadcast(ActiveHandle, NewStackCount, OldStackCount);
		OldStackCount = NewStackCount;
	}
}

void UWaitGameplayEffectStackChange::OnRemoveGameplayEffectCallback(const FActiveGameplayEffect& EffectRemoved)
{
	if (EffectRemoved.Spec.Def.GetClass() == EffectToTrack)
	{
		const int32 NewStackCount = EffectRemoved.Spec.StackCount;
		OnGameplayEffectStackChange.Broadcast(EffectRemoved.Handle, 0, NewStackCount);
		//OldStackCount = NewStackCount;
	}
}

void UWaitGameplayEffectStackChange::GameplayEffectStackChanged(FActiveGameplayEffectHandle EffectHandle, int32 NewStackCount, int32 PreviousStackCount)
{
	//const int32 NewStackCount = ASC->GetCurrentStackCount(EffectHandle);
	OnGameplayEffectStackChange.Broadcast(EffectHandle, NewStackCount, OldStackCount);
	OldStackCount = NewStackCount;
}
