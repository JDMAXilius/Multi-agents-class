// Breachpoint. The glue. Every line here either perceives, thinks, or presses a button.

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

DEFINE_LOG_CATEGORY_STATIC(LogBRBot, Log, All);

namespace
{
	/** Centimetres per metre. A unit constant, never tuned (the FBRWeaponRow precedent). */
	constexpr float CmPerMetre = 100.f;
	constexpr float MsPerSecond = 1000.f;
}

ABRBotController::ABRBotController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// A bot is a player the AI drives: it needs a PlayerState, because the PlayerState is
	// where the ASC and the attribute set live (§3.6). This one line is what makes "same
	// abilities, same costs, same cooldowns" structurally true rather than aspirational.
	bWantsPlayerState = true;

	// Law 4. Nothing in this class runs on a frame boundary.
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	StateTreeComponent = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("BotStateTree"));

	BotPerception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("BotPerception"));
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("BotSight"));

	// Geometry is filled from DT_BotTuning in ConfigureBot — deliberately NOT here, because
	// a sight radius typed in a constructor is a tuning number in C++ (law 3).
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = false;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = false;

	BotPerception->ConfigureSense(*SightConfig);
	BotPerception->SetDominantSense(SightConfig->GetSenseImplementation());
	SetPerceptionComponent(*BotPerception);
}

// =============================================================================================
// Setup
// =============================================================================================

void ABRBotController::ConfigureBot(int32 InSeed, const FBRBotTierScalars& InScalars, const TArray<FBRBotAmbitionDef>& InAmbitions)
{
	if (!HasAuthority())
	{
		// Bots exist only on the server. A client-side bot would be a second simulation.
		UE_LOG(LogBRBot, Error, TEXT("ConfigureBot called without authority — refused."));
		return;
	}

	Brain = NewObject<UBRBotBrain>(this, UBRBotBrain::StaticClass(), TEXT("BotBrain"));
	Brain->Initialize(InSeed, InScalars, InAmbitions);

	// Perception geometry from data. Metres in the table, centimetres in the engine — the
	// conversion happens here, at the boundary, and never inside the brain.
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
		// The honest state until ST_Bot is authored and DT_BotTuning points at it: the bot
		// thinks correctly and does nothing, loudly. Silence here would look like a
		// pathing bug for a week.
		UE_LOG(LogBRBot, Warning,
			TEXT("Tier '%s' names no StateTree asset — this bot will decide but never act."),
			*Brain->GetScalars().TierName.ToString());
		return;
	}

	// SOFT load through the streamable manager (law 3: no hard refs, no ConstructorHelpers).
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
		UE_LOG(LogBRBot, Error, TEXT("ST_Bot failed to load for tier '%s'."), *Brain->GetScalars().TierName.ToString());
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

	// Spawned is the one event with no perceptual component: a bot knows it is alive.
	PostBotEvent(EBRBotEventType::Spawned);

	if (StateTreeComponent != nullptr && bConfigured)
	{
		StateTreeComponent->StartLogic();
	}
}

void ABRBotController::OnUnPossess()
{
	UnbindAbilitySystemEvents();

	// A death must not leave a reaction in flight: the timer would fire into the next life
	// with facts from the previous one. This is the stale-state bug the critic looks for.
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

// =============================================================================================
// Layer 1 <-> layer 2
// =============================================================================================

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
	// The split is "did I have to SEE it?". Own shields breaking, own damage taken, and the
	// spine's own step reports are inherent knowledge and arrive instantly; anything about
	// another combatant or the match state a human reads off the world goes through the
	// tier's reaction. R11's floor therefore applies to every perceptual path there is.
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

	// The reaction delay: seeded, quantized, and clamped at 200 ms inside the accessor —
	// there is no argument, scalar, or code path that produces less (ruling R11).
	const int32 DelayMs = Brain->DrawReactionDelayMs();

	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(
		Handle,
		FTimerDelegate::CreateUObject(this, &ABRBotController::DispatchBotEvent, Event),
		static_cast<float>(DelayMs) / MsPerSecond,
		/*bLoop=*/false);
	PendingReactionTimers.Add(Handle);
}

void ABRBotController::DispatchBotEvent(FBRBotEvent Event)
{
	if (Brain == nullptr)
	{
		return;
	}

	// Facts are snapshotted HERE, after the reaction delay, not when the event fired: a bot
	// reacts to the world as it is when it notices, which is also what a human does.
	const FBRBotFacts Facts = BuildFacts();

#if !UE_BUILD_SHIPPING
	FString FactError;
	if (!Facts.ValidateNormalized(FactError))
	{
		UE_LOG(LogBRBot, Error, TEXT("Fact fill produced an out-of-range value: %s"), *FactError);
	}
#endif

	const FBRBotDecision Decision = Brain->Think(Event, Facts);

	UE_LOG(LogBRBot, Verbose, TEXT("[%s] %s"), *GetName(), *Decision.ToTraceString());

	ApplyDecisionToSpine(Decision);
}

