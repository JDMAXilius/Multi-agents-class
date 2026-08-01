// Breachpoint. THE attribute set: shields over health, one damage entry point, one death.

#include "AbilitySystem/BRAttributeSet.h"

#include "GameplayEffect.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

#include "AbilitySystem/BRAbilitySystemComponent.h"
#include "Core/BRCore.h"
#include "Core/BRGameplayTags.h"

UBRAttributeSet::UBRAttributeSet()
{
	// Every attribute starts at zero, and zero is NOT a tuning number — it is "uninitialized".
	// Real values arrive from GE_InitStats (step 4), whose magnitudes come from data. A starting
	// health typed here would be a law-3 violation AND a second source of truth that silently wins
	// on any pawn GE_InitStats fails to reach.
	//
	// The consequence is deliberate and load-bearing: an ASC that GE_InitStats never touched has
	// MaxHealth == 0, and CheckForDeath refuses to kill it. See that function.
}

void UBRAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// §6.1: Shields/Health (+Max) are COND_None — public bars. An enemy's shield state is
	// information the game intends every player to have (it drives the whole Halo-style push/retreat
	// read), so hiding it would be a design change, not a bandwidth saving.
	//
	// REPNOTIFY_Always, not the default: with Mixed replication a predicted change can leave the
	// client's value already equal to the incoming one, and the default (OnChanged) would then skip
	// the notify and leave the prediction unresolved. Always is the prediction-safe setting.
	DOREPLIFETIME_CONDITION_NOTIFY(UBRAttributeSet, Shields, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBRAttributeSet, MaxShields, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBRAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBRAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);

	// COND_OwnerOnly, and the difference from the four above is the point: a shield bar is public
	// information the mode is built around, a grenade count is not. Only the owner's HUD reads it,
	// and only the owner predicts spending it — so only the owner is sent it.
	//
	// REPNOTIFY_Always matters MORE here than above, not less. The grenade spend is a PREDICTED
	// Instant cost: the client has already decremented locally by the time the server's value
	// arrives, so the two are frequently equal and OnChanged would skip the notify — leaving the
	// predicted modification unresolved on the exact attribute whose whole point is that a rejected
	// throw refunds itself.
	DOREPLIFETIME_CONDITION_NOTIFY(UBRAttributeSet, Grenades, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBRAttributeSet, MaxGrenades, COND_OwnerOnly, REPNOTIFY_Always);

	// IncomingDamage is absent on purpose. See its declaration.
}

// ---------------------------------------------------------------------------
// Clamps. Runs on client and server, and again on every prediction replay.
// ---------------------------------------------------------------------------

void UBRAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
	else if (Attribute == GetShieldsAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxShields());
	}
	else if (Attribute == GetGrenadesAttribute())
	{
		// THE WHOLE RULE FOR GRENADES IS THIS LINE. A floor of zero is what makes "you cannot throw
		// what you do not have" true even if something ever applies the cost without checking it
		// first, and the MaxGrenades ceiling is what stops a pickup from over-filling the pouch.
		//
		// Note what is NOT here and must not be added: rounding to an integer. A count reads like an
		// int, but a clamp is the wrong place to decide that — GE_GrenadeCost spends whole numbers
		// from data, and quantising here would silently absorb a fractional cost bug instead of
		// letting it show. If partial grenades ever become expressible, that is a ruling, not a
		// FMath::RoundToFloat someone added to a clamp.
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxGrenades());
	}
	else if (Attribute == GetMaxHealthAttribute() || Attribute == GetMaxShieldsAttribute() || Attribute == GetMaxGrenadesAttribute())
	{
		// A negative capacity would make every clamp above invert (Clamp with Min > Max is
		// undefined-ish and certainly not what anyone meant). Floor at zero and let the invariant
		// hold everywhere else.
		NewValue = FMath::Max(NewValue, 0.f);
	}

	// NOTE FOR STEP 4, AND IT IS A REAL TRAP, NOT A STYLE NOTE: because Health is clamped to
	// GetMaxHealth() *as it currently is*, GE_InitStats MUST order its modifiers so MaxHealth,
	// MaxShields and MaxGrenades are set BEFORE Health, Shields and Grenades. Set Health first on a
	// fresh set and it clamps to MaxHealth == 0 — a fighter that spawns at zero health, from a table
	// with correct numbers in it. The alternative (skip the clamp while Max is zero) was considered
	// and rejected: it would leave "Health <= MaxHealth" conditionally true, and that invariant is
	// exactly what the pinned suites assert.
	//
	// GRENADES INHERIT THE TRAP EXACTLY, and it bites harder because it is quieter: a fighter who
	// spawns at zero health dies instantly and someone files a bug in the first minute, while a
	// fighter who spawns with zero grenades just... never throws one, and it reads as a missing
	// feature rather than an ordering bug. The order is asserted in GE_InitStats' constructor.
	//
	// Equally deliberate: raising MaxHealth does NOT scale current Health, and lowering it clamps
	// Health down on the next change rather than immediately. Breachpoint has no max-health
	// modifiers, so there is nothing to rescale; if one is ever proposed, the rescale rule is a
	// design decision that belongs in a ruling, not an inference made here.
}

