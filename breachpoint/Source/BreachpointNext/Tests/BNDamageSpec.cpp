#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/BNAttributeSet.h"
#include "AbilitySystem/BNAbilitySystemComponent.h"
#include "AbilitySystem/Effects/BNDamage.h"
#include "AbilitySystem/Effects/BNGameplayEffects.h"
#include "Engine/Engine.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameplayEffect.h"

/**
 * THE ONE DOOR, tested through the door. Every assertion goes in the way the game goes in —
 * BNDamage::ApplyDamage — never by writing an attribute, because a spec that pokes attributes
 * proves the attribute set works and nothing about whether the game can reach it.
 *
 * EVERY ENGINE CALL BELOW IS TRANSCRIBED, NOT REMEMBERED, and this file has been rewritten once
 * to make that true. The first draft used two APIs this project has never compiled —
 * `UAbilitySystemComponent::CanApplyAttributeModifiers` and the four-argument `FHitResult`
 * constructor — both written from memory. A spec is the worst place in the codebase to guess at
 * an API: it is compiled into the editor target, so a spec that does not build takes the game
 * down with it, and it does so while claiming to be the thing that protects the game.
 *
 * The proven set is: what the old module's `BRShieldSpec` used (it compiled and ran against this
 * engine) plus what BreachpointNext itself used in the founder's last successful build. Anything
 * outside that set is either not here or is named below as an uncovered gap.
 *
 * SHIELDS ARE OFF by founder ruling (the init GE ships MaxShield 0), so every number here is a
 * health number. When shields come back, the shield-first assertions belong in this file.
 */
namespace BNDamageSpecInternal
{
	/** OUTSIDE the spec class, as BRShieldSpec declares its own — the shape that compiled. */
	struct FFighter
	{
		AActor* Actor = nullptr;
		UBNAbilitySystemComponent* ASC = nullptr;
		UBNAttributeSet* Set = nullptr;

		bool IsUsable() const { return Actor != nullptr && ASC != nullptr && Set != nullptr; }
	};
}

