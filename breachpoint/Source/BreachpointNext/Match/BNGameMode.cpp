#include "Match/BNGameMode.h"
#include "AI/BNBotController.h"
#include "Characters/BNCharacter.h"
#include "Core/BNGameplayTags.h"
#include "Match/BNGameState.h"
#include "Match/BNPlayerController.h"
#include "Match/BNPlayerState.h"
#include "AbilitySystem/BNAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/BNAttributeSet.h"
#include "AbilitySystem/Effects/BNGameplayEffects.h"
#include "BreachpointNext.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

ABNGameMode::ABNGameMode()
{
	// A constructor value is only a default a Blueprint child can out-serialise; InitGame below is
	// what makes it stick. Kept anyway so the bare C++ class is correct with no BP in play.
	GameStateClass = ABNGameState::StaticClass();

	// THE LINE THAT MAKES A LEVEL RESTART SURVIVABLE. HandleMatchHasEnded now calls RestartGame(),
	// which ServerTravels back to the same map so the round starts from the level's authored
	// state. Without seamless travel that would tear down and reconnect every client on a listen
	// server between rounds; with it, connections and PlayerStates are carried across and the
	// reload is invisible to anyone playing.
	bUseSeamlessTravel = true;
}

void ABNGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	// FORCED, after the Blueprint's serialisation and before PreInitializeComponents spawns the
	// GameState — the same precedent as the pawn path below. This is the line that closed the
	// TASK-R4-GAMESTATE-CLASS editor ticket: no dropdown on BP_BNGameMode can undo it, so the
	// match state, the clock and every score always land on the GameState the game reads.
	GameStateClass = ABNGameState::StaticClass();

	// The ini names the pawn; the dropdown is only what you see. This assignment is the one the
	// DefaultGame.ini comment promises — it was found commented out on 19 Aug and restored, with
	// the guard that keeps a bad path from replacing a working dropdown value with null.
	if (UClass* PawnClass = DefaultPawnClassPath.TryLoadClass<APawn>())
	{
		DefaultPawnClass = PawnClass;
	}
	else if (DefaultPawnClassPath.IsValid())
	{
		UE_LOG(LogBN, Warning, TEXT("BNGameMode: DefaultPawnClassPath '%s' did not resolve — the Blueprint's own Default Pawn Class stands."),
			*DefaultPawnClassPath.ToString());
	}
}

bool ABNGameMode::ReadyToStartMatch_Implementation()
{
	// The gate, nothing else: the parent polls this every warmup frame, so it must not spawn,
	// fill, or log. Humans only — a lobby of four bots is not a match anyone asked for.
	return GetNumPlayers() >= MinPlayers;
}

bool ABNGameMode::PlayerCanRestart_Implementation(APlayerController* Player)
{
	// Warmup restarts skip the PARENT's InProgress-only gate but keep the GRANDPARENT's real
	// checks (valid controller, CanRestartPlayer). Everything else defers to the parent, which is
	// what keeps corpses down during WaitingPostMatch.
	if (GetMatchState() == MatchState::WaitingToStart)
	{
		return AGameModeBase::PlayerCanRestart_Implementation(Player);
	}

	return Super::PlayerCanRestart_Implementation(Player);
}

void ABNGameMode::HandleMatchIsWaitingToStart()
{
	// Super may fire world BeginPlay (first boot); it must run before anything here spawns.
	Super::HandleMatchIsWaitingToStart();

	EnsureBotFill();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// A human who logged in during EnteringMap was refused a body (PlayerCanRestart said no
	// before warmup existed). Stand them up now — warmup bodies are the design.
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (PC && !PC->GetPawn() && PlayerCanRestart(PC))
		{
			RestartPlayer(PC);
		}
	}
}

