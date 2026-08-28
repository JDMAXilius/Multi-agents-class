#include "QA/BNAdversarialAgent.h"

#include "AbilitySystem/Attributes/BNAttributeSet.h"
#include "AbilitySystem/BNAbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Characters/BNCharacter.h"
#include "Core/BNGameplayTags.h"
#include "Dom/JsonObject.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameMode.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/IConsoleManager.h"
#include "Match/BNGameMode.h"
#include "Match/BNPlayerState.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "TimerManager.h"
#include "Weapons/BNEquipmentComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogBNAQA, Log, All);

namespace
{
	// The loop's three cadences. Behavior long enough to develop pressure, act fast enough
	// to be abusive, probe fast enough that a 100ms discontinuity cannot hide between samples.
	constexpr float BehaviorSeconds = 12.f;
	constexpr float ActSeconds = 0.25f;
	constexpr float ProbeSeconds = 0.1f;

	// Detector thresholds. Each states its excuse policy in the detector that uses it.
	constexpr float BelowKillZGraceS = 1.0f;    // KillZ should destroy within a frame or two
	constexpr float StuckAfterS = 3.0f;         // commanded + alive + unfrozen + still, this long
	constexpr float StuckSpeedUU = 10.f;
	constexpr float SpeedTolerance = 1.75f;     // ground speed past MaxWalkSpeed * this = cheat
	constexpr float TeleportUUPerSample = 1200.f; // 12,000 uu/s — beyond any legal mover here
	constexpr float HullMarginUU = 4000.f;      // outside PlayerStart hull + this = escaped

	const TCHAR* BehaviorName(EBNAQABehavior B)
	{
		switch (B)
		{
		case EBNAQABehavior::Roam:          return TEXT("roam");
		case EBNAQABehavior::BoundaryProbe: return TEXT("boundary_probe");
		case EBNAQABehavior::LedgeDive:     return TEXT("ledge_dive");
		case EBNAQABehavior::GrappleAbuse:  return TEXT("grapple_abuse");
		case EBNAQABehavior::AbilityMash:   return TEXT("ability_mash");
		default:                            return TEXT("unknown");
		}
	}
}

ABNAQAController::ABNAQAController()
{
	// A PlayerState so the probe walks the SAME init path a bot does — team assignment,
	// the freeze GE, the death subscription, the respawn loop. A probe that skips the
	// doors is not testing them.
	bWantsPlayerState = true;
}

// ======================================================================================
// console entry — the only way a probe ever exists
// ======================================================================================

static void BNAQA_Start(const TArray<FString>& Args, UWorld* World)
{
	if (!World || World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogBNAQA, Warning, TEXT("bn.aqa.start: authority worlds only — the probe presses server-gated paths."));
		return;
	}
	for (TActorIterator<ABNAQAController> It(World); It; ++It)
	{
		UE_LOG(LogBNAQA, Warning, TEXT("bn.aqa.start: a probe is already running — bn.aqa.stop it first."));
		return;
	}
	ABNGameMode* GM = World->GetAuthGameMode<ABNGameMode>();
	if (!GM)
	{
		UE_LOG(LogBNAQA, Warning, TEXT("bn.aqa.start: no ABNGameMode in this world."));
		return;
	}

	const float Seconds = (Args.Num() > 0) ? FCString::Atof(*Args[0]) : 180.f;

	// SpawnBot's exact recipe (BNGameMode.cpp), minus the mode's bookkeeping: transient
	// controller, named PlayerState, the ONE init seam, then the mode's own spawn path.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;
	ABNAQAController* Probe = World->SpawnActor<ABNAQAController>(
		ABNAQAController::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
	if (!Probe)
	{
		UE_LOG(LogBNAQA, Error, TEXT("bn.aqa.start: SpawnActor failed."));
		return;
	}
	if (APlayerState* PS = Probe->GetPlayerState<APlayerState>())
	{
		PS->SetPlayerName(TEXT("AQA-Probe"));
	}
	GM->GenericPlayerInitialization(Probe);
	GM->RestartPlayer(Probe);
	Probe->StartRun(Seconds > 1.f ? Seconds : 180.f);
}

