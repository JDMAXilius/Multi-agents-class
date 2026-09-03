#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Core/AIBTypes.h"
#include "Skills/AIBTraversalPolicy.h"

/**
 * GETTING BETWEEN PLATFORMS, pinned headless (founder, 1 Sep).
 *
 * The pins are written against the constants rather than against magic distances,
 * because three of the four reaches are [thin] and BN32 will replace them. When it does,
 * the numbers move and every promise below still has to hold — which is the point of
 * writing them this way.
 */
BEGIN_DEFINE_SPEC(FAIBTraversalPolicySpec, "AIBot.Sim.TraversalPolicy",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

	static FAIBTraversalRequest Gap(float Across, float Rise)
	{
		FAIBTraversalRequest R;
		R.HorizontalUU = Across;
		R.VerticalUU = Rise;
		R.bGrounded = true;
		R.bCanDash = true;
		return R;
	}

	// Comfortably inside / outside each reach, allowing for the commit fraction.
	static float Inside(float Reach)  { return Reach * AIB::TraversalCommitFraction * 0.7f; }
	static float Outside(float Reach) { return Reach * 1.2f; }

END_DEFINE_SPEC(FAIBTraversalPolicySpec)

void FAIBTraversalPolicySpec::Define()
{
	Describe("the cheap verb first", [this]()
	{
		It("jumps a step up rather than spending a dash", [this]()
		{
			TestEqual(TEXT("jump"),
				FAIBTraversalPolicy::Choose(Gap(Inside(AIB::JumpReachAcrossUU),
					Inside(AIB::JumpReachUpUU))),
				EAIBTraversal::Jump);
		});

		It("dashes a level gap too wide to jump", [this]()
		{
			// The founder's case: "if you have a platform in front of you but they are
			// separate, there's no path in between... just dash to that platform."
			TestEqual(TEXT("dash"),
				FAIBTraversalPolicy::Choose(Gap(Outside(AIB::JumpReachAcrossUU), 0.f)),
				EAIBTraversal::Dash);
		});

		It("does not reach for a dash that is on cooldown", [this]()
		{
			// A refused verb pressed anyway is the futile press F7 bans — the reload
			// taught this module that lesson once already.
			FAIBTraversalRequest R = Gap(Outside(AIB::JumpReachAcrossUU), 0.f);
			R.bCanDash = false;
			TestEqual(TEXT("nothing"), FAIBTraversalPolicy::Choose(R), EAIBTraversal::None);
		});

		It("will not ask a dash to gain a storey", [this]()
		{
			// A dash is horizontal: it buys distance, not height. Asking it to climb is
			// asking it to fail, and to spend a cooldown failing.
			FAIBTraversalRequest R = Gap(Inside(AIB::DashReachUU), Outside(AIB::JumpReachUpUU));
			R.bGrappleRouteServes = false;
			TestEqual(TEXT("not a dash"), FAIBTraversalPolicy::Choose(R), EAIBTraversal::None);
		});
	});

	Describe("height", [this]()
	{
		It("grapples up, but only where a route actually serves it", [this]()
		{
			FAIBTraversalRequest R = Gap(300.f, Outside(AIB::JumpReachUpUU));
			R.bGrappleRouteServes = true;
			TestEqual(TEXT("grapple"), FAIBTraversalPolicy::Choose(R), EAIBTraversal::Grapple);

			R.bGrappleRouteServes = false;
			TestEqual(TEXT("no route, no crossing"),
				FAIBTraversalPolicy::Choose(R), EAIBTraversal::None);
		});
	});

	Describe("down", [this]()
	{
		It("steps off a platform worth dropping from", [this]()
		{
			// "same deal with just jumping down to get to the platform below."
			TestEqual(TEXT("drop"),
				FAIBTraversalPolicy::Choose(Gap(Inside(AIB::JumpReachAcrossUU),
					-AIB::SafeDropUU * 0.4f)),
				EAIBTraversal::Drop);
		});

		It("refuses a fall that would kill it", [this]()
		{
			TestEqual(TEXT("no"),
				FAIBTraversalPolicy::Choose(Gap(100.f, -Outside(AIB::SafeDropUU))),
				EAIBTraversal::None);
		});

		It("takes the drop up to the SURVIVABLE limit only as a last resort — Egress's ask", [this]()
		{
			// Between the committed limit (80%) and the survivable one: refused by every
			// ordinary caller, taken when the alternative is standing on an island forever.
			const FAIBTraversalRequest Lip = Gap(60.f, -AIB::SafeDropUU * 0.9f);
			TestEqual(TEXT("ordinary: no"), FAIBTraversalPolicy::Choose(Lip), EAIBTraversal::None);
			TestEqual(TEXT("last resort: drop"), FAIBTraversalPolicy::Choose(Lip, /*bLastResort=*/true), EAIBTraversal::Drop);
			TestEqual(TEXT("last resort never exceeds the survivable limit"),
				FAIBTraversalPolicy::Choose(Gap(60.f, -AIB::SafeDropUU * 1.01f), true), EAIBTraversal::None);
		});

		It("refuses a survivable drop it cannot reach the far side of", [this]()
		{
			// Gravity does the vertical work; it does not do the horizontal. A bot
			// stepping off toward a ledge it cannot reach lands in the gap.
			TestEqual(TEXT("no"),
				FAIBTraversalPolicy::Choose(Gap(Outside(AIB::DashReachUU),
					-AIB::SafeDropUU * 0.3f)),
				EAIBTraversal::None);
		});
	});

	Describe("the refusals that keep it from looking broken", [this]()
	{
		It("does nothing mid-air", [this]()
		{
			FAIBTraversalRequest R = Gap(Inside(AIB::JumpReachAcrossUU), 0.f);
			R.bGrounded = false;
			TestEqual(TEXT("nothing"), FAIBTraversalPolicy::Choose(R), EAIBTraversal::None);
		});

		It("does not hop over a doorstep", [this]()
		{
			// A bot that jumped at every tiny step would read as broken all the time,
			// which is worse than not traversing at all.
			TestEqual(TEXT("nothing"),
				FAIBTraversalPolicy::Choose(Gap(AIB::TraversalMinGapUU * 0.5f, 5.f)),
				EAIBTraversal::None);
		});

		It("refuses rather than guesses when nothing fits", [this]()
		{
			// The common answer until BN32 measures the reach, and the correct one: a
			// refused crossing is the wander the bot was doing anyway.
			TestEqual(TEXT("nothing"),
				FAIBTraversalPolicy::Choose(Gap(Outside(AIB::DashReachUU), 0.f)),
				EAIBTraversal::None);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