void ABNGameMode::HandleMatchHasStarted()
{
	// The parent notifies the match started and stands up any pawnless human. State is already
	// InProgress when this runs — the machine set it before dispatching here.
	Super::HandleMatchHasStarted();

	ABNGameState* GS = GetGameState<ABNGameState>();
	UWorld* World = GetWorld();
	if (!GS || !World)
	{
		return;
	}

	// Every round is its own generation, so respawns armed in the last one are ignored in this
	// one. 1 is the first match; anything above is an in-place restart.
	++MatchGeneration;

	// An END STAMP, not a countdown: one replicated write that is already correct for a client
	// joining at any moment. The mode's timer is the server's own copy of the same deadline.
	GS->SetMatchEndServerTime(GS->GetServerWorldTimeSeconds() + TimeLimit);
	SetAllPlayersFrozen(false);

	World->GetTimerManager().SetTimer(MatchTimerHandle, this, &ABNGameMode::OnTimeLimitReached,
		FMath::Max(1.f, TimeLimit), /*bLoop=*/false);

	// A RESTART rebodies everyone: fresh attributes at fresh start points is what "new round"
	// means, and the last round's survivors must not carry 10 health into this one. The first
	// match rebodies only the PAWNLESS — its live players were born seconds ago in this very
	// warmup and destroying them to respawn them is a visible hitch for nothing, but a bot that
	// died to a hazard DURING warmup (death ignores the freeze, its respawn was refused outside
	// InProgress, and Super above stands up PlayerControllers only) would otherwise ghost through
	// the whole first match as a scoreboard row with no body — the critic's find. Thaw ran first,
	// so the respawn's State.* sweep finds no freeze handles left to stale. Known cost, accepted:
	// on a restart, a corpse Super just stood up is destroyed and stood up again — one redundant
	// spawn per corpse per round beats a special case in the one path that must never miss anyone.
	TArray<TObjectPtr<APlayerState>> Players = GS->PlayerArray;
	for (APlayerState* PS : Players)
	{
		AController* Controller = PS ? Cast<AController>(PS->GetOwner()) : nullptr;
		if (Controller && (MatchGeneration > 1 || !Controller->GetPawn()))
		{
			RespawnPlayer(TWeakObjectPtr<AController>(Controller));
		}
	}
}

void ABNGameMode::HandleMatchHasEnded()
{
	Super::HandleMatchHasEnded();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// The clock is done whichever condition fired — the score limit leaves it armed otherwise.
	World->GetTimerManager().ClearTimer(MatchTimerHandle);

	// And the STAMP is cleared, not left to rot: a stale end time reads exactly like "no clock"
	// through GetRemainingSeconds, but any future reader distinguishing the two would conflate
	// last round's expired deadline with a clock that was never set (critic note).
	if (ABNGameState* MutableGS = GetGameState<ABNGameState>())
	{
		MutableGS->SetMatchEndServerTime(0.0);
	}

	SetAllPlayersFrozen(true);

	// The winner was written BEFORE the state flipped (FinishMatch's ordering), so reading it
	// here is reading a decision, not a race.
	const ABNGameState* GS = GetGameState<ABNGameState>();
	const ABNPlayerState* Winner = GS ? GS->GetWinner() : nullptr;
	UE_LOG(LogBN, Log, TEXT("BNGameMode: match over. Winner: %s"),
		Winner ? *Winner->GetPlayerName() : TEXT("none (tie)"));

	World->GetTimerManager().SetTimer(PostMatchTimerHandle, this, &ABNGameMode::RestartMatch,
		FMath::Max(1.f, PostMatchDuration), /*bLoop=*/false);
}

void ABNGameMode::GenericPlayerInitialization(AController* C)
{
	Super::GenericPlayerInitialization(C);

	// Subscribed here rather than in the PlayerState's own BeginPlay: by this point the controller
	// HAS a PlayerState, and the mode is the one object that outlives every pawn and every death,
	// so nothing has to re-subscribe on respawn. HERE and not OnPostLogin because an AIController
	// never sees OnPostLogin — the engine calls this for humans, the bot fill calls it for bots.
	// It can run more than once per controller (seamless travel does), so the guard keeps one
	// death from printing two kill lines and arming two respawns.
	ABNPlayerState* PS = C ? C->GetPlayerState<ABNPlayerState>() : nullptr;
	if (PS && !PS->OnPlayerDeath.IsBoundToObject(this))
	{
		PS->OnPlayerDeath.AddUObject(this, &ABNGameMode::HandlePlayerDeath);
	}

	// A player initialized outside a running match joined a warmup or a post-match and must be
	// frozen like everyone already here — otherwise the one machine that joined late is the one
	// machine that can shoot. Thawing is HandleMatchHasStarted's, the moment the machine starts.
	if (PS && !IsMatchInProgress())
	{
		SetPlayerFrozen(PS, true);
	}
}

