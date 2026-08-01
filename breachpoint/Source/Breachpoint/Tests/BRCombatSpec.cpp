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

/**
 * ===========================================================================================
 * Breachpoint.Sim.Combat  --  the golden suite for the combat sandbox
 * ===========================================================================================
 *
 * OWNER: sim-builder (ticket BP02, step 5). ONE spec file per feature packet, taken by exact
 * path (ruling R25). The SUITE NAME is a property of each TEST, not of the file, so BP03's
 * `BRWeaponFireSpec.cpp` and BP05's `BRTriangleSpec.cpp` register under this same
 * `Breachpoint.Sim.Combat` prefix without colliding with anything here.
 *
 * WHY THIS FILE EXISTS. BP02's Log names the sharpest gap in the packet in its own words:
 * "THESE RULES LANDED WITH ZERO PINNED SPECS ... a TTK-bearing formula with no golden suite."
 * Every assertion below is an attempt to close that, and every one of them can FAIL --
 * a spec that asserts nothing so a rung can go green is a finding, not a deliverable (R25).
 *
 * -------------------------------------------------------------------------------------------
 * WHAT IT IS ALLOWED TO TOUCH, and why that is the whole design
 *
 * Headless, deterministic, no world, no ASC, no PIE, no engine subsystem beyond the object
 * system. Nothing here reads wall-clock time, a frame delta, or a random number. Two machines
 * with the same two CSVs produce the same verdicts, which is the property that lets a pinned
 * number be evidence rather than an anecdote.
 *
 * It reaches the real rules through the two seams the sim was factored to expose:
 *   - `UBRDamageExecCalc::ComputeFinalDamage(Base, Tags, CurveLookup, OutMissing)` -- the pure
 *     static half of THE damage rule. No ASC, no GameplayEffectSpec, no content.
 *   - `BRCombatCurves::SetTableOverrideForTests(Table)` -- points the sim's ONE curve reader at
 *     a table this suite builds in memory from the shipped `Content/Data/CT_Combat.csv`.
 *
 * ...and it reads the real data by importing the shipped CSVs through the REAL schema
 * (`FBRWeaponRow`) at runtime. Deliberately not via `/Game/Data/DT_Weapons` or
 * `/Game/Data/CT_Combat`: neither `.uasset` exists yet (BP02's Log, 1 Aug: "CT_Combat.csv has
 * not been imported to a CurveTable asset yet"), and a suite that pins TTK must not be
 * unrunnable until someone opens an editor. The CSV is the source of truth by law 3; this file
 * asserts against the source of truth directly.
 *
 * -------------------------------------------------------------------------------------------
 * WHAT IT CANNOT REACH -- stated here rather than faked with a spec-side re-implementation
 *
 *  1. `UBRAttributeSet::ApplyIncomingDamageShieldsFirst` is PRIVATE and runs only from
 *     `PostGameplayEffectExecute`, which needs a live `FGameplayEffectModCallbackData`, an ASC
 *     and an applied Instant effect. The shields-first SPLIT therefore has no headless entry
 *     point. Re-implementing "shields absorb, remainder overflows" in this file and asserting
 *     it against itself would be the hollow-spec failure R25 names, so it is NOT done. What is
 *     pinned instead is the split's OBSERVABLE CONSEQUENCE -- that a kill costs exactly
 *     MaxShields + MaxHealth damage, no more and no less, because the seam conserves damage --
 *     which is the property every TTK number below rests on.
 *     Filed as a contract_gap: the split wants the same factoring `ComputeFinalDamage` already
 *     has, e.g. a pure `static void SplitShieldsFirst(float Raw, float ShieldsBefore, float&
 *     OutAbsorbed, float& OutOverflow)`, and then it is four assertions.
 *  2. Attribute VALUES. Every writer of an attribute is a GameplayEffect (purity law 1) and the
 *     `Set*`/`Init*` accessors are exactly the direct-write the rung-2 grep gate hunts for. This
 *     suite therefore asserts `PreAttributeChange` only in the states it can reach without
 *     writing an attribute -- which is still enough to pin the clamp trap that forces
 *     `GE_InitStats`'s modifier order.
 *  3. Anything replicated, predicted, or timed. Those are rungs 3 and 4 and this file does not
 *     pretend otherwise.
 *
 * -------------------------------------------------------------------------------------------
 * READ THIS BEFORE MOVING A NUMBER. Every literal in the assertions below is a PIN: it is the
 * value the shipped CSVs and the closed design rulings produce today. A balance change is a CSV
 * diff, and a CSV diff that moves TTK turns this suite RED -- on purpose. The correct response
 * is to move the pin in the SAME packet, loudly, with the reason in the ticket's Log. Deleting
 * an assertion to make a run green is the one edit this file forbids.
 */

