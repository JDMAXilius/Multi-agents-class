// Breachpoint. The input -> ASC relay. Stubs today; BP02 routes them.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "UObject/SoftObjectPtr.h"

#include "BRPlayerController.generated.h"

class UBRAbilitySystemComponent;
class UBRInputConfig;
class UInputMappingContext;

/**
 * ABRPlayerController — §3.6: "input->ASC relay, death cam, UI intent boundary."
 *
 * BP01 step 4 authors ONLY the relay half, and only its near end. The controller is the tag's
 * destination in §3.2's five-arrow flow:
 *
 *   IMC_Default -> UInputAction -> UBRInputComponent -> InputTag.Fire
 *     -> ABRPlayerController::AbilityInputTagPressed(Tag)      <- THIS FILE (relay)
 *     -> UBRAbilitySystemComponent::AbilityInputTagPressed(Tag) <- where the buffer lives
 *
 * BP02 step 1 closed the relay. The two handlers below are now four lines each and will never be
 * more: everything interesting — idempotency, activation, prediction, the server's say — is the
 * ASC's. A controller that starts deciding things is a controller that has to be kept in sync with
 * the ASC, and §3.2 exists so that never happens.
 *
 * It also owns the two authored data references the flow starts from — the mapping context and
 * the input config — because the controller outlives the pawn: repossession must not lose the
 * player's control scheme, and a config living on the pawn would be re-resolved on every respawn.
 *
 * NOT in this file, and not this packet's to add: death cam, UI intent, any RPC, any replicated
 * property, any ASC. AbilityInputTagPressed/Released are *local client* functions — they run
 * where the key was pressed and hand a tag onward. Authority is the ASC's (BP02) and the
 * server's; nothing here decides whether an ability may run.
 */
UCLASS(meta = (DisplayName = "BR Player Controller"))
class BREACHPOINT_API ABRPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ABRPlayerController();

	// -------------------------------------------------------------------------
	// The relay. THE SIGNATURE IS NOT A CHOICE — see the comment block below.
	// -------------------------------------------------------------------------

	/**
	 * An ability InputTag went down. Called by the binding UBRInputComponent::BindAbilityActions
	 * created on the possessed pawn's input component, once per Triggered event — which means
	 * REPEATEDLY while the key is held (see BRInputComponent.h). Any implementation added here
	 * must therefore be idempotent.
	 *
	 * FGameplayTag BY VALUE, not const&, and that is fixed by the binding mechanism rather than
	 * by taste: Enhanced Input's BindAction is variadic in its trailing payload (VarTypes...),
	 * and passing Row.InputTag deduces VarTypes = FGameplayTag. The generated delegate signature
	 * is void(FGameplayTag), so a const FGameplayTag& parameter matches none of the three handler
	 * shapes and fails overload resolution at the BindAction call — a template error in
	 * BRInputComponent.h, far from whoever "cleaned up" this line. Do not change it.
	 *
	 * The idempotency this comment demands is provided BY THE ASC (its AddUnique-shaped
	 * HeldInputTags), not here. That is the whole point of the relay being a relay: there is exactly
	 * one place that knows what is held.
	 */
	void AbilityInputTagPressed(FGameplayTag InputTag);

	/** An ability InputTag was released. Same signature law as the pressed handler above. */
	void AbilityInputTagReleased(FGameplayTag InputTag);

	/**
	 * This player's ASC, via its ABRPlayerState. Null before the PlayerState exists or replicates —
	 * a real window on a joining client, so callers null-check rather than assert.
	 */
	UBRAbilitySystemComponent* GetBRAbilitySystemComponent() const;

	// -------------------------------------------------------------------------
	// The authored data the input flow starts from. SOFT refs only (law 3 /
	// data-and-assets.md): a hard UPROPERTY asset pointer or a constructor-time asset
	// finder here would drag every input asset into memory with the controller class and
	// make it uncookable apart from them. The guard hook blocks the latter outright.
	// -------------------------------------------------------------------------

	/**
	 * The hardware->tag map: Content/Input/DA_InputConfig, authored by BP01 step 3's committed
	 * generation script (law 7 — input assets are generated, never hand-placed). Unassigned
	 * until that packet lands, which is why every consumer here logs and survives a null.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Input")
	TSoftObjectPtr<UBRInputConfig> InputConfig;

	/**
	 * Content/Input/IMC_Default — the mapping context added to this player's Enhanced Input
	 * subsystem. Arrow one of the five; with no context added, every UInputAction in the config
	 * is bound to a key that never fires, and the whole chain is silently dead.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Input")
	TSoftObjectPtr<UInputMappingContext> DefaultMappingContext;

	/** Priority the context is added at. Higher wins a key conflict; menus/UI layer above this. */
	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Input")
	int32 DefaultMappingContextPriority = 0;

	/**
	 * Resolve InputConfig, loading it if it is not already resident. Called by ABRCharacter at
	 * SetupPlayerInputComponent time — binding happens once per possession, so the blocking load
	 * is survivable there and nowhere else.
	 * @return the config, or nullptr when unassigned (logged, never an assert: an unassigned
	 *         config is the expected state until the generation packet lands).
	 */
	const UBRInputConfig* GetInputConfig() const;

protected:
	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	/**
	 * Runs on the server AND on the owning client (via OnRep_Pawn), which is why the held-input
	 * flush lives here and not in OnUnPossess. See the .cpp.
	 */
	virtual void SetPawn(APawn* InPawn) override;

private:
	/** Add DefaultMappingContext to this local player's Enhanced Input subsystem. Idempotent. */
	void AddDefaultMappingContext();

	// DELETED BY BP02 STEP 1: LoggedHeldInputTags, BP01's transient diagnostic de-duplicator.
	// It was never gameplay state and it is not promoted to any — the authoritative held-input set
	// is UBRAbilitySystemComponent::HeldInputTags, and there is now exactly one of it. Recorded
	// here rather than silently removed so a reader of BP01's Log finds the disposition.
};
