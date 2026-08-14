#pragma once

#include "NativeGameplayTags.h"

// BP91: the entire tag vocabulary. Four families — InputTag, Ability, State, payload
// (Damage / SetByCaller / Event). A tag not declared here does not exist; no
// RequestGameplayTag-by-string anywhere in the module. Extension rule for the
// montage→gameplay seam stays `Event.<Verb>.<Moment>` (R17).
namespace BRGameplayTags
{
	// ---- InputTag.* ------------------------------------------------------------------
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Move);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Look);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Jump);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Crouch);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Sprint);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Weapon_Fire);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Weapon_Reload);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Weapon_Swap);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Grenade);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Melee);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Grapple);

	// ---- Ability.* — one per ability class in the §8 roadmap -------------------------
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Sprint);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Jump);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Melee);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_WeaponFire);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_WeaponUtility);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Grenade);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Grapple);

	// ---- State.* ---------------------------------------------------------------------
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dead);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Movement_Sprinting);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Shields_Broken);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Combat_RecentDamage);

	// ---- payload: Damage.* / SetByCaller.* / Event.* ---------------------------------
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Kinetic);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Explosive);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Melee);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Melee_Rear);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Headshot);

	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_BaseDamage);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_RegenRate);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_CooldownDuration);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Cost);

	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Death);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Kill);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Melee_WindowBegin);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Melee_WindowEnd);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Weapon_ReloadCommit);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Weapon_SwapCommit);

	// ---- COMPAT (pre-rework consumers only — NOT vocabulary) -------------------------
	// Every tag below is still referenced by code outside Core/ that BP93–BP101 will
	// rewrite (AbilitySystem/, Equipment/, Match/, AI/, Tests/). Kept so BP91 does not
	// break the build from Core/; each is deleted with its last consumer. Do not add
	// new references to these.
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Weapon_Fire);      // → Ability_WeaponFire
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Weapon_Reload);    // → Ability_WeaponUtility
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Weapon_Swap);      // → Ability_WeaponUtility
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Fire);            // → InputTag_Weapon_Fire
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Reload);          // → InputTag_Weapon_Reload
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Swap);            // → InputTag_Weapon_Swap
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Rear);              // → Damage_Melee_Rear
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Cooldown);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Movement_Grappling);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Weapon_Reloading);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Weapon_Swapping);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Combat_Meleeing);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Combat_ThrowingGrenade);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Weapon_AR_Fire);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Weapon_Magnum_Fire);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Weapon_Rocket_Fire);
}
