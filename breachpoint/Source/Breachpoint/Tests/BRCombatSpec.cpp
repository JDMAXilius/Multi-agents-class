// Breachpoint. The project's FIRST pinned sim suite: TTK, headshot math, the damage rule, row algebra.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "Engine/CurveTable.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/StrongObjectPtr.h"

#include "AbilitySystem/BRAttributeSet.h"
#include "AbilitySystem/BRCombatCurves.h"
#include "AbilitySystem/BRDamageExecCalc.h"
#include "Core/BRGameplayTags.h"
#include "Data/BRDataRows.h"

namespace BRCombatSpecInternal
{
	static const FName RowAR(TEXT("AR"));
	static const FName RowMagnum(TEXT("Magnum"));
	static const FName RowRocket(TEXT("Rocket"));

	static const TCHAR* SpecContext = TEXT("BRCombatSpec");

	static int32 ShotsToDeplete(float Pool, float DamagePerShot)
	{
		if (DamagePerShot <= 0.f)
		{
			return -1;
		}
		return FMath::CeilToInt(Pool / DamagePerShot);
	}

	static float TimeToLandShots(int32 ShotCount, float ShotIntervalSeconds)
	{
		return (ShotCount <= 1) ? 0.f : (ShotCount - 1) * ShotIntervalSeconds;
	}

	static FGameplayTagContainer MakeDamageTags(const TArray<FGameplayTag>& Tags)
	{
		FGameplayTagContainer Container;
		for (const FGameplayTag& Tag : Tags)
		{
			Container.AddTag(Tag);
		}
		return Container;
	}

	struct FStatedCurves
	{
		TMap<FName, float> Values;

		void Set(const FGameplayTag& Tag, float Multiplier)
		{
			Values.Add(UBRDamageExecCalc::MakeMultiplierCurveName(Tag), Multiplier);
		}

		bool Lookup(FName CurveName, float& OutValue) const
		{
			if (const float* Found = Values.Find(CurveName))
			{
				OutValue = *Found;
				return true;
			}
			return false;
		}
	};

	static constexpr float Tol = 1.e-4f;
}

