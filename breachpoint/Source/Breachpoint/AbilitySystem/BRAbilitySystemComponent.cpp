// Breachpoint. The ASC: input-buffered tag activation, prediction-window helpers.

#include "AbilitySystem/BRAbilitySystemComponent.h"

#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameplayEffect.h"
#include "GameplayPrediction.h"

#include "AbilitySystem/Abilities/BRGameplayAbility.h"
#include "AbilitySystem/BRAttributeSet.h"
#include "AbilitySystem/BRCombatCurves.h"
#include "AbilitySystem/Effects/BRGameplayEffects.h"
#include "Core/BRCore.h"
#include "Core/BRGameplayTags.h"

namespace
{
	/**
	 * `Fighter.MaxGrenades` — the fighter's grenade capacity, read from `CT_Combat` by ApplyInitStats.
	 *
	 * WHY IT IS HERE AND NOT IN `BRCombatCurves::Names`, where its two siblings `Fighter.MaxHealth`
	 * and `Fighter.MaxShields` live and where it plainly belongs: `AbilitySystem/BRCombatCurves.h` is
	 * NOT this packet's file to write, and law 5 says a blocked path is a contract_gap and a stop,
	 * not a shared-header edit. The precedent is `BRGrappleCurveNames` in
	 * `BRCharacterMovementComponent.cpp`, which is the same constant in the same position for the
	 * same reason.
	 *
	 * A NAME IS NOT A NUMBER — nothing here decides how many grenades a fighter carries; the row
	 * `Fighter.MaxGrenades,2.0,2.0,2.0` in `CT_Combat.csv` does. When `BRCombatCurves.h` is next
	 * open, this constant moves up beside its siblings and this comment goes away.
	 */
	const FName FighterMaxGrenadesCurve(TEXT("Fighter.MaxGrenades"));

	/**
	 * The "this input tag matched no granted ability" report is ONCE PER (owner, tag) for the life of
	 * the process, and everything after the first is Verbose.
	 *
	 * WHY IT IS BOUNDED AT ALL: the report fires on the PRESS EDGE, so a held key is already one
	 * line — but a semi-auto Magnum is pressed as fast as a finger moves, and a tag that is
	 * permanently ungranted (no ability set authored yet, which is the state this project is in
	 * today) would otherwise emit a Warning per click for the whole session and bury the stream the
	 * reader opened to find it.
	 *
	 * The pattern, deliberately, is the cue library's `GetSilentCueReportLedger` in
	 * `AbilitySystem/Cues/BRGameplayCues.cpp` — same problem, same shape, same file-static answer.
	 * A file-static and not a member because this header is not this task's to write, and because a
	 * log throttle is not gameplay state and must never become replicated, saved, or read by anything
	 * that decides something. Game thread only, like every input route.
	 */
	TSet<FName>& GetUnmatchedInputTagLedger()
	{
		static TSet<FName> Ledger;
		return Ledger;
	}
}

UBRAbilitySystemComponent::UBRAbilitySystemComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// The generic-GE library, wired by CLASS (ruling R18 — these are C++ classes, so there is
	// nothing to soft-load and nothing to cook). Assigned here rather than left for a designer
	// because there is no designer surface: abilities and effects are code in this project, and an
	// unassigned class would be a runtime hole that compiles.
	RecentDamageEffectClass = UBRGE_RecentDamage::StaticClass();
	ShieldsBrokenEffectClass = UBRGE_ShieldsBroken::StaticClass();
	InitStatsEffectClass = UBRGE_InitStats::StaticClass();
	ShieldRegenEffectClass = UBRGE_Regen::StaticClass();
	DeathEffectClass = UBRGE_Death::StaticClass();

	// §5.4, and it is a decision rather than a default: Mixed sends the OWNER full GameplayEffect
	// replication (so its UI, prediction and rollback are exact) while everyone else receives only
	// tags and cues (so a 4v4 does not pay 8x for effect bookkeeping nobody can see).
	// Minimal is for non-player-shaped AI; our bots sit on PlayerStates and are player-shaped by
	// design (§3.7), so they get Mixed too — one setting, no bot special case.
	SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	// The ASC replicates. Without this the whole mode choice above is decoration.
	SetIsReplicatedByDefault(true);

	// NOT DISABLED, AND THAT IS DELIBERATE — read before "fixing" it for law 4.
	// UAbilitySystemComponent::TickComponent drives ENGINE-internal work (montage replication
	// bookkeeping, target-data cleanup, cue processing). Law 4 bans OUR gameplay from polling; it
	// does not license breaking an engine subsystem's own pump. This class adds no tick work of its
	// own and calls SetShouldTick() nowhere, which is the letter and the spirit both.
}

// ---------------------------------------------------------------------------
// Input — arrow five of §3.2
// ---------------------------------------------------------------------------

