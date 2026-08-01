// Breachpoint. Bot roster management. Authority-only; the GameMode still owns spawning.

#include "AI/BRBotManagerComponent.h"

#include "AI/BRBotController.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogBRBotManager, Log, All);

UBRBotManagerComponent::UBRBotManagerComponent()
{
	// Law 4. Everything here is an event or a timer.
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedByDefault(false); // Server-side bookkeeping; clients see replicated pawns, nothing else.
}

void UBRBotManagerComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwner() != nullptr && !GetOwner()->HasAuthority())
	{
		// A bot manager on a client would be a second simulation deciding who exists.
		UE_LOG(LogBRBotManager, Error, TEXT("BotManager exists on a non-authoritative owner — disabling."));
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

	// The seed is logged at configure time, not at use time: a soak report that cannot name
	// its seed is an anecdote (honesty law).
	UE_LOG(LogBRBotManager, Log, TEXT("Bot roster configured: seed=%d roster=%d backfill=%.2fs"),
		MatchSeed, RosterSize, BackfillDelay_s);
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
		UE_LOG(LogBRBotManager, Error, TEXT("DT_BotAmbitions unavailable: %s — no bots will be spawned."), *Error);
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
			UE_LOG(LogBRBotManager, Error, TEXT("DT_BotTuning row '%s' unavailable: %s — no bots will be spawned."),
				*TierName.ToString(), *Error);
			return false;
		}
		TierScalarsByName.Add(TierName, Scalars);
	}

	bTablesLoaded = true;
	return true;
}

int32 UBRBotManagerComponent::MakeSeedForSlot(int32 SlotIndex) const
{
	// Slot, not spawn order and not time: a backfilled bot's stream is a function of WHICH
	// slot it filled, so a replay does not depend on the moment someone disconnected.
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
		UE_LOG(LogBRBotManager, Error, TEXT("FillToRoster with no tables loaded — refusing to spawn unconfigured bots."));
		return;
	}

	const int32 Humans = CountHumans();
	const int32 Wanted = FMath::Max(RosterSize - Humans - Bots.Num(), 0);

	for (int32 Index = 0; Index < Wanted; ++Index)
	{
		const int32 SlotIndex = Humans + Bots.Num();
		SpawnBotForSlot(SlotIndex);
	}

	UE_LOG(LogBRBotManager, Log, TEXT("Roster filled: %d humans + %d bots (target %d)."), Humans, Bots.Num(), RosterSize);
}

ABRBotController* UBRBotManagerComponent::SpawnBotForSlot(int32 SlotIndex)
{
	UWorld* World = GetWorld();
	AGameModeBase* GameMode = Cast<AGameModeBase>(GetOwner());
	if (World == nullptr || GameMode == nullptr || TierRoster.Num() == 0)
	{
		UE_LOG(LogBRBotManager, Error, TEXT("Cannot spawn a bot: no world, no GameMode owner, or no tier roster."));
		return nullptr;
	}

	const FName TierName = TierRoster[SlotIndex % TierRoster.Num()];
	const FBRBotTierScalars* Scalars = TierScalarsByName.Find(TierName);
	if (Scalars == nullptr)
	{
		UE_LOG(LogBRBotManager, Error, TEXT("Tier '%s' was never resolved — not spawning slot %d."),
			*TierName.ToString(), SlotIndex);
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags |= RF_Transient;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ABRBotController* Bot = World->SpawnActor<ABRBotController>(ABRBotController::StaticClass(), SpawnParams);
	if (Bot == nullptr)
	{
		UE_LOG(LogBRBotManager, Error, TEXT("Bot controller spawn failed for slot %d."), SlotIndex);
		return nullptr;
	}

	// Seed and tuning BEFORE possession: a bot must never take a decision unseeded.
	Bot->ConfigureBot(MakeSeedForSlot(SlotIndex), *Scalars, AmbitionDefs);

	// The GameMode's OWN restart path — the same one a human login takes. Spawn transforms,
	// spawn-point scoring and team assignment stay entirely in the GameMode's domain; this
	// component never chooses a location or writes a team id.
	GameMode->RestartPlayer(Bot);

	Bots.Add(Bot);

	UE_LOG(LogBRBotManager, Log, TEXT("Bot slot %d spawned (tier '%s', seed %d)."),
		SlotIndex, *TierName.ToString(), MakeSeedForSlot(SlotIndex));

	return Bot;
}

void UBRBotManagerComponent::RemoveBot(ABRBotController* Bot)
{
	if (Bot == nullptr)
	{
		return;
	}

	Bots.Remove(Bot);

	// Destroy the pawn through the controller's own unpossess path so the ASC, the timers
	// and the StateTree all shut down the way they do on a normal death.
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

	// One timer, restarted: several departures inside the window collapse into one refill,
	// which is what stops a mass disconnect from spawning a stampede of bots.
	World->GetTimerManager().SetTimer(BackfillTimer, this, &UBRBotManagerComponent::HandleBackfillElapsed,
		BackfillDelay_s, /*bLoop=*/false);
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
		// LAST in, first out: the most recently backfilled bot yields, so a returning player
		// displaces the bot that replaced them rather than a bot that has been fighting all match.
		RemoveBot(Bots.Last());
	}
}
