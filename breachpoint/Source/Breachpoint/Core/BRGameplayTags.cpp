#include "Core/BRGameplayTags.h"

namespace BRGameplayTags
{
	// ---- InputTag.* ------------------------------------------------------------------
	UE_DEFINE_GAMEPLAY_TAG(InputTag_Move, "InputTag.Move");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_Look, "InputTag.Look");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_Jump, "InputTag.Jump");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_Crouch, "InputTag.Crouch");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_Sprint, "InputTag.Sprint");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_Weapon_Fire, "InputTag.Weapon.Fire");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_Weapon_Reload, "InputTag.Weapon.Reload");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_Weapon_Swap, "InputTag.Weapon.Swap");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_Grenade, "InputTag.Grenade");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_Melee, "InputTag.Melee");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_Grapple, "InputTag.Grapple");

	// ---- Ability.* -------------------------------------------------------------------
	UE_DEFINE_GAMEPLAY_TAG(Ability_Sprint, "Ability.Sprint");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Jump, "Ability.Jump");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Melee, "Ability.Melee");
	UE_DEFINE_GAMEPLAY_TAG(Ability_WeaponFire, "Ability.WeaponFire");
	UE_DEFINE_GAMEPLAY_TAG(Ability_WeaponUtility, "Ability.WeaponUtility");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Grenade, "Ability.Grenade");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Grapple, "Ability.Grapple");

	// ---- State.* ---------------------------------------------------------------------
	UE_DEFINE_GAMEPLAY_TAG(State_Dead, "State.Dead");
	UE_DEFINE_GAMEPLAY_TAG(State_Movement_Sprinting, "State.Movement.Sprinting");
	UE_DEFINE_GAMEPLAY_TAG(State_Shields_Broken, "State.Shields.Broken");
	UE_DEFINE_GAMEPLAY_TAG(State_Combat_RecentDamage, "State.Combat.RecentDamage");

	// ---- payload ---------------------------------------------------------------------
	UE_DEFINE_GAMEPLAY_TAG(Damage_Kinetic, "Damage.Kinetic");
	UE_DEFINE_GAMEPLAY_TAG(Damage_Explosive, "Damage.Explosive");
	UE_DEFINE_GAMEPLAY_TAG(Damage_Melee, "Damage.Melee");
	UE_DEFINE_GAMEPLAY_TAG(Damage_Melee_Rear, "Damage.Melee.Rear");
	UE_DEFINE_GAMEPLAY_TAG(Damage_Headshot, "Damage.Headshot");

	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_BaseDamage, "SetByCaller.BaseDamage");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_RegenRate, "SetByCaller.RegenRate");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_CooldownDuration, "SetByCaller.CooldownDuration");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_Cost, "SetByCaller.Cost");

	UE_DEFINE_GAMEPLAY_TAG(Event_Death, "Event.Death");
	UE_DEFINE_GAMEPLAY_TAG(Event_Kill, "Event.Kill");
	UE_DEFINE_GAMEPLAY_TAG(Event_Melee_WindowBegin, "Event.Melee.WindowBegin");
	UE_DEFINE_GAMEPLAY_TAG(Event_Melee_WindowEnd, "Event.Melee.WindowEnd");
	UE_DEFINE_GAMEPLAY_TAG(Event_Weapon_ReloadCommit, "Event.Weapon.ReloadCommit");
	UE_DEFINE_GAMEPLAY_TAG(Event_Weapon_SwapCommit, "Event.Weapon.SwapCommit");

	// ---- COMPAT (see header — deleted with their last pre-rework consumer) -----------
	UE_DEFINE_GAMEPLAY_TAG(Ability_Weapon_Fire, "Ability.Weapon.Fire");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Weapon_Reload, "Ability.Weapon.Reload");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Weapon_Swap, "Ability.Weapon.Swap");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_Fire, "InputTag.Fire");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_Reload, "InputTag.Reload");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_Swap, "InputTag.Swap");
	UE_DEFINE_GAMEPLAY_TAG(Damage_Rear, "Damage.Rear");
	UE_DEFINE_GAMEPLAY_TAG(State_Cooldown, "State.Cooldown");
	UE_DEFINE_GAMEPLAY_TAG(State_Movement_Grappling, "State.Movement.Grappling");
	UE_DEFINE_GAMEPLAY_TAG(State_Weapon_Reloading, "State.Weapon.Reloading");
	UE_DEFINE_GAMEPLAY_TAG(State_Weapon_Swapping, "State.Weapon.Swapping");
	UE_DEFINE_GAMEPLAY_TAG(State_Combat_Meleeing, "State.Combat.Meleeing");
	UE_DEFINE_GAMEPLAY_TAG(State_Combat_ThrowingGrenade, "State.Combat.ThrowingGrenade");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Weapon_AR_Fire, "GameplayCue.Weapon.AR.Fire");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Weapon_Magnum_Fire, "GameplayCue.Weapon.Magnum.Fire");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Weapon_Rocket_Fire, "GameplayCue.Weapon.Rocket.Fire");
}
