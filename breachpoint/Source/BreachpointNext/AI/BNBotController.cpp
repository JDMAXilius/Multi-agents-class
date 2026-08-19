#include "AI/BNBotController.h"

#include "AbilitySystem/BNAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/BNAttributeSet.h"
#include "AI/BNBotBrain.h"
#include "BreachpointNext.h"
#include "Characters/BNCharacter.h"
#include "Core/BNGameplayTags.h"
#include "Data/BNGameData.h"
#include "Match/BNPlayerState.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Components/StateTreeAIComponent.h"
#include "StateTree.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GameplayEffectTypes.h"
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

	// Fresh mind per life: the brain carries only ambition + commit window, both of which a
	// respawn should reset. Rescoring is EVENT-driven from here on — never a tick.
	Brain = NewObject<UBNBotBrain>(this);
	if (UBNAbilitySystemComponent* ASC = GetBotASC())
	{
		BrainEventASC = ASC;
		if (!HealthChangedHandle.IsValid())
		{
			HealthChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(UBNAttributeSet::GetHealthAttribute())
				.AddUObject(this, &ABNBotController::OnHealthChanged);
		}
		LastRecentDamageCount = ASC->GetTagCount(BNTags::State_Combat_RecentDamage);
		if (!RecentDamageHandle.IsValid())
		{
			RecentDamageHandle = ASC->RegisterGameplayTagEvent(BNTags::State_Combat_RecentDamage, EGameplayTagEventType::AnyCountChange)
				.AddUObject(this, &ABNBotController::OnRecentDamageTagChanged);
		}
	}
	RescoreBrain();

	// The tree arrives by ini soft path, set BEFORE logic starts — no Blueprint child exists to
	// hold the reference, and that is deliberate (C++-first). Idempotent across respawns: the
	// same tree on the same component is a no-op re-set.
	if (StateTreeAI)
	{
		if (UStateTree* Tree = BotStateTree.LoadSynchronous())
		{
			StateTreeAI->SetStateTree(Tree);
		}
		else if (!BotStateTree.IsNull())
		{
			UE_LOG(LogBN, Warning, TEXT("BNBotController: BotStateTree '%s' failed to load — bots will stand still. Check the [/Script/BreachpointNext.BNBotController] ini path against the asset."),
				*BotStateTree.ToString());
		}
		else
		{
			UE_LOG(LogBN, Warning, TEXT("BNBotController: BotStateTree is unset — bots will stand still until TASK-R5-ST-BNBOT lands the tree and the ini names it."));
		}
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

	// Unregister on the SAME ASC the handles were taken from (ABNCharacter's EndPlay discipline),
	// then drop the brain — that also mutes the rescore ClearCurrentTarget would fire mid-teardown.
	if (UBNAbilitySystemComponent* ASC = BrainEventASC.Get())
	{
		if (HealthChangedHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(UBNAttributeSet::GetHealthAttribute())
				.Remove(HealthChangedHandle);
		}
		if (RecentDamageHandle.IsValid())
		{
			ASC->UnregisterGameplayTagEvent(RecentDamageHandle, BNTags::State_Combat_RecentDamage, EGameplayTagEventType::AnyCountChange);
		}
	}
	HealthChangedHandle.Reset();
	RecentDamageHandle.Reset();
	BrainEventASC.Reset();
	Brain = nullptr;

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
	// Survive obedience (R6 G2 2.2): report no target so Engage exits by its own condition; the
	// enemy is still held as the threat — see GetThreat.
	if (Brain && Brain->GetAmbition() == EBNBotAmbition::Survive)
	{
		return nullptr;
	}
	return GetThreat();
}

AActor* ABNBotController::GetThreat() const
{
	AActor* Target = TargetEnemy.Get();
	return IsValidTarget(Target) ? Target : nullptr;
}

EBNBotAmbition ABNBotController::GetAmbition() const
{
	return Brain ? Brain->GetAmbition() : EBNBotAmbition::Roam;
}

void ABNBotController::SetCurrentTarget(AActor* Target)
{
	if (TargetEnemy.Get() == Target)
	{
		return;
	}
	TargetEnemy = Target;
	RescoreBrain();
}

void ABNBotController::ClearCurrentTarget()
{
	const bool bHadTarget = TargetEnemy.IsValid();
	TargetEnemy.Reset();
	if (bHadTarget)
	{
		RescoreBrain();
	}
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
	// GetThreat, not GetCurrentTarget: the slot rule reads the raw slot, or Survive (which hides
	// the target) would let every new sighting overwrite the very threat being fled.
	if (!GetThreat() && IsValidTarget(Actor))
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

void ABNBotController::RescoreBrain()
{
	const UWorld* World = GetWorld();
	if (!Brain || !World)
	{
		return;
	}

	// The facts are distilled HERE — the brain never touches an actor, a world, or a clock.
	AActor* Threat = GetThreat();
	FBNBotFacts Facts;
	Facts.bHasTarget = Threat != nullptr;
	if (const UBNAbilitySystemComponent* ASC = GetBotASC())
	{
		const float MaxHealth = ASC->GetNumericAttribute(UBNAttributeSet::GetMaxHealthAttribute());
		if (MaxHealth > 0.f)
		{
			Facts.HealthNorm = FMath::Clamp(ASC->GetNumericAttribute(UBNAttributeSet::GetHealthAttribute()) / MaxHealth, 0.f, 1.f);
		}
	}
	const APawn* MyPawn = GetPawn();
	if (Threat && MyPawn && SightRadius > 0.f)
	{
		Facts.DistToTargetNorm = FMath::Clamp(FVector::Dist(MyPawn->GetActorLocation(), Threat->GetActorLocation()) / SightRadius, 0.f, 1.f);
	}

	const UGameInstance* GI = GetGameInstance();
	const UBNGameData* Data = GI ? GI->GetSubsystem<UBNGameData>() : nullptr;
	auto ResolveRow = [Data](EBNBotAmbition Ambition) -> FBNBotAmbitionRow
	{
		const FBNBotAmbitionRow* Row = Data ? Data->FindBotAmbitionRow(Ambition) : nullptr;
		return Row ? *Row : UBNBotBrain::DefaultRow(Ambition);
	};

	if (Brain->Rescore(Facts, ResolveRow(EBNBotAmbition::Fight), ResolveRow(EBNBotAmbition::Survive),
			ResolveRow(EBNBotAmbition::Roam), World->GetTimeSeconds()))
	{
		// The mind-reading line (§5c): once per ambition CHANGE, never per rescore.
		const APlayerState* PS = PlayerState;
		UE_LOG(LogBN, Log, TEXT("BNBrain: %s wants %s (u=%.2f) because %s."),
			PS ? *PS->GetPlayerName() : *GetName(),
			*UBNBotBrain::AmbitionRowName(Brain->GetAmbition()).ToString(),
			Brain->GetUtility(),
			*Brain->GetWinningConsideration());
	}
}

void ABNBotController::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	if (Data.NewValue != Data.OldValue)
	{
		RescoreBrain();
	}
}

void ABNBotController::OnRecentDamageTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	// Count INCREASE only (BNGA_ADS's proven baseline pattern): a decrease is a damage window
	// expiring, and rescoring on danger receding would flee at exactly the wrong moment.
	const bool bDamaged = NewCount > LastRecentDamageCount;
	LastRecentDamageCount = NewCount;
	if (bDamaged)
	{
		RescoreBrain();
	}
}
