// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagContainer.h"
#include "OSAbilitySystemComponent.generated.h"

// ========================================
// INPUT BUFFER (authoritative intent storage)
// ========================================

/** Minimal buffered intent types. Expand later (feint, stance, payload, etc.). */
enum class EOSBufferedInputType : uint8
{
	None,
	ComboContinue,
};

struct FOSBufferedInput
{
	EOSBufferedInputType Type = EOSBufferedInputType::None;
	float TimePressedSeconds = 0.f;

	void Clear()
	{
		Type = EOSBufferedInputType::None;
		TimePressedSeconds = 0.f;
	}
};

class UOSGameplayAbility;
class UGameplayAbility;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAbilityGrantedDelegate, const FGameplayAbilitySpec&, GrantedAbilitySpec);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAbilityRemovedDelegate, const FGameplayAbilitySpec&, RemovedAbilitySpec);

// Native delegates used by C++ systems (combat, etc.). Prefer these over Blueprint delegates for core loops.
DECLARE_MULTICAST_DELEGATE_OneParam(FOnOSGameplayAbilityEnded, UGameplayAbility*);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnOSGameplayAbilityFailed, const UGameplayAbility*, const FGameplayTagContainer&);
/** ASC subclass: allocates FOSGameplayEffectContext, optional ability-granted/removed and ability-ended delegates. */
UCLASS(BlueprintType, Blueprintable, meta=(BlueprintSpawnableComponent))
class ONSIGHT_API UOSAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()
	
	public:
	UOSAbilitySystemComponent();

	/** How long buffered inputs remain valid. */
	UPROPERTY(EditDefaultsOnly, Category="OnSight|InputBuffer")
	float BufferedInputMaxAgeSeconds = 0.25f;

	/** Buffer a single input intent (overwrites previous). */
	void BufferInput(EOSBufferedInputType Type);
	/** Consume buffered intent if still valid. */
	bool ConsumeBufferedInput(EOSBufferedInputType& OutType);
	/** Clear buffered intent. */
	void ClearBufferedInput();
	/** Returns true if buffered intent matches Type and is still within max age. Does not consume. Used e.g. by movement cancel to respect combo-queued priority. */
	bool HasBufferedInput(EOSBufferedInputType Type) const;
	
	/** Remove all owned gameplay tags that match the parent tag (including child tags) */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Gameplay Tags")
	void RemoveTagChildren(FGameplayTag parentTag);
	
	/** Get ability spec by gameplay tag (useful for checking if ability exists, getting cooldown, etc.)
	 * Example usage:
	 *   FGameplayAbilitySpec Spec;
	 *   if (ASC->GetAbilitySpecByTag(AbilityTag, Spec))
	 *   {
	 *       // Ability exists, use Spec
	 *   }
	 */
	UFUNCTION(BlueprintCallable, Category = "Abilities")
	bool GetAbilitySpecByTag(const FGameplayTag& AbilityTag, FGameplayAbilitySpec& OutSpec) const;
	
	/** Check if ability with tag exists
	 * Example usage:
	 *   bool HasAbility = ASC->HasAbilityWithTag(AbilityTag);
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Abilities")
	bool HasAbilityWithTag(const FGameplayTag& AbilityTag) const;
	
	/** Cancel all active abilities (useful on death, stun, etc.)
	 * Example usage in character death:
	 *   void AOSCharacter::OnDeath()
	 *   {
	 *       if (UOSAbilitySystemComponent* ASC = Cast<UOSAbilitySystemComponent>(GetAbilitySystemComponent()))
	 *       {
	 *           ASC->CancelAllAbilities();
	 *       }
	 *   }
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Abilities")
	void CancelAllAbilities();
	
	/** Cancel abilities with specific tags (e.g., cancel all movement abilities)
	 * Example usage:
	 *   FGameplayTagContainer CancelTags;
	 *   CancelTags.AddTag(MovementAbilityTag);
	 *   ASC->CancelAbilitiesWithTags(CancelTags);
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Abilities")
	void CancelAbilitiesWithTags(const FGameplayTagContainer& CancelTags);
	
	/** Delegate fired when an ability is granted
	 * Example usage in UI widget:
	 *   void UOSAbilityWidget::BindToASC(UOSAbilitySystemComponent* ASC)
	 *   {
	 *       ASC->OnAbilityGranted.AddDynamic(this, &UOSAbilityWidget::OnAbilityGranted);
	 *       ASC->OnAbilityRemoved.AddDynamic(this, &UOSAbilityWidget::OnAbilityRemoved);
	 *   }
	 */
	UPROPERTY(BlueprintAssignable, Category = "Abilities")
	FOnAbilityGrantedDelegate OnAbilityGranted;
	
	/** Delegate fired when an ability is removed */
	UPROPERTY(BlueprintAssignable, Category = "Abilities")
	FOnAbilityRemovedDelegate OnAbilityRemoved;

	/** Native callback when an ability ends (server/client). */
	FOnOSGameplayAbilityEnded AbilityEndedCallbacks;

	/** Native callback when an ability fails to activate/commit (best-effort; may be empty tags). */
	FOnOSGameplayAbilityFailed AbilityFailedCallbacks;

protected:
	virtual void OnGiveAbility(FGameplayAbilitySpec& AbilitySpec) override;
	virtual void OnRemoveAbility(FGameplayAbilitySpec& AbilitySpec) override;

private:
	FOSBufferedInput BufferedInput;
};
