// Breachpoint. THE attribute set: shields over health, one damage entry point, one death.

#pragma once

#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "CoreMinimal.h"

#include "BRAttributeSet.generated.h"

struct FGameplayEffectSpec;

/**
 * Fired ONCE per death, on the server, from UBRAttributeSet::PostGameplayEffectExecute.
 *
 * This is the C++ seam gas-purity.md law 7 requires ("events over calls at the seams"): the
 * attribute set never reaches into GameMode, UI, or the pawn. GameMode (BP04) binds this for
 * scoring and the respawn timer; the pawn reacts to the Event.Death gameplay event sent alongside
 * it for ragdoll and cues. Neither is asked to poll a health value.
 *
 * @param Victim     the actor that died (the ASC's owner — the PlayerState, not the pawn).
 * @param Instigator the original instigator of the killing effect. NULL is legal and means
 *                   "environment / no attributable killer"; the receiver must handle it.
 * @param Causer     the effect causer (the weapon/projectile actor). Also legally null.
 * @param KillingSpec the GameplayEffectSpec that landed the killing blow — carries the Damage.*
 *                   tags, so the killfeed reads the cause without a second channel.
 */
DECLARE_MULTICAST_DELEGATE_FourParams(FBROnDeathSignature, AActor* /*Victim*/, AActor* /*Instigator*/, AActor* /*Causer*/, const FGameplayEffectSpec& /*KillingSpec*/);

/**
 * UBRAttributeSet — THE attribute set (BREACHPOINT-ARCHITECTURE.md §3.3). One set, five attributes,
 * and no second set will be added: attribute proliferation is what the named exceptions in
 * docs/contracts/gas-purity.md exist to prevent (ammo is per-weapon state, not an attribute).
 *
 * THE SHAPE OF THE FIGHTER: Shields sit in front of Health and absorb first. Nothing else is
 * modelled here, because nothing else is a per-fighter continuous quantity.
 *
 * THE ONE ENTRY POINT FOR DAMAGE is IncomingDamage — a META attribute. Meta means: never
 * replicated, never persistent, never read by UI, and zero between executions. A damage source
 * does not subtract from Shields or Health; it writes a positive number into IncomingDamage
 * through GE_Damage/BRDamageExecCalc, and THIS class decides what that means. That is what makes
 * "shields absorb first, then health" one rule in one place instead of a rule every weapon
 * re-implements, and it is why gas-purity.md law 1 forbids setter calls anywhere but here.
 *
 * NO GAMEPLAY NUMBER APPEARS IN THIS FILE. There is no starting health, no shield capacity, no
 * regen rate and no headshot multiplier — those are GE_InitStats/CT_Combat data (law 3). The only
 * literals below are structural bounds (a zero floor; "shields are never negative"), which are
 * invariants of the model rather than tuning of it.
 *
 * WHAT RUNS WHERE:
 *   PreAttributeChange       — client AND server, and during prediction REPLAY. Clamps only.
 *                              Never react to gameplay here; a reaction would fire on every replay.
 *   PostGameplayEffectExecute — SERVER ONLY, after an Instant effect has been applied. The one and
 *                              only reaction point: the shields split, the regen gate, the death.
 */
UCLASS(meta = (DisplayName = "BR Attribute Set"))
class BREACHPOINT_API UBRAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UBRAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Clamp current values into their structural bounds. Clamps only — see the class comment. */
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	/** The one reaction point. Server-only by the engine's contract (Instant effects execute on the authority). */
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	// -------------------------------------------------------------------------
	// The attributes
	// -------------------------------------------------------------------------

	/** Current shields. Absorbs damage before Health. COND_None — public bars (§6.1). */
	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|Attributes", ReplicatedUsing = OnRep_Shields)
	FGameplayAttributeData Shields;
	ATTRIBUTE_ACCESSORS_BASIC(UBRAttributeSet, Shields);

	/** Shield capacity. Set by GE_InitStats from data; the clamp ceiling for Shields. */
	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|Attributes", ReplicatedUsing = OnRep_MaxShields)
	FGameplayAttributeData MaxShields;
	ATTRIBUTE_ACCESSORS_BASIC(UBRAttributeSet, MaxShields);

	/** Current health. Reaching zero is the ONLY definition of death in this project. */
	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|Attributes", ReplicatedUsing = OnRep_Health)
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS_BASIC(UBRAttributeSet, Health);

	/** Health capacity. Set by GE_InitStats from data; the clamp ceiling for Health. */
	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|Attributes", ReplicatedUsing = OnRep_MaxHealth)
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS_BASIC(UBRAttributeSet, MaxHealth);

	/**
	 * META. The one entry point for damage — see the class comment.
	 *
	 * NOT a UPROPERTY with Replicated, and that omission is the design: a replicated meta attribute
	 * would send a value that is always zero by the time anyone could read it, and would invite
	 * exactly the "read the damage number off the target" coupling this attribute exists to prevent.
	 * It is consumed and reset to zero inside PostGameplayEffectExecute, every time, without
	 * exception.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|Attributes")
	FGameplayAttributeData IncomingDamage;
	ATTRIBUTE_ACCESSORS_BASIC(UBRAttributeSet, IncomingDamage);

	// -------------------------------------------------------------------------
	// Seams out
	// -------------------------------------------------------------------------

	/** Fired once per death, server-side. See FBROnDeathSignature. */
	FBROnDeathSignature OnDeath;

	/**
	 * True once death has been reported for the current life and until Health rises above zero
	 * again. Server-only, never replicated. Exposed read-only because a spec must be able to assert
	 * the double-death case without reaching into privates.
	 */
	bool HasReportedDeath() const { return bDeathReported; }

protected:
	UFUNCTION()
	void OnRep_Shields(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxShields(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);

private:
	/**
	 * Consume IncomingDamage: shields absorb first, the remainder overflows to health.
	 * @return the amount that reached Health (zero if the shields ate all of it).
	 */
	float ApplyIncomingDamageShieldsFirst(float RawDamage);

	/**
	 * Death detection, called from every path that can lower Health. Fires at most once per life;
	 * re-arms when Health rises above zero (which respawn's GE_InitStats does).
	 */
	void CheckForDeath(const FGameplayEffectModCallbackData& Data);

	/**
	 * Tell the ASC whether shields are down, so it can apply or remove GE_ShieldsBroken — the one
	 * thing that grants `State.Shields.Broken` (law 5: state tags come from GEs, never from here).
	 *
	 * Called from every path that changes Shields: the damage split AND regen. The ASC's setter is
	 * idempotent, so this is a state ASSERTION, not a transition event — which is what makes it
	 * impossible to miss an edge.
	 */
	void UpdateShieldsBrokenState();

	/**
	 * Server-only latch guarding against a double Event.Death — two damage effects landing in the
	 * same frame, or a corpse taking a second hit before GE_Death has been applied.
	 *
	 * A latch rather than a State.Dead tag query on purpose: the tag is applied by whoever REACTS
	 * to Event.Death (GameMode, BP04), so between the event and the tag there is a real window in
	 * which a tag query would answer "not dead yet" and fire a second death. Timing must not decide
	 * this; the latch does not depend on it.
	 */
	bool bDeathReported = false;
};
