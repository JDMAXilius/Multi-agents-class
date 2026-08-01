// BREACHPOINT — BP05 step 2. The melee path.

#include "AbilitySystem/Abilities/BRGA_Melee.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystem/BRAbilitySystemComponent.h"
#include "AbilitySystem/BRCombatCurves.h"
#include "AbilitySystem/Effects/BRGameplayEffects.h"
#include "CollisionShape.h"
#include "Core/BRCore.h"
#include "Core/BRGameplayTags.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace
{

	/**
	 * THE FOUR CURVE NAMES THIS ABILITY READS FROM `CT_Combat`.
	 *
	 * These are DATA KEYS, not gameplay numbers — the same distinction `BRCombatCurves::Names`
	 * draws, and for the same reason: a misspelling here fails once and visibly instead of
	 * silently returning "curve not found" from one of several call sites.
	 *
	 * **THEY DO NOT EXIST IN `Content/Data/CT_Combat.csv` YET, AND NOTHING IN THIS PACKET MAY ADD
	 * THEM.** `CT_Combat.csv` is granted to BP05 by exact file, but this session's writer owns only
	 * `AbilitySystem/Abilities/` — so the four rows below are a `contract_gap` against the data
	 * crew, filed here and in the header, not a CSV edit made on the way past. The four rows are:
	 *
	 *     Melee.BaseDamage,70.0          <- BP05 step 2 states 70; the CSV is where 70 becomes real
	 *     Melee.RangeMetres,<reach>      <- how far the swing reaches, eye point to victim
	 *     Melee.SweepRadiusMetres,<r>    <- the swung volume's thickness; a line trace would whiff
	 *     Melee.RearArcDegrees,<arc>     <- TOTAL arc behind the victim that counts as a backstab
	 *
	 * They also do not exist in `BRCombatCurves::Names`, which is in `AbilitySystem/` and not in
	 * `AbilitySystem/Abilities/` — a sibling folder, not a child, which is precisely the
	 * owner_path trap BP05's Log warns about. They live here until the packet that owns that file
	 * lifts them, at which point these four lines are deleted and the header's constants used.
	 *
	 * The naming follows `CT_Combat`'s existing spelling: `Domain.Thing.PropertyWithUnitSpelledOut`
	 * (`Shields.Regen.DelaySeconds`, `Fighter.MaxHealth`), never the row-struct `_m` suffix that
	 * `DT_Weapons` uses for its columns.
	 */
	const FName MeleeBaseDamageCurve(TEXT("Melee.BaseDamage"));
	const FName MeleeRangeCurve(TEXT("Melee.RangeMetres"));
	const FName MeleeSweepRadiusCurve(TEXT("Melee.SweepRadiusMetres"));
	const FName MeleeRearArcCurve(TEXT("Melee.RearArcDegrees"));

	/**
	 * THE NO-ANIMATION FALLBACK WINDOW, in seconds. STRUCTURAL, and here is the argument.
	 *
	 * No swing montage exists, so `Event.Melee.WindowBegin`/`WindowEnd` cannot fire — the notify
	 * that would announce them has nothing to live on. Without a stand-in this ability activates,
	 * commits, and then waits forever for an event that is never sent: a leaked instanced ability,
	 * which `gas-purity.md` §9 lists as a finding in its own right.
	 *
	 * It is not tuning because nobody will ever balance it: the instant a montage lands, the
	 * notify closes the window first and this number stops deciding anything (see
	 * `OnSwingWatchdogElapsed` — the watchdog is armed on every swing and simply loses the race).
	 * It is not a melee "duration" either; it is the deadline by which the animation must have
	 * spoken. `animation.md` law 3 is the reason it must NOT become a table value: ability timings
	 * are authored TO the sourced clip, so the real number is the clip's, not a CSV's.
	 */
	constexpr float SwingWindowFallbackSeconds = 0.25f;

	/**
	 * How long the SERVER waits for a remote attacker's claim before abandoning the swing.
	 * Structural, and a netcode bound rather than a balance number: it exists so a client that
	 * disconnects, hitches, or simply never resolves its window cannot leave a committed melee
	 * ability alive on the server. Generous on purpose — a swing lost to a slow claim is a whiff,
	 * and a whiff is cheaper than a leak.
	 */
	constexpr float ClaimTimeoutSeconds = 1.5f;

	/**
	 * How far past the reach a claim may land before it is rejected. Absorbs the difference
	 * between the client's avatar position and the server's, and the victim's capsule radius —
	 * the claim's impact point is on a surface while the reach is measured to a centre. It does
	 * NOT absorb a cheat, because it is a fixed small distance rather than a fraction of reach.
	 * Same device, same reasoning, same units as `BRGA_WeaponFire`'s RangeToleranceUU.
	 */
	constexpr float ReachToleranceUU = 75.f;

	/**
	 * Read the four melee coefficients out of `CT_Combat`.
	 *
	 * FAILURE IS LOUD AND TYPED, never a default — `BRCombatCurves::Evaluate` deliberately offers
	 * no "EvaluateOrDefault", and inventing one here would turn a missing CSV row into a balance
	 * change nobody can see. Returns false and names the missing curve.
	 */
	bool ResolveMeleeTuning(float& OutBaseDamage, float& OutRangeUU, float& OutSweepRadiusUU, float& OutRearArcDegrees, FString& OutMissing)
	{
		float RangeMetres = 0.f;
		float SweepRadiusMetres = 0.f;

		if (!BRCombatCurves::Evaluate(MeleeBaseDamageCurve, OutBaseDamage))
		{
			OutMissing = MeleeBaseDamageCurve.ToString();
			return false;
		}
		if (!BRCombatCurves::Evaluate(MeleeRangeCurve, RangeMetres))
		{
			OutMissing = MeleeRangeCurve.ToString();
			return false;
		}
		if (!BRCombatCurves::Evaluate(MeleeSweepRadiusCurve, SweepRadiusMetres))
		{
			OutMissing = MeleeSweepRadiusCurve.ToString();
			return false;
		}
		if (!BRCombatCurves::Evaluate(MeleeRearArcCurve, OutRearArcDegrees))
		{
			OutMissing = MeleeRearArcCurve.ToString();
			return false;
		}

		OutRangeUU = RangeMetres * BRUnits::MetresToUU;
		OutSweepRadiusUU = SweepRadiusMetres * BRUnits::MetresToUU;

		// A present-but-degenerate row is refused as loudly as a missing one. A zero reach is not
		// "melee with no range", it is a table that was filled in wrong, and a zero-radius sphere
		// sweep silently degrades to the line trace this ability is deliberately not using.
		if (OutRangeUU <= 0.f)
		{
			OutMissing = FString::Printf(TEXT("%s is %.3f (must be > 0)"), *MeleeRangeCurve.ToString(), RangeMetres);
			return false;
		}
		if (OutSweepRadiusUU <= 0.f)
		{
			OutMissing = FString::Printf(TEXT("%s is %.3f (must be > 0)"), *MeleeSweepRadiusCurve.ToString(), SweepRadiusMetres);
			return false;
		}

		return true;
	}
}

