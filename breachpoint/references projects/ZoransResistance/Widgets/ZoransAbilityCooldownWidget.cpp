// Copyright Zollpa LLC


#include "ZoransAbilityCooldownWidget.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "ZoransUserInterfaceWidget.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/KismetMathLibrary.h"
#include "ZoransResistance/AbilitySystem/ZoransAbilitySystemComponent.h"
#include "ZoransResistance/AbilitySystem/ZoransGameplayAbility.h"

void UZoransAbilityCooldownWidget::NativeConstruct()
{
	Super::NativeConstruct();
	InitTrackedAbilityValues();
	return;
	
	OwnerAbilitySystemComponent = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwningPlayerState());
	
	if (OwnerAbilitySystemComponent.Get())
	{
		if (InitTrackedAbilityValues())
		{
			// do nothing, it is being tracked
			Super::NativeConstruct();
			return;			
		}
		
		bool bAbilityFound = false;
		TArray<FGameplayAbilitySpecHandle> AbilityList;
		OwnerAbilitySystemComponent->GetAllAbilities(AbilityList);
		if (!AbilityList.IsEmpty() && AbilityCooldownTag.IsValid())
		{
			for (auto const &AbilityHandle : AbilityList)
			{
				if (const FGameplayAbilitySpec* Ability = OwnerAbilitySystemComponent->FindAbilitySpecFromHandle(AbilityHandle))
				{
					if (IsValid(Ability->Ability))
					{
						if (Ability->GetPrimaryInstance()->AbilityTags.HasTag(AbilityCooldownTag))
						{
							if (UZoransGameplayAbility* ZGA = Cast<UZoransGameplayAbility>(Ability->GetPrimaryInstance()))
							{
								bAbilityFound = true;
								TrackedAbility = ZGA;
								InitTrackedAbilityValues();
								break;
							}
						}
					}
				}
			}
		}

		if (!bAbilityFound)
		{
			if (UZoransAbilitySystemComponent* ZoransAbilitySystemComponent = Cast<UZoransAbilitySystemComponent>(OwnerAbilitySystemComponent))
			{
				{
					ZoransAbilitySystemComponent->OnAbilityGrantedDelegate.AddDynamic(this, &ThisClass::BindAbilityCooldown);
				}
			}
		}
	}
}

void UZoransAbilityCooldownWidget::UpdateDuration(float& NewDuration)
{
	if (OwnerAbilitySystemComponent && !DurationQueryTags.IsEmpty())
	{
		NewDuration = 0;
		
		float InitialDuration = 0.f;
		float TimeRemaining = 0.f;
		
		TArray<TPair<float, float>> TimeRemainingAndDuration = OwnerAbilitySystemComponent->GetActiveEffectsTimeRemainingAndDuration(DurationQueryTags);

		if (TimeRemainingAndDuration.Num() > 0)
		{
			int32 BestIndex = 0;
			
			float LongestTime = TimeRemainingAndDuration[0].Key;

			for (int32 Index = 1; Index < TimeRemainingAndDuration.Num(); ++Index)
			{
				if (TimeRemainingAndDuration[Index].Key > LongestTime)
				{
					LongestTime = TimeRemainingAndDuration[Index].Key;

					BestIndex = Index;
				}
			}

			TimeRemaining = TimeRemainingAndDuration[BestIndex].Key;
			InitialDuration = TimeRemainingAndDuration[BestIndex].Value;
		}
		
		NewDuration = TimeRemaining > 0.05f ? UKismetMathLibrary::SafeDivide(TimeRemaining, InitialDuration) : 0.f;
	}
}

UAbilitySystemComponent* UZoransAbilityCooldownWidget::GetOwningAbilitySystemComponent()
{
	return OwnerAbilitySystemComponent.Get() ? OwnerAbilitySystemComponent : OwnerAbilitySystemComponent = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwningPlayerState());
}

