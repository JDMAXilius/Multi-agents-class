#include "Core/AIBBotController.h"

#include "AIBotModule.h"
#include "Brain/AIBAmbitionEngine.h"
#include "Components/ActorComponent.h"
#include "Components/StateTreeAIComponent.h"
#include "Core/AIBBotManager.h"
#include "Core/AIBFactsBuilder.h"
#include "Core/AIBTags.h"
#include "Data/AIBDataRows.h"
#include "Execution/AIBStateTreeExecutor.h"
#include "Interfaces/AIBAvatarInterface.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISense_Hearing.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"
#include "Team/AIBTeamCoordinator.h"
#include "TimerManager.h"

AAIBBotController::AAIBBotController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
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

	// Delegate binding is NOT here: the shipping host binds at possession, gated and
	// deduped, precisely to keep bindings out of serialized CDO state (W-REVIEW F-4.1).

	SetGenericTeamId(FGenericTeamId(255));
}

ETeamAttitude::Type AAIBBotController::GetTeamAttitudeTowards(const AActor& Other) const
{
	// FFA: every pawn that is not mine is hostile; scenery is neutral. A team system
	// answers through IAIBWorldQuery::AreEnemies and replaces this constant.
	if (const APawn* OtherPawn = Cast<APawn>(&Other))
	{
		return OtherPawn == GetPawn() ? ETeamAttitude::Friendly : ETeamAttitude::Hostile;
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
	if (UAIBBotManager* Manager = GetWorld() ? GetWorld()->GetSubsystem<UAIBBotManager>() : nullptr)
	{
		AmbitionProvider = Manager->GetAmbitionProvider();
		AmbitionProviderObject = Manager->GetAmbitionProviderObject();
		WorldQuery = Manager->GetWorldQuery();
		WorldQueryObject = Manager->GetWorldQueryObject();
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

	// Phase 8 resolves the real tier; until then the row's defaults are the envelope.
	const FAIBTierRow Defaults;
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
	++LifeIndex;
	const uint32 LifeSeed = HashCombine(GetTypeHash(GetUniqueID()), GetTypeHash(LifeIndex));
	Sensorium.SetRandomSeed(static_cast<int32>(LifeSeed));

	// Phase 5: a fresh life judges the fight from nothing. The profile is the same
	// defaults row until Phase 8 resolves the real tier; the misjudge stream is per-bot
	// and DISTINCT from the sensorium's (a redraw must not shift reaction latencies).
	SkillProfile.ResolveFrom(Defaults);
	DamageLedger.Reset();
	ConfidenceState = FAIBConfidenceState();
	ConfidenceRandom.Initialize(static_cast<int32>(HashCombine(LifeSeed, 7919u)));

	// Phase 4 integration: fresh policy scratch per life — a new body must not inherit
	// the old one's aim error, melee clock, grenade cadence, or strafe leg.
	AimState = FAIBAimState();
	MeleeState = FAIBMeleeState();
	GrenadeState = FAIBGrenadeState();
	MovementState = FAIBMovementState();
	PolicyRandom.Initialize(static_cast<int32>(HashCombine(LifeSeed, 131u)));

	GetWorldTimerManager().SetTimer(ThinkTimer, this, &AAIBBotController::Think,
		FMath::Max(ThinkIntervalSeconds, 0.02f), /*bLoop=*/true);

	// Registry + ONE THINK BEFORE THE TREE STARTS. RefreshAmbitions clears, registers
	// core + the current mode's translated set, and Thinks once — the timer's first
	// fire is a whole interval away, but StartLogic selects a state IMMEDIATELY, and a
	// failed initial selection is TERMINAL (TreeRunStatus=Failed, Tick early-outs
	// forever — the engine's own error, hit live). Seeding here means the first
	// selection already mirrors arbitration. (The terminal's live diagnosis and the
	// W-REVIEW P3 barrier landed the Think-first fix independently, same day.)
	RefreshAmbitions();

	// The executor last: by the time the tree evaluates its first gate, the brain and
	// sensorium above are already live. Swapping StateTree for Behavior Tree is this one
	// NewObject line — the rest of possession never changes (the IAIBExecutor seam).
	if (!Executor)
	{
		UAIBStateTreeExecutor* NewExecutor = NewObject<UAIBStateTreeExecutor>(this);
		Executor = NewExecutor;
		ExecutorObject = NewExecutor;
	}
	Executor->Start(*this);
}

void AAIBBotController::OnUnPossess()
{
	// The executor first: no task may run one more evaluation against a dead body.
	if (Executor)
	{
		Executor->Stop();
	}
	// THE BELT under the tree's brace (W-REVIEW P3): FireWhenAble's ExitState releases
	// on every exit the engine runs synchronously — but whether StopLogic exits states
	// synchronously is an engine fact this module must not bet a held trigger on. The
	// release is idempotent at the adapter, the avatar door is still valid HERE, and a
	// verb held on the persistent PlayerState outliving the body is the one leak F6
	// names by shape. Fire is the only HELD verb Phase 3 authors.
	if (IAIBAvatarInterface* AvatarDoor = GetAvatar())
	{
		AvatarDoor->ReleaseVerb(AIBTags::Verb_Fire);
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
	if (Executor)
	{
		Executor->Stop();
	}
	if (IAIBAvatarInterface* AvatarDoor = GetAvatar())
	{
		AvatarDoor->ReleaseVerb(AIBTags::Verb_Fire);
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
	if (GetTeamAttitudeTowards(*Actor) != ETeamAttitude::Hostile)
	{
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

	static const FAIBTierRow Defaults; // Phase 8 resolves the real tier
	const bool bInHearingRange = FVector::Dist(EyesLocation, Center) <= Defaults.HearingRange;

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

	Sensorium.NoteIncomingBlast(Center, Radius, DetonateAtSeconds, World->GetTimeSeconds());
}

void AAIBBotController::Think()
{
	UWorld* World = GetWorld();
	if (!World || !HasAuthority())
	{
		return;
	}

	const double Now = World->GetTimeSeconds();
	Sensorium.Pump(Now);

	// The fairness instrument (aib-verifier samples this line): one log per acquisition,
	// carrying the HONEST happened->surfaced latency — pump quantisation included,
	// recorded for the acquisition itself, never an unrelated stimulus.
	AActor* Visible = Sensorium.GetVisibleTarget();
	if (Visible && LastLoggedTarget != Visible)
	{
		LastLoggedTarget = Visible;
		UE_LOG(LogAIBot, Log, TEXT("AIBot: %s acquired %s after %.3fs reaction."),
			*GetName(), *Visible->GetName(), Sensorium.LastAcquisitionLatencySeconds());
	}
	else if (!Visible)
	{
		LastLoggedTarget = nullptr;
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
}