void ABNGameMode::OnPostLogin(AController* NewPlayer)
{
	Super::OnPostLogin(NewPlayer);

	// The arrival changed the seat math — a bot may have to yield. The START needs no call: the
	// parent's next Tick asks ReadyToStartMatch, which now counts this human.
	EnsureBotFill();
}

void ABNGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);

	// Next tick, not now: the leaving controller is still in the world's iterators inside Logout,
	// so a fill computed here counts a seat that is already empty. Humans only — a bot's own
	// despawn also lands in Logout, and refilling the seat a yield just cleared would fight the
	// yield forever.
	if (Cast<APlayerController>(Exiting) && GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ABNGameMode::EnsureBotFill);
	}
}

void ABNGameMode::HandlePlayerDeath(ABNPlayerState* Victim, ABNPlayerState* Killer, FName SourceName)
{
	if (!Victim)
	{
		return;
	}

	// CREDIT, before the line that prints it. A death always costs the victim; a kill is only a
	// kill when someone ELSE caused it. Killer == victim is their own grenade and a null killer is
	// world damage or a killer who left mid-flight — neither scores, both still cost a death.
	// Only a kill DURING the match scores. A grenade that lands after the buzzer still kills, still
	// ragdolls and still prints — but the scoreboard is final the moment the winner is announced,
	// or the post-match shows a top scorer who is not the winner, wrongly and on every machine.
	const bool bMatchLive = IsMatchInProgress();
	if (bMatchLive)
	{
		Victim->AddDeath();

		if (Killer && Killer != Victim)
		{
			Killer->AddKill();
		}
	}

	// THE KILL LINE — the pipeline's deliverable, and the founder's test. Three wordings for three
	// real cases, decided in the research doc so nobody rediscovers them: a null killer is world
	// damage or a killer who disconnected mid-flight; killer == victim is their own grenade.
	if (!Killer)
	{
		UE_LOG(LogBN, Log, TEXT("BNGameMode: %s died. (%d deaths)"),
			*Victim->GetPlayerName(), Victim->GetDeaths());
	}
	else if (Killer == Victim)
	{
		UE_LOG(LogBN, Log, TEXT("BNGameMode: %s eliminated themselves. (%d deaths)"),
			*Victim->GetPlayerName(), Victim->GetDeaths());
	}
	else
	{
		// The cause rides the line too — the one place a wrong killfeed glyph can be told apart
		// from a wrong CAPTURE without attaching a debugger.
		UE_LOG(LogBN, Log, TEXT("BNGameMode: %s eliminated %s with '%s'. (%s: %d kills)"),
			*Killer->GetPlayerName(), *Victim->GetPlayerName(), *SourceName.ToString(),
			*Killer->GetPlayerName(), Killer->GetKills());
	}

	// R7 — the same decided kill, onto the replicated ring the killfeed renders. Unconditional
	// like the log lines above it: a post-buzzer grenade kill still prints and still shows, it
	// just does not score — one decision, told the same way to every audience.
	if (ABNGameState* FeedGS = GetGameState<ABNGameState>())
	{
		FeedGS->PushKillfeedEntry(Victim, Killer, SourceName);
	}

	// 3.2 — the score limit, checked where the kill was credited. FinishMatch itself refuses
	// unless the match is InProgress, so a second elimination inside the same frame cannot end it
	// twice.
	if (bMatchLive && Killer && Killer != Victim && Killer->GetKills() >= ScoreLimit)
	{
		FinishMatch(Killer);
	}

	// This mode's answer to a death is a timed respawn. Another mode's could be a spectator
	// hand-off, or nothing at all — which is the point of hearing about the death instead of being
	// called by the ability that caused it.
	RequestRespawn(Cast<AController>(Victim->GetOwner()));
}

