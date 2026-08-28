#include "BNGameplayTags.h"

namespace BNTags
{
	UE_DEFINE_GAMEPLAY_TAG(Input_Move, "Input.Move");
	UE_DEFINE_GAMEPLAY_TAG(Input_Look, "Input.Look");
	UE_DEFINE_GAMEPLAY_TAG(Input_Jump, "Input.Jump");
	UE_DEFINE_GAMEPLAY_TAG(Input_Crouch, "Input.Crouch");
	UE_DEFINE_GAMEPLAY_TAG(Input_Weapon_Next, "Input.Weapon.Next");
	UE_DEFINE_GAMEPLAY_TAG(Input_Weapon_Previous, "Input.Weapon.Previous");
	UE_DEFINE_GAMEPLAY_TAG(Input_Weapon_Fire, "Input.Weapon.Fire");
	UE_DEFINE_GAMEPLAY_TAG(Input_Weapon_Reload, "Input.Weapon.Reload");
	UE_DEFINE_GAMEPLAY_TAG(Input_Sprint, "Input.Sprint");
	UE_DEFINE_GAMEPLAY_TAG(Input_Lean_Left, "Input.Lean.Left");
	UE_DEFINE_GAMEPLAY_TAG(Input_Lean_Right, "Input.Lean.Right");
	UE_DEFINE_GAMEPLAY_TAG(Input_Weapon_ADS, "Input.Weapon.ADS");
	UE_DEFINE_GAMEPLAY_TAG(Input_Melee, "Input.Melee");
	UE_DEFINE_GAMEPLAY_TAG(Input_Grenade, "Input.Grenade");

	UE_DEFINE_GAMEPLAY_TAG(State, "State");

	UE_DEFINE_GAMEPLAY_TAG(State_Dead, "State.Dead");
	UE_DEFINE_GAMEPLAY_TAG(State_Movement_Jumping, "State.Movement.Jumping");
	UE_DEFINE_GAMEPLAY_TAG(State_Movement_InAir, "State.Movement.InAir");
	UE_DEFINE_GAMEPLAY_TAG(State_Movement_Crouching, "State.Movement.Crouching");
	UE_DEFINE_GAMEPLAY_TAG(State_Movement_Sprinting, "State.Movement.Sprinting");
	UE_DEFINE_GAMEPLAY_TAG(State_Lean_Left, "State.Lean.Left");
	UE_DEFINE_GAMEPLAY_TAG(State_Lean_Right, "State.Lean.Right");
	UE_DEFINE_GAMEPLAY_TAG(State_Weapon_Reloading, "State.Weapon.Reloading");
	UE_DEFINE_GAMEPLAY_TAG(State_Weapon_ADS, "State.Weapon.ADS");
	UE_DEFINE_GAMEPLAY_TAG(State_Weapon_Firing, "State.Weapon.Firing");
	UE_DEFINE_GAMEPLAY_TAG(State_Weapon_Melee, "State.Weapon.Melee");

	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Weapon_Fire, "Cooldown.Weapon.Fire");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Grenade, "Cooldown.Grenade");
	// GRAPPLE (BN23): the press and its cooldown. Input.Grapple sits beside Jump/Sprint —
	// a body verb, not a weapon's.
	UE_DEFINE_GAMEPLAY_TAG(Input_Grapple, "Input.Grapple");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Grapple, "Cooldown.Grapple");

	UE_DEFINE_GAMEPLAY_TAG(State_Combat_RecentDamage, "State.Combat.RecentDamage");
	// HEALTH REGEN's own window (founder, 27 Aug), a SIBLING of RecentDamage on purpose:
	// a child tag's count propagates into parent queries, so the health window's longer
	// duration would silently extend shield suppression too. Sibling under State.* keeps
	// the respawn sweep's reach.
	UE_DEFINE_GAMEPLAY_TAG(State_Combat_HealthRegenDelay, "State.Combat.HealthRegenDelay");
	UE_DEFINE_GAMEPLAY_TAG(State_Shields_Broken, "State.Shields.Broken");
	UE_DEFINE_GAMEPLAY_TAG(State_Match_Frozen, "State.Match.Frozen");

	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Weapon_MuzzleFlash, "GameplayCue.Weapon.MuzzleFlash");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Weapon_Impact, "GameplayCue.Weapon.Impact");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Weapon_Tracer, "GameplayCue.Weapon.Tracer");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Grenade_Explode, "GameplayCue.Grenade.Explode");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Character_Death, "GameplayCue.Character.Death");

UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Grapple_Fire, "GameplayCue.Grapple.Fire");
UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Grapple_Rope, "GameplayCue.Grapple.Rope");
UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Grapple_Hit, "GameplayCue.Grapple.Hit");

	UE_DEFINE_GAMEPLAY_TAG(Input_Scoreboard, "Input.Scoreboard");
	UE_DEFINE_GAMEPLAY_TAG(Input_Menu, "Input.Menu");
}
