#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/BNAttributeSet.h"
#include "AbilitySystem/BNAbilitySystemComponent.h"
#include "AbilitySystem/Effects/BNDamage.h"
#include "AbilitySystem/Effects/BNGameplayEffects.h"
#include "Data/BNDataRows.h"
#include "Engine/Engine.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

/**
 * THE ONE DOOR, tested through the door. Every assertion here goes in the way the game goes in —
 * BNDamage::ApplyDamage and ApplyWeaponDamage — never by writing an attribute, because a spec that
 * pokes attributes proves the attribute set works and nothing about whether the game can reach it.
 *
 * The world scaffolding is transcribed from the old module's `BRShieldSpec`, which compiled and ran
 * against this engine: CreateWorld + a world context + InitializeActorsForPlay, then an ASC and an
 * attribute set on a bare actor. Nothing here is invented API.
 *
 * SHIELDS ARE OFF by founder ruling (the init GE ships MaxShield 0), so every number below is a
 * health number. When shields come back, the shield-first assertions belong in this file.
 */
BEGIN_DEFINE_SPEC(FBNDamageSpec, "BreachpointNext.Sim.Damage",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

	struct FFighter
	{
		AActor* Actor = nullptr;
		UBNAbilitySystemComponent* ASC = nullptr;
		UBNAttributeSet* Set = nullptr;

		bool IsUsable() const { return Actor && ASC && Set; }
	};

	UWorld* World = nullptr;
	FFighter Target;
	FFighter Attacker;

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

	FFighter SpawnFighter()
	{
		FFighter Fighter;
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
	void ApplyInit(const FFighter& Fighter)
	{
		if (!Fighter.IsUsable())
		{
			return;
		}
		const FGameplayEffectSpecHandle Spec = Fighter.ASC->MakeOutgoingSpec(
			UBNGE_InitAttributes::StaticClass(), 1.f, Fighter.ASC->MakeEffectContext());
		if (Spec.IsValid())
		{
			Fighter.ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}
	}

	bool ApplyCost(const FFighter& Fighter, TSubclassOf<UGameplayEffect> CostClass)
	{
		if (!Fighter.IsUsable())
		{
			return false;
		}
		const FGameplayEffectSpecHandle Spec = Fighter.ASC->MakeOutgoingSpec(
			CostClass, 1.f, Fighter.ASC->MakeEffectContext());
		if (!Spec.IsValid())
		{
			return false;
		}
		Fighter.ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		return true;
	}

	float Attr(const FFighter& Fighter, const FGameplayAttribute& Attribute) const
	{
		return Fighter.IsUsable() ? Fighter.ASC->GetNumericAttribute(Attribute) : -1.f;
	}

	void DestroyWorld()
	{
		Target = FFighter();
		Attacker = FFighter();

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
		ApplyInit(Target);
		ApplyInit(Attacker);
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
		BNDamage::ApplyDamage(Attacker.Actor, Target.Actor, 30.f, FHitResult());
		TestEqual(TEXT("Health"), Attr(Target, UBNAttributeSet::GetHealthAttribute()), 70.f);
	});

	It("never drives health below zero", [this]()
	{
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

	It("doubles a head hit and records WITH WHAT (R7.3)", [this]()
	{
		FBNWeaponRow Row;
		Row.Damage = 20.f;
		Row.HeadshotMultiplier = 2.f;

		// The hit has to name its victim: ApplyWeaponDamage takes the target off the hit, exactly
		// as the fire ability's server-validated trace does.
		FHitResult Hit(Target.Actor, nullptr, FVector::ZeroVector, FVector::UpVector);
		Hit.BoneName = TEXT("head");
		Hit.bBlockingHit = true;

		BNDamage::ApplyWeaponDamage(Attacker.Actor, FName(TEXT("Rifle")), Row, Hit);

		TestEqual(TEXT("Health after a 20-damage head hit"),
			Attr(Target, UBNAttributeSet::GetHealthAttribute()), 60.f);
		TestEqual(TEXT("the cause, which the killfeed and death screen read"),
			Target.Set->GetLastDamage().SourceName, FName(TEXT("Rifle")));
	});

	It("charges a grenade and refuses the throw at zero (R7.4)", [this]()
	{
		ApplyCost(Target, UBNGE_GrenadeCost::StaticClass());
		TestEqual(TEXT("one thrown"), Attr(Target, UBNAttributeSet::GetGrenadesAttribute()), 1.f);

		ApplyCost(Target, UBNGE_GrenadeCost::StaticClass());
		TestEqual(TEXT("two thrown"), Attr(Target, UBNAttributeSet::GetGrenadesAttribute()), 0.f);

		// THE REFUSAL GAS DOES FOR FREE, and the reason the cost is a fixed -1 rather than a
		// SetByCaller: CheckCost asks this of the CDO before any spec exists.
		TestFalse(TEXT("an empty pouch cannot pay"),
			Target.ASC->CanApplyAttributeModifiers(
				GetDefault<UBNGE_GrenadeCost>(), 1.f, Target.ASC->MakeEffectContext()));
	});

	It("clamps the pouch at zero even if a cost lands anyway", [this]()
	{
		ApplyCost(Target, UBNGE_GrenadeCost::StaticClass());
		ApplyCost(Target, UBNGE_GrenadeCost::StaticClass());
		ApplyCost(Target, UBNGE_GrenadeCost::StaticClass());
		// The BASE clamp, which the shield recharge taught this attribute set: an instant Additive
		// writes the base, and a negative base would be hidden by the current-value clamp forever.
		TestEqual(TEXT("Grenades"), Attr(Target, UBNAttributeSet::GetGrenadesAttribute()), 0.f);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
