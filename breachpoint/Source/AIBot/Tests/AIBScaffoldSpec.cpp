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

	It("registered the eight verbs", [this]()
	{
		// HONEST SCOPE (W-REVIEW F-5.4): this proves registration ran, not that the
		// strings are right — UE_DEFINE_GAMEPLAY_TAG registers a typo'd literal just as
		// happily. The verb->host-input map is the adapter's to test, at Phase 3.
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

	It("starts facts at honest UNKNOWNS — never a confident default", [this]()
	{
		// The W-REVIEW ruling (F-6.10): an unknowable health must not read as full
		// health and make a broken adapter fight to the death. Unknown is a state.
		const FAIBFacts Facts;
		TestFalse(TEXT("vitals unknown"), Facts.bVitalsKnown);
		TestFalse(TEXT("no target"), Facts.bHasTarget);
		TestFalse(TEXT("no memory"), Facts.bHasMemory);
		TestFalse(TEXT("no blast"), Facts.bIncomingBlast);
		TestTrue(TEXT("distance unknown (negative)"), Facts.DistToTargetUU < 0.f);
		TestEqual(TEXT("no objectives"), Facts.Objectives.Num(), 0);
	});

	It("holds the F5 ceiling and the queue cap as module constants", [this]()
	{
		TestEqual(TEXT("MaxMemorySeconds"), AIB::MaxMemorySeconds, 20.f);
		TestEqual(TEXT("MaxPendingStimuli"), AIB::MaxPendingStimuli, 64);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
