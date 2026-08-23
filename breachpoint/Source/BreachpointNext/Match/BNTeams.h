#pragma once

#include "CoreMinimal.h"

class AActor;

/**
 * WHICH SIDE — the whole of it. Teams are an integer on the PlayerState and two questions asked
 * about that integer; everything else (scoring, spawns, colour) reads these and adds nothing of
 * its own.
 *
 * A NAMESPACE, not a component or a subsystem: the answer must be identical on the server, the
 * killer's machine and an observer's, and the only input is a replicated int32 that every machine
 * already has. Anything statefull here would be a second source of truth for a fact that has one.
 *
 * Unassigned is a REAL state, not an error — a controller exists for a frame before the mode
 * assigns it, and a joining client can hold a PlayerState whose TeamId bunch has not landed.
 * Both questions below answer conservatively for it: nobody is an ally of the unassigned.
 */
namespace BNTeams
{
	/** INDEX_NONE, so an unset int32 field is not silently team 0. */
	inline constexpr int32 Unassigned = INDEX_NONE;

	/** Two sides. 4v4 is the design; the number lives here so a mode that wants three has one
	 *  place to change and one balance rule to re-read. */
	inline constexpr int32 Count = 2;

	inline bool IsValidTeam(int32 TeamId) { return TeamId >= 0 && TeamId < Count; }

	/** The team of whatever you have in hand: a PlayerState, a Controller, or a Pawn — each
	 *  resolves to the same PlayerState. Unassigned for scenery, a projectile, or null. */
	BREACHPOINTNEXT_API int32 GetTeamId(const AActor* Actor);

	/** True only when BOTH sides are assigned AND equal. Deliberately NOT "!AreEnemies": an
	 *  unassigned pair must not be friendly (friendly fire off would make them invulnerable to
	 *  each other) and must not be enemies either where that decides a kill credit. Callers
	 *  choose which conservative answer their case needs, from a function that never guesses. */
	BREACHPOINTNEXT_API bool AreAllies(const AActor* A, const AActor* B);
}
