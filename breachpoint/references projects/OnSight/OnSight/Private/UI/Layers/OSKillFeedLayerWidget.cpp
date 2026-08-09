// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Layers/OSKillFeedLayerWidget.h"

#include "Components/VerticalBox.h"
#include "Core/OSGameState.h"
#include "Core/OSPlayerController.h"
#include "Core/OSPlayerState.h"
static FString TruncateName(const FString& Name, int32 MaxChars = 12)
{
	if (Name.Len() <= MaxChars)
		return Name;

	return Name.Left(MaxChars - 3) + TEXT("...");
}

static void TrimFeedEntries(UVerticalBox* Feed, int32 Max)
{
	if (!Feed) return;
	while (Feed->GetChildrenCount() > Max)
	{
		Feed->RemoveChildAt(0);
	}
}

void UOSKillFeedLayerWidget::NativeDestruct()
{
	if (_gameState)
		_gameState->OnKillfeedEntryAdded.RemoveAll(this);

	Super::NativeDestruct();
}

void UOSKillFeedLayerWidget::RebuildFromGameState()
{
	if (!MainKillFeed) return;
	if (auto gs = Cast<AOSGameState>(GetWorld()->GetGameState()))
	{
		gs->OnKillfeedEntryAdded.RemoveAll(this);
		gs->OnKillfeedEntryAdded.AddDynamic(this, &UOSKillFeedLayerWidget::OnKillFeedEntry);
	}
}

void UOSKillFeedLayerWidget::OnKillFeedEntry( const FOSKillfeedEntry& KillfeedEntry )
{
	UE_LOG(LogTemp, Warning, TEXT("[Killfeed][Client] Inst=%s Vict=%s"),
		*GetNameSafe(KillfeedEntry.DeathInfo.InstigatorPS),
		*GetNameSafe(KillfeedEntry.DeathInfo.VictimPS));
	OnPlayerDeath(KillfeedEntry.DeathInfo);
	OnKillFeedHook(KillfeedEntry);
}

void UOSKillFeedLayerWidget::OnPlayerDeath( const FOSDeathEventInfo& DeathEventInfo )
{
	auto v = DeathEventInfo.GetVictimPlayerState(GetWorld());
	if (!v) return;
	auto victim = Cast<AOSPlayerState>(v);
	if (!victim) return;
	auto i = DeathEventInfo.GetInstigatorPlayerState(GetWorld());
	
	auto mainFeed = CreateWidget<UOSKillFeedComponentWidget>(GetOwningPlayer(), KillFeedComponentClass);
	if (!mainFeed) return;
	
	MainKillFeed->AddChild(mainFeed);
	TrimFeedEntries(MainKillFeed, MaxKillFeedEntries);
	FString feedString;
	auto instigator = Cast<AOSPlayerState>(i);
	FLinearColor clr = NeutralColor;
	if (instigator == GetOwningPlayerState()) clr = WinColor;
	if (victim == GetOwningPlayerState()) clr = LoseColor;
	
	auto VictimSafeName = TruncateName(victim->GetPlayerName(), TruncateAt);
	EOSKillFeedType type = EOSKillFeedType::GLOBAL;
	
	if (instigator && instigator != victim)
	{
		auto InstigatorSafeName = TruncateName(instigator->GetPlayerName(), TruncateAt);
		if (auto pFeed = CreateWidget<UOSKillFeedComponentWidget>(GetOwningPlayer(), KillFeedComponentClass))
		{
			
			if (instigator == GetOwningPlayerState())
			{
				feedString = FString::Printf(TEXT("[KILLED] %s"), *VictimSafeName);
				type = EOSKillFeedType::INSTIGATOR;
			}
			else if (victim == GetOwningPlayerState())
			{
				feedString = FString::Printf(TEXT("[KILLED BY] %s"),  *InstigatorSafeName);
				type = EOSKillFeedType::VICTIM;
			}
			
			if (type != EOSKillFeedType::GLOBAL && PersonalKillFeed)
			{
				PersonalKillFeed->AddChild(pFeed);
				TrimFeedEntries(PersonalKillFeed, MaxPersonalFeedEntries);
				pFeed->PrintFeed(FText::FromString(feedString), clr);
			}
		}
		
		feedString = FString::Printf(TEXT("%s [KILLED] %s"), *InstigatorSafeName, *VictimSafeName);
		mainFeed->PrintFeed(FText::FromString(feedString), clr);
	}
	else
	{
		feedString = FString::Printf(TEXT("%s [EXPERIENCED A SKILL ISSUE]"),  *VictimSafeName);
		mainFeed->PrintFeed(FText::FromString(feedString), clr);
	}

	OnPlayerDeathHook(DeathEventInfo, type);
}
