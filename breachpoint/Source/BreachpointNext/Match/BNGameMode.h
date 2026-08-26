#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "GameplayEffectTypes.h"
#include "Interfaces/AIBAmbitionProvider.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/SoftObjectPtr.h"
#include "BNGameMode.generated.h"

// Only ever used as a POINTER here (SpawnBot returns one, DespawnBot takes one), so a
// forward declaration is the right dependency -- and the header needs its OWN, because it
// was reaching AAIController transitively through the unity blob. Any edit that pushes a
// consumer out of unity (an adaptive non-unity build does exactly that) breaks the header
// with no source change of its own.
class AAIController;
class ABNBotController;
class ABNHillPoint;
class ABNPlayerState;

/** AGameMode, not GameModeBase: the match machine is the ENGINE'S — the replicated MatchState
 *  FNames, StartMatch/EndMatch, the Handle* transition hooks, and FGameModeEvents that the old
 *  module's session subsystem already listens to. BN implements the machine's own seams instead
 *  of running a private enum beside it. The cycle is WaitingToStart -> InProgress ->
 *  WaitingPostMatch -> WaitingToStart, in place — RestartGame()'s ServerTravel is deliberately
 *  NOT used, so the listen server's connections survive every round.
 *
 *  Law 4 note, stated rather than hidden: AGameMode's own Tick polls ReadyToStartMatch during
 *  warmup and ReadyToEndMatch during play. That tick is the parent's and exists either way; the
 *  queries it polls are trivial. Match END stays event-driven regardless (see ReadyToEndMatch). */
UCLASS(Config=Game)
class BREACHPOINTNEXT_API ABNGameMode : public AGameMode, public IAIBAmbitionProvider
{
	GENERATED_BODY()

public:
	ABNGameMode();

	/** Runs BEFORE PreInitializeComponents spawns the GameState, and AFTER any Blueprint child's
	 *  serialisation — so the classes forced here cannot be out-serialised by a BP_BNGameMode
	 *  dropdown. This is what closed the TASK-R4-GAMESTATE-CLASS editor ticket. */
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

	/** THE START GATE, on the machine's own seam: the parent's Tick polls this during
	 *  WaitingToStart and calls StartMatch the frame it first says yes. Bots do not count —
	 *  GetNumPlayers() is humans — and the bot FILL does not live here: a query polled every
	 *  warmup frame must not spawn actors, so the fill rides the event edges instead
	 *  (HandleMatchIsWaitingToStart and OnPostLogin). bDelayedStart is ignored by this override. */
	virtual bool ReadyToStartMatch_Implementation() override;

	/** Warmup bodies are real: BN's lobby is frozen pawns on the floor, not spectators in a void.
	 *  The parent refuses all restarts outside InProgress; this opens WaitingToStart as well.
	 *  WaitingPostMatch stays closed — corpses do not stand up during the post-match. */
	virtual bool PlayerCanRestart_Implementation(APlayerController* Player) override;

	/** Entering warmup — the first boot and every in-place restart. Super may fire world
	 *  BeginPlay; then the bot fill converges the lobby, and any pawnless human is stood up. */
	virtual void HandleMatchIsWaitingToStart() override;

	/** The match beginning, wherever StartMatch was triggered from. Everything the old BeginMatch
	 *  did: new generation, clock stamp + buzzer timer, thaw. On a RESTART (generation > 1) it
	 *  also rebodies everyone — fresh attributes at fresh start points is what "new round" means. */
	virtual void HandleMatchHasStarted() override;

	/** The match over, wherever EndMatch was triggered from. Clears the buzzer, freezes everyone,
	 *  prints the winner line (the winner was written BEFORE the state flipped — FinishMatch's
	 *  ordering), and arms the post-match timer toward RestartMatch. */
	virtual void HandleMatchHasEnded() override;

	/** The ONE initialization seam for both kinds of player: the engine calls it for humans in the
	 *  login flow, the bot fill calls it for bots — an AIController never sees OnPostLogin.
	 *  Subscribes this mode to the player's death announcement (law 7: the mode LISTENS for deaths
	 *  rather than being called by the ability that causes them) and freezes a warmup/post-match
	 *  joiner. The engine may call it more than once per controller, so the subscription is
	 *  guarded against doubling. */
	virtual void GenericPlayerInitialization(AController* C) override;

	/** Humans only. The arrival changes the seat math, so the fill converges here; the start
	 *  itself is the parent's poll asking ReadyToStartMatch — nothing to call by hand. */
	virtual void OnPostLogin(AController* NewPlayer) override;

	/** TEAMS (BN15): tagged starts when teams are on — a start whose PlayerStartTag is
	 *  "Team0"/"Team1" serves only that side; untagged starts serve anyone; NoTeam and
	 *  teams-off fall through to Super. Never fails a spawn over a missing tag. */
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	/** The other edge of the seat math: a human leaving warmup opens a seat a bot should take.
	 *  Deferred one tick because inside Logout the leaver is still iterable and every count is
	 *  off by one. Mid-match leaves change nothing — the fill's own guard refuses outside
	 *  warmup, so mid-match backfill stays the named deferral it always was. */
	virtual void Logout(AController* Exiting) override;

