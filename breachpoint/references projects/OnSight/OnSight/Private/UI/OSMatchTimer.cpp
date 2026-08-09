#include "UI/OSMatchTimer.h"
#include "CommonTextBlock.h"
#include "Core/OSGameState.h"

void UOSMatchTimer::NativeConstruct()
{
	Super::NativeConstruct();
}

void UOSMatchTimer::NativeDestruct()
{
	Super::NativeDestruct();

	if (AOSGameState* GS = GetOwningGameState())
	{
		GS->OnMatchTimeUpdated.RemoveAll(this);
	}
}

void UOSMatchTimer::OnGameStateReady(AOSGameState* GS)
{
	Super::OnGameStateReady(GS);

	bGameStateReady = true;
	FinalizeMatchTimerWidget();
}

void UOSMatchTimer::InitMatchTimer(UCommonTextBlock* InMatchTimeText)
{
	if (!bInitialized)
	{
		MatchTimeText = InMatchTimeText;
		bInitialized = true;
		FinalizeMatchTimerWidget();
	}
}

void UOSMatchTimer::FinalizeMatchTimerWidget()
{
	if (bInitialized && bGameStateReady)
	{
		if (GetOwningGameState())
		{
			OwningGameState->OnMatchTimeUpdated.AddDynamic(this, &UOSMatchTimer::UpdateMatchTime);
			if (bInitialized && MatchTimeText)
			{
				UpdateMatchTime(OwningGameState->MatchTime);
			}
		}
	}
}

void UOSMatchTimer::UpdateMatchTime(int32 NewMatchTime)
{
	if (MatchTimeText)
	{
		if (OwningGameState && OwningGameState->bMatchInProgress)
		{
			const int32 TotalSeconds = FMath::Max(0, OwningGameState->MatchTime);
			const int32 Minutes = TotalSeconds / 60;
			const int32 Seconds = TotalSeconds % 60;

			const FString Text = FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds);
			MatchTimeText->SetText(FText::FromString(Text));
		}
	}
}