UBRGA_Melee::UBRGA_Melee(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// InstancedPerActor + LocalPredicted: the attacker feels the swing immediately, the server
	// decides whether it connected. Anything a player feels is predicted (`gas-purity.md` §4).
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	ActivationPolicy = EBRAbilityActivationPolicy::OnInputPressed;

	// TODO(BP05): Ability.Melee must be declared by this packet.
	//
	// `Ability.*` is an OPEN family (ruling R23) and the leaf belongs to whichever packet authors
	// the ability — this one. It is NOT declared here because `Core/BRGameplayTags.h` is being
	// written by another agent in this same session, and law 5 is explicit that a blocked write is
	// a contract_gap and a STOP, never a shared-file edit made to unblock oneself.
	//
	// So this line names a symbol that does not exist yet, and **the module will not compile until
	// it does**. Two lines close it, and they are the only two:
	//
	//   BRGameplayTags.h   BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Melee);
	//   BRGameplayTags.cpp UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Melee, "Ability.Melee",
	//                          "UBRGA_Melee's asset tag (BP05).");
	//
	// The alternative — FGameplayTag::RequestGameplayTag(FName("Ability.Melee"), false) — compiles
	// today and is worse: it resolves to an invalid tag, SetAssetTags stores nothing, and melee
	// becomes an ability that no CancelAbilitiesWithTag can ever match. A failed compile naming
	// the exact missing symbol is the loud version of the same fact.
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(BRGameplayTags::Ability_Melee);
	SetAssetTags(AssetTags);

	// Melee ends a sprint, Halo-style, with zero code in either ability — and this is the line
	// BP05 step 2 means by "sprint-cancel via CancelAbilitiesWithTags proven here". Note it lists
	// `Ability.Sprint`, the ABILITY's asset tag, and NOT `State.Movement.Sprinting`, which is a tag
	// on the ACTOR: CancelAbilitiesWithTag is matched against the target ability's asset tags.
	// ARCHITECTURE §3.3's sentence says the latter and does not work; BRGameplayTags.h records the
	// correction and BRGA_WeaponFire already depends on it.
	CancelAbilitiesWithTag.AddTag(BRGameplayTags::Ability_Sprint);

	// NO COOLDOWN TAG, AND THAT IS A DECISION — stated here because "no cooldown" and "someone
	// forgot the cooldown" look identical in code (BRGA_Sprint's precedent).
	//
	// The swing's own length is the rate limit: this ability stays active from activation to
	// WindowEnd, and an InstancedPerActor ability with bRetriggerInstancedAbility false refuses to
	// re-activate while an instance is live. A melee cooldown would therefore be a SECOND rate
	// limiter stacked on the montage's, needing a fifth `CT_Combat` row to express a rule the
	// animation already enforces. If the M2 rhythm pass (BP05 step 3) finds melee spammable
	// between swings, the fix is a `Melee.CooldownSeconds` curve plus `CooldownTag` here — four
	// lines, and the base class already does the rest.

	bCommitOnActivate = true;
}

