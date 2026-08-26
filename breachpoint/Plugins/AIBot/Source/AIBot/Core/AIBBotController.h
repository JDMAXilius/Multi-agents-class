#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Brain/AIBConfidenceModel.h"
#include "Core/AIBTypes.h"
#include "GameplayTagContainer.h"
#include "Interfaces/AIBAmbitionProvider.h"
#include "Perception/AIBSensorium.h"
#include "Skills/AIBAimPolicy.h"
#include "Skills/AIBGrenadePolicy.h"
#include "Skills/AIBMeleePolicy.h"
#include "Skills/AIBMovementPolicy.h"
#include "Skills/AIBSkillProfile.h"
#include "AIBBotController.generated.h"

class IAIBAvatarInterface;
class IAIBExecutor;
class IAIBWorldQuery;
class UAIBAmbitionEngine;
class UAIPerceptionComponent;
class UAISenseConfig_Hearing;
class UAISenseConfig_Sight;
class UStateTree;
class UStateTreeAIComponent;
struct FAIStimulus;

/**
 * The HAND. Owns the engine perception (eyes/ears), feeds the sensorium, hosts the brain
 * (Phase 2), runs the executor (Phase 3), presses verbs — and decides nothing itself.
 *
 * Server-only by construction, tickless by law: a think timer pumps the sensorium and,
 * later, the brain. Because there is no tick, the engine's focus-based aim never runs —
 * aim will be stepped explicitly by executor tasks (the seam audit's lesson).
 *
 * Perception is FFA-open (detect everyone; hostility is decided above perception, so
 * teams can land later without touching the senses) — the pattern transcribed from the
 * host's compiled controller, not designed fresh.
 */
UCLASS(Config=Game)
class AIBOT_API AAIBBotController : public AAIController
{
	GENERATED_BODY()

public:
	AAIBBotController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The avatar door, resolved at possession. Null when the pawn carries no adapter —
	 *  loud (one Error) and the bot stands, never crashes. Validity rides the GC-tracked
	 *  UPROPERTY half of the pair: a host destroying its adapter component mid-life must
	 *  yield null here, never a dangling raw pointer (W-REVIEW P3, two passes). */
	IAIBAvatarInterface* GetAvatar() const { return IsValid(AvatarObject) ? Avatar : nullptr; }

	/** The matured world — the ONLY awareness anything downstream may read. */
	const FAIBSensorium& GetSensorium() const { return Sensorium; }

	/** The arbitration layer; valid while possessing on the authority. */
	UAIBAmbitionEngine* GetAmbitionEngine() const { return AmbitionEngine; }

	/** The execution surface (Phase 3). The executor drives it; nothing else touches it. */
	UStateTreeAIComponent* GetStateTreeComponent() const { return StateTreeComponent; }

	/** Where the compiled behaviour asset lives — ini-set, resolved by the executor at
	 *  possession. Soft by law 3's sibling rule: the module hard-references no asset. */
	const TSoftObjectPtr<UStateTree>& GetBotStateTreePath() const { return BotStateTree; }

	/** The last matured, arbitrated world — what executor tasks read mid-frame. Facts are
	 *  built ONCE per Think (F3: one belief sample per pump); tasks reading this cache
	 *  cannot accidentally re-sample the live world between pumps. */
	const FAIBFacts& GetLastFacts() const { return LastFacts; }

	// -- Phase 4 integration: the skill surface executor tasks consume ---------------
	// The PROFILE answers "how good is this bot at X"; the STATES are per-life policy
	// scratch that must survive branch re-entry (StateTree re-initialises instance data
	// on every state ENTRY — the grenade-cooldown lesson, applied to every policy);
	// the STREAM is per-bot so no two bots dance in lockstep (F-3.7).
	const FAIBSkillProfile& GetSkillProfile() const { return SkillProfile; }

	/** Phase 8: the RESOLVED tier row — every consumer of a tier number reads this,
	 *  never a function-local default (the two "Phase 8 resolves the real tier"
	 *  markers this replaced). Valid from OnPossess; the defaults row before that. */
	const FAIBTierRow& GetTierRow() const { return ResolvedTier; }

	/** Host-callable (the game mode assigning mixed lobbies): takes effect at the NEXT
	 *  possession — a tier is a per-life resolution, same as every policy state. */
	void SetTierName(FName InTierName) { BotTier = InTierName; }
	FName GetTierName() const { return BotTier; }
	FAIBAimState& GetAimState() { return AimState; }
	FAIBMeleeState& GetMeleeState() { return MeleeState; }
	FAIBGrenadeState& GetGrenadeState() { return GrenadeState; }
	FAIBMovementState& GetMovementState() { return MovementState; }
	FRandomStream& GetPolicyRandom() { return PolicyRandom; }

	// -- Phase 6: the provider doors (pulled from UAIBBotManager at possession) --------
	// Same twin-pointer validity rule as the avatar door: the interface half runs, the
	// GC-tracked half decides whether it still may.
	IAIBAmbitionProvider* GetAmbitionProvider() const { return IsValid(AmbitionProviderObject) ? AmbitionProvider : nullptr; }
	IAIBWorldQuery* GetWorldQuery() const { return IsValid(WorldQueryObject) ? WorldQuery : nullptr; }

