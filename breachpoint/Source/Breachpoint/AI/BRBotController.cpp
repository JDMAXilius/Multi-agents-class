#include "AI/BRBotController.h"

#include "AI/BREnvQueryContexts.h"
#include "AI/BRStateTreeTasks.h"
#include "AbilitySystem/BRAbilitySystemComponent.h"
#include "AbilitySystem/BRAttributeSet.h"
#include "Components/StateTreeAIComponent.h"
#include "Core/BRGameplayTags.h"
#include "Engine/AssetManager.h"
#include "Engine/World.h"
#include "EnvironmentQuery/EnvQueryManager.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Navigation/PathFollowingComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AISenseConfig_Sight.h"
#include "StateTree.h"
#include "TimerManager.h"

namespace
{
	constexpr float CmPerMetre = 100.f;
	constexpr float MsPerSecond = 1000.f;
}

ABRBotController::ABRBotController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bWantsPlayerState = true;

	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	StateTreeComponent = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("BotStateTree"));

	BotPerception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("BotPerception"));
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("BotSight"));

	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = false;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = false;

	BotPerception->ConfigureSense(*SightConfig);
	BotPerception->SetDominantSense(SightConfig->GetSenseImplementation());
	SetPerceptionComponent(*BotPerception);
}

void ABRBotController::ConfigureBot(int32 InSeed, const FBRBotTierScalars& InScalars, const TArray<FBRBotAmbitionDef>& InAmbitions)
{
	if (!HasAuthority())
	{
		return;
	}

	Brain = NewObject<UBRBotBrain>(this, UBRBotBrain::StaticClass(), TEXT("BotBrain"));
	Brain->Initialize(InSeed, InScalars, InAmbitions);

	SightConfig->SightRadius = InScalars.SightRadius_m * CmPerMetre;
	SightConfig->LoseSightRadius = SightConfig->SightRadius;
	SightConfig->PeripheralVisionAngleDegrees = InScalars.SightFov_deg * 0.5f;
	SightConfig->SetMaxAge(InScalars.TargetMemory_s);
	BotPerception->ConfigureSense(*SightConfig);
	BotPerception->RequestStimuliListenerUpdate();

	if (!BotPerception->OnTargetPerceptionUpdated.IsAlreadyBound(this, &ABRBotController::HandleTargetPerceptionUpdated))
	{
		BotPerception->OnTargetPerceptionUpdated.AddDynamic(this, &ABRBotController::HandleTargetPerceptionUpdated);
	}

	ResolveStateTreeAsset();

	bConfigured = true;
}

void ABRBotController::ResolveStateTreeAsset()
{
	if (Brain == nullptr)
	{
		return;
	}

	const TSoftObjectPtr<UStateTree>& SoftTree = Brain->GetScalars().StateTreeSoftPath;
	if (SoftTree.IsNull())
	{
		return;
	}

	FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
	Streamable.RequestAsyncLoad(SoftTree.ToSoftObjectPath(),
		FStreamableDelegate::CreateUObject(this, &ABRBotController::OnStateTreeAssetLoaded));
}

void ABRBotController::OnStateTreeAssetLoaded()
{
	if (Brain == nullptr || StateTreeComponent == nullptr)
	{
		return;
	}

	UStateTree* LoadedTree = Brain->GetScalars().StateTreeSoftPath.Get();
	if (LoadedTree == nullptr)
	{
		return;
	}

	FStateTreeReference TreeReference;
	TreeReference.SetStateTree(LoadedTree);
	StateTreeComponent->SetStateTreeReference(TreeReference);

	if (GetPawn() != nullptr)
	{
		StateTreeComponent->StartLogic();
	}
}

void ABRBotController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (!HasAuthority())
	{
		return;
	}

	BindAbilitySystemEvents();

	PostBotEvent(EBRBotEventType::Spawned);

	if (StateTreeComponent != nullptr && bConfigured)
	{
		StateTreeComponent->StartLogic();
	}
}

