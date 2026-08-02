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

namespace BRShieldSpecInternal
{
	static constexpr float Tolerance = 0.001f;

	static constexpr float PillarRechargeDelaySeconds = 2.5f;

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

	UWorld* World = nullptr;
	UCurveTable* CombatTable = nullptr;
	BRShieldSpecInternal::FFighter Target;
	BRShieldSpecInternal::FFighter Attacker;

	float TableMaxHealth = 0.f;
	float TableMaxShields = 0.f;
	float TableRegenRatePerSecond = 0.f;
	float TableRegenPeriodSeconds = 0.f;

	int32 DeathBroadcastCount = 0;

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
		CombatTable->AddToRoot();

		const TArray<FString> Problems = CombatTable->CreateTableFromCSVString(CsvText);
		for (const FString& Problem : Problems)
		{
			AddError(FString::Printf(TEXT("CT_Combat.csv failed to parse: %s"), *Problem));
		}

		BRCombatCurves::SetTableOverrideForTests(CombatTable);
		return Problems.Num() == 0;
	}

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
			World->DestroyWorld(false);
			World = nullptr;
		}
	}

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

		Target.ASC->ApplyGameplayEffectToSelf(Grant, 1.f, Target.ASC->MakeEffectContext());
	}

	float Shields() const { return Target.Set ? Target.Set->GetShields() : TNumericLimits<float>::Lowest(); }
	float Health() const { return Target.Set ? Target.Set->GetHealth() : TNumericLimits<float>::Lowest(); }
	float MaxShieldsAttr() const { return Target.Set ? Target.Set->GetMaxShields() : TNumericLimits<float>::Lowest(); }
	bool DeathLatched() const { return Target.Set != nullptr && Target.Set->HasReportedDeath(); }

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

	bool Respawn()
	{
		if (!Target.IsUsable())
		{
			AddError(TEXT("Respawn called with an unusable fixture."));
			return false;
		}
		return Target.ASC->ApplyInitStats();
	}

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

		Target.Set->OnDeath.AddLambda([this](AActor* Victim, AActor*, AActor*, const FGameplayEffectSpec&)
		{
			++DeathBroadcastCount;
			TestTrue(TEXT("OnDeath names the fighter that died as the victim"), Victim == Target.Actor);
		});

		if (!Attacker.ASC->ApplyInitStats() || !Respawn())
		{
			AddError(TEXT("ApplyInitStats refused; the fixture is uninitialised and no shield rule can be evaluated."));
		}
	});

	AfterEach([this]()
	{
		DestroyWorld();
	});

	Describe(TEXT("the recharge gate"), [this]()
	{
		It(TEXT("pins R5's 2.5 s recharge delay in CT_Combat"), [this]()
		{
			float DelaySeconds = -1.f;
			const bool bFound = BRCombatCurves::Evaluate(BRCombatCurves::Names::ShieldsRegenDelaySeconds, DelaySeconds);

			TestTrue(TEXT("CT_Combat declares Shields.Regen.DelaySeconds"), bFound);
			TestEqual(TEXT("R5 PILLAR: the shield recharge delay is 2.5 s"), DelaySeconds, PillarRechargeDelaySeconds, Tolerance);
		});

		It(TEXT("gates recharge on a tag, not on a timer, and blocks a corpse too"), [this]()
		{
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

			TestTrue(TEXT("a corpse does not recharge: State.Dead also inhibits regen"),
				Requirements->OngoingTagRequirements.IgnoreTags.HasTagExact(BRGameplayTags::State_Dead.GetTag()));

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

			TestEqual(TEXT("R5 PILLAR: the applied gate lasts 2.5 s"), Gate->Spec.GetDuration(), PillarRechargeDelaySeconds, Tolerance);
		});

		It(TEXT("inhibits the running recharge for as long as the gate is up"), [this]()
		{
			TestFalse(TEXT("recharge runs on an undamaged fighter"), IsRechargeInhibited());

			LandHit(TableMaxShields * 0.25f);

			TestTrue(TEXT("R5: recharge must NOT start while RecentDamage is active"), IsRechargeInhibited());

			TestEqual(TEXT("the recharge effect is still applied, merely switched off"),
				CountActiveEffects(UBRGE_Regen::StaticClass()), 1);
		});

		It(TEXT("refreshes one gate rather than stacking a second window"), [this]()
		{
			LandHit(TableMaxShields * 0.1f);
			LandHit(TableMaxShields * 0.1f);
			LandHit(TableMaxShields * 0.1f);

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

			TestEqual(TEXT("the recharge ticks at CT_Combat's period"), Regen->Spec.GetPeriod(), TableRegenPeriodSeconds, Tolerance);
			TestEqual(TEXT("each tick pays rate-per-second x period"),
				Regen->Spec.GetSetByCallerMagnitude(BRGameplayTags::SetByCaller_RegenRate.GetTag(), false),
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
			const UPackage* BreachpointPackage = UBRGE_Regen::StaticClass()->GetOutermost();

			TArray<UClass*> EffectClasses;
			GetDerivedClasses(UGameplayEffect::StaticClass(), EffectClasses, true);

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

			TestTrue(FString::Printf(TEXT("the effect library was actually enumerated (found %d Breachpoint effects, expected at least the seven in BRGameplayEffects.h)"), Examined),
				Examined >= 7);
		});

		It(TEXT("fills shields to max and never overshoots"), [this]()
		{
			LandHit(TableMaxShields * 0.5f);
			TestEqual(TEXT("the hit landed on shields"), Shields(), TableMaxShields * 0.5f, Tolerance);

			GrantShields(TableMaxShields * 10.f);

			TestEqual(TEXT("shields stop at MaxShields"), Shields(), TableMaxShields, Tolerance);
			TestTrue(TEXT("shields never exceed MaxShields"), Shields() <= MaxShieldsAttr() + Tolerance);
		});

		It(TEXT("R5: recharging shields does not restore one point of health"), [this]()
		{
			const float Overflow = TableMaxHealth * 0.4f;
			LandHit(TableMaxShields + Overflow);

			const float HealthAfterDamage = Health();
			TestEqual(TEXT("the overflow reached health"), HealthAfterDamage, TableMaxHealth - Overflow, Tolerance);

			GrantShields(TableMaxShields * 10.f);

			TestEqual(TEXT("shields came back to full"), Shields(), TableMaxShields, Tolerance);
			TestEqual(TEXT("R5 PILLAR: health is exactly where the damage left it"), Health(), HealthAfterDamage, Tolerance);
		});
	});

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
			LandHit(TableMaxShields);

			TestEqual(TEXT("shields are exactly zero"), Shields(), 0.f, Tolerance);
			TestEqual(TEXT("not one point of health was taken"), Health(), TableMaxHealth, Tolerance);
		});

		It(TEXT("neither creates nor loses damage at the shield/health seam"), [this]()
		{
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
			FGameplayTagContainer Tags;
			Tags.AddTag(BRGameplayTags::Damage_Kinetic.GetTag());

			const bool bApplied = LandHit(TableMaxShields * 0.25f, Tags);

			TestTrue(TEXT("a tagged hit applies through the one damage pipeline"), bApplied);
			TestTrue(TEXT("it came out of shields"), Shields() < TableMaxShields - Tolerance);
			TestEqual(TEXT("health is untouched while shields remain"), Health(), TableMaxHealth, Tolerance);
			TestEqual(TEXT("and it gated the recharge"), TagCount(BRGameplayTags::State_Combat_RecentDamage.GetTag()), 1);
		});
	});

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
			const FFighter Fresh = SpawnFighter();
			if (!TestTrue(TEXT("a fresh fighter was built"), Fresh.IsUsable()))
			{
				return;
			}

			TestEqual(TEXT("an uninitialised fighter has no shield capacity"), Fresh.Set->GetMaxShields(), 0.f, Tolerance);
			TestEqual(TEXT("and is NOT marked shields-broken"), Fresh.ASC->GetTagCount(BRGameplayTags::State_Shields_Broken.GetTag()), 0);
		});
	});

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

			LandHit(TableMaxHealth);
			LandHit(TableMaxHealth);

			TestEqual(TEXT("still exactly one death"), DeathBroadcastCount, 1);
			TestEqual(TEXT("health is still zero, never negative"), Health(), 0.f, Tolerance);
			TestTrue(TEXT("health never goes below zero"), Health() >= 0.f);
		});

		It(TEXT("discards overkill rather than banking it"), [this]()
		{
			LandHit((TableMaxShields + TableMaxHealth) * 100.f);

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

			TestEqual(TEXT("the attribute set reports death; it does not apply State.Dead"),
				TagCount(BRGameplayTags::State_Dead.GetTag()), 0);

			TestTrue(TEXT("the authority applies GE_Death"), ApplyDeath());
			TestEqual(TEXT("State.Dead is granted exactly once"), TagCount(BRGameplayTags::State_Dead.GetTag()), 1);

			TestFalse(TEXT("applying death twice is refused"), ApplyDeath());
			TestEqual(TEXT("still exactly one State.Dead"), TagCount(BRGameplayTags::State_Dead.GetTag()), 1);

			TestTrue(TEXT("R5: a corpse does not recharge its shields"), IsRechargeInhibited());
		});
	});
}

#endif
