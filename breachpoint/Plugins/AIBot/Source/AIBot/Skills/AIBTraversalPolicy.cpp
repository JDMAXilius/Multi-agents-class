#include "Skills/AIBTraversalPolicy.h"

#include "Core/AIBTypes.h"

const TCHAR* FAIBTraversalPolicy::Name(EAIBTraversal Verb)
{
	switch (Verb)
	{
	case EAIBTraversal::Drop:    return TEXT("DROP");
	case EAIBTraversal::Jump:    return TEXT("JUMP");
	case EAIBTraversal::Dash:    return TEXT("DASH");
	case EAIBTraversal::Grapple: return TEXT("GRAPPLE");
	default:                     return TEXT("none");
	}
}

EAIBTraversal FAIBTraversalPolicy::Choose(const FAIBTraversalRequest& Request, bool bLastResort)
{
	// FEET ON THE GROUND. Every verb below launches the body, and a second launch
	// mid-flight is either refused by the host or is a bot flailing across a gap it has
	// already committed to. Airborne is not a decision point.
	if (!Request.bGrounded)
	{
		return EAIBTraversal::None;
	}

	// NOT EVERY STEP IS A CROSSING. Below this the navmesh has it, and a bot that hopped
	// over every doorstep would read as broken rather than agile — which is worse than
	// not traversing at all, because it is visible all the time instead of occasionally.
	const float Across = Request.HorizontalUU;
	const float Rise = Request.VerticalUU;
	if (Across < AIB::TraversalMinGapUU && FMath::Abs(Rise) < AIB::JumpReachUpUU)
	{
		return EAIBTraversal::None;
	}

	// COMMIT SHORT OF THE LIMIT. Landing exactly on a lip is landing in the gap half the
	// time, and the cost of being wrong here is a death rather than a delay. Three of
	// these reaches are still [thin] — see AIBTypes.h — so the margin is doing real work.
	const float F = AIB::TraversalCommitFraction;
	const float JumpUp = AIB::JumpReachUpUU * F;
	const float JumpAcross = AIB::JumpReachAcrossUU * F;
	const float DashAcross = AIB::DashReachUU * F;
	const float DropLimit = bLastResort ? AIB::SafeDropUU : AIB::SafeDropUU * F;

	// -- DOWN: the free one, and the founder asked for it by name ("jumping down to get
	//    to the platform below where it's more path"). Gravity does the work, so the
	//    horizontal budget is the JUMP's — a bot steps off a ledge, it does not leap a
	//    chasm on the way down. Refused past the drop limit: a bot must be willing to
	//    take a fall that costs nothing and must refuse one that costs a life.
	if (Rise < -AIB::JumpReachUpUU)
	{
		const bool bSurvivable = -Rise <= DropLimit;
		const bool bReachable = Across <= JumpAcross;
		return (bSurvivable && bReachable) ? EAIBTraversal::Drop : EAIBTraversal::None;
	}

	// -- UP or LEVEL, in ascending cost.

	// JUMP. Cheapest thing a body can do, and the only one with no cooldown at all.
	if (Rise <= JumpUp && Across <= JumpAcross)
	{
		return EAIBTraversal::Jump;
	}

	// DASH. The gap-crosser, and the verb this module owned but never used to GO
	// anywhere. Roughly level on purpose: a dash is horizontal, so it buys distance, not
	// height — asking it to gain a storey is asking it to fail. Reaching for it while it
	// is on cooldown would be the futile press F7 bans, so the cooldown is an input.
	if (FMath::Abs(Rise) <= JumpUp && Across <= DashAcross && Request.bCanDash)
	{
		return EAIBTraversal::Dash;
	}

	// GRAPPLE. The only verb that gains real height, and the most expensive: a cooldown
	// plus a second of riding a hook in the open. Only where a generated route serves the
	// destination — the module never invents one, exactly as AIB19's traverse already
	// asks the host's world query rather than guessing at geometry.
	if (Rise > JumpUp && Request.bGrappleRouteServes)
	{
		return EAIBTraversal::Grapple;
	}

	// Nothing fits. The honest answer, and the common one until BN32 measures the reach.
	return EAIBTraversal::None;
}