void UZoransAbilityCooldownWidget::BindAbilityCooldown(const FGameplayAbilitySpec& AbilitySpec)
{
	if (AbilitySpec.Ability->AbilityTags.HasTagExact(AbilityCooldownTag))
	{
		if (IsValid(AbilitySpec.GetPrimaryInstance()))
		{
			if (UZoransGameplayAbility* ZGA = Cast<UZoransGameplayAbility>(AbilitySpec.GetPrimaryInstance()))
			{
				TrackedAbility = ZGA;
				if (InitTrackedAbilityValues())
				{
					Cast<UZoransAbilitySystemComponent>(OwnerAbilitySystemComponent)->OnAbilityGrantedDelegate.RemoveDynamic(this, &ThisClass::UZoransAbilityCooldownWidget::BindAbilityCooldown);
					TrackedAbilityUpdated(TrackedAbility);
				}
			}
		}
	}
}

void UZoransAbilityCooldownWidget::ListenForGameplayEffectAdded(UAbilitySystemComponent* AbilitySystemComponent, const FGameplayEffectSpec& GameplayEffectSpec, FActiveGameplayEffectHandle ActiveGameplayEffectHandle)
{
	const int32 ReplicationKey = OwnerAbilitySystemComponent->GetActiveGameplayEffect(ActiveGameplayEffectHandle)->ReplicationKey;
	
	if (GameplayEffectSpec.DynamicGrantedTags.HasTagExact(AbilityCooldownTag) && ReplicationKey >= 0)
	{
		On_DurationStarted();
	}
}

void UZoransAbilityCooldownWidget::ListenForGameplayEffectRemoval(const FActiveGameplayEffect& ActiveGameplayEffect)
{
	const FGameplayTagContainer CooldownEffectTags = ActiveGameplayEffect.Spec.DynamicGrantedTags;
	const int32 ReplicationKey = ActiveGameplayEffect.ReplicationKey;
	
	if (CooldownEffectTags.Num() > 0 && CooldownEffectTags.HasTagExact(AbilityCooldownTag) && ReplicationKey >= 0)
	{
		On_DurationFinished();
	}
}

void UZoransAbilityCooldownWidget::NativeDestruct()
{
	if (OwnerAbilitySystemComponent)
	{
		OwnerAbilitySystemComponent->OnActiveGameplayEffectAddedDelegateToSelf.Remove(EffectAddedHandle);
		OwnerAbilitySystemComponent->OnActiveGameplayEffectAddedDelegateToSelf.Remove(EffectRemovedHandle);
	}
	
	Super::NativeDestruct();
}

bool UZoransAbilityCooldownWidget::InitTrackedAbilityValues()
{
	bool bTrackedAbilityValid = false;

	OwnerAbilitySystemComponent = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwningPlayerState());
	
	if (OwnerAbilitySystemComponent.Get() && IsValid(TrackedAbility))
	{
		TrackedAbility->InputPressedDelegate.AddDynamic(this, &UZoransAbilityCooldownWidget::On_InputPressed);
		
		CooldownMaterial = TrackedAbility->CooldownMaterial;
		InputAction = TrackedAbility->ActivationInputAction;
		
		DurationQueryTags = FGameplayEffectQuery::MakeQuery_MatchAllOwningTags(FGameplayTagContainer(TrackedAbility->AbilityCooldownTag));
		
		EffectAddedHandle = OwnerAbilitySystemComponent->OnActiveGameplayEffectAddedDelegateToSelf.AddUObject(this, &UZoransAbilityCooldownWidget::ListenForGameplayEffectAdded);
		EffectRemovedHandle = OwnerAbilitySystemComponent->OnAnyGameplayEffectRemovedDelegate().AddUObject(this, &UZoransAbilityCooldownWidget::ListenForGameplayEffectRemoval);

		bTrackedAbilityValid = true;
	}

	return bTrackedAbilityValid;
}