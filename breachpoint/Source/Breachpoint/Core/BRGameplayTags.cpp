// Breachpoint. All native gameplay tags.

#include "Core/BRGameplayTags.h"

// One definition per declaration in BRGameplayTags.h, same order, same names.
// Tag strings are transcribed from BREACHPOINT-ARCHITECTURE.md §3.1 verbatim.
// The dev comment is what shows in the editor's tag picker.

namespace BRGameplayTags
{
	// --- InputTag.* ---------------------------------------------------------
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Move, "InputTag.Move", "Native action: planar movement, handled by the CMC.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Look, "InputTag.Look", "Native action: look/aim, handled by the controller.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Jump, "InputTag.Jump", "Native action: jump, handled by the CMC.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Crouch, "InputTag.Crouch", "Native action: crouch, handled by the CMC.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Sprint, "InputTag.Sprint", "Ability action: sprint (WhileHeld activation policy).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Fire, "InputTag.Fire", "Ability action: fire the equipped weapon.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Reload, "InputTag.Reload", "Ability action: reload the equipped weapon.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Swap, "InputTag.Swap", "Ability action: swap to the other weapon slot.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Grenade, "InputTag.Grenade", "Ability action: throw a grenade.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Melee, "InputTag.Melee", "Ability action: melee attack.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Grapple, "InputTag.Grapple", "Ability action: grapple.");

	// --- State.* ------------------------------------------------------------
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Shields_Broken, "State.Shields.Broken", "Shields are at zero; applied and removed by GEs only.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Combat_RecentDamage, "State.Combat.RecentDamage", "Damage taken recently; gates shield regen.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Movement_Sprinting, "State.Movement.Sprinting", "Sprinting; read by the CMC for the speed multiplier.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dead, "State.Dead", "Dead; blocks every ability activation through one mechanism.");

	// --- Damage.* -----------------------------------------------------------
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Kinetic, "Damage.Kinetic", "Damage type: hitscan/kinetic weapons.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Explosive, "Damage.Explosive", "Damage type: grenades, rockets, radial sources.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Melee, "Damage.Melee", "Damage type: melee.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Headshot, "Damage.Headshot", "Damage modifier: headshot multiplier in BRDamageExecCalc.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Rear, "Damage.Rear", "Damage modifier: rear arc, validated server-side.");

	// --- SetByCaller.* ------------------------------------------------------
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_BaseDamage, "SetByCaller.BaseDamage", "GE_Damage magnitude.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_RegenRate, "SetByCaller.RegenRate", "GE_Regen periodic magnitude.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_CooldownDuration, "SetByCaller.CooldownDuration", "GE_Cooldown duration magnitude.");

	// --- Event.* ------------------------------------------------------------
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Death, "Event.Death", "Gameplay event: the owner died.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Kill, "Event.Kill", "Gameplay event: the owner got a kill.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Melee_WindowBegin, "Event.Melee.WindowBegin", "Montage notify seam (R17): melee trace window opens.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Melee_WindowEnd, "Event.Melee.WindowEnd", "Montage notify seam (R17): melee trace window closes.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Weapon_ReloadCommit, "Event.Weapon.ReloadCommit", "Montage notify seam (R17): the reload commits.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Weapon_SwapCommit, "Event.Weapon.SwapCommit", "Montage notify seam (R17): the weapon swap commits.");
}