static void BNAQA_Stop(const TArray<FString>& Args, UWorld* World)
{
	if (!World)
	{
		return;
	}
	for (TActorIterator<ABNAQAController> It(World); It; ++It)
	{
		It->StopRun(TEXT("bn.aqa.stop"));
		return;
	}
	UE_LOG(LogBNAQA, Warning, TEXT("bn.aqa.stop: no probe running."));
}

static FAutoConsoleCommandWithWorldAndArgs GBNAQAStartCmd(
	TEXT("bn.aqa.start"),
	TEXT("Spawn the adversarial QA probe into the match. Arg: run seconds (default 180). Report lands in Saved/AdversarialQA/."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&BNAQA_Start));

static FAutoConsoleCommandWithWorldAndArgs GBNAQAStopCmd(
	TEXT("bn.aqa.stop"),
	TEXT("End the adversarial QA run early. The report writes on any stop."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&BNAQA_Stop));

// ======================================================================================
// run lifecycle
// ======================================================================================

void ABNAQAController::StartRun(float DurationSeconds)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	StartedUtc = FDateTime::UtcNow();
	StartedSeconds = World->GetTimeSeconds();
	PlannedDuration = DurationSeconds;

	// The arena's intended space is derived from data the level already carries: the hull
	// of its PlayerStarts. Standing OUTSIDE hull+margin means the probe left the space the
	// designers meant players to reach — a heuristic, and the report says so per finding.
	ArenaHull = FBox(ForceInit);
	Anchors.Reset();
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		ArenaHull += It->GetActorLocation();
		Anchors.Add(*It);
	}

	FTimerManager& TM = World->GetTimerManager();
	TM.SetTimer(BehaviorTimer, this, &ABNAQAController::AdvanceBehavior, BehaviorSeconds, true);
	TM.SetTimer(ActTimer, this, &ABNAQAController::ActTick, ActSeconds, true);
	TM.SetTimer(ProbeTimer, this, &ABNAQAController::ProbeTick, ProbeSeconds, true);
	TM.SetTimer(EndTimer, FTimerDelegate::CreateUObject(this, &ABNAQAController::StopRun,
		FString(TEXT("duration elapsed"))), DurationSeconds, false);

	UE_LOG(LogBNAQA, Log, TEXT("AQA run started: %.0fs, %d anchors, hull %s."),
		DurationSeconds, Anchors.Num(), *ArenaHull.ToString());
}

void ABNAQAController::StopRun(const FString& Reason)
{
	UWorld* World = GetWorld();
	if (!World || !EndTimer.IsValid())
	{
		return; // already stopped, or never started
	}
	FTimerManager& TM = World->GetTimerManager();
	TM.ClearTimer(BehaviorTimer);
	TM.ClearTimer(ActTimer);
	TM.ClearTimer(ProbeTimer);
	TM.ClearTimer(EndTimer);
	EndTimer.Invalidate();

	WriteReport();
	UE_LOG(LogBNAQA, Log, TEXT("AQA run stopped (%s): %d finding class(es), %d behavior cycles, %d presses, %d moves, %d deaths."),
		*Reason, Findings.Num(), BehaviorCycles, Presses, MoveRequests, Deaths);

	// DespawnBot's proven order: pawn first (unpossession unhooks delegates), then the
	// controller, whose destruction retires its PlayerState from every roster.
	if (APawn* P = GetPawn())
	{
		UnPossess();
		P->Destroy();
	}
	Destroy();
}

void ABNAQAController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// Fresh body: never compare a teleport across a respawn — that discontinuity is legal.
	bHasLastSample = false;
	StillSince = -1.0;
	BelowKillZSince = -1.0;

	// One ear on the ASC for the whole run: the PlayerState (and its ASC) persists across
	// respawns, so the binding survives every body this controller wears.
	if (!bASCBound)
	{
		if (UBNAbilitySystemComponent* ASC = GetASC())
		{
			ASC->AbilityActivatedCallbacks.AddUObject(this, &ABNAQAController::OnAbilityActivated);
			bASCBound = true;
		}
	}
}

// ======================================================================================
// the loop
// ======================================================================================

void ABNAQAController::AdvanceBehavior()
{
	Behavior = static_cast<EBNAQABehavior>((static_cast<uint8>(Behavior) + 1)
		% static_cast<uint8>(EBNAQABehavior::COUNT));
	++BehaviorCycles;
	++HeadingIndex;
	StopMovement();
	StillSince = -1.0;
	UE_LOG(LogBNAQA, Verbose, TEXT("AQA behavior -> %s"), BehaviorName(Behavior));
}

