// Breachpoint. All native gameplay tags.

#include "Core/BRGameplayTags.h"

// One definition per declaration in BRGameplayTags.h, same order, same names.
// Tag strings are transcribed from BREACHPOINT-ARCHITECTURE.md §3.1 verbatim.
// The dev comment is what shows in the editor's tag picker.

namespace BRGameplayTags
{
	// --- Ability.* (open family, R23) ---------------------------------------
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Sprint, "Ability.Sprint", "BRGA_Sprint's asset tag; what CancelAbilitiesWithTag must list to end a sprint.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Weapon_Fire, "Ability.Weapon.Fire", "BRGA_WeaponFire's asset tag (BP03); what a future ability would list to cancel firing.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Weapon_Reload, "Ability.Weapon.Reload", "UBRGA_Reload's asset tag (BP03); firing cancels a reload by listing THIS tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Weapon_Swap, "Ability.Weapon.Swap", "UBRGA_WeaponSwap's asset tag (BP03); firing cancels a swap by listing THIS tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Grapple, "Ability.Grapple", "UBRGA_Grapple's asset tag AND its cooldown tag (BP06); the cooldown is a predicted GE so it rolls back on rejection.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Melee, "Ability.Melee", "UBRGA_Melee's asset tag (BP05); firing cancels a melee by listing THIS tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Grenade, "Ability.Grenade", "UBRGA_Grenade's asset tag (BP05); declared natively so the constructor cannot race the tag registry.");

	// --- GameplayCue.* (open family, R23) ------------------------------------
	// The three strings below are byte-for-byte what DT_Weapons.csv's FireCueTag column
	// already contains. If either side is edited, both must move together -- see BP03's Log.
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Weapon_AR_Fire, "GameplayCue.Weapon.AR.Fire", "AR fire FX; named by DT_Weapons.csv row AR.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Weapon_Magnum_Fire, "GameplayCue.Weapon.Magnum.Fire", "Magnum fire FX; named by DT_Weapons.csv row Magnum.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Weapon_Rocket_Fire, "GameplayCue.Weapon.Rocket.Fire", "Rocket fire FX; named by DT_Weapons.csv row Rocket.");

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
