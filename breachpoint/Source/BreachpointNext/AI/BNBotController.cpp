#include "AI/BNBotController.h"

#include "AbilitySystem/BNAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/BNAttributeSet.h"
#include "Data/BNDataRows.h"
#include "AI/BNBotBrain.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "BreachpointNext.h"
#include "Characters/BNCharacter.h"
#include "Weapons/BNEquipmentComponent.h"
#include "Weapons/BNWeapon.h"
#include "Core/BNGameplayTags.h"
#include "Data/BNGameData.h"
#include "Match/BNPlayerState.h"
#include "Match/BNTeams.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Components/StateTreeAIComponent.h"
#include "StateTree.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "GameFramework/PlayerState.h"
#include "GameplayEffectTypes.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISense_Hearing.h"
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

	// Seeded from the ROW's defaults (Marine), because config has not been applied yet and the
	// tier cannot be resolved this early. OnPossess re-applies whatever the real tier says.
	const FBNBotTuningRow Defaults;
	SightConfig->SightRadius = Defaults.SightRadius;
	SightConfig->LoseSightRadius = Defaults.LoseSightRadius;
	SightConfig->PeripheralVisionAngleDegrees = Defaults.PeripheralVisionAngleDegrees;

	// FFA: the sight sense detects EVERYTHING — the target rule below is what decides hostility,
	// not the affiliation filter, so teams can land later without touching perception.
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;

	// R10 — EARS, configured beside the eyes. Sight stays DOMINANT: when a bot both sees and
	// hears the same actor, the sighting is the better information and must win.
	HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("BotHearing"));
	HearingConfig->HearingRange = Defaults.HearingRange;
	HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
	HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
	HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;

	BotPerception->ConfigureSense(*SightConfig);
	BotPerception->ConfigureSense(*HearingConfig);
	BotPerception->SetDominantSense(SightConfig->GetSenseImplementation());
	SetPerceptionComponent(*BotPerception);

	BotPerception->OnTargetPerceptionUpdated.AddDynamic(this, &ABNBotController::OnPerceptionUpdated);
	BotPerception->OnTargetPerceptionForgotten.AddDynamic(this, &ABNBotController::OnPerceptionForgotten);

	SetGenericTeamId(FGenericTeamId(255));
}

FGenericTeamId ABNBotController::GetGenericTeamId() const
{
	// TEAMS (BN15): the PlayerState is the one team datum; the controller only reads it —
	// the same door GetBotASC uses. No PlayerState yet reads NoTeam: honest-unknown, and
	// through the guard below, nobody's friend.
	const ABNPlayerState* PS = GetPlayerState<ABNPlayerState>();
	return PS ? PS->GetGenericTeamId() : FGenericTeamId::NoTeam;
}

ETeamAttitude::Type ABNBotController::GetTeamAttitudeTowards(const AActor& Other) const
{
	// TEAMS (BN15): both sides answer through the PlayerState door and compare via the
	// one guarded helper. Either side at NoTeam falls through AreFriendly's guard to
	// Hostile — the FFA answer this seam always gave, so a teamless match is unchanged:
	// any other pawn is hostile. Non-pawns are scenery.
	const APawn* OtherPawn = Cast<APawn>(&Other);
	if (!OtherPawn)
	{
		return ETeamAttitude::Neutral;
	}
	if (OtherPawn == GetPawn())
	{
		return ETeamAttitude::Friendly;
	}
	const ABNPlayerState* OtherPS = OtherPawn->GetPlayerState<ABNPlayerState>();
	const FGenericTeamId OtherTeam = OtherPS ? OtherPS->GetGenericTeamId() : FGenericTeamId::NoTeam;
	return BNTeams::AreFriendly(GetGenericTeamId(), OtherTeam)
		? ETeamAttitude::Friendly : ETeamAttitude::Hostile;
}

