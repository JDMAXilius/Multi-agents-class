#pragma once

#include "CoreMinimal.h"
#include "Navigation/CrowdFollowingComponent.h"
#include "AIBPathFollowingComponent.generated.h"

class INavLinkCustomInterface;

/**
 * The bot's path follower: Detour Crowd's (Phase 13, AIB24 — separation on, players as
 * crowd obstacles game-side, NEVER the RVO SetAvoidanceGroup family), plus the ONE place
 * a traversal verb fires from a path (AIB22 step 4). A custom link segment presses JUMP as
 * the crowd manager hands control to the link (CrowdManager.cpp:862 calls
 * StartUsingCustomLink by name; FinishUsingCustomLink resumes the agent); a segment
 * starting on a UAIBNavArea_Jump poly presses JUMP once. Reads only the corridor (poly
 * refs), never the string-pulled points — under the crowd PathPoints holds just start+end
 * and SetMoveSegment carries corridor indices; the string-pulled flag on the path is what
 * tells the two apart. The crowd parameters are set at possession (AIBBotController.cpp).
 */
UCLASS()
class AIBOT_API UAIBPathFollowingComponent : public UCrowdFollowingComponent
{
	GENERATED_BODY()

protected:
	virtual void SetMoveSegment(int32 SegmentStartIndex) override;
	virtual void StartUsingCustomLink(INavLinkCustomInterface* CustomNavLink, const FVector& DestPoint) override;
	virtual void Reset() override;

private:
	/** Press JUMP through the owning bot's avatar door (the watchdog's door, reused); logs
	 *  the traverse line. False without authority, pawn, door, or ground under the feet. */
	bool PressJump(const TCHAR* Via, const FVector& To);

	/** Poly the segment starts on: the path point's NodeRef when string-pulled, else the
	 *  corridor entry (crowd paths skip string pulling, so the index IS a corridor index). */
	NavNodeRef SegmentStartPoly(int32 SegmentStartIndex) const;

	/** Once per segment: a resumed/updated path re-enters SetMoveSegment at the same index. */
	int32 JumpedSegmentIndex = INDEX_NONE;
};
