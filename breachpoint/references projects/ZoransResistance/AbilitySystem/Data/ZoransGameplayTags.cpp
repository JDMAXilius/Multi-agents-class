// Copyright Zollpa LLC.


#include "ZoransGameplayTags.h"

// Character States
UE_DEFINE_GAMEPLAY_TAG(GameplayTag_Dead, "Character.State.Dead");
UE_DEFINE_GAMEPLAY_TAG(GameplayTag_Stunned, "Character.State.Stunned");
UE_DEFINE_GAMEPLAY_TAG(GameplayTag_Slowed, "Character.State.Slowed");
UE_DEFINE_GAMEPLAY_TAG(GameplayTag_Rooted, "Character.State.Rooted");
UE_DEFINE_GAMEPLAY_TAG(GameplayTag_Aiming, "Character.State.Aiming");
UE_DEFINE_GAMEPLAY_TAG(GameplayTag_Firing, "Character.State.Firing");
UE_DEFINE_GAMEPLAY_TAG(GameplayTag_Crouching, "Character.State.Crouching");
UE_DEFINE_GAMEPLAY_TAG(GameplayTag_JumpAction, "Character.State.JumpAction");
UE_DEFINE_GAMEPLAY_TAG(GameplayTag_BlockKineticCharge, "Character.State.BlockKineticCharge");
UE_DEFINE_GAMEPLAY_TAG(GameplayTag_BlockKineticChargeRegen, "Character.State.BlockKineticChargeRegen");
UE_DEFINE_GAMEPLAY_TAG(GameplayTag_Sprinting, "Character.State.Sprinting");
UE_DEFINE_GAMEPLAY_TAG(GameplayTag_Hologram, "Character.State.Hologram");

// Weapon
UE_DEFINE_GAMEPLAY_TAG(GameplayTag_WeaponCooldown, "Weapon.State.WeaponCooldown");
UE_DEFINE_GAMEPLAY_TAG(GameplayTag_HideWeapon, "Weapon.State.Hidden");

// Set By Caller
UE_DEFINE_GAMEPLAY_TAG(GameplayTag_Damage, "SetByCaller.Damage");
UE_DEFINE_GAMEPLAY_TAG(GameplayTag_Healing, "SetByCaller.Healing");
UE_DEFINE_GAMEPLAY_TAG(GameplayTag_Magnitude, "SetByCaller.Magnitude");

// Ability Filtering
UE_DEFINE_GAMEPLAY_TAG(GameplayTag_ClassAbility1, "Ability.ClassAbility.1");
UE_DEFINE_GAMEPLAY_TAG(GameplayTag_ClassAbility2, "Ability.ClassAbility.2");
UE_DEFINE_GAMEPLAY_TAG(GameplayTag_ClassAbility3, "Ability.ClassAbility.3");
UE_DEFINE_GAMEPLAY_TAG(GameplayTag_ClassAbility4, "Ability.ClassAbility.4");
UE_DEFINE_GAMEPLAY_TAG(GameplayTag_AbilityZ, "Ability.Z");
UE_DEFINE_GAMEPLAY_TAG(GameplayTag_OffensiveAction, "Ability.OffensiveAction");
UE_DEFINE_GAMEPLAY_TAG(GameplayTag_MobilityAction, "Ability.MobilityAction");
UE_DEFINE_GAMEPLAY_TAG(GameplayTag_JumpAbility, "Ability.Jump");
UE_DEFINE_GAMEPLAY_TAG(GameplayTag_ReloadAction, "Ability.ReloadAction");
UE_DEFINE_GAMEPLAY_TAG(GameplayTag_CharacterAbility, "Ability.CharacterSelection");
UE_DEFINE_GAMEPLAY_TAG(GameplayTag_Emote, "Ability.Emote");

// Input
UE_DEFINE_GAMEPLAY_TAG(GameplayTag_HasInput, "Ability.HasInput");
UE_DEFINE_GAMEPLAY_TAG(GameplayTag_ActiveAbility, "Ability.Type.Active");

// Ability Triggers
UE_DEFINE_GAMEPLAY_TAG(GameplayTag_Reload, "Ability.Trigger.Reload");

// Event Triggers
UE_DEFINE_GAMEPLAY_TAG(GameplayTag_Trigger, "Ability.Event.Trigger");
UE_DEFINE_GAMEPLAY_TAG(GameplayTag_Trigger_L, "Ability.Event.Trigger.L");
UE_DEFINE_GAMEPLAY_TAG(GameplayTag_Trigger_R, "Ability.Event.Trigger.R");
UE_DEFINE_GAMEPLAY_TAG(GameplayTag_Release, "Ability.Event.Release");
UE_DEFINE_GAMEPLAY_TAG(GameplayTag_CosmeticWeaponEvent, "Ability.Event.CosmeticWeaponEvent");

// Effect Filtering
UE_DEFINE_GAMEPLAY_TAG(GameplayTag_GrantThrowdown, "Effect.GrantThrowdown");
UE_DEFINE_GAMEPLAY_TAG(GameplayTag_RemoveOnDeath, "Effect.RemoveOnDeath");
UE_DEFINE_GAMEPLAY_TAG(GameplayTag_PersonalShield, "Effect.PersonalShield");
UE_DEFINE_GAMEPLAY_TAG(GameplayTag_ModularZoran, "Filter.ModularZoran");

// Damage
UE_DEFINE_GAMEPLAY_TAG(GameplayTag_DamageReceived, "Character.Event.DamagedReceived");
UE_DEFINE_GAMEPLAY_TAG(GameplayTag_WeakPoint, "Damage.WeakPoint");

namespace DamageType
{
	UE_DEFINE_GAMEPLAY_TAG(DamageType_Default, "DamageType")
	UE_DEFINE_GAMEPLAY_TAG(DamageType_Light, "DamageType.Light")
	UE_DEFINE_GAMEPLAY_TAG(DamageType_Medium, "DamageType.Medium")
	UE_DEFINE_GAMEPLAY_TAG(DamageType_Heavy, "DamageType.Heavy")
	UE_DEFINE_GAMEPLAY_TAG(DamageType_Explosive, "DamageType.Explosive")
	UE_DEFINE_GAMEPLAY_TAG(DamageType_Impale, "DamageType.Impale")
	UE_DEFINE_GAMEPLAY_TAG(DamageType_Throwdown, "DamageType.Throwdown")
}