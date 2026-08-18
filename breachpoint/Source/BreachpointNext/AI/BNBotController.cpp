#include "AI/BNBotController.h"

#include "AbilitySystem/BNAbilitySystemComponent.h"
#include "BreachpointNext.h"
#include "Characters/BNCharacter.h"
#include "Core/BNGameplayTags.h"
#include "Match/BNPlayerState.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Components/StateTreeAIComponent.h"
#include "GameFramework/Pawn.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"

ABNBotController::ABNBotController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// The WHOLE reuse chain hangs on this line: a real ABNPlayerState is the ASC, the abilities,
	// the weapons and the score. Without it the bot is a pawn nothing in BN recognises.
	bWantsPlayerState = true;

	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	StateTreeAI = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("StateTreeAI"));
	StateTreeAI->SetStartLogicAutomatically(false);

	BotPerception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("BotPerception"));
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("BotSight"));

	SightConfig->SightRadius = SightRadius;
	SightConfig->LoseSightRadius = LoseSightRadius;
	SightConfig->PeripheralVisionAngleDegrees = PeripheralVisionAngleDegrees;

	// FFA: the sight sense detects EVERYTHING — the target rule below is what decides hostility,
	// not the affiliation filter, so teams can land later without touching perception.
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;

	BotPerception->ConfigureSense(*SightConfig);
	BotPerception->SetDominantSense(SightConfig->GetSenseImplementation());
	SetPerceptionComponent(*BotPerception);

	BotPerception->OnTargetPerceptionUpdated.AddDynamic(this, &ABNBotController::OnPerceptionUpdated);
	BotPerception->OnTargetPerceptionForgotten.AddDynamic(this, &ABNBotController::OnPerceptionForgotten);

	SetGenericTeamId(FGenericTeamId(255));
}

FGenericTeamId ABNBotController::GetGenericTeamId() const
{
	// Teams-later seam: FFA is one shared "no team". A team system replaces this constant.
	return FGenericTeamId(255);
}

ETeamAttitude::Type ABNBotController::GetTeamAttitudeTowards(const AActor& Other) const
{
	// Teams-later seam: FFA answers Hostile for any pawn that is not mine. Non-pawns are scenery.
	if (const APawn* OtherPawn = Cast<APawn>(&Other))
	{
		return OtherPawn == GetPawn() ? ETeamAttitude::Friendly : ETeamAttitude::Hostile;
	}
	return ETeamAttitude::Neutral;
}

void ABNBotController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// Config values land AFTER the constructor, so the ctor's numbers were the C++ defaults.
	// Re-applied here so ini tuning reaches the sense; the listener update makes it take.
	if (SightConfig && BotPerception)
	{
		SightConfig->SightRadius = SightRadius;
		SightConfig->LoseSightRadius = LoseSightRadius;
		SightConfig->PeripheralVisionAngleDegrees = PeripheralVisionAngleDegrees;
		BotPerception->ConfigureSense(*SightConfig);
		BotPerception->RequestStimuliListenerUpdate();
	}

	// Fresh logic per body: OnUnPossess stopped it, so this is a clean start on the new pawn.
	if (StateTreeAI)
	{
		StateTreeAI->StartLogic();
	}
}

void ABNBotController::OnUnPossess()
{
	APawn* OldPawn = GetPawn();

	if (StateTreeAI)
	{
		StateTreeAI->StopLogic(TEXT("Unpossessed"));
	}

	ClearCurrentTarget();

	// Lyra's pattern: the ASC is the PERSISTENT PlayerState's, and a stale avatar left on it
	// breaks the respawned bot — clear it if this dying pawn is still the avatar.
	if (UBNAbilitySystemComponent* ASC = GetBotASC())
	{
		if (OldPawn && ASC->GetAvatarActor() == OldPawn)
		{
			ASC->SetAvatarActor(nullptr);
		}
	}

	Super::OnUnPossess();
}

void ABNBotController::PressInputTag(FGameplayTag InputTag)
{
	if (!HasAuthority())
	{
		return;
	}

	if (UBNAbilitySystemComponent* ASC = GetBotASC())
	{
		ASC->AbilityInputTagPressed(InputTag);
	}
}

void ABNBotController::ReleaseInputTag(FGameplayTag InputTag)
{
	if (!HasAuthority())
	{
		return;
	}

	if (UBNAbilitySystemComponent* ASC = GetBotASC())
	{
		ASC->AbilityInputTagReleased(InputTag);
	}
}

AActor* ABNBotController::GetCurrentTarget() const
{
	AActor* Target = TargetEnemy.Get();
	return IsValidTarget(Target) ? Target : nullptr;
}

void ABNBotController::SetCurrentTarget(AActor* Target)
{
	TargetEnemy = Target;
}

void ABNBotController::ClearCurrentTarget()
{
	TargetEnemy.Reset();
}

void ABNBotController::OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!Stimulus.WasSuccessfullySensed())
	{
		if (TargetEnemy.Get() == Actor)
		{
			ClearCurrentTarget();
		}
		return;
	}

	// Keep a live target once held; the slot refills the moment the current one dies or is lost.
	if (!GetCurrentTarget() && IsValidTarget(Actor))
	{
		SetCurrentTarget(Actor);
	}
}

void ABNBotController::OnPerceptionForgotten(AActor* Actor)
{
	if (TargetEnemy.Get() == Actor)
	{
		ClearCurrentTarget();
	}
}

UBNAbilitySystemComponent* ABNBotController::GetBotASC() const
{
	const ABNPlayerState* PS = GetPlayerState<ABNPlayerState>();
	return PS ? PS->GetBNAbilitySystemComponent() : nullptr;
}

bool ABNBotController::IsValidTarget(AActor* Actor) const
{
	ABNCharacter* Character = Cast<ABNCharacter>(Actor);
	if (!Character || Character == GetPawn())
	{
		return false;
	}

	// Alive means the ASC says so — no ASC yet means not a target yet, never a guess.
	const UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Character);
	return ASC && !ASC->HasMatchingGameplayTag(BNTags::State_Dead);
}