void ABNAQAController::ActTick()
{
	ACharacter* Char = Cast<ACharacter>(GetPawn());
	UBNAbilitySystemComponent* ASC = GetASC();
	if (!Char || !ASC)
	{
		return;
	}

	// Opportunistic modes outrank the cycle: a dead or frozen pawn's job is to press
	// anyway — the whole point is leaning on the State.Dead / Match.Frozen gates.
	if (ASC->HasMatchingGameplayTag(BNTags::State_Dead)
		|| ASC->HasMatchingGameplayTag(BNTags::State_Match_Frozen))
	{
		MashAllVerbs();
		return;
	}

	// Spawns are empty-handed by design (list order law: index 0 is the null Unarmed
	// slot) — cycle to a real weapon so the fire pressure means something.
	const ABNCharacter* BNChar = Cast<ABNCharacter>(Char);
	const UBNEquipmentComponent* Equipment = BNChar ? BNChar->GetEquipmentComponent() : nullptr;
	if (Equipment && Equipment->GetCurrentWeapon() == nullptr)
	{
		PressAndRelease(BNTags::Input_Weapon_Next);
	}

	const FVector Here = Char->GetActorLocation();
	switch (Behavior)
	{
	case EBNAQABehavior::Roam:
	{
		if (GetMoveStatus() != EPathFollowingStatus::Moving)
		{
			UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
			FNavLocation Target;
			if (Nav && Nav->GetRandomReachablePointInRadius(Here, 3000.f, Target))
			{
				MoveToLocation(Target.Location, 80.f);
				++MoveRequests;
			}
		}
		break;
	}
	case EBNAQABehavior::BoundaryProbe:
	{
		// Un-pathed, straight into whatever blocks: 8 compass headings swept in turn,
		// jumping at the obstacle — the classic wall-climb / seam-slip hunt.
		const float Angle = (HeadingIndex % 8) * PI / 4.f;
		const FVector Dir(FMath::Cos(Angle), FMath::Sin(Angle), 0.f);
		MoveToLocation(Here + Dir * 10000.f, 50.f, /*bStopOnOverlap*/ true,
			/*bUsePathfinding*/ false, /*bProjectDestinationToNavigation*/ false);
		++MoveRequests;
		PressAndRelease(BNTags::Input_Jump);
		break;
	}
	case EBNAQABehavior::LedgeDive:
	{
		// Sprint straight at the hull's farthest face and off any edge on the way.
		const FVector Out = ArenaHull.IsValid
			? (Here - ArenaHull.GetCenter()).GetSafeNormal2D()
			: FVector::ForwardVector;
		Press(BNTags::Input_Sprint);
		MoveToLocation(Here + Out * 12000.f, 50.f, true, /*bUsePathfinding*/ false, false);
		++MoveRequests;
		PressAndRelease(BNTags::Input_Jump);
		break;
	}
	case EBNAQABehavior::GrappleAbuse:
	{
		// Aim somewhere a designer never intended a hook: straight up, straight down,
		// the horizon at random yaw — then fire it and jump mid-flight.
		const int32 Mode = HeadingIndex % 3;
		const float Pitch = (Mode == 0) ? 89.f : (Mode == 1) ? -89.f : 0.f;
		SetControlRotation(FRotator(Pitch, FMath::FRandRange(0.f, 360.f), 0.f));
		++HeadingIndex;
		PressAndRelease(BNTags::Input_Grapple);
		PressAndRelease(BNTags::Input_Jump);
		break;
	}
	case EBNAQABehavior::AbilityMash:
	default:
		MashAllVerbs();
		break;
	}
}

void ABNAQAController::MashAllVerbs()
{
	// Every input tag the game accepts, at 4 Hz, deliberately mis-combined: fire while
	// sprinting into ADS, crouch strobing, weapon cycling mid-reload, grapple on cooldown.
	// Held verbs (fire, sprint, ADS) are re-pressed without release — sustained illegal
	// pressure; the toggles get clean press/release pairs.
	Press(BNTags::Input_Weapon_Fire);
	Press(BNTags::Input_Sprint);
	Press(BNTags::Input_Weapon_ADS);
	PressAndRelease(BNTags::Input_Jump);
	PressAndRelease(BNTags::Input_Crouch);
	PressAndRelease(BNTags::Input_Melee);
	PressAndRelease(BNTags::Input_Grenade);
	PressAndRelease(BNTags::Input_Weapon_Reload);
	PressAndRelease(BNTags::Input_Weapon_Next);
	PressAndRelease(BNTags::Input_Grapple);
}

