// Breachpoint. The server-only match spine: phase machine, kill attribution, scored respawn.

#include "Match/BRGameMode.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Core/BRCore.h"
#include "Core/BRGameplayTags.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerState.h"
#include "GenericTeamAgentInterface.h"
#include "Character/BRCharacter.h"
#include "Match/BRGameState.h"
#include "Match/BRPlayerController.h"
#include "Match/BRPlayerState.h"
#include "TimerManager.h"

namespace
{
	/** Timers refuse a rate of 0 (it clears them), so every duration is floored here. */
	constexpr float MinPhaseSeconds = 0.1f;

	/** A spawn point used inside this window is heavily penalised, to avoid spawn stacking. */
	constexpr float SpawnReuseWindowSeconds = 5.f;
}

ABRGameMode::ABRGameMode()
{
	// Law 4: the match frame owns four timers and two gameplay events. It does not tick.
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// ---------------------------------------------------------------------
	// THE FOUR CLASS DEFAULTS. Only GameStateClass was set here before, and the omission was not
	// harmless: with PlayerControllerClass unset, AGameModeBase's own default (APlayerController)
	// applied, so nothing in C++ ever asked for ABRPlayerController. GM_BR filled the gap on its
	// Blueprint defaults and — verified 1 Aug 2026 by reading the asset's import table — filled it
	// with BP_ShooterPlayerController, the TEMPLATE's controller. The shipped game therefore ran a
	// BRCharacter pawn under a ShooterPlayerController: mapping contexts arrived (which is why it
	// moved at all), while ABRPlayerController's relay, its key census and its CommonUI viewport
	// check were never reached, and Cast<ABRPlayerController> in the pawn's
	// SetupPlayerInputComponent failed on every possession.
	//
	// WHAT THIS DOES AND DOES NOT FIX. A Blueprint's serialised value beats a C++ CDO default
	// (docs/PHASE2-RELAYER.md step 1), so GM_BR's saved PlayerControllerClass still wins while it
	// holds one. This makes the C++ default correct and makes CLEARING the Blueprint override the
	// fix, rather than requiring the founder to know which asset to point it at.
	// ---------------------------------------------------------------------
	GameStateClass       = ABRGameState::StaticClass();
	PlayerStateClass     = ABRPlayerState::StaticClass();
	PlayerControllerClass = ABRPlayerController::StaticClass();
	DefaultPawnClass     = ABRCharacter::StaticClass();

	TeamDamageDealt.Init(0.f, BRMatch::NumTeams);
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void ABRGameMode::InitGameState()
{
	Super::InitGameState();

	// Publish the rules before anybody can score against them. ServerSetScoreLimit
	// refuses to move once scoring opens, so this is the only window that counts.
	if (ABRGameState* BRGS = GetBRGameState())
	{
		BRGS->ServerSetScoreLimit(ScoreLimit);
	}
}

void ABRGameMode::OnPostLogin(AController* NewPlayer)
{
	Super::OnPostLogin(NewPlayer);

	// Human players bind here. Bots never reach OnPostLogin — their manager calls
	// RegisterCombatant directly, which is why that entry point is public and idempotent.
	RegisterCombatant(NewPlayer);
}

void ABRGameMode::Logout(AController* Exiting)
{
	UnregisterCombatant(Exiting);

	Super::Logout(Exiting);
}

void ABRGameMode::HandleMatchHasStarted()
{
	// Super spawns everyone who was waiting; only then does the phase machine open.
	Super::HandleMatchHasStarted();

	EnterPhase(EBRMatchPhase::WarmUp);
}

bool ABRGameMode::PlayerCanRestart_Implementation(APlayerController* Player)
{
	// Once a winner exists the arena is frozen. AGameMode::PlayerCanRestart already
	// refuses outside MatchInProgress; this is the phase-level statement of the same rule,
	// because PostMatch is entered before EndMatch() settles.
	if (GetMatchPhase() == EBRMatchPhase::PostMatch)
	{
		return false;
	}

	return Super::PlayerCanRestart_Implementation(Player);
}

// ---------------------------------------------------------------------------
// Phase machine — timers and events only
// ---------------------------------------------------------------------------

EBRMatchPhase ABRGameMode::GetMatchPhase() const
{
	const ABRGameState* BRGS = GetBRGameState();
	return BRGS ? BRGS->GetMatchPhase() : EBRMatchPhase::None;
}

void ABRGameMode::EnterPhase(EBRMatchPhase NewPhase)
{
	if (!HasAuthority())
	{
		return;
	}

	ABRGameState* BRGS = GetBRGameState();
	if (!BRGS || BRGS->GetMatchPhase() == NewPhase)
	{
		return;
	}

	FTimerManager& TimerManager = GetWorldTimerManager();

	// Exactly one phase timer may be armed at a time. Clearing all four here is what makes
	// "death during a phase transition" harmless: a stale timer can never fire into the
	// wrong phase and, e.g., send a decided match back into sudden death.
	TimerManager.ClearTimer(WarmupTimerHandle);
	TimerManager.ClearTimer(MatchClockTimerHandle);
	TimerManager.ClearTimer(SuddenDeathTimerHandle);
	TimerManager.ClearTimer(PostMatchTimerHandle);

	BRGS->ServerSetMatchPhase(NewPhase);

	const float Now = GetServerTime();

	switch (NewPhase)
	{
	case EBRMatchPhase::WarmUp:
	{
		const float Duration = FMath::Max(MinPhaseSeconds, WarmupSeconds);
		BRGS->ServerSetMatchEndServerTime(Now + Duration);
		TimerManager.SetTimer(WarmupTimerHandle, this, &ABRGameMode::HandleWarmupElapsed, Duration, false);
		break;
	}
	case EBRMatchPhase::Live:
	{
		const float Duration = FMath::Max(MinPhaseSeconds, MatchDurationSeconds);
		BRGS->ServerSetMatchEndServerTime(Now + Duration);
		TimerManager.SetTimer(MatchClockTimerHandle, this, &ABRGameMode::HandleMatchClockExpired, Duration, false);
		break;
	}
	case EBRMatchPhase::SuddenDeath:
	{
		const float Duration = FMath::Max(MinPhaseSeconds, SuddenDeathSeconds);
		BRGS->ServerSetMatchEndServerTime(Now + Duration);
		TimerManager.SetTimer(SuddenDeathTimerHandle, this, &ABRGameMode::HandleSuddenDeathExpired, Duration, false);
		break;
	}
	case EBRMatchPhase::PostMatch:
	{
		const float Duration = FMath::Max(MinPhaseSeconds, PostMatchSeconds);
		BRGS->ServerSetMatchEndServerTime(Now + Duration);
		TimerManager.SetTimer(PostMatchTimerHandle, this, &ABRGameMode::HandlePostMatchElapsed, Duration, false);

		// No respawns after the whistle: drop every pending timer rather than let one
		// fire into a frozen arena.
		for (TPair<TWeakObjectPtr<AController>, FTimerHandle>& Pair : RespawnTimers)
		{
			TimerManager.ClearTimer(Pair.Value);
		}
		RespawnTimers.Reset();
		break;
	}
	case EBRMatchPhase::None:
	default:
		BRGS->ServerSetMatchEndServerTime(0.f);
		break;
	}
}

void ABRGameMode::HandleWarmupElapsed()
{
	// Warmup -> Live. Scores earned before this instant do not exist (see HandleDeathEvent),
	// so nothing needs resetting here.
	EnterPhase(EBRMatchPhase::Live);
}

void ABRGameMode::HandleMatchClockExpired()
{
	if (bMatchDecided)
	{
		return;
	}

	const ABRGameState* BRGS = GetBRGameState();
	if (!BRGS)
	{
		return;
	}

	// A clean lead ends it; a tie goes to overtime. Sudden death is entered ONLY here.
	int32 BestScore = TNumericLimits<int32>::Min();
	uint8 BestTeam = BRMatch::InvalidTeamId;
	bool bTied = false;

	for (uint8 TeamId = 0; TeamId < BRMatch::NumTeams; ++TeamId)
	{
		const int32 Score = BRGS->GetTeamScore(TeamId);
		if (Score > BestScore)
		{
			BestScore = Score;
			BestTeam = TeamId;
			bTied = false;
		}
		else if (Score == BestScore)
		{
			bTied = true;
		}
	}

	if (bTied)
	{
		EnterPhase(EBRMatchPhase::SuddenDeath);
		return;
	}

	EnterPostMatch(BestTeam);
}

void ABRGameMode::HandleSuddenDeathExpired()
{
	if (bMatchDecided)
	{
		return;
	}

	const ABRGameState* BRGS = GetBRGameState();
	if (!BRGS)
	{
		return;
	}

	// The 60 s cap fired with the score still level: the tiebreak ladder is
	// score -> damage dealt to enemies -> draw. Damage is the server's own ledger, never
	// a client-reported number, which is what makes it safe to decide a match on.
	uint8 BestTeam = BRMatch::InvalidTeamId;
	int32 BestScore = TNumericLimits<int32>::Min();
	float BestDamage = -1.f;
	bool bAmbiguous = false;

	for (uint8 TeamId = 0; TeamId < BRMatch::NumTeams; ++TeamId)
	{
		const int32 Score = BRGS->GetTeamScore(TeamId);
		const float Damage = TeamDamageDealt.IsValidIndex(TeamId) ? TeamDamageDealt[TeamId] : 0.f;

		if (Score > BestScore || (Score == BestScore && Damage > BestDamage))
		{
			BestScore = Score;
			BestDamage = Damage;
			BestTeam = TeamId;
			bAmbiguous = false;
		}
		else if (Score == BestScore && FMath::IsNearlyEqual(Damage, BestDamage))
		{
			bAmbiguous = true;
		}
	}

	UE_LOG(LogBRNet, Log, TEXT("[Match] Sudden death cap reached. Tiebreak -> team %d (score %d, damage %.0f, ambiguous %d)"),
		BestTeam, BestScore, BestDamage, bAmbiguous ? 1 : 0);

	EnterPostMatch(bAmbiguous ? BRMatch::InvalidTeamId : BestTeam);
}

void ABRGameMode::HandlePostMatchElapsed()
{
	RequestMatchTeardown();
}

void ABRGameMode::RequestMatchTeardown()
{
	// Deliberately inert. Travel, session teardown and re-hosting belong to Online/
	// behind IBRServerLifecycle; the match frame's job ends at "a winner exists".
	UE_LOG(LogBRNet, Log, TEXT("[Match] Post-match window elapsed; teardown is the session layer's call."));
}

void ABRGameMode::ScheduleWinCheck()
{
	if (!HasAuthority() || bWinCheckPending || bMatchDecided)
	{
		return;
	}

	bWinCheckPending = true;

	// THE double-KO line. Deferring to the next tick means every death resolved in this
	// frame is scored before anything can end the match, so a trade at 24-24 credits both
	// players and the tiebreak (not the ordering of two Event.Death callbacks) decides it.
	GetWorldTimerManager().SetTimerForNextTick(this, &ABRGameMode::EvaluateWinConditions);
}

void ABRGameMode::EvaluateWinConditions()
{
	bWinCheckPending = false;

	if (!HasAuthority() || bMatchDecided)
	{
		return;
	}

	const ABRGameState* BRGS = GetBRGameState();
	if (!BRGS || !BRGS->IsScoringOpen())
	{
		return;
	}

	const bool bSuddenDeath = BRGS->GetMatchPhase() == EBRMatchPhase::SuddenDeath;

	uint8 BestTeam = BRMatch::InvalidTeamId;
	int32 BestScore = TNumericLimits<int32>::Min();
	int32 SecondScore = TNumericLimits<int32>::Min();
	float BestDamage = -1.f;

	for (uint8 TeamId = 0; TeamId < BRMatch::NumTeams; ++TeamId)
	{
		const int32 Score = BRGS->GetTeamScore(TeamId);
		const float Damage = TeamDamageDealt.IsValidIndex(TeamId) ? TeamDamageDealt[TeamId] : 0.f;

		if (Score > BestScore || (Score == BestScore && Damage > BestDamage))
		{
			SecondScore = FMath::Max(SecondScore, BestScore);
			BestScore = Score;
			BestDamage = Damage;
			BestTeam = TeamId;
		}
		else
		{
			SecondScore = FMath::Max(SecondScore, Score);
		}
	}

	if (bSuddenDeath)
	{
		// Overtime ends the moment the tie breaks. An equal-score trade keeps it running
		// until the 60 s cap.
		if (BestScore > SecondScore)
		{
			EnterPostMatch(BestTeam);
		}
		return;
	}

	if (BestScore >= ScoreLimit)
	{
		// Both teams crossing the limit in the same frame is the double-KO-at-match-point
		// case: BestTeam already carries the score-then-damage ordering, so it resolves
		// here rather than by whichever death event happened to be delivered first.
		EnterPostMatch(BestTeam);
	}
}

void ABRGameMode::EnterPostMatch(uint8 WinningTeamId)
{
	if (!HasAuthority() || bMatchDecided)
	{
		return;
	}

	bMatchDecided = true;

	if (ABRGameState* BRGS = GetBRGameState())
	{
		BRGS->ServerSetWinningTeamId(WinningTeamId);
	}

	EnterPhase(EBRMatchPhase::PostMatch);

	PendingRespawnPlayers.Reset();
	EarliestRespawnServerTime.Reset();

	// Settle the engine's own match state too, so AGameMode's restart paths shut with ours.
	if (IsMatchInProgress())
	{
		EndMatch();
	}

	UE_LOG(LogBRNet, Log, TEXT("[Match] Decided. Winning team %d."), WinningTeamId);
}

// ---------------------------------------------------------------------------
// Damage ledger + kill attribution
// ---------------------------------------------------------------------------

void ABRGameMode::NotifyDamageDealt(APlayerState* Attacker, APlayerState* Victim, float Amount, FGameplayTag DamageTag)
{
	if (!HasAuthority() || !Victim)
	{
		return;
	}

	// Server-side sanity, not client validation — nothing here crosses the wire — but a
	// bad number would still poison both attribution and the tiebreak, so it stops here.
	if (!(Amount > 0.f) || Amount > MaxPlausibleSingleDamage || !FMath::IsFinite(Amount))
	{
		UE_LOG(LogBRNet, Warning, TEXT("[Match] Implausible damage report %.2f dropped (victim %s)."),
			Amount, *GetNameSafe(Victim));
		return;
	}

	PruneDamageLedger(Victim);

	TArray<FBRDamageRecord>& Records = DamageLedger.FindOrAdd(Victim);
	FBRDamageRecord& Record = Records.AddDefaulted_GetRef();
	Record.Attacker = Attacker;
	Record.Amount = Amount;
	Record.ServerTime = GetServerTime();
	Record.DamageTag = DamageTag;

	// Tiebreak credit: enemy damage only, scoring phases only. Farming a teammate or
	// the warmup dummy may not buy an overtime win.
	const ABRGameState* BRGS = GetBRGameState();
	if (!BRGS || !BRGS->IsScoringOpen() || !Attacker || Attacker == Victim)
	{
		return;
	}

	const uint8 AttackerTeam = ResolveTeamId(Attacker);
	const uint8 VictimTeam = ResolveTeamId(Victim);
	if (AttackerTeam != VictimTeam && TeamDamageDealt.IsValidIndex(AttackerTeam))
	{
		TeamDamageDealt[AttackerTeam] += Amount;
	}
}

void ABRGameMode::PruneDamageLedger(APlayerState* Victim)
{
	TArray<FBRDamageRecord>* Records = DamageLedger.Find(Victim);
	if (!Records)
	{
		return;
	}

	const float Cutoff = GetServerTime() - FMath::Max(0.f, KillCreditWindowSeconds);
	Records->RemoveAll([Cutoff](const FBRDamageRecord& Record)
	{
		return Record.ServerTime < Cutoff || !Record.Attacker.IsValid();
	});
}

APlayerState* ABRGameMode::ResolveKiller(APlayerState* Victim, APlayerState* EventInstigator) const
{
	if (!Victim)
	{
		return nullptr;
	}

	const float Cutoff = GetServerTime() - FMath::Max(0.f, KillCreditWindowSeconds);

	// LAST hit inside the window wins, not most damage: the kill-steal rule, decided.
	// Note what is NOT here: any check that the killer is still alive. That absence is
	// the double-KO rule — a corpse still gets its kill.
	if (const TArray<FBRDamageRecord>* Records = DamageLedger.Find(Victim))
	{
		for (int32 Index = Records->Num() - 1; Index >= 0; --Index)
		{
			const FBRDamageRecord& Record = (*Records)[Index];
			if (Record.ServerTime < Cutoff)
			{
				break;
			}

			APlayerState* Attacker = Record.Attacker.Get();
			if (Attacker && Attacker != Victim)
			{
				return Attacker;
			}
		}
	}

	// Fallback for damage that never reached the ledger (a kill volume, a scripted
	// execution). Still refuses to credit the victim to themselves.
	if (EventInstigator && EventInstigator != Victim)
	{
		return EventInstigator;
	}

	// Nothing in the window and no instigator: fall damage / world kill / suicide.
	return nullptr;
}

void ABRGameMode::CollectAssists(APlayerState* Victim, APlayerState* Killer, TArray<APlayerState*>& OutAssists) const
{
	OutAssists.Reset();

	const TArray<FBRDamageRecord>* Records = DamageLedger.Find(Victim);
	if (!Records)
	{
		return;
	}

	const float Cutoff = GetServerTime() - FMath::Max(0.f, KillCreditWindowSeconds);

	for (const FBRDamageRecord& Record : *Records)
	{
		if (Record.ServerTime < Cutoff)
		{
			continue;
		}

		APlayerState* Attacker = Record.Attacker.Get();
		if (!Attacker || Attacker == Victim || Attacker == Killer)
		{
			continue;
		}

		if (ResolveTeamId(Attacker) == ResolveTeamId(Victim))
		{
			continue; // A teammate softening you up is not an enemy assist.
		}

		OutAssists.AddUnique(Attacker);
	}
}

void ABRGameMode::HandleDeathEvent(APlayerState* Victim, APlayerState* EventInstigator, FGameplayTag CauseTag)
{
	if (!HasAuthority() || !Victim)
	{
		return;
	}

	ABRGameState* BRGS = GetBRGameState();
	if (!BRGS)
	{
		return;
	}

	// One death per life. BP02 promises exactly one Event.Death per GE_Death, but a
	// duplicate (a second lethal execution landing in the same frame, or a re-sent event)
	// must not double-score. The set is cleared when the victim actually respawns.
	if (PendingRespawnPlayers.Contains(Victim))
	{
		UE_LOG(LogBRNet, Verbose, TEXT("[Match] Duplicate death for %s ignored."), *GetNameSafe(Victim));
		return;
	}
	PendingRespawnPlayers.Add(Victim);

	APlayerState* Killer = ResolveKiller(Victim, EventInstigator);

	TArray<APlayerState*> Assists;
	CollectAssists(Victim, Killer, Assists);

	// The victim's ledger dies with them; the next life starts with a clean sheet, so a
	// hit taken before dying can never credit a kill after respawning.
	DamageLedger.Remove(Victim);

	const uint8 VictimTeam = ResolveTeamId(Victim);
	const uint8 KillerTeam = Killer ? ResolveTeamId(Killer) : BRMatch::InvalidTeamId;
	const bool bSelfInflicted = (Killer == nullptr);
	const bool bFriendlyFire = (!bSelfInflicted && KillerTeam != BRMatch::InvalidTeamId && KillerTeam == VictimTeam);

	// Phase is read ONCE, here, and every branch below uses that read. A phase change can
	// only happen inside our own timers/handlers, so this function never straddles one.
	const bool bScoringOpen = BRGS->IsScoringOpen() && !bMatchDecided;

	if (bScoringOpen)
	{
		if (bSelfInflicted)
		{
			// DECIDED: no instigator (fall damage, world kill, own grenade) = -1 to the
			// victim's own team, clamped at 0, and a -1 Event.Kill to the victim.
			BRGS->ServerAddTeamScore(VictimTeam, -1);
			SendKillEvent(Victim, Victim, -1.f, CauseTag);
		}
		else if (bFriendlyFire)
		{
			// DECIDED: a team kill pays the killer's team, and the killer gets no credit.
			if (bFriendlyFirePenalizesKillersTeam)
			{
				BRGS->ServerAddTeamScore(KillerTeam, -1);
			}
			SendKillEvent(Killer, Victim, -1.f, CauseTag);
		}
		else
		{
			BRGS->ServerAddTeamScore(KillerTeam, 1);
			SendKillEvent(Killer, Victim, 1.f, CauseTag);
		}

		FBRKillFeedEntry Entry;
		Entry.Killer = Killer;
		Entry.Victim = Victim;
		Entry.KillerTeamId = KillerTeam;
		Entry.VictimTeamId = VictimTeam;
		Entry.CauseTag = CauseTag;
		Entry.bSelfInflicted = bSelfInflicted ? 1 : 0;
		Entry.bFriendlyFire = bFriendlyFire ? 1 : 0;
		BRGS->ServerPushKillFeedEntry(Entry);

		OnPlayerKilled.Broadcast(Killer, Victim, Assists);

		// Deferred by one tick — see ScheduleWinCheck.
		ScheduleWinCheck();
	}
	else
	{
		// Warmup and post-match deaths are real (you die, you respawn) but weightless:
		// no score, no killfeed row, no Event.Kill. Decided, not discovered.
		UE_LOG(LogBRNet, Verbose, TEXT("[Match] Unscored death for %s in phase %d."),
			*GetNameSafe(Victim), static_cast<int32>(BRGS->GetMatchPhase()));
	}

	StartRespawnTimer(Victim->GetOwningController());
}

void ABRGameMode::SendKillEvent(APlayerState* Recipient, APlayerState* Victim, float Magnitude, FGameplayTag CauseTag) const
{
	if (!HasAuthority() || !Recipient)
	{
		return;
	}

	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Recipient);
	if (!ASC)
	{
		return;
	}

	// Awarding by gameplay event keeps the K/D bookkeeping on the PlayerState that owns
	// it (gas-purity: the match frame does not reach into another packet's state), and it
	// gives abilities a legal hook to react to a kill.
	FGameplayEventData Payload;
	Payload.EventTag = BRGameplayTags::Event_Kill;
	Payload.Instigator = Recipient;
	Payload.Target = Victim;
	Payload.EventMagnitude = Magnitude;
	Payload.InstigatorTags.AddTag(CauseTag);

	ASC->HandleGameplayEvent(BRGameplayTags::Event_Kill, &Payload);
}

