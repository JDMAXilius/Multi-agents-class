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
 * NOTE ON THE TWO OPEN FAMILIES. §3.1 names `Ability.*` and `GameplayCue.*` as families
 * but enumerates no leaf tags for either, unlike the other five families. That is not an
 * omission: the leaves cannot exist at BP01 time because the abilities and cues do not
 * exist yet.
 *
 * **Ruling R23 governs: these two families are OPEN.** The packet that authors an ability
 * or a cue declares its own tag here, under an exact-file `owner_path` grant to this file
 * and its `.cpp` — never a grant to the `Core/` folder. The other five families are CLOSED;
 * a packet needing a new `Event.*`, `State.*`, `Damage.*`, `InputTag.*` or `SetByCaller.*`
 * tag files a `contract_gap` and stops.
 *
 * *Corrected 1 Aug 2026.* This paragraph previously read "must first get §3.1 amended with
 * the enumeration" — written before R23 and left stale, so it contradicted the `Ability.*`
 * block twelve lines below, which already cited R23 as OPEN. BP15 step 4 quoted this half
 * and filed a `contract_gap` against BP01 that R23 says does not exist. **No §3.1 amendment
 * is required for a leaf in these two families.**
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

	/**
	 * BRGA_WeaponFire's asset tag (BP03). Declared by the packet that authors the ability,
	 * per R23. Firing cancels sprint by listing `Ability.Sprint` in CancelAbilitiesWithTags;
	 * this tag is what a FUTURE ability would list to cancel firing.
	 */
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Weapon_Fire);

	/**
	 * `UBRGA_Reload` and `UBRGA_WeaponSwap`'s asset tags (BP03). TWO abilities, one file pair
	 * (`BRGA_WeaponUtility.h`) per ARCHITECTURE §3.3's "two tiny sibling abilities, one pair" —
	 * the same one-header-many-classes shape as the GE library, and the same reason: split, the
	 * shared pattern becomes invisible.
	 *
	 * They are separate ABILITIES rather than one with a branch because each is granted against
	 * its own `InputTag` (`InputTag.Reload` / `InputTag.Swap`), and an ability set row maps one
	 * input tag to one ability class.
	 */
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Weapon_Reload);
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Weapon_Swap);

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
	// GameplayCue.*  -- OPEN family (ruling R23). §3.1 names it and enumerates no leaves;
	// the packet that authors the cue declares its tag here, one per cue.
	//
	// THESE THREE WERE ALREADY NAMED BY DATA BEFORE ANY C++ DECLARED THEM. `DT_Weapons.csv`
	// has carried a `FireCueTag` column since the data crew landed it on 29 Jul 2026 --
	// with verifier PASS -- holding exactly these three strings. The verifier checked the
	// CSV's schema, not whether the symbols it names exist on the other side, so a
	// cross-artifact dangling reference passed every gate for three days (BP15 step 4's
	// finding). Declaring them here closes it; the durable fix is a validator that resolves
	// tag-valued CSV columns against the native tag registry, filed in BP03's Log.
	//
	// The leaf is per-WEAPON, not per-ability: one fire ability plays a different cue for
	// the AR, the Magnum and the Rocket, chosen by the weapon row -- which is why the tag
	// travels in data and not in the ability's C++.
	// -------------------------------------------------------------------------

	/** AR fire FX. Named by DT_Weapons.csv row `AR`, column `FireCueTag`. */
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Weapon_AR_Fire);

	/** Magnum fire FX. Named by DT_Weapons.csv row `Magnum`, column `FireCueTag`. */
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Weapon_Magnum_Fire);

	/** Rocket fire FX. Named by DT_Weapons.csv row `Rocket`, column `FireCueTag`. */
	BREACHPOINT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Weapon_Rocket_Fire);

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