void ABNGameMode::RequestRespawn(AController* Controller)
{
	UWorld* World = GetWorld();
	if (!Controller || !World)
	{
		return;
	}

	// The timer is the GAME MODE's, never the dying pawn's: the corpse is destroyed before the
	// delay is up and a timer living on it would be destroyed with it.
	//
	// It also carries the GENERATION it was armed in. A player killed near the end of a round has a
	// respawn in flight when the restart rebodies everyone; without this the stale timer fires a
	// second or two into the NEW round and unpossesses and destroys a perfectly live pawn, which
	// every other client sees as a player blinking out and reappearing at a start point. Comparing
	// generations costs one int and needs no map of handles to keep in sync.
	// R7 — the owner's death screen counts down against this stamp, computed locally the way the
	// match clock is. Stamped from the same number the timer below is armed with, so the screen
	// and the actual rebody cannot drift. InProgress-gated (critic): the score-limit kill ends
	// the match BEFORE this runs, and without the gate every match-ending kill's victim counts
	// "respawning in 3…" over the winner announcement — a countdown to a rebody the post-match
	// refuses. The timer still arms; its refusal path is the second net.
	const float Delay = FMath::Max(0.1f, RespawnDelay);
	if (IsMatchInProgress())
	{
		if (ABNPlayerState* PS = Controller->GetPlayerState<ABNPlayerState>())
		{
			if (const ABNGameState* GS = GetGameState<ABNGameState>())
			{
				PS->SetRespawnAtServerTime(GS->GetServerWorldTimeSeconds() + Delay);
			}
		}
	}

	const int32 ArmedGeneration = MatchGeneration;
	FTimerHandle Handle;
	World->GetTimerManager().SetTimer(Handle,
		FTimerDelegate::CreateWeakLambda(this, [this, WeakController = TWeakObjectPtr<AController>(Controller), ArmedGeneration]()
		{
			if (ArmedGeneration == MatchGeneration)
			{
				RespawnPlayer(WeakController);
			}
		}),
		Delay, /*bLoop=*/false);
}

void ABNGameMode::RespawnPlayer(TWeakObjectPtr<AController> WeakController)
{
	AController* Controller = WeakController.Get();
	if (!Controller)
	{
		return;
	}

	// R7 — the stamp comes off FIRST, refusals included: a respawn refused by the post-match is
	// not coming this round (the restart rebodies instead), and a death screen counting down to
	// a moment that will not happen is the HUD lying.
	ABNPlayerState* StampPS = Controller->GetPlayerState<ABNPlayerState>();
	if (StampPS)
	{
		StampPS->SetRespawnAtServerTime(0.0);
	}

	// 3.5 — a respawn armed during play can land after the match ended. Nobody stands up outside
	// InProgress; the restart is what puts everyone back on their feet.
	if (!IsMatchInProgress())
	{
		return;
	}

	// The corpse goes first, and explicitly: ABNCharacter::EndPlay is what takes the crouch GE off
	// the persistent ASC (debt A2) and UBNEquipmentComponent::EndPlay is what revokes the weapon's
	// ability set. Both must have run before the new body grants its own.
	if (APawn* OldPawn = Controller->GetPawn())
	{
		Controller->UnPossess();
		OldPawn->Destroy();
	}

	// Then the clean slate, on the ASC — which is the PERSISTENT PlayerState's, not the pawn's.
	// Cancel ends the death ability, and that is what removes State.Dead; the sweep then takes any
	// State.* GE the old life left behind. Both happen BEFORE the new pawn exists, so it is born
	// onto an ASC with no tags and full attributes rather than being cleaned up afterwards.
	ABNPlayerState* PS = Controller->GetPlayerState<ABNPlayerState>();
	if (UAbilitySystemComponent* ASC = PS ? PS->GetAbilitySystemComponent() : nullptr)
	{
		ASC->CancelAbilities();

		FGameplayTagContainer StateTags;
		StateTags.AddTag(BNTags::State);
		ASC->RemoveActiveEffectsWithGrantedTags(StateTags);

		// Through the init GE, never hand-set: that GE is the one place the starting numbers live.
		PS->ApplyInitAttributes();

		// A TRIPWIRE on an invariant that had none. The new pawn's health component watches CHANGES
		// only — reading the value at registration would kill a body on the frame it spawned — so
		// health MUST already be positive by the time that component exists. If the init GE ever
		// fails to run, the player is alive at zero and can never die again, because no change will
		// follow. Silent and permanent, and it presents as "that one player is unkillable".
		const float RestoredHealth = ASC->GetNumericAttribute(UBNAttributeSet::GetHealthAttribute());
		if (RestoredHealth <= 0.f)
		{
			UE_LOG(LogBN, Error, TEXT("BNGameMode: respawning %s with Health %.1f — the init GE did not restore it. "
				"This pawn will be unkillable: its health component only reports CHANGES, and none will come."),
				*GetNameSafe(PS), RestoredHealth);
		}
	}

	// Cleared explicitly, server-side. UBNGA_Death set these on the controller, and the comment
	// there says ClientRestart resets them — which it does, but ClientRestart is a CLIENT RPC, so
	// the server's copy of a remote player's controller runs no such reset. A remote client's
	// movement arrives as ServerMove into the CMC rather than through AddMovementInput, so this is
	// most likely harmless; "most likely" is not verified, the symptom would be a respawned player
	// who cannot move, and the cure is two lines.
	Controller->SetIgnoreMoveInput(false);
	Controller->SetIgnoreLookInput(false);

	// The engine's own start point. There is deliberately no ABNPlayerStart: AGameModeBase::
	// FindPlayerStart already picks an APlayerStart, and a BN subclass would hold nothing until
	// teams and spawn scoring exist — which is a later roadmap's, not this fence's.
	RestartPlayer(Controller);
}

