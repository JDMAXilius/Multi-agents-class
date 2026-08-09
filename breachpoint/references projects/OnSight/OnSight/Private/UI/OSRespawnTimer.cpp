#include "UI/OSRespawnTimer.h"

#include "CommonTextBlock.h"
#include "Components/HorizontalBox.h"
#include "Core/OSPlayerState.h"

void UOSRespawnTimer::NativeConstruct()
{
	Super::NativeConstruct();
	RespawnHorizontalBox->SetVisibility(ESlateVisibility::Hidden);
}

void UOSRespawnTimer::NativeDestruct()
{
	Super::NativeDestruct();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RespawnCountdownTickHandle);
	}

	if (GetOwningPlayerState())
	{
		OwningPlayerState->OnRespawnCountdownChanged.RemoveAll(this);
	}
}

void UOSRespawnTimer::OnGASReady_Implementation(UAbilitySystemComponent* ASC, UOSAttributeSet* AS)
{
	Super::OnGASReady_Implementation(ASC, AS);
	FinalizeRespawnWidget();
}

void UOSRespawnTimer::InitRespawnWidget(UHorizontalBox* InRespawnHorizontalBox, UCommonTextBlock* InRespawnText)
{
	if (!bInitialized)
	{
		RespawnHorizontalBox = InRespawnHorizontalBox;
		RespawnCountdownText = InRespawnText;
		bInitialized = true;
	}
}

void UOSRespawnTimer::FinalizeRespawnWidget()
{
	if (bInitialized && GetOwningPlayerState())
	{
		OwningPlayerState->OnRespawnCountdownChanged.RemoveAll(this);
		OwningPlayerState->OnRespawnCountdownChanged.AddDynamic(this, &UOSRespawnTimer::OnRespawnCountdownChanged);
	}
}

void UOSRespawnTimer::OnRespawnSecondChanged_Implementation(int32 SecondsRemaining){}
void UOSRespawnTimer::OnRespawnCountdownTick_Implementation(float TimeRemaining){}
void UOSRespawnTimer::OnRespawnCountdownEnd_Implementation(){}
void UOSRespawnTimer::OnRespawnCountdownBegan_Implementation(float TimeRemaining){}

void UOSRespawnTimer::OnRespawnCountdownChanged(float TimeRemaining)
{
	LocalRespawnTimeRemaining = TimeRemaining;

	if (TimeRemaining > 0.f)
	{
		UE_LOG(LogTemp, Display, TEXT("Start Respawn Widget"));
		StartRespawnCountdownTick();
	}
	else
	{
		StopRespawnCountdownTick();
	}

	UpdateRespawnCountdownDisplay();
}

void UOSRespawnTimer::TickRespawnCountdown()
{
	int32 PrevSecond = FMath::CeilToInt(LocalRespawnTimeRemaining);

	LocalRespawnTimeRemaining -= 0.01f;
	if (LocalRespawnTimeRemaining <= 0.f)
	{
		LocalRespawnTimeRemaining = -1.f;
		StopRespawnCountdownTick();
	}

	int32 CurSecond = FMath::CeilToInt(LocalRespawnTimeRemaining);
	if (CurSecond != PrevSecond)
	{
		UpdateRespawnCountdownDisplay();
	}
}

void UOSRespawnTimer::UpdateRespawnCountdownDisplay()
{
	if (RespawnCountdownText && RespawnHorizontalBox && bInitialized)
	{
		if (LocalRespawnTimeRemaining < 0.f)
		{
			RespawnHorizontalBox->SetVisibility(ESlateVisibility::Collapsed);
			return;
		}
		
		const int32 Seconds = FMath::CeilToInt(LocalRespawnTimeRemaining);
		RespawnCountdownText->SetText(FText::FromString(FString::Printf(TEXT("%d"), Seconds)));
		RespawnHorizontalBox->SetVisibility(ESlateVisibility::Visible);
	}
}

void UOSRespawnTimer::StartRespawnCountdownTick()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RespawnCountdownTickHandle);
		World->GetTimerManager().SetTimer(RespawnCountdownTickHandle, this, &UOSRespawnTimer::TickRespawnCountdown, 0.01f, true);
	}
}

void UOSRespawnTimer::StopRespawnCountdownTick()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RespawnCountdownTickHandle);
	}
}
