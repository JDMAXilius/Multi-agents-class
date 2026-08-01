// Breachpoint. The ASC: input-buffered tag activation, prediction-window helpers.

#pragma once

#include "AbilitySystemComponent.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "BRAbilitySystemComponent.generated.h"

/**
 * UBRAbilitySystemComponent — OUR ability system component (BREACHPOINT-ARCHITECTURE.md §3.3).
 *
 * It lives on ABRPlayerState (§3.6: "ASC + set live here"), never on the pawn: the pawn dies every
 * life and the PlayerState does not, so attributes, granted abilities and grant handles survive a
 * death by construction. The avatar is re-pointed at the new pawn by InitAbilityActorInfo, called
 * from ABRCharacter on BOTH the server (PossessedBy) and the client (OnRep_PlayerState).
 *
 * WHAT THIS CLASS ADDS OVER THE ENGINE'S ASC — three things, and nothing else:
 *
 *  1. TAG-KEYED INPUT (§3.2's fifth arrow). The engine's own input path is InputID-based
 *     (AbilityLocalInputPressed(int32)), an integer enum per ability that every project ends up
 *     hand-maintaining. We replaced it with FGameplayTag before the first ability exists, which is
 *     what lets §3.2's law hold: "abilities are bound by tag, not by binding code — a new ability
 *     is a DataAsset row, no input code changes." The matching is on the spec's DYNAMIC SOURCE
 *     TAGS, which is where BRAbilitySet (step 3) stamps each grant's InputTag.
 *
 *  2. PREDICTION-WINDOW HELPERS. Thin, deliberately: the engine already owns the machinery
 *     (FScopedPredictionWindow, CanPredict(), GetPredictionKeyForNewAction()). What is missing is
 *     a way to run a block of work inside a window without every caller re-deriving the RAII
 *     dance, and the RPC-batch wrapper that BP03's fire path needs.
 *
 *  3. ServerAbilityRPCBatch ENABLED (§3.3). Activate + TargetData + end travel in ONE RPC per
 *     shot instead of three. It is opt-in per project via ShouldDoServerAbilityRPCBatch().
 *
 * WHAT IT DELIBERATELY DOES NOT DO: it does not decide anything. No damage math, no cost, no
 * cooldown, no state — those are GameplayEffects and abilities (docs/contracts/gas-purity.md).
 * This component routes and predicts; it never adjudicates.
 */
