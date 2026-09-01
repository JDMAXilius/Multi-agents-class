#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Core/AIBTags.h"
#include "Interfaces/AIBAvatarInterface.h"

/**
 * THE TEARDOWN BELT, pinned headless (aib-critic M1).
 *
 * The defect this spec exists to make impossible: the unpossess belt released Fire and
 * nothing else, under a comment asserting Fire was the only held verb. The comment was
 * true when Phase 3 wrote it and false from Phase 4, so a Sprint or an Aim survived the
 * body on a host input surface that outlives it. Nothing failed — the HOST cleaned up
 * after the module, which is the dependency FAIRPLAY F6 forbids and is invisible until
 * the day a host does not.
 *
 * A comment cannot be tested. A list can, so the belt now walks AIBTags::HeldVerbs()
 * and this spec walks the same list: every verb in it must come back, whatever is added
 * to it later. Pin 2 is deliberately hard-coded to Fire AND Sprint AND Aim as well, so
 * that shrinking the list back to Fire fails here rather than in a match.
 */
BEGIN_DEFINE_SPEC(FAIBAvatarBeltSpec, "AIBot.Sim.AvatarBelt",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

	/** The door, faked: every call recorded in order, every read benign. Reads have to
	 *  be benign rather than absent because the belt is allowed to consult the avatar —
	 *  it asks IsCrouched — and a fake that lied would test the lie. */
	struct FFakeAvatar : public IAIBAvatarInterface
	{
		TArray<FString> Journal;
		bool bCrouched = false;

		virtual void PressVerb(FGameplayTag VerbTag) override
		{
			Journal.Add(FString::Printf(TEXT("P:%s"), *VerbTag.ToString()));
		}
		virtual void ReleaseVerb(FGameplayTag VerbTag) override
		{
			Journal.Add(FString::Printf(TEXT("R:%s"), *VerbTag.ToString()));
		}

		virtual float GetHealthNorm() const override { return 1.f; }
		virtual float GetShieldNorm() const override { return 1.f; }
		virtual float GetAmmoNorm() const override { return 1.f; }
		virtual bool HasReserveAmmo() const override { return true; }
		virtual bool CanWeaponFight() const override { return true; }
		virtual bool HasUsableWeapon() const override { return true; }
		virtual int32 GetGrenadeCount() const override { return 0; }
		virtual bool IsGrounded() const override { return true; }
		virtual bool IsCrouched() const override { return bCrouched; }
		virtual bool IsAlive() const override { return true; }
		virtual bool IsAiming() const override { return false; }
		virtual float GetMeleeRangeUU() const override { return 0.f; }
		virtual bool IsBestWeaponForRange(float /*DistanceUU*/) const override { return true; }
		virtual float GetHealthNormOf(const AActor* /*Other*/) const override { return 1.f; }
		virtual bool IsAliveTarget(const AActor* /*Other*/) const override { return true; }
	};

	FFakeAvatar Avatar;

	int32 CountOf(const FString& Entry) const
	{
		int32 N = 0;
		for (const FString& Line : Avatar.Journal)
		{
			N += (Line == Entry) ? 1 : 0;
		}
		return N;
	}

	FString Released(const FGameplayTag& Verb) const
	{
		return FString::Printf(TEXT("R:%s"), *Verb.ToString());
	}

	FString Pressed(const FGameplayTag& Verb) const
	{
		return FString::Printf(TEXT("P:%s"), *Verb.ToString());
	}

END_DEFINE_SPEC(FAIBAvatarBeltSpec)

void FAIBAvatarBeltSpec::Define()
{
	BeforeEach([this]()
	{
		Avatar.Journal.Reset();
		Avatar.bCrouched = false;
	});

	Describe("the held-verb list", [this]()
	{
		It("names the verbs the module actually holds, and only those", [this]()
		{
			const TArray<FGameplayTag>& Held = AIBTags::HeldVerbs();
			TestTrue(TEXT("the list is populated"), Held.Num() > 0);
			TestTrue(TEXT("Fire is held"), Held.Contains(AIBTags::Verb_Fire));
			TestTrue(TEXT("Sprint is held"), Held.Contains(AIBTags::Verb_Sprint));
			TestTrue(TEXT("Aim is held"), Held.Contains(AIBTags::Verb_Aim));

			// A TAP CANNOT LEAK, and listing one here would make the belt release a verb
			// nobody is holding. Crouch is the interesting exclusion: it is rented, but
			// it is a TOGGLE, so a release does not give it back and the belt taps it.
			TestFalse(TEXT("Crouch is not a hold"), Held.Contains(AIBTags::Verb_Crouch));
			TestFalse(TEXT("Jump is not a hold"), Held.Contains(AIBTags::Verb_Jump));
			TestFalse(TEXT("Reload is not a hold"), Held.Contains(AIBTags::Verb_Reload));
			TestFalse(TEXT("Melee is not a hold"), Held.Contains(AIBTags::Verb_Melee));
			TestFalse(TEXT("Grenade is not a hold"), Held.Contains(AIBTags::Verb_Grenade));
			TestFalse(TEXT("Grapple is not a hold"), Held.Contains(AIBTags::Verb_Grapple));
		});
	});

	Describe("the belt", [this]()
	{
		It("gives back every verb in the list, exactly once", [this]()
		{
			AIB::ReleaseHeldVerbs(Avatar);
			for (const FGameplayTag& Verb : AIBTags::HeldVerbs())
			{
				TestEqual(*FString::Printf(TEXT("%s released once"), *Verb.ToString()),
					CountOf(Released(Verb)), 1);
			}
		});

		It("gives back Fire AND Sprint AND Aim - the exact regression", [this]()
		{
			// Hard-coded on purpose. The bug was a belt that released the trigger and
			// walked away; if the list ever shrinks back to Fire alone, the pin above
			// still passes (it walks the shrunken list) and this one fails.
			AIB::ReleaseHeldVerbs(Avatar);
			TestEqual(TEXT("Fire"), CountOf(Released(AIBTags::Verb_Fire)), 1);
			TestEqual(TEXT("Sprint"), CountOf(Released(AIBTags::Verb_Sprint)), 1);
			TestEqual(TEXT("Aim"), CountOf(Released(AIBTags::Verb_Aim)), 1);
		});

		It("does not touch the crouch of a bot that is standing", [this]()
		{
			Avatar.bCrouched = false;
			AIB::ReleaseHeldVerbs(Avatar);
			TestEqual(TEXT("no crouch press"), CountOf(Pressed(AIBTags::Verb_Crouch)), 0);
			TestEqual(TEXT("no crouch release"), CountOf(Released(AIBTags::Verb_Crouch)), 0);
		});

		It("taps a squatting bot back up, once, after the releases", [this]()
		{
			Avatar.bCrouched = true;
			AIB::ReleaseHeldVerbs(Avatar);
			TestEqual(TEXT("one crouch press"), CountOf(Pressed(AIBTags::Verb_Crouch)), 1);
			TestEqual(TEXT("one crouch release"), CountOf(Released(AIBTags::Verb_Crouch)), 1);

			// Order matters: the trigger comes back before anything else is pressed, so
			// a host that reacts to the crouch cannot see a bot still holding fire.
			const int32 CrouchAt = Avatar.Journal.IndexOfByKey(Pressed(AIBTags::Verb_Crouch));
			const int32 FireAt = Avatar.Journal.IndexOfByKey(Released(AIBTags::Verb_Fire));
			TestTrue(TEXT("fire released before the crouch tap"),
				FireAt != INDEX_NONE && CrouchAt != INDEX_NONE && FireAt < CrouchAt);
		});

		It("is idempotent, because BOTH teardown paths can run", [this]()
		{
			// OnUnPossess then EndPlay is a normal sequence, not an error case. The
			// second pass must be inert: releases are no-ops at the adapter, and the
			// crouch tap is gated on the avatar's REAL state, which the first pass
			// changed.
			Avatar.bCrouched = true;
			AIB::ReleaseHeldVerbs(Avatar);
			Avatar.bCrouched = false;          // the host's crouch really came back up
			const int32 AfterFirst = Avatar.Journal.Num();

			AIB::ReleaseHeldVerbs(Avatar);
			TestEqual(TEXT("second pass adds only the releases"),
				Avatar.Journal.Num() - AfterFirst, AIBTags::HeldVerbs().Num());
			TestEqual(TEXT("and taps no crouch"),
				CountOf(Pressed(AIBTags::Verb_Crouch)), 1);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