	/** The typed join a mode branch's mover needs: the ObjectiveKind of the CURRENT
	 *  ambition, from the mode set cached at the last refresh. Invalid when the current
	 *  want is not a mode want or names no kind. */
	FGameplayTag GetObjectiveKindForCurrentAmbition() const;

	/** THE EXECUTOR'S ONE REPORT BACK (AIB16). A branch that could not run tells the
	 *  brain so, and the engine silences that want long enough for another to have a
	 *  turn. Without it a failing branch keeps its score, keeps winning, and the bot
	 *  stops making decisions entirely — measured: 0 kills against 76.
	 *
	 *  Deliberately the ONLY direction the executor speaks: it reports what happened,
	 *  never what to want next. Choosing stays with the engine. */
	void NoteCurrentAmbitionFailed();

	/** THE POSSESSION OBLIGATION, finally payable (ARCHITECTURE's recorded CTF-in-Slayer
	 *  debt): clear + core + the CURRENT mode's translated ambitions + one immediate
	 *  Think, so the empty-tag window never reaches a tree selection. Also the mid-life
	 *  mode-swap API — the host calls it on a round transition. */
	void RefreshAmbitions();

	/** THE ONE THROTTLE THAT MUST OUTLIVE A BRANCH, and it lives here for a mechanical
	 *  reason worth naming: a StateTree re-initialises a state's instance data from the
	 *  compiled defaults every time that state is ENTERED, so a cooldown kept in a task's
	 *  scratch is silently reset by an Engage branch that flaps — and Engage flaps by
	 *  design (its belief tasks fail on a visibility loss, re-selecting 0.2s later). A
	 *  grenade cooldown reset every second is no cooldown at all, which is exactly the
	 *  "seven grenades in one second" failure. The CONTROLLER outlives every state, so the
	 *  gate is a wall-clock stamp on it. The duration stays with the behaviour that spends
	 *  it — the task passes it in.
	 *
	 *  Fire, reload, melee and swap deliberately do NOT come here: each is refused
	 *  harmlessly by the host's own ability state, and a re-entry costing one extra tap is
	 *  not a fairness problem. A grenade is. */
	bool CanThrowGrenade() const;
	void NoteGrenadeThrown(float CooldownSeconds);

	/** The game's projectile warning seam calls this (via the adapter wiring). It NOTES —
	 *  the dodge happens only after the stimulus matures (FAIRPLAY F2). */
	void NoteIncomingBlast(const FVector& Center, float Radius, double DetonateAtSeconds);

	/** THE DAMAGE SEAM (Phase 5). The host's one-per-hit damage site calls these on the
	 *  authority. Taken: the hit's fraction of THIS bot's max health, plus the attacker
	 *  and its location AT THE HIT — which becomes a matured MEMORY through the same
	 *  reaction clock as every sense (being shot makes a bot go and LOOK; a perfect lock
	 *  on someone never seen would be omniscience — the host's own ruling, kept). Dealt:
	 *  the fraction of the VICTIM's max health a hit this bot landed removed. Both feed
	 *  the momentum ledger the confidence model and the damage-history facts read. */
	void NoteDamageTaken(AActor* Attacker, const FVector& AttackerLocation, float FractionOfMaxHealth);
	void NoteDamageDealt(float FractionOfVictimMaxHealth);

	// FFA seam, verbatim from the host's proven pattern: one shared "no team",
	// hostility decided per-pawn. A team system replaces these two overrides.
	virtual FGenericTeamId GetGenericTeamId() const override { return FGenericTeamId(255); }
	virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	/** Teardown (PIE end, travel) does not promise UnPossess — this is the ordered path
	 *  that stops the executor while the avatar door is still valid (W-REVIEW P3). */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	UFUNCTION()
	void OnPerceptionForgotten(AActor* Actor);

private:
	void Think();

	UPROPERTY(VisibleAnywhere, Category = "AIBot")
	TObjectPtr<UAIPerceptionComponent> BotPerception;

	UPROPERTY(VisibleAnywhere, Category = "AIBot")
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	UPROPERTY(VisibleAnywhere, Category = "AIBot")
	TObjectPtr<UAISenseConfig_Hearing> HearingConfig;

	/** Born in the constructor (components must be); logic start is the executor's call,
	 *  never automatic — the host controller's proven, respawn-idempotent shape. */
	UPROPERTY(VisibleAnywhere, Category = "AIBot")
	TObjectPtr<UStateTreeAIComponent> StateTreeComponent;

	/** Soft path to the compiled tree, from [/Script/AIBot.AIBBotController] in ini.
	 *  UAIBTreeAuthoring builds the asset it names, from the editor. */
	UPROPERTY(Config)
	TSoftObjectPtr<UStateTree> BotStateTree;