void ABNBotController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// R10 — THE TIER, resolved before the sense is configured because the sense reads it. This is
	// also where a per-bot tier assigned by the GameMode before possession takes effect.
	ResolveTuning();

	// A bot spawns knowing nobody. Without this the fallback would only ever arm on LOSING a
	// target, so a bot that never saw one in the first place would roam for the whole match.
	ArmNoTargetFallback();

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
	// Let go of the key before the pawn goes. A held sprint whose release never arrives leaves
	// the speed GE and the Sprinting tag on the PERSISTENT PlayerState ASC, which outlives the
	// body — the same shape as the jump-tag leak UBNGA_Jump guards against.
	SetSprinting(false);
	SetCrouching(false);

	APawn* PreviousPawn = GetPawn();

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
		if (PreviousPawn && ASC->GetAvatarActor() == PreviousPawn)
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

ABNWeapon* ABNBotController::GetCurrentWeapon() const
{
	// Through the pawn's equipment component, never a cached pointer: the bot respawns, swaps and
	// dies, and every one of those invalidates a cache. The component IS the source of truth.
	const ABNCharacter* BotCharacter = Cast<ABNCharacter>(GetPawn());
	const UBNEquipmentComponent* Equipment = BotCharacter ? BotCharacter->GetEquipmentComponent() : nullptr;
	return Equipment ? Equipment->GetCurrentWeapon() : nullptr;
}

bool ABNBotController::HasLineOfSightToTarget() const
{
	AActor* Target = GetCurrentTarget();
	if (!Target)
	{
		return false;
	}

	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : -1.0;

	// Serve from cache only for the SAME target and inside the window. A target switch drops the
	// cache on the spot: inheriting the previous target's visibility is how a bot ends up firing
	// at a wall for a tenth of a second.
	if (Now >= 0.0
		&& LosCachedTarget.Get() == Target
		&& LosCachedAtSeconds >= 0.0
		&& (Now - LosCachedAtSeconds) < LineOfSightCacheSeconds)
	{
		return bLosCachedResult;
	}

	// AAIController::LineOfSightTo traces from the pawn's view point against the target's own
	// sight-test points — the same geometry the sight sense used to acquire it, so a bot that
	// lost the corner cannot keep firing through it.
	const bool bResult = LineOfSightTo(Target);

	LosCachedTarget = Target;
	LosCachedAtSeconds = Now;
	bLosCachedResult = bResult;
	return bResult;
}

void ABNBotController::SetCrouching(bool bWantCrouch)
{
	const ACharacter* Character = Cast<ACharacter>(GetPawn());
	if (!Character)
	{
		return;
	}

	// Mid-air the toggle only ever UNcrouches (UBNGA_Crouch's own guard), so asking for a crouch
	// while falling would silently do the opposite of what the caller wanted.
	const UCharacterMovementComponent* Move = Character->GetCharacterMovement();
	if (bWantCrouch && Move && Move->IsFalling())
	{
		return;
	}

	// Compare against the ENGINE's replicated crouch state rather than a bool of our own: the
	// ability, a landing, or an uncrouch forced by a low ceiling can all change it behind us, and
	// a private mirror would drift out of step and then press at exactly the wrong moment.
	if (Character->bIsCrouched == bWantCrouch)
	{
		return;
	}

	PressInputTag(BNTags::Input_Crouch);
	ReleaseInputTag(BNTags::Input_Crouch);
}

void ABNBotController::SetSprinting(bool bWantSprint)
{
	if (bWantSprint == bSprintHeld)
	{
		return;
	}

	bSprintHeld = bWantSprint;
	if (bWantSprint)
	{
		PressInputTag(BNTags::Input_Sprint);
	}
	else
	{
		ReleaseInputTag(BNTags::Input_Sprint);
	}
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

	// It has something to fight: stand the fallback down until it loses this one.
	if (UWorld* TimerWorld = GetWorld())
	{
		TimerWorld->GetTimerManager().ClearTimer(NoTargetTimerHandle);
	}

	// R11's clock starts HERE, on acquisition, not on entering the Shoot state — otherwise a bot
	// that loses and re-acquires sight of a target it has been fighting for ten seconds would pay
	// the full reaction cost again, and would read as hesitant rather than alert.
	const UWorld* World = GetWorld();
	TargetAcquiredSeconds = World ? World->GetTimeSeconds() : -1.0;
	CurrentReactionSeconds = DrawReactionSeconds();

	RescoreBrain();
}

