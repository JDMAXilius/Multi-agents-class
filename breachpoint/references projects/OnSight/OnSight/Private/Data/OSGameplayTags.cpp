#include "Data/OSGameplayTags.h"

// Tag definitions: Config/DefaultGameplayTags.ini. Keep C++ strings in sync.
// Categories: Ability | State | Effect | Event | Cue | Data | Meta

const FOSGameplayTags& FOSGameplayTags::Get()
{
	static FOSGameplayTags Tags;
	return Tags;
}

// All tags must exist in Config/DefaultGameplayTags.ini or RequestGameplayTag(..., true) will assert.
FOSGameplayTags::FOSGameplayTags()
{
	constexpr bool bErrorIfNotFound = true;

	// --- Ability ---
	Ability = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.Ability"), bErrorIfNotFound);
	Attack = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.Ability.Attack"), bErrorIfNotFound);
	Attack_Light = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.Ability.Attack.Light"), bErrorIfNotFound);
	Attack_Heavy = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.Ability.Attack.Heavy"), bErrorIfNotFound);
	Ability_Sprint = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.Ability.Sprint"), bErrorIfNotFound);
	Ability_Block = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.Ability.Block"), bErrorIfNotFound);
	Ability_Shield = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.Ability.Shield"), bErrorIfNotFound);
	Ability_HitReaction = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.Ability.HitReaction"), bErrorIfNotFound);
	Ability_Death = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.Ability.Death"), bErrorIfNotFound); // DELETE
	Ability_Dodge = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.Ability.Dodge"), bErrorIfNotFound);
	Ability_Mantle = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.Ability.Mantle"), bErrorIfNotFound);
	Ability_Jump = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.Ability.Jump"), bErrorIfNotFound);
	Ability_ComboAttack = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.Ability.ComboAttack"), bErrorIfNotFound);
	Ability_ComboAttackHeavy = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.Ability.ComboAttackHeavy"), bErrorIfNotFound);
	Ability_ChargedAttack = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.Ability.ChargedAttack"), bErrorIfNotFound);
	Ability_Grab = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.Ability.Grab"), bErrorIfNotFound);
	Ability_GrabReaction = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.Ability.GrabReaction"), bErrorIfNotFound);
	Ability_Recoil = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.Ability.Recoil"), bErrorIfNotFound);
	Ability_LockOn = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.Ability.LockOn"), bErrorIfNotFound); // DELETE
	Ability_Magic_FireCone = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.Ability.Magic.FireCone"), bErrorIfNotFound);
	Ability_Magic_FlameDash = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.Ability.Magic.FlameDash"), bErrorIfNotFound);
	Ability_Magic_FrostBolt = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.Ability.Magic.FrostBolt"), bErrorIfNotFound);
	Ability_Melee = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.Ability.Melee"), bErrorIfNotFound);

	// --- Ability Slots ---
	Ability_Slot1 = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.Ability.Slot1"), bErrorIfNotFound);
	Ability_Slot2 = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.Ability.Slot2"), bErrorIfNotFound);
	Ability_Slot3 = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.Ability.Slot3"), bErrorIfNotFound);
	Ability_Slot4 = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.Ability.Slot4"), bErrorIfNotFound);

	// --- State ---
	IsBlocking = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.State.Guard.IsActive"), bErrorIfNotFound);
	IsAttacking = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.State.IsAttacking"), bErrorIfNotFound);
	IsSprinting = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.State.Sprinting"), bErrorIfNotFound);
	IsMantling = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.State.Mantling"), bErrorIfNotFound);
	IsDead = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.State.IsDead"), bErrorIfNotFound);
	IsStunned = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.State.IsStunned"), bErrorIfNotFound);
	IsHitReacting = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.State.IsHitReacting"), bErrorIfNotFound);
	IsKnockedDown = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.State.IsKnockedDown"), bErrorIfNotFound);
	IsGrabbing = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.State.IsGrabbing"), bErrorIfNotFound);
	IsGrabbed = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.State.IsGrabbed"), bErrorIfNotFound);
	IsBeingGrabbed = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.State.IsBeingGrabbed"), bErrorIfNotFound);
	IsCharging = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.State.IsCharging"), bErrorIfNotFound);
	IsDodging = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.State.IsDodging"), bErrorIfNotFound);
	IsRecoiling = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.State.IsRecoiling"), bErrorIfNotFound);
	IsLockedOn = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.State.LockedOn"), bErrorIfNotFound);
	IsInAir = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.State.InAir"), bErrorIfNotFound);
	State_Movement_Airborne = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.State.Movement.Airborne"), bErrorIfNotFound);
	State_Movement_Grounded = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.State.Movement.Grounded"), bErrorIfNotFound);
	State_Movement_Jumping = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.State.Movement.Jumping"), bErrorIfNotFound);
	IsGrounded = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.State.Grounded"), bErrorIfNotFound);
	IsStaminaBlocked = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.State.IsStaminaBlocked"), bErrorIfNotFound);
	Attack_Committed = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.State.CommittedAttack"), bErrorIfNotFound);
	Block_Move = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.State.MovementBlocked"), bErrorIfNotFound); // DELETE
	Combo_CanQueue = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.State.Combo.CanQueue"), bErrorIfNotFound);
	State_InputCancelBuffer = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.State.InputCancelBuffer"), bErrorIfNotFound);
	State_Invincibility = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.State.Invincibility"), bErrorIfNotFound);
	State_IsDowned = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.State.IsDowned"), bErrorIfNotFound);
	CanCancelRoot = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.State.CanCancel"), bErrorIfNotFound);
	CanCancel_Dodge = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.State.CanCancel.Dodge"), bErrorIfNotFound);
	CanCancel_Block = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.State.CanCancel.Block"), bErrorIfNotFound);
	CanCancel_Jump = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.State.CanCancel.Jump"), bErrorIfNotFound);
	CanCancel_Movement = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.State.CanCancel.Movement"), bErrorIfNotFound);
	State_IsStunLock = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.State.IsStunLock"), bErrorIfNotFound);
	Status_Buff_Shield = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.State.Buff.Shield"), bErrorIfNotFound);
	Cooldown_Shield = FGameplayTag::RequestGameplayTag(TEXT("Cooldown.Shield"), bErrorIfNotFound);
	Cooldown_Dodge = FGameplayTag::RequestGameplayTag(TEXT("Cooldown.Dodge"), bErrorIfNotFound); // DELETE
	Cooldown_Magic_Melee = FGameplayTag::RequestGameplayTag(TEXT("Cooldown.Magic.Melee"), bErrorIfNotFound);
	Cooldown_Magic_Projectile = FGameplayTag::RequestGameplayTag(TEXT("Cooldown.Magic.Projectile"), bErrorIfNotFound);
	// --- Attack Phase (replaces legacy Gameplay.State.Recovery) ---
	State_Attack_Windup = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.State.Attack.Windup"), bErrorIfNotFound);
	State_Attack_Active = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.State.Attack.Active"), bErrorIfNotFound);
	State_Attack_Recovery = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.State.Attack.Recovery"), bErrorIfNotFound);
	State_Attack_ComboBuffered = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.State.Attack.ComboBuffered"), bErrorIfNotFound);

	// --- Debug ---
	Debug_NoHealthDamage = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.Debug.NoHealthDamage"), bErrorIfNotFound);
	Debug_NoStaminaDrain = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.Debug.NoStaminaDrain"), bErrorIfNotFound);
	Debug_BlockHealthRegen = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.Debug.BlockHealthRegen"), bErrorIfNotFound);
	Debug_BlockStaminaRegen = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.Debug.BlockStaminaRegen"), bErrorIfNotFound);
	// --- Effect ---
	Effect = FGameplayTag::RequestGameplayTag(TEXT("GameplayEffect"), bErrorIfNotFound);
	Effect_ClashKnockback = FGameplayTag::RequestGameplayTag(TEXT("GameplayEffect.Clash.KnockBack"), bErrorIfNotFound);
	Effect_Augment = FGameplayTag::RequestGameplayTag(TEXT("GameplayEffect.Augment"), bErrorIfNotFound);
	Effect_Regen = FGameplayTag::RequestGameplayTag(TEXT("GameplayEffect.Regen"), bErrorIfNotFound);
	Effect_Regen_Health = FGameplayTag::RequestGameplayTag(TEXT("GameplayEffect.Regen.Health"), bErrorIfNotFound);
	Effect_Regen_Stamina = FGameplayTag::RequestGameplayTag(TEXT("GameplayEffect.Regen.Stamina"), bErrorIfNotFound);
	Effect_Regen_Aura = FGameplayTag::RequestGameplayTag(TEXT("GameplayEffect.Regen.Aura"), bErrorIfNotFound);
	Effect_Movement = FGameplayTag::RequestGameplayTag(TEXT("GameplayEffect.Movement"), bErrorIfNotFound);
	Effect_Movement_Sprint = FGameplayTag::RequestGameplayTag(TEXT("GameplayEffect.Movement.Sprint"), bErrorIfNotFound);
	Effect_Movement_Block = FGameplayTag::RequestGameplayTag(TEXT("GameplayEffect.Movement.Block"), bErrorIfNotFound);
	Effect_LockOn = FGameplayTag::RequestGameplayTag(TEXT("GameplayEffect.LockOn"), bErrorIfNotFound); // DELETE

	// --- Event ---
	Event_GuardBreak = FGameplayTag::RequestGameplayTag(TEXT("GameplayEvent.Effect.GuardBreak"), bErrorIfNotFound);
	Event_Recoil = FGameplayTag::RequestGameplayTag(TEXT("GameplayEvent.Effect.Recoil"), bErrorIfNotFound);
	Event_ComboInputPressed = FGameplayTag::RequestGameplayTag(TEXT("GameplayEvent.ComboInputPressed"), bErrorIfNotFound);
	Event_ComboWindowOpened = FGameplayTag::RequestGameplayTag(TEXT("GameplayEvent.ComboWindow.Opened"), bErrorIfNotFound);
	Event_AttackHit = FGameplayTag::RequestGameplayTag(TEXT("GameplayEvent.AttackHit"), bErrorIfNotFound);
	Event_Montage_Trigger = FGameplayTag::RequestGameplayTag(TEXT("GameplayEvent.Montage.Trigger"), bErrorIfNotFound);
	Event_HitReact = FGameplayTag::RequestGameplayTag(TEXT("GameplayEvent.HitReact"), bErrorIfNotFound);
	Event_Death = FGameplayTag::RequestGameplayTag(TEXT("GameplayEvent.Death"), bErrorIfNotFound);
	Event_GrabHit = FGameplayTag::RequestGameplayTag(TEXT("GameplayEvent.GrabHit"), bErrorIfNotFound);
	Event_Grab_ActiveFrame = FGameplayTag::RequestGameplayTag(TEXT("GameplayEvent.Grab.ActiveFrame"), bErrorIfNotFound);
	Event_LockOn_Pressed = FGameplayTag::RequestGameplayTag(TEXT("GameplayEvent.LockOn.Pressed"), bErrorIfNotFound); // DELETE
	Event_LockOn_Cancel = FGameplayTag::RequestGameplayTag(TEXT("GameplayEvent.LockOn.Cancel"), bErrorIfNotFound); // DELETE
	Event_LockOn_SwitchRight = FGameplayTag::RequestGameplayTag(TEXT("GameplayEvent.LockOn.SwitchRight"), bErrorIfNotFound); // DELETE
	Event_LockOn_SwitchLeft = FGameplayTag::RequestGameplayTag(TEXT("GameplayEvent.LockOn.SwitchLeft"), bErrorIfNotFound); // DELETE
	Event_DirectDamage = FGameplayTag::RequestGameplayTag(TEXT("GameplayEvent.DirectDamage"), bErrorIfNotFound);
	Event_Dodge_Started = FGameplayTag::RequestGameplayTag(TEXT("GameplayEvent.Dodge.Started"), bErrorIfNotFound);
	Event_Dodge_Ended = FGameplayTag::RequestGameplayTag(TEXT("GameplayEvent.Dodge.Ended"), bErrorIfNotFound);
	// --- Attack Phase Events (replaces legacy GameplayEvent.StateChange.Recovery) ---
	Event_Attack_Phase_Active = FGameplayTag::RequestGameplayTag(TEXT("GameplayEvent.Attack.Phase.Active"), bErrorIfNotFound);
	Event_Attack_Phase_Recovery = FGameplayTag::RequestGameplayTag(TEXT("GameplayEvent.Attack.Phase.Recovery"), bErrorIfNotFound);

	// --- Cue ---
	Cue_Flinch = FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.Flinch"), bErrorIfNotFound);
	Cue_Magic_Fire_FlameTrail = FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.Magic.Fire.FlameTrail"), bErrorIfNotFound);
	Cue_ClashKnockback = FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.Clash.Knockback"), bErrorIfNotFound);
	Cue_Punch = FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.Notify.Punch"), bErrorIfNotFound); // DELETE
	Cue_ShieldBubble = FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.ShieldBubble"), bErrorIfNotFound);

	// --- Cue ~ Sound ---
	Cue_LC_Attack01 = FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.Sound.Player.Combat.Light.Attack01"), bErrorIfNotFound); // DELETE
	Cue_LC_Attack02 = FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.Sound.Player.Combat.Light.Attack02"), bErrorIfNotFound); // DELETE
	Cue_LC_Attack03 = FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.Sound.Player.Combat.Light.Attack03"), bErrorIfNotFound); // DELETE
	Cue_LC_Attack04 = FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.Sound.Player.Combat.Light.Attack04"), bErrorIfNotFound); // DELETE
	Cue_HC_Attack01 = FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.Sound.Player.Combat.Heavy.Attack01"), bErrorIfNotFound); // DELETE
	Cue_HC_Attack02 = FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.Sound.Player.Combat.Heavy.Attack02"), bErrorIfNotFound); // DELETE
	Cue_HC_Attack03 = FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.Sound.Player.Combat.Heavy.Attack03"), bErrorIfNotFound); // DELETE
	Cue_Locomotion_FS_Foley = FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.Sound.Player.Locomotion.Foley"), bErrorIfNotFound); // DELETE
	Cue_Locomotion_FootstepL = FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.Sound.Player.Locomotion.FootstepL"), bErrorIfNotFound); // DELETE
	Cue_Locomotion_FootstepR = FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.Sound.Player.Locomotion.FootstepR"), bErrorIfNotFound); // DELETE
	Cue_Locomotion_Jump_Land_Idle = FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.Sound.Player.Locomotion.JumpLand.Idle"), bErrorIfNotFound); // DELETE
	Cue_Locomotion_Jump_Land_Running = FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.Sound.Player.Locomotion.JumpLand.Running"), bErrorIfNotFound); // DELETE

	// --- UI ---
	UI_TargetChanged = FGameplayTag::RequestGameplayTag(TEXT("GameplayEvent.UI.TargetChanged"), bErrorIfNotFound);

	// --- Data ---
	Data_Cost_Health = FGameplayTag::RequestGameplayTag(TEXT("Data.Cost.Health"), bErrorIfNotFound);
	Data_Cost_Aura = FGameplayTag::RequestGameplayTag(TEXT("Data.Cost.Aura"), bErrorIfNotFound);
	Data_Cost_Stamina = FGameplayTag::RequestGameplayTag(TEXT("Data.Cost.Stamina"), bErrorIfNotFound);
	Data_Cooldown_Duration = FGameplayTag::RequestGameplayTag(TEXT("Data.Cooldown.Duration"), bErrorIfNotFound);
	Data_Damage = FGameplayTag::RequestGameplayTag(TEXT("Data.Damage"), bErrorIfNotFound);
	Data_WeakPoint = FGameplayTag::RequestGameplayTag(TEXT("Data.WeakPoint"), bErrorIfNotFound);
	Data_EffectDuration = FGameplayTag::RequestGameplayTag(TEXT("Data.EffectDuration"), bErrorIfNotFound);
	Data_HitReact_Type = FGameplayTag::RequestGameplayTag(TEXT("Data.HitReact.Type"), bErrorIfNotFound);
	Data_Movement_SpeedMultiplier = FGameplayTag::RequestGameplayTag(TEXT("Data.Movement.SpeedMultiplier"), bErrorIfNotFound);

	// --- Meta ---
	Meta_Cancellable = FGameplayTag::RequestGameplayTag(TEXT("Meta.Cancellable"), bErrorIfNotFound);
	Meta_Cancellable_Active = FGameplayTag::RequestGameplayTag(TEXT("Meta.Cancellable.Active"), bErrorIfNotFound); // DELETE
	Meta_Cancels_Input = FGameplayTag::RequestGameplayTag(TEXT("Meta.Cancels.InputBuffer"), bErrorIfNotFound);
}
