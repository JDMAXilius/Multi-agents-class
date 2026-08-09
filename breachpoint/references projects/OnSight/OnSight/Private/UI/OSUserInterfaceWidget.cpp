#include "UI/OSUserInterfaceWidget.h"
#include "Core/OSPlayerState.h"
#include "GAS/Components/OSAbilitySystemComponent.h"
#include "GAS/Attributes/OSAttributeSet.h"
#include "AbilitySystemGlobals.h"
#include "Subsystems/OSSessionsSubsystem.h"
#include "Engine/GameInstance.h"

void UOSUserInterfaceWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Try to cache the ASC immediately if PlayerState is available
	if (AOSPlayerState* PS = GetOSPlayerState())
	{
		if (UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent())
		{
			if (UOSAbilitySystemComponent* OSASC = Cast<UOSAbilitySystemComponent>(ASC))
			{
				CachedAbilitySystemComponent = OSASC;
			}
		}
	}
}

UOSAbilitySystemComponent* UOSUserInterfaceWidget::GetOwningAbilitySystemComponent()
{
	// Return cached if available
	if (CachedAbilitySystemComponent.IsValid())
	{
		return CachedAbilitySystemComponent.Get();
	}

	// Try to get from PlayerState
	if (AOSPlayerState* PS = GetOSPlayerState())
	{
		if (UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent())
		{
			if (UOSAbilitySystemComponent* OSASC = Cast<UOSAbilitySystemComponent>(ASC))
			{
				CachedAbilitySystemComponent = OSASC;
				return OSASC;
			}
		}
	}

	return nullptr;
}

UOSAttributeSet* UOSUserInterfaceWidget::GetOwningAttributeSet()
{
	if (AOSPlayerState* PS = GetOSPlayerState())
	{
		return PS->GetAttributeSet();
	}

	return nullptr;
}

AOSPlayerState* UOSUserInterfaceWidget::GetOSPlayerState() const
{
	return GetOwningPlayerState<AOSPlayerState>();
}

FString UOSUserInterfaceWidget::GetPlayerName() const
{
	// Try to get the Sessions Subsystem from GameInstance
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			if (UOSSessionsSubsystem* SessionsSubsystem = GameInstance->GetSubsystem<UOSSessionsSubsystem>())
			{
				FString SteamName = SessionsSubsystem->GetSteamPlayerName();
				if (!SteamName.IsEmpty())
				{
					return SteamName;
				}
			}
		}
	}

	// Fallback to "Player Awesome" if Steam name is not available
	return TEXT("Player Awesome");
}

