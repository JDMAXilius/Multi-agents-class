#include "Core/AIBBotController.h"

#include "AIBotModule.h"
#include "Components/ActorComponent.h"
#include "Data/AIBDataRows.h"
#include "Interfaces/AIBAvatarInterface.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISense_Hearing.h"
#include "Perception/AISenseConfig_Sight.h"
#include "TimerManager.h"

AAIBBotController::AAIBBotController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// A real PlayerState: the bot joins the same match machinery a human does.
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

	// FFA-open senses: detection is not hostility. Hostility lives in the attitude
	// override, so teams later change ONE function and never touch perception.
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;

	HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("BotHearing"));
	HearingConfig->HearingRange = Defaults.HearingRange;
	HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
	HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
	HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;

	// Sight dominant: seeing and hearing the same actor, the sighting wins.
	BotPerception->ConfigureSense(*SightConfig);
	BotPerception->ConfigureSense(*HearingConfig);
	BotPerception->SetDominantSense(SightConfig->GetSenseImplementation());
	SetPerceptionComponent(*BotPerception);

	BotPerception->OnTargetPerceptionUpdated.AddDynamic(this, &AAIBBotController::OnPerceptionUpdated);
	BotPerception->OnTargetPerceptionForgotten.AddDynamic(this, &AAIBBotController::OnPerceptionForgotten);

	SetGenericTeamId(FGenericTeamId(255));
}

ETeamAttitude::Type AAIBBotController::GetTeamAttitudeTowards(const AActor& Other) const
{
	// FFA: every pawn that is not mine is hostile; scenery is neutral. Teams replace this.
	if (const APawn* OtherPawn = Cast<APawn>(&Other))
	{
		return OtherPawn == GetPawn() ? ETeamAttitude::Friendly : ETeamAttitude::Hostile;
	}
	return ETeamAttitude::Neutral;
}

void AAIBBotController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	Avatar = nullptr;
	AvatarObject = nullptr;
	LastLoggedTarget = nullptr;

	if (!InPawn)
	{
		return;
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

	// Phase 8 resolves the real tier; until then the row's defaults are the envelope.
	const FAIBTierRow Defaults;
	Sensorium.Reset();
	Sensorium.Configure(Defaults.ReactionSecondsMin, Defaults.ReactionSecondsMax);

	GetWorldTimerManager().SetTimer(ThinkTimer, this, &AAIBBotController::Think,
		FMath::Max(ThinkIntervalSeconds, 0.02f), /*bLoop=*/true);
}

void AAIBBotController::OnUnPossess()
{
	GetWorldTimerManager().ClearTimer(ThinkTimer);
	Sensorium.Reset();
	Avatar = nullptr;
	AvatarObject = nullptr;
	LastLoggedTarget = nullptr;
	Super::OnUnPossess();
}

void AAIBBotController::OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!Actor || Actor == GetPawn())
	{
		return;
	}

	const double Now = GetWorld()->GetTimeSeconds();

	if (Stimulus.Type == UAISense::GetSenseID<UAISense_Hearing>())
	{
		// Ears place, they do not see (F2): a sound becomes memory when it matures.
		Sensorium.NoteSound(Actor, Stimulus.StimulusLocation, Now);
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
	// Perception giving up entirely mirrors a matured loss at the actor's last stimulus
	// location being unavailable — the sensorium already holds the last known spot, so
	// nothing to note; the memory decays on its own clock (F5).
}

void AAIBBotController::NoteIncomingBlast(const FVector& Center, float Radius, double DetonateAtSeconds)
{
	if (UWorld* World = GetWorld())
	{
		Sensorium.NoteIncomingBlast(Center, Radius, DetonateAtSeconds, World->GetTimeSeconds());
	}
}

void AAIBBotController::Think()
{
	const double Now = GetWorld()->GetTimeSeconds();
	Sensorium.Pump(Now);

	// The fairness instrument (aib-verifier samples this line): one log per acquisition,
	// carrying the exact stimulus-to-knowledge latency the F1 floor bounds.
	AActor* Visible = Sensorium.GetVisibleTarget();
	if (Visible && LastLoggedTarget != Visible)
	{
		LastLoggedTarget = Visible;
		UE_LOG(LogAIBot, Log, TEXT("AIBot: %s acquired %s after %.3fs reaction."),
			*GetName(), *Visible->GetName(), Sensorium.LastMaturedLatencySeconds());
	}
	else if (!Visible)
	{
		LastLoggedTarget = nullptr;
	}

	// Phase 2 adds: facts build -> brain rescore -> executor gate.
}
