#include "Core/AIBBotController.h"

#include "AIBotModule.h"
#include "Brain/AIBAmbitionEngine.h"
#include "Components/ActorComponent.h"
#include "Components/StateTreeAIComponent.h"
#include "Core/AIBBotManager.h"
#include "Core/AIBFactsBuilder.h"
#include "Core/AIBNavArea_Lane.h"
#include "Core/AIBPathFollowingComponent.h"
#include "Core/AIBQueryFilter.h"
#include "Core/AIBTags.h"
#include "Data/AIBDataRows.h"
#include "Data/AIBTiers.h"
#include "Debug/AIBGameplayDebugger.h"
#include "EngineUtils.h"
#include "Execution/AIBStateTreeExecutor.h"
#include "GameFramework/PlayerStart.h"
#include "Interfaces/AIBAvatarInterface.h"
#include "Interfaces/AIBWorldQuery.h"
#include "NavigationSystem.h"
#include "NavMesh/NavMeshPath.h"
#include "NavMesh/RecastNavMesh.h"
#include "Navigation/PathFollowingComponent.h"
#include "Navigation/CrowdFollowingComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISense_Hearing.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"
#include "Team/AIBTeamCoordinator.h"
#include "TimerManager.h"

AAIBBotController::AAIBBotController(const FObjectInitializer& ObjectInitializer)
	// The subobject name is a literal in AIController.cpp; the swap is how a path segment
	// gets to press a verb (AIB22 step 4).
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UAIBPathFollowingComponent>(TEXT("PathFollowingComponent")))
{
	// A real PlayerState: the bot joins the same match machinery a human does. (This is
	// the module's one deliberate cause of replication — a bot IS a player at the
	// netcode layer. ARCHITECTURE law 3 names the exception.)
	bWantsPlayerState = true;

	// Law 4: no gameplay Tick. Thinking is the timer's; reacting is the clock's.
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	BotPerception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("BotPerception"));
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("BotSight"));

	// Seeded from the ROW's defaults — the tier system (Phase 8) re-applies at possession.
	const FAIBTierRow Defaults;
	SightConfig->SightRadius = Defaults.SightRadius;
	SightConfig->LoseSightRadius = Defaults.LoseSightRadius;
	SightConfig->PeripheralVisionAngleDegrees = Defaults.PeripheralVisionAngleDegrees;
	// Without a max age the engine never forgets, OnPerceptionForgotten never fires,
	// and sight of an unseen enemy lives forever — the infinite-sight hole.
	SightConfig->SetMaxAge(Defaults.SightMaxAgeSeconds);

	// FFA-open senses: detection is not hostility. Hostility lives in the attitude
	// override plus the Note-boundary filter below, never in the senses.
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;

	HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("BotHearing"));
	HearingConfig->HearingRange = Defaults.HearingRange;
	HearingConfig->SetMaxAge(Defaults.SightMaxAgeSeconds);
	HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
	HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
	HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;

	// Sight dominant: seeing and hearing the same actor, the sighting wins.
	BotPerception->ConfigureSense(*SightConfig);
	BotPerception->ConfigureSense(*HearingConfig);
	BotPerception->SetDominantSense(SightConfig->GetSenseImplementation());
	SetPerceptionComponent(*BotPerception);

	// The execution surface. Auto-start is off (transcribed from the host controller):
	// the tree must not run before the executor has resolved and set the asset, and a
	// respawn must not double-start logic.
	StateTreeComponent = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("BotStateTree"));
	StateTreeComponent->SetStartLogicAutomatically(false);

	// Phase 14: ONE filter class for the mover door and every raw MoveToLocation — the
	// engine substitutes it wherever a request names none (AIController.cpp:604/625), and
	// instantiates it per query with this controller as the Querier.
	DefaultNavigationFilterClass = UAIBQueryFilter::StaticClass();

	// Delegate binding is NOT here: the shipping host binds at possession, gated and
	// deduped, precisely to keep bindings out of serialized CDO state (W-REVIEW F-4.1).

	SetGenericTeamId(FGenericTeamId(255));
}

ETeamAttitude::Type AAIBBotController::GetTeamAttitudeTowards(const AActor& Other) const
{
	// THE hostility authority is the game's (AIBWorldQuery.h): with a world query
	// registered, IAIBWorldQuery::AreAllies answers friend-or-foe — this consult is
	// what the old constant's comment promised, and it is the whole of teams inside
	// this module. AreAllies, NOT !AreEnemies: AreEnemies folds liveness into its
	// answer, and its negation read every corpse and every ASC-less spawn-window pawn
	// as Friendly — eating their perception LOSS events downstream, so a dead target
	// stayed "visible" until its actor was destroyed (teams W-REVIEW 26 Aug, both critics).
	// Alliance has no liveness: a dead enemy is still Hostile, and FFA (AreAllies
	// always false) is byte-identical to the old all-hostile constant. No query
	// registered, or no pawn on either side, keeps the same fallback: every pawn
	// that is not mine is hostile. Scenery is neutral either way; self is always
	// friendly.
	if (const APawn* OtherPawn = Cast<APawn>(&Other))
	{
		APawn* MyPawn = GetPawn();
		if (OtherPawn == MyPawn)
		{
			return ETeamAttitude::Friendly;
		}
		if (IAIBWorldQuery* Query = GetWorldQuery())
		{
			if (MyPawn && Query->AreAllies(MyPawn, &Other))
			{
				return ETeamAttitude::Friendly;
			}
		}
		return ETeamAttitude::Hostile;
	}
	return ETeamAttitude::Neutral;
}