// ---------------------------------------------------------------------------
// The one reaction point. Server-only.
// ---------------------------------------------------------------------------

void UBRAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		// CONSUME THE META ATTRIBUTE FIRST, unconditionally, before any branch that could return.
		// A leftover IncomingDamage would be added to the next hit — the classic GAS meta-attribute
		// bug, and an exploit rather than a glitch, because it is triggerable on demand.
		const float RawDamage = GetIncomingDamage();
		SetIncomingDamage(0.f);

		if (RawDamage < 0.f)
		{
			// REFUSED, not reinterpreted. Negative damage is not healing: healing is a Health
			// modifier on its own effect, with its own tags, and letting damage run backwards would
			// make every Damage.* multiplier in BRDamageExecCalc a potential heal. Error-level
			// because reaching here means a data row or an exec calc is wrong.
			UE_LOG(LogBRCombat, Error, TEXT("BRAttributeSet '%s': IncomingDamage was NEGATIVE (%.3f) from effect '%s' — REFUSED. Damage is never negative; healing is a Health modifier."),
				*GetNameSafe(GetOwningActor()), RawDamage, *GetNameSafe(Data.EffectSpec.Def));
			return;
		}

		if (RawDamage == 0.f)
		{
			// Legal and silent: a fully-mitigated hit is still a hit that did nothing. It applies no
			// regen gate, because gating regen on a zero-damage hit would let a harmless source
			// suppress shields indefinitely.
			return;
		}

		const float OverflowToHealth = ApplyIncomingDamageShieldsFirst(RawDamage);

		// The regen gate: State.Combat.RecentDamage, applied BY A GAMEPLAYEFFECT (law 5) through
		// the ASC. Applied for any damage that landed, whether the shields ate it or not — being
		// shot at is what gates regen, not being hurt by it.
		if (UBRAbilitySystemComponent* BRASC = Cast<UBRAbilitySystemComponent>(GetOwningAbilitySystemComponent()))
		{
			BRASC->ApplyRecentDamageGate();
		}

		UE_LOG(LogBRCombat, Verbose, TEXT("BRAttributeSet '%s': %.2f damage -> shields now %.2f/%.2f, health now %.2f/%.2f (%.2f overflowed to health)."),
			*GetNameSafe(GetOwningActor()), RawDamage, GetShields(), GetMaxShields(), GetHealth(), GetMaxHealth(), OverflowToHealth);

		UpdateShieldsBrokenState();
		CheckForDeath(Data);
		return;
	}

	if (Data.EvaluatedData.Attribute == GetShieldsAttribute())
	{
		// A DIRECT shields modifier — GE_Regen's periodic tick, GE_InitStats on spawn. Damage never
		// arrives here (it arrives as IncomingDamage above), but the broken-state assertion must
		// still run: this is the path that clears State.Shields.Broken when regen brings shields
		// back, and it is the ONLY one.
		UpdateShieldsBrokenState();
		return;
	}

	if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		// A DIRECT health modifier — GE_InitStats on spawn, a future health pickup. Damage never
		// arrives here (it arrives as IncomingDamage above), but death detection still must run:
		// otherwise a health-draining effect could reach zero without anyone dying.
		CheckForDeath(Data);
		return;
	}

	// THERE IS DELIBERATELY NO `Grenades` BRANCH, and its absence is a decision rather than an
	// omission — stated here because the next reader will otherwise "complete the pattern".
	//
	// Every branch above exists because an attribute change has a CONSEQUENCE the rest of the game
	// must learn about: shields hitting zero grants a tag, health hitting zero fires a death. A
	// grenade spend has none. The count going down means the fighter has fewer grenades, and the
	// clamp in PreAttributeChange is the entire rule — nothing is applied, nothing is removed,
	// nothing is broadcast. A branch here would be a reaction point with no reaction in it, and the
	// first person to need one would put it somewhere this file could not see.
	//
	// The one thing that DOES need to know is the owner's HUD, and it learns through OnRep_Grenades
	// and the ASC's attribute-change delegate — on the client, where the HUD is. Reacting on the
	// server here would tell nobody anything.
}

