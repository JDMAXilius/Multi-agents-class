#pragma once

#include "NativeGameplayTags.h"

/**
 * The module's whole vocabulary. VERBS are what the bot can DO — the adapter maps each to
 * whatever the game's input surface calls it (FAIRPLAY F6: the bot acts only through the
 * verb surface). AMBITIONS are what the bot can WANT — identities for the utility layer;
 * game modes contribute more under AIBot.Ambition.Mode via IAIBAmbitionProvider.
 *
 * The verb list is the seam-audit's proven set: the eight actions the audited host's
 * input path accepts from a bot today. A verb with no game mapping is pressed into
 * nothing and logged once — never a crash.
 *
 * THE EXTENSION DOOR (W-REVIEW M4): this list is a floor, not a ceiling. The namespace
 * is open — a HOST may define its own native tags under AIBot.Verb.* in its own module
 * (tag registration is global, not linker-scoped) and reference them from executor data
 * and its adapter map. A ninth action (a grapple, an equipment slot) therefore never
 * forces an edit inside this module; the dependency arrow holds.
 */
namespace AIBTags
{
	// -- verbs (pressed / released) ---------------------------------------------
	AIBOT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Verb_Fire);
	AIBOT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Verb_Jump);
	AIBOT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Verb_Crouch);
	AIBOT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Verb_Sprint);
	AIBOT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Verb_Melee);
	AIBOT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Verb_Grenade);
	AIBOT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Verb_Reload);
	AIBOT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Verb_WeaponNext);

	// -- ambitions (the core set; modes add their own) ----------------------------
	AIBOT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ambition_Engage);
	AIBOT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ambition_Retreat);
	AIBOT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ambition_SeekWeapon);
	AIBOT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ambition_Search);
	AIBOT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ambition_Roam);
	AIBOT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ambition_Mode);
}