void ABNBotController::ClearCurrentTarget()
{
	const bool bHadTarget = TargetEnemy.IsValid();

	// Remember where it WAS before the pointer goes. This is the whole of the search behaviour's
	// memory: one position and one timestamp, written at the only moment the truth is still known.
	if (const AActor* Leaving = TargetEnemy.Get())
	{
		RememberThreatAt(Leaving->GetActorLocation());
	}

	TargetEnemy.Reset();
	TargetAcquiredSeconds = -1.0;
	ArmNoTargetFallback();
	if (bHadTarget)
	{
		RescoreBrain();
	}
}

void ABNBotController::ArmNoTargetFallback()
{
	UWorld* World = GetWorld();
	if (!World || NoTargetGraceSeconds <= 0.f)
	{
		return;
	}

	// One-shot, and re-armed only from ClearCurrentTarget/OnPossess. A LOOPING timer here would
	// re-acquire every tick of it and make the grace period meaningless.
	World->GetTimerManager().SetTimer(NoTargetTimerHandle, this,
		&ABNBotController::OnNoTargetGraceElapsed, NoTargetGraceSeconds, /*bLoop=*/false);
}

void ABNBotController::OnNoTargetGraceElapsed()
{
	// Perception may have found someone while the timer ran; that answer always wins because it
	// is the honest one.
	if (GetThreat())
	{
		return;
	}

	AActor* Nearest = FindNearestValidEnemy();
	if (!Nearest)
	{
		// Nobody alive to fight (between rounds, or last one standing). Try again rather than
		// going quiet forever.
		ArmNoTargetFallback();
		return;
	}

	// Remember WHERE before setting the target: if the tree elects to Search rather than Engage,
	// the memory is what carries the bot across the arena to it.
	RememberThreatAt(Nearest->GetActorLocation());
	SetCurrentTarget(Nearest);

	UE_LOG(LogBN, Verbose, TEXT("BNBots: %s found nothing for %.1fs and is going after the nearest, %s."),
		*GetNameSafe(GetPawn()), NoTargetGraceSeconds, *GetNameSafe(Nearest));
}

AActor* ABNBotController::FindNearestValidEnemy() const
{
	const APawn* MyPawn = GetPawn();
	UWorld* World = GetWorld();
	if (!MyPawn || !World)
	{
		return nullptr;
	}

	const FVector From = MyPawn->GetActorLocation();
	AActor* Best = nullptr;
	double BestDistSq = TNumericLimits<double>::Max();

	// BOUNDED BY THE BOT'S OWN EYES. The first version of this iterated every pawn in the world
	// with no range and no line-of-sight test, which made SightRadius decorative: it decided only
	// whether a bot found you in under 5 seconds or in exactly 5. Every match converged on global
	// omniscience, and GDD §2.8 promises the opposite ("no privileged state").
	//
	// LoseSightRadius rather than SightRadius on purpose: this is the generous edge of what the
	// bot could plausibly still know about, and it is the same number that governs letting a real
	// target go. A bot with nobody inside it genuinely has nothing to chase and should keep
	// roaming, which is also what carries it off the top platform.
	const float Reach = FMath::Max(GetTuning().LoseSightRadius, GetTuning().SightRadius);
	const double ReachSq = static_cast<double>(Reach) * static_cast<double>(Reach);

	// IsValidTarget is the SAME gate perception uses, deliberately: two different notions of
	// "enemy" would let this fallback pick something the tree then refuses to fight.
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		APawn* Candidate = *It;
		if (!Candidate || Candidate == MyPawn)
		{
			continue;
		}
		if (!IsValidTarget(Candidate))
		{
			continue;
		}

		const double DistSq = FVector::DistSquared(From, Candidate->GetActorLocation());
		if (DistSq > ReachSq || DistSq >= BestDistSq)
		{
			continue;
		}

		// And it must be SEEABLE. Without this a bot walks through two walls to a player who has
		// never been perceived, which is the wallhack this whole guard exists to remove.
		if (!LineOfSightTo(Candidate))
		{
			continue;
		}

		BestDistSq = DistSq;
		Best = Candidate;
	}

	return Best;
}