void ABRBotController::OnUnPossess()
{
	UnbindAbilitySystemEvents();

	if (UWorld* World = GetWorld())
	{
		for (FTimerHandle& Handle : PendingReactionTimers)
		{
			World->GetTimerManager().ClearTimer(Handle);
		}
		World->GetTimerManager().ClearTimer(EngageTimer);
		World->GetTimerManager().ClearTimer(HoldTimer);
	}
	PendingReactionTimers.Reset();
	CurrentTarget.Reset();

	if (StateTreeComponent != nullptr)
	{
		StateTreeComponent->StopLogic(TEXT("Unpossessed"));
	}

	Super::OnUnPossess();
}

EBRBotAmbition ABRBotController::GetActiveAmbition() const
{
	return (Brain != nullptr) ? Brain->GetActiveAmbition() : EBRBotAmbition::None;
}

FBRBotPlanStep ABRBotController::GetCurrentStep() const
{
	return (Brain != nullptr) ? Brain->GetCurrentStep() : FBRBotPlanStep();
}

bool ABRBotController::RequiresReactionDelay(EBRBotEventType EventType)
{
	switch (EventType)
	{
	case EBRBotEventType::TargetPerceived:
	case EBRBotEventType::TargetLost:
	case EBRBotEventType::TargetShieldsBroken:
	case EBRBotEventType::CombatantDied:
	case EBRBotEventType::RocketWindowOpened:
	case EBRBotEventType::RocketTaken:
		return true;
	default:
		return false;
	}
}

void ABRBotController::PostBotEvent(EBRBotEventType EventType, int32 SubjectId)
{
	if (Brain == nullptr || !HasAuthority())
	{
		return;
	}

	FBRBotEvent Event;
	Event.Type = EventType;
	Event.SubjectId = SubjectId;

	if (!RequiresReactionDelay(EventType))
	{
		DispatchBotEvent(Event);
		return;
	}

	const int32 DelayMs = Brain->DrawReactionDelayMs();

	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(
		Handle,
		FTimerDelegate::CreateUObject(this, &ABRBotController::DispatchBotEvent, Event),
		static_cast<float>(DelayMs) / MsPerSecond,
		false);
	PendingReactionTimers.Add(Handle);
}

void ABRBotController::DispatchBotEvent(FBRBotEvent Event)
{
	if (Brain == nullptr)
	{
		return;
	}

	const FBRBotFacts Facts = BuildFacts();

#if !UE_BUILD_SHIPPING
	FString FactError;
	if (!Facts.ValidateNormalized(FactError))
	{
	}
#endif

	const FBRBotDecision Decision = Brain->Think(Event, Facts);

	ApplyDecisionToSpine(Decision);
}

void ABRBotController::ApplyDecisionToSpine(const FBRBotDecision& Decision)
{
	if (StateTreeComponent == nullptr)
	{
		return;
	}

	if (Decision.bAmbitionChanged && StateTreeComponent->IsRunning())
	{
		StateTreeComponent->RestartLogic();
	}
}

void ABRBotController::ReportStepCompleted()
{
	if (Brain != nullptr)
	{
		const FBRBotDecision Decision = Brain->NotifyStepCompleted(BuildFacts());
		ApplyDecisionToSpine(Decision);
	}
}

void ABRBotController::ReportStepFailed()
{
	if (Brain != nullptr)
	{
		const FBRBotDecision Decision = Brain->NotifyStepFailed(BuildFacts());
		ApplyDecisionToSpine(Decision);
	}
}

UBRAbilitySystemComponent* ABRBotController::GetBotASC() const
{
	if (const APlayerState* BotPlayerState = PlayerState)
	{
		return BotPlayerState->FindComponentByClass<UBRAbilitySystemComponent>();
	}
	return nullptr;
}

void ABRBotController::PressInputTag(FGameplayTag InputTag)
{
	if (!HasAuthority())
	{
		return;
	}

	if (UBRAbilitySystemComponent* ASC = GetBotASC())
	{
		ASC->AbilityInputTagPressed(InputTag);
	}
}

void ABRBotController::ReleaseInputTag(FGameplayTag InputTag)
{
	if (!HasAuthority())
	{
		return;
	}

	if (UBRAbilitySystemComponent* ASC = GetBotASC())
	{
		ASC->AbilityInputTagReleased(InputTag);
	}
}

