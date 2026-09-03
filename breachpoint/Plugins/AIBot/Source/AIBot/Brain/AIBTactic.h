#pragma once

#include "CoreMinimal.h"
#include "Brain/AIBAmbition.h"
#include "NativeGameplayTags.h"

/**
 * THE TACTIC LAYER (AIB26 / Phase 15) — HOW the bot fights once Engage has won: Push,
 * Flank or Hold. It is a SECOND instance of the same worldless UAIBAmbitionEngine on the
 * controller, scored against the same FAIBFacts (the native StateTree utility selectors
 * were rejected on evidence — wall-clock RNG, first-child-on-all-zero, no hysteresis or
 * commit; see TICKET_AIB26's W-AUDIT). One nested level under Engage in the tree, gated by
 * FAIBTacticGateCondition, mirrors the winner exactly as the ambition gates do.
 *
 * The one rule the audit made load-bearing: a tactic's product may reach ZERO only through
 * a LATCHED fact. A per-think trace (visible? reachable now?) would VETO Flank's own commit
 * on the first tick it went dark and the bot would dither through the very manoeuvre the
 * commit exists to protect. So every curve here floors above 0 except Flank's point term,
 * which reads the controller's latched flank point through the objective join.
 */
namespace AIBTags
{
	AIBOT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tactic_Push);
	AIBOT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tactic_Flank);
	AIBOT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tactic_Hold);
}

namespace AIBTactic
{
	/** The three tactics with C++-authored curves. Push is the FLOOR (no term can zero it,
	 *  no commit — like Roam), so the tactic engine always elects something and the
	 *  ungated Push child is only ever the mirror of that, never a fallback that hides an
	 *  all-zero board. FlankCommitSeconds is the tier row's; the other numbers are the
	 *  builder's defaults, retuned by data the same way the core ambitions are. */
	AIBOT_API void BuildDefaultTacticSpecs(TArray<FAIBAmbitionSpec>& OutSpecs, float FlankCommitSeconds);
}
