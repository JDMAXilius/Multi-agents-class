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
	BREACHPOINTNEXT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Weapon_ADS);
	BREACHPOINTNEXT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Melee);
	BREACHPOINTNEXT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Grenade);

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
	BREACHPOINTNEXT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Weapon_ADS);
	BREACHPOINTNEXT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Weapon_Firing);
	BREACHPOINTNEXT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Weapon_Melee);

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

	/** Warmup and post-match. The MODE owns it, so the refusal is the server's and every machine
	 *  can read it — a disabled input is a claim the server cannot verify. Under State so respawn's
	 *  State.* sweep cannot carry a stale freeze into a new body. */
	BREACHPOINTNEXT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Match_Frozen);

	BREACHPOINTNEXT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Weapon_MuzzleFlash);
	BREACHPOINTNEXT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Weapon_Impact);
	BREACHPOINTNEXT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Weapon_Tracer);
	BREACHPOINTNEXT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Grenade_Explode);

	/** Death presentation. Executed from the authority's UBNGA_Death so it multicasts — the ability
	 *  is ServerOnly and nothing in it runs on a client, so the cue is the ONLY route by which a
	 *  corpse ragdolls on the machines watching it. */
	BREACHPOINTNEXT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Character_Death);

	/** R7 — a UI verb, NOT an ability: ABNPlayerController handles it BEFORE the ASC forward, so
	 *  holding Tab never logs "NO granted ability carries it". The UI.Layer.* tags are NOT here —
	 *  CommonUI keys its stacks on FUITag, which these macros cannot produce; they live in
	 *  FBNUITags (UI/BNUITypes.h), the second registrar with the second job. */
	BREACHPOINTNEXT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Scoreboard);

	/** R7.2 — opens the pause menu. A UI verb like Scoreboard: the controller handles it and
	 *  the ASC never sees it. CLOSING is the menu's own job (Menu input mode makes a game
	 *  action an unreliable way back). */
	BREACHPOINTNEXT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Menu);
}
