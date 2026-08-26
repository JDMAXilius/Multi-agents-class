#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Brain/AIBAmbitionEngine.h"
#include "Core/AIBTags.h"
#include "Core/AIBTypes.h"
#include "Interfaces/AIBAmbitionProvider.h"
#include "Skills/AIBSkillProfile.h"
#include "Team/AIBClaimsBoard.h"
#include "NativeGameplayTags.h"

/** A host-shaped mode child and a slot kind, registered by the spec itself. */
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AIBSpec_Mode_Claim, "AIBot.Ambition.Mode.SpecClaim");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AIBSpec_POI_Slot, "AIBot.POI.SpecSlot");

/**
 * PHASE 7, proven headless — the board is a plain struct (time as a parameter, hostility
 * as an injected predicate), and the scoring effect flows through the same translated
 * mode spec Phase 6 pinned. What each pin defends: arrival order grants (1); renewal and
 * the self-pass (2); the slot/zone line (3); CLAIM-SCOPE-IS-ALLIANCE — enemies are never
 * bound, which is the all-hostile-host inertness invariant (4); TTL lapse as the drift
 * release (5); the death belt (6); key stability (7); EXACTLY-zero scoring through the
 * veto, present-zero not absence (8-10); and the default profile's Novice Teamwork,
 * which is what keeps the whole board inert until a tier raises it (11).
 */
