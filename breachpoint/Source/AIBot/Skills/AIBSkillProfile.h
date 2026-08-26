#pragma once

#include "CoreMinimal.h"
#include "Core/AIBTypes.h"
#include "Data/AIBDataRows.h"

/**
 * PHASE 4, the serial foundation (one writer; the four policies fan out after it).
 *
 * A resolved tier: the FAIBTierRow's competence VECTOR loaded into a queryable form.
 * Policies take their level from here; nothing else reads the row directly — one
 * resolve site (the controller, at possession), one query surface, so Phase 8's tier
 * system swaps the ROW without touching a policy.
 *
 * THE POLICY CONVENTIONS (bind all four policy files; the wave packet cites this block):
 * - Policies are plain structs, WORLDLESS by the folder law: facts, levels, positions,
 *   and time come IN as parameters; intents and numbers come OUT. No engine world
 *   types, no live handles — a target is an opaque id the caller supplies.
 * - Per-competence CONSTANTS are static functions over EAIBCompetence (the ladder is
 *   readable in one screen; Phase 8 retunes via the tier table, never by editing them).
 * - Mutable per-life STATE is a small struct the CONTROLLER owns and hands in by
 *   reference; policies never own lifetime.
 * - ALL randomness draws through the caller's FRandomStream (per-bot seeded — shared
 *   sequences read as coordinated omniscience, W-REVIEW F-3.7). Time is a double
 *   parameter. Nothing reads a clock or a world.
 * - LEVELS GATE CAPABILITIES, not just numbers (the roadmap's Halo rule): a Novice
 *   grenade skill cannot evade a blast at ANY tuning value.
 */
struct AIBOT_API FAIBSkillProfile
{
	/** Load the vector from a tier row. Called at possession; Phase 8 re-resolves when
	 *  the real tier system lands. */
	void ResolveFrom(const FAIBTierRow& Row)
	{
		Levels[static_cast<uint8>(EAIBSkill::Movement)]   = Row.Movement;
		Levels[static_cast<uint8>(EAIBSkill::Aim)]        = Row.Aim;
		Levels[static_cast<uint8>(EAIBSkill::Grenade)]    = Row.Grenade;
		Levels[static_cast<uint8>(EAIBSkill::Melee)]      = Row.Melee;
		Levels[static_cast<uint8>(EAIBSkill::Confidence)] = Row.Confidence;
		Levels[static_cast<uint8>(EAIBSkill::Teamwork)]   = Row.Teamwork;
	}

	/** The one query. Out-of-range asks answer Trained — the row's own baseline — so a
	 *  future enum entry degrades to average, never to a crash or a superhuman. */
	EAIBCompetence Level(EAIBSkill Skill) const
	{
		const uint8 Index = static_cast<uint8>(Skill);
		return Index < UE_ARRAY_COUNT(Levels) ? Levels[Index] : EAIBCompetence::Trained;
	}

private:
	EAIBCompetence Levels[6] = {
		EAIBCompetence::Trained, EAIBCompetence::Trained, EAIBCompetence::Trained,
		EAIBCompetence::Trained, EAIBCompetence::Trained, EAIBCompetence::Novice };
};