	/** The subscriber. Turns "this player died" into this mode's answer: THE kill line, then a
	 *  timed respawn. The score-limit check lives here because this is where the kill credits. */
	void HandlePlayerDeath(ABNPlayerState* Victim, ABNPlayerState* Killer, FName SourceName);

	/** Read by ABNGameState, which mirrors it so clients can render "12 / 25". */
	int32 GetScoreLimit() const { return ScoreLimit; }

	// -- IAIBAmbitionProvider (Phase 6: the mode tells bots what it wants) --------------
	/** One ambition when the hill is on, none in plain Slayer — "a mode adds ambitions,
	 *  never a system" is the interface's own sentence, and Slayer proves the zero case. */
	virtual void GetModeAmbitions(TArray<FAIBModeAmbition>& OutAmbitions) const override;

	/** HUD-grade urgency from the hill state the scoring timer already computes: a human
	 *  reads held/contested/empty off the objective marker, so a bot may too. Per-bot
	 *  ONLY through "is the holder me" — no enemy positions leak through this number. */
	virtual float GetObjectiveUrgency(const AActor* Bot, FGameplayTag AmbitionTag) const override;

	/** Authority: the ONE respawn path. Delay then RestartPlayer. */
	void RequestRespawn(AController* Controller);

protected:
	/** Weak, because the player can leave inside the delay and a raw pointer would outlive them.
	 *  Refuses outside InProgress — a corpse must not stand up during the post-match. */
	void RespawnPlayer(TWeakObjectPtr<AController> WeakController);

	/** Decides, then ends: writes the winner onto the GameState FIRST, then runs the parent's
	 *  EndMatch() so the state flip replicates in the same bunch as a winner that is already
	 *  there. InWinner may be null — a tie at the buzzer is a legal, renderable outcome.
	 *  Not named EndMatch: the parent's EndMatch() is the machine's verb and stays visible.
	 *
	 *  ReadyToEndMatch is deliberately NOT overridden (the parent's constant false): the score
	 *  limit must fire on the exact kill that crossed it, and the buzzer must decide the winner
	 *  before the state flips — a poll-driven EndMatch() flips state with no winner written. */
	void FinishMatch(ABNPlayerState* InWinner);

	/** G4 — the fill, now a CONVERGENCE: authority, WaitingToStart only. Short of TargetPlayers
	 *  spawns bots; OVER it (a human claimed a seat a bot was warming) despawns the newest bots
	 *  until humans + bots == TargetPlayers. That closes R5's recorded overshoot. EDGE-driven,
	 *  not continuous: it runs on warmup entry, human join, and human leave — between edges the
	 *  lobby is whatever the last edge left (critic's precision, worth keeping precise). */
	void EnsureBotFill();

	/** One bot, Lyra's way: spawn the controller (transient — never saved into a map), name its
	 *  PlayerState, GenericPlayerInitialization, RestartPlayer. Null on failure, loudly. */
	AAIController* SpawnBot(int32 Index);

	/** The reverse, for seat-yield: pawn first, then controller — destroying the controller is
	 *  what retires its PlayerState from every roster. */
	void DespawnBot(AAIController* Bot);

	/** The time limit's timer fires here. Sole leader wins; a tie leaves Winner null. */
	void OnTimeLimitReached();

	/** IN PLACE — no ServerTravel, no map reload. Clears the winner and every score, then sends
	 *  the machine back to WaitingToStart; the parent's poll starts the next round the moment the
	 *  gate holds, and HandleMatchHasStarted rebodies everyone because the generation says restart.
	 *  If the humans left during the post-match, the mode simply sits in warmup — which is new,
	 *  and correct: the old code restarted an empty match. */
	void RestartMatch();

	/** The freeze, as GAS state: a UBNGE_State spec carrying State.Match.Frozen. The tag rides the
	 *  SPEC, never a CDO container — native tags are not guaranteed registered while CDOs build. */
	void SetPlayerFrozen(ABNPlayerState* InPlayerState, bool bFrozen);
	void SetAllPlayersFrozen(bool bFrozen);

	/** THE HILL's one second (law 4: a timer, and the mode's ONLY recurring one besides the
	 *  clock). Overlaps the hill sphere, counts distinct living players, scores the sole
	 *  occupant, caches holder/contested for GetObjectiveUrgency, ends the match on the
	 *  second that crosses the limit. TEAMS (BN15): occupants dedupe per TEAM — same-team
	 *  bodies do not contest each other; contested = more than one distinct team present. */
	void HillTick();

	/** TEAMS (BN15): idempotent (a NoTeam check makes re-inits free), authority-only,
	 *  fewest-members-wins over GameState->PlayerArray, ties to the lower id. Called
	 *  from GenericPlayerInitialization — the ONE seam both humans and the bot fill
	 *  pass through, before each entity's first spawn choice. No-op when teams are off. */
	void AssignTeamIfNeeded(AController* C);
	uint8 GetLowestPopulationTeam() const;

