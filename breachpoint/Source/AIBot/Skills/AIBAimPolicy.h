#pragma once

#include "CoreMinimal.h"
#include "Core/AIBTypes.h"
#include "Math/RandomStream.h"

/**
 * PHASE 4, skill 2 — aim, under FAIRPLAY F4: aim DRIFTS, corrected over time, never a
 * per-shot dice roll on a perfect solution. The conventions block in AIBSkillProfile.h
 * binds this file.
 *
 * The model (transcribed intent from the studied talks): the bot's aim carries a current
 * angular ERROR — a direction and a magnitude drawn inside the level's cone — and that
 * error DECAYS toward zero over the level's correction time, the way a human settles
 * onto a target. On the re-aim cadence the error is re-drawn (aim wanders, then
 * corrects again); a TARGET SWITCH resets the error to a full-cone draw and restarts
 * the clock — switching targets costs the whole settle, which is the anti-flick law.
 * The executor's face task asks for the aim POINT each tick and steers at its bounded
 * turn rate toward it; this policy never touches a controller.
 */

/** Per-life aim state; the controller owns one, the face task hands it in. */
struct AIBOT_API FAIBAimState
{
	/** Opaque identity of the target the current error was drawn against (the caller's
	 *  id for it); 0 = none. A different id on the next step is a target switch. */
	uint32 TargetId = 0;

	/** Unit direction of the current error, in world space, perpendicular-ish to the
	 *  line of aim (the draw site orthonormalises). */
	FVector ErrorDir = FVector::ZeroVector;

	/** Degrees of error at the moment it was drawn, and when it was drawn / next
	 *  re-draws. Current error = drawn magnitude decayed by elapsed/correction time. */
	float DrawnErrorDegrees = 0.f;
	double DrawnAtSeconds = 0.0;
	double NextRedrawAtSeconds = 0.0;

	/** When the policy last stepped. A GAP here longer than ReacquireGapSeconds means
	 *  the bot was not tracking (branch left, sight lost) — the next step is a
	 *  RE-ACQUISITION and costs a full switch draw, whatever the id says. This is what
	 *  closes both halves of the W-REVIEW P4+5 anti-flick finding: the corner re-peek
	 *  (same id, settled error) and the recycled UObject index (new pawn, old id). */
	double LastStepSeconds = -1.0e9;
};

struct AIBOT_API FAIBAimPolicy
{
	// -- the ladder (per-competence constants; one screen, Phase 8 retunes by table) --
	/** Full cone half-angle a fresh draw may land inside. Novice wide, Expert tight. */
	static float ErrorConeDegrees(EAIBCompetence Level);
	/** Seconds a drawn error takes to decay to ~zero (the settle). */
	static float CorrectSeconds(EAIBCompetence Level);
	/** Seconds between re-draws while ON one target (aim wander cadence). */
	static float RedrawSeconds(EAIBCompetence Level);
	/** The FLOOR the settle decays TO — never zero (W-REVIEW P4+5 F-H1: a floor of
	 *  zero made a settled Expert a perfect tracker for 80% of every fight). */
	static float ResidualErrorDegrees(EAIBCompetence Level);
	/** A step gap longer than this is a re-acquisition, not a continuation. */
	static constexpr float ReacquireGapSeconds = 0.30f;

	/**
	 * The one step, called at task tick rate: returns the aim POINT — the belief point
	 * displaced by the CURRENT (decayed) angular error as seen from the eye. Handles
	 * the first draw, the redraw cadence, and the target-switch reset internally.
	 * BeliefPoint is the sensorium's belief (F3 — never a live position; the caller
	 * guarantees that). Pure over its inputs + Rng; no clocks, no world reads.
	 */
	static FVector StepAimPoint(FAIBAimState& State, const FVector& EyeLocation,
		const FVector& BeliefPoint, uint32 TargetId, EAIBCompetence Level,
		FRandomStream& Rng, double NowSeconds);
};