void ABNGameMode::EnsureBotFill()
{
	if (!HasAuthority())
	{
		return;
	}

	// TWO STATES, TWO POWERS (R9). Warmup both fills and yields. A LIVE match may only YIELD:
	// R5's own limitation was that a human joining after the start left the lobby one wide for
	// the whole match (the function returned before it could count), and the fix is not to also
	// start spawning mid-fight — a bot materialising beside you is worse than a seat over. Lyra
	// draws the same line: remove on join, backfill only between matches.
	const bool bWarmup = GetMatchState() == MatchState::WaitingToStart;
	const bool bLive = GetMatchState() == MatchState::InProgress;
	if (!bWarmup && !bLive)
	{
		return;
	}

	// A lobby with no one in it needs no bots. This guard is for the DEDICATED boot, where the
	// machine enters warmup with zero humans and would otherwise spawn a full bot lobby only to
	// yield a seat when the first human arrives. In PIE and on a listen host the order is the
	// reverse — the local player's login runs BEFORE StartPlay (critic-verified against the
	// engine's LoadMap/PIE flow), so the first fill here already counts one human and prints
	// R5's own line: "filled 3 bots to reach 4".
	if (GetNumPlayers() == 0)
	{
		return;
	}

	// GetNumPlayers counts humans only (PlayerControllers), which is exactly right — the bots
	// already here are counted from our own book, pruned of anything destroyed.
	SpawnedBots.RemoveAll([](const TObjectPtr<ABNBotController>& Bot) { return !IsValid(Bot); });

	const int32 BotsNeeded = TargetPlayers - GetNumPlayers() - SpawnedBots.Num();

	// OVER target: humans arrived after the fill (the machine enters warmup before the local
	// player logs in, so the first fill legitimately runs at zero humans). Newest bots yield
	// their seats — popping the tail keeps the named veterans and their scores.
	if (BotsNeeded < 0)
	{
		int32 Yielded = 0;
		for (int32 i = BotsNeeded; i < 0 && SpawnedBots.Num() > 0; ++i)
		{
			DespawnBot(SpawnedBots.Pop());
			++Yielded;
		}
		UE_LOG(LogBN, Log, TEXT("BNBots: %d bot(s) yielded seats to humans (%d humans, %d bots, target %d)"),
			Yielded, GetNumPlayers(), SpawnedBots.Num(), TargetPlayers);
		return;
	}

	if (BotsNeeded == 0)
	{
		return;
	}

	// Short-handed mid-match: left alone deliberately. See the two-states note above.
	if (bLive)
	{
		return;
	}

	int32 Filled = 0;
	for (int32 i = 0; i < BotsNeeded; ++i)
	{
		if (SpawnBot(NextBotNameIndex++))
		{
			++Filled;
		}
	}

	UE_LOG(LogBN, Log, TEXT("BNBots: filled %d bots to reach %d"), Filled, TargetPlayers);
}

