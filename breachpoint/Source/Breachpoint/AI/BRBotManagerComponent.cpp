#include "AI/BRBotManagerComponent.h"

#include "AI/BRBotController.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

UBRBotManagerComponent::UBRBotManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedByDefault(false);
}

void UBRBotManagerComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwner() != nullptr && !GetOwner()->HasAuthority())
	{
		Deactivate();
	}
}

void UBRBotManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BackfillTimer);
	}
	Super::EndPlay(EndPlayReason);
}

void UBRBotManagerComponent::ConfigureRoster(int32 InMatchSeed, int32 InRosterSize, float InBackfillDelay_s)
{
	MatchSeed = InMatchSeed;
	RosterSize = FMath::Max(InRosterSize, 0);
	BackfillDelay_s = FMath::Max(InBackfillDelay_s, 0.f);
}

bool UBRBotManagerComponent::LoadBotTables(const UDataTable* TuningTable, const UDataTable* AmbitionsTable, const TArray<FName>& TierPerSlot)
{
	bTablesLoaded = false;
	TierRoster = TierPerSlot;
	TierScalarsByName.Reset();
	AmbitionDefs.Reset();

	FString Error;
	if (!UBRBotBrain::ReadAmbitionDefs(AmbitionsTable, AmbitionDefs, Error))
	{
		return false;
	}

	for (const FName& TierName : TierRoster)
	{
		if (TierScalarsByName.Contains(TierName))
		{
			continue;
		}

		FBRBotTierScalars Scalars;
		if (!UBRBotBrain::ReadTierScalars(TuningTable, TierName, Scalars, Error))
		{
			return false;
		}
		TierScalarsByName.Add(TierName, Scalars);
	}

	bTablesLoaded = true;
	return true;
}

int32 UBRBotManagerComponent::MakeSeedForSlot(int32 SlotIndex) const
{
	return static_cast<int32>(HashCombine(static_cast<uint32>(MatchSeed), static_cast<uint32>(SlotIndex)));
}

int32 UBRBotManagerComponent::CountHumans() const
{
	int32 Count = 0;
	if (const UWorld* World = GetWorld())
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			if (It->IsValid())
			{
				++Count;
			}
		}
	}
	return Count;
}

void UBRBotManagerComponent::FillToRoster()
{
	if (!bTablesLoaded)
	{
		return;
	}

	const int32 Humans = CountHumans();
	const int32 Wanted = FMath::Max(RosterSize - Humans - Bots.Num(), 0);

	for (int32 Index = 0; Index < Wanted; ++Index)
	{
		const int32 SlotIndex = Humans + Bots.Num();
		SpawnBotForSlot(SlotIndex);
	}
}

ABRBotController* UBRBotManagerComponent::SpawnBotForSlot(int32 SlotIndex)
{
	UWorld* World = GetWorld();
	AGameModeBase* GameMode = Cast<AGameModeBase>(GetOwner());
	if (World == nullptr || GameMode == nullptr || TierRoster.Num() == 0)
	{
		return nullptr;
	}

	const FName TierName = TierRoster[SlotIndex % TierRoster.Num()];
	const FBRBotTierScalars* Scalars = TierScalarsByName.Find(TierName);
	if (Scalars == nullptr)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags |= RF_Transient;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ABRBotController* Bot = World->SpawnActor<ABRBotController>(ABRBotController::StaticClass(), SpawnParams);
	if (Bot == nullptr)
	{
		return nullptr;
	}

	Bot->ConfigureBot(MakeSeedForSlot(SlotIndex), *Scalars, AmbitionDefs);

	GameMode->RestartPlayer(Bot);

	Bots.Add(Bot);

	return Bot;
}

void UBRBotManagerComponent::RemoveBot(ABRBotController* Bot)
{
	if (Bot == nullptr)
	{
		return;
	}

	Bots.Remove(Bot);

	if (APawn* BotPawn = Bot->GetPawn())
	{
		Bot->UnPossess();
		BotPawn->Destroy();
	}
	Bot->Destroy();
}

void UBRBotManagerComponent::NotifyCombatantLeft(AController* Leaver)
{
	if (ABRBotController* AsBot = Cast<ABRBotController>(Leaver))
	{
		Bots.Remove(AsBot);
	}

	UWorld* World = GetWorld();
	if (World == nullptr || BackfillDelay_s <= 0.f)
	{
		FillToRoster();
		return;
	}

	World->GetTimerManager().SetTimer(BackfillTimer, this, &UBRBotManagerComponent::HandleBackfillElapsed,
		BackfillDelay_s, false);
}

void UBRBotManagerComponent::HandleBackfillElapsed()
{
	FillToRoster();
}

void UBRBotManagerComponent::NotifyHumanJoined()
{
	const int32 Humans = CountHumans();
	while (Bots.Num() > 0 && (Humans + Bots.Num()) > RosterSize)
	{
		RemoveBot(Bots.Last());
	}
}