	/** The team win: writes WinningTeamId FIRST, then EndMatch — FinishMatch's own
	 *  ordering, sibling'd for a team (the PlayerState winner stays null; the HUD renders
	 *  whichever record is set). */
	void FinishTeamMatch(uint8 WinningTeam);

	/** Spawn-or-adopt at match start (idempotent — a restart reuses the live hill), push it
	 *  into the world query, arm the timer. No-op when bHillEnabled is false. */
	void StartHill();

	UPROPERTY(Config)
	FSoftClassPath DefaultPawnClassPath;

	UPROPERTY(Config)
	float RespawnDelay = 3.f;

	/** EditDefaultsOnly so BP_BNGameMode's details panel can dial this per-Blueprint (R26: a BP
	 *  child holding default values only). TESTING DEFAULT — 3, not a tuning decision, so a
	 *  round ends after one or two kills and both the win and lose screens are actually reachable
	 *  without sitting through a real match. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Match")
	int32 ScoreLimit = 3;

	UPROPERTY(Config)
	float TimeLimit = 600.f;

	/** Default 1 so solo PIE always runs. */
	UPROPERTY(Config)
	int32 MinPlayers = 1;

	/** The lobby size the fill converges to. Humans + bots together reach this, never exceed it. */
	UPROPERTY(Config)
	int32 TargetPlayers = 4;

	/** In order; a bot past the end of the list falls back to "Bot <n>". */
	UPROPERTY(Config)
	TArray<FString> BotNames;

	/** Soft (law 3): the ini names the C++ class path, nothing here hard-references it. */
	UPROPERTY(Config)
	TSoftClassPtr<ABNBotController> BotControllerClass;

	/** THE A/B SWITCH: "BN" (default) spawns BotControllerClass, "AIB" spawns
	 *  AIBBotControllerClass. Both systems live in one build; one ini line flips them,
	 *  so any AIB regression is a one-line revert to a known-good baseline. An unknown
	 *  value warns once and falls back to BN — ResolveTuning's discipline. */
	UPROPERTY(Config)
	FName BotSystem = TEXT("BN");

	UPROPERTY(Config)
	TSoftClassPtr<AAIController> AIBBotControllerClass;

	/** Server-only bookkeeping — bots exist nowhere else, so nothing here replicates.
	 *  Widened to AAIController for the switch: everything downstream (init, respawn,
	 *  despawn, prune) was already controller-class-agnostic — the seam audit's finding. */
	UPROPERTY()
	TArray<TObjectPtr<AAIController>> SpawnedBots;

	/** Monotonic, never reused: naming from the live book's COUNT hands a warmup refill the same
	 *  name twice, and two Ossians make every kill line and the winner announce ambiguous. */
	int32 NextBotNameIndex = 0;

	UPROPERTY(Config)
	float PostMatchDuration = 10.f;

	/** Bumped by every match start; 1 is the first match, above 1 is a restart. A respawn timer
	 *  armed in an older generation is dropped when it fires rather than destroying a pawn that
	 *  belongs to the round after it. */
	int32 MatchGeneration = 0;

	/** TEAMS (BN15). Off = today's FFA byte-for-byte: no assignment, every TeamId stays
	 *  NoTeam, and every team code path is inert behind the NoTeam guard. */
	UPROPERTY(Config)
	bool bTeamsEnabled = false;

	/** Only read when teams are on; the damage door consults it. Self-damage is always
	 *  allowed — the kill-credit rules already word that case. */
	UPROPERTY(Config)
	bool bFriendlyFire = false;

	/** THE HILL (founder ruling, 26 Aug 2026). Off by default so Slayer stays Slayer;
	 *  one ini flip runs the objective mode with zero level edits (C++-first). */
	UPROPERTY(Config)
	bool bHillEnabled = false;

	UPROPERTY(Config)
	FVector HillLocation = FVector::ZeroVector;

	UPROPERTY(Config)
	float HillRadius = 600.f;

	UPROPERTY(Config)
	int32 HillPointsPerSecond = 1;

	/** Weak: the hill is a world actor and a level unload must not be fought over it. */
	TWeakObjectPtr<ABNHillPoint> Hill;

	/** The urgency cache, written ONLY by HillTick, read by GetObjectiveUrgency — the
	 *  provider answers from last second's ruling instead of re-overlapping per think. */
	TWeakObjectPtr<ABNPlayerState> HillHolder;
	bool bHillContested = false;

	/** ONE timer for the whole match clock (law 4: no Tick, no per-frame poll). */
	FTimerHandle MatchTimerHandle;
	FTimerHandle PostMatchTimerHandle;
	FTimerHandle HillTimerHandle;

	/** The active freeze GE per player, so it is removed by handle rather than by sweeping tags off
	 *  an ASC that holds other State.* effects. Weak keys: a player can leave while frozen. */
	TMap<TWeakObjectPtr<ABNPlayerState>, FActiveGameplayEffectHandle> FrozenHandles;
};
