// Breachpoint. The ASC: input-buffered tag activation, prediction-window helpers.

#include "AbilitySystem/BRAbilitySystemComponent.h"

#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameplayEffect.h"
#include "GameplayPrediction.h"

#include "Core/BRCore.h"

UBRAbilitySystemComponent::UBRAbilitySystemComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// §5.4, and it is a decision rather than a default: Mixed sends the OWNER full GameplayEffect
	// replication (so its UI, prediction and rollback are exact) while everyone else receives only
	// tags and cues (so a 4v4 does not pay 8x for effect bookkeeping nobody can see).
	// Minimal is for non-player-shaped AI; our bots sit on PlayerStates and are player-shaped by
	// design (§3.7), so they get Mixed too — one setting, no bot special case.
	SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	// The ASC replicates. Without this the whole mode choice above is decoration.
	SetIsReplicatedByDefault(true);

	// NOT DISABLED, AND THAT IS DELIBERATE — read before "fixing" it for law 4.
	// UAbilitySystemComponent::TickComponent drives ENGINE-internal work (montage replication
	// bookkeeping, target-data cleanup, cue processing). Law 4 bans OUR gameplay from polling; it
	// does not license breaking an engine subsystem's own pump. This class adds no tick work of its
	// own and calls SetShouldTick() nowhere, which is the letter and the spirit both.
}

// ---------------------------------------------------------------------------
// Input — arrow five of §3.2
// ---------------------------------------------------------------------------

void UBRAbilitySystemComponent::AbilityInputTagPressed(FGameplayTag InputTag)
{
	if (!InputTag.IsValid())
	{
		// A row with no tag has no destination. UBRInputConfig::IsDataValid reports the authoring
		// error; here we refuse rather than iterate every spec against an empty tag, which would
		// match nothing on a good day and everything on a bad one.
		UE_LOG(LogBRInput, Warning, TEXT("BRAbilitySystemComponent '%s': AbilityInputTagPressed with an INVALID tag; ignored."),
			*GetNameSafe(GetOwner()));
		return;
	}

	// THE IDEMPOTENCY GATE. Bound to ETriggerEvent::Triggered, this runs every frame the key is
	// held; AddUnique is what turns that stream into a single press edge. See the header for the
	// decided edge case (an ability cancelled while held does not auto-restart).
	const int32 NumBeforeAdd = HeldInputTags.Num();
	HeldInputTags.AddUnique(InputTag);
	if (HeldInputTags.Num() == NumBeforeAdd)
	{
		UE_LOG(LogBRInput, Verbose, TEXT("BRAbilitySystemComponent '%s': %s HELD (repeat Triggered, no edge)."),
			*GetNameSafe(GetOwner()), *InputTag.ToString());
		return;
	}

	UE_LOG(LogBRInput, Log, TEXT("BRAbilitySystemComponent '%s': %s PRESSED (edge) -> matching granted abilities."),
		*GetNameSafe(GetOwner()), *InputTag.ToString());

	// The list must not be reallocated while we hold references into it — activation can grant or
	// revoke abilities re-entrantly. This is the engine's own guard, used the engine's own way.
	ABILITYLIST_SCOPE_LOCK();
	for (FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (!Spec.Ability || !Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			continue;
		}

		Spec.InputPressed = true;

		if (Spec.IsActive())
		{
			// Already running: this is a re-press, not an activation. Tell the live instance, and
			// tell the server if the ability wants raw input replicated (held-fire, charge-ups).
			if (Spec.Ability->bReplicateInputDirectly && !IsOwnerActorAuthoritative())
			{
				ServerSetInputPressed(Spec.Handle);
			}

			AbilitySpecInputPressed(Spec);
			InvokeInputEventForSpec(Spec, EAbilityGenericReplicatedEvent::InputPressed);
		}
		else
		{
			// Not running: try to activate. TryActivateAbility is where costs, cooldowns and
			// blocking tags (State.Dead) are consulted — this function does not second-guess any
			// of them, and must not: a branch here would be a second, divergent rule set.
			TryActivateAbility(Spec.Handle);
		}
	}
}

void UBRAbilitySystemComponent::AbilityInputTagReleased(FGameplayTag InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}

	if (HeldInputTags.Remove(InputTag) == 0)
	{
		// A release with no matching press. Reachable and harmless: the key was already released by
		// ClearAbilityInput on unpossess, or the press landed before this pawn's bindings existed.
		UE_LOG(LogBRInput, Verbose, TEXT("BRAbilitySystemComponent '%s': %s RELEASED but was not held; ignored."),
			*GetNameSafe(GetOwner()), *InputTag.ToString());
		return;
	}

	UE_LOG(LogBRInput, Log, TEXT("BRAbilitySystemComponent '%s': %s RELEASED (edge)."),
		*GetNameSafe(GetOwner()), *InputTag.ToString());

	ABILITYLIST_SCOPE_LOCK();
	for (FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (!Spec.Ability || !Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			continue;
		}

		Spec.InputPressed = false;

		if (Spec.IsActive())
		{
			if (Spec.Ability->bReplicateInputDirectly && !IsOwnerActorAuthoritative())
			{
				ServerSetInputReleased(Spec.Handle);
			}

			// This is what WaitInputRelease listens to, and therefore how every WhileHeld ability
			// (sprint, held fire, grenade cook) ends. If this call is ever lost, those abilities do
			// not error — they simply never stop, which is the failure mode §3.2 warns about.
			AbilitySpecInputReleased(Spec);
			InvokeInputEventForSpec(Spec, EAbilityGenericReplicatedEvent::InputReleased);
		}
	}
}

