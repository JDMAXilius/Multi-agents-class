// Breachpoint. The input -> ASC relay. Stubs today; BP02 routes them.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "UObject/SoftObjectPtr.h"

#include "BRPlayerController.generated.h"

class UBRAbilitySystemComponent;
class UBRInputConfig;
class UInputAction;
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
/**
 * `config = Game` IS LOAD-BEARING, NOT DECORATION. Without it, plus a `Config` specifier on each
 * property below, UE never reads `[/Script/Breachpoint.BRPlayerController]` out of
 * `Config/DefaultGame.ini` and every ini pin in that section is silently inert — the properties
 * keep their C++ initialisers (null) and only a Blueprint child's saved details panel can supply
 * them. That was the state of this class until 1 Aug 2026, and it was PROVEN inert from a PIE log:
 * the section pinned `MouseLookAction` and `+AdditionalMappingContexts`, yet the pawn's bind line
 * printed without " + MouseLook" and no `IMC_MouseLook` was ever added. Compare
 * `AShooterPlayerController`, which is `UCLASS(abstract, config="Game")` with
 * `UPROPERTY(EditAnywhere, Config, ...)` — the template's ini pins work because of exactly these
 * two specifiers.
 */
UCLASS(config = Game, meta = (DisplayName = "BR Player Controller"))
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
	UPROPERTY(EditDefaultsOnly, Config, Category = "Breachpoint|Input")
	TSoftObjectPtr<UBRInputConfig> InputConfig;

	/**
	 * Content/Input/IMC_Default — the mapping context added to this player's Enhanced Input
	 * subsystem. Arrow one of the five; with no context added, every UInputAction in the config
	 * is bound to a key that never fires, and the whole chain is silently dead.
	 */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Breachpoint|Input")
	TSoftObjectPtr<UInputMappingContext> DefaultMappingContext;

	/**
	 * Extra mapping contexts added AFTER DefaultMappingContext (same priority).
	 *
	 * Learned from the First Person / Shooter template's split: IMC_Default holds Move/Jump and
	 * the ability keys; IMC_MouseLook holds the mouse->IA_MouseLook mapping so touch builds can
	 * omit it. Missing this list is how "WASD works, mouse does nothing" shows up after Move is
	 * fixed. Soft refs only (law 3); pinned from Config/DefaultGame.ini so a C++-class PC still aims.
	 */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Breachpoint|Input")
	TArray<TSoftObjectPtr<UInputMappingContext>> AdditionalMappingContexts;

	/**
	 * HARD context pointers, settable on the Blueprint — the template's proven shape, added as a
	 * BELT-AND-BRACES source alongside the soft/ini ones above. Empty by default, so it changes
	 * nothing until someone fills it in.
	 *
	 * WHY THIS EXISTS. `BP_ShooterPlayerController` holds `TArray<UInputMappingContext*>` with
	 * `IMC_Default`, `IMC_MouseLook` and `IMC_Weapons` assigned in the editor, and that controller
	 * demonstrably moves `BP_BRcharacter`. Ours resolves soft refs from an ini string instead:
	 * same intent, strictly more failure modes — the ini section must be read (`config = Game`),
	 * the path must parse, the soft ref must resolve, and a Blueprint that ever serialises the
	 * property silently wins over all of it (`PHASE2-RELAYER.md` step 1).
	 *
	 * Every one of those was verified correct on 1 Aug and input still differed between the two
	 * controllers, so the remaining difference is the mechanism itself. This project has now lost
	 * a day twice to input that fails invisibly; a second, dumber path that a human can see and
	 * set in the editor is worth its cost.
	 *
	 * **It ADDS to the soft list, it does not replace it** — a context named in both is added
	 * once (Enhanced Input ignores a duplicate add of the same context at the same priority), and
	 * the census below reports what actually landed either way.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Input")
	TArray<TObjectPtr<UInputMappingContext>> HardMappingContexts;

	/**
	 * IA_MouseLook — the template's mouse-only look action, consumed by IMC_MouseLook.
	 *
	 * NOT in DA_InputConfig: the eleven-action contract forbids a second native row with
	 * InputTag.Look, and IA_MouseLook is listed unmanaged in Tools/gen_input. Bound beside
	 * InputTag.Look to the same Input_Look handler (Shooter/FP template parity). Soft, ini-pinned.
	 */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Breachpoint|Input")
	TSoftObjectPtr<UInputAction> MouseLookAction;

	/** Priority the context is added at. Higher wins a key conflict; menus/UI layer above this. */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Breachpoint|Input")
	int32 DefaultMappingContextPriority = 0;

	/**
	 * Resolve InputConfig, loading it if it is not already resident. Called by ABRCharacter at
	 * SetupPlayerInputComponent time — binding happens once per possession, so the blocking load
	 * is survivable there and nowhere else.
	 * @return the config, or nullptr when unassigned (logged, never an assert: an unassigned
	 *         config is the expected state until the generation packet lands).
	 */
	const UBRInputConfig* GetInputConfig() const;

	/** Resolve MouseLookAction for the pawn's SetupPlayerInputComponent bind. Null if unassigned. */
	const UInputAction* GetMouseLookAction() const;

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
	/**
	 * Add DefaultMappingContext + AdditionalMappingContexts to this local player's Enhanced Input
	 * subsystem. Idempotent per context. Also screams if CommonUI's viewport client is wrong —
	 * that failure looks identical to "bindings succeeded, keys do nothing" (BP18).
	 */
	void AddDefaultMappingContext();

	/**
	 * Log, once per possession, which InputActions named by InputConfig actually have a KEY in the
	 * mapping contexts this controller just added.
	 *
	 * THIS IS THE LINE THAT MAKES THE NEXT PIE RUN DIAGNOSTIC, and it exists because the previous
	 * log set could not tell two very different failures apart. "Bindings created" and "context
	 * added" were both being printed while WASD was dead, because neither of them answers the
	 * question in between: does the context the controller loaded contain a key for the action the
	 * pawn bound? A row in DA_InputConfig with no mapping in the IMC is a bound delegate that
	 * nothing can ever fire, and it logs as total success at both ends of the gap.
	 *
	 * Data query only — it walks UInputMappingContext::GetMappings() and counts. It touches no
	 * subsystem state, adds nothing, and is called exactly once (law 4: no per-frame anything).
	 */
	void LogMappingKeyCensus(const TArray<const UInputMappingContext*>& AddedContexts) const;

	// DELETED BY BP02 STEP 1: LoggedHeldInputTags, BP01's transient diagnostic de-duplicator.
	// It was never gameplay state and it is not promoted to any — the authoritative held-input set
	// is UBRAbilitySystemComponent::HeldInputTags, and there is now exactly one of it. Recorded
	// here rather than silently removed so a reader of BP01's Log finds the disposition.
};