void AAIBBotController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// ARCHITECTURE law 3, enforced rather than asserted: on a client this controller
	// does nothing at all — no timer, no perception state, no verbs ever (W-REVIEW 1c).
	if (!HasAuthority())
	{
		UE_LOG(LogAIBot, Warning, TEXT("AIBot: %s possessed on a non-authority — refusing to run."), *GetName());
		return;
	}

	Avatar = nullptr;
	AvatarObject = nullptr;
	LastLoggedTarget = nullptr;

	if (!InPawn)
	{
		return;
	}

	// Bind at possession, deduped — the shipping host's pattern, not CDO state.
	if (!BotPerception->OnTargetPerceptionUpdated.IsAlreadyBound(this, &AAIBBotController::OnPerceptionUpdated))
	{
		BotPerception->OnTargetPerceptionUpdated.AddDynamic(this, &AAIBBotController::OnPerceptionUpdated);
	}
	if (!BotPerception->OnTargetPerceptionForgotten.IsAlreadyBound(this, &AAIBBotController::OnPerceptionForgotten))
	{
		BotPerception->OnTargetPerceptionForgotten.AddDynamic(this, &AAIBBotController::OnPerceptionForgotten);
	}

	// The one lookup that joins bot to body: any component on the pawn implementing the
	// avatar interface. The game's adapter supplies it; this module never knows its type.
	for (UActorComponent* Component : InPawn->GetComponents())
	{
		if (IAIBAvatarInterface* AsAvatar = Cast<IAIBAvatarInterface>(Component))
		{
			Avatar = AsAvatar;
			AvatarObject = Component;
			break;
		}
	}

	if (Avatar)
	{
		UE_LOG(LogAIBot, Log, TEXT("AIBot: %s possessed %s, avatar door open."),
			*GetName(), *InPawn->GetName());
	}
	else
	{
		// The designed miss answer: loud, once, standing still — never a mystery.
		UE_LOG(LogAIBot, Error, TEXT("AIBot: %s possessed %s but found NO avatar adapter "
			"component. The bot will stand still. The game must add its adapter to the "
			"pawn (see Interfaces/AIBAvatarInterface.h)."),
			*GetName(), *InPawn->GetName());
	}

	// Phase 6: pull the providers from the manager — the host pushed them once; a bot
	// pulls per possession so a respawn re-resolves for free and a dead provider yields
	// null (loud below), never a dangling call.
	AmbitionProvider = nullptr;
	AmbitionProviderObject = nullptr;
	WorldQuery = nullptr;
	WorldQueryObject = nullptr;
	// Phase 14: the match seed and this controller's stable slot ride the same pull. No
	// manager (a headless world) leaves seed 0 / slot -1 — still deterministic.
	int32 MatchSeed = 0;
	BotIndex = INDEX_NONE;
	if (UAIBBotManager* Manager = GetWorld() ? GetWorld()->GetSubsystem<UAIBBotManager>() : nullptr)
	{
		AmbitionProvider = Manager->GetAmbitionProvider();
		AmbitionProviderObject = Manager->GetAmbitionProviderObject();
		WorldQuery = Manager->GetWorldQuery();
		WorldQueryObject = Manager->GetWorldQueryObject();
		MatchSeed = Manager->GetMatchSeed();
		BotIndex = Manager->AssignBotIndex(*this);
	}

	// The brain. The ENGINE object survives across lives; its REGISTRY no longer does —
	// RefreshAmbitions below pays ARCHITECTURE's recorded possession obligation (clear +
	// core + current mode), which is what keeps a CTF want from scoring inside Slayer.
	if (!AmbitionEngine)
	{
		AmbitionEngine = NewObject<UAIBAmbitionEngine>(this);
	}
	LastLoggedAmbition = FGameplayTag();
	LastFacts = FAIBFacts(); // a fresh life reads no stale world
	AllyFightMemory = FAIBAllyFightMemory(); // and heard no stale fights (AIB17)

	// PHASE 8 — the real tier, resolved per possession from the C++ registry (the ONE
	// source of truth; DT_AIBTiers only mirrors it). An unknown name is a config typo
	// worth a loud line and a Marine-shaped fallback, never a crash or a superhuman.
	const FAIBTierRow* FoundRow = AIBTiers::Find(BotTier);
	if (!FoundRow)
	{
		UE_LOG(LogAIBot, Warning, TEXT("AIBot: %s asked for unknown tier '%s' — the defaults row drives (F7)."),
			*GetName(), *BotTier.ToString());
	}
	ResolvedTier = FoundRow ? *FoundRow : FAIBTierRow();
	for (const FString& Warning : AIBTiers::ValidateRow(BotTier, ResolvedTier))
	{
		UE_LOG(LogAIBot, Warning, TEXT("AIBot: tier row — %s"), *Warning);
	}

	// The envelope re-applied per life — the constructor seeded the defaults; the tier
	// is what a POSSESSION knows. ConfigureSense is the engine's own re-apply door
	// (the same call the constructor makes).
	if (SightConfig && HearingConfig && BotPerception)
	{
		SightConfig->SightRadius = ResolvedTier.SightRadius;
		SightConfig->LoseSightRadius = ResolvedTier.LoseSightRadius;
		SightConfig->PeripheralVisionAngleDegrees = ResolvedTier.PeripheralVisionAngleDegrees;
		SightConfig->SetMaxAge(ResolvedTier.SightMaxAgeSeconds);
		HearingConfig->HearingRange = ResolvedTier.HearingRange;
		HearingConfig->SetMaxAge(ResolvedTier.SightMaxAgeSeconds);
		BotPerception->ConfigureSense(*SightConfig);
		BotPerception->ConfigureSense(*HearingConfig);
	}

	// The grep-able tier line (proof 3): which vector this life runs.
	UE_LOG(LogAIBot, Log, TEXT("AIBot: %s resolved tier %s (Mv %d Aim %d Gr %d Me %d Cf %d Tw %d, react %.2f-%.2f)."),
		*GetName(), *BotTier.ToString(),
		static_cast<int32>(ResolvedTier.Movement), static_cast<int32>(ResolvedTier.Aim),
		static_cast<int32>(ResolvedTier.Grenade), static_cast<int32>(ResolvedTier.Melee),
		static_cast<int32>(ResolvedTier.Confidence), static_cast<int32>(ResolvedTier.Teamwork),
		ResolvedTier.ReactionSecondsMin, ResolvedTier.ReactionSecondsMax);

	// PHASE 13 (AIB24) — Detour Crowd owns steering during moves: separation on (the
	// crowd's default is off), weight from the row, medium avoidance, path offset so a
	// pair parts in a corridor. Query range 400, obstacle avoidance, RotateToVelocity and
	// AffectFallingVelocity=false stay at the component's defaults (F6: a fall is the
	// fall). NEVER the SetAvoidanceGroup family — that is the RVO path, and mixing the
	// two is forbidden. Players become crowd OBSTACLES game-side (ICrowdAgentInterface on
	// the player pawn, authority + player-controller gated); this module registers
	// nothing but its own follower.
	if (UCrowdFollowingComponent* Crowd = Cast<UCrowdFollowingComponent>(GetPathFollowingComponent()))
	{
		Crowd->SetCrowdSeparation(true);
		Crowd->SetCrowdSeparationWeight(ResolvedTier.CrowdSeparationWeight);
		Crowd->SetCrowdAvoidanceQuality(ECrowdAvoidanceQuality::Medium);
		Crowd->SetCrowdPathOffset(true);
	}

	const FAIBTierRow& Defaults = ResolvedTier;
	Sensorium.Reset();
	Sensorium.Configure(Defaults.ReactionSecondsMin, Defaults.ReactionSecondsMax);

	// PER-BOT and PER-LIFE seeding, hashed (W-REVIEW P4+5 H5/M4). Per-bot because shared
	// draws make four bots acquire in lockstep (F-3.7); per-LIFE because re-seeding the
	// same value every possession replayed a byte-identical sequence each respawn — the
	// same first jink, the same reaction latency, the loudest "not a person" tell there
	// is, reset by every death. Hashed because raw consecutive object indices feed an
	// LCG an evenly-spaced first-draw progression across the lobby, not independent
	// samples. Still fully deterministic given (bot, life): specs and replays keep what
	// they actually need.
	// Phase 14: (bot) is the manager's BotIndex, not GetUniqueID — object ids are not
	// stable across runs, so two -AIBSeed=N matches now replay the same draws.
	++LifeIndex;
	PossessedAtSeconds = GetWorld()->GetTimeSeconds();
	// Fix #4 R1: every PlayerStart is a confirmation anchor (the spawn alone was the false
	// one — Arena01's corner pads are islands themselves). Once per possession; the gate
	// projects them when it tests.
	PlayerStartLocations.Reset();
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		PlayerStartLocations.Add(It->GetActorLocation());
	}
	bHasLastFullPathGoal = false; // #3 H3: no full-path move has completed this life yet
	PendingMoveRequestId = 0;
	EgressMoveRequestId = 0;
	bStopOnLanding = false;
	Locomotion = FAIBLocomotionState(); // R3: one stall clock per body, born clean
	LastLinkJumpAtSeconds = -1.0;
	bNavSeen = false;                   // R7: nothing decides until the feet are on the mesh once
	bWaitingForNavLogged = false;
	LifeSeed = FAIBRouteBias::LifeSeed(MatchSeed, BotIndex, LifeIndex); // Phase 14: match x bot x life
	Sensorium.SetRandomSeed(static_cast<int32>(LifeSeed));

	// Phase 14: this life's lane taste, and the line two seeded runs must agree on.
	RouteBias.Draw(LifeSeed, ResolvedTier.RouteLaneWeightSpread);
	LastRouteSignature.Reset();
	UE_LOG(LogAIBot, Log, TEXT("AIBot: %s t=%.1f route bias — bot=%d life=%d seed=%u lanes=%s"),
		*GetName(), PossessedAtSeconds, BotIndex, LifeIndex, LifeSeed, *RouteBias.Describe());

	// Phase 5: a fresh life judges the fight from nothing. The profile is the same
	// defaults row until Phase 8 resolves the real tier; the misjudge stream is per-bot
	// and DISTINCT from the sensorium's (a redraw must not shift reaction latencies).
	SkillProfile.ResolveFrom(Defaults);
	DamageLedger.Reset();
	YawClaimedAtSeconds = -1.0;   // an absolute stamp must not cross into a new world
	ConfidenceState = FAIBConfidenceState();
	ConfidenceRandom.Initialize(static_cast<int32>(HashCombine(LifeSeed, 7919u)));

	// Phase 4 integration: fresh policy scratch per life — a new body must not inherit
	// the old one's aim error, melee clock, grenade cadence, or strafe leg.
	AimState = FAIBAimState();
	MeleeState = FAIBMeleeState();
	GrenadeState = FAIBGrenadeState();
	MovementState = FAIBMovementState();
	PolicyRandom.Initialize(static_cast<int32>(HashCombine(LifeSeed, 131u)));
	IdleSinceSeconds = -1.0; // a fresh body has stood still for nothing yet
	StillTactics = 0;
	IdleTactics = 0;
	OverlapEpisode.Reset();  // and has brushed nobody yet (Phase 13)
	LastPositionSampleSeconds = -1.0;
	SweepBudget.Reset();     // a fresh body has looked at nothing yet (AIB22)
	TravelPanDegrees = 0.f;
	IslandLatch.Reset();     // and stands on no island it has measured yet (cooldown stamp too)

	GetWorldTimerManager().SetTimer(ThinkTimer, this, &AAIBBotController::Think,
		FMath::Max(ThinkIntervalSeconds, 0.02f), /*bLoop=*/true);

	// The executor object. Swapping StateTree for Behavior Tree is this one NewObject
	// line — the rest of possession never changes (the IAIBExecutor seam). It STARTS from
	// Think, not here (fix #4 R7): the first Think whose pawn projects onto the navmesh
	// rescores and then starts the tree.
	if (!Executor)
	{
		UAIBStateTreeExecutor* NewExecutor = NewObject<UAIBStateTreeExecutor>(this);
		Executor = NewExecutor;
		ExecutorObject = NewExecutor;
	}
	bExecutorStarted = false;

	// Registry + ONE THINK BEFORE THE TREE STARTS. RefreshAmbitions clears, registers
	// core + the current mode's translated set, and Thinks once — the timer's first
	// fire is a whole interval away, but StartLogic selects a state IMMEDIATELY, and a
	// failed initial selection is TERMINAL (TreeRunStatus=Failed, Tick early-outs
	// forever — the engine's own error, hit live). Seeding here means the first
	// selection already mirrors arbitration. (The terminal's live diagnosis and the
	// W-REVIEW P3 barrier landed the Think-first fix independently, same day.) With the
	// feet already on the mesh at possession this Think also starts the executor.
	RefreshAmbitions();
}