void UBRAbilitySystemComponent::AbilityInputTagPressed(FGameplayTag InputTag)
{
	// ---------------------------------------------------------------------
	// STAGE GATE — `InputRouted` (docs/GAS-INTEGRATION-ROADMAP.md, stage 3).
	//
	// This is the hook `BRCharacter.cpp` names as *"`InputRouted` — where the tag reaches the
	// ASC"*. The idiom is that file's, unchanged: FIRST STATEMENT IN THE FUNCTION, before even
	// the tag-validity test, so that below the stage this function is one comparison and a
	// return. `HeldInputTags` is not touched, no spec is walked, nothing is activated, and no
	// prediction key is opened.
	//
	// WHY THE **ASC** AND NOT `ABRPlayerController::AbilityInputTagPressed`, which is the other
	// end of the same relay and the more obvious place to put it. Three reasons, and the third
	// is the one that decides it:
	//
	//   1. THE REFUSAL HAS TO COME FROM THE FAR END OR IT PROVES NOTHING. Stage 3 exists to tell
	//      "the key is dead" apart from "the key is alive and the tag has nowhere to go" — this
	//      file's own `matched NO granted ability` block, twenty lines down, is written for
	//      exactly that and says so. A controller-side gate makes the log below the stage read
	//      *"the controller was reached"* and stop there, which leaves the untested half of the
	//      chain — the PlayerState arriving, `GetBRAbilitySystemComponent()` resolving, this
	//      component existing at all — unproven. That untested half IS stage 3. Refusing HERE
	//      prints a line that can only have been printed by an ASC that the tag actually reached,
	//      so the founder's first keypress below the stage still proves five of the six arrows in
	//      §3.2 are alive and names the one thing holding the sixth.
	//
	//   2. IT IS THE CHOKE POINT, so the gate is one edit and cannot be half-applied. Everything
	//      that routes ability input in this project converges on this function: the human path
	//      (`ABRPlayerController::AbilityInputTagPressed`), and `ABRBotController::PressInputTag`,
	//      whose own comment already promises *"same function, same component, same gates as
	//      ABRPlayerController's human path."*
	//
	//   3. AND THAT SECOND CALLER IS WHY THE CONTROLLER IS THE WRONG ANSWER, not merely the
	//      weaker one. At stage `Granting` the roadmap's contract is *"abilities exist; no input
	//      reaches them"* — and grants land on a BOT's ASC exactly as they land on a human's. Gate
	//      only the player controller and a bot at stage `Granting` routes its intent straight
	//      into this function and activates the abilities that stage just granted. Stage 2 and
	//      stage 3 would then land in one test run, with a bot as the second suspect, which is
	//      precisely the hole the roadmap's one rule was written to keep shut.
	//
	// WHAT THIS COSTS AT `Off`, stated because the acceptance criterion demands it: the two log
	// lines this function prints today for an ungranted tag — `PRESSED (edge)` at Log and
	// `matched NO granted ability` at Warning — become one line at Verbose. No gameplay changes,
	// because below `Granting` nothing is granted for a press to match in the first place. The
	// one diagnostic that moves below the gate with it is the INVALID-tag Warning below, and that
	// is acceptable rather than lost: an empty `InputTag` is an authoring defect in
	// `DA_InputConfig` that `UBRInputConfig::IsDataValid` reports at author time, the runtime
	// Warning is its second copy, and the gate line names the (empty) tag anyway.
	//
	// The bypassed path is not removed and not made unreachable: one ini line brings it back.
	// ---------------------------------------------------------------------
	if (!BRGas::IsStageEnabled(EBRGasStage::InputRouted))
	{
		// Verbose, so a default-verbosity playtest log stays byte-for-byte what it is today apart
		// from the one `BRGas: stage = ...` line. Present at all — and phrased to say what it
		// PROVES, not just what it refused — because a silent drop is indistinguishable from a
		// dead key, and that is the exact confusion that cost this project a day.
		UE_LOG(LogBRInput, Verbose, TEXT("BRAbilitySystemComponent '%s': %s PRESSED and reached the ASC, but the GAS stage gate is '%s'; NOT routed to any ability (needs at least 'InputRouted'). The key, the mapping context, the binding and the controller relay are all ALIVE — only the stage is holding it. Set GasStage in Config/DefaultGame.ini."),
			*GetNameSafe(GetOwner()), *InputTag.ToString(), BRGas::ToString(BRGas::GetStage()));
		return;
	}

	if (!InputTag.IsValid())
	{
		// A row with no tag has no destination. UBRInputConfig::IsDataValid reports the authoring
		// error; here we refuse rather than iterate every spec against an empty tag, which would
		// match nothing on a good day and everything on a bad one.
		UE_LOG(LogBRInput, Warning, TEXT("BRAbilitySystemComponent '%s': AbilityInputTagPressed with an INVALID tag; ignored."),
			*GetNameSafe(GetOwner()));
		return;
	}

	// THE IDEMPOTENCY GATE. Bound to ETriggerEvent::Triggered, this runs every frame the key is
	// held; AddUnique is what turns that stream into a single press edge. See the header for the
	// decided edge case (an ability cancelled while held does not auto-restart).
	const int32 NumBeforeAdd = HeldInputTags.Num();
	HeldInputTags.AddUnique(InputTag);
	if (HeldInputTags.Num() == NumBeforeAdd)
	{
		UE_LOG(LogBRInput, Verbose, TEXT("BRAbilitySystemComponent '%s': %s HELD (repeat Triggered, no edge)."),
			*GetNameSafe(GetOwner()), *InputTag.ToString());
		return;
	}

	UE_LOG(LogBRInput, Log, TEXT("BRAbilitySystemComponent '%s': %s PRESSED (edge) -> matching granted abilities."),
		*GetNameSafe(GetOwner()), *InputTag.ToString());

	// Counts the specs this tag actually reached. Nothing branches on it and nothing outside this
	// function sees it — it exists so the "matched nothing" report below can tell the difference
	// between a tag with no home and a tag whose ability simply refused.
	int32 MatchedSpecs = 0;

	// The list must not be reallocated while we hold references into it — activation can grant or
	// revoke abilities re-entrantly. This is the engine's own guard, used the engine's own way.
	ABILITYLIST_SCOPE_LOCK();
	for (FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (!Spec.Ability || !Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			continue;
		}

		++MatchedSpecs;
		Spec.InputPressed = true;

		if (Spec.IsActive())
		{
			// Already running: this is a re-press, not an activation. Tell the live instance, and
			// tell the server if the ability wants raw input replicated (held-fire, charge-ups).
			if (Spec.Ability->bReplicateInputDirectly && !IsOwnerActorAuthoritative())
			{
				ServerSetInputPressed(Spec.Handle);
			}

			AbilitySpecInputPressed(Spec);
			InvokeInputEventForSpec(Spec, EAbilityGenericReplicatedEvent::InputPressed);
		}
		else
		{
			// Not running: try to activate. TryActivateAbility is where costs, cooldowns and
			// blocking tags (State.Dead) are consulted — this function does not second-guess any
			// of them, and must not: a branch here would be a second, divergent rule set.
			TryActivateAbility(Spec.Handle);
		}
	}

	if (MatchedSpecs > 0)
	{
		return;
	}

	// -----------------------------------------------------------------------------------------
	// STAGE 3's OTHER HALF (docs/GAS-INTEGRATION-ROADMAP.md). THE LINE THIS PROJECT LOST A DAY TO.
	//
	// A key that was never bound and a key whose tag arrives here and matches NOTHING produce the
	// SAME observable: no activation, no complaint, nothing in the log. They have opposite fixes.
	// The "PRESSED (edge)" line above proves the tag reached the ASC; this line proves it had
	// nowhere to go once it did — which makes it a GRANT problem (roadmap Stage 2: the three
	// DA_AbilitySet_* assets, or a set whose tag does not match InputTag.*), never a binding one.
	//
	// It reports the granted count too, because zero-granted and wrong-tag are also different
	// problems: zero means no ability set landed at all, non-zero means the set landed and its
	// InputTag does not spell what the input config spells.
	//
	// NOT AN ERROR, and that is a judgement rather than caution: an ungranted tag is the CORRECT
	// state for a fighter holding nothing, or before the first equip completes. Warning is the
	// level for "this is almost certainly not what you wanted"; Error is reserved in this file for
	// states that are wrong no matter what the player is doing.
	// -----------------------------------------------------------------------------------------
	const FName LedgerKey(*FString::Printf(TEXT("%s|%s"), *GetNameSafe(GetOwner()), *InputTag.ToString()));
	TSet<FName>& Ledger = GetUnmatchedInputTagLedger();

	if (!Ledger.Contains(LedgerKey))
	{
		Ledger.Add(LedgerKey);
		UE_LOG(LogBRInput, Warning,
			TEXT("BRAbilitySystemComponent '%s': %s PRESSED and matched NO granted ability (%d ability(ies) granted on this ASC in total). The key is ALIVE and the tag reached the ASC — nothing on this fighter answers to that tag. Look at the GRANT (the equipped weapon's ability set, or the InputTag on its entries), not at the input bindings. Reported once per owner+tag; repeats are Verbose."),
			*GetNameSafe(GetOwner()), *InputTag.ToString(), ActivatableAbilities.Items.Num());
	}
	else
	{
		// The repeat, so "is it STILL unmatched?" stays answerable for the rest of the session
		// without the first answer being drowned by the ninetieth.
		UE_LOG(LogBRInput, Verbose,
			TEXT("BRAbilitySystemComponent '%s': %s PRESSED and matched NO granted ability (repeat; %d granted in total)."),
			*GetNameSafe(GetOwner()), *InputTag.ToString(), ActivatableAbilities.Items.Num());
	}
}