// ---------------------------------------------------------------------------
// The shields-first split
// ---------------------------------------------------------------------------

float UBRAttributeSet::ApplyIncomingDamageShieldsFirst(float RawDamage)
{
	// THE RULE, in three lines, with one invariant: AbsorbedByShields + OverflowToHealth ==
	// RawDamage, exactly, for every input. No damage is created and none is lost at the boundary —
	// the shield/health seam is the one place a rounding shortcut would be invisible and would
	// change TTK.
	const float ShieldsBefore = GetShields();
	const float AbsorbedByShields = FMath::Min(ShieldsBefore, RawDamage);
	const float OverflowToHealth = RawDamage - AbsorbedByShields;

	// Shields are reduced first and always (AbsorbedByShields is zero when they are already down,
	// which writes the same value back — cheap, and it keeps this branch-free).
	SetShields(ShieldsBefore - AbsorbedByShields);

	if (OverflowToHealth > 0.f)
	{
		// PreAttributeChange floors this at zero; the FMath::Max is not redundant defensiveness but
		// intent: overkill is DISCARDED here, so "how much did the killing blow overkill by" is not
		// silently recoverable from Health. Anything that needs it reads the killing spec.
		SetHealth(FMath::Max(GetHealth() - OverflowToHealth, 0.f));
	}

	return OverflowToHealth;
}

void UBRAttributeSet::UpdateShieldsBrokenState()
{
	UBRAbilitySystemComponent* BRASC = Cast<UBRAbilitySystemComponent>(GetOwningAbilitySystemComponent());
	if (!BRASC)
	{
		return;
	}

	if (GetMaxShields() <= 0.f)
	{
		// EXPLICIT REFUSAL, the same one CheckForDeath makes and for the same reason: MaxShields == 0
		// means GE_InitStats has not run, so Shields == 0 is "uninitialised", not "broken". Applying
		// the tag here would mark every fighter shields-broken for the instant between spawning and
		// being initialised, and anything reacting to the tag (a cue, a bot's aggression) would fire.
		return;
	}

	// A state assertion, not a transition: the ASC's setter compares against the live effect and
	// does nothing when they already agree. That is what makes "shields hit zero twice in one
	// frame" and "regen crossed zero upward" the same code path.
	BRASC->SetShieldsBrokenState(GetShields() <= 0.f);
}

// ---------------------------------------------------------------------------
// Death — detected here, decided nowhere else
// ---------------------------------------------------------------------------

