// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/OSAttributeWidget.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Core/OSPlayerState.h"
#include "CommonTextBlock.h"
#include "Components/ProgressBar.h"
#include "Core/OSGameState.h"
#include "GAS/Attributes/OSAttributeSet.h"
#include "UI/OSHUDWidget.h"

void UOSAttributeWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UOSAttributeWidget::NativeDestruct()
{
	Super::NativeDestruct();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ChaserDelayTimer);
		World->GetTimerManager().ClearTimer(ChaserTimer);
	}

	if (GetOwningAttributeSet())
	{
		UE_LOG(LogTemp, Warning, TEXT("Nameplate UOSAttributeWidget::NativeDestruct"));
		GetOwningAttributeSet()->OnAttributeHealthChanged.RemoveAll(this);
		GetOwningAttributeSet()->OnAttributeStaminaChanged.RemoveAll(this);
	}
}

void UOSAttributeWidget::OnGASReady_Implementation(UAbilitySystemComponent* ASC, UOSAttributeSet* AS)
{
	
	bGASReady = true;
	FinalizeAttributeWidget();
}

void UOSAttributeWidget::InitAttributeWidget(UCommonTextBlock* InName, UProgressBar* InHealthBar, UProgressBar* InChaser, UProgressBar* InStaminaBar, UProgressBar* InAuraBar)
{
	if (!bInitialized)
	{
		Name = InName;
		HealthBar = InHealthBar;
		ChaserHealthBar = InChaser;
		StaminaBar = InStaminaBar;
		AuraBar = InAuraBar;
		bInitialized = true;
		FinalizeAttributeWidget();
	}
}

void UOSAttributeWidget::FinalizeAttributeWidget()
{
	if (bInitialized && bGASReady)
	{
		if (HealthBar && ChaserHealthBar && StaminaBar)
		{
			HealthBar->SetPercent(1.f);
			ChaserHealthBar->SetPercent(1.f);
			StaminaBar->SetPercent(1.f);
			CurrentHealthPercent = 1.f;
			HealthBar->SetFillColorAndOpacity(HealthFullColor);
		}
		
		if (GetOwningAttributeSet())
		{
			UE_LOG(LogTemp, Warning, TEXT("Nameplate UOSAttributeWidget::FinalizeAttributeWidget"));
			OwningAttributeSet->OnAttributeHealthChanged.AddUObject(this, &UOSAttributeWidget::OnHealthChanged);
			OwningAttributeSet->OnAttributeStaminaChanged.AddUObject(this, &UOSAttributeWidget::OnStaminaChanged);
			OwningAttributeSet->OnAttributeAuraChanged.AddUObject(this, &UOSAttributeWidget::OnAuraChanged);
			OnHealthChanged(OwningAttributeSet->GetHealth(), OwningAttributeSet->GetMaxHealth(), FOSDeathEventInfo());
			OnStaminaChanged(OwningAttributeSet->GetStamina(), OwningAttributeSet->GetMaxStamina());
			OnAuraChanged(OwningAttributeSet->GetAura(), OwningAttributeSet->GetMaxAura());
		}

		if (Name && GetOwningPlayerState())
		{
			SetPlayerName(FText::FromString(GetOwningPlayerState()->GetPlayerName()));
		}
	}
}

void UOSAttributeWidget::SetPlayerName(const FText& InName)
{
	if (Name)
	{
		Name->SetText(InName);
	}
}

void UOSAttributeWidget::OnHealthChanged(float Current, float Max, const FOSDeathEventInfo& DeathEvent)
{
	float Percent = Current / Max;
	Percent = FMath::Clamp(Percent, 0.0f, 1.0f);

	if (HealthBar)
	{
		HealthBar->SetPercent(Percent);
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
	
	if (Percent < CurrentHealthPercent)
	{
		CurrentHealthPercent = Percent;
		if (HealthBar)
		{
			HealthBar->SetPercent(Percent);
		}
	}
	else
	{
		TargetHealthPercent = Percent;
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(HealthRegenTimer);
			World->GetTimerManager().SetTimer(HealthRegenTimer, this, &UOSAttributeWidget::HealthRegenTimerTick, 0.01f, true);
		}
	}
	
	if (ChaserHealthBar)
	{
		TargetChaserPercent = Percent;
		float CurrentChase = ChaserHealthBar->GetPercent();

		if (CurrentChase > Percent)
		{
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().ClearTimer(ChaserDelayTimer);
				World->GetTimerManager().ClearTimer(ChaserTimer);
				World->GetTimerManager().SetTimer(ChaserDelayTimer, this, &UOSAttributeWidget::StartChaserTick, ChaseDelay, false, -1);
			}
			else
			{
				ChaserHealthBar->SetPercent(Percent);
			}
		}
	}
}

void UOSAttributeWidget::OnStaminaChanged(float Current, float Max)
{
	float Percent = Current / Max;
	Percent = FMath::Clamp(Percent, 0.0f, 1.0f);
	
	if (UWorld* World = GetWorld())
	{
		TargetStaminaPercent = Percent;
		World->GetTimerManager().ClearTimer(StaminaRegenTimer);
		World->GetTimerManager().SetTimer(StaminaRegenTimer, this, &UOSAttributeWidget::StaminaRegenTimerTick, 0.01f, true);
	}
}

void UOSAttributeWidget::OnAuraChanged(float Current, float Max)
{
	float Percent = Current / Max;
	Percent = FMath::Clamp(Percent, 0.0f, 1.0f);
	
	if (AuraBar)
	{
		AuraBar->SetPercent(Percent);
	}
}

void UOSAttributeWidget::StartChaserTick()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ChaserTimer);
		World->GetTimerManager().SetTimer(ChaserTimer, this, &UOSAttributeWidget::ChaserTimerTick, 0.01f, true, -1);
	}
}

void UOSAttributeWidget::ChaserTimerTick()
{
	if (!ChaserHealthBar) return;

	float Current = ChaserHealthBar->GetPercent();
	float NewValue = FMath::FInterpTo(Current, TargetChaserPercent, 0.01f, ChaserLerpSpeed);

	ChaserHealthBar->SetPercent(NewValue);

	if (FMath::IsNearlyEqual(NewValue, TargetChaserPercent, 0.001f))
	{
		ChaserHealthBar->SetPercent(TargetChaserPercent);
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(ChaserDelayTimer);
			World->GetTimerManager().ClearTimer(ChaserTimer);
		}
	}
}

void UOSAttributeWidget::HealthRegenTimerTick()
{
	if (HealthBar)
	{
		float Current = HealthBar->GetPercent();
		float NewValue = FMath::FInterpTo(Current, TargetHealthPercent, 0.01f, SmoothLerpSpeedIncrease);
		
		HealthBar->SetPercent(NewValue);

		if (FMath::IsNearlyEqual(NewValue, TargetHealthPercent, 0.001f))
		{
			HealthBar->SetPercent(TargetHealthPercent);
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().ClearTimer(HealthRegenTimer);
			}
		}
	}
}

void UOSAttributeWidget::StaminaRegenTimerTick()
{
	if (StaminaBar)
	{
		float Current = StaminaBar->GetPercent();
		float NewValue = FMath::FInterpTo(Current, TargetStaminaPercent, 0.01f, SmoothLerpSpeedIncrease);

		StaminaBar->SetPercent(NewValue);

		if (FMath::IsNearlyEqual(NewValue, TargetStaminaPercent, 0.001f))
		{
			StaminaBar->SetPercent(TargetStaminaPercent);
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().ClearTimer(StaminaRegenTimer);
			}
		}
	}
}