void AAIBBotController::OnUnPossess()
{
	// The idle spell closes BEFORE the tree stops, while the state name is still readable.
	if (const UWorld* IdleWorld = GetWorld())
	{
		CloseIdleEpisode(IdleWorld->GetTimeSeconds());
		CloseOverlapEpisode(IdleWorld->GetTimeSeconds());
	}
	StillTactics = 0;
	// The executor first: no task may run one more evaluation against a dead body.
	if (Executor)
	{
		Executor->Stop();
	}
	bExecutorStarted = false;
	// THE BELT under the tree's brace (W-REVIEW P3): FireWhenAble's ExitState releases
	// on every exit the engine runs synchronously — but whether StopLogic exits states
	// synchronously is an engine fact this module must not bet a held trigger on. The
	// release is idempotent at the adapter, the avatar door is still valid HERE, and a
	// verb held on the persistent PlayerState outliving the body is the one leak F6
	// names by shape.
	// CORRECTED 1 Sep 2026 (aib-critic M1): this released Fire alone, under a comment
	// asserting Fire was the only held verb. That was true when Phase 3 wrote it and
	// false from Phase 4 on — Sprint and Aim are held too, and the crouch toggle is
	// rented. AIB::ReleaseHeldVerbs walks AIBTags::HeldVerbs() so the belt can no longer
	// go stale behind a comment.
	if (IAIBAvatarInterface* AvatarDoor = GetAvatar())
	{
		AIB::ReleaseHeldVerbs(*AvatarDoor);
	}
	// The host's proven guard: unpossession during world teardown has no timer manager.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ThinkTimer);
		// Phase 7: claims die with the body, immediately — a dead bot's claim would
		// shadow a slot from its teammates for the TTL's remainder (the stale-pawn
		// belt would catch it; this is the ordered path that does not wait).
		if (UAIBTeamCoordinator* Coordinator = World->GetSubsystem<UAIBTeamCoordinator>())
		{
			Coordinator->ReleaseAll(*this);
		}
	}
	Sensorium.Reset();
	LastFacts = FAIBFacts();
	// THE BRAIN DIES WITH THE BODY (W-REVIEW P2 M-1, two passes independently): the
	// registry survives (re-registering the same wants would be waste), but the
	// arbitration state — winner, commit clock, cliff baseline — must not carry a dead
	// life's commit into a fresh spawn, or an absolute-time CommitEnd into a new world.
	if (AmbitionEngine)
	{
		AmbitionEngine->ResetArbitration();
	}
	Avatar = nullptr;
	AvatarObject = nullptr;
	AmbitionProvider = nullptr;
	AmbitionProviderObject = nullptr;
	WorldQuery = nullptr;
	WorldQueryObject = nullptr;
	CachedModeAmbitions.Reset();
	LastLoggedTarget = nullptr;
	// Same reasoning as the arbitration reset above: an absolute-time gate must not carry
	// into a new world, where GetTimeSeconds starts over.
	NextGrenadeThrowTimeSeconds = 0.f;
	// Momentum and judgment die with the body too — a fresh spawn that still "feels"
	// its last death's beating would flee its first fight (absolute-time stamps included).
	DamageLedger.Reset();
	YawClaimedAtSeconds = -1.0;   // an absolute stamp must not cross into a new world
	SweepBudget.Reset();
	TravelPanDegrees = 0.f;
	IslandLatch.Reset();
	PlayerStartLocations.Reset();
	Locomotion = FAIBLocomotionState();
	bStopOnLanding = false;
	ConfidenceState = FAIBConfidenceState();
	AimState = FAIBAimState();
	MeleeState = FAIBMeleeState();
	GrenadeState = FAIBGrenadeState();
	MovementState = FAIBMovementState();
	Super::OnUnPossess();
}