float ABNBotController::DrawReactionSeconds()
{
	// Seeded on identity + draw index, so the trace replays: no wall clock, no global RNG (§5).
	FRandomStream Stream(static_cast<int32>(GetTypeHash(this)) ^ (ReactionDrawCount++ * 0x9E3779B9));
	const FBNBotTuningRow& Row = GetTuning();

	// R11 IS A LAW, NOT A DIFFICULTY KNOB: "No tier, scalar, or ambition weight may produce
	// sub-human reaction." It is enforced HERE, at the single point every reaction is drawn,
	// rather than trusted to four tier tables plus a DataTable a designer can edit — because
	// that trust already failed. The shipped config ran BotTier=Spartan at 0.08-0.16s and
	// ODST at 0.14-0.28s: a closed ruling breached in data, silently, with no compile error.
	// A clamp at the draw cannot be edited around from a table or an ini.
	static constexpr float R11ReactionFloorSeconds = 0.20f;

	const float Low = FMath::Max(R11ReactionFloorSeconds, Row.ReactionSecondsMin);
	const float High = FMath::Max(Low, Row.ReactionSecondsMax);
	const float Raw = Stream.FRandRange(Low, High);
	return ReactionQuantumSeconds > 0.f
		? FMath::Max(Low, FMath::GridSnap(Raw, ReactionQuantumSeconds))
		: Raw;
}

bool ABNBotController::HasReactedToBlast() const
{
	// The warning stamps IncomingBlastNoticedSeconds; the bot may not act until its own drawn
	// reaction has elapsed, exactly as for a sighting. Same clock, same law, one fewer exception.
	if (IncomingBlastNoticedSeconds < 0.0)
	{
		return false;
	}
	const UWorld* World = GetWorld();
	return World && (World->GetTimeSeconds() - IncomingBlastNoticedSeconds) >= CurrentReactionSeconds;
}

bool ABNBotController::HasReactedToTarget() const
{
	if (TargetAcquiredSeconds < 0.0)
	{
		return false;
	}
	const UWorld* World = GetWorld();
	return World && (World->GetTimeSeconds() - TargetAcquiredSeconds) >= CurrentReactionSeconds;
}

void ABNBotController::NotifyTargetUnreachable(AActor* Target)
{
	if (!Target)
	{
		return;
	}

	const UWorld* World = GetWorld();
	UnreachableActor = Target;
	UnreachableUntilSeconds = World ? World->GetTimeSeconds() + UnreachableForgetSeconds : -1.0;

	// Legibility (§1 lesson 3): giving up is a decision the founder can read, so it says so once.
	UE_LOG(LogBN, Log, TEXT("BNBots: %s cannot reach %s — dropping it for %.0fs and finding something else to do."),
		*GetNameSafe(GetPawn()), *GetNameSafe(Target), UnreachableForgetSeconds);

	// Clearing the slot is what actually frees the bot: the brain rescores, Fight loses its
	// target consideration, and Roam or Search wins instead.
	if (TargetEnemy.Get() == Target)
	{
		ClearCurrentTarget();
	}
}

FVector ABNBotController::GetLastKnownThreatLocation() const
{
	return LastKnownThreatLocation;
}

