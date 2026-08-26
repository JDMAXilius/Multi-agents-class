#include "Core/AIBBotController.h"

#include "AIBotModule.h"
#include "Brain/AIBAmbitionEngine.h"
#include "Components/ActorComponent.h"
#include "Core/AIBFactsBuilder.h"
#include "Data/AIBDataRows.h"
#include "Interfaces/AIBAvatarInterface.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISense_Hearing.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"
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

	// The brain: core wants registered from C++ defaults; a mode adds its own at
	// Phase 6 through IAIBAmbitionProvider, into this same engine.
	if (!AmbitionEngine)
	{
		AmbitionEngine = NewObject<UAIBAmbitionEngine>(this);
		TArray<FAIBAmbitionSpec> CoreAmbitions;
		UAIBAmbitionEngine::BuildDefaultCoreAmbitions(CoreAmbitions);
		for (const FAIBAmbitionSpec& Spec : CoreAmbitions)
		{
			AmbitionEngine->RegisterAmbition(Spec);
		}
	}
	LastLoggedAmbition = FGameplayTag();

	// Phase 8 resolves the real tier; until then the row's defaults are the envelope.
	const FAIBTierRow Defaults;
	Sensorium.Reset();
	Sensorium.Configure(Defaults.ReactionSecondsMin, Defaults.ReactionSecondsMax);
	// Per-bot latency sequence: shared draws make four bots acquire in lockstep, which
	// reads as coordinated omniscience (W-REVIEW F-3.7).
	Sensorium.SetRandomSeed(static_cast<int32>(GetUniqueID()));

	GetWorldTimerManager().SetTimer(ThinkTimer, this, &AAIBBotController::Think,
		FMath::Max(ThinkIntervalSeconds, 0.02f), /*bLoop=*/true);
}

void AAIBBotController::OnUnPossess()
{
	// The host's proven guard: unpossession during world teardown has no timer manager.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ThinkTimer);
	}
	Sensorium.Reset();
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
	LastLoggedTarget = nullptr;
	Super::OnUnPossess();
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
		bHasLineOfSight = !World->LineTraceSingleByChannel(
			Hit, EyesLocation, Center, ECC_Visibility, TraceParams);
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
		const FAIBFacts Facts = AIBFactsBuilder::Build(*this, Now);
		const FGameplayTag Ambition = AmbitionEngine->Rescore(Facts, Now);
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
	}
}
