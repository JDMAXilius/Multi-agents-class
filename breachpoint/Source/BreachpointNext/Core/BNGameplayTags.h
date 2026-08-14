#pragma once

#include "NativeGameplayTags.h"

namespace BNTags
{
	BREACHPOINTNEXT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Move);
	BREACHPOINTNEXT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Look);
	BREACHPOINTNEXT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Jump);
	BREACHPOINTNEXT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Crouch);
	BREACHPOINTNEXT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Weapon_Next);
	BREACHPOINTNEXT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Weapon_Previous);
	BREACHPOINTNEXT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Weapon_Fire);
	BREACHPOINTNEXT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Weapon_Reload);
	BREACHPOINTNEXT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Sprint);
	BREACHPOINTNEXT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Lean_Left);
	BREACHPOINTNEXT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Lean_Right);
	BREACHPOINTNEXT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Melee);
	BREACHPOINTNEXT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Grenade);
	BREACHPOINTNEXT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Debug_DamageSelf);

	/** The ROOT, and it is load-bearing: respawn sweeps every State.* GE off the persistent ASC
	 *  with one query, so a state tag a dead life left behind cannot reach the next body. */
	BREACHPOINTNEXT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State);

	BREACHPOINTNEXT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dead);
	BREACHPOINTNEXT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Movement_Jumping);
	BREACHPOINTNEXT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Movement_InAir);
	BREACHPOINTNEXT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Movement_Crouching);
	BREACHPOINTNEXT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Movement_Sprinting);
	BREACHPOINTNEXT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Lean_Left);
	BREACHPOINTNEXT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Lean_Right);
	BREACHPOINTNEXT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Weapon_Reloading);

	/** Fire rate. Held by the cooldown GE UBNGA_Fire applies; its duration is the row's FireDelay. */
	BREACHPOINTNEXT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Weapon_Fire);
	BREACHPOINTNEXT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Grenade);

	/** Applied by every landed damage. Blocks shield recharge while it is present — the delay in
	 *  the shield dance is this tag's duration, not a timer anyone hand-runs. */
	BREACHPOINTNEXT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Combat_RecentDamage);

	/** Shield at zero. Named by gas-purity/SKILL.md §5 — "State.Dead/State.Shields.Broken rules
	 *  live in tags, not branches" — so the shield-down state is queryable by UI, audio and
	 *  abilities without any of them reading the attribute and re-deciding what "broken" means. */
	BREACHPOINTNEXT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Shields_Broken);

	BREACHPOINTNEXT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Weapon_MuzzleFlash);
	BREACHPOINTNEXT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Weapon_Impact);
	BREACHPOINTNEXT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Weapon_Tracer);
	BREACHPOINTNEXT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Grenade_Explode);
}