// ================================================================================================
// Preconditions
// ================================================================================================

bool UBRGA_Melee::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// Melee carries no ammo and no equipment precondition — it is the verb that works when the
	// weapon does not, which is the whole reason the Golden Triangle has three sides. The ONE
	// precondition is that its numbers exist.
	float BaseDamage = 0.f, RangeUU = 0.f, SweepRadiusUU = 0.f, RearArcDegrees = 0.f;
	FString Missing;
	if (!ResolveMeleeTuning(BaseDamage, RangeUU, SweepRadiusUU, RearArcDegrees, Missing))
	{
		// REFUSED, not silently degraded, and refused HERE rather than mid-swing: an ability that
		// commits, animates and then declines to damage anything is indistinguishable from a
		// missed hit, and BP05's rhythm pass would be judging a bug.
		UE_LOG(LogBRCombat, Warning,
			TEXT("BRGA_Melee refused to activate: CT_Combat is missing '%s'. Melee's four coefficients "
				 "(Melee.BaseDamage, Melee.RangeMetres, Melee.SweepRadiusMetres, Melee.RearArcDegrees) "
				 "are DATA — typing one into this ability would be a law-3 violation. Filed as a "
				 "contract_gap in TICKET_BP05_TRIANGLE."),
			*Missing);
		return false;
	}

	return true;
}

// ================================================================================================
// Activation — two paths, and which machine you are on decides which
// ================================================================================================

void UBRGA_Melee::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// The base commits cost + cooldown and will have ended us if the commit failed.
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive())
	{
		return;
	}

	// Every bit of this ability's state is per-activation. Nothing here survives a swing, which is
	// what makes a respawn between two melees uninteresting (`gas-purity.md` §7).
	SwingOriginLocation = FVector::ZeroVector;
	bWindowOpen = false;
	bSwingResolved = false;
	bClaimHandled = false;

	UBRAbilitySystemComponent* ASC = GetBRAbilitySystemComponent();
	if (!ASC)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	if (IsLocallyControlled())
	{
		// TODO(BP05, gap 3): play the swing montage here, via UAbilityTask_PlayMontageAndWait, with
		// the montage arriving as a SOFT reference from data (law 3 forbids a hard asset ref or
		// ConstructorHelpers in C++). Its two notifies raise Event.Melee.WindowBegin/WindowEnd on
		// this ASC and the two listeners below pick them up unchanged — the montage adds a
		// PRESENTATION and a pair of MOMENTS, and changes nothing about the decision below it.
		// That is `animation.md` law 4 stated as a code shape rather than as a rule.

		WindowBeginTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this, BRGameplayTags::Event_Melee_WindowBegin, /*OptionalExternalTarget=*/nullptr, /*OnlyTriggerOnce=*/true);
		WindowEndTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this, BRGameplayTags::Event_Melee_WindowEnd, /*OptionalExternalTarget=*/nullptr, /*OnlyTriggerOnce=*/true);

		if (!WindowBeginTask || !WindowEndTask)
		{
			UE_LOG(LogBRCombat, Error, TEXT("BRGA_Melee: could not create the notify-window tasks; ending without swinging."));
			EndAbility(Handle, ActorInfo, ActivationInfo, true, /*bWasCancelled=*/true);
			return;
		}

		// BOTH listeners are armed before either can fire. A montage whose two notifies land in the
		// same frame — a very short window, which is exactly what a melee window is — would
		// otherwise deliver WindowEnd to nobody.
		WindowBeginTask->EventReceived.AddDynamic(this, &UBRGA_Melee::OnWindowBegin);
		WindowEndTask->EventReceived.AddDynamic(this, &UBRGA_Melee::OnWindowEnd);
		WindowBeginTask->ReadyForActivation();
		WindowEndTask->ReadyForActivation();

		// The watchdog. Today it is the whole window (no montage exists); after the montage lands
		// it is the guard that keeps a missing or misplaced notify from leaking a live ability.
		WatchdogTask = UAbilityTask_WaitDelay::WaitDelay(this, SwingWindowFallbackSeconds);
		if (WatchdogTask)
		{
			WatchdogTask->OnFinish.AddDynamic(this, &UBRGA_Melee::OnSwingWatchdogElapsed);
			WatchdogTask->ReadyForActivation();
		}
		else
		{
			// No watchdog and no montage means an ability that never ends. Refuse the swing rather
			// than leak the instance.
			UE_LOG(LogBRCombat, Error, TEXT("BRGA_Melee: could not create the swing watchdog; ending without swinging."));
			EndAbility(Handle, ActorInfo, ActivationInfo, true, /*bWasCancelled=*/true);
		}
		return;
	}

	// SERVER, for a remote attacker: do not sweep anything yet, and do not run the window here.
	// The montage plays on this machine too and its notifies do arrive, but the server's job is not
	// to swing — it is to judge the swing the client says it took. Sweeping here and hoping the two
	// agree is how melee gets a "server says you missed" bug that nobody can reproduce.
	//
	// The timeout is armed BEFORE the replay call below, because CallReplicatedTargetDataDelegatesIfSet
	// can deliver an already-arrived claim synchronously and end this ability on the spot.
	WatchdogTask = UAbilityTask_WaitDelay::WaitDelay(this, ClaimTimeoutSeconds);
	if (WatchdogTask)
	{
		WatchdogTask->OnFinish.AddDynamic(this, &UBRGA_Melee::OnClaimTimeoutElapsed);
		WatchdogTask->ReadyForActivation();
	}

	if (!IsActive())
	{
		return;
	}

	TargetDataDelegateHandle = ASC->AbilityTargetDataSetDelegate(Handle, ActivationInfo.GetActivationPredictionKey())
		.AddUObject(this, &UBRGA_Melee::OnTargetDataReady);
	ASC->CallReplicatedTargetDataDelegatesIfSet(Handle, ActivationInfo.GetActivationPredictionKey());
}