void UBRAbilitySystemComponent::AbilityInputTagReleased(FGameplayTag InputTag)
{
	// ---------------------------------------------------------------------
	// STAGE GATE — `InputRouted`. The press half above carries the argument for gating the ASC
	// rather than the controller; this half exists so the pair is SYMMETRIC, which is the whole
	// of its safety case.
	//
	// A GATE ON PRESS BUT NOT RELEASE WOULD BE THE SAME MISTAKE STAGE 2 REFUSES TO MAKE — except
	// that here the symmetric pairing is the safe one and the asymmetry is the leak, because
	// press and release are two ends of ONE piece of state (`HeldInputTags`) rather than two ends
	// of a lifetime. Gating both means nothing can enter the held set below the stage, so nothing
	// needs to leave it: the set is provably empty and `ClearAbilityInput`'s `IsEmpty` early-out
	// covers the flush. Gating only press would leave release walking the spec list and firing
	// `AbilitySpecInputReleased` on every key-up for a press this component never saw — work
	// below the stage, which the acceptance criterion forbids outright.
	//
	// There is no window in which the two halves can disagree: `BRGas::GetStage()` resolves once
	// per process, so the stage cannot change between a press and its release.
	//
	// AT `Off` this costs one Verbose line in place of the `RELEASED but was not held` Verbose
	// line it would otherwise have printed. Nothing else in this function runs.
	// ---------------------------------------------------------------------
	if (!BRGas::IsStageEnabled(EBRGasStage::InputRouted))
	{
		UE_LOG(LogBRInput, Verbose, TEXT("BRAbilitySystemComponent '%s': %s RELEASED and reached the ASC, but the GAS stage gate is '%s'; NOT routed to any ability (needs at least 'InputRouted'). Nothing was held — the matching press was refused by this same gate. Set GasStage in Config/DefaultGame.ini."),
			*GetNameSafe(GetOwner()), *InputTag.ToString(), BRGas::ToString(BRGas::GetStage()));
		return;
	}

	if (!InputTag.IsValid())
	{
		return;
	}

	if (HeldInputTags.Remove(InputTag) == 0)
	{
		// A release with no matching press. Reachable and harmless: the key was already released by
		// ClearAbilityInput on unpossess, or the press landed before this pawn's bindings existed.
		UE_LOG(LogBRInput, Verbose, TEXT("BRAbilitySystemComponent '%s': %s RELEASED but was not held; ignored."),
			*GetNameSafe(GetOwner()), *InputTag.ToString());
		return;
	}

	UE_LOG(LogBRInput, Log, TEXT("BRAbilitySystemComponent '%s': %s RELEASED (edge)."),
		*GetNameSafe(GetOwner()), *InputTag.ToString());

	ABILITYLIST_SCOPE_LOCK();
	for (FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (!Spec.Ability || !Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			continue;
		}

		Spec.InputPressed = false;

		if (Spec.IsActive())
		{
			if (Spec.Ability->bReplicateInputDirectly && !IsOwnerActorAuthoritative())
			{
				ServerSetInputReleased(Spec.Handle);
			}

			// This is what WaitInputRelease listens to, and therefore how every WhileHeld ability
			// (sprint, held fire, grenade cook) ends. If this call is ever lost, those abilities do
			// not error — they simply never stop, which is the failure mode §3.2 warns about.
			AbilitySpecInputReleased(Spec);
			InvokeInputEventForSpec(Spec, EAbilityGenericReplicatedEvent::InputReleased);
		}
	}
}

