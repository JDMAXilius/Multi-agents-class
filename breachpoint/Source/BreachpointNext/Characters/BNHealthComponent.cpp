#include "Characters/BNHealthComponent.h"

#include "AbilitySystem/Attributes/BNAttributeSet.h"
#include "AbilitySystem/Abilities/BNGA_HitReact.h"
#include "AbilitySystem/Effects/BNGameplayEffects.h"
#include "Core/AIBBotController.h"
#include "Core/BNGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Pawn.h"

namespace
{
	/** THE AIB DAMAGE SEAM (Phase 5) — the projectile's blast-warning branch's sibling:
	 *  one small AIB notify inside a shared BN file, feeding the bot framework's damage
	 *  ledger for EVERY victim (bot or human), which the adapter alone cannot see. Both
	 *  directions ride one pool-decrease: the victim's own controller learns TAKEN (with
	 *  the attacker, who becomes a matured memory, never a lock), and the attacker's
	 *  controller — when it is an AIB bot — learns DEALT. Fractions are of the victim's
	 *  MaxHealth for both pools: one scale, "damage pressure", exactly how the momentum
	 *  ledger reads it. Authority-only; BN behaviour untouched.
	 */
	void NotifyAIBDamage(UAbilitySystemComponent* ASC, AActor* VictimOwner, float PoolDrop)
	{
		if (!ASC || !VictimOwner || PoolDrop <= 0.f || !ASC->IsOwnerActorAuthoritative())
		{
			return;
		}
		const UBNAttributeSet* Attributes = ASC->GetSet<UBNAttributeSet>();
		if (!Attributes)
		{
			return;
		}
		const float MaxHealth = Attributes->GetMaxHealth();
		if (MaxHealth <= 0.f)
		{
			return;
		}
		const float Fraction = PoolDrop / MaxHealth;
		AActor* Attacker = Attributes->GetLastDamage().Instigator.Get();

		const APawn* VictimPawn = Cast<APawn>(VictimOwner);
		if (AAIBBotController* VictimBot = VictimPawn ? Cast<AAIBBotController>(VictimPawn->GetController()) : nullptr)
		{
			VictimBot->NoteDamageTaken(Attacker, Attacker ? Attacker->GetActorLocation() : FVector::ZeroVector, Fraction);
		}
		// Self-damage never credits the DEALT book (W-REVIEW P4+5 M3): a bot grenading its
		// own feet otherwise records Taken and Dealt in equal measure, its momentum term
		// reads exactly zero, and it feels neutral about blowing itself up. The Taken note
		// above stands — taking your own blast IS damage taken.
		const APawn* AttackerPawn = Attacker != VictimOwner ? Cast<APawn>(Attacker) : nullptr;
		if (AAIBBotController* AttackerBot = AttackerPawn ? Cast<AAIBBotController>(AttackerPawn->GetController()) : nullptr)
		{
			AttackerBot->NoteDamageDealt(Fraction);
		}
	}
}

UBNHealthComponent::UBNHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UBNHealthComponent::InitializeWithAbilitySystem(UAbilitySystemComponent* InASC)
{
	if (!InASC || HealthChangedHandle.IsValid())
	{
		return;
	}

	CachedAbilitySystem = InASC;

	// CHANGES only — the value at registration is deliberately not read. A respawned pawn
	// registers while the persistent ASC still holds the zero that killed the last one, and the
	// init GE lands after; a value check here would kill the new body on the frame it spawned.
	HealthChangedHandle = InASC->GetGameplayAttributeValueChangeDelegate(UBNAttributeSet::GetHealthAttribute())
		.AddUObject(this, &UBNHealthComponent::HandleHealthChanged);

	// The shield gate. Registered on every machine because the delegate is cheap and harmless
	// there; SetShieldRechargeActive is what refuses to act without authority.
	RecentDamageHandle = InASC->RegisterGameplayTagEvent(BNTags::State_Combat_RecentDamage, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &UBNHealthComponent::HandleRecentDamageChanged);

	ShieldChangedHandle = InASC->GetGameplayAttributeValueChangeDelegate(UBNAttributeSet::GetShieldAttribute())
		.AddUObject(this, &UBNHealthComponent::HandleShieldChanged);

	// HEALTH REGEN's two gate tags (founder, 27 Aug). One handler recomputes from both —
	// the window arriving/expiring and death arriving/clearing are the same question:
	// "may this body heal right now".
	HealthRegenDelayHandle = InASC->RegisterGameplayTagEvent(BNTags::State_Combat_HealthRegenDelay, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &UBNHealthComponent::HandleHealthRegenGateChanged);
	DeadTagHandle = InASC->RegisterGameplayTagEvent(BNTags::State_Dead, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &UBNHealthComponent::HandleHealthRegenGateChanged);

	// Start recharging immediately: a pawn that has just spawned has taken no damage, so the tag is
	// absent and the dance's resting state is "coming back". The clamp to MaxShield makes that a
	// no-op at full rather than a runaway.
	SetShieldRechargeActive(ShouldShieldRechargeRun());
	SetHealthRegenActive(ShouldHealthRegenRun());
}

