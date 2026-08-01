// Breachpoint. Breachpoint.Sim.Shields -- the R5 pillars, pinned so they cannot move quietly.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "Engine/CurveTable.h"
#include "Engine/Engine.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameplayEffect.h"
#include "GameplayEffectComponents/TargetTagRequirementsGameplayEffectComponent.h"
#include "GameplayTagContainer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ScalableFloat.h"
#include "UObject/UObjectHash.h"

#include "AbilitySystem/BRAbilitySystemComponent.h"
#include "AbilitySystem/BRAttributeSet.h"
#include "AbilitySystem/BRCombatCurves.h"
#include "AbilitySystem/Effects/BRGameplayEffects.h"
#include "Core/BRGameplayTags.h"

/**
 * ===========================================================================================
 * WHAT THIS SUITE IS FOR
 * ===========================================================================================
 *
 * `docs/DESIGN-RULINGS.md` **R5** says: *shields-first, no health regen, 2.5 s recharge delay
 * are PILLARS, not tunables. Proposals touching them go to the founder, not through the
 * pipeline.* A pillar that no test asserts is a pillar held up by everyone remembering it.
 * This file is the mechanism that replaces the remembering.
 *
 * The five rules pinned here, and where each one actually lives:
 *
 *   1. THE RECHARGE GATE -- recharge is inhibited while `State.Combat.RecentDamage` is on the
 *      target, and that tag lasts `Shields.Regen.DelaySeconds` = **2.5 s** (`CT_Combat.csv`).
 *      Lives in `UBRGE_Regen`'s ongoing tag requirements + `UBRGE_RecentDamage`'s duration +
 *      `UBRAbilitySystemComponent::ApplyRecentDamageGate`.
 *   2. RECHARGE RATE AND THE CLAMP -- the per-tick magnitude is rate x period (so the period is
 *      a fidelity knob, never a balance knob), shields stop at MaxShields, and **nothing in the
 *      effect library regenerates Health**.
 *   3. SHIELDS-FIRST ORDERING -- `UBRAttributeSet::ApplyIncomingDamageShieldsFirst`: shields are
 *      consumed to zero before one point reaches Health, and the seam neither creates nor loses
 *      damage.
 *   4. THE ShieldsBroken TRANSITION -- `State.Shields.Broken` is granted exactly ONCE on the
 *      break and not again per hit. Bots read this (**R12**: break-off on shield-crack is a
 *      legibility rule), so a double-fire is a real defect, not a cosmetic one.
 *   5. THE DEATH BOUNDARY -- Health reaching zero reports death exactly once, Health never goes
 *      negative, overkill is discarded, and the corpse does not recharge.
 *
 * ===========================================================================================
 * FOUR DELIBERATE CHOICES, NAMED HERE SO A REVIEWER DOES NOT HAVE TO INFER THEM
 * ===========================================================================================
 *
 * **(a) TIME IS AN INPUT, NEVER A CLOCK.** Not one test advances a world tick, sleeps, or reads
 * wall-clock. The 2.5 s delay is pinned as *data* (the curve value) and as *structure* (which
 * tag inhibits which effect); "the gate expired" is simulated by REMOVING the gate effect, which
 * is the same state transition the duration would produce and is reproducible on any machine at
 * any frame rate. A spec that slept for 2.5 s would be slower, flakier, and would still not
 * prove the delay is 2.5 rather than 2.4.
 *
 * **(b) THE NUMBERS COME FROM THE SHIPPED CSV, NOT FROM LITERALS HERE.** `BeforeEach` reads
 * `Content/Data/CT_Combat.csv` off disk, builds a CurveTable from it in memory, and installs it
 * through `BRCombatCurves::SetTableOverrideForTests` -- the test seam that header already
 * documents. So this suite pins the file designers actually edit. The ONE hard-coded gameplay
 * number in this file is **2.5**, and it is hard-coded precisely because R5 says it is a pillar:
 * if it changes, this suite must go red and the change must be argued at the founder, not
 * absorbed by a test that reads whatever the CSV happens to say.
 *
 * Everything else -- MaxShields, MaxHealth, regen rate, regen period -- is read from the table and
 * asserted RELATIONALLY. A legitimate balance change to a tunable must not turn this suite red;
 * a change to a pillar must.
 *
 * **(c) DAMAGE ENTERS THROUGH THE ONE PIPELINE, WITH NO `Damage.*` TAG.** Every hit below is a
 * real `UBRGE_Damage` spec applied through `UBRGE_Damage::ApplyToTarget` -> `UBRDamageExecCalc`
 * (gas-purity.md law 3 -- there is no second path and this suite does not invent one). The specs
 * carry NO `Damage.*` tag, which makes `ComputeFinalDamage`'s multiplier the empty product, 1.0,
 * for structural reasons rather than for tuning reasons. That is what lets the arithmetic below
 * be pinned EXACTLY while staying completely independent of `Damage.Kinetic.Multiplier` and
 * friends -- those are the damage RULE's numbers and they belong to `Breachpoint.Sim.Combat`
 * (`BRCombatSpec.cpp`, a sibling file this one neither reads nor assumes). One test deliberately
 * does use `Damage.Kinetic` and asserts only shape, not magnitude, to prove the realistic path
 * reaches the same shields-first rule.
 *
 * **(d) NO ATTRIBUTE IS EVER WRITTEN DIRECTLY.** Not once. `SetShields`/`SetHealth`/`InitShields`
 * exist and are public, and calling them here would both violate gas-purity.md law 1 and hide
 * the very hooks under test. Every state change in this file is a GameplayEffect: `GE_InitStats`
 * via `ApplyInitStats()`, `GE_Damage` via the pipeline, `GE_Death` via `ApplyDeathEffect()`, and
 * two transient effects built at runtime (a shield grant and nothing else) where the shipped
 * library has no non-periodic way to add shields.
 *
 * ===========================================================================================
 * HONEST SCOPE -- read before quoting this suite as evidence
 * ===========================================================================================
 *
 * Rung 2 only: headless, single process, server-authority path. It proves the RULE. It proves
 * nothing about replication, prediction, or what a client sees (`testing.md` rungs 4a/4b), and
 * nothing about a bot actually reacting to `State.Shields.Broken` (`Breachpoint.Bots.*`).
 *
 * It DOES create a transient `EWorldType::Game` world. There is no way around that and it is
 * stated rather than glossed: a `UAbilitySystemComponent` reads world time to stamp every
 * applied effect, so "no world" and "apply a GameplayEffect" are mutually exclusive. The world
 * here has no map, no rendering, no game mode, no net driver and is never ticked -- it exists so
 * two actors can hold ASCs, and it is destroyed after every single test.
 */