void AAIBBotController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// World teardown does not promise OnUnPossess, and the component-shutdown order it
	// falls back to runs task ExitStates against half-destroyed objects. This is the
	// ordered path: stop the tree and release the held verb while the doors still stand.
	if (const UWorld* IdleWorld = GetWorld())
	{
		CloseIdleEpisode(IdleWorld->GetTimeSeconds());
		CloseOverlapEpisode(IdleWorld->GetTimeSeconds());
	}
	StillTactics = 0;
	if (Executor)
	{
		Executor->Stop();
	}
	if (IAIBAvatarInterface* AvatarDoor = GetAvatar())
	{
		// Same belt, same reason (aib-critic M1): every held verb, not just the trigger.
		AIB::ReleaseHeldVerbs(*AvatarDoor);
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ThinkTimer);
		if (UAIBTeamCoordinator* Coordinator = World->GetSubsystem<UAIBTeamCoordinator>())
		{
			Coordinator->ReleaseAll(*this);
		}
	}
	Avatar = nullptr;
	AvatarObject = nullptr;
	Super::EndPlay(EndPlayReason);
}

bool AAIBBotController::CanThrowGrenade() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimeSeconds() >= NextGrenadeThrowTimeSeconds;
}

void AAIBBotController::NoteGrenadeThrown(float CooldownSeconds)
{
	if (const UWorld* World = GetWorld())
	{
		NextGrenadeThrowTimeSeconds = World->GetTimeSeconds() + FMath::Max(0.f, CooldownSeconds);
	}
}

bool AAIBBotController::CanDash() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimeSeconds() >= NextDashTimeSeconds;
}

void AAIBBotController::NoteDashed(float CooldownSeconds)
{
	if (const UWorld* World = GetWorld())
	{
		NextDashTimeSeconds = World->GetTimeSeconds() + FMath::Max(0.f, CooldownSeconds);
	}
}

void AAIBBotController::NoteCurrentAmbitionFailed()
{
	const UWorld* World = GetWorld();
	if (!World || !AmbitionEngine)
	{
		return;
	}
	const FGameplayTag Failed = AmbitionEngine->GetCurrent();
	if (!Failed.IsValid())
	{
		return;
	}
	AmbitionEngine->NoteAmbitionFailed(Failed, World->GetTimeSeconds());
	UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s branch for %s failed — suppressing that want so another can run."),
		*GetName(), *Failed.ToString());
}

FName AAIBBotController::GetActiveStateName() const
{
	return Executor ? Executor->GetActiveStateName() : NAME_None;
}

void AAIBBotController::ForgetSearchMemory(const TCHAR* Why, float AfterSeconds)
{
	const FAIBTargetMemory& Memory = Sensorium.Memory();
	if (Memory.AgeSeconds(GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0) < 0.f)
	{
		return; // nothing remembered: nothing to abandon, nothing to log
	}
	// The lead's NAME, for the log only: the memory hides its actor by law (F2-A), so
	// this walks the same read-only belief view the debugger draws. No decision rides it.
	FString Who = TEXT("the lead");
	for (const FAIBTargetCandidate& Candidate : Sensorium.GetCandidates())
	{
		if (Candidate.Actor.IsValid() && Memory.Remembers(Candidate.Actor.Get()))
		{
			Who = Candidate.Actor->GetName();
			break;
		}
	}
	UE_LOG(LogAIBot, Log, TEXT("AIBot: %s t=%.1f search abandoned — forgot %s after %.1fs (%s)"),
		*GetName(), GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0, *Who, AfterSeconds, Why);
	Sensorium.ForgetMemory();
}

void AAIBBotController::CloseIdleEpisode(double NowSeconds)
{
	if (IdleSinceSeconds < 0.0)
	{
		return;
	}
	// One name per spell, most specific first — the harness keys on tactic=none.
	const TCHAR* Tactic = TEXT("none");
	if (IdleTactics & static_cast<uint8>(EAIBStillTactic::Reload))          { Tactic = TEXT("Reload"); }
	else if (IdleTactics & static_cast<uint8>(EAIBStillTactic::StrafeHold)) { Tactic = TEXT("StrafeHold"); }
	else if (IdleTactics & static_cast<uint8>(EAIBStillTactic::Hold))       { Tactic = TEXT("Hold"); }
	else if (IdleTactics & static_cast<uint8>(EAIBStillTactic::Sweep))      { Tactic = TEXT("Sweep"); }
	else if (IdleTactics & static_cast<uint8>(EAIBStillTactic::Stranded))   { Tactic = TEXT("Stranded"); }
	else if (IdleTactics & static_cast<uint8>(EAIBStillTactic::Yield))      { Tactic = TEXT("Yield"); }
	const FName State = GetActiveStateName();
	UE_LOG(LogAIBot, Log, TEXT("AIBot: %s t=%.1f idle over — %.1fs state=%s tactic=%s"),
		*GetName(), NowSeconds, NowSeconds - IdleSinceSeconds,
		State.IsNone() ? TEXT("?") : *State.ToString(), Tactic);
	IdleSinceSeconds = -1.0;
	IdleTactics = 0;
}

void AAIBBotController::CloseOverlapEpisode(double NowSeconds)
{
	float Seconds = 0.f;
	int32 Peak = 0;
	if (OverlapEpisode.Close(NowSeconds, Seconds, Peak) && Seconds >= AIB::TeammateOverlapReportSeconds)
	{
		UE_LOG(LogAIBot, Log, TEXT("AIBot: %s t=%.1f teammate overlap over — %.1fs, %d inside %.0fuu"),
			*GetName(), NowSeconds, Seconds, Peak, GetTierRow().TeammateYieldRadiusUU);
	}
}

FGameplayTag AAIBBotController::GetObjectiveKindForCurrentAmbition() const
{
	const FGameplayTag Current = AmbitionEngine ? AmbitionEngine->GetCurrent() : FGameplayTag();
	if (Current.IsValid())
	{
		for (const FAIBModeAmbition& Mode : CachedModeAmbitions)
		{
			if (Mode.AmbitionTag == Current)
			{
				return Mode.ObjectiveKind;
			}
		}
	}
	return FGameplayTag();
}

void AAIBBotController::RefreshAmbitions()
{
	if (!AmbitionEngine || !HasAuthority())
	{
		return;
	}

	// Clear + core + mode, then ONE Think — the empty-tag window closes before any tree
	// selection can see it, so a mid-match mode swap never sprays the Fallback Warning
	// the verifier counts (W-AUDIT P6 finding 10).
	AmbitionEngine->ClearAmbitions();

	TArray<FAIBAmbitionSpec> CoreAmbitions;
	UAIBAmbitionEngine::BuildDefaultCoreAmbitions(CoreAmbitions);
	for (const FAIBAmbitionSpec& Spec : CoreAmbitions)
	{
		AmbitionEngine->RegisterAmbition(Spec);
	}

	CachedModeAmbitions.Reset();
	if (IAIBAmbitionProvider* Provider = GetAmbitionProvider())
	{
		Provider->GetModeAmbitions(CachedModeAmbitions);
		for (const FAIBModeAmbition& Mode : CachedModeAmbitions)
		{
			// NEVER raw: the translation attaches the urgency consideration that makes
			// a fact-less mode want silent (the constant-that-camps defect).
			FAIBAmbitionSpec ModeSpec;
			UAIBAmbitionEngine::BuildModeAmbitionSpec(Mode, ModeSpec);
			AmbitionEngine->RegisterAmbition(ModeSpec);
		}
		UE_LOG(LogAIBot, Log, TEXT("AIBot: %s registered %d mode ambition(s) from the provider."),
			*GetName(), CachedModeAmbitions.Num());
	}

	Think();
}