bool UBNHealthComponent::ShouldHealthRegenRun() const
{
	const UAbilitySystemComponent* ASC = CachedAbilitySystem.Get();
	return ASC
		&& !ASC->HasMatchingGameplayTag(BNTags::State_Combat_HealthRegenDelay)
		&& !ASC->HasMatchingGameplayTag(BNTags::State_Dead)
		&& !bDeathReported; // belt for the frame between Health hitting 0 and State.Dead landing
}

bool UBNHealthComponent::ShouldShieldRechargeRun() const
{
	const UAbilitySystemComponent* ASC = CachedAbilitySystem.Get();
	return ASC
		&& !ASC->HasMatchingGameplayTag(BNTags::State_Combat_RecentDamage)
		&& !ASC->HasMatchingGameplayTag(BNTags::State_Dead)
		&& !bDeathReported;
}

void UBNHealthComponent::HandleHealthRegenGateChanged(const FGameplayTag Tag, int32 NewCount)
{
	// The parameter is deliberately unread: whichever tag moved, BOTH answers recompute —
	// this handler hears State.Dead too, and a death mid-window must stop the shield the
	// same way it stops the healing (a window expiring on a corpse starts neither).
	SetHealthRegenActive(ShouldHealthRegenRun());
	SetShieldRechargeActive(ShouldShieldRechargeRun());
	UpdateRegenCues();
}

void UBNHealthComponent::HandleRecentDamageChanged(const FGameplayTag Tag, int32 NewCount)
{
	// Tag up = just been hit, stop. Tag gone = the window expired, start again — unless the
	// body died inside the window (shields ON, 27 Aug: the dead read joined the recompute).
	// The delay itself is UBNGE_RecentDamage's duration, so nothing here counts time.
	SetShieldRechargeActive(ShouldShieldRechargeRun());
	UpdateRegenCues();
}

void UBNHealthComponent::HandleShieldChanged(const FOnAttributeChangeData& Data)
{
	// Shield-absorbed damage never touches Health, so the AIB ledger hears it here; a
	// recharge is an increase and passes through untouched.
	if (Data.NewValue < Data.OldValue)
	{
		NotifyAIBDamage(CachedAbilitySystem.Get(), GetOwner(), Data.OldValue - Data.NewValue);
	}

	// Zero is broken, anything above it is not. Deliberately a THRESHOLD and not an edge: the
	// recharge walks the shield up 10 at a time, so "crossed upward" would need history this does
	// not keep, while a threshold is correct on every write including the first.
	//
	// No pool means nothing to break. Without this, shields-off would leave every player
	// permanently State.Shields.Broken — a tag that is supposed to mean "you are exposed RIGHT NOW"
	// reduced to always-true, which is worse than absent for anything that later reads it.
	SetShieldsBroken(HasShieldPool() && Data.NewValue <= 0.f);

	// Every recharge tick lands here, which is how the cue stops the instant the pool tops out.
	UpdateRegenCues();
}

void UBNHealthComponent::UpdateRegenCues()
{
	const UAbilitySystemComponent* ASC = CachedAbilitySystem.Get();
	const UBNAttributeSet* Attributes = ASC ? ASC->GetSet<UBNAttributeSet>() : nullptr;
	if (!Attributes)
	{
		return;
	}

	// The handle is "the engine is running", the comparison is "it is actually filling something".
	// Both halves, or the cue means nothing: shields sit at full for most of a match with the
	// recharge GE applied the whole time.
	SetRegenCueActive(BNTags::GameplayCue_Character_ShieldRegen, bShieldRegenCueActive,
		ShieldRechargeHandle.IsValid() && Attributes->GetShield() < Attributes->GetMaxShield());
	SetRegenCueActive(BNTags::GameplayCue_Character_HealthRegen, bHealthRegenCueActive,
		HealthRegenHandle.IsValid() && Attributes->GetHealth() < Attributes->GetMaxHealth());
}