bool ABRBotController::IsInputTagHeld(FGameplayTag InputTag) const
{
	const UBRAbilitySystemComponent* ASC = GetBotASC();
	return (ASC != nullptr) && ASC->IsAbilityInputHeld(InputTag);
}

bool ABRBotController::CanActivateByInputTag(FGameplayTag InputTag) const
{
	const UBRAbilitySystemComponent* ASC = GetBotASC();
	if (ASC == nullptr)
	{
		return false;
	}

	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.Ability == nullptr || !Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			continue;
		}
		if (Spec.Ability->CanActivateAbility(Spec.Handle, ASC->AbilityActorInfo.Get()))
		{
			return true;
		}
	}
	return false;
}

void ABRBotController::AimAtTargetWithError(const AActor* Target)
{
	if (Target == nullptr || Brain == nullptr || GetPawn() == nullptr)
	{
		return;
	}

	const FVector ViewLocation = GetPawn()->GetPawnViewLocation();
	const FVector ToTarget = Target->GetActorLocation() - ViewLocation;
	if (ToTarget.IsNearlyZero())
	{
		return;
	}

	FRotator AimRotation = ToTarget.Rotation();

	const FBRBotTierScalars& TierScalars = Brain->GetScalars();
	FRandomStream& Stream = Brain->GetStream();

	if (Stream.FRand() > TierScalars.AccuracyPct)
	{
		AimRotation.Yaw += Stream.FRandRange(-TierScalars.AimErrorDeg, TierScalars.AimErrorDeg);
		AimRotation.Pitch += Stream.FRandRange(-TierScalars.AimErrorDeg, TierScalars.AimErrorDeg);
	}

	SetControlRotation(AimRotation);
}

void ABRBotController::BeginEngage(AActor* Target)
{
	if (Brain == nullptr || Target == nullptr)
	{
		return;
	}

	SetFocus(Target, EAIFocusPriority::Gameplay);

	const int32 IntervalMs = Brain->GetScalars().EngageUpdateMs;
	if (IntervalMs <= 0)
	{
		RunEngageSelector();
		return;
	}

	const float Interval = static_cast<float>(IntervalMs) / MsPerSecond;
	GetWorldTimerManager().SetTimer(EngageTimer, this, &ABRBotController::RunEngageSelector, Interval, true, 0.f);
}

void ABRBotController::EndEngage()
{
	GetWorldTimerManager().ClearTimer(EngageTimer);
	ClearFocus(EAIFocusPriority::Gameplay);

	ReleaseInputTag(BRGameplayTags::InputTag_Fire);
	ReleaseInputTag(BRGameplayTags::InputTag_Sprint);
}

void ABRBotController::RunEngageSelector()
{
	AActor* Target = GetCurrentTarget();
	if (Target == nullptr)
	{
		ReleaseInputTag(BRGameplayTags::InputTag_Fire);
		return;
	}

	BRBotEngage::RunSelectorOnce(*this, Target);
}

void ABRBotController::RequestMoveToAnchor(UEnvQuery* LocationQuery, float AcceptanceRadius)
{
	PendingMoveAcceptanceRadius = AcceptanceRadius;

	if (LocationQuery == nullptr)
	{
		ReportStepFailed();
		return;
	}

	FEnvQueryRequest Request(LocationQuery, this);
	Request.Execute(EEnvQueryRunMode::SingleResult,
		FQueryFinishedSignature::CreateUObject(this, &ABRBotController::HandleAnchorQueryFinished));
}

void ABRBotController::HandleAnchorQueryFinished(TSharedPtr<FEnvQueryResult> Result)
{
	if (!Result.IsValid() || !Result->IsSuccessful() || Result->Items.Num() == 0)
	{
		ReportStepFailed();
		return;
	}

	FAIMoveRequest MoveRequest(Result->GetItemAsLocation(0));
	MoveRequest.SetAcceptanceRadius(PendingMoveAcceptanceRadius);
	MoveRequest.SetUsePathfinding(true);

	const FPathFollowingRequestResult MoveResult = MoveTo(MoveRequest);
	if (MoveResult.Code == EPathFollowingRequestResult::Failed)
	{
		ReportStepFailed();
	}
	else if (MoveResult.Code == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		ReportStepCompleted();
	}
}

