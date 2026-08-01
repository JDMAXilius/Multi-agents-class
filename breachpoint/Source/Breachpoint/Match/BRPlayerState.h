// Breachpoint. The PlayerState: the ASC and the attribute set live here.

#pragma once

#include "AbilitySystemInterface.h"
#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"

#include "BRPlayerState.generated.h"

class UBRAbilitySystemComponent;
class UBRAttributeSet;

/**
 * ABRPlayerState — BREACHPOINT-ARCHITECTURE.md §3.6: "**ASC + set live here**".
 *
 * WHY THE PLAYERSTATE AND NOT THE PAWN, stated once so nobody moves it back: the pawn is destroyed
 * on every death. An ASC on the pawn takes attributes, granted abilities, grant handles, active
 * effects and cooldowns with it — so respawn would have to re-grant a loadout and re-apply
 * cooldowns from some other record, and that record would become a second source of truth for
 * everything GAS already tracks. The PlayerState survives death and travel; put the ASC there and
 * "abilities survive respawn" stops being a feature and becomes a property of the object graph.
 * The only thing that changes on respawn is the AVATAR, which ABRCharacter re-points via
 * InitAbilityActorInfo.
 *
 * WHAT THIS FILE OWNS TODAY: the ASC, the attribute set, and the replication frequency decision.
 *
 * WHAT §3.6 STILL OWES IT, AND NOT FROM THIS PACKET: TeamID and K/D/A (BP04's match meta — a named
 * exception in gas-purity.md's ledger, server-mutated in GameMode and replicated from here; NOT
 * attributes, because match bookkeeping is not combat simulation and must never be predicted).
 */
UCLASS(meta = (DisplayName = "BR Player State"))
class BREACHPOINT_API ABRPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ABRPlayerState(const FObjectInitializer& ObjectInitializer);

	// -------------------------------------------------------------------------
	// IAbilitySystemInterface — the real one. ABRCharacter forwards to this.
	// -------------------------------------------------------------------------

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	/** The same component, already typed. This is what ABRPlayerController's input relay calls. */
	UBRAbilitySystemComponent* GetBRAbilitySystemComponent() const { return AbilitySystemComponent; }

	/**
	 * THE attribute set, read-only.
	 *
	 * Const on purpose and not negotiable: a non-const handle is an invitation to call SetHealth()
	 * from gameplay code, which is gas-purity.md law 1's exact prohibition. Everything that changes
	 * an attribute does it with a GameplayEffect; everything that READS one is welcome here.
	 */
	const UBRAttributeSet* GetBRAttributeSet() const { return AttributeSet; }

	virtual void PostInitializeComponents() override;

private:
	/**
	 * The one ASC for this fighter, human or bot. Created as a default subobject so it exists
	 * before any possession, replication or ability grant can reference it — a lazily created ASC
	 * is a race with the very first InitAbilityActorInfo.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Breachpoint|Abilities")
	TObjectPtr<UBRAbilitySystemComponent> AbilitySystemComponent;

	/**
	 * THE attribute set. A subobject of this PlayerState rather than of the ASC, which is the
	 * engine's own pattern: the ASC discovers and registers it in InitializeComponent, and its
	 * replication rides the PlayerState's actor channel.
	 */
	UPROPERTY()
	TObjectPtr<UBRAttributeSet> AttributeSet;
};
