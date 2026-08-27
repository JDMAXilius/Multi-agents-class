#pragma once

#include "NativeGameplayTags.h"

/**
 * The HOST's contributions to the AIBot open namespaces (the module's extension door:
 * tag registration is global, never linker-scoped, so a game names its own wants and
 * POI kinds without an edit inside the AIBot module). Phase 6's Hill lives here.
 */
namespace BNAIBTags
{
	/** The Hill's want — a CHILD of AIBot.Ambition.Mode, which is exactly what the
	 *  module's hierarchy-matching Mode gate exists to serve. */
	BREACHPOINTNEXT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ambition_Mode_Hold);

	/** The Hill's POI kind — the typed join between the mode's ambition and the world
	 *  query's points. */
	BREACHPOINTNEXT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(POI_Hill);

	/** TEAMS: regroup with the team (the want) and where the team is (the POI kind).
	 *  The join that makes bots move TOWARD each other when isolated — the measured
	 *  teams-ON collapse was bots wandering alone on a halved target density. */
	BREACHPOINTNEXT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ambition_Mode_Rally);
	BREACHPOINTNEXT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(POI_Ally);
}
