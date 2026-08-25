#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Core/AIBTags.h"
#include "Core/AIBTypes.h"

/**
 * Phase 0's honest spec: it pins only what Phase 0 actually built — the fairness floor
 * and the tag vocabulary — so rung 2 has something REAL to count from day one (a suite
 * that runs zero tests reads as INCONCLUSIVE, and rightly). Each later phase brings its
 * own suite; nothing here pretends to test code that does not exist yet.
 */
BEGIN_DEFINE_SPEC(FAIBScaffoldSpec, "AIBot.Sim.Scaffold",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FAIBScaffoldSpec)

void FAIBScaffoldSpec::Define()
{
	It("holds the FAIRPLAY F1 floor at 200ms, as a constant no tier can undercut", [this]()
	{
		TestEqual(TEXT("MinReactionSeconds"), AIB::MinReactionSeconds, 0.20f);
	});

	It("registered the eight verbs the seam audit proved the game accepts", [this]()
	{
		// A tag that failed native registration is invalid at runtime — this is the
		// earliest point a typo'd UE_DEFINE_GAMEPLAY_TAG string can be caught headless.
		TestTrue(TEXT("Fire"), AIBTags::Verb_Fire.IsValid());
		TestTrue(TEXT("Jump"), AIBTags::Verb_Jump.IsValid());
		TestTrue(TEXT("Crouch"), AIBTags::Verb_Crouch.IsValid());
		TestTrue(TEXT("Sprint"), AIBTags::Verb_Sprint.IsValid());
		TestTrue(TEXT("Melee"), AIBTags::Verb_Melee.IsValid());
		TestTrue(TEXT("Grenade"), AIBTags::Verb_Grenade.IsValid());
		TestTrue(TEXT("Reload"), AIBTags::Verb_Reload.IsValid());
		TestTrue(TEXT("WeaponNext"), AIBTags::Verb_WeaponNext.IsValid());
	});

	It("registered the core ambitions and the mode root", [this]()
	{
		TestTrue(TEXT("Engage"), AIBTags::Ambition_Engage.IsValid());
		TestTrue(TEXT("Retreat"), AIBTags::Ambition_Retreat.IsValid());
		TestTrue(TEXT("SeekWeapon"), AIBTags::Ambition_SeekWeapon.IsValid());
		TestTrue(TEXT("Search"), AIBTags::Ambition_Search.IsValid());
		TestTrue(TEXT("Roam"), AIBTags::Ambition_Roam.IsValid());
		TestTrue(TEXT("Mode root"), AIBTags::Ambition_Mode.IsValid());
	});

	It("starts facts at honest defaults — full health, no target, no memory", [this]()
	{
		const FAIBFacts Facts;
		TestEqual(TEXT("HealthNorm"), Facts.HealthNorm, 1.f);
		TestFalse(TEXT("no target"), Facts.bHasTarget);
		TestTrue(TEXT("no memory (negative age)"), Facts.LastKnownAgeSeconds < 0.f);
		TestEqual(TEXT("no allies counted"), Facts.NearbyAllies, 0);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