bool ABNBotController::HasFreshLastKnownLocation() const
{
	// FLEEING IS NOT SEARCHING (R9). Survive blanks GetCurrentTarget by design, so the test below
	// cannot tell "no target because I lost them" from "no target because I am running" — and
	// without this a hurt bot walked back toward the thing it was escaping. Roam already flips to
	// the point of interest FARTHEST from the threat when Surviving; Search simply never got told.
	// FLEEING REQUIRES SOMETHING TO FLEE FROM. The Survive check above is right when a bot is
	// running from a live threat; it was wrong for the far more common case. Survive also wins on
	// UTILITY below ~33% health with NO target at all, and BN has no health regeneration — so a
	// bot that dropped under a third health had Search switched off for THE REST OF ITS LIFE and
	// could never hunt a last-known again. Requiring a raw threat scopes the R9 fix to the case
	// it was written for.
	if (Brain && Brain->GetAmbition() == EBNBotAmbition::Survive && GetThreat() != nullptr)
	{
		return false;
	}

	// A live target outranks a memory of one — searching while looking at the enemy is nonsense.
	if (LastKnownThreatSeconds < 0.0 || GetCurrentTarget() != nullptr)
	{
		return false;
	}
	const UWorld* World = GetWorld();
	return World && (World->GetTimeSeconds() - LastKnownThreatSeconds) <= LastKnownFreshSeconds;
}

void ABNBotController::OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	// R10 — A NOISE IS NOT A SIGHTING. Hearing gives a bot a PLACE to look, never a target: a
	// bot that acquired you through a wall because you fired would be omniscient, and it would
	// also skip the reaction window that makes a firefight readable. The rest of this function
	// is the sight path and stays exactly as it was.
	if (Stimulus.Type == UAISense::GetSenseID<UAISense_Hearing>())
	{
		if (Stimulus.WasSuccessfullySensed() && IsValidTarget(Actor) && !GetThreat())
		{
			// StimulusLocation is where the NOISE was, which is the honest thing to walk to —
			// not the actor's position now, which the bot has no way of knowing.
			RememberThreatAt(Stimulus.StimulusLocation);
			UE_LOG(LogBN, Verbose, TEXT("BNBots: %s heard something at %s and will go looking."),
				*GetNameSafe(GetPawn()), *Stimulus.StimulusLocation.ToCompactString());
		}
		return;
	}

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
	ABNCharacter* TargetCharacter = Cast<ABNCharacter>(Actor);
	if (!TargetCharacter || TargetCharacter == GetPawn())
	{
		return false;
	}

	// TEAMS (BN15): a teammate is never a target, decided HERE because this is the single
	// function that says what a target is — perception, the slot rule, the fallback sweep
	// and the revenge memory all inherit it and cannot disagree. In FFA every other pawn
	// still answers Hostile (the NoTeam guard), so a teamless match is unchanged.
	if (GetTeamAttitudeTowards(*TargetCharacter) != ETeamAttitude::Hostile)
	{
		return false;
	}

	// The unreachable blacklist lives HERE and nowhere else — this is the single function that
	// decides what counts as a target, so perception, the slot rule and the tree all inherit it
	// for free and cannot disagree with each other.
	if (UnreachableActor.Get() == Actor)
	{
		const UWorld* World = GetWorld();
		if (World && World->GetTimeSeconds() < UnreachableUntilSeconds)
		{
			return false;
		}
	}

	// Alive means the ASC says so — no ASC yet means not a target yet, never a guess.
	const UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetCharacter);
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
	// ONE health function (R10): the cover condition asks the same question, and two ways of
	// computing "how hurt am I" would disagree on exactly the frame that decides a behaviour.
	Facts.HealthNorm = GetHealthNorm();

	// Distance is normalised against THIS BOT'S sight, which is now the tier's — a Recruit and a
	// Spartan standing the same distance away are not equally close to their own horizon.
	const APawn* MyPawn = GetPawn();
	const float TierSight = GetTuning().SightRadius;
	if (Threat && MyPawn && TierSight > 0.f)
	{
		Facts.DistToTargetNorm = FMath::Clamp(FVector::Dist(MyPawn->GetActorLocation(), Threat->GetActorLocation()) / TierSight, 0.f, 1.f);
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
	if (!bDamaged)
	{
		return;
	}

	// R9 — WHO HIT ME. Until now a bot shot in the back re-scored its ambition and did nothing
	// else: you could empty a magazine into it and it would never turn around. The attacker is
	// already recorded — FBNLastDamage is captured at the one reaction point every damage passes
	// through, on the authority, which is where bots live.
	//
	// It becomes a MEMORY, not a target. Being hit should make a bot go and look; acquiring a
	// perfect lock on someone it has never seen would be omniscience, and it would also skip the
	// reaction window that makes a firefight readable. Perception still has to do the seeing.
	if (const UBNAbilitySystemComponent* ASC = GetBotASC())
	{
		if (const UBNAttributeSet* Attributes = ASC->GetSet<UBNAttributeSet>())
		{
			const AActor* Attacker = Attributes->GetLastDamage().Instigator.Get();
			// Not myself, not the thing I am already looking at, and only something that would
			// have been a legal target anyway — IsValidTarget is the one rule for that.
			if (Attacker && Attacker != GetPawn() && Attacker != GetThreat()
				&& IsValidTarget(const_cast<AActor*>(Attacker)))
			{
				RememberThreatAt(Attacker->GetActorLocation());
				UE_LOG(LogBN, Verbose, TEXT("BNBots: %s was hit by %s and will go looking."),
					*GetNameSafe(GetPawn()), *GetNameSafe(Attacker));
			}
		}
	}

	RescoreBrain();
}

