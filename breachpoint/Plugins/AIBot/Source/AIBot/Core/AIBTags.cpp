#include "Core/AIBTags.h"

namespace AIBTags
{
	UE_DEFINE_GAMEPLAY_TAG(Verb_Fire, "AIBot.Verb.Fire");
	UE_DEFINE_GAMEPLAY_TAG(Verb_Jump, "AIBot.Verb.Jump");
	UE_DEFINE_GAMEPLAY_TAG(Verb_Crouch, "AIBot.Verb.Crouch");
	UE_DEFINE_GAMEPLAY_TAG(Verb_Sprint, "AIBot.Verb.Sprint");
	UE_DEFINE_GAMEPLAY_TAG(Verb_Melee, "AIBot.Verb.Melee");
	UE_DEFINE_GAMEPLAY_TAG(Verb_Grenade, "AIBot.Verb.Grenade");
	UE_DEFINE_GAMEPLAY_TAG(Verb_Reload, "AIBot.Verb.Reload");
	UE_DEFINE_GAMEPLAY_TAG(Verb_WeaponNext, "AIBot.Verb.WeaponNext");
	// ADS (founder, 27 Aug). HELD like Fire — press raises the sights, release drops
	// them; what aiming DOES (spread, speed, descope-on-hit) is entirely the host's.
	UE_DEFINE_GAMEPLAY_TAG(Verb_Aim, "AIBot.Verb.Aim");

	// Grapple traversal (founder, 27 Aug: "climb or drop down"). A PRESS, not a hold —
	// the host's hook is fire-and-forget; what the pull does is entirely the host's.
	UE_DEFINE_GAMEPLAY_TAG(Verb_Grapple, "AIBot.Verb.Grapple");
	UE_DEFINE_GAMEPLAY_TAG(Verb_Dash, "AIBot.Verb.Dash");

	UE_DEFINE_GAMEPLAY_TAG(Ambition_Engage, "AIBot.Ambition.Engage");
	UE_DEFINE_GAMEPLAY_TAG(Ambition_Retreat, "AIBot.Ambition.Retreat");
	UE_DEFINE_GAMEPLAY_TAG(Ambition_Evade, "AIBot.Ambition.Evade");
	UE_DEFINE_GAMEPLAY_TAG(Ambition_Seek, "AIBot.Ambition.Seek");
	UE_DEFINE_GAMEPLAY_TAG(Ambition_Search, "AIBot.Ambition.Search");
	UE_DEFINE_GAMEPLAY_TAG(Ambition_Roam, "AIBot.Ambition.Roam");
	UE_DEFINE_GAMEPLAY_TAG(Ambition_Mode, "AIBot.Ambition.Mode");

	const TArray<FGameplayTag>& HeldVerbs()
	{
		// Built once, on first use, rather than as a namespace-scope array: native tags
		// register at module load and a static initialiser racing that order is exactly
		// the class of bug that produces a silently EMPTY tag. By first call the module
		// is up.
		static const TArray<FGameplayTag> Held = []
		{
			TArray<FGameplayTag> Out;
			Out.Add(Verb_Fire);    // the trigger (Phase 3)
			Out.Add(Verb_Sprint);  // the legs   (Phase 4 - SetSprint's rising/falling edge)
			Out.Add(Verb_Aim);     // the sights (Phase 4 - ADS, founder 27 Aug)
			return Out;
		}();
		return Held;
	}

}