// ---------------------------------------------------------------------------
// Combatant registration — the Event.Death subscription
// ---------------------------------------------------------------------------

void ABRGameMode::RegisterCombatant(AController* Combatant)
{
	if (!HasAuthority() || !Combatant || !Combatant->PlayerState)
	{
		return;
	}

	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Combatant->PlayerState);
	if (!ASC || DeathListenerHandles.Contains(ASC))
	{
		return;
	}

	// The ASC lives on the PlayerState (ARCHITECTURE §3.6), so the subscription survives
	// every pawn death and respawn — bind once per player, not once per life.
	const FDelegateHandle Handle = ASC->GenericGameplayEventCallbacks
		.FindOrAdd(BRGameplayTags::Event_Death)
		.AddUObject(this, &ABRGameMode::OnDeathGameplayEvent);

	DeathListenerHandles.Add(ASC, Handle);

	UE_LOG(LogBRNet, Log, TEXT("[Match] Registered combatant %s for Event.Death."), *GetNameSafe(Combatant->PlayerState));
}

void ABRGameMode::UnregisterCombatant(AController* Combatant)
{
	if (!Combatant)
	{
		return;
	}

	APlayerState* PS = Combatant->PlayerState;
	if (PS)
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PS))
		{
			if (const FDelegateHandle* Handle = DeathListenerHandles.Find(ASC))
			{
				if (FGameplayEventMulticastDelegate* Delegate = ASC->GenericGameplayEventCallbacks.Find(BRGameplayTags::Event_Death))
				{
					Delegate->Remove(*Handle);
				}
				DeathListenerHandles.Remove(ASC);
			}
		}

		// Law 7: a leaver's state must not linger. Their ledger entries would otherwise
		// keep a stale weak pointer alive in the tiebreak and the killfeed.
		DamageLedger.Remove(PS);
		PendingRespawnPlayers.Remove(PS);
	}

	if (FTimerHandle* Handle = RespawnTimers.Find(Combatant))
	{
		GetWorldTimerManager().ClearTimer(*Handle);
		RespawnTimers.Remove(Combatant);
	}
	EarliestRespawnServerTime.Remove(Combatant);

	// Weak keys go stale as actors are destroyed; sweep them so the maps stay bounded
	// across a long match with reconnects.
	for (auto It = DeathListenerHandles.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
	for (auto It = DamageLedger.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
	for (auto It = SpawnPointLastUsed.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
}

void ABRGameMode::OnDeathGameplayEvent(const FGameplayEventData* Payload)
{
	if (!HasAuthority() || !Payload)
	{
		return;
	}

	APlayerState* Victim = ResolvePlayerState(Payload->Target);
	if (!Victim)
	{
		// Death events from non-player actors (destructibles, turrets) are not match state.
		return;
	}

	// The cause tag is cosmetic (a killfeed icon), so taking it straight off the payload
	// is safe: it selects an image, never a score.
	FGameplayTag CauseTag;
	for (const FGameplayTag& Tag : Payload->InstigatorTags)
	{
		if (Tag.MatchesTag(BRGameplayTags::Damage_Kinetic) || Tag.MatchesTag(BRGameplayTags::Damage_Explosive) || Tag.MatchesTag(BRGameplayTags::Damage_Melee))
		{
			CauseTag = Tag;
			break;
		}
	}

	HandleDeathEvent(Victim, ResolvePlayerState(Payload->Instigator), CauseTag);
}

// ---------------------------------------------------------------------------
// Respawn
// ---------------------------------------------------------------------------

void ABRGameMode::StartRespawnTimer(AController* Victim)
{
	if (!HasAuthority() || !Victim)
	{
		return;
	}

	if (GetMatchPhase() == EBRMatchPhase::PostMatch || bMatchDecided)
	{
		return;
	}

	const float Delay = FMath::Max(0.f, RespawnDelaySeconds);

	// The single source of truth for "not yet": both the timer and any player-initiated
	// request check this stamp, so a request path can never beat the timer.
	EarliestRespawnServerTime.Add(Victim, GetServerTime() + Delay);

	FTimerHandle& Handle = RespawnTimers.FindOrAdd(Victim);
	GetWorldTimerManager().ClearTimer(Handle);
	GetWorldTimerManager().SetTimer(
		Handle,
		FTimerDelegate::CreateUObject(this, &ABRGameMode::HandleRespawnTimerElapsed, TWeakObjectPtr<AController>(Victim)),
		FMath::Max(MinPhaseSeconds, Delay),
		false);
}

void ABRGameMode::HandleRespawnTimerElapsed(TWeakObjectPtr<AController> WeakVictim)
{
	AController* Victim = WeakVictim.Get();
	if (!Victim)
	{
		return;
	}

	RespawnTimers.Remove(Victim);

	if (!CanRespawnNow(Victim))
	{
		return;
	}

	if (APlayerState* PS = Victim->PlayerState)
	{
		PendingRespawnPlayers.Remove(PS);
	}
	EarliestRespawnServerTime.Remove(Victim);

	RestartPlayer(Victim);
}

bool ABRGameMode::CanRespawnNow(AController* Candidate) const
{
	if (!HasAuthority() || !Candidate || !Candidate->PlayerState)
	{
		return false;
	}

	const EBRMatchPhase Phase = GetMatchPhase();

	if (Phase == EBRMatchPhase::PostMatch || Phase == EBRMatchPhase::None || bMatchDecided)
	{
		return false;
	}

	if (Phase == EBRMatchPhase::SuddenDeath && !bAllowRespawnInSuddenDeath)
	{
		return false;
	}

	// Only the dead respawn. Without this, a live player asking to respawn would be a
	// free teleport to the safest spawn on the map — the exact shape of a movement exploit.
	if (!PendingRespawnPlayers.Contains(Candidate->PlayerState))
	{
		return false;
	}

	if (const float* Earliest = EarliestRespawnServerTime.Find(Candidate))
	{
		if (GetServerTime() + KINDA_SMALL_NUMBER < *Earliest)
		{
			return false;
		}
	}

	return true;
}

bool ABRGameMode::RequestRespawn(AController* Requester)
{
	if (!HasAuthority() || !Requester)
	{
		return false;
	}

	// Everything a Server RPC's _Validate could check is re-checked here, because
	// _Validate can only see the wire arguments — possession, liveness, phase and the
	// earliest-allowed stamp are server truth and are checked at the point of effect.
	// A spammed request is refused, never queued: refusal is cheap, a queue is a weapon.
	if (!CanRespawnNow(Requester))
	{
		UE_LOG(LogBRNet, Verbose, TEXT("[Match] Respawn request from %s refused."), *GetNameSafe(Requester->PlayerState));
		return false;
	}

	if (FTimerHandle* Handle = RespawnTimers.Find(Requester))
	{
		GetWorldTimerManager().ClearTimer(*Handle);
		RespawnTimers.Remove(Requester);
	}

	if (APlayerState* PS = Requester->PlayerState)
	{
		PendingRespawnPlayers.Remove(PS);
	}
	EarliestRespawnServerTime.Remove(Requester);

	RestartPlayer(Requester);
	return true;
}

// ---------------------------------------------------------------------------
// Scored spawn selection
// ---------------------------------------------------------------------------

void ABRGameMode::GatherSpawnCandidates(TArray<AActor*>& OutCandidates) const
{
	OutCandidates.Reset();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		if (APlayerStart* Start = *It)
		{
			OutCandidates.Add(Start);
		}
	}
}

float ABRGameMode::ScoreSpawnCandidate(const AActor* Candidate, AController* ForPlayer) const
{
	if (!Candidate)
	{
		return -1.f;
	}

	const UWorld* World = GetWorld();
	const AGameStateBase* GS = GetGameState<AGameStateBase>();
	if (!World || !GS)
	{
		return 0.f;
	}

	const FVector CandidateLocation = Candidate->GetActorLocation();
	const uint8 SpawningTeam = ForPlayer ? ResolveTeamId(ForPlayer->PlayerState) : BRMatch::InvalidTeamId;

	float NearestThreatDistance = TNumericLimits<float>::Max();
	bool bVisibleToThreat = false;

	for (APlayerState* PS : GS->PlayerArray)
	{
		if (!PS || PS == (ForPlayer ? ForPlayer->PlayerState : nullptr))
		{
			continue;
		}

		// Teammates are not threats; the dead are not threats.
		if (ResolveTeamId(PS) == SpawningTeam || PendingRespawnPlayers.Contains(PS))
		{
			continue;
		}

		const APawn* ThreatPawn = PS->GetPawn();
		if (!ThreatPawn)
		{
			continue;
		}

		const FVector ThreatLocation = ThreatPawn->GetActorLocation();
		const float Distance = FVector::Dist(CandidateLocation, ThreatLocation);
		NearestThreatDistance = FMath::Min(NearestThreatDistance, Distance);

		if (!bVisibleToThreat)
		{
			// Bounded work: this runs once per respawn (event-driven), never per frame.
			FCollisionQueryParams Params(SCENE_QUERY_STAT(BRSpawnVisibility), /*bTraceComplex=*/false);
			Params.AddIgnoredActor(Candidate);
			Params.AddIgnoredActor(ThreatPawn);
			bVisibleToThreat = !World->LineTraceTestByChannel(
				CandidateLocation + FVector(0.f, 0.f, 88.f), ThreatLocation, ECC_Visibility, Params);
		}
	}

	if (NearestThreatDistance == TNumericLimits<float>::Max())
	{
		NearestThreatDistance = 100000.f; // No living enemy: every point is equally safe.
	}

	float Score = NearestThreatDistance;

	// Farthest-from-threat, with the two things distance alone gets wrong: a spawn an
	// enemy is already looking at, and a spawn somebody just used.
	if (bVisibleToThreat)
	{
		Score *= 0.25f;
	}

	if (const float* LastUsed = SpawnPointLastUsed.Find(const_cast<AActor*>(Candidate)))
	{
		if (GetServerTime() - *LastUsed < SpawnReuseWindowSeconds)
		{
			Score *= 0.5f;
		}
	}

	return Score;
}

AActor* ABRGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	TArray<AActor*> Candidates;
	GatherSpawnCandidates(Candidates);

	if (Candidates.Num() == 0)
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	AActor* Best = nullptr;
	float BestScore = -1.f;

	for (AActor* Candidate : Candidates)
	{
		const float Score = ScoreSpawnCandidate(Candidate, Player);
		if (Score > BestScore)
		{
			BestScore = Score;
			Best = Candidate;
		}
	}

	if (!Best)
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	SpawnPointLastUsed.Add(Best, GetServerTime());
	return Best;
}