FBNBotTuningRow ABNBotController::DefaultTuning(FName TierName)
{
	// The four tiers, and what a tier actually IS: reaction, aim, awareness and movement moved
	// TOGETHER. Halo's own lesson — a Recruit that only aims worse reads as a broken Spartan.
	FBNBotTuningRow Row;

	if (TierName == FName(TEXT("Recruit")))
	{
		// Sees you late, waits a long beat, sprays, and stands still to trade. This is the tier a
		// player who has never held a controller can beat, and it must be beatable on purpose.
		Row.ReactionSecondsMin = 0.60f;
		Row.ReactionSecondsMax = 1.10f;
		Row.AimErrorDegrees = 7.f;
		Row.ReaimSeconds = 0.9f;
		Row.SightRadius = 900.f;
		Row.LoseSightRadius = 1200.f;
		Row.PeripheralVisionAngleDegrees = 55.f;
		Row.JumpCooldownSeconds = 6.f;
		Row.StrafeIntervalSeconds = 2.2f;
		Row.JukeEveryNthStep = 0;   // never jumps in a fight
		Row.bEvadesBlasts = false;  // and you can catch it with a grenade
		Row.HearingRange = 1600.f;  // still hears further than it sees (900), per R10.2
	}
	else if (TierName == FName(TEXT("ODST")))
	{
		Row.ReactionSecondsMin = 0.14f;
		Row.ReactionSecondsMax = 0.28f;
		Row.AimErrorDegrees = 1.4f;
		Row.ReaimSeconds = 0.35f;
		Row.SightRadius = 1500.f;
		Row.LoseSightRadius = 1900.f;
		Row.PeripheralVisionAngleDegrees = 85.f;
		Row.JumpCooldownSeconds = 1.2f;
		Row.StrafeIntervalSeconds = 0.9f;
		Row.JukeEveryNthStep = 2;
		Row.HearingRange = 2700.f;
	}
	else if (TierName == FName(TEXT("Spartan")))
	{
		// The ceiling. Aim error is deliberately NOT zero: a bot that never misses is not hard,
		// it is unfair, and the thing that makes a Spartan hard is the reaction and the footwork.
		Row.ReactionSecondsMin = 0.08f;
		Row.ReactionSecondsMax = 0.16f;
		Row.AimErrorDegrees = 0.6f;
		Row.ReaimSeconds = 0.2f;
		Row.SightRadius = 1800.f;
		Row.LoseSightRadius = 2200.f;
		Row.PeripheralVisionAngleDegrees = 100.f;
		Row.JumpCooldownSeconds = 0.9f;
		Row.StrafeIntervalSeconds = 0.7f;
		Row.JukeEveryNthStep = 2;
		// R10.2 SAYS HEARING IS LONGER THAN SIGHT, and until now it was only true by accident:
		// HearingRange was never set per tier, so every tier inherited the struct's 2200. That
		// left Recruit hearing 1300 further than it sees and a Spartan only 400 - the top tier
		// was the DEAFEST relative to its own eyes, which is backwards. Now scaled with sight.
		Row.HearingRange = 3200.f;
	}
	// Marine is the struct's own defaults — the founder's tuned arena numbers — so the default
	// tier changes NOTHING about how today's bots behave. Every other tier is scaled around those
	// rather than around the engine defaults they replaced: see FBNBotTuningRow's sight comment.

	return Row;
}