void ABNAQAController::Press(FGameplayTag InputTag)
{
	if (UBNAbilitySystemComponent* ASC = GetASC())
	{
		ASC->AbilityInputTagPressed(InputTag);
		++Presses;
	}
}

void ABNAQAController::PressAndRelease(FGameplayTag InputTag)
{
	if (UBNAbilitySystemComponent* ASC = GetASC())
	{
		ASC->AbilityInputTagPressed(InputTag);
		ASC->AbilityInputTagReleased(InputTag);
		++Presses;
	}
}

UBNAbilitySystemComponent* ABNAQAController::GetASC() const
{
	const ABNPlayerState* PS = GetPlayerState<ABNPlayerState>();
	return PS ? PS->GetBNAbilitySystemComponent() : nullptr;
}

// ======================================================================================
// detectors
// ======================================================================================

void ABNAQAController::ProbeTick()
{
	UWorld* World = GetWorld();
	if (World)
	{
		if (const AGameMode* GM = World->GetAuthGameMode<AGameMode>())
		{
			MatchStatesSeen.Add(GM->GetMatchState().ToString());
		}
	}

	ACharacter* Char = Cast<ACharacter>(GetPawn());
	UBNAbilitySystemComponent* ASC = GetASC();
	if (!World || !Char || !ASC)
	{
		bHasLastSample = false;
		return;
	}
	const UCharacterMovementComponent* CMC = Char->GetCharacterMovement();
	const FVector Here = Char->GetActorLocation();
	const double Now = World->GetTimeSeconds();
	const bool bAlive = !ASC->HasMatchingGameplayTag(BNTags::State_Dead);
	const bool bFrozen = ASC->HasMatchingGameplayTag(BNTags::State_Match_Frozen);
	const float Speed2D = CMC ? CMC->Velocity.Size2D() : 0.f;

	// death bookkeeping (context, not a defect — dying is the game working)
	if (bWasAlive && !bAlive)
	{
		++Deaths;
	}
	bWasAlive = bAlive;

	// 1. fell_out_of_world_alive — KillZ exists to destroy; surviving below it is a hole.
	const float KillZ = World->GetWorldSettings() ? World->GetWorldSettings()->KillZ : -1.0e8f;
	if (bAlive && Here.Z < KillZ)
	{
		if (BelowKillZSince < 0.0)
		{
			BelowKillZSince = Now;
		}
		else if (Now - BelowKillZSince > BelowKillZGraceS)
		{
			RecordFinding(TEXT("fell_out_of_world_alive"), TEXT("high"), FString::Printf(
				TEXT("alive %.1fs below KillZ (z=%.0f, KillZ=%.0f) — the kill volume never fired"),
				Now - BelowKillZSince, Here.Z, KillZ));
		}
	}
	else
	{
		BelowKillZSince = -1.0;
	}

	// 2. escaped_playable_space — grounded outside the PlayerStart hull + margin.
	//    Heuristic bounds (the evidence says so): falling past the edge is KillZ's case;
	//    STANDING out there means walkable geometry exists outside the intended arena.
	if (ArenaHull.IsValid && bAlive && CMC && CMC->IsMovingOnGround())
	{
		const FBox Expanded = ArenaHull.ExpandBy(FVector(HullMarginUU, HullMarginUU, 10000.f));
		if (!Expanded.IsInside(Here))
		{
			RecordFinding(TEXT("escaped_playable_space"), TEXT("high"), FString::Printf(
				TEXT("standing on ground at (%.0f, %.0f, %.0f), outside the PlayerStart hull + %.0fuu margin (heuristic bounds)"),
				Here.X, Here.Y, Here.Z, HullMarginUU));
		}
	}

	// 3. stuck_state — the game accepted a move and the body goes nowhere. Frozen is
	//    excused (the freeze is SUPPOSED to pin us); so is having no live move request.
	const bool bCommanded = GetMoveStatus() == EPathFollowingStatus::Moving;
	if (bAlive && !bFrozen && bCommanded && Speed2D < StuckSpeedUU)
	{
		if (StillSince < 0.0)
		{
			StillSince = Now;
		}
		else if (Now - StillSince > StuckAfterS)
		{
			RecordFinding(TEXT("stuck_state"), TEXT("medium"), FString::Printf(
				TEXT("move request active %.1fs with speed %.1f uu/s during %s — pawn pinned by geometry or pathing"),
				Now - StillSince, Speed2D, BehaviorName(Behavior)));
			StillSince = Now; // re-arm so one long pin logs occurrences, not one per sample
		}
	}
	else
	{
		StillSince = -1.0;
	}

	// 4. speed_violation — ground speed far past what CMC says this body can do. Falling
	//    and flying (the grapple's regime) are excluded: gravity and root motion have
	//    their own legal envelopes; WALKING faster than the movement model is the cheat.
	if (CMC && CMC->IsMovingOnGround() && CMC->MaxWalkSpeed > 0.f
		&& Speed2D > CMC->MaxWalkSpeed * SpeedTolerance)
	{
		RecordFinding(TEXT("speed_violation"), TEXT("medium"), FString::Printf(
			TEXT("ground speed %.0f uu/s vs MaxWalkSpeed %.0f (x%.2f) during %s"),
			Speed2D, CMC->MaxWalkSpeed, Speed2D / CMC->MaxWalkSpeed, BehaviorName(Behavior)));
	}

	// 5. attribute_anomaly — the attribute set's own invariants: finite, non-negative,
	//    never above its max. GAS purity says only GEs mutate these; this checks they
	//    never mutated past their own rails.
	const float Health = ASC->GetNumericAttribute(UBNAttributeSet::GetHealthAttribute());
	const float MaxHealth = ASC->GetNumericAttribute(UBNAttributeSet::GetMaxHealthAttribute());
	const float Shield = ASC->GetNumericAttribute(UBNAttributeSet::GetShieldAttribute());
	const float MaxShield = ASC->GetNumericAttribute(UBNAttributeSet::GetMaxShieldAttribute());
	if (FMath::IsNaN(Health) || FMath::IsNaN(Shield)
		|| Health < -KINDA_SMALL_NUMBER || Shield < -KINDA_SMALL_NUMBER
		|| (MaxHealth > 0.f && Health > MaxHealth + 1.f)
		|| (MaxShield > 0.f && Shield > MaxShield + 1.f))
	{
		RecordFinding(TEXT("attribute_anomaly"), TEXT("high"), FString::Printf(
			TEXT("health %.2f / max %.2f, shield %.2f / max %.2f — outside the attribute rails"),
			Health, MaxHealth, Shield, MaxShield));
	}

	// 6. teleport_discontinuity — more distance in one 100ms sample than any legal mover
	//    covers. Respawns never trip it: OnPossess resets the sample chain.
	if (bHasLastSample)
	{
		const double Step = FVector::Dist(Here, LastSampleLocation);
		TravelledUU += Step;
		if (Step > TeleportUUPerSample)
		{
			RecordFinding(TEXT("teleport_discontinuity"), TEXT("medium"), FString::Printf(
				TEXT("%.0f uu in one %.0fms sample (%.0f uu/s) during %s"),
				Step, ProbeSeconds * 1000.f, Step / ProbeSeconds, BehaviorName(Behavior)));
		}
	}
	LastSampleLocation = Here;
	bHasLastSample = true;
}

