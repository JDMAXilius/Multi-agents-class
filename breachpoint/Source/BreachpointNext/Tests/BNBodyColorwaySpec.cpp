#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Characters/BNCharacter.h"
#include "GenericTeamAgentInterface.h"

/**
 * TEAM-COLOURED BODIES, pinned where the decision lives. The mannequin wears the ally
 * colourway or the threat one depending on who is LOOKING — the same viewer-relative rule
 * BN16 fixed for the scoreboard and the killfeed ("one hue, one meaning — held, not bent"),
 * so a team-mate is never red on one surface and blue on another.
 *
 * Why a pure static rather than a PIE check: the material swap needs a mesh, a viewer and a
 * world, but the part that can silently go WRONG is the three-answer decision — and the
 * dangerous answer is the third one. AreFriendly has two, and the engine's default attitude
 * solver famously calls NoTeam-vs-NoTeam FRIENDLY (see BNTeamsSpec). A colourway resolver
 * that leaned on it would paint every FFA lobby, and every joining client's first frames
 * before the id lands, as one big allied team — or, with the test inverted, as all threats.
 * SHIPPED is the honest unknown, and these rows are what keep it.
 */
BEGIN_DEFINE_SPEC(FBNBodyColorwaySpec, "BreachpointNext.Sim.BodyColorway",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

	void TestColorway(const TCHAR* What, EBNBodyColorway Actual, EBNBodyColorway Expected)
	{
		// Names, not raw uint8s: a failure that prints "1 != 2" costs the next session the
		// lookup this line does once.
		const auto Name = [](EBNBodyColorway C)
		{
			switch (C)
			{
				case EBNBodyColorway::Ally:   return TEXT("Ally");
				case EBNBodyColorway::Threat: return TEXT("Threat");
				default:                      return TEXT("Shipped");
			}
		};
		TestEqual(What, FString(Name(Actual)), FString(Name(Expected)));
	}

END_DEFINE_SPEC(FBNBodyColorwaySpec)

void FBNBodyColorwaySpec::Define()
{
	const FGenericTeamId Red(0);
	const FGenericTeamId Blue(1);
	const FGenericTeamId None = FGenericTeamId::NoTeam;

	It("reads a team-mate as ally and an opponent as threat, from the VIEWER's side", [=, this]()
	{
		TestColorway(TEXT("same side is ally"),
			ABNCharacter::ResolveBodyColorway(Red, Red, false), EBNBodyColorway::Ally);
		TestColorway(TEXT("other side is threat"),
			ABNCharacter::ResolveBodyColorway(Red, Blue, false), EBNBodyColorway::Threat);

		// THE MIRROR: the same two players, seen from the other machine, must swap. This is
		// what "viewer-relative" means, and an absolute Red-team/Blue-team scheme would fail
		// exactly this row.
		TestColorway(TEXT("and it swaps on the other client"),
			ABNCharacter::ResolveBodyColorway(Blue, Red, false), EBNBodyColorway::Threat);
		TestColorway(TEXT("both directions agree"),
			ABNCharacter::ResolveBodyColorway(Blue, Blue, false), EBNBodyColorway::Ally);
	});

	It("leaves an UNASSIGNED body in its shipped colours — THE guard that keeps FFA neutral", [=, this]()
	{
		// The regression this file exists for. Either side unknown = no colour, never a wrong
		// one. A two-answer resolver would send all three of these rows to Threat (or, leaning
		// on the engine's NoTeam-vs-NoTeam == Friendly, all to Ally).
		TestColorway(TEXT("their id has not landed"),
			ABNCharacter::ResolveBodyColorway(Red, None, false), EBNBodyColorway::Shipped);
		TestColorway(TEXT("MY id has not landed"),
			ABNCharacter::ResolveBodyColorway(None, Red, false), EBNBodyColorway::Shipped);
		TestColorway(TEXT("FFA: nobody has a side"),
			ABNCharacter::ResolveBodyColorway(None, None, false), EBNBodyColorway::Shipped);
	});

	It("calls YOUR OWN body your own side, even in FFA", [=, this]()
	{
		// Checked before the unassigned guard on purpose: in FFA you have no team, but you are
		// still yourself, and the kill-cam should not show you as an unpainted stranger.
		TestColorway(TEXT("self with a team"),
			ABNCharacter::ResolveBodyColorway(Red, Red, true), EBNBodyColorway::Ally);
		TestColorway(TEXT("self with NO team is still self"),
			ABNCharacter::ResolveBodyColorway(None, None, true), EBNBodyColorway::Ally);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