void ABNBotController::ResolveTuning()
{
	FName TierName = BotTier.IsNone() ? FName(TEXT("Marine")) : BotTier;

	static const FName ValidTiers[] = { FName(TEXT("Recruit")), FName(TEXT("Marine")), FName(TEXT("ODST")), FName(TEXT("Spartan")) };
	bool bKnownTier = false;
	for (const FName& Valid : ValidTiers)
	{
		bKnownTier = bKnownTier || (Valid == TierName);
	}
	if (!bKnownTier)
	{
		UE_LOG(LogBN, Warning, TEXT("BNBotController: BotTier '%s' is not one of Recruit/Marine/ODST/Spartan — falling back to Marine."),
			*TierName.ToString());
		TierName = FName(TEXT("Marine"));
	}

	// C++ first, table second: the row on disk OVERRIDES, it never has to exist. A project with
	// no DT_BNBotTuning still has four working tiers, which is the same contract the ambition
	// rows keep.
	Tuning = MakeShared<FBNBotTuningRow>(DefaultTuning(TierName));

	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	if (const UBNGameData* GameData = GameInstance ? GameInstance->GetSubsystem<UBNGameData>() : nullptr)
	{
		if (const FBNBotTuningRow* TableRow = GameData->FindBotTuningRow(TierName))
		{
			*Tuning = *TableRow;
		}
	}

	// The sense is the one thing applied rather than read: perception caches its config, so a
	// value changed after ConfigureSense is a value nothing ever reads.
	if (SightConfig && BotPerception)
	{
		SightConfig->SightRadius = Tuning->SightRadius;
		SightConfig->LoseSightRadius = Tuning->LoseSightRadius;
		SightConfig->PeripheralVisionAngleDegrees = Tuning->PeripheralVisionAngleDegrees;
		BotPerception->ConfigureSense(*SightConfig);
	}
	if (HearingConfig && BotPerception)
	{
		// Zero is a REAL setting, not a missing one: a tier with no ears is a tier a player can
		// flank, and Recruit is supposed to be flankable.
		HearingConfig->HearingRange = Tuning->HearingRange;
		BotPerception->ConfigureSense(*HearingConfig);
	}
	if (BotPerception)
	{
		BotPerception->RequestStimuliListenerUpdate();
	}

	UE_LOG(LogBN, Log, TEXT("BNBots: %s fights at tier %s (reaction %.2f-%.2fs, aim ±%.1f°, sight %.0fuu)."),
		*GetNameSafe(GetPawn()), *TierName.ToString(),
		Tuning->ReactionSecondsMin, Tuning->ReactionSecondsMax, Tuning->AimErrorDegrees, Tuning->SightRadius);
}

const FBNBotTuningRow& ABNBotController::GetTuning() const
{
	// Never null after OnPossess, and a static fallback rather than a crash for the window before
	// it: a task that asks early gets Marine, which is the shipped behaviour.
	static const FBNBotTuningRow Fallback;
	return Tuning.IsValid() ? *Tuning : Fallback;
}

