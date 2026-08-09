// Fill out your copyright notice in the Description page of Project Settings.


#include "GAS/Abilities/GA_OSBlock.h"
#include "Data/OSDefenseAndReactions.h"
#include "GAS/Components/OSAbilitySystemComponent.h"
#include "GAS/Attributes/OSAttributeSet.h"
#include "Data/OSGameplayTags.h"
#include "Data/OSAbilityCostAndEffects.h"
#include "GAS/Effects/GE_OSBlockState.h"


UGA_OSBlock::UGA_OSBlock()
{
	// Set ability tag for identification and cancellation
	const FOSGameplayTags& Tags = FOSGameplayTags::Get();
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(Tags.Ability_Block);
	SetAssetTags(AssetTags);

	// Grant IsBlocking state tag for the ability's lifetime.
	// GAS auto-manages: granted on ActivateAbility, removed on EndAbility/CancelAbility.
	// Read by: ExecCalc_OSDamage (stamina-first damage), regen GEs (IgnoreTags), AnimInstance (CombatState + tag bools),
	// OSAnimNotify_CancelActionBuffer (cancel-window logic).
	// NOTE: Blueprint CDO serialization may override this. If blocking doesn't grant the tag at runtime,
	// verify the Blueprint subclass has Gameplay.State.Guard.IsActive in ActivationOwnedTags.
	ActivationOwnedTags.AddTag(Tags.IsBlocking);

	// GAS CancelAbilitiesWithTag: on commit, engine cancels active abilities whose ability tags match any of these (attack family only; not sprint).
	// NOTE: Blueprint CDO can override this container — keep attack tags in sync if you customize defaults.
	if (Tags.Attack.IsValid()) { CancelAbilitiesWithTag.AddTag(Tags.Attack); }
	if (Tags.Ability_ComboAttack.IsValid()) { CancelAbilitiesWithTag.AddTag(Tags.Ability_ComboAttack); }
	if (Tags.Ability_ComboAttackHeavy.IsValid()) { CancelAbilitiesWithTag.AddTag(Tags.Ability_ComboAttackHeavy); }
	if (Tags.Ability_ChargedAttack.IsValid()) { CancelAbilitiesWithTag.AddTag(Tags.Ability_ChargedAttack); }
	if (Tags.Ability_Sprint.IsValid()) { CancelAbilitiesWithTag.AddTag(Tags.Ability_Sprint); }

	// While blocking, prevent activating attacks and dodge.
	// (Block can still be cancelled via its own input/completion and cancel-window tags.)
	BlockAbilitiesWithTag.Reset();
	if (Tags.Attack.IsValid()) { BlockAbilitiesWithTag.AddTag(Tags.Attack); }
	if (Tags.Ability_Dodge.IsValid()) { BlockAbilitiesWithTag.AddTag(Tags.Ability_Dodge); }

	// Set default drain interval for block (slower than sprint)
	DrainCheckInterval = 1.0f;

	ActivationBlockedTags.AddTag(Tags.IsGrabbed);
	ActivationBlockedTags.AddTag(Tags.IsDodging);

	BlockStateEffectHandle.Invalidate();
	BlockStateEffectClass = UGE_OSBlockState::StaticClass();
}

bool UGA_OSBlock::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)) return false;
	const UOSAttributeSet* AS = OSAttrs();
	if (!IsValid(AS) || AS->GetStamina() <= 0.f)
	{
		return false;
	}
	// Gate before commit so CancelAbilitiesWithTag never runs when block is illegal (GAS cancels on commit).
	if (const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
	{
		const FOSGameplayTags& Tags = FOSGameplayTags::Get();
		if (ASC->HasMatchingGameplayTag(Tags.IsAttacking) && !ASC->HasMatchingGameplayTag(Tags.CanCancel_Block))
		{
			return false;
		}
	}
	return true;
}

FOSBlockInfo UGA_OSBlock::EvaluateBlock()
{
	FOSBlockInfo blockInfo;
	auto* ASC = GetOSAbilitySystemComponent();
	if (!ASC) return blockInfo;

	blockInfo.bIsBlocking = ASC->HasMatchingGameplayTag(IsBlocking);
	blockInfo.bIsBroken = ASC->HasMatchingGameplayTag(IsBroken);

	return blockInfo;
}

