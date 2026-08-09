// Fill out your copyright notice in the Description page of Project Settings.

#include "Animations/Notifies/OSAnimNotify_CancelActionBuffer.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Data/OSGameplayTags.h"
#include "GAS/Components/OSAbilitySystemComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

// TO BE DELETED
UOSAnimNotify_CancelActionBuffer::UOSAnimNotify_CancelActionBuffer()
{
}

void UOSAnimNotify_CancelActionBuffer::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	if (!MeshComp) return;

	UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(MeshComp->GetOwner());
	if (!IsValid(ASC)) return;

	const FOSGameplayTags& Tags = FOSGameplayTags::Get();

	if (bOnlyCancelIfInDodgeBlockJump)
	{
		const bool bIsDodging = Tags.IsDodging.IsValid() && ASC->HasMatchingGameplayTag(Tags.IsDodging);
		const bool bIsBlocking = Tags.IsBlocking.IsValid() && ASC->HasMatchingGameplayTag(Tags.IsBlocking);
		bool bIsJumping = false;
		if (bTreatFallingAsJumping)
		{
			if (const ACharacter* Char = Cast<ACharacter>(MeshComp->GetOwner()))
			{
				if (const UCharacterMovementComponent* Move = Char->GetCharacterMovement())
				{
					bIsJumping = Move->IsFalling();
				}
			}
		}

		// Only cancel if we're currently in one of the cancel actions.
		if (!bIsDodging && !bIsBlocking && !bIsJumping)
		{
			return;
		}
	}

	if (bCancelAllAttacks)
	{
		// Cancel by tags (no hard dependency on GA classes).
		FGameplayTagContainer CancelTags;
		if (Tags.Attack.IsValid()) CancelTags.AddTag(Tags.Attack);
		if (Tags.Ability_ComboAttack.IsValid()) CancelTags.AddTag(Tags.Ability_ComboAttack);
		if (Tags.Ability_ComboAttackHeavy.IsValid()) CancelTags.AddTag(Tags.Ability_ComboAttackHeavy);
		if (Tags.Ability_ChargedAttack.IsValid()) CancelTags.AddTag(Tags.Ability_ChargedAttack);
		if (Tags.Attack_Light.IsValid()) CancelTags.AddTag(Tags.Attack_Light);
		if (Tags.Attack_Heavy.IsValid()) CancelTags.AddTag(Tags.Attack_Heavy);
		if (!CancelTags.IsEmpty())
		{
			ASC->CancelAbilities(&CancelTags, nullptr, nullptr);
		}
	}

	if (bDisableComboBuffer && Tags.Combo_CanQueue.IsValid())
		ASC->RemoveLooseGameplayTag(Tags.Combo_CanQueue);

	// Clear buffered intent (if enabled)
	if (bClearComboInput)
	{
		if (UOSAbilitySystemComponent* OSASC = Cast<UOSAbilitySystemComponent>(ASC))
		{
			OSASC->ClearBufferedInput();
		}
	}
}
