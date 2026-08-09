#pragma once
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

/**
 * Native gameplay tag references for OnSight.
 * Tag names and hierarchy are defined in Config/DefaultGameplayTags.ini.
 * Do not add tags here without adding them to that config (or project tag table).
 *
 * Categories: Ability | State | Effect | Event | Cue | Data | Meta
 *
 * Members marked // DELETE are unused in C++ (candidates to drop from this struct; tags may remain in ini/assets).
 */
struct FOSGameplayTags
{
	static const FOSGameplayTags& Get();

	// --- Ability (activation, blocking, cost) ---
	FGameplayTag Ability;
	FGameplayTag Attack;
	FGameplayTag Attack_Light;
	FGameplayTag Attack_Heavy;
	FGameplayTag Ability_Sprint;
	FGameplayTag Ability_Block;
	FGameplayTag Ability_Shield;
	FGameplayTag Ability_HitReaction;
	FGameplayTag Ability_Death; // DELETE
	FGameplayTag Ability_Dodge;
	FGameplayTag Ability_Mantle;
	FGameplayTag Ability_Jump;
	FGameplayTag Ability_ComboAttack;
	FGameplayTag Ability_ComboAttackHeavy;
	FGameplayTag Ability_ChargedAttack;
	FGameplayTag Ability_Grab;
	FGameplayTag Ability_GrabReaction;
	FGameplayTag Ability_Recoil;
	FGameplayTag Ability_LockOn; // DELETE
	FGameplayTag Ability_Magic_FireCone;
	FGameplayTag Ability_Magic_FlameDash;
	FGameplayTag Ability_Magic_FrostBolt;
	FGameplayTag Ability_Melee;

	// --- Ability Slots (input bindings) ---
	FGameplayTag Ability_Slot1;
	FGameplayTag Ability_Slot2;
	FGameplayTag Ability_Slot3;
	FGameplayTag Ability_Slot4;

	// --- State (blocked/granted tags, cooldowns, status) ---
	FGameplayTag IsBlocking;
	FGameplayTag IsAttacking;
	FGameplayTag IsSprinting;
	FGameplayTag IsMantling;
	FGameplayTag IsDead;
	FGameplayTag IsStunned;
	FGameplayTag IsHitReacting;
	FGameplayTag IsKnockedDown;
	FGameplayTag IsGrabbing;
	FGameplayTag IsGrabbed;
	FGameplayTag IsBeingGrabbed;
	FGameplayTag IsCharging;
	FGameplayTag IsDodging;
	FGameplayTag IsRecoiling;
	FGameplayTag IsLockedOn;
	FGameplayTag IsInAir;
	/** Granted with IsInAir via GE_OSInAirState (anim-friendly name). */
	FGameplayTag State_Movement_Airborne;
	/** Granted via GE_OSGroundedState while walking / nav walking. */
	FGameplayTag State_Movement_Grounded;
	/** Short duration from GA_OSJump (server). */
	FGameplayTag State_Movement_Jumping;
	FGameplayTag IsGrounded;
	FGameplayTag IsStaminaBlocked;
	FGameplayTag Attack_Committed;
	FGameplayTag Block_Move; // DELETE
	FGameplayTag Combo_CanQueue;
	FGameplayTag State_InputCancelBuffer;
	FGameplayTag State_Invincibility;
	/** Victim is downed (mid-grab Proned loop or dead-on-grab). Reuses Invincibility for damage rejection. */
	FGameplayTag State_IsDowned;
	FGameplayTag CanCancelRoot;
	FGameplayTag CanCancel_Dodge;
	FGameplayTag CanCancel_Block;
	FGameplayTag CanCancel_Jump;
	FGameplayTag CanCancel_Movement;
	FGameplayTag State_IsStunLock;
	FGameplayTag Status_Buff_Shield;
	FGameplayTag Cooldown_Shield;
	FGameplayTag Cooldown_Dodge; // DELETE
	/** Shared cooldown for all magic melee abilities (one equipped per Grimoire at a time). */
	FGameplayTag Cooldown_Magic_Melee;
	/** Shared cooldown for all magic projectile abilities (one equipped per Grimoire at a time). */
	FGameplayTag Cooldown_Magic_Projectile;
	// --- Attack Phase (phase system tags) ---
	// Replaces: State_InAttackRecovery / "Gameplay.State.Recovery"
	// Old recovery tag → State_Attack_Recovery
	// New additions: Windup and Active phases
	FGameplayTag State_Attack_Windup;   // Gameplay.State.Attack.Windup  (NEW — no old equivalent)
	FGameplayTag State_Attack_Active;   // Gameplay.State.Attack.Active  (NEW — no old equivalent)
	FGameplayTag State_Attack_Recovery;      // Gameplay.State.Attack.Recovery (was: Gameplay.State.Recovery)
	FGameplayTag State_Attack_ComboBuffered; // Gameplay.State.Attack.ComboBuffered — combo input buffered during Windup/Active