void UBRAbilitySystemComponent::InvokeInputEventForSpec(const FGameplayAbilitySpec& Spec, EAbilityGenericReplicatedEvent::Type EventType)
{
	// The prediction key must come from the LIVE INSTANCE, not from FGameplayAbilitySpec's own
	// ActivationInfo: that member is UE_DEPRECATED(5.5) and only ever applied to NonInstanced
	// abilities, which no longer exist. Our abilities are InstancedPerActor (§3.3), so the instance
	// is where the activation prediction key actually lives, and using the spec's copy would send a
	// stale or empty key — the shape of bug that makes ONE ability in ten fail to confirm.
	const TArray<UGameplayAbility*> Instances = Spec.GetAbilityInstances();
	if (Instances.IsEmpty() || !Instances.Last())
	{
		// An active spec with no instance. Only reachable for a NonInstanced ability, which this
		// project does not author. Refused rather than guessed at.
		UE_LOG(LogBRInput, Warning, TEXT("BRAbilitySystemComponent '%s': active ability '%s' has no instance; input event not invoked. Non-instanced abilities are not supported."),
			*GetNameSafe(GetOwner()), *GetNameSafe(Spec.Ability));
		return;
	}

	InvokeReplicatedEvent(EventType, Spec.Handle, Instances.Last()->GetCurrentActivationInfoRef().GetActivationPredictionKey());
}

