#pragma once

#include "CoreMinimal.h"

/** What a bot can do when walking is not going to work. */
enum class EAIBTraversal : uint8
{
	/** No crossing this bot can make — keep walking, or give up and want something else.
	 *  The default and the safe answer: a refused crossing costs a wander, a wrong one
	 *  costs a life. */
	None,
	/** Step off. Down, within a fall worth taking. */
	Drop,
	/** Up a step or over a short gap. */
	Jump,
	/** Across a gap that is too wide to jump and roughly level. */
	Dash,
	/** Up beyond jump reach, where a grapple route serves the destination. */
	Grapple,
};

/**
 * ONE CHOOSER, FOUR VERBS — "if you have a platform in front of you but they are
 * separate, there's no path in between, if it's possible to just dash to that platform,
 * that will be something that the AI should be capable of doing... same deal with just
 * jumping down to get to the platform below" (founder, 1 Sep).
 *
 * WHY THIS EXISTS AS ONE DECISION rather than three behaviours. The three verbs were in
 * completely different places or nowhere at all: jumping was a wedge reflex when a path
 * stalled, dashing was pressed only to escape a grenade and during combat footwork —
 * never to GO anywhere — and the grapple had its own traverse state machine. Nothing
 * asked the single question that matters, which is "I want to be over there and walking
 * will not get me there; what have I got?" Asked once, the answer is geometry.
 *
 * WHY IT IS CONSERVATIVE. Three of the four reach constants are unmeasured (BN32 owes
 * them) and the chooser commits at a fraction of each. A refused crossing is the wander
 * the bot was already doing; a crossing attempted on a guessed number is a bot walking
 * off a ledge. When the numbers land, the behaviour gets braver by changing data.
 *
 * Pure and worldless: two distances and three booleans in, one verb out. It does not
 * know what a platform is, cannot see the navmesh, and never presses anything — the
 * caller owns the world and the verbs. That is what lets AIBot.Sim.TraversalPolicy pin
 * the priority order and the refusals without a world.
 */
struct AIBOT_API FAIBTraversalRequest
{
	/** Ground distance to the far side. */
	float HorizontalUU = 0.f;

	/** Height of the far side relative to the bot. Positive is UP. */
	float VerticalUU = 0.f;

	/** Feet on the ground: nothing may be chosen mid-air. */
	bool bGrounded = true;

	/** The dash is off cooldown (the bot tracks the host's own refusal window). */
	bool bCanDash = false;

	/** A generated grapple route serves this destination. The module never invents one —
	 *  the host's world query answers, exactly as AIB19's traverse already asks. */
	bool bGrappleRouteServes = false;
};

struct AIBOT_API FAIBTraversalPolicy
{
	/** The verb, or None. Ordered by cost to the bot: a drop is free, a jump is cheap, a
	 *  dash spends a cooldown, a grapple spends a longer one and a second of vulnerability.
	 *  bLastResort (AIB22 W-REVIEW #3 M5 — Egress's ask, nothing else's): the DROP commits
	 *  at the SURVIVABLE limit (SafeDropUU, no commit fraction) — the alternative to a
	 *  drop off an island is standing on it forever. Every other reach keeps its margin. */
	static EAIBTraversal Choose(const FAIBTraversalRequest& Request, bool bLastResort = false);

	/** For logs and the debugger — never for control flow. */
	static const TCHAR* Name(EAIBTraversal Verb);
};