void AAIBBotController::NoteDamageTaken(AActor* Attacker, const FVector& AttackerLocation, float FractionOfMaxHealth)
{
	UWorld* World = GetWorld();
	if (!World || !HasAuthority())
	{
		// A dropped note is a bot with no momentum and no confidence — never silently
		// (F-4.2, the blast seam's own rule, applied to its sibling — W-REVIEW P4+5 M5).
		UE_LOG(LogAIBot, Warning, TEXT("AIBot: %s dropped a damage-taken note (no world or no authority)."), *GetName());
		return;
	}
	const double Now = World->GetTimeSeconds();
	bDamageSeamSeen = true; // the host's seam exists: history is a real fact from here on
	DamageLedger.NoteTaken(FractionOfMaxHealth, Now);

	// WHO HIT ME becomes a stimulus, not a target: it rides the same reaction clock as
	// every sense and lands as MEMORY after maturing — being shot makes a bot go and
	// look, never lock on (the host's own ruling on this exact seam, kept 1:1). The
	// hostility filter matches the perception boundary's.
	//
	// AND IT IS A BEARING, NOT A POSITION (W-REVIEW P4+5 F-H3, closing the seam to the
	// FAIRPLAY amendment's own word: "the DIRECTION and identity of damage remain
	// sensorium-only"). The raw seam delivers the attacker's exact coordinates at any
	// range — a sniper at 3000uu behind cover would hand the bot a point it could not
	// have known, and Search would path to the perch. What a human reads off a hit is a
	// direction arc, so the remembered point is the attacker's BEARING from the bot,
	// capped at the sight envelope's own fade: inside it the point is real, beyond it
	// the bot goes and looks THAT WAY, not THERE.
	APawn* SelfPawn = GetPawn();
	if (Attacker && SelfPawn && Attacker != SelfPawn
		&& GetTeamAttitudeTowards(*Attacker) == ETeamAttitude::Hostile)
	{
		const FVector SelfLocation = SelfPawn->GetActorLocation();
		FVector ToAttacker = AttackerLocation - SelfLocation;
		const float DistanceUU = ToAttacker.Size();
		FVector RememberedPoint = AttackerLocation;
		if (DistanceUU > AIB::EngageFadeEndUU && DistanceUU > KINDA_SMALL_NUMBER)
		{
			RememberedPoint = SelfLocation + ToAttacker * (AIB::EngageFadeEndUU / DistanceUU);
		}
		Sensorium.NoteDamageFrom(Attacker, RememberedPoint, Now);
	}
}

void AAIBBotController::NoteDamageDealt(float FractionOfVictimMaxHealth)
{
	UWorld* World = GetWorld();
	if (!World || !HasAuthority())
	{
		UE_LOG(LogAIBot, Warning, TEXT("AIBot: %s dropped a damage-dealt note (no world or no authority)."), *GetName());
		return;
	}
	bDamageSeamSeen = true;
	DamageLedger.NoteDealt(FractionOfVictimMaxHealth, World->GetTimeSeconds());
}

void AAIBBotController::OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	UWorld* World = GetWorld();
	if (!World || !Actor || Actor == GetPawn() || !GetPawn())
	{
		return; // !GetPawn(): unpossessed bots must not accumulate a backlog (F-4.3)
	}

	// The hostility filter at the Note boundary: senses are FFA-open by design, but a
	// friendly must never become a stimulus — or a teammate's footstep evicts the
	// enemy from memory and the bot "acquires" its own side (F5-C and two handoffs).
	const ETeamAttitude::Type Attitude = GetTeamAttitudeTowards(*Actor);
	if (Attitude != ETeamAttitude::Hostile)
	{
		// THE ALLY-FIGHT TAP (AIB17), inside the drop and smaller than it: a FRIENDLY's
		// WEAPON noise (the host's tag door says which tags are weapons), heard by this
		// bot's own ears, becomes one place-and-stamp note — never a stimulus, never a
		// target, consumed only by the idle wander's destination draw. Teamwork-gated
		// at note time (the CanEvadeBlast precedent: a Novice never even receives).
		// Friendly ONLY — Neutral (scenery, a dead man's PlayerState-attributed blast)
		// stays fully dropped. FFA has no friendlies, so this is unreachable there by
		// construction. The log fires once per fight-heard spell, not per shot.
		if (Attitude == ETeamAttitude::Friendly
			&& Stimulus.Type == UAISense::GetSenseID<UAISense_Hearing>()
			&& SkillProfile.Level(EAIBSkill::Teamwork) >= EAIBCompetence::Trained)
		{
			if (IAIBWorldQuery* Query = GetWorldQuery(); Query && Query->IsWeaponNoiseTag(Stimulus.Tag))
			{
				const double HeardNow = World->GetTimeSeconds();
				const bool bAlreadyFresh = AllyFightMemory.IsFresh(HeardNow);
				AllyFightMemory.HeardAt = Stimulus.StimulusLocation;
				AllyFightMemory.HeardAtSeconds = HeardNow;
				if (!bAlreadyFresh)
				{
					UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s heard the team's fight — %.0fuu away."),
						*GetName(), GetPawn() ? FVector::Dist(GetPawn()->GetActorLocation(), Stimulus.StimulusLocation) : 0.f);
				}
			}
		}
		return;
	}

	const double Now = World->GetTimeSeconds();

	if (Stimulus.Type == UAISense::GetSenseID<UAISense_Hearing>())
	{
		Sensorium.NoteSound(Actor, Stimulus.StimulusLocation, Now);
		return;
	}

	// POSITIVE sense test: only sight feeds the visual channel. A sense added later
	// (damage, team comms) must not silently become vision (W-REVIEW F-4.5).
	if (Stimulus.Type != UAISense::GetSenseID<UAISense_Sight>())
	{
		UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s dropped a stimulus from an unmapped sense."), *GetName());
		return;
	}

	if (Stimulus.WasSuccessfullySensed())
	{
		Sensorium.NoteSighting(Actor, Stimulus.StimulusLocation, Now);
	}
	else
	{
		Sensorium.NoteSightingLost(Actor, Stimulus.StimulusLocation, Now);
	}
}

void AAIBBotController::OnPerceptionForgotten(AActor* Actor)
{
	// Engine perception aged the actor out with no loss event. Route it through the
	// clock as a loss so visibility cannot outlive perception — the no-op here was the
	// infinite-sight hole all three sibling review passes flagged.
	UWorld* World = GetWorld();
	if (World && Actor && Actor == Sensorium.GetVisibleTarget())
	{
		Sensorium.NoteForgotten(Actor, World->GetTimeSeconds());
	}
}