void UBRAttributeSet::CheckForDeath(const FGameplayEffectModCallbackData& Data)
{
	if (GetHealth() > 0.f)
	{
		// Alive. Re-arm the latch so the NEXT death fires — this is what makes respawn work without
		// anyone remembering to reset anything: GE_InitStats raises Health above zero and the latch
		// clears as a consequence.
		bDeathReported = false;
		return;
	}

	if (GetMaxHealth() <= 0.f)
	{
		// EXPLICIT REFUSAL, not a fallthrough. MaxHealth == 0 means GE_InitStats has never run on
		// this ASC, so Health == 0 means "uninitialized", not "dead". Killing here would fire
		// Event.Death during spawn — a bug that would look like random spawn deaths and would be
		// blamed on everything except the attribute set.
		UE_LOG(LogBRCombat, Warning, TEXT("BRAttributeSet '%s': Health is 0 but MaxHealth is 0 — treating as UNINITIALIZED, not dead. GE_InitStats has not run on this ASC."),
			*GetNameSafe(GetOwningActor()));
		return;
	}

	if (bDeathReported)
	{
		// The second hit on a corpse, or two effects landing the same frame. One death, one event.
		UE_LOG(LogBRCombat, Verbose, TEXT("BRAttributeSet '%s': health at zero and death already reported this life; no second Event.Death."),
			*GetNameSafe(GetOwningActor()));
		return;
	}

	bDeathReported = true;

	AActor* Victim = GetOwningActor();
	const FGameplayEffectContextHandle Context = Data.EffectSpec.GetContext();
	AActor* Instigator = Context.GetOriginalInstigator();
	AActor* Causer = Context.GetEffectCauser();

	UE_LOG(LogBRCombat, Log, TEXT("BRAttributeSet '%s': DEATH. Killer '%s', causer '%s', effect '%s'."),
		*GetNameSafe(Victim), *GetNameSafe(Instigator), *GetNameSafe(Causer), *GetNameSafe(Data.EffectSpec.Def));

	// Seam one: the gameplay event. Reaches abilities with an Event.Death trigger on this ASC (the
	// pawn's ragdoll/cue reaction, §3.4's "death is a consequence, not a decision").
	if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
	{
		FGameplayEventData Payload;
		Payload.EventTag = BRGameplayTags::Event_Death;
		Payload.Instigator = Instigator;
		Payload.Target = Victim;
		Payload.ContextHandle = Context;
		Payload.EventMagnitude = Data.EvaluatedData.Magnitude;

		ASC->HandleGameplayEvent(BRGameplayTags::Event_Death, &Payload);
	}

	// Seam two: the C++ delegate, for systems that are not ASCs. GameMode (BP04) binds this for
	// scoring, kill attribution and the respawn timer.
	//
	// WHAT THIS FUNCTION DOES NOT DO, and must never start doing: apply GE_Death. Death is a
	// GameplayEffect (gas-purity.md law 8) applied by the authority that also decides scoring and
	// respawn — one mechanism, one owner. The attribute set REPORTS death; it does not administer
	// it. If GE_Death is ever applied from here, the ability base's State.Dead block and GameMode's
	// respawn will have two masters.
	OnDeath.Broadcast(Victim, Instigator, Causer, Data.EffectSpec);
}

// ---------------------------------------------------------------------------
// RepNotifies. GAMEPLAYATTRIBUTE_REPNOTIFY is not boilerplate: it hands the OLD value to the ASC
// so predicted changes reconcile instead of double-applying.
// ---------------------------------------------------------------------------

void UBRAttributeSet::OnRep_Shields(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBRAttributeSet, Shields, OldValue);
}

void UBRAttributeSet::OnRep_MaxShields(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBRAttributeSet, MaxShields, OldValue);
}

void UBRAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBRAttributeSet, Health, OldValue);
}

void UBRAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBRAttributeSet, MaxHealth, OldValue);
}

void UBRAttributeSet::OnRep_Grenades(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBRAttributeSet, Grenades, OldValue);
}

void UBRAttributeSet::OnRep_MaxGrenades(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBRAttributeSet, MaxGrenades, OldValue);
}