// ================================================================================================
// The notify seam — three ways in, one way through
// ================================================================================================

void UBRGA_Melee::OnWindowBegin(FGameplayEventData Payload)
{
	if (bWindowOpen || bSwingResolved)
	{
		// A repeated WindowBegin must not move the swing's origin mid-swing.
		return;
	}

	FVector ViewLocation, ViewDirection;
	if (!GetViewPoint(ViewLocation, ViewDirection))
	{
		UE_LOG(LogBRCombat, Warning, TEXT("BRGA_Melee: Event.Melee.WindowBegin with no resolvable view point; ending."));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, /*bWasCancelled=*/true);
		return;
	}

	// The notify announced a MOMENT. All this file does with it is remember where the attacker was
	// standing — the consequence is decided at WindowEnd, on the authority (`animation.md` law 4).
	bWindowOpen = true;
	SwingOriginLocation = ViewLocation;
}

void UBRGA_Melee::OnWindowEnd(FGameplayEventData Payload)
{
	if (!bWindowOpen)
	{
		// WindowEnd without WindowBegin is a mis-authored montage, not a swing. Say so and end;
		// resolving it would damage from an origin nobody recorded.
		UE_LOG(LogBRCombat, Warning,
			TEXT("BRGA_Melee: Event.Melee.WindowEnd arrived with no open window — the montage's notify "
				 "pair is mis-ordered or WindowBegin is missing. No swing was resolved."));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, /*bWasCancelled=*/true);
		return;
	}

	bWindowOpen = false;
	ResolveSwing();
}

void UBRGA_Melee::OnSwingWatchdogElapsed()
{
	if (bSwingResolved)
	{
		// The notify won the race — which is the state this file wants to be in permanently.
		return;
	}

	if (!bWindowOpen)
	{
		// NOTHING ANNOUNCED ANYTHING. Today that is the normal case and it is not a malfunction:
		// there is no swing montage, so there is no notify to raise the window. Logged once per
		// process at Warning so a playtest cannot mistake this for a working animation seam, and at
		// Verbose every time for anyone reading a full log.
		static bool bLoggedNoAnimationOnce = false;
		if (!bLoggedNoAnimationOnce)
		{
			bLoggedNoAnimationOnce = true;
			UE_LOG(LogBRCombat, Warning,
				TEXT("BRGA_Melee IS RUNNING WITHOUT ANIMATION. No swing montage exists, so "
					 "Event.Melee.WindowBegin/WindowEnd never fired; the swing is being resolved from the "
					 "structural %.2f s fallback window instead. This is BP05 gap 3 — once the montage "
					 "lands, its notifies are authoritative and this fallback only ever guards against a "
					 "missing one. Logged once per process."),
				SwingWindowFallbackSeconds);
		}
		UE_LOG(LogBRCombat, Verbose, TEXT("BRGA_Melee: resolving a swing from the no-animation fallback window."));

		// The whole window collapses to this instant: origin and reach point are both taken from
		// where the attacker is now. With a montage the origin is a quarter-second older, which is
		// the only difference between this path and the real one.
		FVector ViewLocation, ViewDirection;
		if (!GetViewPoint(ViewLocation, ViewDirection))
		{
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, /*bWasCancelled=*/true);
			return;
		}
		SwingOriginLocation = ViewLocation;
	}
	else
	{
		// WindowBegin arrived and WindowEnd did not. The montage is mis-authored or was interrupted
		// after the opening notify; either way the ability must not stay alive waiting for it.
		UE_LOG(LogBRCombat, Warning,
			TEXT("BRGA_Melee: Event.Melee.WindowBegin fired but WindowEnd did not within %.2f s. "
				 "Closing the window from the watchdog so the ability cannot leak."),
			SwingWindowFallbackSeconds);
	}

	bWindowOpen = false;
	ResolveSwing();
}