void UGA_OSBlock::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// Super may call EndAbility if CommitAbility fails (not enough resources).
	// Without this check, the shield cue and block effect get applied to a dead ability
	// and are never cleaned up — permanent shield VFX stuck on the character.
	if (!IsActive()) return;

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		const FOSGameplayTags& Tags = FOSGameplayTags::Get();
		// Adds the shield bubble actor when blocking is activated.
		if (ActorInfo && Tags.Cue_ShieldBubble.IsValid())
		{
			ASC->AddGameplayCue(Tags.Cue_ShieldBubble);
		}
		ApplyBlockStateEffect();
	}

	if (GuardBreakEffectClass)
	{
		if (UOSAbilitySystemComponent* ASC = GetOSAbilitySystemComponent())
		{
			GuardBreakDelegateHandle =
				ASC->OnActiveGameplayEffectAddedDelegateToSelf.AddUObject(
					this, &UGA_OSBlock::OnEffectAppliedToSelf);
		}
	}

	// Apply periodic resource drain as GameplayEffect
	ApplyPeriodicResourceDrain();

	//auto blockInfo = EvaluateBlock();
	//OSChooser::EvaluateChooserTable(ChooserTable, blockInfo);
}

void UGA_OSBlock::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// Remove periodic resource drain GameplayEffect
	RemovePeriodicResourceDrain();
	RemoveBlockStateEffect();

	// Server Removes shield bubble actor when blocking ends.
	if (ActorInfo )
	{
		UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
		const FOSGameplayTags& Tags = FOSGameplayTags::Get();
		if (Tags.Cue_ShieldBubble.IsValid()&&ASC)
			ASC->RemoveGameplayCue(Tags.Cue_ShieldBubble);
	}

	if (UOSAbilitySystemComponent* ASC = GetOSAbilitySystemComponent())
	{
		if (GuardBreakDelegateHandle.IsValid())
		{
			ASC->OnActiveGameplayEffectAddedDelegateToSelf.Remove(GuardBreakDelegateHandle);
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}


void UGA_OSBlock::OnEffectAppliedToSelf(UAbilitySystemComponent* TargetASC, const FGameplayEffectSpec& SpecApplied,
	FActiveGameplayEffectHandle ActiveHandle)
{
	const UGameplayEffect* Def = SpecApplied.Def.Get();
	if (!Def || !GuardBreakEffectClass)
		return;

	if (Def->GetClass() == GuardBreakEffectClass)
	{
		// Guard break must actually have a consequence — cancel the block ability so the
		// player is dropped out of their defensive stance. Without this, the delegate fires,
		// detects the guard break effect, and returns silently — block mechanics non-functional.
		// EndAbility handles cleanup (periodic drain, block state GE, shield cue, delegate).
		CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateCancelAbility=*/true);

		// ORIGINAL SCAFFOLDING (kept for reference — parked pending hit-info-driven chooser integration):
		// The reaction montage path below referenced FOSBlockInfo + EAttackType that don't yet exist
		// in the damage context. Once FOSHitNetInfo carries the attack type/direction the chooser
		// needs, restore this to play a guard-break reaction montage instead of a silent cancel.
		//temp struct. will be properly analyzed once hit info is implemented
		//FOSBlockInfo blockInfo;
		//blockInfo.bIsBroken = true;
		//blockInfo.AttackType = EAttackType::Light;
		//auto montage = OSChooser::EvaluateChooserTable(ChooserTable, blockInfo);
		//PrepTaskFromMontage(montage, this, "GuardBreak", 1, NAME_None, false);

		return;
	}
}

void UGA_OSBlock::ApplyBlockStateEffect()
{
	UOSAbilitySystemComponent* ASC = GetOSAbilitySystemComponent();
	if (!ASC || BlockStateEffectHandle.IsValid() || !BlockStateEffectClass)
	{
		return;
	}

	FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
	EffectContext.AddSourceObject(this);

	const FOSGameplayTags& Tags = FOSGameplayTags::Get();
	FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(BlockStateEffectClass, 1.0f, EffectContext);
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		return;
	}

	// Drive block multiplier through SetByCaller (authoritative & predictable).
	SpecHandle.Data->SetSetByCallerMagnitude(Tags.Data_Movement_SpeedMultiplier, BlockSpeedMultiplier);

	BlockStateEffectHandle = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());

}

void UGA_OSBlock::RemoveBlockStateEffect()
{
	UOSAbilitySystemComponent* ASC = GetOSAbilitySystemComponent();
	if (!ASC)
	{
		BlockStateEffectHandle.Invalidate();
		return;
	}

	if (BlockStateEffectHandle.IsValid())
	{
		ASC->RemoveActiveGameplayEffect(BlockStateEffectHandle);
		BlockStateEffectHandle.Invalidate();
		return;
	}

	const FOSGameplayTags& Tags = FOSGameplayTags::Get();
	FGameplayTagContainer EffectTags;
	EffectTags.AddTag(Tags.Effect_Movement_Block);
	ASC->RemoveActiveEffectsWithGrantedTags(EffectTags);
}