BEGIN_DEFINE_SPEC(FBNDamageSpec, "BreachpointNext.Sim.Damage",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

	UWorld* World = nullptr;
	BNDamageSpecInternal::FFighter Target;
	BNDamageSpecInternal::FFighter Attacker;

	bool BuildWorld()
	{
		World = UWorld::CreateWorld(EWorldType::Game, false);
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

	BNDamageSpecInternal::FFighter SpawnFighter()
	{
		BNDamageSpecInternal::FFighter Fighter;
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

		Fighter.ASC = NewObject<UBNAbilitySystemComponent>(Fighter.Actor);
		Fighter.ASC->RegisterComponent();

		Fighter.Set = NewObject<UBNAttributeSet>(Fighter.Actor);
		Fighter.ASC->AddAttributeSetSubobject(Fighter.Set);
		Fighter.ASC->InitAbilityActorInfo(Fighter.Actor, Fighter.Actor);

		return Fighter;
	}

	/** The ONE way attributes reach their starting numbers in the game, used the same way here. */
	void ApplyEffect(const BNDamageSpecInternal::FFighter& Fighter, TSubclassOf<UGameplayEffect> EffectClass)
	{
		if (!Fighter.IsUsable())
		{
			return;
		}
		const FGameplayEffectSpecHandle Spec = Fighter.ASC->MakeOutgoingSpec(
			EffectClass, 1.f, Fighter.ASC->MakeEffectContext());
		if (Spec.IsValid())
		{
			Fighter.ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}
	}

	float Attr(const BNDamageSpecInternal::FFighter& Fighter, const FGameplayAttribute& Attribute) const
	{
		return Fighter.IsUsable() ? Fighter.ASC->GetNumericAttribute(Attribute) : -1.f;
	}

	void DestroyWorld()
	{
		Target = BNDamageSpecInternal::FFighter();
		Attacker = BNDamageSpecInternal::FFighter();

		if (World)
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
			World = nullptr;
		}
	}

END_DEFINE_SPEC(FBNDamageSpec)

void FBNDamageSpec::Define()
{
	BeforeEach([this]()
	{
		if (!BuildWorld())
		{
			return;
		}
		Target = SpawnFighter();
		Attacker = SpawnFighter();
		ApplyEffect(Target, UBNGE_InitAttributes::StaticClass());
		ApplyEffect(Attacker, UBNGE_InitAttributes::StaticClass());
	});

	AfterEach([this]()
	{
		DestroyWorld();
	});

	It("starts a fighter on the init GE's numbers, maxes included", [this]()
	{
		TestEqual(TEXT("MaxHealth"), Attr(Target, UBNAttributeSet::GetMaxHealthAttribute()), 100.f);
		TestEqual(TEXT("Health"), Attr(Target, UBNAttributeSet::GetHealthAttribute()), 100.f);
		// R7.4's pouch, from the same GE — which is also what refills it on every respawn.
		TestEqual(TEXT("MaxGrenades"), Attr(Target, UBNAttributeSet::GetMaxGrenadesAttribute()), 2.f);
		TestEqual(TEXT("Grenades"), Attr(Target, UBNAttributeSet::GetGrenadesAttribute()), 2.f);
	});

	It("drains health through the door", [this]()
	{
		// SHIELDS FIRST, and that is why this spec had to change. It read
		// "30 damage -> Health 70" and went red the day BN22 turned shields back ON
		// (7afc0aff): the shield swallowed all 30 and health never moved. The CODE was
		// right and the assertion was stale — the honest fix is to drop the shield and
		// then test the thing this spec is named for, not to weaken the number.
		Target.ASC->SetNumericAttributeBase(UBNAttributeSet::GetShieldAttribute(), 0.f);

		BNDamage::ApplyDamage(Attacker.Actor, Target.Actor, 30.f, FHitResult());
		TestEqual(TEXT("Health"), Attr(Target, UBNAttributeSet::GetHealthAttribute()), 70.f);
	});

	It("spends the shield before the health, and spills the remainder into it", [this]()
	{
		// THE SUMMING PATH, which nothing covered. AIB4 closed with the double-fire check
		// passing VACUOUSLY: every damage event in that match read `shield 0 -> 0` because
		// shields were disabled project-wide, so a hit that crosses BOTH pools could not
		// occur and the claim went unproven. BN22 re-enabled shields and made it reachable,
		// so it is proven here rather than left as a note in a closed ticket.
		Target.ASC->SetNumericAttributeBase(UBNAttributeSet::GetShieldAttribute(), 20.f);

		BNDamage::ApplyDamage(Attacker.Actor, Target.Actor, 50.f, FHitResult());

		// 50 against a 20 shield: the shield empties and the remaining 30 reaches health.
		// Both pools move on ONE hit — the shape the ledger sums rather than double-counts.
		TestEqual(TEXT("Shield emptied"), Attr(Target, UBNAttributeSet::GetShieldAttribute()), 0.f);
		TestEqual(TEXT("Health took the overflow"), Attr(Target, UBNAttributeSet::GetHealthAttribute()), 70.f);
	});

	It("leaves health untouched while the shield can absorb the whole hit", [this]()
	{
		// The other half of the same contract, and the one the stale assertion was
		// accidentally testing: a hit smaller than the shield must not reach health at all.
		Target.ASC->SetNumericAttributeBase(UBNAttributeSet::GetShieldAttribute(), 100.f);

		BNDamage::ApplyDamage(Attacker.Actor, Target.Actor, 30.f, FHitResult());

		TestEqual(TEXT("Shield absorbed it"), Attr(Target, UBNAttributeSet::GetShieldAttribute()), 70.f);
		TestEqual(TEXT("Health untouched"), Attr(Target, UBNAttributeSet::GetHealthAttribute()), 100.f);
	});

	It("never drives health below zero", [this]()
	{
		// Passes either way (500 overruns any shield), but the shield is dropped for the
		// same reason as above: the spec should say what it depends on.
		Target.ASC->SetNumericAttributeBase(UBNAttributeSet::GetShieldAttribute(), 0.f);
		BNDamage::ApplyDamage(Attacker.Actor, Target.Actor, 500.f, FHitResult());
		TestEqual(TEXT("Health"), Attr(Target, UBNAttributeSet::GetHealthAttribute()), 0.f);
	});

	It("records WHO did it, at the one reaction point", [this]()
	{
		BNDamage::ApplyDamage(Attacker.Actor, Target.Actor, 10.f, FHitResult());

		const FBNLastDamage& Last = Target.Set->GetLastDamage();
		TestEqual(TEXT("amount"), Last.Amount, 10.f);
		TestTrue(TEXT("the instigator is the attacker"), Last.Instigator.Get() == Attacker.Actor);
	});

	It("records WITH WHAT, which the killfeed and the death screen read (R7.3)", [this]()
	{
		// The door's fifth argument is the whole of R7.3's chain on this side: a name in, the
		// same name out of the capture that the death ability reads.
		BNDamage::ApplyDamage(Attacker.Actor, Target.Actor, 10.f, FHitResult(), FName(TEXT("Rifle")));
		TestEqual(TEXT("the cause"), Target.Set->GetLastDamage().SourceName, FName(TEXT("Rifle")));
	});

	It("leaves the cause blank when the door was told nothing", [this]()
	{
		// The honest-unknown rule, on the damage path: a hit with no named source must not inherit
		// the last one's weapon. This is the assertion that would fail if the door's stash were
		// ever left set instead of restored.
		BNDamage::ApplyDamage(Attacker.Actor, Target.Actor, 10.f, FHitResult(), FName(TEXT("Rifle")));
		BNDamage::ApplyDamage(Attacker.Actor, Target.Actor, 10.f, FHitResult());
		TestTrue(TEXT("cause is none"), Target.Set->GetLastDamage().SourceName.IsNone());
	});

	It("charges one grenade per cost and clamps the pouch at zero (R7.4)", [this]()
	{
		ApplyEffect(Target, UBNGE_GrenadeCost::StaticClass());
		TestEqual(TEXT("one thrown"), Attr(Target, UBNAttributeSet::GetGrenadesAttribute()), 1.f);

		ApplyEffect(Target, UBNGE_GrenadeCost::StaticClass());
		TestEqual(TEXT("two thrown"), Attr(Target, UBNAttributeSet::GetGrenadesAttribute()), 0.f);

		// A third cost landing anyway must not go negative. This is the BASE clamp the shield
		// recharge taught this attribute set: an instant Additive writes the base, and a negative
		// base would be hidden by the current-value clamp forever.
		ApplyEffect(Target, UBNGE_GrenadeCost::StaticClass());
		TestEqual(TEXT("Grenades"), Attr(Target, UBNAttributeSet::GetGrenadesAttribute()), 0.f);
	});
}

/**
 * NAMED GAPS — what this file deliberately does NOT cover, so nobody reads a green run as more
 * than it is:
 *
 * 1. GAS's own REFUSAL at zero grenades. CheckCost calls CanApplyAttributeModifiers, which is API
 *    this project has never compiled; asserting it from memory is how a spec breaks the build it
 *    exists to protect. The test above covers OUR half — the clamp and the modifier's shape —
 *    and the refusal itself is a PIE read-back (TEST-MATCH: the third throw does nothing).
 *
 * 2. The COST GE's SHAPE. A draft asserted on `UGameplayEffect::Modifiers` from outside the
 *    class. BNGameplayEffects.cpp writes that member from INSIDE its own constructors, which
 *    proves the member exists and proves nothing about its access level from a spec — the same
 *    "fairly confident" that produced the two APIs this file was rewritten to remove. The
 *    behaviour it would have guarded (one grenade per throw) is already covered above by the
 *    pouch going 2 -> 1 -> 0.
 *
 * 3. The HEAD-HIT MULTIPLIER and the rest of ApplyWeaponDamage. That function takes its victim
 *    from the hit (`Hit.GetActor()`), and naming a victim on a synthetic FHitResult needs either
 *    the four-argument constructor or FActorInstanceHandle — neither compiled here. Covering it
 *    honestly wants a real trace against a real collider, which is a heavier fixture than this
 *    file should grow on its first pass.
 */

#endif // WITH_DEV_AUTOMATION_TESTS