namespace BRShieldSpecInternal
{
	/** Float comparison tolerance. Attribute math here is exact; this guards float printing, not sloppiness. */
	static constexpr float Tolerance = 0.001f;

	/**
	 * R5's pillar number, written as a literal ON PURPOSE. See choice (b) above: every other
	 * number in this file is read from CT_Combat so a balance change does not falsely fail; this
	 * one is pinned so a pillar change CANNOT quietly pass.
	 */
	static constexpr float PillarRechargeDelaySeconds = 2.5f;

	/** One fighter: an actor, its ASC, and its attribute set. Assembled the way ABRPlayerState assembles it. */
	struct FFighter
	{
		AActor* Actor = nullptr;
		UBRAbilitySystemComponent* ASC = nullptr;
		UBRAttributeSet* Set = nullptr;

		bool IsUsable() const { return Actor != nullptr && ASC != nullptr && Set != nullptr; }
	};
}

BEGIN_DEFINE_SPEC(FBRShieldSpec, "Breachpoint.Sim.Shields",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

	// --- fixture state (rebuilt and torn down around EVERY It) ---------------------------------
	UWorld* World = nullptr;
	UCurveTable* CombatTable = nullptr;
	BRShieldSpecInternal::FFighter Target;
	BRShieldSpecInternal::FFighter Attacker;

	/** Read from the shipped CSV in BeforeEach. Asserted relationally -- these are tunables, not pillars. */
	float TableMaxHealth = 0.f;
	float TableMaxShields = 0.f;
	float TableRegenRatePerSecond = 0.f;
	float TableRegenPeriodSeconds = 0.f;

	/** Bumped by the OnDeath delegate. "Exactly once" is the whole point of the death tests. */
	int32 DeathBroadcastCount = 0;

	// --- fixture construction ------------------------------------------------------------------

	/**
	 * Build a CurveTable in memory from Content/Data/CT_Combat.csv and install it as the sim's
	 * table. Reading the CSV rather than the imported .uasset is deliberate twice over: the CSV is
	 * the source of truth designers edit (data-and-assets.md), and it removes any dependency on an
	 * asset having been imported on this machine.
	 */
	bool InstallShippedCombatTable()
	{
		const FString CsvPath = FPaths::ProjectContentDir() / TEXT("Data/CT_Combat.csv");

		FString CsvText;
		if (!FFileHelper::LoadFileToString(CsvText, *CsvPath))
		{
			AddError(FString::Printf(TEXT("Could not read the shipped combat table at '%s'. Every number this suite pins lives in that file."), *CsvPath));
			return false;
		}

		CombatTable = NewObject<UCurveTable>(GetTransientPackage(), NAME_None, RF_Transient);
		// The override is held as a weak pointer, so an unrooted table would be collected out from
		// under the sim mid-test and every curve lookup would start failing for no visible reason.
		CombatTable->AddToRoot();

		const TArray<FString> Problems = CombatTable->CreateTableFromCSVString(CsvText);
		for (const FString& Problem : Problems)
		{
			AddError(FString::Printf(TEXT("CT_Combat.csv failed to parse: %s"), *Problem));
		}

		BRCombatCurves::SetTableOverrideForTests(CombatTable);
		return Problems.Num() == 0;
	}

	/** Read the tunables this suite reasons about. Missing curve = error, never a substituted default. */
	bool ReadTunables()
	{
		struct FRead { FName Curve; float* Out; };
		const FRead Reads[] = {
			{ BRCombatCurves::Names::FighterMaxHealth,           &TableMaxHealth },
			{ BRCombatCurves::Names::FighterMaxShields,          &TableMaxShields },
			{ BRCombatCurves::Names::ShieldsRegenRatePerSecond,  &TableRegenRatePerSecond },
			{ BRCombatCurves::Names::ShieldsRegenPeriodSeconds,  &TableRegenPeriodSeconds },
		};

		bool bAllPresent = true;
		for (const FRead& Read : Reads)
		{
			if (!BRCombatCurves::Evaluate(Read.Curve, *Read.Out))
			{
				AddError(FString::Printf(TEXT("CT_Combat has no '%s' curve. The shield model cannot be evaluated without it."), *Read.Curve.ToString()));
				bAllPresent = false;
			}
		}
		return bAllPresent;
	}

	/** A bare Game world: no map, no game mode, no net driver, never ticked. See the honest-scope note. */
	bool BuildWorld()
	{
		World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false);
		if (!World)
		{
			AddError(TEXT("UWorld::CreateWorld returned null; no ASC can be built without a world."));
			return false;
		}

		FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
		Context.SetCurrentWorld(World);
		World->InitializeActorsForPlay(FURL());
		return true;
	}

	/** An ASC + UBRAttributeSet on a plain actor -- the shape ABRPlayerState carries at runtime. */
	BRShieldSpecInternal::FFighter SpawnFighter()
	{
		BRShieldSpecInternal::FFighter Fighter;
		if (!World)
		{
			return Fighter;
		}

		Fighter.Actor = World->SpawnActor<AActor>();
		if (!Fighter.Actor)
		{
			AddError(TEXT("SpawnActor failed for a fighter."));
			return Fighter;
		}

		Fighter.ASC = NewObject<UBRAbilitySystemComponent>(Fighter.Actor);
		Fighter.ASC->RegisterComponent();

		// Outered to the ACTOR, not the component: UAttributeSet::GetOwningActor is GetTypedOuter,
		// and the whole shields/death reaction path runs through GetOwningAbilitySystemComponent(),
		// which resolves from that actor.
		Fighter.Set = NewObject<UBRAttributeSet>(Fighter.Actor);
		Fighter.ASC->AddAttributeSetSubobject(Fighter.Set);
		Fighter.ASC->InitAbilityActorInfo(Fighter.Actor, Fighter.Actor);

		return Fighter;
	}

	void DestroyWorld()
	{
		BRCombatCurves::SetTableOverrideForTests(nullptr);

		if (CombatTable)
		{
			CombatTable->RemoveFromRoot();
			CombatTable = nullptr;
		}

		Target = BRShieldSpecInternal::FFighter();
		Attacker = BRShieldSpecInternal::FFighter();

		if (World)
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(/*bInformEngineOfWorld=*/false);
			World = nullptr;
		}
	}

	// --- acting on the fixture -----------------------------------------------------------------

	/**
	 * One hit through THE damage pipeline: GE_Damage spec on the attacker -> BRDamageExecCalc ->
	 * the IncomingDamage meta attribute -> UBRAttributeSet's one reaction point.
	 *
	 * Untagged by default. See choice (c): with no `Damage.*` tag the exec calc's multiplier is the
	 * empty product (1.0) by STRUCTURE, so FinalDamage == BaseDamage exactly and these assertions
	 * cannot be broken by a tuning change to a multiplier this suite does not own.
	 */
	bool LandHit(float BaseDamage, const FGameplayTagContainer& DamageTags = FGameplayTagContainer())
	{
		if (!Attacker.IsUsable() || !Target.IsUsable())
		{
			AddError(TEXT("LandHit called with an unusable fixture."));
			return false;
		}

		const FGameplayEffectContextHandle Context = Attacker.ASC->MakeEffectContext();
		const FGameplayEffectSpecHandle SpecHandle = UBRGE_Damage::MakeSpec(Attacker.ASC, BaseDamage, DamageTags, Context);
		return UBRGE_Damage::ApplyToTarget(SpecHandle, Attacker.ASC, Target.ASC);
	}

	/**
	 * Add shields through an instant GameplayEffect built at runtime.
	 *
	 * WHY A RUNTIME EFFECT AND NOT `UBRGE_Regen`: regen is INFINITE + PERIODIC and deliberately
	 * does not execute on application, so making it pay out requires advancing a clock -- which
	 * choice (a) forbids. This effect is a stand-in for exactly one regen tick, and it is still a
	 * GameplayEffect, so gas-purity.md law 1 holds: no attribute is written by hand anywhere here.
	 * What it tests is the CLAMP and the broken-state clear, both of which live in the attribute
	 * set and are indifferent to which effect delivered the points.
	 */
	void GrantShields(float Amount)
	{
		if (!Target.IsUsable())
		{
			AddError(TEXT("GrantShields called with an unusable fixture."));
			return;
		}

		UGameplayEffect* Grant = NewObject<UGameplayEffect>(GetTransientPackage(), NAME_None, RF_Transient);
		Grant->DurationPolicy = EGameplayEffectDurationType::Instant;

		FGameplayModifierInfo Mod;
		Mod.Attribute = UBRAttributeSet::GetShieldsAttribute();
		Mod.ModifierOp = EGameplayModOp::AddBase;
		Mod.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Amount));
		Grant->Modifiers.Add(Mod);

		// Instant effects return an invalid handle by design (nothing is left active to hold), so
		// there is no success value to check here and none is invented -- the assertion that the
		// points arrived is made by the caller, on the attribute.
		Target.ASC->ApplyGameplayEffectToSelf(Grant, /*Level=*/1.f, Target.ASC->MakeEffectContext());
	}

	// --- reading the fixture -------------------------------------------------------------------

	/**
	 * Attribute reads, routed through one place so a failed BeforeEach reports FAILURES instead of
	 * crashing the automation process on a null fixture. Read-only: gas-purity.md law 1 means this
	 * suite never calls the setters, which exist and are public two lines from these getters.
	 */
	float Shields() const { return Target.Set ? Target.Set->GetShields() : TNumericLimits<float>::Lowest(); }
	float Health() const { return Target.Set ? Target.Set->GetHealth() : TNumericLimits<float>::Lowest(); }
	float MaxShieldsAttr() const { return Target.Set ? Target.Set->GetMaxShields() : TNumericLimits<float>::Lowest(); }
	bool DeathLatched() const { return Target.Set != nullptr && Target.Set->HasReportedDeath(); }

	/** Every active effect of one class on the target, inhibited or not. */
	TArray<FActiveGameplayEffectHandle> ActiveEffectsOfClass(TSubclassOf<UGameplayEffect> EffectClass) const
	{
		if (!Target.ASC)
		{
			return TArray<FActiveGameplayEffectHandle>();
		}

		FGameplayEffectQuery Query;
		Query.EffectDefinition = EffectClass;
		return Target.ASC->GetActiveEffects(Query);
	}

	int32 CountActiveEffects(TSubclassOf<UGameplayEffect> EffectClass) const
	{
		return ActiveEffectsOfClass(EffectClass).Num();
	}

	/**
	 * Is the one active GE_Regen currently inhibited?
	 *
	 * `bIsInhibited` is the engine's own word for "applied but switched off by an ongoing tag
	 * requirement" -- which is exactly what the recharge gate is. Returns true when regen is absent
	 * too: an absent regen is not recharging either, and a test that mistook "missing" for
	 * "running" would pass while shields never came back.
	 */
	bool IsRechargeInhibited() const
	{
		const TArray<FActiveGameplayEffectHandle> Handles = ActiveEffectsOfClass(UBRGE_Regen::StaticClass());
		if (Handles.Num() != 1)
		{
			return true;
		}

		const FActiveGameplayEffect* Active = Target.ASC->GetActiveGameplayEffect(Handles[0]);
		return Active == nullptr || Active->bIsInhibited;
	}

	int32 TagCount(const FGameplayTag& Tag) const
	{
		return Target.ASC ? Target.ASC->GetTagCount(Tag) : -1;
	}

	/** The shipped respawn write: GE_InitStats (+ the regen it ensures). Never a hand-written attribute. */
	bool Respawn()
	{
		if (!Target.IsUsable())
		{
			AddError(TEXT("Respawn called with an unusable fixture."));
			return false;
		}
		return Target.ASC->ApplyInitStats();
	}

	/** GE_Death, applied by the authority -- never by the attribute set. See the death-boundary tests. */
	bool ApplyDeath()
	{
		if (!Target.IsUsable())
		{
			AddError(TEXT("ApplyDeath called with an unusable fixture."));
			return false;
		}
		return Target.ASC->ApplyDeathEffect();
	}