// ---------------------------------------------------------------------------
// Rules + helpers
// ---------------------------------------------------------------------------

void ABRGameMode::ApplyMatchRules(int32 InScoreLimit, float InMatchSeconds, float InSuddenDeathSeconds,
	float InWarmupSeconds, float InRespawnSeconds, float InKillCreditWindowSeconds)
{
	if (!HasAuthority())
	{
		return;
	}

	const ABRGameState* BRGS = GetBRGameState();
	if (BRGS && BRGS->IsScoringOpen())
	{
		UE_LOG(LogBRNet, Warning, TEXT("[Match] ApplyMatchRules refused: scoring already open."));
		return;
	}

	// Clamped, not trusted. A CSV typo may cost a designer a confusing playtest; it may
	// not produce a zero-length match, a negative respawn or an infinite credit window.
	ScoreLimit = FMath::Clamp(InScoreLimit, 1, 999);
	MatchDurationSeconds = FMath::Clamp(InMatchSeconds, 10.f, 3600.f);
	SuddenDeathSeconds = FMath::Clamp(InSuddenDeathSeconds, 5.f, 600.f);
	WarmupSeconds = FMath::Clamp(InWarmupSeconds, MinPhaseSeconds, 300.f);
	RespawnDelaySeconds = FMath::Clamp(InRespawnSeconds, 0.f, 60.f);
	KillCreditWindowSeconds = FMath::Clamp(InKillCreditWindowSeconds, 0.f, 30.f);

	if (ABRGameState* MutableGS = GetBRGameState())
	{
		MutableGS->ServerSetScoreLimit(ScoreLimit);
	}
}

