#include "Match/BNGameMode.h"
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
	// The match has to have somewhere to live that a client can read. BP_BNGameMode may still
	// out-serialise this dropdown the way it does the pawn class — that read-back is bn-editor's.
	GameStateClass = ABNGameState::StaticClass();

	/*DefaultPawnClass = ABNCharacter::StaticClass();
	PlayerControllerClass = ABNPlayerController::StaticClass();
	PlayerStateClass = ABNPlayerState::StaticClass();*/
}

void ABNGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	/*
	if (UClass* PawnClass = DefaultPawnClassPath.TryLoadClass<APawn>())
	{
		DefaultPawnClass = PawnClass;
	}*/
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
	// machine that can shoot. A first arrival that starts the match is thawed by BeginMatch.
	const ABNGameState* GS = GetGameState<ABNGameState>();
	if (PS && GS && GS->GetMatchState() != EBNMatchState::InProgress)
	{
		SetPlayerFrozen(PS, true);
	}
}

void ABNGameMode::OnPostLogin(AController* NewPlayer)
{
	Super::OnPostLogin(NewPlayer);

	// The arrival itself is what can satisfy MinPlayers, so the gate is asked here and nowhere else.
	TryStartMatch();
}

void ABNGameMode::HandlePlayerDeath(ABNPlayerState* Victim, ABNPlayerState* Killer)
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
	const ABNGameState* ScoringState = GetGameState<ABNGameState>();
	const bool bMatchLive = ScoringState && ScoringState->GetMatchState() == EBNMatchState::InProgress;
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
		UE_LOG(LogBN, Log, TEXT("BNGameMode: %s eliminated %s. (%s: %d kills)"),
			*Killer->GetPlayerName(), *Victim->GetPlayerName(), *Killer->GetPlayerName(), Killer->GetKills());
	}

	// 3.2 — the score limit, checked where the kill was credited. EndMatch itself refuses unless
	// the match is InProgress, so a second elimination inside the same frame cannot end it twice.
	if (bMatchLive && Killer && Killer != Victim && Killer->GetKills() >= ScoreLimit)
	{
		EndMatch(Killer);
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
		FMath::Max(0.1f, RespawnDelay), /*bLoop=*/false);
}

void ABNGameMode::RespawnPlayer(TWeakObjectPtr<AController> WeakController)
{
	AController* Controller = WeakController.Get();
	if (!Controller)
	{
		return;
	}

	// 3.5 — a respawn armed during play can land after the match ended. Nobody stands up outside
	// InProgress; the restart is what puts everyone back on their feet.
	const ABNGameState* GS = GetGameState<ABNGameState>();
	if (!GS || GS->GetMatchState() != EBNMatchState::InProgress)
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

void ABNGameMode::TryStartMatch()
{
	ABNGameState* GS = GetGameState<ABNGameState>();
	if (!GS || GS->GetMatchState() != EBNMatchState::WaitingToStart || GetNumPlayers() < MinPlayers)
	{
		return;
	}

	BeginMatch();
}

void ABNGameMode::BeginMatch()
{
	ABNGameState* GS = GetGameState<ABNGameState>();
	UWorld* World = GetWorld();
	if (!GS || !World)
	{
		return;
	}

	// An END STAMP, not a countdown: one replicated write that is already correct for a client
	// joining at any moment. The mode's timer is the server's own copy of the same deadline.
	// Every round is its own generation, so respawns armed in the last one are ignored in this one.
	++MatchGeneration;

	GS->SetMatchEndServerTime(GS->GetServerWorldTimeSeconds() + TimeLimit);
	GS->SetMatchState(EBNMatchState::InProgress);
	SetAllPlayersFrozen(false);

	World->GetTimerManager().SetTimer(MatchTimerHandle, this, &ABNGameMode::OnTimeLimitReached,
		FMath::Max(1.f, TimeLimit), /*bLoop=*/false);
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
	EndMatch(Leaders.Num() == 1 ? Leaders[0] : nullptr);
}

void ABNGameMode::EndMatch(ABNPlayerState* InWinner)
{
	ABNGameState* GS = GetGameState<ABNGameState>();
	UWorld* World = GetWorld();
	if (!GS || !World || GS->GetMatchState() != EBNMatchState::InProgress)
	{
		return;
	}

	// The clock is done whichever condition fired — the score limit leaves it armed otherwise.
	World->GetTimerManager().ClearTimer(MatchTimerHandle);

	// Winner BEFORE the state, so the two land in the same bunch and no client ever sees PostMatch
	// announced against a winner the server has not written yet.
	GS->SetWinner(InWinner);
	GS->SetMatchState(EBNMatchState::PostMatch);
	SetAllPlayersFrozen(true);

	UE_LOG(LogBN, Log, TEXT("BNGameMode: match over. Winner: %s"),
		InWinner ? *InWinner->GetPlayerName() : TEXT("none (tie)"));

	World->GetTimerManager().SetTimer(PostMatchTimerHandle, this, &ABNGameMode::RestartMatch,
		FMath::Max(1.f, PostMatchDuration), /*bLoop=*/false);
}

void ABNGameMode::RestartMatch()
{
	ABNGameState* GS = GetGameState<ABNGameState>();
	if (!GS)
	{
		return;
	}

	GS->SetWinner(nullptr);

	// Every score to zero. Without this the restarted match inherits the old one's kills and the
	// first elimination of the new round crosses the limit again immediately.
	for (APlayerState* PS : GS->PlayerArray)
	{
		if (ABNPlayerState* BNPS = Cast<ABNPlayerState>(PS))
		{
			BNPS->ResetScore();
		}
	}

	// The state goes back FIRST: the freeze comes off here and RespawnPlayer refuses outside
	// InProgress, so the order below is the only one that both thaws and rebodies everyone.
	BeginMatch();

	// Copied, because RestartPlayer spawns actors while this iterates.
	TArray<TObjectPtr<APlayerState>> Players = GS->PlayerArray;
	for (APlayerState* PS : Players)
	{
		RespawnPlayer(TWeakObjectPtr<AController>(PS ? Cast<AController>(PS->GetOwner()) : nullptr));
	}
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