ABNBotController* ABNGameMode::SpawnBot(int32 Index)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UClass* ControllerClass = BotControllerClass.LoadSynchronous();
	if (!ControllerClass)
	{
		UE_LOG(LogBN, Error, TEXT("BNBots: BotControllerClass '%s' did not resolve — no bot spawned."),
			*BotControllerClass.ToString());
		return nullptr;
	}

	// Lyra's flags: transient, so a bot controller can never be saved into a map.
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags |= RF_Transient;

	ABNBotController* NewBot = World->SpawnActor<ABNBotController>(ControllerClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	if (!NewBot)
	{
		UE_LOG(LogBN, Error, TEXT("BNBots: SpawnActor failed for %s — no bot spawned."), *GetNameSafe(ControllerClass));
		return nullptr;
	}

	// The name lands on the PlayerState bWantsPlayerState already made — the same name the kill
	// line, the leaders query and the winner announcement all read.
	if (APlayerState* PS = NewBot->GetPlayerState<APlayerState>())
	{
		PS->SetPlayerName(BotNames.IsValidIndex(Index) ? BotNames[Index] : FString::Printf(TEXT("Bot %d"), Index));
	}

	// The ONE initialization seam (G2): death subscription + warmup freeze, same as a human.
	GenericPlayerInitialization(NewBot);
	RestartPlayer(NewBot);

	SpawnedBots.Add(NewBot);
	return NewBot;
}

void ABNGameMode::DespawnBot(ABNBotController* Bot)
{
	if (!Bot)
	{
		return;
	}

	// Pawn first — OnUnPossess is what stops the tree and unhooks the brain's delegates — then
	// the controller, whose destruction is what retires its PlayerState from PlayerArray and
	// every roster built on it.
	if (APawn* BotPawn = Bot->GetPawn())
	{
		Bot->UnPossess();
		BotPawn->Destroy();
	}
	Bot->Destroy();
}

void ABNGameMode::OnTimeLimitReached()
{
	const ABNGameState* GS = GetGameState<ABNGameState>();
	if (!GS)
	{
		return;
	}

	// The sole leader wins. GetLeaders returns the whole tie set precisely so this can tell a
	// decided match from a tied one, and a tie is a null winner rather than an arbitrary pick.
	TArray<ABNPlayerState*> Leaders;
	GS->GetLeaders(Leaders);
	FinishMatch(Leaders.Num() == 1 ? Leaders[0] : nullptr);
}

void ABNGameMode::FinishMatch(ABNPlayerState* InWinner)
{
	ABNGameState* GS = GetGameState<ABNGameState>();
	if (!GS || !IsMatchInProgress())
	{
		return;
	}

	// Winner BEFORE the state, so the two land in the same bunch and no client ever sees the end
	// announced against a winner the server has not written yet. Then the machine's own verb —
	// EndMatch flips to WaitingPostMatch and dispatches HandleMatchHasEnded.
	GS->SetWinner(InWinner);
	EndMatch();
}