uint8 ABRGameMode::ResolveTeamId(const APlayerState* Player) const
{
	if (!Player)
	{
		return BRMatch::InvalidTeamId;
	}

	APlayerState* MutablePlayer = const_cast<APlayerState*>(Player);

	// Seam, not a stub: IGenericTeamAgentInterface is the engine's own team contract and
	// BRPlayerState/BRPlayerController (BP02) are expected to implement it. If they carry
	// a bare TeamID instead, the BP04 follow-up overrides this one function — no other
	// line in this file learns about it.
	if (const IGenericTeamAgentInterface* Agent = Cast<IGenericTeamAgentInterface>(MutablePlayer))
	{
		const uint8 TeamId = Agent->GetGenericTeamId().GetId();
		return (TeamId == FGenericTeamId::NoTeam.GetId()) ? BRMatch::InvalidTeamId : TeamId;
	}

	if (const AController* OwningController = MutablePlayer->GetOwningController())
	{
		if (const IGenericTeamAgentInterface* ControllerAgent = Cast<IGenericTeamAgentInterface>(const_cast<AController*>(OwningController)))
		{
			const uint8 TeamId = ControllerAgent->GetGenericTeamId().GetId();
			return (TeamId == FGenericTeamId::NoTeam.GetId()) ? BRMatch::InvalidTeamId : TeamId;
		}
	}

	return BRMatch::InvalidTeamId;
}

APlayerState* ABRGameMode::ResolvePlayerState(const AActor* Actor)
{
	if (!Actor)
	{
		return nullptr;
	}

	if (const APlayerState* AsPlayerState = Cast<APlayerState>(Actor))
	{
		return const_cast<APlayerState*>(AsPlayerState);
	}

	if (const APawn* AsPawn = Cast<APawn>(Actor))
	{
		return AsPawn->GetPlayerState();
	}

	if (const AController* AsController = Cast<AController>(Actor))
	{
		return AsController->PlayerState;
	}

	return nullptr;
}

ABRGameState* ABRGameMode::GetBRGameState() const
{
	return GetGameState<ABRGameState>();
}

float ABRGameMode::GetServerTime() const
{
	// One clock for the whole match frame, and the SAME clock the clients read through
	// GetServerWorldTimeSeconds(). Mixing in GetTimeSeconds() here is how a deadline ends
	// up meaning two different instants on two machines.
	if (const AGameStateBase* GS = GetGameState<AGameStateBase>())
	{
		return GS->GetServerWorldTimeSeconds();
	}

	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.f;
}