BEGIN_DEFINE_SPEC(FBRCombatSpec, "Breachpoint.Sim.Combat",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

	TStrongObjectPtr<UDataTable> WeaponTable;

	TStrongObjectPtr<UCurveTable> CombatTable;

	bool EnsureTables();

	const FBRWeaponRow* Weapon(FName RowName);

	bool Curve(FName CurveName, float& OutValue);

END_DEFINE_SPEC(FBRCombatSpec)

bool FBRCombatSpec::EnsureTables()
{
	if (WeaponTable.IsValid() && CombatTable.IsValid())
	{
		BRCombatCurves::SetTableOverrideForTests(CombatTable.Get());
		return true;
	}

	const FString WeaponPath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data"), TEXT("DT_Weapons.csv"));
	const FString CurvePath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data"), TEXT("CT_Combat.csv"));

	FString WeaponCsv;
	if (!FFileHelper::LoadFileToString(WeaponCsv, *WeaponPath))
	{
		AddError(FString::Printf(TEXT("Could not read '%s'. This suite pins the SHIPPED table; it does not carry a copy of the numbers."), *WeaponPath));
		return false;
	}

	FString CurveCsv;
	if (!FFileHelper::LoadFileToString(CurveCsv, *CurvePath))
	{
		AddError(FString::Printf(TEXT("Could not read '%s'. This suite pins the SHIPPED coefficients; it does not carry a copy of them."), *CurvePath));
		return false;
	}

	UDataTable* NewWeaponTable = NewObject<UDataTable>();
	NewWeaponTable->RowStruct = FBRWeaponRow::StaticStruct();
	const TArray<FString> WeaponProblems = NewWeaponTable->CreateTableFromCSVString(WeaponCsv);
	for (const FString& Problem : WeaponProblems)
	{
		AddWarning(FString::Printf(TEXT("DT_Weapons.csv import: %s"), *Problem));
	}
	WeaponTable.Reset(NewWeaponTable);

	UCurveTable* NewCombatTable = NewObject<UCurveTable>();
	const TArray<FString> CurveProblems = NewCombatTable->CreateTableFromCSVString(CurveCsv);
	for (const FString& Problem : CurveProblems)
	{
		AddWarning(FString::Printf(TEXT("CT_Combat.csv import: %s"), *Problem));
	}
	CombatTable.Reset(NewCombatTable);

	BRCombatCurves::SetTableOverrideForTests(CombatTable.Get());
	return true;
}

const FBRWeaponRow* FBRCombatSpec::Weapon(FName RowName)
{
	if (!WeaponTable.IsValid())
	{
		return nullptr;
	}

	const FBRWeaponRow* Row = WeaponTable->FindRow<FBRWeaponRow>(RowName, BRCombatSpecInternal::SpecContext, false);
	if (!Row)
	{
		AddError(FString::Printf(TEXT("DT_Weapons.csv has no row '%s'. The row NAME is the handle the whole game addresses this weapon by; renaming it is a breaking change, not a tidy-up."), *RowName.ToString()));
	}
	return Row;
}

bool FBRCombatSpec::Curve(FName CurveName, float& OutValue)
{
	OutValue = 0.f;
	if (!BRCombatCurves::Evaluate(CurveName, OutValue))
	{
		AddError(FString::Printf(TEXT("CT_Combat.csv has no curve '%s'. The sim reads it by this exact name; a missing row is not a default, it is a coefficient the sim cannot get."), *CurveName.ToString()));
		return false;
	}
	return true;
}

void FBRCombatSpec::Define()
{
	using namespace BRCombatSpecInternal;

	AfterEach([this]()
	{
		BRCombatCurves::SetTableOverrideForTests(nullptr);
	});

	Describe("FBRWeaponRow row algebra", [this]()
	{
		It("converts RPM into the shortest legal interval between two shots, per shipped row", [this]()
		{
			if (!EnsureTables()) { return; }
			const FBRWeaponRow* AR = Weapon(RowAR);
			const FBRWeaponRow* Magnum = Weapon(RowMagnum);
			const FBRWeaponRow* Rocket = Weapon(RowRocket);
			if (!AR || !Magnum || !Rocket) { return; }

			TestEqual(TEXT("AR: 600 RPM is one shot every 0.100 s"), AR->GetMinShotIntervalSeconds(), 0.1f, Tol);
			TestEqual(TEXT("Magnum: 180 RPM is one shot every 0.3333 s"), Magnum->GetMinShotIntervalSeconds(), 60.f / 180.f, Tol);
			TestEqual(TEXT("Rocket: 30 RPM is one shot every 2.000 s"), Rocket->GetMinShotIntervalSeconds(), 2.f, Tol);
		});

		It("refuses to express a rate gate for a row with no RPM, rather than reporting 'unlimited'", [this]()
		{
			const FBRWeaponRow Unconfigured;
			TestEqual(TEXT("RPM <= 0 yields a 0 s interval, meaning 'no rate gate expressible'"), Unconfigured.GetMinShotIntervalSeconds(), 0.f, Tol);
			TestEqual(TEXT("...and 'shots to deplete' is unanswerable at zero damage, not infinite"), ShotsToDeplete(200.f, Unconfigured.DamagePerShot), -1);
		});

		It("computes starting reserve ammo as MagSize x ReserveMags, and the Rocket's is ZERO (R4)", [this]()
		{
			if (!EnsureTables()) { return; }
			const FBRWeaponRow* AR = Weapon(RowAR);
			const FBRWeaponRow* Magnum = Weapon(RowMagnum);
			const FBRWeaponRow* Rocket = Weapon(RowRocket);
			if (!AR || !Magnum || !Rocket) { return; }

			TestEqual(TEXT("AR: 32 x 5 = 160 rounds in reserve on pickup"), AR->GetStartingReserveAmmo(), 160);
			TestEqual(TEXT("Magnum: 8 x 4 = 32 rounds in reserve on pickup"), Magnum->GetStartingReserveAmmo(), 32);

			TestEqual(TEXT("R4: the Rocket carries ZERO reserve -- reload is intentionally unreachable"), Rocket->GetStartingReserveAmmo(), 0);
			TestEqual(TEXT("R4: ...and its magazine is 2"), Rocket->MagSize, 2);
		});

		It("converts designer metres into Unreal centimetres at the accessor, never at the call site", [this]()
		{
			if (!EnsureTables()) { return; }
			const FBRWeaponRow* AR = Weapon(RowAR);
			const FBRWeaponRow* Rocket = Weapon(RowRocket);
			if (!AR || !Rocket) { return; }

			TestEqual(TEXT("Rocket: a 4 m splash radius is 400 uu"), Rocket->GetSplashRadiusCm(), 400.f, Tol);
			TestEqual(TEXT("Rocket: 30 m/s of projectile is 3000 uu/s"), Rocket->GetProjectileSpeedCmPerSecond(), 3000.f, Tol);

			TestEqual(TEXT("AR: a hitscan row has no splash radius, in either unit"), AR->GetSplashRadiusCm(), 0.f, Tol);
			TestEqual(TEXT("AR: a hitscan row has no projectile speed, in either unit"), AR->GetProjectileSpeedCmPerSecond(), 0.f, Tol);
		});

		It("accepts every shipped row's schema", [this]()
		{
			if (!EnsureTables()) { return; }

			TArray<FBRWeaponRow*> Rows;
			WeaponTable->GetAllRows<FBRWeaponRow>(SpecContext, Rows);
			TestEqual(TEXT("DT_Weapons.csv ships exactly three weapons"), Rows.Num(), 3);

			for (const FBRWeaponRow* Row : Rows)
			{
				FString Error;
				const bool bValid = Row && Row->ValidateSchema(Error);
				TestTrue(FString::Printf(TEXT("a shipped row violates its own schema: %s"), *Error), bValid);
			}
		});

		It("refuses the structural violations ValidateSchema exists to catch", [this]()
		{
			if (!EnsureTables()) { return; }
			const FBRWeaponRow* AR = Weapon(RowAR);
			if (!AR) { return; }

			FString Error;

			{
				FBRWeaponRow Row = *AR;
				Row.HeadshotMult = 0.9f;
				TestFalse(TEXT("HeadshotMult below 1.0 is refused"), Row.ValidateSchema(Error));
			}
			{
				FBRWeaponRow Row = *AR;
				Row.ProjectileSpeed = 30.f;
				TestFalse(TEXT("a hitscan row carrying a projectile speed is refused"), Row.ValidateSchema(Error));
			}
			{
				FBRWeaponRow Row = *AR;
				Row.DamageDelivery = EBRDamageDelivery::Projectile;
				TestFalse(TEXT("a projectile row carrying no projectile speed is refused"), Row.ValidateSchema(Error));
			}
			{
				FBRWeaponRow Row = *AR;
				Row.RPM = 0.f;
				TestFalse(TEXT("a row with no RPM is refused -- the rate gate needs a ceiling"), Row.ValidateSchema(Error));
			}
			{
				FBRWeaponRow Row = *AR;
				Row.MagSize = 0;
				TestFalse(TEXT("a row that cannot hold a round is refused"), Row.ValidateSchema(Error));
			}
			{
				FBRWeaponRow Row = *AR;
				Row.SplashRadius_m = 4.f;
				TestFalse(TEXT("a splash radius with no splash damage is refused"), Row.ValidateSchema(Error));
			}
			{
				FBRWeaponRow Row = *AR;
				Row.DamagePerShot = -1.f;
				TestFalse(TEXT("negative damage in a row is refused -- it would HEAL the target"), Row.ValidateSchema(Error));
			}

			TestTrue(TEXT("ValidateSchema writes a human-readable reason when it refuses"), !Error.IsEmpty());
		});
	});

	Describe("Headshot multipliers", [this]()
	{
		It("R2: the AR's HeadshotMult is 1.0 -- it is the shield-stripper, not a finisher", [this]()
		{
			if (!EnsureTables()) { return; }
			const FBRWeaponRow* AR = Weapon(RowAR);
			if (!AR) { return; }

			TestEqual(TEXT("R2: AR HeadshotMult is exactly 1.0 (no bonus, deliberately)"), AR->HeadshotMult, 1.f, Tol);
			TestEqual(TEXT("R2: an AR headshot deals the same 8 as an AR body shot"), AR->DamagePerShot * AR->HeadshotMult, AR->DamagePerShot, Tol);
		});

		It("R4: the Rocket's HeadshotMult is 1.0 -- a 240-damage one-shot headshot is a DEFECT", [this]()
		{
			if (!EnsureTables()) { return; }
			const FBRWeaponRow* Rocket = Weapon(RowRocket);
			if (!Rocket) { return; }

			float MaxHealth = 0.f;
			float MaxShields = 0.f;
			if (!Curve(BRCombatCurves::Names::FighterMaxHealth, MaxHealth)) { return; }
			if (!Curve(BRCombatCurves::Names::FighterMaxShields, MaxShields)) { return; }
			const float EffectiveHealth = MaxHealth + MaxShields;

			TestEqual(TEXT("R4: Rocket HeadshotMult is exactly 1.0"), Rocket->HeadshotMult, 1.f, Tol);

			const float HeadshotAsShipped = Rocket->DamagePerShot * Rocket->HeadshotMult;
			TestEqual(TEXT("R4: a Rocket headshot deals 120, the same as a body hit"), HeadshotAsShipped, 120.f, Tol);
			TestTrue(TEXT("R4: 120 does NOT one-shot a full-EHP fighter"), HeadshotAsShipped < EffectiveHealth);
			TestTrue(TEXT("R4: at HeadshotMult 2.0 it WOULD (240 >= 200) -- that is the defect, not a buff"), (Rocket->DamagePerShot * 2.f) >= EffectiveHealth);
		});

		It("R1: the Magnum's HeadshotMult is 2.0 -- precision weapons are where the bonus lives", [this]()
		{
			if (!EnsureTables()) { return; }
			const FBRWeaponRow* Magnum = Weapon(RowMagnum);
			if (!Magnum) { return; }

			TestEqual(TEXT("R1: Magnum HeadshotMult is exactly 2.0"), Magnum->HeadshotMult, 2.f, Tol);
			TestEqual(TEXT("R1: a Magnum headshot deals 44, a body shot 22"), Magnum->DamagePerShot * Magnum->HeadshotMult, 44.f, Tol);
		});

		It("keeps the GLOBAL Damage.Headshot multiplier at identity, so the per-weapon column is the ONLY headshot bonus", [this]()
		{
			if (!EnsureTables()) { return; }

			float GlobalHeadshot = 0.f;
			const FName HeadshotCurve = UBRDamageExecCalc::MakeMultiplierCurveName(BRGameplayTags::Damage_Headshot.GetTag());
			if (!Curve(HeadshotCurve, GlobalHeadshot)) { return; }

			TestEqual(TEXT("CT_Combat's Damage.Headshot.Multiplier is the identity 1.0"), GlobalHeadshot, 1.f, Tol);

			const FGameplayTagContainer Body = MakeDamageTags({ BRGameplayTags::Damage_Kinetic.GetTag() });
			const FGameplayTagContainer Head = MakeDamageTags({ BRGameplayTags::Damage_Kinetic.GetTag(), BRGameplayTags::Damage_Headshot.GetTag() });
			auto RealLookup = [](FName Name, float& Out) { return BRCombatCurves::Evaluate(Name, Out); };

			const float BodyDamage = UBRDamageExecCalc::ComputeFinalDamage(22.f, Body, RealLookup);
			const float HeadDamage = UBRDamageExecCalc::ComputeFinalDamage(22.f, Head, RealLookup);
			TestEqual(TEXT("the exec calc adds NO second headshot bonus of its own"), HeadDamage, BodyDamage, Tol);
		});
	});

	Describe("CT_Combat coefficients", [this]()
	{
		It("pins the fighter at 100 shields over 100 health -- 200 EHP", [this]()
		{
			if (!EnsureTables()) { return; }

			float MaxHealth = 0.f;
			float MaxShields = 0.f;
			if (!Curve(BRCombatCurves::Names::FighterMaxHealth, MaxHealth)) { return; }
			if (!Curve(BRCombatCurves::Names::FighterMaxShields, MaxShields)) { return; }

			TestEqual(TEXT("Fighter.MaxHealth is 100"), MaxHealth, 100.f, Tol);
			TestEqual(TEXT("Fighter.MaxShields is 100"), MaxShields, 100.f, Tol);

			TestEqual(TEXT("effective health is 200, and every TTK in this file rests on it"), MaxHealth + MaxShields, 200.f, Tol);
		});

		It("pins R5's shield-recharge pillars: 60 points per second after a 2.5 s delay", [this]()
		{
			if (!EnsureTables()) { return; }

			float Rate = 0.f;
			float Delay = 0.f;
			if (!Curve(BRCombatCurves::Names::ShieldsRegenRatePerSecond, Rate)) { return; }
			if (!Curve(BRCombatCurves::Names::ShieldsRegenDelaySeconds, Delay)) { return; }

			TestEqual(TEXT("R5: the shield recharge delay is 2.5 s"), Delay, 2.5f, Tol);
			TestEqual(TEXT("shields recharge at 60 points/second"), Rate, 60.f, Tol);

			float Period = 0.f;
			if (!Curve(BRCombatCurves::Names::ShieldsRegenPeriodSeconds, Period)) { return; }
			TestTrue(TEXT("the regen period is positive -- a zero period is not a rule, it is a hang"), Period > 0.f);
			TestTrue(TEXT("the regen period is shorter than the gate, or the first tick lands late"), Period < Delay);
			TestEqual(TEXT("the shipped regen period is 0.25 s (unsourced; curator owns it)"), Period, 0.25f, Tol);
		});

		It("ships a multiplier curve for every one of the five Damage.* tags", [this]()
		{
			if (!EnsureTables()) { return; }

			const TArray<FGameplayTag> AllDamageTags = {
				BRGameplayTags::Damage_Kinetic.GetTag(),
				BRGameplayTags::Damage_Explosive.GetTag(),
				BRGameplayTags::Damage_Melee.GetTag(),
				BRGameplayTags::Damage_Headshot.GetTag(),
				BRGameplayTags::Damage_Rear.GetTag()
			};

			TArray<FName> Missing;
			const FGameplayTagContainer Everything = MakeDamageTags(AllDamageTags);
			auto RealLookup = [](FName Name, float& Out) { return BRCombatCurves::Evaluate(Name, Out); };
			const float Final = UBRDamageExecCalc::ComputeFinalDamage(100.f, Everything, RealLookup, &Missing);

			TestEqual(TEXT("no Damage.* tag falls through to the identity today"), Missing.Num(), 0);

			TestEqual(TEXT("all five multipliers are 1.0 today, so the product is the base"), Final, 100.f, Tol);
		});
	});

	Describe("UBRDamageExecCalc", [this]()
	{
		It("derives the curve name from the tag, with no switch and no tag-to-curve map", [this]()
		{
			TestTrue(TEXT("Damage.Headshot -> Damage.Headshot.Multiplier"),
				UBRDamageExecCalc::MakeMultiplierCurveName(BRGameplayTags::Damage_Headshot.GetTag()) == FName(TEXT("Damage.Headshot.Multiplier")));
			TestTrue(TEXT("Damage.Rear -> Damage.Rear.Multiplier"),
				UBRDamageExecCalc::MakeMultiplierCurveName(BRGameplayTags::Damage_Rear.GetTag()) == FName(TEXT("Damage.Rear.Multiplier")));
			TestTrue(TEXT("Damage.Explosive -> Damage.Explosive.Multiplier"),
				UBRDamageExecCalc::MakeMultiplierCurveName(BRGameplayTags::Damage_Explosive.GetTag()) == FName(TEXT("Damage.Explosive.Multiplier")));
		});

		It("R22: Damage.* is FLAT -- there is no Damage.Melee.Rear, and Rear's parent is Damage", [this]()
		{
			const FGameplayTag Nested = FGameplayTag::RequestGameplayTag(FName(TEXT("Damage.Melee.Rear")), false);
			TestFalse(TEXT("R22: `Damage.Melee.Rear` is not a registered tag and must never become one"), Nested.IsValid());

			const FGameplayTag TypeParent = BRGameplayTags::Damage_Melee.GetTag().RequestDirectParent();
			const FGameplayTag ModifierParent = BRGameplayTags::Damage_Rear.GetTag().RequestDirectParent();
			TestTrue(TEXT("R22: Damage.Melee (a type) and Damage.Rear (a modifier) share one parent"), TypeParent == ModifierParent);
			TestTrue(TEXT("R22: ...and that parent is `Damage`"), TypeParent == FGameplayTag::RequestGameplayTag(FName(TEXT("Damage")), false));

			FStatedCurves Stated;
			Stated.Set(BRGameplayTags::Damage_Melee.GetTag(), 2.f);
			Stated.Set(BRGameplayTags::Damage_Rear.GetTag(), 3.f);

			TArray<FName> Missing;
			const FGameplayTagContainer RearMelee = MakeDamageTags({ BRGameplayTags::Damage_Melee.GetTag(), BRGameplayTags::Damage_Rear.GetTag() });
			const float Final = UBRDamageExecCalc::ComputeFinalDamage(10.f, RearMelee,
				[&Stated](FName Name, float& Out) { return Stated.Lookup(Name, Out); }, &Missing);

			TestEqual(TEXT("R22: a rear melee multiplies BOTH curves (10 x 2 x 3)"), Final, 60.f, Tol);
			TestEqual(TEXT("R22: and looks up exactly two curve names, not one nested one"), Missing.Num(), 0);
		});

		It("ignores every tag outside the Damage.* family", [this]()
		{
			FStatedCurves Stated;
			Stated.Set(BRGameplayTags::Damage_Kinetic.GetTag(), 1.f);
			Stated.Values.Add(FName(TEXT("State.Dead.Multiplier")), 3.f);

			const FGameplayTagContainer Mixed = MakeDamageTags({ BRGameplayTags::Damage_Kinetic.GetTag(), BRGameplayTags::State_Dead.GetTag() });
			const float Final = UBRDamageExecCalc::ComputeFinalDamage(50.f, Mixed,
				[&Stated](FName Name, float& Out) { return Stated.Lookup(Name, Out); });

			TestEqual(TEXT("a State.* tag on the spec contributes nothing, even when a curve exists for it"), Final, 50.f, Tol);
		});

		It("treats a Damage.* tag with no curve row as the identity, and REPORTS the gap", [this]()
		{
			FStatedCurves Empty;

			TArray<FName> Missing;
			const FGameplayTagContainer Kinetic = MakeDamageTags({ BRGameplayTags::Damage_Kinetic.GetTag() });
			const float Final = UBRDamageExecCalc::ComputeFinalDamage(50.f, Kinetic,
				[&Empty](FName Name, float& Out) { return Empty.Lookup(Name, Out); }, &Missing);

			TestEqual(TEXT("a missing curve contributes 1.0, so the base survives untouched"), Final, 50.f, Tol);
			TestEqual(TEXT("...and the gap is reported, once, by curve name"), Missing.Num(), 1);
			if (Missing.Num() == 1)
			{
				TestTrue(TEXT("...naming the curve the CSV is missing"), Missing[0] == FName(TEXT("Damage.Kinetic.Multiplier")));
			}
		});

		It("refuses negative damage at the front door", [this]()
		{
			FStatedCurves Stated;
			Stated.Set(BRGameplayTags::Damage_Kinetic.GetTag(), 2.f);

			const FGameplayTagContainer Kinetic = MakeDamageTags({ BRGameplayTags::Damage_Kinetic.GetTag() });
			auto Lookup = [&Stated](FName Name, float& Out) { return Stated.Lookup(Name, Out); };

			TestEqual(TEXT("a negative base magnitude yields 0, never a heal"), UBRDamageExecCalc::ComputeFinalDamage(-25.f, Kinetic, Lookup), 0.f, Tol);
			TestEqual(TEXT("a zero base magnitude yields 0"), UBRDamageExecCalc::ComputeFinalDamage(0.f, Kinetic, Lookup), 0.f, Tol);
		});

		It("never returns a negative number, whatever the CSV says", [this]()
		{
			const TArray<float> Bases = { 0.f, 1.f, 8.f, 22.f, 120.f, 1.e6f };
			const TArray<float> Multipliers = { -5.f, -1.f, 0.f, 0.5f, 1.f, 2.f };
			const FGameplayTagContainer Kinetic = MakeDamageTags({ BRGameplayTags::Damage_Kinetic.GetTag() });

			for (float Base : Bases)
			{
				for (float Multiplier : Multipliers)
				{
					FStatedCurves Stated;
					Stated.Set(BRGameplayTags::Damage_Kinetic.GetTag(), Multiplier);
					const float Final = UBRDamageExecCalc::ComputeFinalDamage(Base, Kinetic,
						[&Stated](FName Name, float& Out) { return Stated.Lookup(Name, Out); });

					TestTrue(FString::Printf(TEXT("damage is never negative (base %.1f x multiplier %.1f gave %.3f)"), Base, Multiplier, Final), Final >= 0.f);
					if (Multiplier >= 0.f)
					{
						TestEqual(FString::Printf(TEXT("a non-negative multiplier composes exactly (base %.1f x %.1f)"), Base, Multiplier), Final, Base * Multiplier, Tol);
					}
				}
			}
		});

		It("is order-independent: the product does not depend on tag container order", [this]()
		{
			FStatedCurves Stated;
			Stated.Set(BRGameplayTags::Damage_Explosive.GetTag(), 2.f);
			Stated.Set(BRGameplayTags::Damage_Headshot.GetTag(), 3.f);
			Stated.Set(BRGameplayTags::Damage_Rear.GetTag(), 5.f);
			auto Lookup = [&Stated](FName Name, float& Out) { return Stated.Lookup(Name, Out); };

			const FGameplayTagContainer Forward = MakeDamageTags({
				BRGameplayTags::Damage_Explosive.GetTag(), BRGameplayTags::Damage_Headshot.GetTag(), BRGameplayTags::Damage_Rear.GetTag() });
			const FGameplayTagContainer Reverse = MakeDamageTags({
				BRGameplayTags::Damage_Rear.GetTag(), BRGameplayTags::Damage_Headshot.GetTag(), BRGameplayTags::Damage_Explosive.GetTag() });

			const float A = UBRDamageExecCalc::ComputeFinalDamage(4.f, Forward, Lookup);
			const float B = UBRDamageExecCalc::ComputeFinalDamage(4.f, Reverse, Lookup);

			TestEqual(TEXT("4 x 2 x 3 x 5 = 120, in either order"), A, 120.f, Tol);
			TestEqual(TEXT("the two orders agree exactly"), A, B, Tol);
		});

		It("never lets a modifier of 1.0 or more REDUCE the damage dealt", [this]()
		{
			const TArray<float> Modifiers = { 1.f, 1.25f, 2.f, 10.f };
			for (float Modifier : Modifiers)
			{
				FStatedCurves Stated;
				Stated.Set(BRGameplayTags::Damage_Kinetic.GetTag(), 1.f);
				Stated.Set(BRGameplayTags::Damage_Headshot.GetTag(), Modifier);
				auto Lookup = [&Stated](FName Name, float& Out) { return Stated.Lookup(Name, Out); };

				const float Plain = UBRDamageExecCalc::ComputeFinalDamage(22.f,
					MakeDamageTags({ BRGameplayTags::Damage_Kinetic.GetTag() }), Lookup);
				const float Modified = UBRDamageExecCalc::ComputeFinalDamage(22.f,
					MakeDamageTags({ BRGameplayTags::Damage_Kinetic.GetTag(), BRGameplayTags::Damage_Headshot.GetTag() }), Lookup);

				TestTrue(FString::Printf(TEXT("a x%.2f modifier never lowers the number"), Modifier), Modified >= Plain - Tol);
			}
		});

		It("computes the shipped weapons' single-hit numbers against the real CT_Combat", [this]()
		{
			if (!EnsureTables()) { return; }
			const FBRWeaponRow* AR = Weapon(RowAR);
			const FBRWeaponRow* Magnum = Weapon(RowMagnum);
			const FBRWeaponRow* Rocket = Weapon(RowRocket);
			if (!AR || !Magnum || !Rocket) { return; }

			auto RealLookup = [](FName Name, float& Out) { return BRCombatCurves::Evaluate(Name, Out); };
			const FGameplayTagContainer Kinetic = MakeDamageTags({ BRGameplayTags::Damage_Kinetic.GetTag() });
			const FGameplayTagContainer KineticHead = MakeDamageTags({ BRGameplayTags::Damage_Kinetic.GetTag(), BRGameplayTags::Damage_Headshot.GetTag() });
			const FGameplayTagContainer Explosive = MakeDamageTags({ BRGameplayTags::Damage_Explosive.GetTag() });

			TestEqual(TEXT("AR body hit: 8"), UBRDamageExecCalc::ComputeFinalDamage(AR->DamagePerShot, Kinetic, RealLookup), 8.f, Tol);
			TestEqual(TEXT("AR head hit: 8 (R2 -- no bonus)"), UBRDamageExecCalc::ComputeFinalDamage(AR->DamagePerShot * AR->HeadshotMult, KineticHead, RealLookup), 8.f, Tol);
			TestEqual(TEXT("Magnum body hit: 22"), UBRDamageExecCalc::ComputeFinalDamage(Magnum->DamagePerShot, Kinetic, RealLookup), 22.f, Tol);
			TestEqual(TEXT("Magnum head hit: 44 (R1 -- the bonus lives on precision weapons)"), UBRDamageExecCalc::ComputeFinalDamage(Magnum->DamagePerShot * Magnum->HeadshotMult, KineticHead, RealLookup), 44.f, Tol);
			TestEqual(TEXT("Rocket direct hit: 120"), UBRDamageExecCalc::ComputeFinalDamage(Rocket->DamagePerShot, Explosive, RealLookup), 120.f, Tol);
			TestEqual(TEXT("Rocket splash at centre: 90"), UBRDamageExecCalc::ComputeFinalDamage(Rocket->SplashDamage, Explosive, RealLookup), 90.f, Tol);
		});
	});

	Describe("Time to kill", [this]()
	{
		It("R1: the Magnum's all-headshot TTK is 5 shots in 1.333 s, and beats every other path", [this]()
		{
			if (!EnsureTables()) { return; }
			const FBRWeaponRow* AR = Weapon(RowAR);
			const FBRWeaponRow* Magnum = Weapon(RowMagnum);
			const FBRWeaponRow* Rocket = Weapon(RowRocket);
			if (!AR || !Magnum || !Rocket) { return; }

			float MaxHealth = 0.f;
			float MaxShields = 0.f;
			if (!Curve(BRCombatCurves::Names::FighterMaxHealth, MaxHealth)) { return; }
			if (!Curve(BRCombatCurves::Names::FighterMaxShields, MaxShields)) { return; }
			const float EffectiveHealth = MaxHealth + MaxShields;

			const int32 MagnumHeadshots = ShotsToDeplete(EffectiveHealth, Magnum->DamagePerShot * Magnum->HeadshotMult);
			const float MagnumHeadshotTTK = TimeToLandShots(MagnumHeadshots, Magnum->GetMinShotIntervalSeconds());

			TestEqual(TEXT("R1: five 44-damage headshots kill a 200 EHP fighter"), MagnumHeadshots, 5);
			TestTrue(TEXT("R1: ...and four do not -- the fifth is the kill, not a rounding artefact"), (4 * Magnum->DamagePerShot * Magnum->HeadshotMult) < EffectiveHealth);
			TestTrue(TEXT("R1: five shots fit in the 8-round magazine, with three misses' forgiveness"), MagnumHeadshots <= Magnum->MagSize);
			TestEqual(TEXT("R1: the Magnum's all-headshot TTK is 1.3333 s"), MagnumHeadshotTTK, 4.f * (60.f / 180.f), Tol);

			const float ARSoloTTK = TimeToLandShots(ShotsToDeplete(EffectiveHealth, AR->DamagePerShot), AR->GetMinShotIntervalSeconds());
			const float RocketTTK = TimeToLandShots(ShotsToDeplete(EffectiveHealth, Rocket->DamagePerShot), Rocket->GetMinShotIntervalSeconds());

			TestTrue(TEXT("R1: the all-headshot Magnum beats the AR's solo TTK"), MagnumHeadshotTTK < ARSoloTTK);
			TestTrue(TEXT("R1: the all-headshot Magnum beats the Rocket's two-shot TTK"), MagnumHeadshotTTK < RocketTTK);
		});

		It("R2: the AR's solo TTK is 25 shots in 2.4 s, and aiming at the head changes nothing", [this]()
		{
			if (!EnsureTables()) { return; }
			const FBRWeaponRow* AR = Weapon(RowAR);
			if (!AR) { return; }

			float MaxHealth = 0.f;
			float MaxShields = 0.f;
			if (!Curve(BRCombatCurves::Names::FighterMaxHealth, MaxHealth)) { return; }
			if (!Curve(BRCombatCurves::Names::FighterMaxShields, MaxShields)) { return; }
			const float EffectiveHealth = MaxHealth + MaxShields;

			const int32 BodyShots = ShotsToDeplete(EffectiveHealth, AR->DamagePerShot);
			const int32 HeadShots = ShotsToDeplete(EffectiveHealth, AR->DamagePerShot * AR->HeadshotMult);

			TestEqual(TEXT("R2: 25 body shots of 8 damage kill a 200 EHP fighter"), BodyShots, 25);
			TestEqual(TEXT("R2: an all-headshot AR needs the SAME 25 shots (HeadshotMult 1.0)"), HeadShots, 25);
			TestTrue(TEXT("R2: 25 shots fit inside the 32-round magazine -- one mag, one kill"), BodyShots <= AR->MagSize);
			TestEqual(TEXT("R2: the AR's solo TTK is 2.400 s"), TimeToLandShots(BodyShots, AR->GetMinShotIntervalSeconds()), 2.4f, Tol);
		});

		It("R3: the Magnum cannot solo a full-EHP target from one magazine of body shots", [this]()
		{
			if (!EnsureTables()) { return; }
			const FBRWeaponRow* Magnum = Weapon(RowMagnum);
			if (!Magnum) { return; }

			float MaxHealth = 0.f;
			float MaxShields = 0.f;
			if (!Curve(BRCombatCurves::Names::FighterMaxHealth, MaxHealth)) { return; }
			if (!Curve(BRCombatCurves::Names::FighterMaxShields, MaxShields)) { return; }
			const float EffectiveHealth = MaxHealth + MaxShields;

			const int32 BodyShots = ShotsToDeplete(EffectiveHealth, Magnum->DamagePerShot);
			TestEqual(TEXT("R3: 10 body shots of 22 would be needed"), BodyShots, 10);
			TestTrue(TEXT("R3: ...and the magazine holds 8 -- the Magnum is a finisher, not a primary"), BodyShots > Magnum->MagSize);
			TestEqual(TEXT("R3: a full 8-round mag of body shots deals 176, short of 200"), Magnum->DamagePerShot * Magnum->MagSize, 176.f, Tol);
		});

		It("R3: the intended line is AR-strip -> 0.4 s swap -> Magnum finish", [this]()
		{
			if (!EnsureTables()) { return; }
			const FBRWeaponRow* AR = Weapon(RowAR);
			const FBRWeaponRow* Magnum = Weapon(RowMagnum);
			if (!AR || !Magnum) { return; }

			float MaxHealth = 0.f;
			float MaxShields = 0.f;
			if (!Curve(BRCombatCurves::Names::FighterMaxHealth, MaxHealth)) { return; }
			if (!Curve(BRCombatCurves::Names::FighterMaxShields, MaxShields)) { return; }

			const int32 StripShots = ShotsToDeplete(MaxShields, AR->DamagePerShot);
			const float Overkill = (StripShots * AR->DamagePerShot) - MaxShields;
			const float HealthRemaining = MaxHealth - Overkill;

			TestEqual(TEXT("R3: 13 AR shots strip a 100-point shield"), StripShots, 13);
			TestEqual(TEXT("R3: the 13th shot overflows 4 damage into health -- no damage is lost at the seam"), Overkill, 4.f, Tol);
			TestEqual(TEXT("R3: 96 health is left for the finisher"), HealthRemaining, 96.f, Tol);

			const float StripTime = TimeToLandShots(StripShots, AR->GetMinShotIntervalSeconds());
			TestEqual(TEXT("R3: the strip takes 1.200 s"), StripTime, 1.2f, Tol);

			TestEqual(TEXT("R3: the swap to the Magnum costs 0.400 s, exactly as the ruling names"), Magnum->EquipTime_s, 0.4f, Tol);

			const int32 FinishHeadshots = ShotsToDeplete(HealthRemaining, Magnum->DamagePerShot * Magnum->HeadshotMult);
			const float FinishTime = TimeToLandShots(FinishHeadshots, Magnum->GetMinShotIntervalSeconds());
			TestEqual(TEXT("R3: three 44-damage headshots finish 96 health"), FinishHeadshots, 3);
			TestTrue(TEXT("R3: three shots fit in the 8-round magazine"), FinishHeadshots <= Magnum->MagSize);

			const float LineTTK = StripTime + Magnum->EquipTime_s + FinishTime;
			TestEqual(TEXT("R3: the whole intended line is 2.2667 s"), LineTTK, 1.2f + 0.4f + 2.f * (60.f / 180.f), Tol);

			const float ARSoloTTK = TimeToLandShots(ShotsToDeplete(MaxHealth + MaxShields, AR->DamagePerShot), AR->GetMinShotIntervalSeconds());
			TestTrue(TEXT("R3: the strip-swap-finish line beats the AR's 2.400 s solo TTK"), LineTTK < ARSoloTTK);
		});

		It("R3: the swap only pays when the finish is headshots -- a body-shot finish is SLOWER than the AR alone", [this]()
		{
			if (!EnsureTables()) { return; }
			const FBRWeaponRow* AR = Weapon(RowAR);
			const FBRWeaponRow* Magnum = Weapon(RowMagnum);
			if (!AR || !Magnum) { return; }

			float MaxHealth = 0.f;
			float MaxShields = 0.f;
			if (!Curve(BRCombatCurves::Names::FighterMaxHealth, MaxHealth)) { return; }
			if (!Curve(BRCombatCurves::Names::FighterMaxShields, MaxShields)) { return; }

			const int32 StripShots = ShotsToDeplete(MaxShields, AR->DamagePerShot);
			const float HealthRemaining = MaxHealth - ((StripShots * AR->DamagePerShot) - MaxShields);
			const float StripTime = TimeToLandShots(StripShots, AR->GetMinShotIntervalSeconds());

			const int32 FinishBodyShots = ShotsToDeplete(HealthRemaining, Magnum->DamagePerShot);
			const float BodyLineTTK = StripTime + Magnum->EquipTime_s + TimeToLandShots(FinishBodyShots, Magnum->GetMinShotIntervalSeconds());
			const float ARSoloTTK = TimeToLandShots(ShotsToDeplete(MaxHealth + MaxShields, AR->DamagePerShot), AR->GetMinShotIntervalSeconds());

			TestEqual(TEXT("five 22-damage body shots finish 96 health"), FinishBodyShots, 5);
			TestEqual(TEXT("the body-shot line is 2.9333 s"), BodyLineTTK, 1.2f + 0.4f + 4.f * (60.f / 180.f), Tol);

			TestTrue(TEXT("R1/R3: swapping for a BODY-shot finish is slower than just holding the AR trigger"), BodyLineTTK > ARSoloTTK);
		});

		It("R4: the Rocket needs its whole 2-round magazine to kill, and cannot reload", [this]()
		{
			if (!EnsureTables()) { return; }
			const FBRWeaponRow* Rocket = Weapon(RowRocket);
			if (!Rocket) { return; }

			float MaxHealth = 0.f;
			float MaxShields = 0.f;
			if (!Curve(BRCombatCurves::Names::FighterMaxHealth, MaxHealth)) { return; }
			if (!Curve(BRCombatCurves::Names::FighterMaxShields, MaxShields)) { return; }
			const float EffectiveHealth = MaxHealth + MaxShields;

			const int32 DirectHits = ShotsToDeplete(EffectiveHealth, Rocket->DamagePerShot);
			TestEqual(TEXT("R4: two 120-damage direct hits kill a 200 EHP fighter"), DirectHits, 2);
			TestTrue(TEXT("R4: one does not (120 < 200)"), Rocket->DamagePerShot < EffectiveHealth);
			TestEqual(TEXT("R4: two rounds IS the magazine -- the kill costs the whole weapon"), DirectHits, Rocket->MagSize);
			TestEqual(TEXT("R4: and there is nothing to reload from"), Rocket->GetStartingReserveAmmo(), 0);
			TestEqual(TEXT("R4: the two-direct-hit TTK is 2.000 s at 30 RPM"), TimeToLandShots(DirectHits, Rocket->GetMinShotIntervalSeconds()), 2.f, Tol);

			TestTrue(TEXT("R4: direct 120 + centre splash 90 exceeds 200 EHP"), (Rocket->DamagePerShot + Rocket->SplashDamage) > EffectiveHealth);
		});
	});

	Describe("UBRAttributeSet clamps", [this]()
	{
		It("clamps Health and Shields against an UNINITIALISED capacity of zero", [this]()
		{
			TStrongObjectPtr<UBRAttributeSet> Set(NewObject<UBRAttributeSet>());

			float ProposedHealth = 100.f;
			Set->PreAttributeChange(UBRAttributeSet::GetHealthAttribute(), ProposedHealth);
			TestEqual(TEXT("Health proposed at 100 clamps to 0 while MaxHealth is 0 (uninitialised)"), ProposedHealth, 0.f, Tol);

			float ProposedShields = 100.f;
			Set->PreAttributeChange(UBRAttributeSet::GetShieldsAttribute(), ProposedShields);
			TestEqual(TEXT("Shields proposed at 100 clamps to 0 while MaxShields is 0 (uninitialised)"), ProposedShields, 0.f, Tol);

			float NegativeHealth = -50.f;
			Set->PreAttributeChange(UBRAttributeSet::GetHealthAttribute(), NegativeHealth);
			TestEqual(TEXT("Health never goes below zero"), NegativeHealth, 0.f, Tol);
		});

		It("floors a negative capacity at zero, so no clamp can ever invert", [this]()
		{
			TStrongObjectPtr<UBRAttributeSet> Set(NewObject<UBRAttributeSet>());

			float NegativeMaxHealth = -10.f;
			Set->PreAttributeChange(UBRAttributeSet::GetMaxHealthAttribute(), NegativeMaxHealth);
			TestEqual(TEXT("a negative MaxHealth floors at 0"), NegativeMaxHealth, 0.f, Tol);

			float NegativeMaxShields = -10.f;
			Set->PreAttributeChange(UBRAttributeSet::GetMaxShieldsAttribute(), NegativeMaxShields);
			TestEqual(TEXT("a negative MaxShields floors at 0"), NegativeMaxShields, 0.f, Tol);

			float PositiveMaxHealth = 100.f;
			Set->PreAttributeChange(UBRAttributeSet::GetMaxHealthAttribute(), PositiveMaxHealth);
			TestEqual(TEXT("a positive MaxHealth is not clamped"), PositiveMaxHealth, 100.f, Tol);
		});
	});
}

#endif