void UBRAbilitySystemComponent::ClearAbilityInput()
{
	if (HeldInputTags.IsEmpty())
	{
		return;
	}

	// Copy first: AbilityInputTagReleased mutates HeldInputTags, and an ability ending inside it can
	// reach further code still. Iterating the live array here would be a use-after-free waiting for
	// the first ability that cancels a sibling on end.
	const TArray<FGameplayTag> TagsToRelease = HeldInputTags;

	UE_LOG(LogBRInput, Log, TEXT("BRAbilitySystemComponent '%s': clearing %d held input tag(s)."),
		*GetNameSafe(GetOwner()), TagsToRelease.Num());

	for (const FGameplayTag& Tag : TagsToRelease)
	{
		AbilityInputTagReleased(Tag);
	}

	HeldInputTags.Reset();
}

// ---------------------------------------------------------------------------
// Prediction-window helpers
// ---------------------------------------------------------------------------

void UBRAbilitySystemComponent::ExecuteInPredictionWindow(TFunctionRef<void()> Work)
{
	// FScopedPredictionWindow is a no-op on the authority (there is nothing to predict when you are
	// the truth) and opens/closes a key on a predicting client. Constructing it unconditionally is
	// what lets ability code stay authority-agnostic.
	FScopedPredictionWindow PredictionWindow(this, /*bCanGenerateNewKey=*/true);
	Work();
}

bool UBRAbilitySystemComponent::BatchRPCTryActivateAbility(FGameplayAbilitySpecHandle AbilityHandle, bool bEndAbilityImmediately)
{
	if (!AbilityHandle.IsValid())
	{
		return false;
	}

	// The batcher accumulates the activate/TargetData/end RPCs and flushes them as ONE on scope
	// exit. It only does anything when ShouldDoServerAbilityRPCBatch() is true — which it is, above.
	FScopedServerAbilityRPCBatcher Batcher(this, AbilityHandle);

	const bool bActivated = TryActivateAbility(AbilityHandle, /*bAllowRemoteActivation=*/true);

	if (bActivated && bEndAbilityImmediately)
	{
		if (FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(AbilityHandle))
		{
			// The primary instance, not the CDO: InstancedPerActor abilities keep their state on the
			// instance, and ending the CDO would end nothing.
			if (UBRGameplayAbility* BRAbility = Cast<UBRGameplayAbility>(Spec->GetPrimaryInstance()))
			{
				// The reason UBRGameplayAbility::ExternalEndAbility exists at all — see its comment.
				BRAbility->ExternalEndAbility();
			}
			else
			{
				// Refused rather than fudged with CancelAbilityHandle: a cancel is not an end, and
				// substituting one here would silently change what every task's cleanup sees.
				UE_LOG(LogBRCombat, Warning, TEXT("BRAbilitySystemComponent '%s': bEndAbilityImmediately requested for an ability that is not a UBRGameplayAbility; NOT ended. Only our base has a public external end."),
					*GetNameSafe(GetOwner()));
			}
		}
	}

	return bActivated;
}

// ---------------------------------------------------------------------------
// The RecentDamage regen gate
// ---------------------------------------------------------------------------

bool UBRAbilitySystemComponent::ApplyRecentDamageGate()
{
	if (!RecentDamageEffectClass)
	{
		// LOUD, not silent: with no gate effect, damage lands and shields regenerate through
		// sustained fire — a difference a playtester would feel and never be able to name.
		UE_LOG(LogBRCombat, Warning, TEXT("BRAbilitySystemComponent '%s': damage landed but RecentDamageEffectClass is UNSET — State.Combat.RecentDamage was NOT applied and shield regen is UNGATED."),
			*GetNameSafe(GetOwner()));
		return false;
	}

	// The gate duration is a gameplay number, so it comes from CT_Combat and not from the effect.
	float DelaySeconds = 0.f;
	if (!BRCombatCurves::Evaluate(BRCombatCurves::Names::ShieldsRegenDelaySeconds, DelaySeconds) || DelaySeconds <= 0.f)
	{
		// REFUSED. A zero-length gate applies successfully and blocks nothing: shields would
		// regenerate mid-fight and the log would say the gate was applied. Better to have no gate
		// and one Error than a gate that lies.
		UE_LOG(LogBRCombat, Error, TEXT("BRAbilitySystemComponent '%s': CT_Combat has no usable '%s' curve; the RecentDamage gate was NOT applied. Shield regen is UNGATED."),
			*GetNameSafe(GetOwner()), *BRCombatCurves::Names::ShieldsRegenDelaySeconds.ToString());
		return false;
	}

	// Self-applied on the server (this runs from PostGameplayEffectExecute, which is authority
	// only). MakeOutgoingSpec/ApplyGameplayEffectSpecToSelf is the ONE lawful path — nothing here
	// touches a tag directly, because a loose tag would not replicate and the regen gate has to be
	// visible to the GE that blocks on it (gas-purity.md law 5).
	const FGameplayEffectContextHandle Context = MakeEffectContext();
	const FGameplayEffectSpecHandle SpecHandle = MakeOutgoingSpec(RecentDamageEffectClass, /*Level=*/1.f, Context);
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		UE_LOG(LogBRCombat, Error, TEXT("BRAbilitySystemComponent '%s': failed to build a spec for RecentDamageEffectClass '%s'."),
			*GetNameSafe(GetOwner()), *GetNameSafe(RecentDamageEffectClass));
		return false;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(UBRGE_RecentDamage::DurationSetByCallerName, DelaySeconds);

	ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	return true;
}

