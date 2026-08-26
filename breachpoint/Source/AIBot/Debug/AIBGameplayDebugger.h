#pragma once

#include "CoreMinimal.h"

class AAIBBotController;

/**
 * PHASE 8 — the eyes-on half of proof 3: live arbitration over each bot's head. One
 * multi-line debug string per think (0.1s lifetime, so it breathes with the brain),
 * behind the controller's Config bDebugOverlay — flip one ini line, watch the whole
 * lobby think. Draws: tier + skill vector, every ambition's score with the incumbent
 * marked, confidence (or its honest unknown), and the matured-stimulus queue depth.
 *
 * DELIBERATELY NOT an FGameplayDebuggerCategory: that module's registration surface is
 * unproven in this codebase (no compiled call to transcribe), while the drawn-string
 * path rides the same DrawDebugHelpers include the host's cue placeholders already
 * compile. If the terminal later proves the debugger-category API by header probe, this
 * file is the seam that grows it — the data assembly below is category-shaped already.
 */
namespace AIBDebug
{
	/** No-op unless the world can draw (shipping strips debug draw). Server-side data,
	 *  drawn in PIE/listen where the server IS a viewport — the phase's proof rig. */
	AIBOT_API void DrawBotOverlay(const AAIBBotController& Bot);
}