void UBRGA_Melee::OnClaimTimeoutElapsed()
{
	if (bClaimHandled)
	{
		return;
	}

	// The client committed a melee and never told the server what it hit. That is a whiff as far as
	// the sim is concerned — no damage, no guessing, and above all no server-side sweep invented to
	// fill the silence, which would be the server playing the attacker's swing for it.
	UE_LOG(LogBRCombat, Log,
		TEXT("BRGA_Melee: no target data from the attacker within %.2f s; the swing is abandoned. "
			 "Nothing was damaged."),
		ClaimTimeoutSeconds);

	bClaimHandled = true;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, /*bWasCancelled=*/true);
}

// ================================================================================================
// The swing
// ================================================================================================

void UBRGA_Melee::ResolveSwing()
{
	if (bSwingResolved)
	{
		return;
	}
	bSwingResolved = true;

	UBRAbilitySystemComponent* ASC = GetBRAbilitySystemComponent();
	if (!ASC)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	// Re-read rather than cache from CanActivateAbility: the CDO answers CanActivate, the instance
	// swings, and a table reimport between the two is a supported thing to do in the editor.
	float BaseDamage = 0.f, RangeUU = 0.f, SweepRadiusUU = 0.f, RearArcDegrees = 0.f;
	FString Missing;
	if (!ResolveMeleeTuning(BaseDamage, RangeUU, SweepRadiusUU, RearArcDegrees, Missing))
	{
		UE_LOG(LogBRCombat, Error, TEXT("BRGA_Melee: CT_Combat lost '%s' between activation and the swing; nothing was resolved."), *Missing);
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, /*bWasCancelled=*/true);
		return;
	}

	FVector ViewLocation, ViewDirection;
	if (!GetViewPoint(ViewLocation, ViewDirection))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, /*bWasCancelled=*/true);
		return;
	}

	// ONE query, from where the window opened to where the reach ends now. If the attacker stood
	// still these are a plain forward sweep; if they moved, the same sweep covers the travel. This
	// is the no-Tick answer to "trace during the window" (law 4) — and it is also the one both
	// sides could re-derive, because both endpoints are transforms rather than samples.
	const FVector SweepEnd = ViewLocation + ViewDirection * RangeUU;
	const FHitResult Hit = SweepSwing(SwingOriginLocation, SweepEnd, SweepRadiusUU);

	// Package the claim. SingleTargetHit carries the actor and the impact point — the two things
	// the server needs to re-derive reach and line of sight. It carries nothing about the rear arc
	// and there is no field for it: see the class comment.
	FGameplayAbilityTargetData_SingleTargetHit* HitData = new FGameplayAbilityTargetData_SingleTargetHit(Hit);
	FGameplayAbilityTargetDataHandle DataHandle;
	DataHandle.Add(HitData);

	{
		// The prediction window is what lets a predicted GE or cue run now and be reconciled later.
		// Outside it, a predicted effect would never be rolled back.
		FScopedPredictionWindow ScopedPrediction(ASC, CurrentActivationInfo.GetActivationPredictionKey());

		ASC->ServerSetReplicatedTargetData(
			CurrentSpecHandle,
			CurrentActivationInfo.GetActivationPredictionKey(),
			DataHandle,
			FGameplayTag(),
			ASC->ScopedPredictionKey);
	}

	// Resolve locally too: on the listen-server host this IS the authoritative path, and on a
	// remote client it is the prediction.
	OnTargetDataReady(DataHandle, FGameplayTag());
}