void AAIBBotController::NoteIncomingBlast(const FVector& Center, float Radius, double DetonateAtSeconds)
{
	UWorld* World = GetWorld();
	APawn* MyPawn = GetPawn();
	if (!World || !MyPawn || !HasAuthority())
	{
		// F7: a dropped warning is a bot that doesn't dodge — never silently (F-4.2).
		UE_LOG(LogAIBot, Warning, TEXT("AIBot: %s dropped a blast warning (no world/pawn/authority)."), *GetName());
		return;
	}

	// THE PERCEIVABILITY GATE (W-REVIEW P2 H3): a maturation delay makes an omniscient
	// fact LATE, not EARNED. The warning enters the clock only if the bot could have
	// perceived the grenade — line of sight from its eyes to the blast point, or the
	// point inside hearing range. A grenade thrown from behind an unseen corner is a
	// grenade this bot cannot dodge, which is F2 working, not failing.
	FVector EyesLocation;
	FRotator EyesRotation;
	MyPawn->GetActorEyesViewPoint(EyesLocation, EyesRotation);

	// The RESOLVED tier's ears, not a defaults row — the Phase-8 marker this replaced.
	const bool bInHearingRange = FVector::Dist(EyesLocation, Center) <= ResolvedTier.HearingRange;

	bool bHasLineOfSight = false;
	if (!bInHearingRange)
	{
		FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(AIBBlastPerceivability), /*bTraceComplex=*/false, MyPawn);
		FHitResult Hit;
		// The channel is the HOST's call (Config, see the header): what "blocks eyes"
		// means is collision-profile config this module cannot know. Default Visibility.
		bHasLineOfSight = !World->LineTraceSingleByChannel(
			Hit, EyesLocation, Center,
			static_cast<ECollisionChannel>(FMath::Clamp<int32>(BlastPerceivabilityChannel, 0, ECC_MAX - 1)),
			TraceParams);
	}

	if (!bInHearingRange && !bHasLineOfSight)
	{
		UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s could not perceive a blast warning — dropped (F2)."), *GetName());
		return;
	}

	// THE CAPABILITY GATE'S promised caller (P4+5 review L4 — authored, spec'd, wired
	// nowhere): a Novice grenade competence does not READ the grenade at its feet, so
	// the warning never enters its clock and no fact combination makes it dodge — the
	// design's sentence, finally true in the field. Giving less information is always
	// fair; the level is the answer, not a number.
	if (!FAIBGrenadePolicy::CanEvadeBlast(SkillProfile.Level(EAIBSkill::Grenade)))
	{
		UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s does not read the incoming blast — below the evade capability."), *GetName());
		return;
	}

	Sensorium.NoteIncomingBlast(Center, Radius, DetonateAtSeconds, World->GetTimeSeconds());
}

void AAIBBotController::ArmStopOnLanding()
{
	const FAIRequestID InFlight = GetCurrentMoveRequestID();
	bStopOnLanding = InFlight.IsValid();
	StopOnLandingRequestId = InFlight.GetID();
}

FPathFollowingRequestResult AAIBBotController::MoveTo(const FAIMoveRequest& MoveRequest, FNavPathSharedPtr* OutPath)
{
	// Dropped BEFORE Super: the request it replaces is aborted inside, and an
	// already-at-goal request completes inside — neither may claim this destination.
	PendingMoveRequestId = 0;
	const FPathFollowingRequestResult Result = Super::MoveTo(MoveRequest, OutPath);
	if (MoveRequest.IsUsingPathfinding() && Result.Code == EPathFollowingRequestResult::RequestSuccessful)
	{
		PendingMoveGoal = MoveRequest.GetDestination();
		PendingMoveRequestId = Result.MoveId.GetID();
		LogRouteIfChanged(PendingMoveGoal);
	}
	return Result;
}

void AAIBBotController::MarkEgressMove()
{
	EgressMoveRequestId = GetCurrentMoveRequestID().GetID();
	if (PendingMoveRequestId == EgressMoveRequestId)
	{
		PendingMoveRequestId = 0; // never the anchor either
	}
}

void AAIBBotController::LogRouteIfChanged(const FVector& Goal)
{
	// The follower holds the path the request just found (RequestMove is synchronous);
	// the corridor's polys, not the string-pulled points, are what cross a lane.
	const UPathFollowingComponent* Follow = GetPathFollowingComponent();
	const FNavPathSharedPtr Path = Follow ? Follow->GetPath() : nullptr;
	const FNavMeshPath* NavMeshPath = Path.IsValid() ? Path->CastPath<FNavMeshPath>() : nullptr;
	const ARecastNavMesh* NavMesh = NavMeshPath ? Cast<ARecastNavMesh>(NavMeshPath->GetNavigationDataUsed()) : nullptr;
	if (!NavMesh)
	{
		return;
	}
	FString Lanes;
	int32 LastLane = 0;
	for (const NavNodeRef Poly : NavMeshPath->PathCorridor)
	{
		const int32 Lane = AIBLanes::LaneIdOf(NavMesh->GetAreaClass(static_cast<int32>(NavMesh->GetPolyAreaID(Poly))));
		if (Lane != 0 && Lane != LastLane)
		{
			Lanes += FString::Printf(TEXT("%s%d"), Lanes.IsEmpty() ? TEXT("") : TEXT(">"), Lane);
			LastLane = Lane;
		}
	}
	if (Lanes.IsEmpty())
	{
		Lanes = TEXT("none");
	}
	if (Lanes == LastRouteSignature)
	{
		return; // the belief drifted, the route did not
	}
	LastRouteSignature = Lanes;
	const APawn* SelfPawn = GetPawn();
	UE_LOG(LogAIBot, Log, TEXT("AIBot: %s t=%.1f route — lanes=%s len=%.0fuu direct=%.0fuu goal=(%.0f,%.0f,%.0f)"),
		*GetName(), GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0, *Lanes,
		static_cast<double>(NavMeshPath->GetLength()),
		SelfPawn ? FVector::Dist(SelfPawn->GetActorLocation(), Goal) : 0.f,
		Goal.X, Goal.Y, Goal.Z);
}

void AAIBBotController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	Super::OnMoveCompleted(RequestID, Result);
	// Fix #4 R8: Egress's own moves prove nothing about the mainland — the lip walk is a
	// full path ON the island by construction, and its completion cleared the latch one
	// tick before the step-off (the PIE gantry watch).
	if (EgressMoveRequestId != 0 && RequestID.GetID() == EgressMoveRequestId)
	{
		EgressMoveRequestId = 0;
		return;
	}
	// DidMoveReachGoal is Success AND not a partial path, computed by the follower before
	// its Reset — a partial path's end is the island's edge, never proof of the mainland.
	const UPathFollowingComponent* Follow = GetPathFollowingComponent();
	if (!Follow || !Follow->DidMoveReachGoal())
	{
		return;
	}
	// W-REVIEW #3 H3: the goal of the last completed PATHED full-path move is the island
	// anchor — a bot on healthy ground refreshes it every move, a bot on an island never.
	if (PendingMoveRequestId != 0 && RequestID.GetID() == PendingMoveRequestId)
	{
		LastFullPathGoal = PendingMoveGoal;
		bHasLastFullPathGoal = true;
		PendingMoveRequestId = 0;
	}
	if (IslandLatch.bOnIsland || IslandLatch.bStranded)
	{
		IslandLatch.Clear();
		UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s island latch cleared — a full-path move completed."), *GetName());
	}
}

