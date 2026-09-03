#include "Core/AIBBotManager.h"

#include "AIBotModule.h"
#include "Core/AIBBotController.h"
#include "Engine/World.h"
#include "Interfaces/AIBAmbitionProvider.h"
#include "Interfaces/AIBWorldQuery.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/Parse.h"

bool UAIBBotManager::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// Game worlds only — the shape the host's own cue registrar proved: editor preview
	// worlds get no bot machinery.
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UAIBBotManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// The command line wins over everything: a verifier passing -AIBSeed=N expects N,
	// whatever the host's game mode would have chosen. Resolution (and the log line)
	// still waits for the first GetMatchSeed, so the source is named in one place.
	int32 Parsed = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("AIBSeed="), Parsed))
	{
		MatchSeed = Parsed;
		bSeedFromCommandLine = true;
	}
}

void UAIBBotManager::RegisterProviders(UObject* AmbitionProvider, UObject* WorldQuery)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		// Law 3: providers are authority state. A client registering one is a wiring
		// bug worth a loud line, not a silent acceptance.
		UE_LOG(LogAIBot, Warning, TEXT("AIBot: RegisterProviders refused on a client world."));
		return;
	}

	AmbitionProviderObject = AmbitionProvider;
	WorldQueryObject = WorldQuery;
	UE_LOG(LogAIBot, Log, TEXT("AIBot: providers registered (ambitions: %s, world query: %s)."),
		*GetNameSafe(AmbitionProvider), *GetNameSafe(WorldQuery));
}

IAIBAmbitionProvider* UAIBBotManager::GetAmbitionProvider() const
{
	UObject* Object = AmbitionProviderObject.Get();
	return Object ? Cast<IAIBAmbitionProvider>(Object) : nullptr;
}

IAIBWorldQuery* UAIBBotManager::GetWorldQuery() const
{
	UObject* Object = WorldQueryObject.Get();
	return Object ? Cast<IAIBWorldQuery>(Object) : nullptr;
}

void UAIBBotManager::SetMatchSeed(int32 Seed)
{
	if (bSeedFromCommandLine)
	{
		UE_LOG(LogAIBot, Log, TEXT("AIBot: host match seed %d ignored — -AIBSeed=%d is on the command line."), Seed, MatchSeed);
		return;
	}
	if (bMatchSeedResolved)
	{
		// F7's shape: a seed set after a bot has drawn from another cannot be honoured
		// silently — the run would be half one seed, half the other.
		UE_LOG(LogAIBot, Warning, TEXT("AIBot: host match seed %d ignored — a bot already drew from seed %d."), Seed, MatchSeed);
		return;
	}
	MatchSeed = Seed;
	bSeedFromHost = true;
}

int32 UAIBBotManager::GetMatchSeed()
{
	if (!bMatchSeedResolved)
	{
		if (!bSeedFromCommandLine && !bSeedFromHost)
		{
			const int64 Ticks = FDateTime::UtcNow().GetTicks();
			MatchSeed = static_cast<int32>(HashCombine(static_cast<uint32>(Ticks), static_cast<uint32>(Ticks >> 32)));
		}
		bMatchSeedResolved = true;
		UE_LOG(LogAIBot, Log, TEXT("AIBot: match seed %d (source=%s)."), MatchSeed,
			bSeedFromCommandLine ? TEXT("cmdline") : bSeedFromHost ? TEXT("host") : TEXT("clock"));
	}
	return MatchSeed;
}

int32 UAIBBotManager::AssignBotIndex(const AAIBBotController& Bot)
{
	for (int32 Index = 0; Index < BotSlots.Num(); ++Index)
	{
		if (BotSlots[Index].Get() == &Bot)
		{
			return Index;
		}
	}
	// Never reused: a slot outlives its controller so a late joiner cannot inherit a
	// dead bot's draw sequence.
	return BotSlots.Add(&Bot);
}