void UBNHealthComponent::SetRegenCueActive(const FGameplayTag& CueTag, bool& bCueState, bool bActive)
{
	if (bCueState == bActive)
	{
		return;
	}
	UAbilitySystemComponent* ASC = CachedAbilitySystem.Get();
	if (!ASC || !ASC->IsOwnerActorAuthoritative())
	{
		// The server's alone, like every other write here. It lands on every client through the
		// ASC's cue container and its add/remove multicast — clients do not infer "regenerating"
		// from watching an attribute climb, because a client that missed the start would never
		// see it and a joiner would have no way to know it was already running.
		return;
	}
	bCueState = bActive;
	if (bActive)
	{
		ASC->AddGameplayCue(CueTag);
	}
	else
	{
		ASC->RemoveGameplayCue(CueTag);
	}
}

bool UBNHealthComponent::HasShieldPool() const
{
	const UAbilitySystemComponent* ASC = CachedAbilitySystem.Get();
	return ASC && ASC->GetNumericAttribute(UBNAttributeSet::GetMaxShieldAttribute()) > 0.f;
}

void UBNHealthComponent::SetShieldsBroken(bool bBroken)
{
	UAbilitySystemComponent* ASC = CachedAbilitySystem.Get();
	if (!ASC || !ASC->IsOwnerActorAuthoritative())
	{
		// A GE-applied tag replicates on its own; a client raising its own would be a second,
		// disagreeing source of truth (purity law 5).
		return;
	}

	if (bBroken)
	{
		if (ShieldBrokenHandle.IsValid())
		{
			return;
		}
		const FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UBNGE_State::StaticClass(), 1.f, Context);
		if (Spec.IsValid())
		{
			// Tag on the SPEC — UBNGE_State is the shared infinite carrier and holds no tag of its
			// own, for the construction-order reason its own comment gives.
			Spec.Data->DynamicGrantedTags.AddTag(BNTags::State_Shields_Broken);
			ShieldBrokenHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}
		return;
	}

	if (ShieldBrokenHandle.IsValid())
	{
		ASC->RemoveActiveGameplayEffect(ShieldBrokenHandle);
		ShieldBrokenHandle.Invalidate();
	}
}

void UBNHealthComponent::SetShieldRechargeActive(bool bActive)
{
	// The switch. Off = the recharge GE is never applied, so the shield behaves exactly as it did
	// before this work landed: it drains and stays drained until a respawn.
	if (bActive && !bShieldRechargeEnabled)
	{
		return;
	}

	// And the shields-off case. With MaxShield at 0 there is no pool to refill, and the recharge is
	// an ADD modifier — PreAttributeChange's zero-max branch floors but cannot cap, so an ungated
	// recharge would walk Shield upward forever against a maximum of nothing. Gating here rather
	// than teaching the clamp to tell "deliberately zero" from "not replicated yet", which it
	// genuinely cannot.
	if (bActive && !HasShieldPool())
	{
		return;
	}

	UAbilitySystemComponent* ASC = CachedAbilitySystem.Get();
	if (!ASC || !ASC->IsOwnerActorAuthoritative())
	{
		// The recharge moves an attribute, so it is the server's alone (purity law 1). Clients see
		// Shield replicate up exactly as they see it replicate down.
		return;
	}

	if (bActive)
	{
		if (ShieldRechargeHandle.IsValid())
		{
			return;
		}
		const FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UBNGE_ShieldRecharge::StaticClass(), 1.f, Context);
		if (Spec.IsValid())
		{
			ShieldRechargeHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}
		return;
	}

	if (ShieldRechargeHandle.IsValid())
	{
		ASC->RemoveActiveGameplayEffect(ShieldRechargeHandle);
		ShieldRechargeHandle.Invalidate();
	}
}

void UBNHealthComponent::SetHealthRegenActive(bool bActive)
{
	// The switch, the shield's contract: off = the GE is never applied and health behaves
	// exactly as before this landed — it drains and stays drained until a respawn.
	if (bActive && !bHealthRegenEnabled)
	{
		return;
	}

	// The shield's zero-max gate, mirrored (BN22 W-REVIEW L3): an Add modifier against a
	// MaxHealth that has not been initialised yet has no ceiling — the floor-only clamp
	// branch cannot cap it. Unobservable today (init lands in the same call stack), but
	// the hazard shape is documented and the gate is one line.
	if (bActive)
	{
		const UAbilitySystemComponent* GateASC = CachedAbilitySystem.Get();
		if (!GateASC || GateASC->GetNumericAttribute(UBNAttributeSet::GetMaxHealthAttribute()) <= 0.f)
		{
			return;
		}
	}

	UAbilitySystemComponent* ASC = CachedAbilitySystem.Get();
	if (!ASC || !ASC->IsOwnerActorAuthoritative())
	{
		// An attribute change is the server's alone (purity law 1); clients see Health
		// replicate up exactly as they see it replicate down.
		return;
	}

	if (bActive)
	{
		if (HealthRegenHandle.IsValid())
		{
			return;
		}
		const FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UBNGE_HealthRegen::StaticClass(), 1.f, Context);
		if (Spec.IsValid())
		{
			HealthRegenHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}
		return;
	}

	if (HealthRegenHandle.IsValid())
	{
		ASC->RemoveActiveGameplayEffect(HealthRegenHandle);
		HealthRegenHandle.Invalidate();
	}
}