END_DEFINE_SPEC(FBRShieldSpec)

void FBRShieldSpec::Define()
{
	using namespace BRShieldSpecInternal;

	BeforeEach([this]()
	{
		DeathBroadcastCount = 0;

		if (!InstallShippedCombatTable() || !ReadTunables() || !BuildWorld())
		{
			return;
		}

		Attacker = SpawnFighter();
		Target = SpawnFighter();
		if (!Attacker.IsUsable() || !Target.IsUsable())
		{
			return;
		}

		Target.Set->OnDeath.AddLambda([this](AActor* Victim, AActor* /*Instigator*/, AActor* /*Causer*/, const FGameplayEffectSpec& /*KillingSpec*/)
		{
			++DeathBroadcastCount;
			TestTrue(TEXT("OnDeath names the fighter that died as the victim"), Victim == Target.Actor);
		});

		// The shipped spawn path: GE_InitStats (capacities first, then current values) plus the
		// GE_Regen that runs for the rest of the life. Nothing here writes an attribute by hand.
		if (!Attacker.ASC->ApplyInitStats() || !Respawn())
		{
			AddError(TEXT("ApplyInitStats refused; the fixture is uninitialised and no shield rule can be evaluated."));
		}
	});

	AfterEach([this]()
	{
		DestroyWorld();
	});

	// =======================================================================================
	// 1. THE RECHARGE GATE -- R5's 2.5 s, and the mechanism that enforces it
	// =======================================================================================
	Describe(TEXT("the recharge gate"), [this]()
	{
		It(TEXT("pins R5's 2.5 s recharge delay in CT_Combat"), [this]()
		{
			// THE PILLAR. If this line fails, someone changed a number that R5 says only the founder
			// may change. The correct response is not to update this test -- it is to take the change
			// back to the founder, or land it with the ruling amended in the same packet.
			float DelaySeconds = -1.f;
			const bool bFound = BRCombatCurves::Evaluate(BRCombatCurves::Names::ShieldsRegenDelaySeconds, DelaySeconds);

			TestTrue(TEXT("CT_Combat declares Shields.Regen.DelaySeconds"), bFound);
			TestEqual(TEXT("R5 PILLAR: the shield recharge delay is 2.5 s"), DelaySeconds, PillarRechargeDelaySeconds, Tolerance);
		});

		It(TEXT("gates recharge on a tag, not on a timer, and blocks a corpse too"), [this]()
		{
			// The structural half of the pillar. "Regen starts 2.5 s after the last hit" is TRUE only
			// because GE_Regen refuses to run while State.Combat.RecentDamage is present; delete this
			// requirement and the CSV number becomes decoration.
			const UBRGE_Regen* RegenCDO = GetDefault<UBRGE_Regen>();
			if (!TestNotNull(TEXT("GE_Regen exists"), RegenCDO))
			{
				return;
			}

			TestEqual(TEXT("GE_Regen is infinite -- applied once per life, inhibited rather than re-applied"),
				static_cast<int32>(RegenCDO->DurationPolicy), static_cast<int32>(EGameplayEffectDurationType::Infinite));

			const UTargetTagRequirementsGameplayEffectComponent* Requirements =
				RegenCDO->FindComponent<UTargetTagRequirementsGameplayEffectComponent>();
			if (!TestNotNull(TEXT("GE_Regen carries ongoing tag requirements"), Requirements))
			{
				return;
			}

			TestTrue(TEXT("R5: recharge is inhibited by State.Combat.RecentDamage"),
				Requirements->OngoingTagRequirements.IgnoreTags.HasTagExact(BRGameplayTags::State_Combat_RecentDamage.GetTag()));

			// Not decoration: without it a body waiting out a respawn timer comes back with shields it
			// never earned, and nothing anywhere would look wrong.
			TestTrue(TEXT("a corpse does not recharge: State.Dead also inhibits regen"),
				Requirements->OngoingTagRequirements.IgnoreTags.HasTagExact(BRGameplayTags::State_Dead.GetTag()));

			// ResetPeriod is what makes the gate lifting cost a FULL period before the next tick.
			// NeverReset would let a fighter bank ticks while being shot; ExecuteAndResetPeriod would
			// pay one out instantly at the 2.5 s boundary. Both are silent TTK changes.
			TestEqual(TEXT("when the gate lifts, the next tick is a full period away"),
				static_cast<int32>(RegenCDO->PeriodicInhibitionPolicy),
				static_cast<int32>(EGameplayEffectPeriodInhibitionRemovedPolicy::ResetPeriod));
		});

		It(TEXT("applies the gate for exactly the pillar duration when damage lands"), [this]()
		{
			TestEqual(TEXT("no gate before the first hit"), TagCount(BRGameplayTags::State_Combat_RecentDamage.GetTag()), 0);

			LandHit(TableMaxShields * 0.25f);

			TestEqual(TEXT("the hit applied State.Combat.RecentDamage"), TagCount(BRGameplayTags::State_Combat_RecentDamage.GetTag()), 1);

			const TArray<FActiveGameplayEffectHandle> Gates = ActiveEffectsOfClass(UBRGE_RecentDamage::StaticClass());
			if (!TestEqual(TEXT("exactly one gate effect is live"), Gates.Num(), 1))
			{
				return;
			}

			const FActiveGameplayEffect* Gate = Target.ASC->GetActiveGameplayEffect(Gates[0]);
			if (!TestNotNull(TEXT("the gate effect is readable"), Gate))
			{
				return;
			}

			// End to end: CSV -> BRCombatCurves -> ApplyRecentDamageGate -> the live spec's duration.
			// This is the pillar asserted as BEHAVIOUR, not just as a cell in a file.
			TestEqual(TEXT("R5 PILLAR: the applied gate lasts 2.5 s"), Gate->Spec.GetDuration(), PillarRechargeDelaySeconds, Tolerance);
		});

		It(TEXT("inhibits the running recharge for as long as the gate is up"), [this]()
		{
			TestFalse(TEXT("recharge runs on an undamaged fighter"), IsRechargeInhibited());

			LandHit(TableMaxShields * 0.25f);

			TestTrue(TEXT("R5: recharge must NOT start while RecentDamage is active"), IsRechargeInhibited());

			// Inhibited, not removed. If regen were removed instead, respawn's idempotency check in
			// ApplyInitStats would silently apply a SECOND infinite regen later in the match.
			TestEqual(TEXT("the recharge effect is still applied, merely switched off"),
				CountActiveEffects(UBRGE_Regen::StaticClass()), 1);
		});

		It(TEXT("refreshes one gate rather than stacking a second window"), [this]()
		{
			LandHit(TableMaxShields * 0.1f);
			LandHit(TableMaxShields * 0.1f);
			LandHit(TableMaxShields * 0.1f);

			// Three hits, one window. Without AggregateByTarget + StackLimitCount 1, an AR at 600 RPM
			// leaves 25 live gate effects and the moment recharge resumes becomes an emergent property
			// of a list rather than 2.5 s after the last hit.
			TestEqual(TEXT("three hits leave exactly one gate effect"), CountActiveEffects(UBRGE_RecentDamage::StaticClass()), 1);
			TestEqual(TEXT("three hits leave exactly one State.Combat.RecentDamage"), TagCount(BRGameplayTags::State_Combat_RecentDamage.GetTag()), 1);

			const UBRGE_RecentDamage* GateCDO = GetDefault<UBRGE_RecentDamage>();
			if (!TestNotNull(TEXT("GE_RecentDamage exists"), GateCDO))
			{
				return;
			}

			TestEqual(TEXT("the gate is capped at one stack"), GateCDO->StackLimitCount, 1);
			TestEqual(TEXT("a later hit REFRESHES the window instead of extending it"),
				static_cast<int32>(GateCDO->StackDurationRefreshPolicy),
				static_cast<int32>(EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication));
		});

		It(TEXT("resumes recharge once -- and only once -- the gate is gone"), [this]()
		{
			LandHit(TableMaxShields * 0.25f);
			TestTrue(TEXT("gated immediately after the hit"), IsRechargeInhibited());

			// The deterministic stand-in for the 2.5 s expiring: remove the gate effect. That is the
			// same state transition the duration produces, minus the clock -- see choice (a).
			const TArray<FActiveGameplayEffectHandle> Gates = ActiveEffectsOfClass(UBRGE_RecentDamage::StaticClass());
			if (!TestEqual(TEXT("there is one gate to remove"), Gates.Num(), 1))
			{
				return;
			}
			Target.ASC->RemoveActiveGameplayEffect(Gates[0]);

			TestEqual(TEXT("the gate tag is gone"), TagCount(BRGameplayTags::State_Combat_RecentDamage.GetTag()), 0);
			TestFalse(TEXT("recharge resumes when the gate expires"), IsRechargeInhibited());
		});
	});

	// =======================================================================================
	// 2. RECHARGE RATE AND THE CLAMP -- including "no health regen", R5's second pillar
	// =======================================================================================
	Describe(TEXT("recharge rate and the clamp"), [this]()
	{
		It(TEXT("derives the per-tick magnitude from rate x period, so the period is a fidelity knob"), [this]()
		{
			const TArray<FActiveGameplayEffectHandle> Regens = ActiveEffectsOfClass(UBRGE_Regen::StaticClass());
			if (!TestEqual(TEXT("exactly one recharge effect runs per life"), Regens.Num(), 1))
			{
				return;
			}

			const FActiveGameplayEffect* Regen = Target.ASC->GetActiveGameplayEffect(Regens[0]);
			if (!TestNotNull(TEXT("the recharge effect is readable"), Regen))
			{
				return;
			}

			// THE BUG THIS PINS: bake the per-tick number into the CSV (or forget the multiply) and
			// changing the period silently changes the recharge RATE -- a balance change nobody made,
			// invisible until someone times a recharge. Both operands come from the shipped table, so
			// a legitimate retune of either stays green; only the composition rule is pinned.
			TestEqual(TEXT("the recharge ticks at CT_Combat's period"), Regen->Spec.GetPeriod(), TableRegenPeriodSeconds, Tolerance);
			TestEqual(TEXT("each tick pays rate-per-second x period"),
				Regen->Spec.GetSetByCallerMagnitude(BRGameplayTags::SetByCaller_RegenRate.GetTag(), /*WarnIfNotFound=*/false),
				TableRegenRatePerSecond * TableRegenPeriodSeconds, Tolerance);
		});

		It(TEXT("recharges shields and nothing else"), [this]()
		{
			const UBRGE_Regen* RegenCDO = GetDefault<UBRGE_Regen>();
			if (!TestNotNull(TEXT("GE_Regen exists"), RegenCDO))
			{
				return;
			}

			if (!TestEqual(TEXT("GE_Regen carries exactly one modifier"), RegenCDO->Modifiers.Num(), 1))
			{
				return;
			}

			TestTrue(TEXT("R5: the recharge modifier targets Shields"),
				RegenCDO->Modifiers[0].Attribute == UBRAttributeSet::GetShieldsAttribute());
		});

		It(TEXT("R5: no GameplayEffect in the project regenerates health over time"), [this]()
		{
			// The broad version of "no health regen", and the reason it is written as an enumeration
			// rather than as four assertions about four known classes: a future `UBRGE_HealthRegen`
			// would sail past a hard-coded list. This walks EVERY UGameplayEffect subclass compiled
			// into the Breachpoint module and applies one rule:
			//
			//   a modifier on Health is legal ONLY on an INSTANT effect.
			//
			// Instant covers the writes the design does allow -- GE_InitStats' spawn override, and a
			// health pickup if one is ever designed. HasDuration or Infinite on Health IS regeneration
			// (or its mirror, a damage-over-time bypassing the one damage pipeline), and R5 forbids it.
			const UPackage* BreachpointPackage = UBRGE_Regen::StaticClass()->GetOutermost();

			TArray<UClass*> EffectClasses;
			GetDerivedClasses(UGameplayEffect::StaticClass(), EffectClasses, /*bRecursive=*/true);

			int32 Examined = 0;
			for (const UClass* EffectClass : EffectClasses)
			{
				if (!EffectClass || EffectClass->GetOutermost() != BreachpointPackage)
				{
					continue;
				}
				if (EffectClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
				{
					continue;
				}

				const UGameplayEffect* CDO = EffectClass->GetDefaultObject<UGameplayEffect>();
				if (!CDO)
				{
					continue;
				}

				++Examined;
				for (const FGameplayModifierInfo& Modifier : CDO->Modifiers)
				{
					if (Modifier.Attribute != UBRAttributeSet::GetHealthAttribute())
					{
						continue;
					}

					TestEqual(FString::Printf(TEXT("R5 PILLAR: '%s' modifies Health, so it must be Instant -- there is no health regeneration in Breachpoint"), *EffectClass->GetName()),
						static_cast<int32>(CDO->DurationPolicy), static_cast<int32>(EGameplayEffectDurationType::Instant));
				}
			}

			// Anti-hollow guard. Without it, a reflection change that returned zero classes would make
			// the loop above assert nothing and this test would go green having checked NOTHING --
			// which is precisely the failure R25 names as a finding.
			TestTrue(FString::Printf(TEXT("the effect library was actually enumerated (found %d Breachpoint effects, expected at least the seven in BRGameplayEffects.h)"), Examined),
				Examined >= 7);
		});

		It(TEXT("fills shields to max and never overshoots"), [this]()
		{
			LandHit(TableMaxShields * 0.5f);
			TestEqual(TEXT("the hit landed on shields"), Shields(), TableMaxShields * 0.5f, Tolerance);

			// Ten times more than could possibly be owed. A missing clamp shows up as a fighter walking
			// around with several times their shield capacity, which no HUD would reveal.
			GrantShields(TableMaxShields * 10.f);

			TestEqual(TEXT("shields stop at MaxShields"), Shields(), TableMaxShields, Tolerance);
			TestTrue(TEXT("shields never exceed MaxShields"), Shields() <= MaxShieldsAttr() + Tolerance);
		});

		It(TEXT("R5: recharging shields does not restore one point of health"), [this]()
		{
			// Break the shield and bite into health, then recharge to full and check health did not
			// come with it. A suite that only tested a shields-only hit would pass even if the
			// recharge effect quietly also topped up Health.
			const float Overflow = TableMaxHealth * 0.4f;
			LandHit(TableMaxShields + Overflow);

			const float HealthAfterDamage = Health();
			TestEqual(TEXT("the overflow reached health"), HealthAfterDamage, TableMaxHealth - Overflow, Tolerance);

			GrantShields(TableMaxShields * 10.f);

			TestEqual(TEXT("shields came back to full"), Shields(), TableMaxShields, Tolerance);
			TestEqual(TEXT("R5 PILLAR: health is exactly where the damage left it"), Health(), HealthAfterDamage, Tolerance);
		});
	});

	// =======================================================================================
	// 3. SHIELDS-FIRST ORDERING -- R5's first pillar, and the seam that must not leak
	// =======================================================================================
	Describe(TEXT("shields-first ordering"), [this]()
	{
		It(TEXT("consumes shields before touching health"), [this]()
		{
			const float Damage = TableMaxShields * 0.5f;
			LandHit(Damage);

			TestEqual(TEXT("shields absorbed the whole hit"), Shields(), TableMaxShields - Damage, Tolerance);
			TestEqual(TEXT("R5 PILLAR: health is untouched while shields remain"), Health(), TableMaxHealth, Tolerance);
		});

		It(TEXT("carries the overflow into health on the shot that breaks the shield"), [this]()
		{
			const float Overflow = TableMaxHealth * 0.25f;
			LandHit(TableMaxShields + Overflow);

			TestEqual(TEXT("the shield is emptied, not driven negative"), Shields(), 0.f, Tolerance);
			TestEqual(TEXT("exactly the overflow crossed into health"), Health(), TableMaxHealth - Overflow, Tolerance);
		});

		It(TEXT("treats the exact-capacity hit as the boundary it is"), [this]()
		{
			// The off-by-one that would be invisible in play and decisive in a duel: a hit for exactly
			// MaxShields must break the shield and take ZERO health, not one point.
			LandHit(TableMaxShields);

			TestEqual(TEXT("shields are exactly zero"), Shields(), 0.f, Tolerance);
			TestEqual(TEXT("not one point of health was taken"), Health(), TableMaxHealth, Tolerance);
		});

		It(TEXT("neither creates nor loses damage at the shield/health seam"), [this]()
		{
			// A property, checked over a spread of magnitudes that straddle every branch of the split:
			// well under the shield, exactly the shield, just over it, deep into health, and lethal
			// overkill. For every input, the total taken equals the damage dealt, capped by the total
			// pool the fighter had -- nothing invented, nothing quietly swallowed.
			const float Cases[] = {
				1.f,
				TableMaxShields * 0.5f,
				TableMaxShields,
				TableMaxShields + 1.f,
				TableMaxShields + (TableMaxHealth * 0.5f),
				TableMaxShields + TableMaxHealth,
				(TableMaxShields + TableMaxHealth) * 3.f
			};

			for (const float Damage : Cases)
			{
				// Respawn between cases through the shipped path, so each case starts from a full,
				// re-armed fighter rather than from the previous case's corpse.
				Respawn();

				const float ShieldsBefore = Shields();
				const float HealthBefore = Health();

				LandHit(Damage);

				const float Taken = (ShieldsBefore - Shields()) + (HealthBefore - Health());
				const float Expected = FMath::Min(Damage, ShieldsBefore + HealthBefore);

				TestEqual(FString::Printf(TEXT("%.1f damage takes exactly %.1f across the seam"), Damage, Expected), Taken, Expected, Tolerance);
				TestTrue(FString::Printf(TEXT("%.1f damage leaves shields non-negative"), Damage), Shields() >= 0.f);
				TestTrue(FString::Printf(TEXT("%.1f damage leaves health non-negative"), Damage), Health() >= 0.f);
				TestTrue(FString::Printf(TEXT("%.1f damage never touches health while shields remain"), Damage),
					Shields() <= Tolerance || FMath::IsNearlyEqual(Health(), HealthBefore, Tolerance));
			}
		});

		It(TEXT("reaches the same rule through a realistically tagged hit"), [this]()
		{
			// The one test that uses a real `Damage.*` type tag. It asserts SHAPE only -- never a
			// magnitude -- because the multiplier chain is the damage rule's business and lives in
			// Breachpoint.Sim.Combat. R22: the family is flat, so one type tag is a complete hit.
			FGameplayTagContainer Tags;
			Tags.AddTag(BRGameplayTags::Damage_Kinetic.GetTag());

			const bool bApplied = LandHit(TableMaxShields * 0.25f, Tags);

			TestTrue(TEXT("a tagged hit applies through the one damage pipeline"), bApplied);
			TestTrue(TEXT("it came out of shields"), Shields() < TableMaxShields - Tolerance);
			TestEqual(TEXT("health is untouched while shields remain"), Health(), TableMaxHealth, Tolerance);
			TestEqual(TEXT("and it gated the recharge"), TagCount(BRGameplayTags::State_Combat_RecentDamage.GetTag()), 1);
		});
	});

	// =======================================================================================
	// 4. THE ShieldsBroken TRANSITION -- R12: bots read this, so a double-fire is a defect
	// =======================================================================================
	Describe(TEXT("the ShieldsBroken transition"), [this]()
	{
		It(TEXT("is silent while the shield holds"), [this]()
		{
			LandHit(TableMaxShields * 0.9f);

			TestEqual(TEXT("a shield at one tenth is not a broken shield"), TagCount(BRGameplayTags::State_Shields_Broken.GetTag()), 0);
			TestEqual(TEXT("no GE_ShieldsBroken is applied"), CountActiveEffects(UBRGE_ShieldsBroken::StaticClass()), 0);
		});

		It(TEXT("grants State.Shields.Broken exactly once on the break"), [this]()
		{
			LandHit(TableMaxShields);

			// EXACTLY one. A count of 2 is the double-fire this test exists to catch: bots break off on
			// the shield-crack (R12), and an event that fires twice reads to a player as a bot that
			// flinched at nothing.
			TestEqual(TEXT("R12: the crack announces itself exactly once"), TagCount(BRGameplayTags::State_Shields_Broken.GetTag()), 1);
			TestEqual(TEXT("exactly one GE_ShieldsBroken is applied"), CountActiveEffects(UBRGE_ShieldsBroken::StaticClass()), 1);
		});

		It(TEXT("does not re-fire on every subsequent hit to a broken shield"), [this]()
		{
			LandHit(TableMaxShields);
			TestEqual(TEXT("broken by the first hit"), TagCount(BRGameplayTags::State_Shields_Broken.GetTag()), 1);

			LandHit(TableMaxHealth * 0.1f);
			LandHit(TableMaxHealth * 0.1f);

			TestEqual(TEXT("R12: two more hits do not re-announce the crack"), TagCount(BRGameplayTags::State_Shields_Broken.GetTag()), 1);
			TestEqual(TEXT("and do not stack a second GE_ShieldsBroken"), CountActiveEffects(UBRGE_ShieldsBroken::StaticClass()), 1);
		});

		It(TEXT("clears when the shield comes back"), [this]()
		{
			LandHit(TableMaxShields);
			TestEqual(TEXT("broken"), TagCount(BRGameplayTags::State_Shields_Broken.GetTag()), 1);

			GrantShields(TableMaxShields);

			TestEqual(TEXT("a restored shield clears State.Shields.Broken"), TagCount(BRGameplayTags::State_Shields_Broken.GetTag()), 0);
			TestEqual(TEXT("and removes the effect rather than leaving a dormant one"), CountActiveEffects(UBRGE_ShieldsBroken::StaticClass()), 0);
		});

		It(TEXT("is cleared by respawn, so nobody spawns marked"), [this]()
		{
			LandHit(TableMaxShields);
			TestEqual(TEXT("broken"), TagCount(BRGameplayTags::State_Shields_Broken.GetTag()), 1);

			Respawn();

			TestEqual(TEXT("a respawned fighter is not marked shields-broken"), TagCount(BRGameplayTags::State_Shields_Broken.GetTag()), 0);
			TestEqual(TEXT("with full shields"), Shields(), TableMaxShields, Tolerance);
		});

		It(TEXT("refuses to mark an uninitialised fighter as broken"), [this]()
		{
			// Zero shields on a fighter GE_InitStats never reached means "uninitialised", not "broken".
			// Marking it would fire a cue and swing bot aggression for the instant between spawning and
			// initialising -- an explicit refusal in UpdateShieldsBrokenState, pinned here so it stays
			// a decision rather than becoming an accident.
			const FFighter Fresh = SpawnFighter();
			if (!TestTrue(TEXT("a fresh fighter was built"), Fresh.IsUsable()))
			{
				return;
			}

			TestEqual(TEXT("an uninitialised fighter has no shield capacity"), Fresh.Set->GetMaxShields(), 0.f, Tolerance);
			TestEqual(TEXT("and is NOT marked shields-broken"), Fresh.ASC->GetTagCount(BRGameplayTags::State_Shields_Broken.GetTag()), 0);
		});
	});

	// =======================================================================================
	// 5. THE DEATH BOUNDARY
	// =======================================================================================
	Describe(TEXT("the death boundary"), [this]()
	{
		It(TEXT("reports death exactly once when health reaches zero"), [this]()
		{
			LandHit(TableMaxShields + TableMaxHealth);

			TestEqual(TEXT("health is exactly zero"), Health(), 0.f, Tolerance);
			TestTrue(TEXT("the death latch is set"), DeathLatched());
			TestEqual(TEXT("Event.Death was reported exactly once"), DeathBroadcastCount, 1);
		});

		It(TEXT("does not fire a second death for a corpse"), [this]()
		{
			LandHit(TableMaxShields + TableMaxHealth);
			TestEqual(TEXT("dead once"), DeathBroadcastCount, 1);

			// Two effects landing in the same frame, or a body being shot again before the respawn
			// timer. One life, one death -- a second Event.Death would double-count a kill in scoring.
			LandHit(TableMaxHealth);
			LandHit(TableMaxHealth);

			TestEqual(TEXT("still exactly one death"), DeathBroadcastCount, 1);
			TestEqual(TEXT("health is still zero, never negative"), Health(), 0.f, Tolerance);
			TestTrue(TEXT("health never goes below zero"), Health() >= 0.f);
		});

		It(TEXT("discards overkill rather than banking it"), [this]()
		{
			LandHit((TableMaxShields + TableMaxHealth) * 100.f);

			// Floored, not stored. If overkill were retained as negative health, a respawn's additive
			// heal would come back short and the bug would look like a spawn bug.
			TestEqual(TEXT("a hundredfold overkill still floors health at zero"), Health(), 0.f, Tolerance);
			TestEqual(TEXT("shields floor at zero too"), Shields(), 0.f, Tolerance);
			TestEqual(TEXT("and it is still one death"), DeathBroadcastCount, 1);
		});

		It(TEXT("re-arms for the next life when GE_InitStats restores health"), [this]()
		{
			LandHit(TableMaxShields + TableMaxHealth);
			TestEqual(TEXT("first death"), DeathBroadcastCount, 1);

			Respawn();

			TestFalse(TEXT("the latch re-arms when health rises above zero"), DeathLatched());
			TestEqual(TEXT("respawn restores full health"), Health(), TableMaxHealth, Tolerance);
			TestEqual(TEXT("respawn restores full shields"), Shields(), TableMaxShields, Tolerance);

			LandHit(TableMaxShields + TableMaxHealth);

			// The property that makes respawn work without anyone remembering to reset anything.
			TestEqual(TEXT("the second life can die too"), DeathBroadcastCount, 2);
		});

		It(TEXT("does not resurrect: a corpse cannot be healed by a shield grant"), [this]()
		{
			LandHit(TableMaxShields + TableMaxHealth);
			TestEqual(TEXT("dead"), DeathBroadcastCount, 1);

			GrantShields(TableMaxShields);

			TestEqual(TEXT("shields can be granted to a corpse"), Shields(), TableMaxShields, Tolerance);
			TestEqual(TEXT("but health stays at zero -- nothing here resurrects"), Health(), 0.f, Tolerance);
			TestTrue(TEXT("and the fighter is still recorded as dead"), DeathLatched());
		});

		It(TEXT("refuses to kill a fighter GE_InitStats never reached"), [this]()
		{
			// MaxHealth == 0 means uninitialised, not dead. Killing here would fire Event.Death during
			// spawn -- a bug that presents as random spawn deaths and gets blamed on everything except
			// the attribute set. The refusal is explicit in CheckForDeath; this pins it.
			const FFighter Fresh = SpawnFighter();
			if (!TestTrue(TEXT("a fresh fighter was built"), Fresh.IsUsable()))
			{
				return;
			}

			int32 FreshDeaths = 0;
			Fresh.Set->OnDeath.AddLambda([&FreshDeaths](AActor*, AActor*, AActor*, const FGameplayEffectSpec&)
			{
				++FreshDeaths;
			});

			const FGameplayEffectContextHandle Context = Attacker.ASC->MakeEffectContext();
			const FGameplayEffectSpecHandle SpecHandle = UBRGE_Damage::MakeSpec(Attacker.ASC, 1.f, FGameplayTagContainer(), Context);
			UBRGE_Damage::ApplyToTarget(SpecHandle, Attacker.ASC, Fresh.ASC);

			TestEqual(TEXT("an uninitialised fighter at zero health is NOT dead"), FreshDeaths, 0);
			TestFalse(TEXT("and no death was latched"), Fresh.Set->HasReportedDeath());
		});

		It(TEXT("carries State.Dead through GE_Death, and the corpse stops recharging"), [this]()
		{
			LandHit(TableMaxShields + TableMaxHealth);

			// gas-purity.md law 8 and the attribute set's own boundary, asserted together: the set
			// REPORTS death, and the authority ADMINISTERS it by applying GE_Death. If the set ever
			// starts applying GE_Death itself, State.Dead would be present before this call and the
			// first assertion below goes red -- which is the alarm this test exists to raise.
			TestEqual(TEXT("the attribute set reports death; it does not apply State.Dead"),
				TagCount(BRGameplayTags::State_Dead.GetTag()), 0);

			TestTrue(TEXT("the authority applies GE_Death"), ApplyDeath());
			TestEqual(TEXT("State.Dead is granted exactly once"), TagCount(BRGameplayTags::State_Dead.GetTag()), 1);

			// Idempotent: a second call must not stack a second infinite effect, or respawn's single
			// removal would leave the fighter permanently dead.
			TestFalse(TEXT("applying death twice is refused"), ApplyDeath());
			TestEqual(TEXT("still exactly one State.Dead"), TagCount(BRGameplayTags::State_Dead.GetTag()), 1);

			TestTrue(TEXT("R5: a corpse does not recharge its shields"), IsRechargeInhibited());
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
