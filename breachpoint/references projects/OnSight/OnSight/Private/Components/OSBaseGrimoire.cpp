// Fill out your copyright notice in the Description page of Project Settings.

#include "Components/OSBaseGrimoire.h"
#include "AbilitySystemComponent.h"
#include "GameplayAbilitySpecHandle.h"
#include "ActiveGameplayEffectHandle.h"
#include "Characters/OSCharacter.h"
#include "GAS/Abilities/OSGameplayAbility.h"

UOSBaseGrimoire::UOSBaseGrimoire()
	: Quadrant(EOSGrimoireQuadrant::Pressure)
	, AuraColor(FLinearColor::White)
	, CurrentAbilityLevel(0)
	, bIsEquipped(false)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	AuraTierThresholds = { 0.f, 0.25f, 0.5f, 1.f };
}

void UOSBaseGrimoire::BeginPlay()
{
	Super::BeginPlay();
}

void UOSBaseGrimoire::Equip()
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
		return;

	if (bIsEquipped)
		return;

	UAbilitySystemComponent* ASC = GetASC();
	if (!ASC)
		return;

	bIsEquipped = true;

	ApplyEffects(BaseStatEffects);

	if (MeleeAbilityClass)
	{
		FGameplayAbilitySpec Spec(MeleeAbilityClass, 1, INDEX_NONE, Owner);
		MeleeHandle = ASC->GiveAbility(Spec);
	}

	if (ProjectileAbilityClass)
	{
		FGameplayAbilitySpec Spec(ProjectileAbilityClass, 1, INDEX_NONE, Owner);
		ProjectileHandle = ASC->GiveAbility(Spec);
	}

	CurrentAbilityLevel = 1;
	SetAbilityLevels(CurrentAbilityLevel);

	GrantPassivesUpToTier(0);

	OnEquipped();
}

void UOSBaseGrimoire::Unequip()
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
		return;

	if (!bIsEquipped)
		return;

	OnUnequipped();

	UAbilitySystemComponent* ASC = GetASC();
	if (ASC)
	{
		if (MeleeHandle.IsValid())
			ASC->ClearAbility(MeleeHandle);

		if (ProjectileHandle.IsValid())
			ASC->ClearAbility(ProjectileHandle);
	}

	MeleeHandle = FGameplayAbilitySpecHandle();
	ProjectileHandle = FGameplayAbilitySpecHandle();

	RevokeAllPassives();
	RemoveAllAppliedEffects();

	CurrentAbilityLevel = 0;
	bIsEquipped = false;
}

void UOSBaseGrimoire::UpdateAura(float AuraPercent)
{
	if (!bIsEquipped)
		return;

	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
		return;

	int32 NewLevel = CalcAbilityLevel(AuraPercent);
	if (NewLevel == CurrentAbilityLevel)
		return;

	int32 OldLevel = CurrentAbilityLevel;
	CurrentAbilityLevel = NewLevel;
	SetAbilityLevels(NewLevel);

	int32 OldTier = OldLevel - 1;
	int32 NewTier = NewLevel - 1;

	if (NewTier > OldTier)
		GrantPassivesUpToTier(NewTier);
	else
		RevokePassivesAboveTier(NewTier);

	OnAbilityLevelChanged(OldLevel, NewLevel);
}

// --- Blueprint Native Events (default implementations) ---

void UOSBaseGrimoire::OnAbilityLevelChanged_Implementation(int32 OldLevel, int32 NewLevel)
{
}

void UOSBaseGrimoire::OnEquipped_Implementation()
{
}

void UOSBaseGrimoire::OnUnequipped_Implementation()
{
}

AOSCharacter* UOSBaseGrimoire::GetOwningCharacter() const
{
	return Cast<AOSCharacter>(GetOwner());
}

UAbilitySystemComponent* UOSBaseGrimoire::GetASC() const
{
	AOSCharacter* Character = GetOwningCharacter();
	if (!Character)
		return nullptr;
	return Character->GetAbilitySystemComponent();
}

int32 UOSBaseGrimoire::CalcAbilityLevel(float AuraPercent) const
{
	int32 Level = 1;
	for (int32 i = AuraTierThresholds.Num() - 1; i >= 0; --i)
	{
		if (AuraPercent >= AuraTierThresholds[i])
		{
			Level = i + 1;
			break;
		}
	}
	return Level;
}

void UOSBaseGrimoire::SetAbilityLevels(int32 NewLevel)
{
	UAbilitySystemComponent* ASC = GetASC();
	if (!ASC)
		return;

	FGameplayAbilitySpecHandle* Handles[] = { &MeleeHandle, &ProjectileHandle };
	for (FGameplayAbilitySpecHandle* Handle : Handles)
	{
		if (!Handle->IsValid())
			continue;

		FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(*Handle);
		if (Spec)
		{
			Spec->Level = NewLevel;
			ASC->MarkAbilitySpecDirty(*Spec);
		}
	}
}

void UOSBaseGrimoire::GrantPassivesUpToTier(int32 TierIndex)
{
	UAbilitySystemComponent* ASC = GetASC();
	if (!ASC)
		return;

	int32 AlreadyGranted = PassiveHandles.Num();

	for (int32 i = AlreadyGranted; i <= TierIndex && i < PassiveAbilities.Num(); ++i)
	{
		if (!PassiveAbilities[i])
		{
			PassiveHandles.Add(FGameplayAbilitySpecHandle());
			continue;
		}

		FGameplayAbilitySpec Spec(PassiveAbilities[i], 1, INDEX_NONE, GetOwner());
		PassiveHandles.Add(ASC->GiveAbility(Spec));
	}
}

void UOSBaseGrimoire::RevokePassivesAboveTier(int32 TierIndex)
{
	UAbilitySystemComponent* ASC = GetASC();
	if (!ASC)
		return;

	int32 KeepCount = TierIndex + 1;

	for (int32 i = PassiveHandles.Num() - 1; i >= KeepCount; --i)
	{
		if (PassiveHandles[i].IsValid())
			ASC->ClearAbility(PassiveHandles[i]);

		PassiveHandles.RemoveAt(i);
	}
}

void UOSBaseGrimoire::RevokeAllPassives()
{
	UAbilitySystemComponent* ASC = GetASC();
	if (!ASC)
		return;

	for (FGameplayAbilitySpecHandle& Handle : PassiveHandles)
	{
		if (Handle.IsValid())
			ASC->ClearAbility(Handle);
	}
	PassiveHandles.Empty();
}

void UOSBaseGrimoire::ApplyEffects(const TArray<TSubclassOf<UGameplayEffect>>& Effects)
{
	UAbilitySystemComponent* ASC = GetASC();
	if (!ASC)
		return;

	for (const TSubclassOf<UGameplayEffect>& EffectClass : Effects)
	{
		if (!EffectClass)
			continue;

		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		Context.AddSourceObject(GetOwner());

		FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(EffectClass, 1, Context);
		if (SpecHandle.IsValid())
		{
			FActiveGameplayEffectHandle ActiveHandle = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
			if (ActiveHandle.IsValid())
				EffectHandles.Add(ActiveHandle);
		}
	}
}

void UOSBaseGrimoire::RemoveAllAppliedEffects()
{
	UAbilitySystemComponent* ASC = GetASC();
	if (!ASC)
		return;

	for (FActiveGameplayEffectHandle& Handle : EffectHandles)
	{
		if (Handle.IsValid())
			ASC->RemoveActiveGameplayEffect(Handle);
	}
	EffectHandles.Empty();
}
