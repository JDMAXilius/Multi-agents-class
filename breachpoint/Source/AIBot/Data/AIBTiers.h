#pragma once

#include "CoreMinimal.h"
#include "Data/AIBDataRows.h"

/**
 * PHASE 8 — the four tiers, as C++ (the one direction of flow: these rows are the
 * source of truth, DT_AIBTiers MIRRORS them at authoring time, and a missing table
 * changes nothing at runtime because the controller resolves from HERE).
 *
 * Halo's model, kept: a tier is a VECTOR of competences — capability gating, not stat
 * inflation. What separates a Spartan from a Recruit is which rungs of each skill
 * ladder it stands on (a Novice grenade cannot evade a blast at ANY number), plus a
 * modest perception spread: reaction draw, peripheral cone, memory window. The SIGHT
 * RADII are deliberately IDENTICAL across tiers — too many module constants anchor to
 * that envelope (the engage appetite curve, the grenade bands' reachability proof),
 * and a tier that shrank it below the anchors would re-open the inert-band defect the
 * P4 wave closed. Reaction floors: the CLOCK clamps at AIB::MinReactionSeconds (F1),
 * so a row can only slow a tier down; ValidateRow warns where an authored number is
 * silently doing nothing.
 *
 * Teamwork is the claims-board gate (Phase 7, symmetric): Recruit and Marine sit BELOW
 * it — they neither call out nor listen, and their pack-on-rocket convergence is
 * intended, legible novice play. ODST and Spartan coordinate.
 */
namespace AIBTiers
{
	/** The canonical four, in ascending order. */
	AIBOT_API void GetTierNames(TArray<FName>& OutNames);

	/** Row for a tier name, or null (caller falls back to FAIBTierRow defaults, loudly). */
	AIBOT_API const FAIBTierRow* Find(FName TierName);

	/** Authoring-time sanity: returns human-readable warnings for numbers that clamp
	 *  or contradict (sub-floor reaction, inverted draw, memory past the module
	 *  ceiling, a lose-sight below the band anchors). Empty = clean. */
	AIBOT_API TArray<FString> ValidateRow(FName TierName, const FAIBTierRow& Row);
}
