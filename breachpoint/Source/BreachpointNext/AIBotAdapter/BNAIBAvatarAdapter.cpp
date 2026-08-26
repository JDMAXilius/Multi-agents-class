#include "AIBotAdapter/BNAIBAvatarAdapter.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/Attributes/BNAttributeSet.h"
#include "AbilitySystem/BNAbilitySystemComponent.h"
#include "Characters/BNCharacter.h"
#include "Core/AIBBotController.h"
#include "Core/AIBTags.h"
#include "Core/BNGameplayTags.h"
#include "BreachpointNext.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Match/BNPlayerState.h"
#include "Weapons/BNEquipmentComponent.h"
#include "Weapons/BNWeapon.h"

void UBNAIBAvatarAdapter::EnsureOn(ABNCharacter* Character, AController* Possessor)
{
	if (!Character || !Possessor || !Possessor->IsA<AAIBBotController>())
	{
		return;
	}
	if (Character->FindComponentByClass<UBNAIBAvatarAdapter>())
	{
		return;
	}
	UBNAIBAvatarAdapter* Adapter = NewObject<UBNAIBAvatarAdapter>(Character, TEXT("AIBAvatarAdapter"));
	Adapter->RegisterComponent();
}

FGameplayTag UBNAIBAvatarAdapter::MapVerb(FGameplayTag VerbTag)
{
	// The map IS the seam: the module speaks AIBot.Verb.*, this game listens on
	// Input.* — and only the eight the seam audit proved the input path accepts.
	if (VerbTag == AIBTags::Verb_Fire)       { return BNTags::Input_Weapon_Fire; }
	if (VerbTag == AIBTags::Verb_Jump)       { return BNTags::Input_Jump; }
	if (VerbTag == AIBTags::Verb_Crouch)     { return BNTags::Input_Crouch; }
	if (VerbTag == AIBTags::Verb_Sprint)     { return BNTags::Input_Sprint; }
	if (VerbTag == AIBTags::Verb_Melee)      { return BNTags::Input_Melee; }
	if (VerbTag == AIBTags::Verb_Grenade)    { return BNTags::Input_Grenade; }
	if (VerbTag == AIBTags::Verb_Reload)     { return BNTags::Input_Weapon_Reload; }
	if (VerbTag == AIBTags::Verb_WeaponNext) { return BNTags::Input_Weapon_Next; }
	return FGameplayTag();
}

UBNAbilitySystemComponent* UBNAIBAvatarAdapter::GetASC() const
{
	// The audited chain: pawn -> PlayerState -> the one ASC that outlives the body.
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const ABNPlayerState* PS = Pawn ? Pawn->GetPlayerState<ABNPlayerState>() : nullptr;
	return PS ? PS->GetBNAbilitySystemComponent() : nullptr;
}

ABNWeapon* UBNAIBAvatarAdapter::GetHeldWeapon() const
{
	const ABNCharacter* Character = Cast<ABNCharacter>(GetOwner());
	UBNEquipmentComponent* Equipment = Character ? Character->GetEquipmentComponent() : nullptr;
	return Equipment ? Equipment->GetCurrentWeapon() : nullptr;
}

void UBNAIBAvatarAdapter::PressVerb(FGameplayTag VerbTag)
{
	// The server-only gate lives HERE, per the audit: the ASC does not gate presses.
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	const FGameplayTag InputTag = MapVerb(VerbTag);
	if (!InputTag.IsValid())
	{
		// The module's contract: an unmapped verb is a no-op, logged once — never a crash.
		UE_LOG(LogBN, Verbose, TEXT("BNAIB: unmapped verb %s pressed into nothing."), *VerbTag.ToString());
		return;
	}
	if (UBNAbilitySystemComponent* ASC = GetASC())
	{
		ASC->AbilityInputTagPressed(InputTag);
	}
}

void UBNAIBAvatarAdapter::ReleaseVerb(FGameplayTag VerbTag)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	const FGameplayTag InputTag = MapVerb(VerbTag);
	if (!InputTag.IsValid())
	{
		return;
	}
	if (UBNAbilitySystemComponent* ASC = GetASC())
	{
		ASC->AbilityInputTagReleased(InputTag);
	}
}