// ---------------------------------------------------------------------------
// Shields-broken state, and the life-cycle effects
// ---------------------------------------------------------------------------

bool UBRAbilitySystemComponent::HasActiveEffectOfClass(TSubclassOf<UGameplayEffect> EffectClass) const
{
	if (!EffectClass)
	{
		return false;
	}

	FGameplayEffectQuery Query;
	Query.EffectDefinition = EffectClass;
	return GetActiveEffects(Query).Num() > 0;
}

void UBRAbilitySystemComponent::SetShieldsBrokenState(bool bBroken)
{
	if (!ShieldsBrokenEffectClass)
	{
		UE_LOG(LogBRCombat, Warning, TEXT("BRAbilitySystemComponent '%s': ShieldsBrokenEffectClass is UNSET — State.Shields.Broken is never applied."), *GetNameSafe(GetOwner()));
		return;
	}

	const bool bCurrentlyBroken = HasActiveEffectOfClass(ShieldsBrokenEffectClass);
	if (bCurrentlyBroken == bBroken)
	{
		// The idempotency gate. Called from a transition, but transitions can be reported twice
		// (two hits in one frame), and a second infinite effect would need two removals to clear.
		return;
	}

	if (bBroken)
	{
		const FGameplayEffectSpecHandle SpecHandle = MakeOutgoingSpec(ShieldsBrokenEffectClass, /*Level=*/1.f, MakeEffectContext());
		if (SpecHandle.IsValid() && SpecHandle.Data.IsValid())
		{
			ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		}
	}
	else
	{
		RemoveActiveGameplayEffectBySourceEffect(ShieldsBrokenEffectClass, this);
	}

	UE_LOG(LogBRCombat, Verbose, TEXT("BRAbilitySystemComponent '%s': shields %s."), *GetNameSafe(GetOwner()), bBroken ? TEXT("BROKEN") : TEXT("restored"));
}