void UBRAbilitySystemComponent::InvokeInputEventForSpec(const FGameplayAbilitySpec& Spec, EAbilityGenericReplicatedEvent::Type EventType)
{
	// The prediction key must come from the LIVE INSTANCE, not from FGameplayAbilitySpec's own
	// ActivationInfo: that member is UE_DEPRECATED(5.5) and only ever applied to NonInstanced
	// abilities, which no longer exist. Our abilities are InstancedPerActor (§3.3), so the instance
	// is where the activation prediction key actually lives, and using the spec's copy would send a
	// stale or empty key — the shape of bug that makes ONE ability in ten fail to confirm.
	const TArray<UGameplayAbility*> Instances = Spec.GetAbilityInstances();
	if (Instances.IsEmpty() || !Instances.Last())
	{
		// An active spec with no instance. Only reachable for a NonInstanced ability, which this
		// project does not author. Refused rather than guessed at.
		UE_LOG(LogBRInput, Warning, TEXT("BRAbilitySystemComponent '%s': active ability '%s' has no instance; input event not invoked. Non-instanced abilities are not supported."),
			*GetNameSafe(GetOwner()), *GetNameSafe(Spec.Ability));
		return;
	}

	InvokeReplicatedEvent(EventType, Spec.Handle, Instances.Last()->GetCurrentActivationInfoRef().GetActivationPredictionKey());
}

void UBRAbilitySystemComponent::ClearAbilityInput()
{
	if (HeldInputTags.IsEmpty())
	{
		return;
	}

	// Copy first: AbilityInputTagReleased mutates HeldInputTags, and an ability ending inside it can
	// reach further code still. Iterating the live array here would be a use-after-free waiting for
	// the first ability that cancels a sibling on end.
	const TArray<FGameplayTag> TagsToRelease = HeldInputTags;

	UE_LOG(LogBRInput, Log, TEXT("BRAbilitySystemComponent '%s': clearing %d held input tag(s)."),
		*GetNameSafe(GetOwner()), TagsToRelease.Num());

	for (const FGameplayTag& Tag : TagsToRelease)
	{
		AbilityInputTagReleased(Tag);
	}

	HeldInputTags.Reset();
}

// ---------------------------------------------------------------------------
// Prediction-window helpers
// ---------------------------------------------------------------------------

void UBRAbilitySystemComponent::ExecuteInPredictionWindow(TFunctionRef<void()> Work)
{
	// FScopedPredictionWindow is a no-op on the authority (there is nothing to predict when you are
	// the truth) and opens/closes a key on a predicting client. Constructing it unconditionally is
	// what lets ability code stay authority-agnostic.
	FScopedPredictionWindow PredictionWindow(this, /*bCanGenerateNewKey=*/true);
	Work();
}

bool UBRAbilitySystemComponent::BatchRPCTryActivateAbility(FGameplayAbilitySpecHandle AbilityHandle)
{
	// The batcher accumulates the activate/TargetData/end RPCs and flushes them as ONE on scope
	// exit. It only does anything when ShouldDoServerAbilityRPCBatch() is true — which it is, above.
	// An ability that ends itself before this scope closes therefore rides the same packet, which is
	// how the whole optimization is meant to be reached: from inside the ability, not from here.
	// See the header for why the bEndAbilityImmediately parameter is absent until step 3.
	FScopedServerAbilityRPCBatcher Batcher(this, AbilityHandle);

	return TryActivateAbility(AbilityHandle, /*bAllowRemoteActivation=*/true);
}

// ---------------------------------------------------------------------------
// The RecentDamage regen gate
// ---------------------------------------------------------------------------

bool UBRAbilitySystemComponent::ApplyRecentDamageGate()
{
	if (!RecentDamageEffectClass)
	{
		// LOUD, not silent. Until step 4 authors GE_RecentDamage, damage lands and nothing gates
		// regen — a difference a playtester would feel and never be able to name. Warning-level so
		// it survives a default-verbosity log.
		UE_LOG(LogBRCombat, Warning, TEXT("BRAbilitySystemComponent '%s': damage landed but RecentDamageEffectClass is UNSET — State.Combat.RecentDamage was NOT applied and shield regen is UNGATED. Expected until BP02 step 4 authors GE_RecentDamage."),
			*GetNameSafe(GetOwner()));
		return false;
	}

	// Self-applied on the server (this runs from PostGameplayEffectExecute, which is authority
	// only). MakeOutgoingSpec/ApplyGameplayEffectSpecToSelf is the ONE lawful path — nothing here
	// touches a tag directly, because a loose tag would not replicate and the regen gate has to be
	// visible to the GE that blocks on it (gas-purity.md law 5).
	const FGameplayEffectContextHandle Context = MakeEffectContext();
	const FGameplayEffectSpecHandle SpecHandle = MakeOutgoingSpec(RecentDamageEffectClass, /*Level=*/1.f, Context);
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		UE_LOG(LogBRCombat, Error, TEXT("BRAbilitySystemComponent '%s': failed to build a spec for RecentDamageEffectClass '%s'."),
			*GetNameSafe(GetOwner()), *GetNameSafe(RecentDamageEffectClass));
		return false;
	}

	ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	return true;
}
