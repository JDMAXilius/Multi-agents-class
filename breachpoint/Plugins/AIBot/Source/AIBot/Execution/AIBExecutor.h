#pragma once

#include "CoreMinimal.h"

class AAIBBotController;

/**
 * The seam that keeps StateTree and Behavior Tree interchangeable. The executor READS the
 * active ambition from the controller's engine each evaluation — nothing pushes a copy in,
 * because the P2 respawn bug is what a second copy of arbitration state costs.
 */
class AIBOT_API IAIBExecutor
{
public:
	virtual ~IAIBExecutor() = default;

	/** Begin running against this controller. Loads whatever asset the impl needs; a miss
	 *  is ONE loud line and a standing bot, never a crash. */
	virtual void Start(AAIBBotController& Bot) = 0;

	/** Possession ended: leave nothing running, nothing held. */
	virtual void Stop() = 0;
};
