#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Animation/BNLAnimInstance.h"

/**
 * THE ONE DECISION THAT MADE EVERY BOT SLIDE.
 *
 * Lyra's locomotion state machine leaves Idle on `HasAcceleration`. The old spine answered it
 * with `GetCurrentAcceleration()` alone, which is fed ONLY by ConsumeInputVector — i.e. only by
 * AddMovementInput, i.e. only by a player. A bot moves through path following:
 * MoveTo* -> RequestDirectMove -> CMC::RequestedVelocity, and CalcVelocity applies that via a
 * LOCAL RequestedAcceleration without ever assigning the member. So a pathing bot's
 * GetCurrentAcceleration() is exactly zero, and every bot ran at full speed in an Idle pose.
 *
 * The ROOT fix lives in ABNCharacter (bUseAccelerationForPaths); what is proved here is the
 * against the old one-source read and passes against the owner-aware one.
 *
 * It can exist at all because the decision was lifted out of NativeThreadSafeUpdateAnimation
 * into a static: no world, no pawn, no controller, no CMC. Keep it that way.
 */
BEGIN_DEFINE_SPEC(FBNAnimOwnerDriverSpec, "BreachpointNext.Sim.AnimOwnerDriver",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

	static constexpr bool AI = true;
	static constexpr bool Player = false;

	const FVector Still = FVector::ZeroVector;
	const FVector Pushing = FVector(1200.f, 0.f, 0.f);   // an input vector, cm/s^2
	const FVector Pathing = FVector(600.f, 0.f, 0.f);    // path following's ask, cm/s

	bool Moves(bool bAI, const FVector& Accel, const FVector& Requested) const
	{
		return UBNLAnimInstance::ComputeHasAcceleration(bAI, Accel, Requested);
	}

END_DEFINE_SPEC(FBNAnimOwnerDriverSpec)

void FBNAnimOwnerDriverSpec::Define()
{
	Describe("a player", [this]
	{
		It("moves when input is applied", [this]
		{
			TestTrue(TEXT("input acceleration"), Moves(Player, Pushing, Still));
		});

		It("is idle with no input", [this]
		{
			TestFalse(TEXT("no input"), Moves(Player, Still, Still));
		});

		It("ignores a stale path request, because a player never has one", [this]
		{
			// Guards the OR from leaking: a possessed-by-player pawn must not animate off
			// whatever path following last left on the component.
			TestFalse(TEXT("player + requested velocity"), Moves(Player, Still, Pathing));
		});
	});

	Describe("a bot", [this]
	{
		It("still reports movement if bUseAccelerationForPaths is ever turned back off", [this]
		{
			// The SECONDARY net, not the fix. With ABNCharacter's bUseAccelerationForPaths set,
			// a pathing bot's input acceleration is populated and this case never arises. It is
			// kept so that flipping that flag off degrades to a stale pose rather than silently
			// restoring the slide.
			TestTrue(TEXT("requested velocity with zero input"), Moves(AI, Still, Pathing));
		});


		It("is idle when path following has stopped asking", [this]
		{
			TestFalse(TEXT("neither source"), Moves(AI, Still, Still));
		});

		It("also moves on direct input, for tasks that steer by AddMovementInput", [this]
		{
			TestTrue(TEXT("input acceleration"), Moves(AI, Pushing, Still));
		});
	});

	Describe("an unpossessed pawn", [this]
	{
		It("does not animate off a path request left behind by its dead owner", [this]
		{
			// Unpossessed resolves to NEITHER player nor AI, so it takes the player branch's
			// strictness. A corpse must not walk.
			TestFalse(TEXT("no controller"), Moves(/*bAIControlled=*/false, Still, Pathing));
		});

	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