FHitResult UBRGA_Melee::SweepSwing(const FVector& From, const FVector& To, float SweepRadiusUU) const
{
	FHitResult Hit;
	const UWorld* World = GetWorld();
	if (!World)
	{
		return Hit;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(BRMeleeSwing), /*bTraceComplex=*/false);
	Params.AddIgnoredActor(GetAvatarActorFromActorInfo());

	// SINGLE, not multi, and on a channel where world geometry BLOCKS. That is the whole defence
	// against BP05 step 5's "melee through-floor" REFUTER: a wall between the attacker and the
	// victim is the first blocking hit, the sweep stops there, and the claim carries a piece of
	// architecture with no ASC — which the damage path drops. The rule is expressed by the channel
	// (`BRMelee` in DefaultEngine.ini) rather than by a branch in this file.
	//
	// bTraceComplex is FALSE, unlike the fire path's: melee needs no bone name because it has no
	// per-bone modifier, and a complex sweep against every skeletal mesh in reach is the expensive
	// version of a question nobody asked.
	World->SweepSingleByChannel(Hit, From, To, FQuat::Identity, BRCollision::MeleeTrace, FCollisionShape::MakeSphere(SweepRadiusUU), Params);

	if (!Hit.bBlockingHit)
	{
		// A whiff still carries its geometry, so the server can see the direction of a swing that
		// hit nothing and a future cue knows where the blade went.
		Hit.TraceStart = From;
		Hit.TraceEnd = To;
		Hit.ImpactPoint = To;
	}
	return Hit;
}

// ================================================================================================
// The gate
// ================================================================================================

void UBRGA_Melee::OnTargetDataReady(const FGameplayAbilityTargetDataHandle& TargetData, FGameplayTag ApplicationTag)
{
	if (bClaimHandled)
	{
		// A second TargetData packet for one activation must not land a second hit.
		return;
	}
	bClaimHandled = true;

	UBRAbilitySystemComponent* ASC = GetBRAbilitySystemComponent();
	if (!ASC)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	if (ASC->GetOwnerRole() == ROLE_Authority)
	{
		ASC->ConsumeClientReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey());
	}

	// TODO(BP05, gap 4): the swing and impact cues. `gas-purity.md` law 6 puts EVERY cosmetic
	// consequence in a GameplayCue, and this ability deliberately plays none rather than spawning
	// an effect from an ability body. It needs two `GameplayCue.*` leaves — an Executed swing cue
	// raised here on both sides (the swing is a consequence of pressing melee, not of connecting,
	// exactly as BRGA_WeaponFire plays its muzzle flash before validation) and an Executed impact
	// cue raised on the authority when a hit is accepted. `GameplayCue.*` is the second OPEN family
	// under R23, so the same one-line-per-tag block in BRGameplayTags closes it — blocked here for
	// the same owner_path reason as Ability.Melee.

	const FGameplayAbilityTargetData* Data = TargetData.Get(0);
	const FHitResult* Claim = Data ? Data->GetHitResult() : nullptr;

	if (!Claim || !Claim->GetActor())
	{
		// A WHIFF, not a rejection. Melee misses constantly and by design; logging it at Warning
		// would drown the log line that matters. Nothing to validate, nothing to damage.
		UE_LOG(LogBRCombat, Verbose, TEXT("BRGA_Melee: swing connected with nothing."));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	if (HasAuthority(&CurrentActivationInfo))
	{
		float BaseDamage = 0.f, RangeUU = 0.f, SweepRadiusUU = 0.f, RearArcDegrees = 0.f;
		FString Missing;
		FString Reason;

		if (!ResolveMeleeTuning(BaseDamage, RangeUU, SweepRadiusUU, RearArcDegrees, Missing))
		{
			UE_LOG(LogBRCombat, Error, TEXT("BRGA_Melee: CT_Combat is missing '%s' on the server; no damage was applied."), *Missing);
		}
		else if (!ValidateClaim(*Claim, RangeUU, Reason))
		{
			// REJECTION IS LOUD. A silently dropped melee is indistinguishable from a bug, and this
			// line is the anti-cheat's only witness until BP05 step 5's REFUTER tests exist.
			UE_LOG(LogBRCombat, Warning, TEXT("BRGA_Melee REJECTED a client claim: %s"), *Reason);
		}
		else
		{
			// SERVER TRUTH, computed from transforms the server already owns. The client's view
			// rotation is not an input here, which is what makes rear-arc spoofing a non-question.
			const bool bRearHit = IsRearHit(Claim->GetActor(), RearArcDegrees);
			ApplyMeleeDamage(*Claim, BaseDamage, bRearHit);
		}
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

bool UBRGA_Melee::ValidateClaim(const FHitResult& Claim, float RangeUU, FString& OutReason) const
{
	const AActor* Attacker = GetAvatarActorFromActorInfo();
	AActor* Victim = Claim.GetActor();
	if (!Attacker || !Victim)
	{
		OutReason = TEXT("server has no avatar for the attacker or the victim");
		return false;
	}

	if (Victim == Attacker)
	{
		OutReason = TEXT("the claim names the attacker as its own victim");
		return false;
	}

	FVector ServerLocation, ServerDirection;
	if (!GetViewPoint(ServerLocation, ServerDirection))
	{
		OutReason = TEXT("server could not resolve the attacker's view point");
		return false;
	}

	const FVector VictimLocation = Victim->GetActorLocation();

	// 1. REACH. Measured from the SERVER's eye point to the SERVER's victim — neither endpoint
	// comes from the packet. This is the check that refuses a claim against someone across the map.
	const float MaxReachUU = RangeUU + ReachToleranceUU;
	const float ClaimedDistance = FVector::Dist(ServerLocation, VictimLocation);
	if (ClaimedDistance > MaxReachUU)
	{
		OutReason = FString::Printf(
			TEXT("victim '%s' is %.0f uu from the server's attacker; melee reach (+tolerance) is %.0f uu"),
			*GetNameSafe(Victim), ClaimedDistance, MaxReachUU);
		return false;
	}

	// 2. IN FRONT. Not a tuned cone — a STRUCTURAL half-space: you cannot swing through your own
	// back. The number is zero (the dot product's sign), so there is no gameplay literal here and
	// nothing for a designer to balance. If a narrower swing arc is ever DESIGNED, it arrives as a
	// `Melee.SwingArcDegrees` curve and this comparison gains a cosine — deliberately not
	// pre-invented, because a made-up cone angle is a balance decision smuggled in as validation.
	const FVector ToVictim = (VictimLocation - ServerLocation).GetSafeNormal();
	if (!ToVictim.IsNearlyZero() && FVector::DotProduct(ServerDirection, ToVictim) <= 0.f)
	{
		OutReason = FString::Printf(
			TEXT("victim '%s' is behind the server's view direction for the attacker"), *GetNameSafe(Victim));
		return false;
	}

	// 3. LINE OF SIGHT, re-derived on the server. The client's own sweep already stops at geometry
	// (see SweepSwing), but that is the CLIENT's copy of the level and a client is free to lie
	// about it. This is BP05 step 5's "melee through-floor" REFUTER answered on the authority: a
	// blocking hit on the melee channel that is not the claimed victim means something solid is in
	// the way.
	if (const UWorld* World = GetWorld())
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(BRMeleeLineOfSight), /*bTraceComplex=*/false);
		Params.AddIgnoredActor(Attacker);

		FHitResult Blocker;
		if (World->LineTraceSingleByChannel(Blocker, ServerLocation, VictimLocation, BRCollision::MeleeTrace, Params)
			&& Blocker.GetActor() != Victim)
		{
			OutReason = FString::Printf(
				TEXT("'%s' blocks the line from the attacker to victim '%s' — melee through geometry"),
				*GetNameSafe(Blocker.GetActor()), *GetNameSafe(Victim));
			return false;
		}
	}

	// 4. RATE is NOT checked here. The swing's own length gates re-activation (see the constructor's
	// no-cooldown note); re-implementing a rate rule in the validator would be a second source of
	// truth for the same question.

	return true;
}