void AAIBBotController::GetIslandAnchors(TArray<FAIBIslandAnchor>& OutAnchors) const
{
	OutAnchors.Reset();
	// The CURRENT want's goal first — the anchor most likely to refute a floor bot in one
	// test. A mode want: the objective POIs of its kind (the mover's own pick set, the
	// world query's, all of them — a hill is one, a rally is a handful). Search/Seek: the
	// fresh last-known. Fight wants chase a body, not ground; Roam has no goal.
	const FGameplayTag Current = AmbitionEngine ? AmbitionEngine->GetCurrent() : FGameplayTag();
	const FGameplayTag Kind = GetObjectiveKindForCurrentAmbition();
	const APawn* SelfPawn = GetPawn();
	if (Kind.IsValid() && SelfPawn)
	{
		if (IAIBWorldQuery* Query = GetWorldQuery())
		{
			TArray<FAIBPointOfInterest> Points;
			Query->QueryPointsOfInterest(SelfPawn, AIB::ObjectiveQueryRadiusUU, Points);
			for (const FAIBPointOfInterest& Point : Points)
			{
				if (Point.Kind == Kind)
				{
					OutAnchors.Add({ Point.Location, TEXT("objective") });
				}
			}
		}
	}
	else if (Current == AIBTags::Ambition_Search || Current == AIBTags::Ambition_Seek)
	{
		const float Window = LastFacts.MemoryFreshWindowSeconds;
		FVector LastKnown;
		if (Sensorium.Memory().GetFresh(GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0,
			Window > 0.f ? Window : AIB::DefaultMemoryFreshSeconds, LastKnown))
		{
			OutAnchors.Add({ LastKnown, TEXT("last-known") });
		}
	}
	if (bHasLastFullPathGoal)
	{
		OutAnchors.Add({ LastFullPathGoal, TEXT("last full-path move") });
	}
	for (const FVector& Start : PlayerStartLocations)
	{
		OutAnchors.Add({ Start, TEXT("PlayerStart") });
	}
}