void ABNAQAController::OnAbilityActivated(UGameplayAbility* Ability)
{
	// Detector 7. The gates this probe leans on all run: State.Dead and Match.Frozen are
	// supposed to refuse EVERY activation (UBNGameplayAbility::CanActivateAbility). An
	// activation that lands while either tag is on the ASC is the gate giving way.
	const UBNAbilitySystemComponent* ASC = GetASC();
	if (!ASC || !Ability)
	{
		return;
	}
	if (ASC->HasMatchingGameplayTag(BNTags::State_Dead))
	{
		RecordFinding(TEXT("acted_while_dead"), TEXT("high"), FString::Printf(
			TEXT("%s activated while State.Dead was on the ASC"), *Ability->GetClass()->GetName()));
	}
	else if (ASC->HasMatchingGameplayTag(BNTags::State_Match_Frozen))
	{
		RecordFinding(TEXT("input_during_freeze"), TEXT("high"), FString::Printf(
			TEXT("%s activated while State.Match.Frozen was on the ASC"), *Ability->GetClass()->GetName()));
	}
}

// ======================================================================================
// recording + the report
// ======================================================================================

void ABNAQAController::RecordFinding(const FString& ErrorType, const FString& Severity,
	const FString& Evidence)
{
	const APawn* P = GetPawn();
	const FVector At = P ? P->GetActorLocation() : FVector::ZeroVector;

	// Dedup by class + 2000uu cell: one bad ledge logs once with a count, not 600 times
	// at 10 Hz. First occurrence keeps its evidence and context — the freshest scent.
	const FString Key = FString::Printf(TEXT("%s@%d,%d,%d"), *ErrorType,
		FMath::FloorToInt(At.X / 2000.f), FMath::FloorToInt(At.Y / 2000.f),
		FMath::FloorToInt(At.Z / 2000.f));
	if (int32* Existing = FindingIndexByKey.Find(Key))
	{
		++Findings[*Existing].Occurrences;
		return;
	}

	FBNAQAFinding F;
	F.ErrorType = ErrorType;
	F.Severity = Severity;
	F.Behavior = BehaviorName(Behavior);
	F.Location = At;
	F.NearestAnchor = NearestAnchor(At);
	F.Evidence = Evidence;
	F.FirstUtc = FDateTime::UtcNow();
	if (const UWorld* World = GetWorld())
	{
		F.SimTimeS = static_cast<float>(World->GetTimeSeconds() - StartedSeconds);
		if (const AGameMode* GM = World->GetAuthGameMode<AGameMode>())
		{
			F.MatchState = GM->GetMatchState().ToString();
		}
	}
	if (const UBNAbilitySystemComponent* ASC = GetASC())
	{
		F.bAlive = !ASC->HasMatchingGameplayTag(BNTags::State_Dead);
		const float MaxHealth = ASC->GetNumericAttribute(UBNAttributeSet::GetMaxHealthAttribute());
		const float MaxShield = ASC->GetNumericAttribute(UBNAttributeSet::GetMaxShieldAttribute());
		F.HealthNorm = MaxHealth > 0.f
			? ASC->GetNumericAttribute(UBNAttributeSet::GetHealthAttribute()) / MaxHealth : 1.f;
		F.ShieldNorm = MaxShield > 0.f
			? ASC->GetNumericAttribute(UBNAttributeSet::GetShieldAttribute()) / MaxShield : 1.f;
	}
	if (const ACharacter* Char = Cast<ACharacter>(GetPawn()))
	{
		F.SpeedUU = Char->GetVelocity().Size();
	}
	FindingIndexByKey.Add(Key, Findings.Add(F));

	UE_LOG(LogBNAQA, Warning, TEXT("AQA FINDING [%s/%s] at (%.0f, %.0f, %.0f): %s"),
		*ErrorType, *Severity, At.X, At.Y, At.Z, *Evidence);
}

