#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Core/AIBTypes.h"
#include "Data/AIBTiers.h"

/**
 * PHASE 8, proven headless. What each pin defends: the four canonical tiers resolve and
 * an unknown name honestly does not (1); the ladder never inverts and every rung
 * changes something — "observably distinct" as arithmetic before it is ever eyes-on
 * (2); the claims gate splits the ladder exactly where the design says (3); every
 * shipped row validates clean (4); the validator itself is alive — a doctored row
 * names its defects (5); and the envelope anchors hold, so no tier can re-open the
 * inert-band defect by shrinking sight under the module's constants (6).
 */
BEGIN_DEFINE_SPEC(FAIBTiersSpec, "AIBot.Sim.Tiers",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

	TArray<FName> Names;

	static int32 Rank(EAIBCompetence Level) { return static_cast<int32>(Level); }

	static void SkillVector(const FAIBTierRow& Row, TArray<int32>& Out)
	{
		Out = { Rank(Row.Movement), Rank(Row.Aim), Rank(Row.Grenade),
			Rank(Row.Melee), Rank(Row.Confidence), Rank(Row.Teamwork) };
	}

END_DEFINE_SPEC(FAIBTiersSpec)

void FAIBTiersSpec::Define()
{
	BeforeEach([this]()
	{
		Names.Reset();
		AIBTiers::GetTierNames(Names);
	});

	It("resolves the canonical four and refuses an unknown name", [this]()
	{
		TestEqual(TEXT("four tiers"), Names.Num(), 4);
		for (const FName& Name : Names)
		{
			TestTrue(FString::Printf(TEXT("%s resolves"), *Name.ToString()),
				AIBTiers::Find(Name) != nullptr);
		}
		TestTrue(TEXT("an unknown name is null, not a guess"),
			AIBTiers::Find(TEXT("SpartanII")) == nullptr);
	});

	It("climbs a real ladder — no skill ever decreases, and every rung changes something", [this]()
	{
		for (int32 Index = 1; Index < Names.Num(); ++Index)
		{
			const FAIBTierRow* Below = AIBTiers::Find(Names[Index - 1]);
			const FAIBTierRow* Above = AIBTiers::Find(Names[Index]);
			if (!TestTrue(TEXT("rows exist"), Below && Above))
			{
				return;
			}
			TArray<int32> Lo, Hi;
			SkillVector(*Below, Lo);
			SkillVector(*Above, Hi);
			bool bAnyStrict = false;
			for (int32 Skill = 0; Skill < Lo.Num(); ++Skill)
			{
				TestTrue(FString::Printf(TEXT("%s>=%s on skill %d"),
					*Names[Index].ToString(), *Names[Index - 1].ToString(), Skill),
					Hi[Skill] >= Lo[Skill]);
				bAnyStrict |= Hi[Skill] > Lo[Skill];
			}
			TestTrue(FString::Printf(TEXT("%s is DISTINCT from %s"),
				*Names[Index].ToString(), *Names[Index - 1].ToString()), bAnyStrict);
		}
	});

	It("splits the claims gate where the design says — two tiers call out, two do not", [this]()
	{
		TestTrue(TEXT("Recruit below the gate"),
			AIBTiers::Find(TEXT("Recruit"))->Teamwork < EAIBCompetence::Trained);
		TestTrue(TEXT("Marine below the gate"),
			AIBTiers::Find(TEXT("Marine"))->Teamwork < EAIBCompetence::Trained);
		TestTrue(TEXT("ODST on the board"),
			AIBTiers::Find(TEXT("ODST"))->Teamwork >= EAIBCompetence::Trained);
		TestTrue(TEXT("Spartan on the board"),
			AIBTiers::Find(TEXT("Spartan"))->Teamwork >= EAIBCompetence::Trained);
	});

	It("ships only clean rows — the shipped four produce zero validator warnings", [this]()
	{
		for (const FName& Name : Names)
		{
			const TArray<FString> Warnings = AIBTiers::ValidateRow(Name, *AIBTiers::Find(Name));
			for (const FString& Warning : Warnings)
			{
				AddError(Warning); // print the actual complaint, not just a count
			}
			TestEqual(FString::Printf(TEXT("%s validates clean"), *Name.ToString()),
				Warnings.Num(), 0);
		}
	});

	It("keeps the validator honest — a doctored row names each defect", [this]()
	{
		FAIBTierRow Doctored;
		Doctored.ReactionSecondsMin = 0.05f;              // under the F1 floor
		Doctored.ReactionSecondsMax = 0.04f;              // inverted draw
		Doctored.LoseSightRadius = 900.f;                 // inside sight AND the band anchor
		Doctored.MemoryFreshSeconds = 100.f;              // past the module ceiling
		const TArray<FString> Warnings = AIBTiers::ValidateRow(TEXT("Doctored"), Doctored);
		TestEqual(TEXT("five defects, five warnings"), Warnings.Num(), 5);
		TestTrue(TEXT("the floor complaint names F1"),
			FString::Join(Warnings, TEXT("|")).Contains(TEXT("F1 floor")));
	});

	It("holds the envelope anchors on every tier — sight never shrinks under the constants", [this]()
	{
		for (const FName& Name : Names)
		{
			const FAIBTierRow* Row = AIBTiers::Find(Name);
			TestTrue(FString::Printf(TEXT("%s lose-sight covers the band anchor"), *Name.ToString()),
				Row->LoseSightRadius >= AIB::EngageFadeEndUU);
			TestTrue(FString::Printf(TEXT("%s memory inside the ceiling"), *Name.ToString()),
				Row->MemoryFreshSeconds <= AIB::MaxMemorySeconds);
			TestTrue(FString::Printf(TEXT("%s reaction respects the floor as authored"), *Name.ToString()),
				Row->ReactionSecondsMin >= AIB::MinReactionSeconds);
		}
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