void UBNHealthComponent::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	// The AIB ledger hears every health drop — the lethal one included, which is why
	// this sits above the survivable/death split. One hit that drains shield AND health
	// fires both handlers with its own pool's drop, so the two notifies SUM to the hit.
	if (Data.NewValue < Data.OldValue)
	{
		NotifyAIBDamage(CachedAbilitySystem.Get(), GetOwner(), Data.OldValue - Data.NewValue);
	}

	// The regen tick that healed this body, or the hit that stopped it — both arrive here.
	UpdateRegenCues();

	if (Data.NewValue > 0.f)
	{
		bDeathReported = false;

		// Survivable damage flinches; lethal damage falls through to death below, which cancels
		// every ability anyway — a corpse must not start a reaction it cannot finish. AUTHORITY
		// only, exactly like the death activation: the GA is ServerOnly and its montage replicates.
		if (Data.NewValue < Data.OldValue)
		{
			if (UAbilitySystemComponent* ASC = CachedAbilitySystem.Get(); ASC && ASC->IsOwnerActorAuthoritative())
			{
				ASC->TryActivateAbilityByClass(UBNGA_HitReact::StaticClass());
			}
		}
		return;
	}

	if (bDeathReported)
	{
		return;
	}
	bDeathReported = true;

	OnDeath.Broadcast(this);
}

void UBNHealthComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HealthChangedHandle.IsValid())
	{
		if (UAbilitySystemComponent* ASC = CachedAbilitySystem.Get())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(UBNAttributeSet::GetHealthAttribute())
				.Remove(HealthChangedHandle);
		}
		HealthChangedHandle.Reset();
	}
	if (RecentDamageHandle.IsValid())
	{
		if (UAbilitySystemComponent* ASC = CachedAbilitySystem.Get())
		{
			ASC->UnregisterGameplayTagEvent(RecentDamageHandle, BNTags::State_Combat_RecentDamage, EGameplayTagEventType::NewOrRemoved);
		}
		RecentDamageHandle.Reset();
	}
	if (ShieldChangedHandle.IsValid())
	{
		if (UAbilitySystemComponent* ASC = CachedAbilitySystem.Get())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(UBNAttributeSet::GetShieldAttribute())
				.Remove(ShieldChangedHandle);
		}
		ShieldChangedHandle.Reset();
	}
	if (HealthRegenDelayHandle.IsValid())
	{
		if (UAbilitySystemComponent* ASC = CachedAbilitySystem.Get())
		{
			ASC->UnregisterGameplayTagEvent(HealthRegenDelayHandle, BNTags::State_Combat_HealthRegenDelay, EGameplayTagEventType::NewOrRemoved);
		}
		HealthRegenDelayHandle.Reset();
	}
	if (DeadTagHandle.IsValid())
	{
		if (UAbilitySystemComponent* ASC = CachedAbilitySystem.Get())
		{
			ASC->UnregisterGameplayTagEvent(DeadTagHandle, BNTags::State_Dead, EGameplayTagEventType::NewOrRemoved);
		}
		DeadTagHandle.Reset();
	}
	// Both pawn-scoped states off the PERSISTENT ASC before the corpse goes: a body that died with
	// its shield down would otherwise hand State.Shields.Broken to the one that replaces it.
	SetShieldsBroken(false);
	// The recharge is this pawn's, not the persistent ASC's: a corpse left recharging would keep
	// filling the shield of the body that replaces it. Same class of leak as the ability-set grant
	// Wave 2 found, and the same cure — the CACHED ASC, because UnPossessed() has already nulled
	// the path back through PlayerState by the time EndPlay runs. The health regen leaks the
	// same way and gets the same cure.
	SetShieldRechargeActive(false);
	SetHealthRegenActive(false);
	// And the cues with them, BEFORE the cached ASC goes: an added cue outlives the pawn on the
	// persistent PlayerState ASC, so a body that died mid-recharge would hand a stuck
	// "regenerating" loop to the one that replaces it — the same leak, one layer up.
	UpdateRegenCues();
	CachedAbilitySystem.Reset();

	Super::EndPlay(EndPlayReason);
}