bool UBRGA_Melee::IsRearHit(const AActor* Victim, float RearArcDegrees) const
{
	const AActor* Attacker = GetAvatarActorFromActorInfo();
	if (!Attacker || !Victim)
	{
		return false;
	}

	// FLATTENED TO THE HORIZONTAL PLANE, and that is the design, not a shortcut. A backstab is a
	// question about which way the victim is FACING; an attacker standing on a crate above them is
	// behind them or is not, and the pitch of the approach must not decide it. A character's actor
	// forward is already yaw-only, so the flattening matters for the approach vector — without it,
	// a steep approach silently shortens the arc.
	FVector VictimForward = Victim->GetActorForwardVector();
	VictimForward.Z = 0.f;

	FVector VictimToAttacker = Attacker->GetActorLocation() - Victim->GetActorLocation();
	VictimToAttacker.Z = 0.f;

	if (!VictimForward.Normalize() || !VictimToAttacker.Normalize())
	{
		// Degenerate geometry (perfectly co-located, or a victim with no horizontal facing). Not a
		// backstab: the modifier tag is only ever ADDED on a positive answer, never assumed.
		return false;
	}

	// `Melee.RearArcDegrees` is the TOTAL arc behind the victim, so the half-angle is what the
	// cosine compares against: 90 degrees means "within 45 degrees of directly behind".
	const float HalfArcDegrees = FMath::Clamp(RearArcDegrees, 0.f, 360.f) * 0.5f;
	const float MinCos = FMath::Cos(FMath::DegreesToRadians(HalfArcDegrees));

	return FVector::DotProduct(-VictimForward, VictimToAttacker) >= MinCos;
}

