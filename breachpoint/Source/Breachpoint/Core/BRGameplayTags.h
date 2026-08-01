// Breachpoint. All native gameplay tags.

#pragma once

#include "NativeGameplayTags.h"

/**
 * BRGameplayTags — THE authoritative native gameplay tag declaration for Breachpoint.
 *
 * Specification: BREACHPOINT-ARCHITECTURE.md §3.1, the single authoritative tag list.
 * Every tag below is transcribed from that row; nothing here is invented, and nothing
 * there is abbreviated. Ticket BP01 step 2 closes Core/, so a tag that a later packet
 * needs and does not find here is a contract_gap against §3.1, not a local fix.
 *
 * Conventions
 *  - Variable name == tag string with '.' replaced by '_'. This is what makes the
 *    §3.1 <-> header comparison mechanical rather than a reading exercise.
 *  - Declared here with UE_DECLARE_GAMEPLAY_TAG_EXTERN, defined once in BRGameplayTags.cpp
 *    with UE_DEFINE_GAMEPLAY_TAG_COMMENT. Native tags self-register at module load; they
 *    need no entry in DefaultGameplayTags.ini.
 *  - Extension rule for future montage->gameplay seams (ruling R17): Event.<Verb>.<Moment>.
 *
 * NOTE ON TWO FAMILIES DELIBERATELY LEFT EMPTY — see this file's contract_gap in the
 * BP01 Log. §3.1 names `Ability.*` and `GameplayCue.*` as families but enumerates no
 * leaf tags for either, unlike the other five families. This packet declares exactly
 * what §3.1 enumerates and does not invent leaves. Whoever needs `Ability.Weapon.Fire`
 * or `GameplayCue.Weapon.Fire` must first get §3.1 amended with the enumeration.
 */
namespace BRGameplayTags
{
	// -------------------------------------------------------------------------
	// Ability.*  -- OPEN family (ruling R23): §3.1 names it and enumerates no leaves,
	// because the leaves cannot exist before the abilities do. ONE tag per ability,
	// declared by the packet that authors the ability, appended here in ability order.
	//
	// WHAT AN Ability.* TAG IS FOR, precisely — it is the ability's ASSET tag, and it is
	// the ONLY thing UGameplayAbility::CancelAbilitiesWithTag matches against. §3.3's
	// sentence "BRGA_WeaponFire lists State.Movement.Sprinting in CancelAbilitiesWithTags"
	// does NOT work as written: that field is compared against the target ability's asset
	// tags, and State.Movement.Sprinting is a tag on the ACTOR (granted by activation),
	// not on the ability. Firing cancels sprint by listing `Ability.Sprint` — see
	// BRGA_Sprint.h. Recorded as a BP02 finding against §3.3 rather than worked around.
	// -------------------------------------------------------------------------

	/** BRGA_Sprint's asset tag. Fire/melee/grenade cancel sprint by listing THIS tag. */
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Sprint);

	// -------------------------------------------------------------------------
	// InputTag.*  -- hardware -> tag. Consumed by BRInputConfig/BRInputComponent (step 3)
	// and relayed to the ASC; bots press the same tags on their own ASC.
	// -------------------------------------------------------------------------
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Move);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Look);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Jump);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Crouch);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Sprint);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Fire);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Reload);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Swap);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Grenade);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Melee);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Grapple);

	// -------------------------------------------------------------------------
	// State.*  -- state is a GE-applied tag (gas-purity.md), never a bool on an actor.
	// -------------------------------------------------------------------------
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Shields_Broken);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Combat_RecentDamage);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Movement_Sprinting);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dead);

	// -------------------------------------------------------------------------
	// GameplayCue.*  -- §3.1 names the family; it enumerates NO leaves. See the note above.
	// -------------------------------------------------------------------------

	// -------------------------------------------------------------------------
	// Damage.*  -- dynamic tags read by BRDamageExecCalc alongside SetByCaller.BaseDamage.
	// -------------------------------------------------------------------------
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Kinetic);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Explosive);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Melee);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Headshot);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Rear);

	// -------------------------------------------------------------------------
	// SetByCaller.*  -- the magnitudes that make six generic GEs cover the game.
	// -------------------------------------------------------------------------
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_BaseDamage);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_RegenRate);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_CooldownDuration);

	// -------------------------------------------------------------------------
	// Event.*  -- gameplay events. The four Melee/Weapon entries are the montage ->
	// gameplay notify seam (ruling R17), needed by BP03 and BP05. Nothing consumes them
	// yet and that is correct: an unconsumed native tag is free; a missing one stops a
	// packet months after Core/ closed.
	// -------------------------------------------------------------------------
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Death);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Kill);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Melee_WindowBegin);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Melee_WindowEnd);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Weapon_ReloadCommit);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Weapon_SwapCommit);
}