	// --- Debug (non-shipping only) ---
	FGameplayTag Debug_NoHealthDamage;
	FGameplayTag Debug_NoStaminaDrain;
	FGameplayTag Debug_BlockHealthRegen;
	FGameplayTag Debug_BlockStaminaRegen;
	// --- Effect (GE tags, application) ---
	FGameplayTag Effect;
	FGameplayTag Effect_ClashKnockback;
	FGameplayTag Effect_Augment;
	FGameplayTag Effect_Regen;
	FGameplayTag Effect_Regen_Health;
	FGameplayTag Effect_Regen_Stamina;
	FGameplayTag Effect_Regen_Aura;
	FGameplayTag Effect_Movement;
	FGameplayTag Effect_Movement_Sprint;
	FGameplayTag Effect_Movement_Block;
	FGameplayTag Effect_LockOn; // DELETE

	// --- Event (HandleGameplayEvent) ---
	FGameplayTag Event_GuardBreak;
	FGameplayTag Event_Recoil;
	FGameplayTag Event_ComboInputPressed;
	FGameplayTag Event_ComboWindowOpened;
	FGameplayTag Event_AttackHit;
	FGameplayTag Event_Montage_Trigger;
	FGameplayTag Event_HitReact;
	FGameplayTag Event_Death;
	FGameplayTag Event_GrabHit;
	/** Attacker's Entry-section active-frame notify. UGA_OSGrab subscribes on activation; receipt runs the confirm trace that promotes Entry → Grab section. Distinct from Event_DirectDamage so damage notify stays on the impact frame, not the trace frame. */
	FGameplayTag Event_Grab_ActiveFrame;
	FGameplayTag Event_LockOn_Pressed; // DELETE
	FGameplayTag Event_LockOn_Cancel; // DELETE
	FGameplayTag Event_LockOn_SwitchRight; // DELETE
	FGameplayTag Event_LockOn_SwitchLeft; // DELETE
	FGameplayTag Event_DirectDamage;
	// Published by GA_OSDodge; reaction abilities (FlameDash etc.) subscribe as AbilityTrigger.
	FGameplayTag Event_Dodge_Started;
	FGameplayTag Event_Dodge_Ended;
	// --- Attack Phase Events (notify → GA communication) ---
	// Replaces: Event_OnAttackRecovery / "GameplayEvent.StateChange.Recovery"
	// Old recovery event → Event_Attack_Phase_Recovery
	// New addition: Active phase event
	FGameplayTag Event_Attack_Phase_Active;   // GameplayEvent.Attack.Phase.Active  (NEW — no old equivalent)
	FGameplayTag Event_Attack_Phase_Recovery; // GameplayEvent.Attack.Phase.Recovery (was: GameplayEvent.StateChange.Recovery)

	// --- Cue (Gameplay Cue Notifies) ---
	FGameplayTag Cue_Flinch;
	FGameplayTag Cue_Magic_Fire_FlameTrail;
	FGameplayTag Cue_ClashKnockback;
	FGameplayTag Cue_Punch; // DELETE
	FGameplayTag Cue_ShieldBubble;

	// --- Cue ~ Sound (Gameplay Cue Notifies) ---
	FGameplayTag Cue_LC_Attack01; // DELETE
	FGameplayTag Cue_LC_Attack02; // DELETE
	FGameplayTag Cue_LC_Attack03; // DELETE
	FGameplayTag Cue_LC_Attack04; // DELETE
	FGameplayTag Cue_HC_Attack01; // DELETE
	FGameplayTag Cue_HC_Attack02; // DELETE
	FGameplayTag Cue_HC_Attack03; // DELETE
	FGameplayTag Cue_Locomotion_FS_Foley; // DELETE
	FGameplayTag Cue_Locomotion_FootstepL; // DELETE
	FGameplayTag Cue_Locomotion_FootstepR; // DELETE
	FGameplayTag Cue_Locomotion_Jump_Land_Idle; // DELETE
	FGameplayTag Cue_Locomotion_Jump_Land_Running; // DELETE

	// --- UI (gameplay events consumed by HUD/widgets) ---
	FGameplayTag UI_TargetChanged;

	// --- Data (SetByCaller magnitude keys) ---
	FGameplayTag Data_Cost_Aura;
	FGameplayTag Data_Cost_Stamina;
	FGameplayTag Data_Cost_Health;
	/** SetByCaller duration (seconds) for UGE_OSGenericCooldown. */
	FGameplayTag Data_Cooldown_Duration;
	/** SetByCaller magnitude; ini tag `Data.Damage`. */
	FGameplayTag Data_Damage;
	/** SetByCaller weak-point / crit-style multiplier (ini `Data.WeakPoint`). */
	FGameplayTag Data_WeakPoint;
	FGameplayTag Data_EffectDuration;
	FGameplayTag Data_HitReact_Type;
	FGameplayTag Data_Movement_SpeedMultiplier;

	// --- Meta (montage/cancel metadata) ---
	FGameplayTag Meta_Cancellable;
	FGameplayTag Meta_Cancellable_Active; // DELETE
	FGameplayTag Meta_Cancels_Input;


private:
	FOSGameplayTags();
};
