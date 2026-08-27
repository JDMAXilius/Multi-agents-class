#include "AIBotAdapter/BNAIBModeTags.h"

namespace BNAIBTags
{
	UE_DEFINE_GAMEPLAY_TAG(Ambition_Mode_Hold, "AIBot.Ambition.Mode.Hold");
	UE_DEFINE_GAMEPLAY_TAG(POI_Hill, "AIBot.POI.Hill");
	// TEAMS (founder, 27 Aug: "the AI should work as a team"): the regroup want and the
	// teammate-position POI kind it joins to. Both HUD-grade — teammate radar shows a
	// human exactly this.
	UE_DEFINE_GAMEPLAY_TAG(Ambition_Mode_Rally, "AIBot.Ambition.Mode.Rally");
	UE_DEFINE_GAMEPLAY_TAG(POI_Ally, "AIBot.POI.Ally");
}