BEGIN_DEFINE_SPEC(FAIBClaimsSpec, "AIBot.Sim.Claims",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

	UAIBAmbitionEngine* Engine = nullptr;
	UObject* BotA = nullptr;
	UObject* BotB = nullptr;

	// The board's predicate is AreAllies (teams W-REVIEW 26 Aug: binding on !AreEnemies
	// conflated "ally" with "dead"), so the helpers now answer their own names.
	static bool Allies(const AActor*, const AActor*) { return true; }
	static bool Enemies(const AActor*, const AActor*) { return false; }

	FAIBPointOfInterest Slot(const FVector& Location) const
	{
		FAIBPointOfInterest Point;
		Point.Location = Location;
		Point.Kind = TAG_AIBSpec_POI_Slot;
		Point.Worth = 1.f;
		Point.bClaimableSlot = true;
		return Point;
	}

	FAIBAmbitionSpec Constant(FGameplayTag Tag, float Base) const
	{
		FAIBAmbitionSpec Spec;
		Spec.Tag = Tag;
		Spec.BaseUtility = Base;
		return Spec;
	}

	bool TestTag(const TCHAR* What, FGameplayTag Actual, FGameplayTag Expected)
	{
		return TestEqual(What, Actual.GetTagName(), Expected.GetTagName());
	}

END_DEFINE_SPEC(FAIBClaimsSpec)

void FAIBClaimsSpec::Define()
{
	BeforeEach([this]()
	{
		Engine = NewObject<UAIBAmbitionEngine>(GetTransientPackage(), NAME_None, RF_Transient);
		Engine->AddToRoot();
		// Claimant identity is any UObject — the board compares, never dereferences.
		BotA = NewObject<UAIBAmbitionEngine>(GetTransientPackage(), NAME_None, RF_Transient);
		BotA->AddToRoot();
		BotB = NewObject<UAIBAmbitionEngine>(GetTransientPackage(), NAME_None, RF_Transient);
		BotB->AddToRoot();
	});

	AfterEach([this]()
	{
		if (Engine) { Engine->RemoveFromRoot(); Engine = nullptr; }
		if (BotA)   { BotA->RemoveFromRoot();   BotA = nullptr; }
		if (BotB)   { BotB->RemoveFromRoot();   BotB = nullptr; }
	});

	It("grants the first asker and denies the second — arrival order, pinned", [this]()
	{
		FAIBClaimsBoard Board;
		const FAIBPointOfInterest Rocket = Slot(FVector(1000.f, 0.f, 0.f));
		TestTrue(TEXT("first come is granted"),
			Board.TryClaim(FObjectKey(BotA), nullptr, Rocket, 0.0, 5.f, &Allies));
		TestFalse(TEXT("second asker is denied"),
			Board.TryClaim(FObjectKey(BotB), nullptr, Rocket, 0.1, 5.f, &Allies));
		TestTrue(TEXT("and reads the slot as spoken for"),
			Board.IsClaimedByOther(FObjectKey(BotB), nullptr, Rocket, 0.1, &Allies));
	});

	It("renews for the claimant — a renewal is not a denial, and self never suppresses", [this]()
	{
		FAIBClaimsBoard Board;
		const FAIBPointOfInterest Rocket = Slot(FVector(1000.f, 0.f, 0.f));
		Board.TryClaim(FObjectKey(BotA), nullptr, Rocket, 0.0, 5.f, &Allies);
		TestTrue(TEXT("the claimant's re-ask renews"),
			Board.TryClaim(FObjectKey(BotA), nullptr, Rocket, 4.0, 5.f, &Allies));
		// The renewal extended the lease past the original horizon.
		TestTrue(TEXT("the renewed lease outlives the first"),
			Board.IsClaimedByOther(FObjectKey(BotB), nullptr, Rocket, 8.0, &Allies));
		// The self-pass: a bot must never veto its own route — the engine releases a
		// commit whose raw hits zero, and a self-suppressing claim would fire that
		// veto one think after every grant.
		TestFalse(TEXT("a claim never suppresses its own claimant"),
			Board.IsClaimedByOther(FObjectKey(BotA), nullptr, Rocket, 1.0, &Allies));
	});

	It("refuses zones outright — only provider-declared slots enter the board", [this]()
	{
		FAIBClaimsBoard Board;
		FAIBPointOfInterest Hill = Slot(FVector::ZeroVector);
		Hill.bClaimableSlot = false; // the zone: one volume that WANTS many bodies
		TestFalse(TEXT("a zone cannot be claimed"),
			Board.TryClaim(FObjectKey(BotA), nullptr, Hill, 0.0, 5.f, &Allies));
		TestFalse(TEXT("and is never read as suppressed"),
			Board.IsClaimedByOther(FObjectKey(BotB), nullptr, Hill, 0.0, &Allies));
		TestEqual(TEXT("nothing landed on the board"), Board.Claims.Num(), 0);
	});

	It("does not bind enemies — the all-hostile host gets an inert board, never a colluding one", [this]()
	{
		FAIBClaimsBoard Board;
		const FAIBPointOfInterest Rocket = Slot(FVector(1000.f, 0.f, 0.f));
		TestTrue(TEXT("A claims"),
			Board.TryClaim(FObjectKey(BotA), nullptr, Rocket, 0.0, 5.f, &Enemies));
		// B is A's enemy: A's book is not B's. Suppressing here would be bots conceding
		// contested resources to opponents — rigged, strictly worse than unfair.
		TestFalse(TEXT("an enemy's claim never suppresses"),
			Board.IsClaimedByOther(FObjectKey(BotB), nullptr, Rocket, 0.1, &Enemies));
		TestTrue(TEXT("and never denies — each alliance runs its own book"),
			Board.TryClaim(FObjectKey(BotB), nullptr, Rocket, 0.1, 5.f, &Enemies));
	});

	It("expires at the TTL — ambition drift releases by non-renewal, on the board's clock", [this]()
	{
		FAIBClaimsBoard Board;
		const FAIBPointOfInterest Rocket = Slot(FVector(1000.f, 0.f, 0.f));
		Board.TryClaim(FObjectKey(BotA), nullptr, Rocket, 0.0, 5.f, &Allies);
		TestTrue(TEXT("live inside the lease"),
			Board.IsClaimedByOther(FObjectKey(BotB), nullptr, Rocket, 4.9, &Allies));
		TestFalse(TEXT("lapsed past it"),
			Board.IsClaimedByOther(FObjectKey(BotB), nullptr, Rocket, 5.01, &Allies));
		TestTrue(TEXT("and the slot reopens"),
			Board.TryClaim(FObjectKey(BotB), nullptr, Rocket, 5.1, 5.f, &Allies));
	});

	It("releases everything for a finished claimant immediately — no dead bot holds a slot", [this]()
	{
		FAIBClaimsBoard Board;
		Board.TryClaim(FObjectKey(BotA), nullptr, Slot(FVector(1000.f, 0.f, 0.f)), 0.0, 5.f, &Allies);
		Board.TryClaim(FObjectKey(BotA), nullptr, Slot(FVector(3000.f, 0.f, 0.f)), 0.0, 5.f, &Allies);
		Board.ReleaseAll(FObjectKey(BotA));
		TestEqual(TEXT("the book is empty"), Board.Claims.Num(), 0);
		TestTrue(TEXT("a teammate takes the slot the same instant"),
			Board.TryClaim(FObjectKey(BotB), nullptr, Slot(FVector(1000.f, 0.f, 0.f)), 0.5, 5.f, &Allies));
	});

	It("keys a slot stably — float drift on one slot never splits it into two claims", [this]()
	{
		FAIBClaimsBoard Board;
		// Same authored slot, two queries, 1uu of drift: one cell, one claim.
		Board.TryClaim(FObjectKey(BotA), nullptr, Slot(FVector(50.f, 50.f, 50.f)), 0.0, 5.f, &Allies);
		TestTrue(TEXT("the drifted read is the same slot"),
			Board.IsClaimedByOther(FObjectKey(BotB), nullptr, Slot(FVector(51.f, 50.f, 50.f)), 0.1, &Allies));
		TestFalse(TEXT("a genuinely distinct slot is not"),
			Board.IsClaimedByOther(FObjectKey(BotB), nullptr, Slot(FVector(500.f, 50.f, 50.f)), 0.1, &Allies));
	});

	It("scores a claimed want at exactly zero — it loses to the floor, and only the flag differs", [this]()
	{
		FAIBModeAmbition Mode;
		Mode.AmbitionTag = TAG_AIBSpec_Mode_Claim;
		Mode.BaseUtility = 1.2f;
		FAIBAmbitionSpec Translated;
		UAIBAmbitionEngine::BuildModeAmbitionSpec(Mode, Translated);
		Engine->RegisterAmbition(Translated);
		Engine->RegisterAmbition(Constant(AIBTags::Ambition_Roam, 0.3f));

		FAIBFacts Facts;
		FAIBObjectiveFact& Objective = Facts.Objectives.AddDefaulted_GetRef();
		Objective.AmbitionTag = TAG_AIBSpec_Mode_Claim;
		Objective.Urgency = 0.9f;

		Objective.bClaimedElsewhere = true;
		TestTag(TEXT("claimed: the want dies and the floor wins"),
			Engine->Rescore(Facts, 1.0), AIBTags::Ambition_Roam);

		Objective.bClaimedElsewhere = false;
		TestTag(TEXT("unclaimed, SAME facts otherwise: the want wins"),
			Engine->Rescore(Facts, 10.0), TAG_AIBSpec_Mode_Claim);
	});

	It("releases a committed loser in ONE rescore — exactly zero is what feeds the veto", [this]()
	{
		FAIBModeAmbition Mode;
		Mode.AmbitionTag = TAG_AIBSpec_Mode_Claim;
		Mode.BaseUtility = 1.2f;
		FAIBAmbitionSpec Translated;
		UAIBAmbitionEngine::BuildModeAmbitionSpec(Mode, Translated); // CommitSeconds = 3
		Engine->RegisterAmbition(Translated);
		Engine->RegisterAmbition(Constant(AIBTags::Ambition_Roam, 0.3f));

		FAIBFacts Facts;
		FAIBObjectiveFact& Objective = Facts.Objectives.AddDefaulted_GetRef();
		Objective.AmbitionTag = TAG_AIBSpec_Mode_Claim;
		Objective.Urgency = 0.9f;
		TestTag(TEXT("the race loser committed first"),
			Engine->Rescore(Facts, 1.0), TAG_AIBSpec_Mode_Claim);

		// A teammate's claim lands; the next rescore is INSIDE the 3s commit window —
		// and the zero-score veto releases it anyway. This is the pin that makes the
		// contract's "exactly 0" load-bearing: an epsilon here would hold the loser on
		// a dead route until the window ran out.
		Objective.bClaimedElsewhere = true;
		TestTag(TEXT("one rescore later the loser is elsewhere"),
			Engine->Rescore(Facts, 1.5), AIBTags::Ambition_Roam);
	});

	It("stays silent WITHOUT the fact exactly as Phase 6 pinned — absence and the flag are different silences", [this]()
	{
		FAIBModeAmbition Mode;
		Mode.AmbitionTag = TAG_AIBSpec_Mode_Claim;
		Mode.BaseUtility = 1.2f;
		FAIBAmbitionSpec Translated;
		UAIBAmbitionEngine::BuildModeAmbitionSpec(Mode, Translated);
		Engine->RegisterAmbition(Translated);
		Engine->RegisterAmbition(Constant(AIBTags::Ambition_Roam, 0.3f));

		// No objective fact at all: the URGENCY consideration silences the want
		// (ValueWhenUnknown 0) and the claim consideration must NOT double-veto —
		// its unknown waves through (1), so the one silence has one owner.
		const FAIBFacts Bare;
		TestTag(TEXT("factless mode want is silent — the Phase-6 law, undisturbed"),
			Engine->Rescore(Bare, 1.0), AIBTags::Ambition_Roam);
	});

	It("ships inert: the default profile's Teamwork is Novice, below the gate", [this]()
	{
		// The symmetric tier gate (a Novice neither files nor honours) lives at the
		// call sites; what is pinnable headless is the default that keeps the whole
		// phase dormant until Phase 8 raises a tier — and that the row's baseline says
		// the same thing.
		const FAIBSkillProfile Defaults;
		TestEqual(TEXT("profile default"), static_cast<int32>(Defaults.Level(EAIBSkill::Teamwork)),
			static_cast<int32>(EAIBCompetence::Novice));
		const FAIBTierRow Row;
		TestEqual(TEXT("row default"), static_cast<int32>(Row.Teamwork),
			static_cast<int32>(EAIBCompetence::Novice));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