namespace BRCombatSpecInternal
{
	/** Row handles of `Content/Data/DT_Weapons.csv`. The rest of the game addresses weapons by these. */
	static const FName RowAR(TEXT("AR"));
	static const FName RowMagnum(TEXT("Magnum"));
	static const FName RowRocket(TEXT("Rocket"));

	static const TCHAR* SpecContext = TEXT("BRCombatSpec");

	/**
	 * Shots needed to remove `Pool` points at `DamagePerShot` each.
	 *
	 * Plain ceiling division, NOT a re-implementation of a sim rule: the sim has no
	 * shots-to-kill function, and this is the arithmetic a designer does on paper. The PINS are
	 * the integers it returns, which come from the CSV.
	 *
	 * A non-positive DamagePerShot returns -1 -- an explicit "unanswerable", never a silent
	 * INT_MAX or a divide. No test below is allowed to reach it, and one asserts that.
	 */
	static int32 ShotsToDeplete(float Pool, float DamagePerShot)
	{
		if (DamagePerShot <= 0.f)
		{
			return -1;
		}
		return FMath::CeilToInt(Pool / DamagePerShot);
	}

	/**
	 * Seconds from the FIRST shot landing to the Nth shot landing, at a fixed cadence.
	 *
	 * TTK is measured shot-to-shot, so N shots cost (N-1) intervals: a one-shot kill has a TTK
	 * of zero and a two-shot kill costs exactly one interval. Stating that convention here is
	 * what makes the pinned seconds below comparable to each other and to the GDD.
	 */
	static float TimeToLandShots(int32 ShotCount, float ShotIntervalSeconds)
	{
		return (ShotCount <= 1) ? 0.f : (ShotCount - 1) * ShotIntervalSeconds;
	}

	/** Build a spec's damage tag container from a plain list. GAS holds type + modifiers at once (R22). */
	static FGameplayTagContainer MakeDamageTags(const TArray<FGameplayTag>& Tags)
	{
		FGameplayTagContainer Container;
		for (const FGameplayTag& Tag : Tags)
		{
			Container.AddTag(Tag);
		}
		return Container;
	}

	/**
	 * A curve lookup whose coefficients the TEST states, for the property-style cases where the
	 * point is the ALGEBRA and not the content. `ComputeFinalDamage` takes the lookup as a
	 * parameter precisely so this is possible without an asset.
	 */
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

	/** Tolerance for every float comparison in this file. Tight enough to catch a real change. */
	static constexpr float Tol = 1.e-4f;
}