float UBNAIBAvatarAdapter::GetHealthNorm() const
{
	// The interface contract: 1 when unknowable — a missing ASC must not read as dying.
	const UBNAbilitySystemComponent* ASC = GetASC();
	if (!ASC)
	{
		return 1.f;
	}
	const float MaxHealth = ASC->GetNumericAttribute(UBNAttributeSet::GetMaxHealthAttribute());
	if (MaxHealth <= 0.f)
	{
		return 1.f;
	}
	return FMath::Clamp(ASC->GetNumericAttribute(UBNAttributeSet::GetHealthAttribute()) / MaxHealth, 0.f, 1.f);
}

float UBNAIBAvatarAdapter::GetAmmoNorm() const
{
	const ABNWeapon* Weapon = GetHeldWeapon();
	if (!Weapon)
	{
		return 0.f;
	}
	const int32 MagazineSize = Weapon->GetMagazineSize();
	if (MagazineSize <= 0)
	{
		return 0.f;
	}
	return FMath::Clamp(static_cast<float>(Weapon->GetCurrentAmmo()) / MagazineSize, 0.f, 1.f);
}

bool UBNAIBAvatarAdapter::HasReserveAmmo() const
{
	const ABNWeapon* Weapon = GetHeldWeapon();
	return Weapon && Weapon->GetAmmoReserve() > 0;
}

bool UBNAIBAvatarAdapter::CanWeaponFight() const
{
	// The assembled four-read answer, transcribed from the fire ability's own gates —
	// the recipe the brain must not know, in the one place allowed to know it.
	const ABNWeapon* Weapon = GetHeldWeapon();
	if (!Weapon || (!Weapon->HasAmmo() && Weapon->GetAmmoReserve() <= 0))
	{
		return false;
	}
	const UBNAbilitySystemComponent* ASC = GetASC();
	if (!ASC)
	{
		return false;
	}
	return !ASC->HasMatchingGameplayTag(BNTags::Cooldown_Weapon_Fire)
		&& !ASC->HasMatchingGameplayTag(BNTags::State_Weapon_Reloading)
		&& !ASC->HasMatchingGameplayTag(BNTags::State_Weapon_Melee)
		&& !ASC->HasMatchingGameplayTag(BNTags::State_Dead)
		&& !ASC->HasMatchingGameplayTag(BNTags::State_Match_Frozen);
}

int32 UBNAIBAvatarAdapter::GetGrenadeCount() const
{
	const UBNAbilitySystemComponent* ASC = GetASC();
	return ASC ? FMath::FloorToInt32(ASC->GetNumericAttribute(UBNAttributeSet::GetGrenadesAttribute())) : 0;
}

bool UBNAIBAvatarAdapter::IsGrounded() const
{
	// Movement truth, never a tag: the InAir tag provably misses a walk-off (the audit).
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const UCharacterMovementComponent* Move = Character ? Character->GetCharacterMovement() : nullptr;
	return Move ? !Move->IsFalling() : true;
}

bool UBNAIBAvatarAdapter::IsAlive() const
{
	const UBNAbilitySystemComponent* ASC = GetASC();
	return ASC && !ASC->HasMatchingGameplayTag(BNTags::State_Dead);
}

float UBNAIBAvatarAdapter::GetHealthNormOf(const AActor* Other) const
{
	// FAIRPLAY ruling (26 Aug): the facts builder must NOT call this for live targets.
	// It exists for the adapter's own future uses (the damage-derived estimate).
	const UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(const_cast<AActor*>(Other));
	if (!ASC)
	{
		return 1.f;
	}
	const float MaxHealth = ASC->GetNumericAttribute(UBNAttributeSet::GetMaxHealthAttribute());
	if (MaxHealth <= 0.f)
	{
		return 1.f;
	}
	return FMath::Clamp(ASC->GetNumericAttribute(UBNAttributeSet::GetHealthAttribute()) / MaxHealth, 0.f, 1.f);
}

bool UBNAIBAvatarAdapter::IsAliveTarget(const AActor* Other) const
{
	// The audited no-cast door plus the one tag that means dead.
	const UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(const_cast<AActor*>(Other));
	return ASC && !ASC->HasMatchingGameplayTag(BNTags::State_Dead);
}