void ABRBotController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	Super::OnMoveCompleted(RequestID, Result);

	if (Result.IsSuccess())
	{
		ReportStepCompleted();
	}
	else
	{
		ReportStepFailed();
	}
}

void ABRBotController::CancelActiveMove()
{
	if (UPathFollowingComponent* PathFollowing = GetPathFollowingComponent())
	{
		PathFollowing->AbortMove(*this, FPathFollowingResultFlags::MovementStop);
	}
}

void ABRBotController::StartHoldTimer(float HoldSeconds)
{
	if (HoldSeconds <= 0.f)
	{
		ReportStepCompleted();
		return;
	}
	GetWorldTimerManager().SetTimer(HoldTimer, this, &ABRBotController::HandleHoldElapsed, HoldSeconds, false);
}

void ABRBotController::CancelHoldTimer()
{
	GetWorldTimerManager().ClearTimer(HoldTimer);
}

void ABRBotController::HandleHoldElapsed()
{
	ReportStepCompleted();
}

AActor* ABRBotController::GetCurrentTarget() const
{
	return CurrentTarget.Get();
}

float ABRBotController::GetMatchTimeSeconds() const
{
	if (const AGameStateBase* GameStateBase = GetWorld() ? GetWorld()->GetGameState() : nullptr)
	{
		return GameStateBase->GetServerWorldTimeSeconds();
	}
	return 0.f;
}

FBRBotFacts ABRBotController::BuildFacts() const
{
	FBRBotFacts Facts;
	Facts.NowSeconds = GetMatchTimeSeconds();

	FillSelfFacts(Facts);
	FillTargetFacts(Facts);
	FillMatchFacts(Facts);

	return Facts;
}

void ABRBotController::FillSelfFacts(FBRBotFacts& Facts) const
{
	const UBRAbilitySystemComponent* ASC = GetBotASC();
	if (ASC == nullptr)
	{
		return;
	}

	const float MaxShields = ASC->GetNumericAttribute(UBRAttributeSet::GetMaxShieldsAttribute());
	const float MaxHealth = ASC->GetNumericAttribute(UBRAttributeSet::GetMaxHealthAttribute());
	Facts.MyShieldNorm = (MaxShields > 0.f)
		? FMath::Clamp(ASC->GetNumericAttribute(UBRAttributeSet::GetShieldsAttribute()) / MaxShields, 0.f, 1.f)
		: 0.f;
	Facts.MyHealthNorm = (MaxHealth > 0.f)
		? FMath::Clamp(ASC->GetNumericAttribute(UBRAttributeSet::GetHealthAttribute()) / MaxHealth, 0.f, 1.f)
		: 0.f;

	Facts.Set(EBRBotPrecondition::MyShieldsBroken, ASC->HasMatchingGameplayTag(BRGameplayTags::State_Shields_Broken));
	Facts.Set(EBRBotPrecondition::SelfDead, ASC->HasMatchingGameplayTag(BRGameplayTags::State_Dead));

	Facts.Set(EBRBotPrecondition::GrenadeReady, CanActivateByInputTag(BRGameplayTags::InputTag_Grenade));
	Facts.Set(EBRBotPrecondition::GrappleReady, CanActivateByInputTag(BRGameplayTags::InputTag_Grapple));
	Facts.Set(EBRBotPrecondition::HasAmmo, CanActivateByInputTag(BRGameplayTags::InputTag_Fire));
	Facts.Set(EBRBotPrecondition::ReloadNeeded, CanActivateByInputTag(BRGameplayTags::InputTag_Reload)
		&& !CanActivateByInputTag(BRGameplayTags::InputTag_Fire));

	const APawn* SelfPawn = GetPawn();
	Facts.CoverQualityNorm = BRBotArena::GetCoverQualityAt(SelfPawn);
	Facts.Set(EBRBotPrecondition::InCover, Facts.CoverQualityNorm > 0.f);
}