void AAIBBotController::Think()
{
	UWorld* World = GetWorld();
	if (!World || !HasAuthority())
	{
		return;
	}

	const double Now = World->GetTimeSeconds();

	// LAW F9's instrument (AIB22 `idle_seconds`), riding the think timer — no Tick. A
	// spell opens on the first ALIVE sample with no locomotion input and closes on the
	// first with some (or with the body: OnUnPossess/EndPlay). A corpse is not idle.
	{
		const APawn* SelfPawn = GetPawn();
		const IAIBAvatarInterface* Door = GetAvatar();
		const bool bStill = SelfPawn && Door && Door->IsAlive()
			&& SelfPawn->GetLastMovementInputVector().IsNearlyZero();
		// W-REVIEW #3 H2: Stranded mirrors the latch each sample — Egress sets it, the
		// cooldown or a completed full-path move drops it.
		SetStillTactic(EAIBStillTactic::Stranded, IslandLatch.IsStranded(Now));
		if (bStill && IdleSinceSeconds < 0.0)
		{
			IdleSinceSeconds = Now;
			IdleTactics = StillTactics;
		}
		else if (IdleSinceSeconds >= 0.0 && (!bStill || StillTactics != IdleTactics))
		{
			// W-REVIEW #3 H1: a spell is ONE tactic set. The set changing closes it — the
			// seconds went to the tactics that were up while they elapsed — and a still
			// body opens the next on the new set at once, so tactic=none resumes the
			// sample a tactic clears (one 2s Sweep no longer names a 280s stand).
			CloseIdleEpisode(Now);
			if (bStill)
			{
				IdleSinceSeconds = Now;
				IdleTactics = StillTactics;
			}
		}
		// The sweep budget refills on the BODY standing somewhere new (fix #4 R4): a still
		// sample 1.5x SweepRefillRadiusUU or farther from where the last refill stood.
		// Neither motion by itself (a wedged body pushing against a wall has input and no
		// displacement — that was the 7.0s sweep) nor the post moving refills anything.
		if (bStill && SelfPawn)
		{
			SweepBudget.ArriveAt(SelfPawn->GetActorLocation(), AIB::SweepRefillRadiusUU);
		}
		// W-REVIEW M6: the step-off request Egress left in flight, stopped on the first
		// grounded sample — unless something newer already owns the mover.
		if (bStopOnLanding && Door && Door->IsGrounded())
		{
			bStopOnLanding = false;
			if (GetCurrentMoveRequestID().GetID() == StopOnLandingRequestId)
			{
				StopMovement();
			}
		}

		// PHASE 13's two instruments, on the same sample. The overlap is an EPISODE (a
		// teammate inside the capsule sum, closed on the first sample without one — the
		// line only past the report threshold); the position sample is what the harness
		// pairs across a team for `mean_pairwise_teammate_distance` (bots know no team id;
		// the game mode's assignment line names it). Ally COUNTS only, through the
		// HUD-grade door — never a position, never an enemy.
		if (IAIBWorldQuery* Query = SelfPawn && Door && Door->IsAlive() ? GetWorldQuery() : nullptr)
		{
			const float OverlapRadiusUU = GetTierRow().TeammateYieldRadiusUU;
			const int32 Inside = Query->CountNearbyAllies(SelfPawn, OverlapRadiusUU);
			float Seconds = 0.f;
			int32 Peak = 0;
			if (OverlapEpisode.Note(Inside, Now, Seconds, Peak) && Seconds >= AIB::TeammateOverlapReportSeconds)
			{
				UE_LOG(LogAIBot, Log, TEXT("AIBot: %s t=%.1f teammate overlap over — %.1fs, %d inside %.0fuu"),
					*GetName(), Now, Seconds, Peak, OverlapRadiusUU);
			}
			if (LastPositionSampleSeconds < 0.0 || Now - LastPositionSampleSeconds >= AIB::PositionSampleSeconds)
			{
				LastPositionSampleSeconds = Now;
				const FVector Here = SelfPawn->GetActorLocation();
				UE_LOG(LogAIBot, Log, TEXT("AIBot: %s t=%.1f position (%.0f,%.0f,%.0f) allies within %.0fuu: %d"),
					*GetName(), Now, Here.X, Here.Y, Here.Z, OverlapRadiusUU, Inside);
			}
		}
	}

	// Told, never asked: the sensorium stays worldless, so the two things target
	// selection needs from outside are pushed in here, once, right before the pump that
	// uses them. Without the self location proximity simply does not score — the safe
	// direction, and what the headless specs run in.
	if (const APawn* SelfPawn = GetPawn())
	{
		Sensorium.SetSelfLocation(SelfPawn->GetActorLocation());
	}
	Sensorium.SetMemoryWindowSeconds(
		FMath::Min(GetTierRow().MemoryFreshSeconds, AIB::MaxMemorySeconds));
	Sensorium.Pump(Now);

	// The fairness instrument (aib-verifier samples this line): one log per acquisition,
	// carrying the HONEST happened->surfaced latency — pump quantisation included,
	// recorded for the acquisition itself, never an unrelated stimulus.
	AActor* Visible = Sensorium.GetVisibleTarget();
	if (Visible && LastLoggedTarget != Visible)
	{
		// A CHANGE of target is now a real decision rather than "whoever was seen last",
		// so the line says which it was and how many enemies were in the running. That
		// is what makes the founder's persistence rule checkable from a log instead of
		// from an impression: a bot flipping between two names every second is a
		// hysteresis failure, and it should be visible as one.
		const bool bSwitched = LastLoggedTarget.IsValid();
		UE_LOG(LogAIBot, Log, TEXT("AIBot: %s %s %s after %.3fs reaction (%d believed%s)."),
			*GetName(), bSwitched ? TEXT("SWITCHED to") : TEXT("acquired"),
			*Visible->GetName(), Sensorium.LastAcquisitionLatencySeconds(),
			Sensorium.GetCandidates().Num(),
			bSwitched ? *FString::Printf(TEXT(", was %s"), *LastLoggedTarget.Get()->GetName())
					  : TEXT(""));
		LastLoggedTarget = Visible;
	}
	else if (!Visible)
	{
		LastLoggedTarget = nullptr;
	}

	// FIX #4 R7 — NO DECISION BEFORE THE FEET ARE ON THE MESH. The spawn burst (t<2s,
	// the mesh not yet under a fresh pawn) was 95% of Spillway's refusals: every want
	// that won issued a move the follower refused. Perception above keeps pumping; the
	// arbitration below, the claims, and the tree itself wait for one successful
	// projection this life — polled here, at think cadence, never a tick.
	if (!bNavSeen)
	{
		const APawn* SelfPawn = GetPawn();
		UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		FNavLocation OnNav;
		if (!SelfPawn || !NavSys
			|| !NavSys->ProjectPointToNavigation(SelfPawn->GetActorLocation(), OnNav, FVector(300.f, 300.f, 400.f)))
		{
			if (!bWaitingForNavLogged)
			{
				bWaitingForNavLogged = true;
				UE_LOG(LogAIBot, Log, TEXT("AIBot: %s t=%.1f waiting for nav — no decisions until the pawn projects onto the mesh"),
					*GetName(), Now);
			}
			return;
		}
		bNavSeen = true;
		if (bWaitingForNavLogged)
		{
			UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s t=%.1f on nav after %.1fs"), *GetName(), Now, Now - PossessedAtSeconds);
		}
	}

	// Facts -> arbitration. The winner is the executor's gate at Phase 3; today the
	// switch LOG is the deliverable — the countable instrument for the verifier's
	// rung-3 protocol ("ambition switches with scores", proof 3).
	if (AmbitionEngine)
	{
		// Cached, not local: executor tasks read GetLastFacts() between pumps, so the
		// whole frame's execution sees the same matured snapshot (F3, one sample site).
		LastFacts = AIBFactsBuilder::Build(*this, Now);

		// Phase 5, in order: the ledger sources the damage-history facts, then the
		// model judges the fight FROM the finished facts, then the brain scores.
		// History is KNOWN only once the host's seam has ever spoken — a host that
		// never wires it must stay UNKNOWN, not "confidently untouched" while being
		// shot (unknown-is-a-state, applied to our own ledger).
		if (bDamageSeamSeen)
		{
			LastFacts.bDamageHistoryKnown = true;
			LastFacts.RecentDamageTakenNorm = DamageLedger.TakenNorm(Now);
			LastFacts.RecentDamageDealtNorm = DamageLedger.DealtNorm(Now);
		}
		LastFacts.ConfidenceNorm = FAIBConfidenceModel::Step(ConfidenceState, LastFacts,
			SkillProfile.Level(EAIBSkill::Confidence), ConfidenceRandom, Now);
		// KNOWN only when the judgment had at least one real input: a broken avatar door
		// left every term unknown, and publishing that assembly as a confident judgment
		// is the mirror of the unknowable-health-reads-as-full ban (W-REVIEW P4+5 M4).
		// The Nerve considerations' ValueWhenUnknown=1 then correctly mutes confidence.
		LastFacts.bConfidenceKnown = LastFacts.bVitalsKnown || LastFacts.bDamageHistoryKnown;

		const FGameplayTag Ambition = AmbitionEngine->Rescore(LastFacts, Now);
		if (Ambition.IsValid() && Ambition != LastLoggedAmbition)
		{
			LastLoggedAmbition = Ambition;
			// Winner, score, runner-up, and the cause class — enough for a log reader
			// to see hysteresis and interrupts working without a playtest (the
			// fairness pass's instrument-quality note).
			const FAIBScoredAmbition& RunnerUp = AmbitionEngine->GetLastRunnerUp();
			UE_LOG(LogAIBot, Log, TEXT("AIBot: %s ambition -> %s (%.2f) over %s (%.2f)%s"),
				*GetName(), *Ambition.ToString(), AmbitionEngine->GetCurrentScore(),
				RunnerUp.Tag.IsValid() ? *RunnerUp.Tag.ToString() : TEXT("none"),
				RunnerUp.Score,
				AmbitionEngine->WasLastRescoreInterrupted() ? TEXT(" [interrupt]") : TEXT(""));
		}

		// Phase 7: FILE THE CLAIM AT THINK-COMMIT, synchronously — think timers run
		// serialized on the game thread, so within one same-frame cluster the second
		// thinker's facts already see the first thinker's claim; claiming later, at
		// task enter, opens a multi-frame window in which the whole lobby commits to
		// one slot and then holds it on commit+hysteresis (W-AUDIT P7). The claim
		// renews while this want keeps winning; when it drifts away the claim lapses
		// by NON-renewal at TTL — a flapping claimant must not flap its teammates'
		// arbitration, so the board's transitions stay slower than the engine's
		// commit windows. Death releases immediately (the belts below).
		if (GetSkillProfile().Level(EAIBSkill::Teamwork) >= EAIBCompetence::Trained)
		{
			const FGameplayTag PursuedKind = GetObjectiveKindForCurrentAmbition();
			UAIBTeamCoordinator* Coordinator = World->GetSubsystem<UAIBTeamCoordinator>();
			IAIBWorldQuery* Query = GetWorldQuery();
			APawn* SelfPawn = GetPawn();
			if (PursuedKind.IsValid() && Coordinator && Query && SelfPawn)
			{
				TArray<FAIBPointOfInterest> Points;
				Query->QueryPointsOfInterest(SelfPawn, AIB::ObjectiveQueryRadiusUU, Points);
				// The task's own pick rule (best Worth among free slots), so the claim
				// and the walk always name the same slot.
				const FAIBPointOfInterest* Best = nullptr;
				for (const FAIBPointOfInterest& Point : Points)
				{
					if (Point.Kind == PursuedKind && Point.bClaimableSlot
						&& !Coordinator->IsClaimedByOtherTeammate(*this, Point)
						&& (!Best || Point.Worth > Best->Worth))
					{
						Best = &Point;
					}
				}
				if (Best)
				{
					Coordinator->TryClaim(*this, *Best, AIB::ClaimTtlSeconds);
				}
			}
		}
	}

	// The executor, ONCE, after the first on-nav rescore (R7 + the Think-first rule): the
	// tree's first selection already mirrors arbitration and the feet can path.
	if (!bExecutorStarted && Executor && GetPawn())
	{
		bExecutorStarted = true;
		Executor->Start(*this);
	}

	// Phase 8: the eyes-on instrument, drawn at think cadence so it breathes with the
	// brain. Config-gated; a shipping build strips the draw either way.
	if (bDebugOverlay)
	{
		AIBDebug::DrawBotOverlay(*this);
	}
}