	/** Seconds between thinks. Config so the terminal can tune cadence without a
	 *  recompile; the floor law does not live here (the clock owns it). */
	UPROPERTY(Config)
	float ThinkIntervalSeconds = 0.1f;

	/** The trace channel the blast perceivability gate tests (an ECollisionChannel
	 *  value; int because the ini writes a number). Default ECC_Visibility — but a
	 *  channel's MEANING is entirely the host project's collision config (a project
	 *  whose cover blocks weapons yet ignores Visibility turns this gate into a no-op
	 *  through a wall). The HOST decides via ini what channel honestly answers "could
	 *  eyes reach the blast point" (W-REVIEW P3); this module only defaults it. */
	UPROPERTY(Config)
	int32 BlastPerceivabilityChannel = ECC_Visibility;

	/** Phase 8: which tier this bot resolves at possession. Config so one ini line
	 *  runs a whole lobby at a tier; the host's mode can override per-bot through
	 *  SetTierName for mixed lobbies. An unknown name falls back to the defaults row,
	 *  loudly (F7). */
	UPROPERTY(Config)
	FName BotTier = TEXT("Marine");

	/** Phase 8: draw the per-bot overlay (ambition scores, confidence, skill vector,
	 *  stimulus queue) over each bot's head every think. Config: flip in the ini, or
	 *  at runtime from the editor's settings — the eyes-on half of proof 3. */
	UPROPERTY(Config)
	bool bDebugOverlay = false;

	/** The resolved row (see GetTierRow). */
	FAIBTierRow ResolvedTier;

	FAIBSensorium Sensorium;
	FTimerHandle ThinkTimer;

	UPROPERTY()
	TObjectPtr<UAIBAmbitionEngine> AmbitionEngine;

	/** For the one ambition-switch log line per change (the verifier's instrument). */
	FGameplayTag LastLoggedAmbition;

	/** For the one fairness log line per acquisition (aib-verifier's sample). */
	TWeakObjectPtr<AActor> LastLoggedTarget;

	/** One fact snapshot per Think; see GetLastFacts. */
	FAIBFacts LastFacts;

	/** World seconds before which no grenade may be thrown; see CanThrowGrenade. */
	float NextGrenadeThrowTimeSeconds = 0.f;

	/** Counts possessions, and exists ONLY to vary the per-life seeds: re-seeding a
	 *  respawn from the bare controller id replayed a byte-identical draw sequence every
	 *  life — the same first jink, the same reaction latency, a learnable tell that
	 *  reset on death (W-REVIEW P4+5 H5). Deterministic given (bot, life). */
	int32 LifeIndex = 0;

	// Phase 5: momentum + judgment. The ledger is the damage-history facts' source; the
	// model turns facts into ConfidenceNorm at think cadence. Phase 4's profile gets its
	// first consumer here (Confidence level = judgment quality); Phase 8 re-resolves it
	// from the real tier row.
	FAIBDamageLedger DamageLedger;
	FAIBConfidenceState ConfidenceState;
	FAIBSkillProfile SkillProfile;

	// Phase 4 integration: per-life policy scratch (see the accessors' comment) and the
	// execution-side draw stream. One stream for all four skills is deliberate — the
	// F-3.7 hazard is cross-BOT lockstep, not cross-skill; per-skill streams would buy
	// replay granularity nothing downstream reads yet.
	FAIBAimState AimState;
	FAIBMeleeState MeleeState;
	FAIBGrenadeState GrenadeState;
	FAIBMovementState MovementState;
	FRandomStream PolicyRandom;

	/** The misjudge draws. Its OWN stream, seeded per bot beside the sensorium's:
	 *  sharing would let a confidence redraw shift every later reaction latency, and
	 *  determinism per subsystem is what keeps specs and replays honest. */
	FRandomStream ConfidenceRandom;

	/** True once the host's damage seam has EVER called in — per controller, never reset
	 *  per life: seam wiring is a host property. Until then damage history stays an
	 *  honest UNKNOWN rather than a confident zero. */
	bool bDamageSeamSeen = false;

	IAIBAvatarInterface* Avatar = nullptr;

	UPROPERTY()
	TObjectPtr<UObject> AvatarObject;

	// Phase 6: the provider doors + the mode set the last refresh translated (cached so
	// the executor's kind join reads a list this controller owns, never a live provider
	// walk per tick).
	IAIBAmbitionProvider* AmbitionProvider = nullptr;

	UPROPERTY()
	TObjectPtr<UObject> AmbitionProviderObject;

	IAIBWorldQuery* WorldQuery = nullptr;

	UPROPERTY()
	TObjectPtr<UObject> WorldQueryObject;

	TArray<FAIBModeAmbition> CachedModeAmbitions;

	// The executor door, the avatar door's twin: the interface pointer is what runs, the
	// UObject pointer is what keeps it alive. Today the concrete type is the StateTree
	// executor; a Behavior Tree impl replaces one NewObject line, nothing else.
	IAIBExecutor* Executor = nullptr;

	UPROPERTY()
	TObjectPtr<UObject> ExecutorObject;
};