bool UBRAbilitySystemComponent::ApplyInitStats()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(LogBRCombat, Error, TEXT("BRAbilitySystemComponent '%s': ApplyInitStats without authority — REFUSED. Attribute initialisation is server truth."), *GetNameSafe(GetOwner()));
		return false;
	}

	if (!InitStatsEffectClass)
	{
		UE_LOG(LogBRCombat, Error, TEXT("BRAbilitySystemComponent '%s': InitStatsEffectClass is UNSET; attributes stay at zero."), *GetNameSafe(GetOwner()));
		return false;
	}

	float MaxHealth = 0.f;
	float MaxShields = 0.f;
	float MaxGrenades = 0.f;
	const bool bHaveHealth = BRCombatCurves::Evaluate(BRCombatCurves::Names::FighterMaxHealth, MaxHealth);
	const bool bHaveShields = BRCombatCurves::Evaluate(BRCombatCurves::Names::FighterMaxShields, MaxShields);
	const bool bHaveGrenades = BRCombatCurves::Evaluate(FighterMaxGrenadesCurve, MaxGrenades);

	if (!bHaveHealth || !bHaveShields || MaxHealth <= 0.f)
	{
		// REFUSED rather than defaulted. A fighter that spawns with an invented health pool is a
		// balance change nobody made; a fighter that spawns uninitialised is a bug report.
		UE_LOG(LogBRCombat, Error, TEXT("BRAbilitySystemComponent '%s': CT_Combat is missing '%s' or '%s' (read %.2f / %.2f); GE_InitStats NOT applied and this fighter is UNINITIALISED."),
			*GetNameSafe(GetOwner()), *BRCombatCurves::Names::FighterMaxHealth.ToString(), *BRCombatCurves::Names::FighterMaxShields.ToString(), MaxHealth, MaxShields);
		return false;
	}

	if (!bHaveGrenades || MaxGrenades < 0.f)
	{
		// REFUSED, and the SAME refusal health and shields get, for the same reason: the count is a
		// gameplay number and this file may not invent one. Two grenades is a design decision that
		// lives in `Fighter.MaxGrenades`; a `2.f` typed here would be a law-3 violation AND a second
		// source of truth that silently beats the CSV on every spawn.
		//
		// THE WHOLE SPEC IS ABANDONED, not just the grenade half. Applying GE_InitStats with the
		// grenade keys unset would not leave grenades untouched — both modifiers are `Override` and a
		// missing SetByCaller evaluates to ZERO, so a fighter would be initialised to a full health
		// bar and an empty pouch, every respawn, quietly. Refusing the whole effect means the fighter
		// is visibly uninitialised, which is the failure this function already knows how to report.
		//
		// Note `< 0.f` and not `<= 0.f`, unlike MaxHealth: a capacity of zero grenades is ABSURD for
		// health (a fighter who cannot exist) but is a legal design for a mode that ships without
		// grenades. Zero here means "the row says none"; a missing row means "nobody decided".
		UE_LOG(LogBRCombat, Error, TEXT("BRAbilitySystemComponent '%s': CT_Combat is missing a usable '%s' (read %.2f); GE_InitStats NOT applied and this fighter is UNINITIALISED. The grenade count is data and is not invented here."),
			*GetNameSafe(GetOwner()), *FighterMaxGrenadesCurve.ToString(), MaxGrenades);
		return false;
	}

	const FGameplayEffectSpecHandle SpecHandle = MakeOutgoingSpec(InitStatsEffectClass, /*Level=*/1.f, MakeEffectContext());
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		UE_LOG(LogBRCombat, Error, TEXT("BRAbilitySystemComponent '%s': failed to build a GE_InitStats spec."), *GetNameSafe(GetOwner()));
		return false;
	}

	// All SIX, always. A partial set would leave one attribute at whatever the last life left it,
	// and GE_InitStats' whole job is that a respawn is not "the previous life minus the damage".
	// Spawning at FULL health, shields and grenades is the design (Halo); it is not a separate
	// number, which is why MaxGrenades is written to both keys exactly as MaxHealth is.
	//
	// ORDER IS IRRELEVANT HERE and load-bearing in the effect. These are named key/value writes onto
	// a spec, not modifiers; the ordering that matters (capacities before current values) is the
	// Modifiers array in UBRGE_InitStats' constructor. Rearranging these six lines changes nothing —
	// rearranging those six changes everything.
	SpecHandle.Data->SetSetByCallerMagnitude(UBRGE_InitStats::MaxHealthName, MaxHealth);
	SpecHandle.Data->SetSetByCallerMagnitude(UBRGE_InitStats::MaxShieldsName, MaxShields);
	SpecHandle.Data->SetSetByCallerMagnitude(UBRGE_InitStats::MaxGrenadesName, MaxGrenades);
	SpecHandle.Data->SetSetByCallerMagnitude(UBRGE_InitStats::HealthName, MaxHealth);
	SpecHandle.Data->SetSetByCallerMagnitude(UBRGE_InitStats::ShieldsName, MaxShields);
	SpecHandle.Data->SetSetByCallerMagnitude(UBRGE_InitStats::GrenadesName, MaxGrenades);

	ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());

	// -----------------------------------------------------------------------------------------
	// STAGE 1's EXIT CRITERION (docs/GAS-INTEGRATION-ROADMAP.md).
	//
	// Every other log in this function is a REFUSAL, so until this line existed a successful
	// initialisation was byte-for-byte indistinguishable from one that never ran — and on a fresh
	// binary "never ran" is by far the likelier of the two. Silence was the wrong success shape.
	//
	// READ BACK, DO NOT RESTATE, and this is the whole value of the line. The six numbers below
	// come from the ATTRIBUTE SET after application, not from the three floats this function read
	// out of CT_Combat. PreAttributeChange clamps Health to GetMaxHealth() *as it currently is*,
	// so a perfectly good table still resolves to `Health=0/100` if GE_InitStats' modifier order
	// ever regresses (the trap written up in UBRAttributeSet::PreAttributeChange and asserted in
	// UBRGE_InitStats' constructor). Echoing MaxHealth back would have printed 100/100 and hidden
	// exactly the failure this stage exists to catch. The CT_Combat values are printed too, in
	// their own clause, so "the table is wrong" and "the effect is wrong" are told apart on one
	// line instead of two runs.
	//
	// Log, not Verbose: this happens once per life, on a spawn — it is not traffic.
	// -----------------------------------------------------------------------------------------
	if (const UBRAttributeSet* Attributes = GetSet<UBRAttributeSet>())
	{
		UE_LOG(LogBRCombat, Log,
			TEXT("BRAbilitySystemComponent '%s': BRAttributeSet init — Health=%g/%g Shields=%g/%g Grenades=%g/%g (CT_Combat gave MaxHealth=%g MaxShields=%g MaxGrenades=%g)."),
			*GetNameSafe(GetOwner()),
			Attributes->GetHealth(), Attributes->GetMaxHealth(),
			Attributes->GetShields(), Attributes->GetMaxShields(),
			Attributes->GetGrenades(), Attributes->GetMaxGrenades(),
			MaxHealth, MaxShields, MaxGrenades);
	}
	else
	{
		// GE_InitStats applied against an ASC that has no UBRAttributeSet registered. The effect
		// "succeeds" and modifies nothing, which is the quietest possible total failure: every
		// number the rest of the game reads off this fighter is zero, CheckForDeath refuses to
		// kill it, and nothing else in the project would ever say why.
		UE_LOG(LogBRCombat, Error,
			TEXT("BRAbilitySystemComponent '%s': GE_InitStats was applied but this ASC has NO UBRAttributeSet registered — the effect modified NOTHING and every attribute is still zero. The set is a PlayerState subobject; this is a construction problem, not a data one."),
			*GetNameSafe(GetOwner()));
	}

	// Shields are full again, so the broken state must go with the same call that filled them —
	// otherwise a respawned fighter carries State.Shields.Broken with full shields.
	SetShieldsBrokenState(false);

	// --- and the regen that runs for the rest of the life -------------------------------------
	if (!ShieldRegenEffectClass)
	{
		UE_LOG(LogBRCombat, Warning, TEXT("BRAbilitySystemComponent '%s': ShieldRegenEffectClass is UNSET; shields will never recharge."), *GetNameSafe(GetOwner()));
		return true;
	}

	if (HasActiveEffectOfClass(ShieldRegenEffectClass))
	{
		// GE_Regen is INFINITE. Applying a second one on respawn would double the regen rate for
		// the rest of the match, and nothing would look wrong until someone timed a recharge.
		return true;
	}

	float RatePerSecond = 0.f;
	float PeriodSeconds = 0.f;
	if (!BRCombatCurves::Evaluate(BRCombatCurves::Names::ShieldsRegenRatePerSecond, RatePerSecond) || RatePerSecond <= 0.f
		|| !BRCombatCurves::Evaluate(BRCombatCurves::Names::ShieldsRegenPeriodSeconds, PeriodSeconds) || PeriodSeconds <= 0.f)
	{
		UE_LOG(LogBRCombat, Error, TEXT("BRAbilitySystemComponent '%s': CT_Combat is missing a usable '%s' or '%s' (read %.2f / %.2f); GE_Regen NOT applied and shields will never recharge."),
			*GetNameSafe(GetOwner()), *BRCombatCurves::Names::ShieldsRegenRatePerSecond.ToString(), *BRCombatCurves::Names::ShieldsRegenPeriodSeconds.ToString(), RatePerSecond, PeriodSeconds);
		return true;
	}

	const FGameplayEffectSpecHandle RegenSpec = MakeOutgoingSpec(ShieldRegenEffectClass, /*Level=*/1.f, MakeEffectContext());
	if (RegenSpec.IsValid() && RegenSpec.Data.IsValid())
	{
		// PERIOD AND RATE ARE SEPARATE DATA and this is where they meet: the CSV states points per
		// SECOND, the effect ticks every Period, so the per-tick magnitude is the product. Change
		// the period in the CSV and the rate per second does not move — which is what makes the
		// period a fidelity knob and not a balance knob.
		RegenSpec.Data->Period = PeriodSeconds;
		RegenSpec.Data->SetSetByCallerMagnitude(BRGameplayTags::SetByCaller_RegenRate, RatePerSecond * PeriodSeconds);
		ApplyGameplayEffectSpecToSelf(*RegenSpec.Data.Get());
	}

	return true;
}

