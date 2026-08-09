#include "UI/OSCommonUserWidget.h"

#include "AbilitySystemGlobals.h"
#include "Characters/OSCharacter.h"
#include "Core/OSPlayerState.h"
#include "GAS/Attributes/OSAttributeSet.h"
#include "Core/OSGameState.h"

void UOSCommonUserWidget::NativeConstruct()
{
	Super::NativeConstruct();
	Init();
}

void UOSCommonUserWidget::NativeDestruct()
{
	Super::NativeDestruct();
}

void UOSCommonUserWidget::NotifyGASReady()
{
	if (OwningASC && OwningAttributeSet)
	{
		OnGASReady(OwningASC, OwningAttributeSet);
		OnGASReady_Implementation(OwningASC, OwningAttributeSet);
	}
}

void UOSCommonUserWidget::Init()
{
	if (bInitialized) return;
	
	if (UWorld* World = GetWorld())
	{
		if (AOSGameState* GameState = World->GetGameState<AOSGameState>())
		{
			OwningGameState = GameState;
		}
	}
	OnGameStateReady(OwningGameState);
	
	if (APlayerController* LocalPC = GetOwningPlayer())
	{
		if (AOSCharacter* Character = Cast<AOSCharacter>(LocalPC->GetPawn()))
		{
			OwningCharacter = Character;
			if (APlayerState* PlayerState = OwningCharacter->GetPlayerState())
			{
				OwningPlayerState = Cast<AOSPlayerState>(PlayerState);
				if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PlayerState))
				{
					OwningASC = ASC;
					if (const UAttributeSet* AS = ASC->GetAttributeSet(UOSAttributeSet::StaticClass()))
					{
						if (const UOSAttributeSet* OSAS = Cast<UOSAttributeSet>(AS))
						{
							if (UOSAttributeSet* OSAS_Mutable = const_cast<UOSAttributeSet*>(OSAS))
							{
								OwningAttributeSet = OSAS_Mutable;
							}
						}
					}
				}
			}
		}
	}
	NotifyGASReady();
}