BEGIN_DEFINE_SPEC(FBRCombatSpec, "Breachpoint.Sim.Combat",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

	/** DT_Weapons.csv imported through FBRWeaponRow. Strong, because GC does not know about specs. */
	TStrongObjectPtr<UDataTable> WeaponTable;

	/** CT_Combat.csv as a live CurveTable, handed to the sim through its own test seam. */
	TStrongObjectPtr<UCurveTable> CombatTable;

	/** Load both tables and point `BRCombatCurves` at ours. Returns false having reported why. */
	bool EnsureTables();

	/** Row or nullptr, having reported the miss. Never returns a default-constructed row. */
	const FBRWeaponRow* Weapon(FName RowName);

	/** `BRCombatCurves::Evaluate` at input 0, reported and defaulted to NaN-free 0 on a miss. */
	bool Curve(FName CurveName, float& OutValue);

END_DEFINE_SPEC(FBRCombatSpec)

bool FBRCombatSpec::EnsureTables()
{
	if (WeaponTable.IsValid() && CombatTable.IsValid())
	{
		// The override is re-asserted every time: AfterEach clears it so no other suite in the
		// process inherits our in-memory table.
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
		// A WARNING, not an error, and the distinction is deliberate: the importer complains
		// about columns this suite does not pin (it is not the schema's owner), and the value
		// assertions below are what decide whether the import was good enough to trust.
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

	const FBRWeaponRow* Row = WeaponTable->FindRow<FBRWeaponRow>(RowName, BRCombatSpecInternal::SpecContext, /*bWarnIfRowMissing=*/false);
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
		// Never leave the sim pointing at a table this suite owns. `SetTableOverrideForTests`
		// also resets the reader's "already reported this missing curve" set, so a later suite
		// gets its own warnings rather than our silence.
		BRCombatCurves::SetTableOverrideForTests(nullptr);
	});

	// =======================================================================================
	// FBRWeaponRow's pure accessors. The cheapest true assertions available: unit conversion
	// and algebra, no world, no GAS. If these are wrong, every number above them is wrong.
	// =======================================================================================

	Describe("FBRWeaponRow row algebra", [this]()
	{
		It("converts RPM into the shortest legal interval between two shots, per shipped row", [this]()
		{
			if (!EnsureTables()) { return; }
			const FBRWeaponRow* AR = Weapon(RowAR);
			const FBRWeaponRow* Magnum = Weapon(RowMagnum);
			const FBRWeaponRow* Rocket = Weapon(RowRocket);
			if (!AR || !Magnum || !Rocket) { return; }

			// 600 / 180 / 30 RPM. These are the server's rate gate, so a change here changes
			// what a client is allowed to send, not only how fast the gun feels.
			TestEqual(TEXT("AR: 600 RPM is one shot every 0.100 s"), AR->GetMinShotIntervalSeconds(), 0.1f, Tol);
			TestEqual(TEXT("Magnum: 180 RPM is one shot every 0.3333 s"), Magnum->GetMinShotIntervalSeconds(), 60.f / 180.f, Tol);
			TestEqual(TEXT("Rocket: 30 RPM is one shot every 2.000 s"), Rocket->GetMinShotIntervalSeconds(), 2.f, Tol);
		});

		It("refuses to express a rate gate for a row with no RPM, rather than reporting 'unlimited'", [this]()
		{
			// The documented edge case, pinned so nobody 'fixes' it into a permissive default.
			// A default-constructed row carries RPM 0; the accessor returns 0 seconds, which the
			// server's rate check must read as REFUSE-TO-FIRE. Zero is the only answer that is
			// safe to misread in one direction and never in the other.
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

			// R4 in one number: the Rocket is balanced by SCARCITY. ReserveMags 0 makes reload
			// intentionally unreachable, so two shots is the entire weapon. A non-zero here is
			// not a buff, it is a different weapon.
			TestEqual(TEXT("R4: the Rocket carries ZERO reserve -- reload is intentionally unreachable"), Rocket->GetStartingReserveAmmo(), 0);
			TestEqual(TEXT("R4: ...and its magazine is 2"), Rocket->MagSize, 2);
		});

		It("converts designer metres into Unreal centimetres at the accessor, never at the call site", [this]()
		{
			if (!EnsureTables()) { return; }
			const FBRWeaponRow* AR = Weapon(RowAR);
			const FBRWeaponRow* Rocket = Weapon(RowRocket);
			if (!AR || !Rocket) { return; }

			// The table speaks metres and seconds; Unreal speaks centimetres. The suffix in the
			// column name IS the unit contract, and these accessors are the one place it is spent.
			TestEqual(TEXT("Rocket: a 4 m splash radius is 400 uu"), Rocket->GetSplashRadiusCm(), 400.f, Tol);
			TestEqual(TEXT("Rocket: 30 m/s of projectile is 3000 uu/s"), Rocket->GetProjectileSpeedCmPerSecond(), 3000.f, Tol);

			// Hitscan rows carry zero on both, and zero converts to zero -- the invariant that
			// keeps a bare *100 from ever looking harmless.
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

			// A headshot that HURTS LESS than a body shot is never a design position. This is the
			// floor R2 and R4 sit exactly on: 1.0 is legal, 0.9 is a typo.
			{
				FBRWeaponRow Row = *AR;
				Row.HeadshotMult = 0.9f;
				TestFalse(TEXT("HeadshotMult below 1.0 is refused"), Row.ValidateSchema(Error));
			}
			// The delivery/speed invariant, both directions -- the one the verifier forced.
			{
				FBRWeaponRow Row = *AR;
				Row.ProjectileSpeed = 30.f; // still Hitscan
				TestFalse(TEXT("a hitscan row carrying a projectile speed is refused"), Row.ValidateSchema(Error));
			}
			{
				FBRWeaponRow Row = *AR;
				Row.DamageDelivery = EBRDamageDelivery::Projectile; // still speed 0
				TestFalse(TEXT("a projectile row carrying no projectile speed is refused"), Row.ValidateSchema(Error));
			}
			// No rate ceiling means the server's rate gate has nothing to compare against.
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
			// Splash is a PAIR. Half of it is a rocket that explodes for nothing, or damages
			// everything at radius zero -- both silently, which is the point of the check.
			{
				FBRWeaponRow Row = *AR;
				Row.SplashRadius_m = 4.f; // no SplashDamage
				TestFalse(TEXT("a splash radius with no splash damage is refused"), Row.ValidateSchema(Error));
			}
			{
				FBRWeaponRow Row = *AR;
				Row.DamagePerShot = -1.f;
				TestFalse(TEXT("negative damage in a row is refused -- it would HEAL the target"), Row.ValidateSchema(Error));
			}

			// And the refusal must NAME itself, or a red import is a mystery.
			TestTrue(TEXT("ValidateSchema writes a human-readable reason when it refuses"), !Error.IsEmpty());
		});
	});

	// =======================================================================================
	// Headshot math. Two of the three weapons carry 1.0 as a DESIGN POSITION (R2, R4), which is
	// exactly the kind of number a well-meaning balance pass "fixes".
	// =======================================================================================

	Describe("Headshot multipliers", [this]()
	{
		It("R2: the AR's HeadshotMult is 1.0 -- it is the shield-stripper, not a finisher", [this]()
		{
			if (!EnsureTables()) { return; }
			const FBRWeaponRow* AR = Weapon(RowAR);
			if (!AR) { return; }

			// R2 is CLOSED: "its headshot multiplier stays 1.0 -- headshot bonuses belong to
			// precision weapons." A spec asserting otherwise would be a defect in the spec.
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

			// The defect this ruling was written from, pinned in BOTH directions so the guard
			// cannot rot into a tautology: at 1.0 a direct headshot leaves the target alive; at
			// the 2.0 someone would "obviously" want, it one-shots a full-EHP fighter.
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

			// THE DOUBLE-COUNT GUARD, and the reason this test is worth more than it looks.
			// There are TWO places a headshot could be multiplied: `FBRWeaponRow::HeadshotMult`
			// (per weapon, folded into SetByCaller.BaseDamage by the server-side fire path) and
			// the `Damage.Headshot.Multiplier` curve the ExecCalc composes for every hit.
			// Shipping both LIVE multiplies them -- and would silently buff the two weapons whose
			// 1.0 is a design position (R2, R4). CT_Combat therefore ships the global curve at
			// identity: the modifier axis exists and is switched OFF.
			// Raise this to 2.0 and this test goes red BEFORE a playtest discovers it.
			float GlobalHeadshot = 0.f;
			const FName HeadshotCurve = UBRDamageExecCalc::MakeMultiplierCurveName(BRGameplayTags::Damage_Headshot.GetTag());
			if (!Curve(HeadshotCurve, GlobalHeadshot)) { return; }

			TestEqual(TEXT("CT_Combat's Damage.Headshot.Multiplier is the identity 1.0"), GlobalHeadshot, 1.f, Tol);

			// Proven through the real rule, not by reading the CSV: the same base survives the
			// pipeline whether or not the hit was stamped as a headshot.
			const FGameplayTagContainer Body = MakeDamageTags({ BRGameplayTags::Damage_Kinetic.GetTag() });
			const FGameplayTagContainer Head = MakeDamageTags({ BRGameplayTags::Damage_Kinetic.GetTag(), BRGameplayTags::Damage_Headshot.GetTag() });
			auto RealLookup = [](FName Name, float& Out) { return BRCombatCurves::Evaluate(Name, Out); };

			const float BodyDamage = UBRDamageExecCalc::ComputeFinalDamage(22.f, Body, RealLookup);
			const float HeadDamage = UBRDamageExecCalc::ComputeFinalDamage(22.f, Head, RealLookup);
			TestEqual(TEXT("the exec calc adds NO second headshot bonus of its own"), HeadDamage, BodyDamage, Tol);
		});
	});

	// =======================================================================================
	// CT_Combat -- the coefficients the one damage rule reads. R5's pillars live here.
	// =======================================================================================

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

			// EVERY TTK below is arithmetic on this one number. It is asserted here, once, so a
			// change to it fails in a place that says what it means instead of in seven places
			// that say "expected 25, got 30".
			TestEqual(TEXT("effective health is 200, and every TTK in this file rests on it"), MaxHealth + MaxShields, 200.f, Tol);
		});

		It("pins R5's shield-recharge pillars: 60 points per second after a 2.5 s delay", [this]()
		{
			if (!EnsureTables()) { return; }

			float Rate = 0.f;
			float Delay = 0.f;
			if (!Curve(BRCombatCurves::Names::ShieldsRegenRatePerSecond, Rate)) { return; }
			if (!Curve(BRCombatCurves::Names::ShieldsRegenDelaySeconds, Delay)) { return; }

			// R5: "Shields-first, no health regen, 2.5 s recharge delay are PILLARS, not
			// tunables. Proposals touching them go to the founder, not through the pipeline."
			// This pin is what makes a quiet CSV edit to 1.5 loud.
			TestEqual(TEXT("R5: the shield recharge delay is 2.5 s"), Delay, 2.5f, Tol);
			TestEqual(TEXT("shields recharge at 60 points/second"), Rate, 60.f, Tol);

			// Rate and period are SEPARATE questions: the applier reads points-per-second and the
			// tick period independently and multiplies, so changing the granularity must not move
			// the rate per second. That is what makes the period a fidelity knob, not balance.
			//
			// 0.25 is pinned here as the value that SHIPS, and it is the one CT_Combat entry BP02's
			// Log records as having no source -- tuning-curator's to confirm. A red here therefore
			// means "the curator moved it": move the pin in the same packet, do not delete it.
			float Period = 0.f;
			if (!Curve(BRCombatCurves::Names::ShieldsRegenPeriodSeconds, Period)) { return; }
			TestTrue(TEXT("the regen period is positive -- a zero period is not a rule, it is a hang"), Period > 0.f);
			TestTrue(TEXT("the regen period is shorter than the gate, or the first tick lands late"), Period < Delay);
			TestEqual(TEXT("the shipped regen period is 0.25 s (unsourced; curator owns it)"), Period, 0.25f, Tol);
		});

		It("ships a multiplier curve for every one of the five Damage.* tags", [this]()
		{
			if (!EnsureTables()) { return; }

			// The ExecCalc treats a missing curve as the identity 1.0 and LOGS it -- a decision,
			// not a fallthrough, because refusing the hit would turn a missing CSV row into an
			// invulnerable player. That decision is only safe while the reachable path is the one
			// the CSV states. This test is what keeps it reachable.
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

			// All five ship at 1.0, so a hit carrying every tag at once is still its base. That is
			// not a coincidence to be tidied away: it is the shipped balance, stated.
			TestEqual(TEXT("all five multipliers are 1.0 today, so the product is the base"), Final, 100.f, Tol);
		});
	});

	// =======================================================================================
	// UBRDamageExecCalc::ComputeFinalDamage -- THE damage rule, as a pure function.
	// =======================================================================================

	Describe("UBRDamageExecCalc", [this]()
	{
		It("derives the curve name from the tag, with no switch and no tag-to-curve map", [this]()
		{
			// The property that makes a new damage modifier a tag plus a CSV row and ZERO lines
			// of code. If someone adds a mapping table, this still passes -- but the names below
			// are what the CSV must spell, so a rename is caught either way.
			TestTrue(TEXT("Damage.Headshot -> Damage.Headshot.Multiplier"),
				UBRDamageExecCalc::MakeMultiplierCurveName(BRGameplayTags::Damage_Headshot.GetTag()) == FName(TEXT("Damage.Headshot.Multiplier")));
			TestTrue(TEXT("Damage.Rear -> Damage.Rear.Multiplier"),
				UBRDamageExecCalc::MakeMultiplierCurveName(BRGameplayTags::Damage_Rear.GetTag()) == FName(TEXT("Damage.Rear.Multiplier")));
			TestTrue(TEXT("Damage.Explosive -> Damage.Explosive.Multiplier"),
				UBRDamageExecCalc::MakeMultiplierCurveName(BRGameplayTags::Damage_Explosive.GetTag()) == FName(TEXT("Damage.Explosive.Multiplier")));
		});

		It("R22: Damage.* is FLAT -- there is no Damage.Melee.Rear, and Rear's parent is Damage", [this]()
		{
			// R22 is CLOSED. `Damage.Melee[.Rear]` in the older documents is SHORTHAND for the
			// PAIR {Damage.Melee, Damage.Rear}, not a nested tag. A builder grepping for
			// `Damage.Melee.Rear` correctly finds nothing, and this is the test that says so out
			// loud instead of leaving it to be rediscovered.
			const FGameplayTag Nested = FGameplayTag::RequestGameplayTag(FName(TEXT("Damage.Melee.Rear")), /*ErrorIfNotFound=*/false);
			TestFalse(TEXT("R22: `Damage.Melee.Rear` is not a registered tag and must never become one"), Nested.IsValid());

			// Types and modifiers are SIBLINGS: both hang directly off `Damage`. That is what
			// lets the ExecCalc derive its family root from any one leaf.
			const FGameplayTag TypeParent = BRGameplayTags::Damage_Melee.GetTag().RequestDirectParent();
			const FGameplayTag ModifierParent = BRGameplayTags::Damage_Rear.GetTag().RequestDirectParent();
			TestTrue(TEXT("R22: Damage.Melee (a type) and Damage.Rear (a modifier) share one parent"), TypeParent == ModifierParent);
			TestTrue(TEXT("R22: ...and that parent is `Damage`"), TypeParent == FGameplayTag::RequestGameplayTag(FName(TEXT("Damage")), false));

			// And the pair composes: both curves are consulted, neither is nested inside the other.
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
			// The spec carries whatever else the caller stamped on it -- ability tags, state tags,
			// cue tags. None of those are coefficients. A lookup that WOULD answer for a State.*
			// curve proves the filter is the family test and not the lookup's silence.
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
			// The documented decision: the identity is the only value that cannot change an
			// outcome, and the miss is reported rather than swallowed. Refusing the hit would
			// make one missing CSV row an invulnerable player -- a worse failure, and a harder
			// one to diagnose. Pinned so "make it fail closed" is a ruling, not a patch.
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
			// Negative damage is NOT healing. Healing is a Health modifier with its own effect and
			// its own tags. Allowing it here would make every multiplier a potential heal and a
			// negative cell in a CSV a potential exploit.
			FStatedCurves Stated;
			Stated.Set(BRGameplayTags::Damage_Kinetic.GetTag(), 2.f);

			const FGameplayTagContainer Kinetic = MakeDamageTags({ BRGameplayTags::Damage_Kinetic.GetTag() });
			auto Lookup = [&Stated](FName Name, float& Out) { return Stated.Lookup(Name, Out); };

			TestEqual(TEXT("a negative base magnitude yields 0, never a heal"), UBRDamageExecCalc::ComputeFinalDamage(-25.f, Kinetic, Lookup), 0.f, Tol);
			TestEqual(TEXT("a zero base magnitude yields 0"), UBRDamageExecCalc::ComputeFinalDamage(0.f, Kinetic, Lookup), 0.f, Tol);
		});

		It("never returns a negative number, whatever the CSV says", [this]()
		{
			// A negative product is reachable only from a negative number in a CSV cell. The rule
			// clamps at its own exit rather than letting the attribute set log an impossible
			// input -- and this is the invariant that keeps "a typo'd cell" from being a healing
			// gun. Property-style, over a stated grid: no randomness, so a failure is reproducible.
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
			// Determinism is law. Two machines that build the same tag set in different orders
			// must agree on the damage, or a disputed result is unreproducible by construction.
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
			// The project's analogue of "more armour never increases damage taken": adding a
			// modifier tag whose coefficient is at least 1.0 can only ever raise or hold the
			// number. A modifier that quietly subtracted would be invisible in every playtest.
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

			// The fire path folds the weapon's HeadshotMult into SetByCaller.BaseDamage on the
			// SERVER, after it has validated the hit bone; the exec calc then applies the global
			// Damage.* curves. These are the end-to-end numbers of that composition.
			TestEqual(TEXT("AR body hit: 8"), UBRDamageExecCalc::ComputeFinalDamage(AR->DamagePerShot, Kinetic, RealLookup), 8.f, Tol);
			TestEqual(TEXT("AR head hit: 8 (R2 -- no bonus)"), UBRDamageExecCalc::ComputeFinalDamage(AR->DamagePerShot * AR->HeadshotMult, KineticHead, RealLookup), 8.f, Tol);
			TestEqual(TEXT("Magnum body hit: 22"), UBRDamageExecCalc::ComputeFinalDamage(Magnum->DamagePerShot, Kinetic, RealLookup), 22.f, Tol);
			TestEqual(TEXT("Magnum head hit: 44 (R1 -- the bonus lives on precision weapons)"), UBRDamageExecCalc::ComputeFinalDamage(Magnum->DamagePerShot * Magnum->HeadshotMult, KineticHead, RealLookup), 44.f, Tol);
			TestEqual(TEXT("Rocket direct hit: 120"), UBRDamageExecCalc::ComputeFinalDamage(Rocket->DamagePerShot, Explosive, RealLookup), 120.f, Tol);
			TestEqual(TEXT("Rocket splash at centre: 90"), UBRDamageExecCalc::ComputeFinalDamage(Rocket->SplashDamage, Explosive, RealLookup), 90.f, Tol);
		});
	});

	// =======================================================================================
	// Time-to-kill. The rulings' actual subject matter.
	//
	// CONVENTION, stated once: TTK is measured from the first shot LANDING to the killing shot
	// LANDING, at the weapon's minimum legal cadence, against a full 200 EHP fighter, with no
	// shield recharge (the 2.5 s gate exceeds every window below). Travel time is excluded for
	// the hitscan weapons because there is none, and for the Rocket because range is a map fact,
	// not a weapon fact -- named so nobody reads the Rocket's 2.0 s as a range-independent claim.
	//
	// Every one of these rests on the shields-first split CONSERVING damage: absorbed + overflow
	// == raw, exactly, for every hit. That is the invariant this suite cannot reach directly
	// (see the file header) and the one a rung-3/4 test owes.
	// =======================================================================================

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

			// R1 is CLOSED: "The Magnum's all-headshot TTK beating every other path is the
			// intended fantasy, paid for by an 8-round mag and no forgiveness on a miss."
			const int32 MagnumHeadshots = ShotsToDeplete(EffectiveHealth, Magnum->DamagePerShot * Magnum->HeadshotMult);
			const float MagnumHeadshotTTK = TimeToLandShots(MagnumHeadshots, Magnum->GetMinShotIntervalSeconds());

			TestEqual(TEXT("R1: five 44-damage headshots kill a 200 EHP fighter"), MagnumHeadshots, 5);
			TestTrue(TEXT("R1: ...and four do not -- the fifth is the kill, not a rounding artefact"), (4 * Magnum->DamagePerShot * Magnum->HeadshotMult) < EffectiveHealth);
			TestTrue(TEXT("R1: five shots fit in the 8-round magazine, with three misses' forgiveness"), MagnumHeadshots <= Magnum->MagSize);
			TestEqual(TEXT("R1: the Magnum's all-headshot TTK is 1.3333 s"), MagnumHeadshotTTK, 4.f * (60.f / 180.f), Tol);

			// "Beating every other path", pinned as an ORDERING rather than as one number, because
			// the ordering is the design claim and any of the four numbers moving is worth a look.
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

			// R2 is CLOSED: "Slower solo TTK is INTENDED." This pin is not a complaint about 2.4 s,
			// it is a tripwire for anyone who shortens it without a ruling.
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

			// R3 is CLOSED and states this as a REQUIREMENT, not an observation: the Magnum "is
			// NOT required to solo a full-200-EHP target from one mag on body shots". So a data
			// change that made it possible would be a design change, and this is where it surfaces.
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

			// Step 1 -- strip the shields with the AR. 13 shots deal 104 into a 100-point shield,
			// and the 4-point remainder OVERFLOWS to health. That overflow is not a detail: it is
			// the shields-first seam conserving damage, and dropping it would change this TTK.
			const int32 StripShots = ShotsToDeplete(MaxShields, AR->DamagePerShot);
			const float Overkill = (StripShots * AR->DamagePerShot) - MaxShields;
			const float HealthRemaining = MaxHealth - Overkill;

			TestEqual(TEXT("R3: 13 AR shots strip a 100-point shield"), StripShots, 13);
			TestEqual(TEXT("R3: the 13th shot overflows 4 damage into health -- no damage is lost at the seam"), Overkill, 4.f, Tol);
			TestEqual(TEXT("R3: 96 health is left for the finisher"), HealthRemaining, 96.f, Tol);

			const float StripTime = TimeToLandShots(StripShots, AR->GetMinShotIntervalSeconds());
			TestEqual(TEXT("R3: the strip takes 1.200 s"), StripTime, 1.2f, Tol);

			// Step 2 -- the swap. R3 names "0.4 s" explicitly and the number lives in the row of
			// the weapon being BROUGHT UP, which is the Magnum.
			TestEqual(TEXT("R3: the swap to the Magnum costs 0.400 s, exactly as the ruling names"), Magnum->EquipTime_s, 0.4f, Tol);

			// Step 3 -- the finish, headshots, which is what makes the Magnum "the finisher".
			const int32 FinishHeadshots = ShotsToDeplete(HealthRemaining, Magnum->DamagePerShot * Magnum->HeadshotMult);
			const float FinishTime = TimeToLandShots(FinishHeadshots, Magnum->GetMinShotIntervalSeconds());
			TestEqual(TEXT("R3: three 44-damage headshots finish 96 health"), FinishHeadshots, 3);
			TestTrue(TEXT("R3: three shots fit in the 8-round magazine"), FinishHeadshots <= Magnum->MagSize);

			const float LineTTK = StripTime + Magnum->EquipTime_s + FinishTime;
			TestEqual(TEXT("R3: the whole intended line is 2.2667 s"), LineTTK, 1.2f + 0.4f + 2.f * (60.f / 180.f), Tol);

			// And the line has to be WORTH walking, or it is not the intended line -- it is a
			// trap. It must beat simply holding the AR trigger.
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

			// This is a DESIGN CONSEQUENCE, pinned rather than reported as a bug: R1 says
			// precision is what the Magnum is paid for, so a player who cannot land heads is
			// correctly better off never swapping. If this ever flips, the swap has become free
			// and R1/R3's economy has changed without anyone deciding to change it.
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

			// R4 is CLOSED: "balanced by scarcity, not by damage ... It is SUPPOSED to win the
			// fight it is present for." So the pin is not "is it strong" but "is it scarce":
			// two rounds, no reserve, and the kill costs both of them on a clean pair of hits.
			const int32 DirectHits = ShotsToDeplete(EffectiveHealth, Rocket->DamagePerShot);
			TestEqual(TEXT("R4: two 120-damage direct hits kill a 200 EHP fighter"), DirectHits, 2);
			TestTrue(TEXT("R4: one does not (120 < 200)"), Rocket->DamagePerShot < EffectiveHealth);
			TestEqual(TEXT("R4: two rounds IS the magazine -- the kill costs the whole weapon"), DirectHits, Rocket->MagSize);
			TestEqual(TEXT("R4: and there is nothing to reload from"), Rocket->GetStartingReserveAmmo(), 0);
			TestEqual(TEXT("R4: the two-direct-hit TTK is 2.000 s at 30 RPM"), TimeToLandShots(DirectHits, Rocket->GetMinShotIntervalSeconds()), 2.f, Tol);

			// A direct hit plus its own centre-of-blast splash exceeds 200 -- which is what makes
			// a single well-placed rocket lethal in practice. The RADIAL half (falloff, the
			// overlap query) belongs to BP05/BP09 and is deliberately not asserted here; only the
			// arithmetic that is a property of this row is.
			TestTrue(TEXT("R4: direct 120 + centre splash 90 exceeds 200 EHP"), (Rocket->DamagePerShot + Rocket->SplashDamage) > EffectiveHealth);
		});
	});

	// =======================================================================================
	// UBRAttributeSet -- only the clamps a headless spec can honestly reach.
	// See the file header for what it cannot, and why that is filed rather than faked.
	// =======================================================================================

	Describe("UBRAttributeSet clamps", [this]()
	{
		It("clamps Health and Shields against an UNINITIALISED capacity of zero", [this]()
		{
			// THE TRAP THAT FORCES GE_InitStats' MODIFIER ORDER, pinned in the only state a spec
			// can build without writing an attribute (which would be the direct write purity law 1
			// forbids and the rung-2 grep gate hunts for).
			//
			// PreAttributeChange clamps Health against GetMaxHealth() *as it currently is*. On a
			// fresh set that is zero, so ANY proposed health clamps to zero. That is why
			// GE_InitStats must set MaxHealth/MaxShields BEFORE Health/Shields -- set them the
			// other way round and a fighter spawns dead from a correct table.
			//
			// The alternative (skip the clamp while Max is zero) was considered and REJECTED in
			// the attribute set itself, because it would leave "Health <= MaxHealth" only
			// conditionally true. If someone implements that alternative, this test goes red --
			// which is the conversation this pin exists to force.
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
			// A negative MaxHealth would make every Clamp above run with Min > Max. Flooring the
			// capacity is what keeps that unreachable instead of undefined.
			TStrongObjectPtr<UBRAttributeSet> Set(NewObject<UBRAttributeSet>());

			float NegativeMaxHealth = -10.f;
			Set->PreAttributeChange(UBRAttributeSet::GetMaxHealthAttribute(), NegativeMaxHealth);
			TestEqual(TEXT("a negative MaxHealth floors at 0"), NegativeMaxHealth, 0.f, Tol);

			float NegativeMaxShields = -10.f;
			Set->PreAttributeChange(UBRAttributeSet::GetMaxShieldsAttribute(), NegativeMaxShields);
			TestEqual(TEXT("a negative MaxShields floors at 0"), NegativeMaxShields, 0.f, Tol);

			// A positive capacity passes through untouched -- otherwise the two assertions above
			// would also pass on a PreAttributeChange that simply zeroed everything.
			float PositiveMaxHealth = 100.f;
			Set->PreAttributeChange(UBRAttributeSet::GetMaxHealthAttribute(), PositiveMaxHealth);
			TestEqual(TEXT("a positive MaxHealth is not clamped"), PositiveMaxHealth, 100.f, Tol);
		});
	});
}

#endif // WITH_AUTOMATION_TESTS