FString ABNAQAController::NearestAnchor(const FVector& At) const
{
	const AActor* Best = nullptr;
	double BestDist = TNumericLimits<double>::Max();
	for (const TWeakObjectPtr<AActor>& Anchor : Anchors)
	{
		if (const AActor* A = Anchor.Get())
		{
			const double D = FVector::Dist(At, A->GetActorLocation());
			if (D < BestDist)
			{
				BestDist = D;
				Best = A;
			}
		}
	}
	return Best
		? FString::Printf(TEXT("%s (%.0f uu away)"), *Best->GetName(), BestDist)
		: FString(TEXT("no PlayerStart anchors in world"));
}

void ABNAQAController::WriteReport()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("schema"), TEXT("aqa-report/1"));
	Root->SetStringField(TEXT("tool"), TEXT("BNAdversarialAgent (bn.aqa.*)"));
	Root->SetStringField(TEXT("project"), TEXT("BREACHPOINT"));
	Root->SetStringField(TEXT("map"), UWorld::RemovePIEPrefix(World->GetMapName()));
	Root->SetStringField(TEXT("net_mode"), World->GetNetMode() == NM_ListenServer
		? TEXT("listen server") : TEXT("standalone PIE"));
	Root->SetStringField(TEXT("started_utc"), StartedUtc.ToIso8601());
	Root->SetStringField(TEXT("ended_utc"), FDateTime::UtcNow().ToIso8601());
	Root->SetNumberField(TEXT("duration_s"), World->GetTimeSeconds() - StartedSeconds);
	Root->SetNumberField(TEXT("planned_duration_s"), PlannedDuration);

	TArray<TSharedPtr<FJsonValue>> BehaviorList;
	for (uint8 i = 0; i < static_cast<uint8>(EBNAQABehavior::COUNT); ++i)
	{
		BehaviorList.Add(MakeShared<FJsonValueString>(BehaviorName(static_cast<EBNAQABehavior>(i))));
	}
	Root->SetArrayField(TEXT("behaviors"), BehaviorList);
	Root->SetNumberField(TEXT("behavior_cycles"), BehaviorCycles);

	TArray<TSharedPtr<FJsonValue>> States;
	for (const FString& S : MatchStatesSeen)
	{
		States.Add(MakeShared<FJsonValueString>(S));
	}
	Root->SetArrayField(TEXT("match_states_seen"), States);

	TSharedRef<FJsonObject> Stats = MakeShared<FJsonObject>();
	Stats->SetNumberField(TEXT("ability_presses"), Presses);
	Stats->SetNumberField(TEXT("move_requests"), MoveRequests);
	Stats->SetNumberField(TEXT("deaths"), Deaths);
	Stats->SetNumberField(TEXT("distance_travelled_uu"), FMath::RoundToDouble(TravelledUU));
	Root->SetObjectField(TEXT("stats"), Stats);

	TArray<TSharedPtr<FJsonValue>> FindingList;
	for (const FBNAQAFinding& F : Findings)
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("error_type"), F.ErrorType);
		J->SetStringField(TEXT("severity"), F.Severity);
		J->SetNumberField(TEXT("occurrences"), F.Occurrences);

		TSharedRef<FJsonObject> Loc = MakeShared<FJsonObject>();
		Loc->SetNumberField(TEXT("x"), FMath::RoundToDouble(F.Location.X));
		Loc->SetNumberField(TEXT("y"), FMath::RoundToDouble(F.Location.Y));
		Loc->SetNumberField(TEXT("z"), FMath::RoundToDouble(F.Location.Z));
		J->SetObjectField(TEXT("location"), Loc);
		J->SetStringField(TEXT("nearest_anchor"), F.NearestAnchor);

		TSharedRef<FJsonObject> Ctx = MakeShared<FJsonObject>();
		Ctx->SetStringField(TEXT("behavior"), F.Behavior);
		Ctx->SetStringField(TEXT("match_state"), F.MatchState);
		Ctx->SetNumberField(TEXT("sim_time_s"), F.SimTimeS);
		Ctx->SetBoolField(TEXT("alive"), F.bAlive);
		Ctx->SetNumberField(TEXT("health_norm"), F.HealthNorm);
		Ctx->SetNumberField(TEXT("shield_norm"), F.ShieldNorm);
		Ctx->SetNumberField(TEXT("speed_uu_s"), FMath::RoundToDouble(F.SpeedUU));
		J->SetObjectField(TEXT("game_context"), Ctx);

		J->SetStringField(TEXT("evidence"), F.Evidence);
		J->SetStringField(TEXT("first_seen_utc"), F.FirstUtc.ToIso8601());
		FindingList.Add(MakeShared<FJsonValueObject>(J));
	}
	Root->SetArrayField(TEXT("findings"), FindingList);

	FString Out;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Root, Writer);

	const FString Path = FPaths::ProjectSavedDir() / TEXT("AdversarialQA")
		/ FString::Printf(TEXT("aqa_report_%s.json"), *StartedUtc.ToString(TEXT("%Y%m%d-%H%M%S")));
	if (FFileHelper::SaveStringToFile(Out, *Path))
	{
		UE_LOG(LogBNAQA, Log, TEXT("AQA report written: %s"), *Path);
	}
	else
	{
		UE_LOG(LogBNAQA, Error, TEXT("AQA report FAILED to write: %s"), *Path);
	}
}
