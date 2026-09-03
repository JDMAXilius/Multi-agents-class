#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Brain/AIBAmbitionEngine.h"
#include "Core/AIBTypes.h"
#include "GameFramework/Actor.h"
#include "Team/AIBTargetClaims.h"

/**
 * PHASE 12 (AIB23), proven headless — the capped target claims are a plain struct (time as
 * a parameter, alliance and liveness as injected predicates, actors as opaque handles).
 * Pins: the cap grants two and denies the third (1); a renewal is not a grant and the
 * asker's own claim never counts against it (2); ordinals follow grant order — the ring
 * spread's seed (3); a TTL lapse reopens the slot (4); Engage exit honours the minimum
 * hold: a young claim stays, an old one goes (5); death releases through the injected
 * liveness alone — no position, no vitals (6); the unpossess belt (7); and enemies never
 * bind — each alliance runs its own book, an all-hostile host is inert (8).
 */
BEGIN_DEFINE_SPEC(FAIBTargetClaimsSpec, "AIBot.Sim.TargetClaims",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

	UObject* BotA = nullptr;
	UObject* BotB = nullptr;
	UObject* BotC = nullptr;
	AActor* Enemy = nullptr;

	static bool Allies(const AActor*, const AActor*) { return true; }
	static bool Enemies(const AActor*, const AActor*) { return false; }
	static bool Alive(const AActor*, const AActor*) { return true; }
	static bool Dead(const AActor*, const AActor*) { return false; }

	EAIBTargetClaimResult Claim(FAIBTargetClaims& Board, UObject* Bot, double Now, int32* OutHolders = nullptr)
	{
		int32 Holders = 0;
		const EAIBTargetClaimResult R = Board.TryClaim(FObjectKey(Bot), nullptr, Enemy, Now, 5.f, &Allies, Holders);
		if (OutHolders) { *OutHolders = Holders; }
		return R;
	}

END_DEFINE_SPEC(FAIBTargetClaimsSpec)

void FAIBTargetClaimsSpec::Define()
{
	BeforeEach([this]()
	{
		// Identities are any UObject — the board compares, never dereferences. The target
		// is a bare actor handle: validity is all the board reads of it.
		BotA = NewObject<UAIBAmbitionEngine>(GetTransientPackage(), NAME_None, RF_Transient); BotA->AddToRoot();
		BotB = NewObject<UAIBAmbitionEngine>(GetTransientPackage(), NAME_None, RF_Transient); BotB->AddToRoot();
		BotC = NewObject<UAIBAmbitionEngine>(GetTransientPackage(), NAME_None, RF_Transient); BotC->AddToRoot();
		Enemy = NewObject<AActor>(GetTransientPackage(), NAME_None, RF_Transient); Enemy->AddToRoot();
	});

	AfterEach([this]()
	{
		for (UObject** O : { &BotA, &BotB, &BotC })
		{
			if (*O) { (*O)->RemoveFromRoot(); *O = nullptr; }
		}
		if (Enemy) { Enemy->RemoveFromRoot(); Enemy = nullptr; }
	});

	It("grants two and denies the third — the cap is the invariant", [this]()
	{
		FAIBTargetClaims Board;
		int32 Holders = 0;
		TestEqual(TEXT("first"), static_cast<int32>(Claim(Board, BotA, 0.0, &Holders)), static_cast<int32>(EAIBTargetClaimResult::Granted));
		TestEqual(TEXT("1/2"), Holders, 1);
		TestEqual(TEXT("second"), static_cast<int32>(Claim(Board, BotB, 0.1, &Holders)), static_cast<int32>(EAIBTargetClaimResult::Granted));
		TestEqual(TEXT("2/2"), Holders, 2);
		TestEqual(TEXT("third denied"), static_cast<int32>(Claim(Board, BotC, 0.2, &Holders)), static_cast<int32>(EAIBTargetClaimResult::Denied));
		TestEqual(TEXT("denied line reads 2/2"), Holders, AIB::TargetClaimCap);
		TestEqual(TEXT("the third sees two allies on him, self excluded"),
			Board.CountAlliesOn(FObjectKey(BotC), nullptr, Enemy, 0.2, &Allies), 2);
		TestFalse(TEXT("and holds nothing"), Board.Holds(FObjectKey(BotC), Enemy, 0.2));
	});

	It("renews for a holder — not a grant, and its own claim never counts against it", [this]()
	{
		FAIBTargetClaims Board;
		Claim(Board, BotA, 0.0);
		Claim(Board, BotB, 0.0);
		TestEqual(TEXT("A renews at the cap"), static_cast<int32>(Claim(Board, BotA, 4.0)), static_cast<int32>(EAIBTargetClaimResult::Renewed));
		TestEqual(TEXT("A sees ONE ally on him"), Board.CountAlliesOn(FObjectKey(BotA), nullptr, Enemy, 4.0, &Allies), 1);
		TestTrue(TEXT("the renewed lease outlives the first"), Board.Holds(FObjectKey(BotA), Enemy, 8.0));
	});

	It("ranks holders by grant order — the ring spread's seed", [this]()
	{
		FAIBTargetClaims Board;
		Claim(Board, BotA, 0.0);
		Claim(Board, BotB, 0.5);
		TestEqual(TEXT("A first"), Board.Ordinal(FObjectKey(BotA), nullptr, Enemy, 1.0, &Allies), 0);
		TestEqual(TEXT("B second"), Board.Ordinal(FObjectKey(BotB), nullptr, Enemy, 1.0, &Allies), 1);
		TestEqual(TEXT("C none"), Board.Ordinal(FObjectKey(BotC), nullptr, Enemy, 1.0, &Allies), INDEX_NONE);
	});

	It("lapses at the TTL by non-renewal and reports reason=ttl — the slot reopens", [this]()
	{
		FAIBTargetClaims Board;
		Claim(Board, BotA, 0.0);
		Claim(Board, BotB, 0.0);
		TestEqual(TEXT("denied inside the lease"), static_cast<int32>(Claim(Board, BotC, 4.9)), static_cast<int32>(EAIBTargetClaimResult::Denied));
		TArray<FAIBReleasedTargetClaim> Released;
		Board.Prune(5.01, &Alive, Released);
		TestEqual(TEXT("both lapsed"), Released.Num(), 2);
		TestTrue(TEXT("as ttl"), Released.Num() == 2 && Released[0].Reason == EAIBTargetClaimRelease::Ttl);
		TestEqual(TEXT("C takes it"), static_cast<int32>(Claim(Board, BotC, 5.1)), static_cast<int32>(EAIBTargetClaimResult::Granted));
	});

	It("releases on Engage exit only past the minimum hold — a blink keeps its claim", [this]()
	{
		FAIBTargetClaims Board;
		Claim(Board, BotA, 0.0);
		TArray<FAIBReleasedTargetClaim> Released;
		Board.ReleaseOnExit(FObjectKey(BotA), 1.0, /*MinHold*/ 2.f, Released);
		TestEqual(TEXT("too young: kept"), Released.Num(), 0);
		TestTrue(TEXT("still held"), Board.Holds(FObjectKey(BotA), Enemy, 1.0));
		Board.ReleaseOnExit(FObjectKey(BotA), 2.5, 2.f, Released);
		TestEqual(TEXT("old enough: released"), Released.Num(), 1);
		TestTrue(TEXT("as exit"), Released.Num() == 1 && Released[0].Reason == EAIBTargetClaimRelease::Exit);
		TestFalse(TEXT("gone"), Board.Holds(FObjectKey(BotA), Enemy, 2.5));
	});

	It("releases on target death through the injected liveness — nothing else crosses", [this]()
	{
		FAIBTargetClaims Board;
		Claim(Board, BotA, 0.0);
		TArray<FAIBReleasedTargetClaim> Released;
		Board.Prune(1.0, &Alive, Released);
		TestEqual(TEXT("alive: kept"), Released.Num(), 0);
		Board.Prune(1.1, &Dead, Released);
		TestEqual(TEXT("dead: released"), Released.Num(), 1);
		TestTrue(TEXT("as death"), Released.Num() == 1 && Released[0].Reason == EAIBTargetClaimRelease::Death);
		TestEqual(TEXT("book empty"), Board.NumLive(1.1), 0);
	});

	It("releases everything for a finished claimant at once — reason=unpossess", [this]()
	{
		FAIBTargetClaims Board;
		Claim(Board, BotA, 0.0);
		TArray<FAIBReleasedTargetClaim> Released;
		Board.ReleaseAll(FObjectKey(BotA), Released);
		TestEqual(TEXT("one release"), Released.Num(), 1);
		TestTrue(TEXT("as unpossess"), Released.Num() == 1 && Released[0].Reason == EAIBTargetClaimRelease::Unpossess);
		TestEqual(TEXT("C takes it the same instant"), static_cast<int32>(Claim(Board, BotC, 0.1)), static_cast<int32>(EAIBTargetClaimResult::Granted));
	});

	It("does not bind enemies — each alliance runs its own book, an all-hostile host is inert", [this]()
	{
		FAIBTargetClaims Board;
		int32 Holders = 0;
		for (UObject* Bot : { BotA, BotB, BotC })
		{
			TestEqual(TEXT("every stranger is granted"),
				static_cast<int32>(Board.TryClaim(FObjectKey(Bot), nullptr, Enemy, 0.0, 5.f, &Enemies, Holders)),
				static_cast<int32>(EAIBTargetClaimResult::Granted));
			TestEqual(TEXT("and each counts only itself"), Holders, 1);
		}
		TestEqual(TEXT("nobody's allies are on him"), Board.CountAlliesOn(FObjectKey(BotA), nullptr, Enemy, 0.0, &Enemies), 0);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
