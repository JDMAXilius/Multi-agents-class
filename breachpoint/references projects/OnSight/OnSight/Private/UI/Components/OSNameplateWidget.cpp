#include "UI/Components/OSNameplateWidget.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/ProgressBar.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Characters/OSCharacter.h"
#include "Core/OSPlayerState.h"
#include "GAS/Attributes/OSAttributeSet.h"

void UOSNameplateWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UOSNameplateWidget::NativeDestruct()
{
	Super::NativeDestruct();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ChaserDelayTimer);
		World->GetTimerManager().ClearTimer(ChaserTimer);
	}

	if (AttributeSet)
	{
		AttributeSet->OnAttributeHealthChanged.RemoveAll(this);
	}
}

void UOSNameplateWidget::InitNameplate(UBorder* InTeamBG, UCommonTextBlock* InName, UProgressBar* InHealthBar, UProgressBar* InChaser, UCommonTextBlock* InCurrentHealth, UCommonTextBlock* InTotalHealth)
{
	if (!bInitialized)
	{
		Team_BG = InTeamBG;
		Name = InName;
		HealthBar = InHealthBar;
		ChaserHealthBar = InChaser;
		CurrentHealth = InCurrentHealth;
		TotalHealth = InTotalHealth;
		bInitialized = true;
		FinalizeNameplate();
	}
}

void UOSNameplateWidget::FinalizeNameplate()
{
	if (bInitialized && bCharacterSet)
	{
		if (GetOwningCharacter())
		{
			if (APlayerState* PlayerState = OwningCharacter->GetPlayerState())
			{
				OwningPlayerState = Cast<AOSPlayerState>(PlayerState);
				if (GetOwningPlayerState())
				{
					UpdateNameplateColor(GetOwningPlayerState());
					OwningPlayerState->OnTeamAssigned.AddUObject(this, &UOSNameplateWidget::UpdateNameplateColor);
				}
				
				SetPlayerName(FText::FromString(PlayerState->GetPlayerName()));
				if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PlayerState))
				{
					if (const UAttributeSet* AS = ASC->GetAttributeSet(UOSAttributeSet::StaticClass()))
					{
						if (const UOSAttributeSet* OSAS = Cast<UOSAttributeSet>(AS))
						{
							if (UOSAttributeSet* OSAS_Mutable = const_cast<UOSAttributeSet*>(OSAS))
							{
								AttributeSet = OSAS_Mutable;
								AttributeSet->OnAttributeHealthChanged.AddUObject(this, &UOSNameplateWidget::UpdateNameplateHealth);
								UpdateNameplateHealth(OSAS->GetHealth(), OSAS->GetMaxHealth(), FOSDeathEventInfo());
							}
						}
					}
				}
			}
		}
	}
}

void UOSNameplateWidget::UpdateNameplateHealth(float Health, float MaxHealth, const FOSDeathEventInfo& DeathEvent)
{
	SetHealthValues(Health, MaxHealth);
}

void UOSNameplateWidget::UpdateNameplateColor(AOSPlayerState* InPlayerState)
{
	if (const UWorld* World = GetWorld())
	{
		if (InPlayerState)
		{
			if (const APlayerController* LocalPC = World->GetFirstPlayerController())
			{
				if (const AOSPlayerState* LocalPS = LocalPC->GetPlayerState<AOSPlayerState>())
				{
					const int32 LocalTeam = LocalPS->GetTeamIndex();
					const int32 ThisTeam = InPlayerState->GetTeamIndex();
					if (LocalTeam >= 0 && ThisTeam >= 0)
					{
						bFriendly = (LocalTeam == ThisTeam);
					}

					if (Team_BG)
					{
						Team_BG->SetBrushColor(bFriendly ? FriendlyColor : EnemyColor);
						HealthBar->SetFillColorAndOpacity(EnemyColor);
					}
				}
			}
		}
	}
}

void UOSNameplateWidget::SetPlayerName(const FText& InName)
{
	if (Name)
	{
		Name->SetText(InName);
	}
}

void UOSNameplateWidget::SetHealthPercent(float Percent)
{
	if (!HealthBar)
		return;

	Percent = FMath::Clamp(Percent, 0.0f, 1.0f);
	HealthBar->SetPercent(Percent);

	if (bFriendly)
	{
		if (Percent < 0.5f)
		{
			const float localProgress = Percent/0.5f;
			HealthBar->SetFillColorAndOpacity(FLinearColor::LerpUsingHSV(HealthEmptyColor, HealthMidColor, localProgress));
		}
		else
		{
			const float localProgress = (Percent - 0.5f)/0.5f;
			HealthBar->SetFillColorAndOpacity(FLinearColor::LerpUsingHSV(HealthMidColor, HealthFullColor, localProgress));
		}
	}

	TargetChaserPercent = Percent;
	float CurrentChase = ChaserHealthBar->GetPercent();

	if (CurrentChase > Percent)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(ChaserDelayTimer);
			World->GetTimerManager().ClearTimer(ChaserTimer);
			
			bool bLoopChaser = false;
			World->GetTimerManager().SetTimer(ChaserDelayTimer, this, &UOSNameplateWidget::StartChaserTick, ChaseDelay, bLoopChaser, -1);
		}
	}
	else
	{
		ChaserHealthBar->SetPercent(Percent);
	}
}

void UOSNameplateWidget::SetHealthValues(float Current, float Max)
{
	if (CurrentHealth)
		CurrentHealth->SetText(FText::AsNumber(FMath::CeilToInt(Current)));

	if (TotalHealth)
		TotalHealth->SetText(FText::AsNumber(FMath::CeilToInt(Max)));

	if (Max > 0.f)
		SetHealthPercent(Current / Max);
}

void UOSNameplateWidget::SetOwningCharacter(AOSCharacter* InOwningCharacter)
{
	if (!bCharacterSet && IsValid(InOwningCharacter))
	{
		OwningCharacter = InOwningCharacter;
		bCharacterSet = true;
		FinalizeNameplate();
	}
}

void UOSNameplateWidget::StartChaserTick()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ChaserTimer);

		float ChaserInterval = 0.01f;
		bool bLoopChaser = true;
		World->GetTimerManager().SetTimer(ChaserTimer, this, &UOSNameplateWidget::ChaserTimerTick, ChaserInterval, bLoopChaser, -1);
	}
}

void UOSNameplateWidget::ChaserTimerTick()
{
	if (!ChaserHealthBar)
		return;

	float Current = ChaserHealthBar->GetPercent();
	float NewValue = FMath::FInterpTo(Current, TargetChaserPercent, 0.01f, ChaserLerpSpeed);

	ChaserHealthBar->SetPercent(NewValue);

	if (FMath::IsNearlyEqual(NewValue, TargetChaserPercent, 0.001f))
	{
		ChaserHealthBar->SetPercent(TargetChaserPercent);
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(ChaserTimer);
		}
	}
}