bool UBRAbilitySystemComponent::ApplyDeathEffect()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(LogBRCombat, Error, TEXT("BRAbilitySystemComponent '%s': ApplyDeathEffect without authority — REFUSED."), *GetNameSafe(GetOwner()));
		return false;
	}

	if (!DeathEffectClass)
	{
		UE_LOG(LogBRCombat, Error, TEXT("BRAbilitySystemComponent '%s': DeathEffectClass is UNSET; State.Dead is never applied and NOTHING blocks a dead fighter's abilities."), *GetNameSafe(GetOwner()));
		return false;
	}

	if (HasActiveEffectOfClass(DeathEffectClass))
	{
		// Already dead. One death, one effect — the attribute set's latch makes this unreachable in
		// the normal path, and this makes it harmless in the abnormal one.
		return false;
	}

	const FGameplayEffectSpecHandle SpecHandle = MakeOutgoingSpec(DeathEffectClass, /*Level=*/1.f, MakeEffectContext());
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		return false;
	}

	ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());

	// Every verb is now blocked by ActivationBlockedTags on UBRGameplayAbility. Cancel what is
	// already RUNNING — blocking activation says nothing about an ability already active, and a
	// sprint started before death would otherwise hold the CMC at sprint speed on a corpse.
	CancelAbilities(/*WithTags=*/nullptr, /*WithoutTags=*/nullptr, /*Ignore=*/nullptr);

	return true;
}

void UBRAbilitySystemComponent::ClearDeathEffect()
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !DeathEffectClass)
	{
		return;
	}

	RemoveActiveGameplayEffectBySourceEffect(DeathEffectClass, this);
}
