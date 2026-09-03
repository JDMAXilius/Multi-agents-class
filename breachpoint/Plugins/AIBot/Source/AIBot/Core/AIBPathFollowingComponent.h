#pragma once

#include "CoreMinimal.h"
#include "Navigation/PathFollowingComponent.h"
#include "AIBPathFollowingComponent.generated.h"

class INavLinkCustomInterface;

/**
 * The bot's path follower: the engine's, plus the ONE place a traversal verb fires from a
 * path (AIB22 step 4). A custom link segment presses JUMP as the engine hands control to
 * the link; a segment starting on a UAIBNavArea_Jump poly presses JUMP once. Reads only
 * the corridor (poly refs), never the string-pulled points — under Detour Crowd (Phase 13)
 * PathPoints holds just start+end and SetMoveSegment carries corridor indices; the
 * string-pulled flag on the path is what tells the two apart.
 */
UCLASS()
// Phase 13: BASE LINE — rebase is this one token -> UCrowdFollowingComponent (plus the include
// "Navigation/CrowdFollowingComponent.h"). UHT needs the literal name, so no alias. Nothing
// below reads PathPoints geometry, so the hook survives the crowd.
class AIBOT_API UAIBPathFollowingComponent : public UPathFollowingComponent
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