void UBRGA_Melee::ApplyMeleeDamage(const FHitResult& Hit, float BaseDamage, bool bRearHit) const
{
	UBRAbilitySystemComponent* SourceASC = GetBRAbilitySystemComponent();
	AActor* HitActor = Hit.GetActor();
	if (!SourceASC || !HitActor)
	{
		return;
	}

	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
	if (!TargetASC || TargetASC == SourceASC)
	{
		// No ASC means geometry, not a fighter — a melee into a wall is a whiff with a sound, and
		// the sound is a cue's problem. Self-hits are dropped rather than reflected.
		return;
	}

	// The damage TYPE and MODIFIER tags. The family is FLAT (ruling R22): a rear melee is the PAIR
	// {Damage.Melee, Damage.Rear}, never Damage.Melee.Rear — the exec calc multiplies each tag's
	// own `<TagName>.Multiplier` curve, so the two axes compose instead of switching. "Rear melee
	// is lethal" is therefore a value in `Damage.Rear.Multiplier` and NOT a branch in this file;
	// both curve rows already exist in CT_Combat.csv at 1.0, which is the identity — the lethality
	// BP05 step 2 describes is a data change the balance owner makes, and this ability will not
	// notice it.
	FGameplayTagContainer DamageTags;
	DamageTags.AddTag(BRGameplayTags::Damage_Melee);
	if (bRearHit)
	{
		DamageTags.AddTag(BRGameplayTags::Damage_Rear);
	}

	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	Context.AddInstigator(GetAvatarActorFromActorInfo(), GetAvatarActorFromActorInfo());
	Context.AddHitResult(Hit);

	// ONE pipeline, and the same two calls the AR makes. This ability supplies a base number and
	// tags; BRDamageExecCalc reads CT_Combat and decides what they mean; BRAttributeSet decides
	// what shields and health do about it. The engine damage API is banned (`gas-purity.md` law 3)
	// and `guard_laws.py` blocks it at tool-call time.
	const FGameplayEffectSpecHandle Spec = UBRGE_Damage::MakeSpec(SourceASC, BaseDamage, DamageTags, Context);
	UBRGE_Damage::ApplyToTarget(Spec, SourceASC, TargetASC);
}

// ================================================================================================
// Teardown and helpers
// ================================================================================================

void UBRGA_Melee::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// EVERY path tears everything down, including the cancelled one — and melee is cancelled often,
	// because death, a grapple, and BP05's rhythm pass all interrupt it mid-window. A leaked
	// target-data delegate re-fires against the NEXT activation's prediction key and lands a hit
	// that was never swung; a leaked WaitGameplayEvent listens on the ASC forever and resolves a
	// swing from the montage of a later melee. Both are the same bug with different symptoms.
	if (TargetDataDelegateHandle.IsValid())
	{
		if (UBRAbilitySystemComponent* ASC = GetBRAbilitySystemComponent())
		{
			ASC->AbilityTargetDataSetDelegate(Handle, ActivationInfo.GetActivationPredictionKey())
				.Remove(TargetDataDelegateHandle);
		}
		TargetDataDelegateHandle.Reset();
	}

	ClearTasks();

	// Cancel cleanliness (`animation.md` law 5): there is nothing else to undo. No loose tag was
	// added, no cost was hand-tracked, no FX was spawned outside a cue, and every flag above is
	// per-activation and re-initialised in ActivateAbility. A rejected prediction leaves zero
	// residue because there is no residue to leave.
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UBRGA_Melee::ClearTasks()
{
	if (WindowBeginTask)
	{
		WindowBeginTask->EndTask();
		WindowBeginTask = nullptr;
	}
	if (WindowEndTask)
	{
		WindowEndTask->EndTask();
		WindowEndTask = nullptr;
	}
	if (WatchdogTask)
	{
		WatchdogTask->EndTask();
		WatchdogTask = nullptr;
	}
}

bool UBRGA_Melee::GetViewPoint(FVector& OutLocation, FVector& OutDirection) const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		return false;
	}

	// GetActorEyesViewPoint is the seam that works on BOTH sides: on the server for a remote pawn it
	// returns the replicated control rotation, which is precisely the "server's attacker" the
	// validation is supposed to compare against. Same helper, same reasoning, same shape as
	// BRGA_WeaponFire::GetViewPoint — deliberately duplicated rather than shared, because the two
	// abilities are in different packets and a shared helper would need a home neither owns.
	FRotator ViewRotation;
	Avatar->GetActorEyesViewPoint(OutLocation, ViewRotation);
	OutDirection = ViewRotation.Vector();
	return true;
}
