#include "Characters/BNHealthComponent.h"

#include "AbilitySystem/Attributes/BNAttributeSet.h"
#include "AbilitySystem/Abilities/BNGA_HitReact.h"
#include "AbilitySystem/Effects/BNGameplayEffects.h"
#include "Core/BNGameplayTags.h"
#include "AbilitySystemComponent.h"

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

	// Start recharging immediately: a pawn that has just spawned has taken no damage, so the tag is
	// absent and the dance's resting state is "coming back". The clamp to MaxShield makes that a
	// no-op at full rather than a runaway.
	SetShieldRechargeActive(!InASC->HasMatchingGameplayTag(BNTags::State_Combat_RecentDamage));
}

void UBNHealthComponent::HandleRecentDamageChanged(const FGameplayTag Tag, int32 NewCount)
{
	// Tag up = just been hit, stop. Tag gone = the window expired, start again. The delay itself is
	// UBNGE_RecentDamage's duration, so nothing here counts time.
	SetShieldRechargeActive(NewCount <= 0);
}

void UBNHealthComponent::HandleShieldChanged(const FOnAttributeChangeData& Data)
{
	// Zero is broken, anything above it is not. Deliberately a THRESHOLD and not an edge: the
	// recharge walks the shield up 10 at a time, so "crossed upward" would need history this does
	// not keep, while a threshold is correct on every write including the first.
	//
	// No pool means nothing to break. Without this, shields-off would leave every player
	// permanently State.Shields.Broken — a tag that is supposed to mean "you are exposed RIGHT NOW"
	// reduced to always-true, which is worse than absent for anything that later reads it.
	SetShieldsBroken(HasShieldPool() && Data.NewValue <= 0.f);
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

void UBNHealthComponent::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
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
	// Both pawn-scoped states off the PERSISTENT ASC before the corpse goes: a body that died with
	// its shield down would otherwise hand State.Shields.Broken to the one that replaces it.
	SetShieldsBroken(false);
	// The recharge is this pawn's, not the persistent ASC's: a corpse left recharging would keep
	// filling the shield of the body that replaces it. Same class of leak as the ability-set grant
	// Wave 2 found, and the same cure — the CACHED ASC, because UnPossessed() has already nulled
	// the path back through PlayerState by the time EndPlay runs.
	SetShieldRechargeActive(false);
	CachedAbilitySystem.Reset();

	Super::EndPlay(EndPlayReason);
}
