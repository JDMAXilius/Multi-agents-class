#pragma once

#include "CoreMinimal.h"
#include "Core/AIBTypes.h"

class AAIBBotController;

/**
 * The world-touching assembler — in Core/ BECAUSE it touches the world, keeping law 4
 * ("Brain/ and Skills/ name no UWorld/AActor") literally true (W-REVIEW 2b).
 *
 * One function, one direction: world in, facts out. The F3 discipline is concentrated
 * here so the brain cannot cheat even by accident:
 *  - Self facts come from the avatar door; absent adapter => bVitalsKnown stays false
 *    and the brain scores "unknown", never "healthy".
 *  - Target position: the LIVE actor location ONLY while the sensorium says sight is
 *    current; during the juke window and from memory it is GetLastSeenLocation, with
 *    bTargetFactsFromMemory set — the audit marker reviewers grep for (F-6.5).
 *  - Target health: read through the avatar door ONLY while sight is current; a bot
 *    does not learn how hurt you are through a wall.
 *  - Mode urgency is CLAMPED 0..1 here — the one site (F-6.7).
 *
 * Phase 3 adds IAIBWorldQuery/IAIBAmbitionProvider inputs (allies, enemies, objectives);
 * Phase 2 builds what the sensorium and the avatar door already know.
 */
namespace AIBFactsBuilder
{
	AIBOT_API FAIBFacts Build(const AAIBBotController& Bot, double NowSeconds);
}