UCLASS(meta = (DisplayName = "BR Ability System Component"))
class BREACHPOINT_API UBRAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UBRAbilitySystemComponent(const FObjectInitializer& ObjectInitializer);

	// -------------------------------------------------------------------------
	// Input — the destination of §3.2's five-arrow flow
	// -------------------------------------------------------------------------

	/**
	 * An ability InputTag went down. Relayed here by ABRPlayerController (human) or by
	 * ABRBotController pressing the same tag on its own ASC (bots — §3.7's "no privileged paths").
	 *
	 * IDEMPOTENT BY CONSTRUCTION, because it has to be: the binding is on
	 * ETriggerEvent::Triggered, so this is called EVERY FRAME the key is held. HeldInputTags is
	 * AddUnique-shaped and the function early-returns on a repeat — the activation happens on the
	 * press EDGE only. This is the single authoritative copy of held-input state in the project;
	 * BP01's diagnostic duplicate in ABRPlayerController was deleted when this landed, because two
	 * copies of held-input state drift and one of them is always the wrong one.
	 *
	 * DECIDED EDGE CASE (not a fallthrough): if an ability ends while its key is still held — a
	 * fired weapon cancelling sprint, per §3.3's CancelAbilitiesWithTags — the held tag stays held
	 * and the ability does NOT auto-restart on the next Triggered. Re-pressing is required. That is
	 * Halo's sprint behaviour and it is chosen, not inherited; a WhileHeld ability that wants
	 * auto-resume must say so in its own activation policy (step 3), not by making input re-fire.
	 */
	void AbilityInputTagPressed(FGameplayTag InputTag);

	/**
	 * An ability InputTag was released. Drives WaitInputRelease / WhileHeld ability ends.
	 * Safe to call for a tag that is not held (a release with no matching press is ignored).
	 */
	void AbilityInputTagReleased(FGameplayTag InputTag);

	/**
	 * Release every held tag and empty the buffer. Called on unpossess so a respawn cannot inherit
	 * a "still held" tag from the previous life — the ASC outlives the pawn, so without this the
	 * stale state is genuinely reachable.
	 */
	void ClearAbilityInput();

	/** Is this InputTag currently held, per the one authoritative buffer? */
	bool IsAbilityInputHeld(FGameplayTag InputTag) const { return HeldInputTags.Contains(InputTag); }

	/** The held-input buffer, read-only. For diagnostics and specs; nothing mutates it but this class. */
	const TArray<FGameplayTag>& GetHeldInputTags() const { return HeldInputTags; }

	// -------------------------------------------------------------------------
	// Prediction-window helpers
	// -------------------------------------------------------------------------

	/**
	 * Run Work inside a client prediction window (FScopedPredictionWindow), so any GE applied,
	 * tag granted or cue raised inside it is rolled back automatically if the server disagrees.
	 *
	 * On the server this is a plain call — the server IS the truth and needs no key — which is why
	 * callers do not branch on authority. That non-branching is the point: the same line of ability
	 * code is correct on both sides, which is what makes a mispredicted result a visual artifact
	 * instead of a divergence.
	 *
	 * NOT a substitute for server validation: predicting an action never authorizes it. The server
	 * re-checks every claim (BP03's fire path is the worked example).
	 */
	void ExecuteInPredictionWindow(TFunctionRef<void()> Work);

	/**
	 * Activate an ability inside a server-RPC batch: activate + TargetData (+ end) become ONE RPC
	 * instead of three. The GAS-shooter bandwidth optimization, and the reason fire abilities are
	 * written to end same-frame rather than stay active across the round trip.
	 *
	 * @return whether activation was attempted successfully (the SERVER still decides the outcome).
	 *
	 * DELIBERATELY MISSING, AND NAMED RATHER THAN FAKED: the `bEndAbilityImmediately` half of the
	 * canonical helper. Ending someone else's ability from here requires a public end on the
	 * ability, and UE 5.8 has none — UGameplayAbility::EndAbility and K2_EndAbility are both
	 * PROTECTED, and CancelAbilityHandle is a cancel, which is a different thing with different
	 * consequences (bWasCancelled propagates into every task's cleanup). The GAS-shooter recipe
	 * everyone copies gets around this with an ExternalEndAbility() they added to their OWN ability
	 * base — which is BP02 step 3's UBRGameplayAbility, and is not this packet's to write.
	 *
	 * STEP 3 CLOSES THIS: add `void ExternalEndAbility()` to UBRGameplayAbility (body:
	 * `EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false)`), then
	 * give this function its bEndAbilityImmediately parameter back. Until then a caller that needs
	 * the one-RPC round trip gets the batch, and ends its own ability from inside itself — which is
	 * where an ability should be ended from anyway.
	 */
	bool BatchRPCTryActivateAbility(FGameplayAbilitySpecHandle AbilityHandle);

	/** §3.3: batching is ON for this project. The engine default is off. */
	virtual bool ShouldDoServerAbilityRPCBatch() const override { return true; }

	// -------------------------------------------------------------------------
	// The RecentDamage regen gate — an extension point, not an implementation
	// -------------------------------------------------------------------------

	/**
	 * Apply the RecentDamage gate effect to this ASC's owner. Called by UBRAttributeSet the instant
	 * damage lands; the effect grants State.Combat.RecentDamage for its duration, and GE_Regen's
	 * ongoing-tag requirement blocks on that tag. "Shield regen starts 2.5 s after the last hit"
	 * is therefore a consequence of a tag expiring, with zero timer code anywhere.
	 *
	 * @return true if an effect was applied. FALSE, LOUDLY, when RecentDamageEffectClass is unset —
	 *         which is the honest state until step 4 authors GE_RecentDamage. A silent false here
	 *         would mean shields regenerate through sustained fire and nothing would say so.
	 */
	bool ApplyRecentDamageGate();

	/**
	 * The GE class that grants State.Combat.RecentDamage. A CLASS reference, not an asset path:
	 * under ruling R18 the six generic GEs are C++ UGameplayEffect subclasses in
	 * AbilitySystem/Effects/, so there is nothing to soft-load and nothing to cook.
	 *
	 * Null until BP02 step 4 lands GE_RecentDamage and assigns it on this class's defaults.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Abilities")
	TSubclassOf<UGameplayEffect> RecentDamageEffectClass;

private:
	/**
	 * Send InputPressed/InputReleased for an ACTIVE spec, keyed off the live instance's activation
	 * prediction key. Factored out because getting that key from the right place is the subtle part
	 * — see the .cpp; the spec's own ActivationInfo is deprecated and would be the wrong key.
	 */
	void InvokeInputEventForSpec(const FGameplayAbilitySpec& Spec, EAbilityGenericReplicatedEvent::Type EventType);

	/**
	 * Every ability InputTag currently held, in press order.
	 *
	 * THE authoritative copy. A TArray rather than a TSet on purpose: the set is tiny (bounded by
	 * the ability rows in DA_InputConfig), press order is occasionally meaningful, and — the real
	 * reason — a TArray iterates in a deterministic order, while TSet iteration order depends on
	 * hashing and capacity. Anything a spec pins must not depend on container internals.
	 *
	 * Client-local: this is the local player's (or bot's) own view of its input stream. It is not
	 * replicated and must never become gameplay state — the STATE is the tags GEs apply.
	 */
	TArray<FGameplayTag> HeldInputTags;
};