void ABNGameMode::RestartMatch()
{
	ABNGameState* GS = GetGameState<ABNGameState>();
	if (!GS)
	{
		return;
	}

	GS->SetWinner(nullptr);
	GS->ResetKillfeed();

	// Every score to zero. Without this the restarted match inherits the old one's kills and the
	// first elimination of the new round crosses the limit again immediately.
	for (APlayerState* PS : GS->PlayerArray)
	{
		if (ABNPlayerState* BNPS = Cast<ABNPlayerState>(PS))
		{
			BNPS->ResetScore();
		}
	}

	// FOUNDER, 25 Aug: "when match ends restart level to start a new match."
	//
	// This used to be SetMatchState(WaitingToStart) — a reset IN PLACE, chosen so the listen
	// server's connections survived the round. That concern is real and is NOT abandoned here:
	// bUseSeamlessTravel is enabled in the constructor, which is what lets the map actually
	// reload without dropping a single client. A bare RestartGame() with seamless travel off
	// would ServerTravel every connection through a full reconnect, which is exactly the cost
	// the old comment was avoiding.
	//
	// A real reload is what an in-place reset cannot give: the level's own actors go back to
	// their authored state — dropped weapons, spent pickups, projectiles in flight, corpses and
	// any navmesh dirtied during the round. The score wipe above still matters because seamless
	// travel CARRIES PlayerStates across, so kills would otherwise survive the reload and the
	// first elimination of the new round would cross the limit again immediately.
	//
	// BUT NOT IN PIE, AND THIS IS MEASURED, NOT ASSUMED. RestartGame() -> ServerTravel ENDS a
	// Play-In-Editor session rather than reloading it: tested 25 Aug, the match reached
	// WaitingPostMatch, the timer fired, and PIE simply stopped. Shipping the travel unguarded
	// would kill the founder's test session at the end of every single round.
	//
	// So the path is chosen by whether travel can actually work here, and it SAYS WHICH ONE IT
	// TOOK — a silent difference between what PIE exercises and what a packaged build does is
	// exactly the class of bug the honesty ladder exists to stop.
	const UWorld* MatchWorld = GetWorld();
	const bool bCanTravel = MatchWorld && !MatchWorld->IsPlayInEditor();
	if (bCanTravel)
	{
		UE_LOG(LogBN, Log, TEXT("BNGameMode: match over — reloading the level (seamless travel)."));
		RestartGame();
		return;
	}

	UE_LOG(LogBN, Log, TEXT("BNGameMode: match over — resetting IN PLACE (PIE: ServerTravel would end the session)."));
	SetMatchState(MatchState::WaitingToStart);
}

void ABNGameMode::SetPlayerFrozen(ABNPlayerState* InPlayerState, bool bFrozen)
{
	UAbilitySystemComponent* ASC = InPlayerState ? InPlayerState->GetAbilitySystemComponent() : nullptr;
	if (!ASC)
	{
		return;
	}

	// Built once: the map's key type is the weak pointer, and a raw pointer passed straight to
	// Find/Remove would hash as an address rather than as an object index and never match.
	const TWeakObjectPtr<ABNPlayerState> Key(InPlayerState);

	if (!bFrozen)
	{
		FActiveGameplayEffectHandle Handle;
		if (FrozenHandles.RemoveAndCopyValue(Key, Handle) && Handle.IsValid())
		{
			ASC->RemoveActiveGameplayEffect(Handle);
		}
		return;
	}

	if (FrozenHandles.Contains(Key))
	{
		return;
	}

	// The PERSISTENT PlayerState's ASC, so the freeze outlives the pawn a post-match death takes.
	const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UBNGE_State::StaticClass(), 1.f, ASC->MakeEffectContext());
	if (Spec.IsValid())
	{
		Spec.Data->DynamicGrantedTags.AddTag(BNTags::State_Match_Frozen);
		FrozenHandles.Add(Key, ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data));
	}

	// And STOP what is already running. Refusing activation does nothing to an ability that keeps
	// its own loop: a player holding an Auto trigger at the buzzer would go on killing through the
	// entire post-match, with the server confirming every shot. Death and hit-react survive this.
	if (UBNAbilitySystemComponent* BNASC = InPlayerState->GetBNAbilitySystemComponent())
	{
		BNASC->CancelAbilitiesBlockedByFreeze();
	}
}

void ABNGameMode::SetAllPlayersFrozen(bool bFrozen)
{
	const ABNGameState* GS = GetGameState<ABNGameState>();
	if (!GS)
	{
		return;
	}

	for (APlayerState* PS : GS->PlayerArray)
	{
		SetPlayerFrozen(Cast<ABNPlayerState>(PS), bFrozen);
	}

	// Thawing clears the map outright, which is also what collects the entries of players who left
	// while frozen — their weak keys never come back to be removed one at a time.
	if (!bFrozen)
	{
		FrozenHandles.Reset();
	}
}
