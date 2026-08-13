#include "Characters/BNHealthComponent.h"

#include "AbilitySystem/Attributes/BNAttributeSet.h"
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

void UBNHealthComponent::SetShieldRechargeActive(bool bActive)
{
	// The switch. Off = the recharge GE is never applied, so the shield behaves exactly as it did
	// before this work landed: it drains and stays drained until a respawn.
	if (bActive && !bShieldRechargeEnabled)
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
	// The recharge is this pawn's, not the persistent ASC's: a corpse left recharging would keep
	// filling the shield of the body that replaces it. Same class of leak as the ability-set grant
	// Wave 2 found, and the same cure — the CACHED ASC, because UnPossessed() has already nulled
	// the path back through PlayerState by the time EndPlay runs.
	SetShieldRechargeActive(false);
	CachedAbilitySystem.Reset();

	Super::EndPlay(EndPlayReason);
}