void ABRBotController::FillTargetFacts(FBRBotFacts& Facts) const
{
	const AActor* Target = CurrentTarget.Get();
	const APawn* SelfPawn = GetPawn();
	if (Target == nullptr || SelfPawn == nullptr || Brain == nullptr)
	{
		return;
	}

	Facts.Set(EBRBotPrecondition::TargetKnown, true);

	const bool bCurrentlyVisible = (BotPerception != nullptr)
		&& BotPerception->HasActiveStimulus(*Target, UAISense::GetSenseID<UAISense_Sight>());
	Facts.Set(EBRBotPrecondition::TargetVisible, bCurrentlyVisible);

	const float SightRadiusCm = FMath::Max(Brain->GetScalars().SightRadius_m * CmPerMetre, KINDA_SMALL_NUMBER);
	const float Distance = FVector::Dist(SelfPawn->GetActorLocation(), Target->GetActorLocation());

	Facts.DistToTargetNorm = FMath::Clamp(1.f - (Distance / SightRadiusCm), 0.f, 1.f);
	Facts.ThreatExposureNorm = 1.f - Facts.CoverQualityNorm;

	Facts.TargetShieldNorm = Facts.Has(EBRBotPrecondition::TargetShieldsBroken) ? 0.f : 1.f;
}

void ABRBotController::FillMatchFacts(FBRBotFacts& Facts) const
{
	if (!bWarnedMatchFactsSeam)
	{
		bWarnedMatchFactsSeam = true;
	}

	const APawn* SelfPawn = GetPawn();
	FVector PadLocation = FVector::ZeroVector;
	if (SelfPawn != nullptr && Brain != nullptr && BRBotArena::GetRocketPadLocation(GetWorld(), PadLocation))
	{
		const float SightRadiusCm = FMath::Max(Brain->GetScalars().SightRadius_m * CmPerMetre, KINDA_SMALL_NUMBER);
		const float Distance = FVector::Dist(SelfPawn->GetActorLocation(), PadLocation);
		Facts.DistToRocketNorm = FMath::Clamp(1.f - (Distance / SightRadiusCm), 0.f, 1.f);
	}
}

void ABRBotController::HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (Actor == nullptr || !HasAuthority())
	{
		return;
	}

	if (GetTeamAttitudeTowards(*Actor) != ETeamAttitude::Hostile)
	{
		return;
	}

	const int32 SubjectId = INDEX_NONE;

	if (Stimulus.WasSuccessfullySensed())
	{
		if (!CurrentTarget.IsValid())
		{
			CurrentTarget = Actor;
		}
		LastTargetSeenSeconds = GetMatchTimeSeconds();
		PostBotEvent(EBRBotEventType::TargetPerceived, SubjectId);
	}
	else if (CurrentTarget.Get() == Actor)
	{
		CurrentTarget.Reset();
		PostBotEvent(EBRBotEventType::TargetLost, SubjectId);
	}
}

void ABRBotController::HandleShieldsBrokenTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	if (NewCount > 0)
	{
		PostBotEvent(EBRBotEventType::SelfShieldsBroken);
	}
}

void ABRBotController::BindAbilitySystemEvents()
{
	UBRAbilitySystemComponent* ASC = GetBotASC();
	if (ASC == nullptr)
	{
		return;
	}

	ShieldsBrokenHandle = ASC->RegisterGameplayTagEvent(
		BRGameplayTags::State_Shields_Broken, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &ABRBotController::HandleShieldsBrokenTagChanged);
}

void ABRBotController::UnbindAbilitySystemEvents()
{
	if (UBRAbilitySystemComponent* ASC = GetBotASC())
	{
		if (ShieldsBrokenHandle.IsValid())
		{
			ASC->RegisterGameplayTagEvent(BRGameplayTags::State_Shields_Broken, EGameplayTagEventType::NewOrRemoved)
				.Remove(ShieldsBrokenHandle);
		}
	}
	ShieldsBrokenHandle.Reset();
}