void ABRBotController::ApplyDecisionToSpine(const FBRBotDecision& Decision)
{
	if (StateTreeComponent == nullptr)
	{
		return;
	}

	// A new AMBITION is a stance change, and a stance change should be visible (R12): the
	// tree is restarted so the bot re-enters a state rather than sliding between them.
	// A new STEP inside the same ambition is picked up by the tree's own transition
	// conditions, which read GetCurrentStep().
	//
	// CONTRACT GAP 4: the clean form of this is StateTreeComponent->SendStateTreeEvent() with
	// an `Event.Bot.Replan` tag, which would let the tree gate transitions on the event
	// instead of on condition re-evaluation. BRGameplayTags.h enumerates no such tag and
	// explicitly refuses to invent one, so §3.1 has to be amended first.
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

// =============================================================================================
// Layer 2 -> layer 3: the ONE intent function
// =============================================================================================

UBRAbilitySystemComponent* ABRBotController::GetBotASC() const
{
	// The ASC lives on the PlayerState, exactly as a human's does. There is no bot-specific
	// component and no second ASC anywhere in this class.
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
		// *** The entire AI -> world surface of Breachpoint is this one call. ***
		// Same function, same component, same gates as ABRPlayerController's human path.
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

	// A GOAP precondition asked as a GAS question (AI-BOTS §2: "the ASC IS the world-model").
	// Costs, cooldowns and blocking tags all answer here, so the planner cannot want
	// something the hand may not do.
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

// =============================================================================================
// Aim
// =============================================================================================

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

	// accuracy_pct of shots are aimed true; the rest are pushed into a cone of aim_error_deg.
	// Drawn from the BOT'S OWN seeded stream — FMath::FRand / FMath::VRandCone would make a
	// match unreplayable, and the pre-tool hook blocks them outright.
	if (Stream.FRand() > TierScalars.AccuracyPct)
	{
		AimRotation.Yaw += Stream.FRandRange(-TierScalars.AimErrorDeg, TierScalars.AimErrorDeg);
		AimRotation.Pitch += Stream.FRandRange(-TierScalars.AimErrorDeg, TierScalars.AimErrorDeg);
	}

	// Control rotation only: this steers the same view direction a human's mouse does. The
	// fire ability reads it afterwards, which is why aim error must be applied BEFORE the
	// press and never as a post-hoc fudge of the hit result (BP08 step 3).
	SetControlRotation(AimRotation);
}

// =============================================================================================
// Engage — a data-rate timer, never a Tick (law 4)
// =============================================================================================

void ABRBotController::BeginEngage(AActor* Target)
{
	if (Brain == nullptr || Target == nullptr)
	{
		return;
	}

	// Focus makes the pawn LOOK at what it is fighting, which is half of legibility: an
	// observer can tell who a bot has decided to kill (R12).
	SetFocus(Target, EAIFocusPriority::Gameplay);

	const int32 IntervalMs = Brain->GetScalars().EngageUpdateMs;
	if (IntervalMs <= 0)
	{
		// Unconfigured tier: one pass, then nothing. Inert and loud beats a plausible default.
		UE_LOG(LogBRBot, Warning, TEXT("engage_update_ms is 0 for tier '%s' — the fight loop will not run."),
			*Brain->GetScalars().TierName.ToString());
		RunEngageSelector();
		return;
	}

	const float Interval = static_cast<float>(IntervalMs) / MsPerSecond;
	GetWorldTimerManager().SetTimer(EngageTimer, this, &ABRBotController::RunEngageSelector, Interval, /*bLoop=*/true, /*FirstDelay=*/0.f);
}

void ABRBotController::EndEngage()
{
	GetWorldTimerManager().ClearTimer(EngageTimer);
	ClearFocus(EAIFocusPriority::Gameplay);

	// Release everything this stance could be holding. A held trigger that survives a state
	// change is a bot firing at nothing forever — and it is the ASC, not us, that owns the
	// held-input truth, so we release through the same path a human's key-up takes.
	ReleaseInputTag(BRGameplayTags::InputTag_Fire);
	ReleaseInputTag(BRGameplayTags::InputTag_Sprint);
}

void ABRBotController::RunEngageSelector()
{
	AActor* Target = GetCurrentTarget();
	if (Target == nullptr)
	{
		// Nothing to fight: stop holding the trigger and let the brain notice on the next
		// TargetLost event. The task does not decide to leave — layer 1 does.
		ReleaseInputTag(BRGameplayTags::InputTag_Fire);
		return;
	}

	// The BT-shaped priority selector lives in BRStateTreeTasks.cpp, where §3.7 puts it.
	BRBotEngage::RunSelectorOnce(*this, Target);
}

// =============================================================================================
// Movement — EQS -> path follow -> delegate
// =============================================================================================

void ABRBotController::RequestMoveToAnchor(UEnvQuery* LocationQuery, float AcceptanceRadius)
{
	PendingMoveAcceptanceRadius = AcceptanceRadius;

	if (LocationQuery == nullptr)
	{
		UE_LOG(LogBRBot, Warning, TEXT("MoveTo step has no EQS query authored on the state — failing the step."));
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
		// The authored vocabulary had no answer. Fail the step; the brain replans. We do NOT
		// fall back to "any reachable point" — that is the nav divination this design rejects.
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
	// RequestSuccessful: OnMoveCompleted reports it.
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
		// Blocked, aborted or invalid path. Reported, never absorbed: "bot stuck on navmesh"
		// is a nightly-soak finding and it only becomes one if this path is honest.
		UE_LOG(LogBRBot, Verbose, TEXT("[%s] move failed (%s)"), *GetName(), *Result.ToString());
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
	GetWorldTimerManager().SetTimer(HoldTimer, this, &ABRBotController::HandleHoldElapsed, HoldSeconds, /*bLoop=*/false);
}

void ABRBotController::CancelHoldTimer()
{
	GetWorldTimerManager().ClearTimer(HoldTimer);
}

void ABRBotController::HandleHoldElapsed()
{
	ReportStepCompleted();
}

// =============================================================================================
// Facts
// =============================================================================================

AActor* ABRBotController::GetCurrentTarget() const
{
	return CurrentTarget.Get();
}

float ABRBotController::GetMatchTimeSeconds() const
{
	// The SERVER's replicated match clock, not a platform clock: FPlatformTime / FDateTime
	// appear nowhere in AI/ (AI-BOTS §5, "no wall-clock").
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

	// Vitals: MY attributes, which a human reads off their own HUD.
	const float MaxShields = ASC->GetNumericAttribute(UBRAttributeSet::GetMaxShieldsAttribute());
	const float MaxHealth = ASC->GetNumericAttribute(UBRAttributeSet::GetMaxHealthAttribute());
	Facts.MyShieldNorm = (MaxShields > 0.f)
		? FMath::Clamp(ASC->GetNumericAttribute(UBRAttributeSet::GetShieldsAttribute()) / MaxShields, 0.f, 1.f)
		: 0.f;
	Facts.MyHealthNorm = (MaxHealth > 0.f)
		? FMath::Clamp(ASC->GetNumericAttribute(UBRAttributeSet::GetHealthAttribute()) / MaxHealth, 0.f, 1.f)
		: 0.f;

	// State is a tag, never a bool on an actor (gas-purity).
	Facts.Set(EBRBotPrecondition::MyShieldsBroken, ASC->HasMatchingGameplayTag(BRGameplayTags::State_Shields_Broken));
	Facts.Set(EBRBotPrecondition::SelfDead, ASC->HasMatchingGameplayTag(BRGameplayTags::State_Dead));

	// Ability readiness: the ASC's own answer, cost and cooldown included.
	Facts.Set(EBRBotPrecondition::GrenadeReady, CanActivateByInputTag(BRGameplayTags::InputTag_Grenade));
	Facts.Set(EBRBotPrecondition::GrappleReady, CanActivateByInputTag(BRGameplayTags::InputTag_Grapple));
	Facts.Set(EBRBotPrecondition::HasAmmo, CanActivateByInputTag(BRGameplayTags::InputTag_Fire));
	Facts.Set(EBRBotPrecondition::ReloadNeeded, CanActivateByInputTag(BRGameplayTags::InputTag_Reload)
		&& !CanActivateByInputTag(BRGameplayTags::InputTag_Fire));

	// CONTRACT GAP 3 (ammo half): my_ammo_norm needs BREquipmentComponent's magazine count,
	// which BP08 does not own and which was being written in parallel. Until that accessor
	// exists the fact stays at its 1.0 default and any consideration weighting it is inert —
	// stated here so nobody reads "bots never reload" as a brain bug.

	// Space: the arena's authored vocabulary answers "am I in cover", not a trace of my own.
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

	// "Visible" is the perception system's answer, never a fresh trace of our own.
	const bool bCurrentlyVisible = (BotPerception != nullptr)
		&& BotPerception->HasActiveStimulus(*Target, UAISense::GetSenseID<UAISense_Sight>());
	Facts.Set(EBRBotPrecondition::TargetVisible, bCurrentlyVisible);

	const float SightRadiusCm = FMath::Max(Brain->GetScalars().SightRadius_m * CmPerMetre, KINDA_SMALL_NUMBER);
	const float Distance = FVector::Dist(SelfPawn->GetActorLocation(), Target->GetActorLocation());

	// Normalized against what this tier can SEE — the only scale that means anything to a
	// bot, and a data-driven one, so no arena constant enters C++.
	Facts.DistToTargetNorm = FMath::Clamp(1.f - (Distance / SightRadiusCm), 0.f, 1.f);
	Facts.ThreatExposureNorm = 1.f - Facts.CoverQualityNorm;

	// CONTRACT GAP 5: enemy_shield_norm is a BELIEF, updated by the observed shield-break
	// cue (TargetShieldsBroken) and nothing else. Wiring that observation needs the cue tag
	// family §3.1 has not enumerated; until then the belief is "full shields on sight",
	// which is the conservative, non-cheating default. It must NEVER be filled from the
	// target's ASC — that would be the aimbot-grade privileged read this whole design forbids.
	Facts.TargetShieldNorm = Facts.Has(EBRBotPrecondition::TargetShieldsBroken) ? 0.f : 1.f;
}

void ABRBotController::FillMatchFacts(FBRBotFacts& Facts) const
{
	// CONTRACT GAP 2 — the declared seam. rocket_timer_norm, dist_to_rocket_norm,
	// score_deficit_norm and time_remaining_norm come from ABRGameState and the rocket
	// subsystem (BP04/BP09), neither of which BP08 owns. Everything except the rocket
	// distance is left at its neutral default, so ControlRocket simply never outscores
	// anything until the seam is wired — visibly, and with this warning, rather than
	// silently at some accidental value.
	if (!bWarnedMatchFactsSeam)
	{
		bWarnedMatchFactsSeam = true;
		UE_LOG(LogBRBot, Warning,
			TEXT("Match facts (rocket timer, score, clock) are not wired yet — BP08 contract_gap 2."));
	}

	// The one match fact BP08 can compute from what it owns: how close I am to the pad.
	const APawn* SelfPawn = GetPawn();
	FVector PadLocation = FVector::ZeroVector;
	if (SelfPawn != nullptr && Brain != nullptr && BRBotArena::GetRocketPadLocation(GetWorld(), PadLocation))
	{
		const float SightRadiusCm = FMath::Max(Brain->GetScalars().SightRadius_m * CmPerMetre, KINDA_SMALL_NUMBER);
		const float Distance = FVector::Dist(SelfPawn->GetActorLocation(), PadLocation);
		Facts.DistToRocketNorm = FMath::Clamp(1.f - (Distance / SightRadiusCm), 0.f, 1.f);
	}
}

// =============================================================================================
// Perception
// =============================================================================================

void ABRBotController::HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (Actor == nullptr || !HasAuthority())
	{
		return;
	}

	// Only hostiles are targets, and hostility is the engine's team attitude — the same
	// answer the HUD gives a human, not a scan of anyone's state.
	if (GetTeamAttitudeTowards(*Actor) != ETeamAttitude::Hostile)
	{
		return;
	}

	const int32 SubjectId = INDEX_NONE; // Filled with the PlayerState player id once §3.6's accessor is used.

	if (Stimulus.WasSuccessfullySensed())
	{
		// Keep the incumbent target unless there is none: target-swapping on every stimulus
		// is precisely the ambition thrash R12 rejects, one layer down.
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
	// THE Halo break-off beat (R12): the shield-crack is an event, the ambition rescore is
	// its consequence, and a playtester should be able to name what happened from behaviour
	// alone. Immediate, not reaction-delayed: you do not have to notice your own shields.
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
		UE_LOG(LogBRBot, Warning, TEXT("No ASC on this bot's PlayerState — it will never perceive its own state."));
		return;
	}

	ShieldsBrokenHandle = ASC->RegisterGameplayTagEvent(
		BRGameplayTags::State_Shields_Broken, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &ABRBotController::HandleShieldsBrokenTagChanged);

	// CONTRACT GAP 6: Event.Kill / Event.Death arrive through the ASC's generic gameplay
	// event callbacks, which ABRGameMode owns the sending half of (§3.6). Binding them from
	// here would duplicate the GameMode's own bindings; the GameMode should instead call
	// PostBotEvent(CombatantDied) on nearby bot controllers. One line in its death handler,
	// and it is not BP08's line to write.
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