void ABNBotController::NotifyIncomingBlast(const FVector& Center, double DetonateAtSeconds, float BlastRadius)
{
	// TIERED: a Recruit does not dodge grenades, and that is the tier doing its job rather than a
	// bot failing at one. It is also Halo's own shape — the low tiers are the ones you can catch
	// with a grenade, and taking that away is taking away the tier.
	if (!GetTuning().bEvadesBlasts)
	{
		return;
	}

	// The SOONEST warning wins. Two grenades means the one about to go off is the emergency, and
	// a later one must not push the deadline out from under a bot already running.
	if (IncomingBlastAtSeconds > 0.0 && DetonateAtSeconds > IncomingBlastAtSeconds)
	{
		const UWorld* World = GetWorld();
		if (World && World->GetTimeSeconds() < IncomingBlastAtSeconds)
		{
			return;
		}
	}

	IncomingBlastCenter = Center;
	IncomingBlastAtSeconds = DetonateAtSeconds;
	IncomingBlastRadius = FMath::Max(0.f, BlastRadius);

	// Start R11's clock, and DRAW a fresh reaction for it. Without the draw a bot would inherit
	// whatever reaction its last sighting happened to roll, which is not this event's.
	if (const UWorld* NoticeWorld = GetWorld())
	{
		IncomingBlastNoticedSeconds = NoticeWorld->GetTimeSeconds();
		CurrentReactionSeconds = DrawReactionSeconds();
	}

	UE_LOG(LogBN, Verbose, TEXT("BNBots: %s sees a grenade about to go off %.0fuu away."),
		*GetNameSafe(GetPawn()),
		GetPawn() ? FVector::Dist(GetPawn()->GetActorLocation(), Center) : 0.f);
}

bool ABNBotController::HasIncomingBlast(FVector& OutCenter, float& OutRadius) const
{
	const UWorld* World = GetWorld();
	if (!World || IncomingBlastAtSeconds <= 0.0 || World->GetTimeSeconds() >= IncomingBlastAtSeconds)
	{
		return false;
	}
	OutCenter = IncomingBlastCenter;
	OutRadius = IncomingBlastRadius;
	return true;
}

float ABNBotController::GetHealthNorm() const
{
	const UBNAbilitySystemComponent* ASC = GetBotASC();
	if (!ASC)
	{
		return 1.f;
	}
	const float MaxHealth = ASC->GetNumericAttribute(UBNAttributeSet::GetMaxHealthAttribute());
	if (MaxHealth <= 0.f)
	{
		// No denominator is not "nearly dead" — the honest-unknown rule the HUD keeps, applied to
		// a bot's own body. A bot that read 0 here would dive for cover on the frame it spawned.
		return 1.f;
	}
	return FMath::Clamp(ASC->GetNumericAttribute(UBNAttributeSet::GetHealthAttribute()) / MaxHealth, 0.f, 1.f);
}

bool ABNBotController::CanTakeCoverNow() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimeSeconds() >= NextCoverAllowedSeconds;
}

void ABNBotController::NotifyTookCover()
{
	if (const UWorld* World = GetWorld())
	{
		NextCoverAllowedSeconds = World->GetTimeSeconds() + FMath::Max(0.f, CoverCooldownSeconds);
	}
}

bool ABNBotController::TryJump()
{
	const UWorld* World = GetWorld();
	const ACharacter* Character = Cast<ACharacter>(GetPawn());
	const UCharacterMovementComponent* MoveComp = Character ? Character->GetCharacterMovement() : nullptr;
	if (!World || !MoveComp)
	{
		return false;
	}

	// Already airborne: BN gives humans no double jump, and a bot that pressed again mid-flight
	// would just spend the cooldown for nothing.
	if (MoveComp->IsFalling())
	{
		return false;
	}
	if (World->GetTimeSeconds() < NextJumpAllowedSeconds)
	{
		return false;
	}

	// The SAME press a spacebar makes. Not Character->Jump(): that would be a second movement path
	// with no ability, no State.Movement.Jumping tag and no landing handling — the exact split
	// this controller exists to avoid.
	PressInputTag(BNTags::Input_Jump);
	ReleaseInputTag(BNTags::Input_Jump);

	NextJumpAllowedSeconds = World->GetTimeSeconds() + FMath::Max(0.f, GetTuning().JumpCooldownSeconds);
	return true;
}

void ABNBotController::RememberThreatAt(const FVector& Where)
{
	const UWorld* World = GetWorld();
	LastKnownThreatLocation = Where;
	LastKnownThreatSeconds = World ? World->GetTimeSeconds() : -1.0;
}
