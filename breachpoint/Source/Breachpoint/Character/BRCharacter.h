// Breachpoint. The pawn: a body, not a brain.

#pragma once

#include "AbilitySystemInterface.h"
#include "CoreMinimal.h"
#include "GameFramework/Character.h"

#include "BRCharacter.generated.h"

class UBRCharacterMovementComponent;
class UBRInputConfig;
class UCameraComponent;
class UEnhancedInputComponent;
class UInputAction;
class USkeletalMeshComponent;
struct FInputActionValue;

/**
 * ABRCharacter — "the pawn is a body, not a brain" (BREACHPOINT-ARCHITECTURE.md §3.4).
 *
 * AUTHORED FRESH from §3.4. This class is not the First Person template's pawn rebased, copied,
 * subclassed or read for structure: the founder decision recorded in BP01's Log keeps all 47
 * template sources on disk and forbids reusing them, and no template header is included here or
 * in the .cpp. If a later reader finds a similarity, it is because §3.4 describes the pro-FPS
 * standard that the template also implements — not because anything was inherited.
 *
 * WHAT BP01 STEP 4 AUTHORS (all of it below is the shell, and the shell is the deliverable):
 *   - the dual-mesh setup,
 *   - the IAbilitySystemInterface seam, forwarding to nothing yet,
 *   - the input wiring: native verbs handled here, ability tags relayed to the controller,
 *   - the registration of UBRCharacterMovementComponent as the movement component.
 *
 * WHAT IT DELIBERATELY DOES NOT HAVE, AND WHY THE ABSENCE IS THE DESIGN (§3.4): no health, no
 * scoring, no weapon logic, no ammo, no death decision. Attributes live on the ASC, score on
 * PlayerState, equipment in Weapons/. Death is a *consequence* — the pawn reacts to Event.Death
 * by playing a cue and ragdolling Mesh3P; it never decides it. And no Tick (law 4): every
 * function in this class is called by a delegate, an override, or an input binding.
 *
 * STILL OWED BY §3.4, NOT BY THIS PACKET (named so the next builder does not re-derive them):
 *   - BP02 step 1: DONE. The GAS init dance is below — InitAbilityActorInfo(PS, this) from
 *     PossessedBy (server) AND OnRep_PlayerState (client), plus a real
 *     GetAbilitySystemComponent(). BP01's LoggedHeldInputTags diagnostic went with it; the
 *     authoritative held-input buffer is UBRAbilitySystemComponent's.
 *   - BP03/anim-builder: two ABPs, one per mesh; remote aim pitch from replicated RemoteViewPitch.
 *   - BP05: the server-side rear-arc melee helper, and the weapon mesh socket attach on both
 *     meshes.
 */
/**
 * `config = Game` is what makes `[/Script/Breachpoint.BRCharacter]` in `Config/DefaultGame.ini`
 * reach the properties below. Without it — plus a `Config` specifier per property — that ini
 * section is inert and only a Blueprint child's saved details panel can supply values. That was
 * the exact defect on `ABRPlayerController`; see its header for the empirical proof.
 */
UCLASS(config = Game, meta = (DisplayName = "BR Character"))
class BREACHPOINT_API ABRCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	/**
	 * The FObjectInitializer form is mandatory, not stylistic: it is the only place a subclass can
	 * substitute the movement component class (SetDefaultSubobjectClass), and §3.4 requires
	 * UBRCharacterMovementComponent to be the component that exists on this pawn from the first
	 * spawn onward. See the .cpp for the one line that does it.
	 */
	ABRCharacter(const FObjectInitializer& ObjectInitializer);

	// -------------------------------------------------------------------------
	// IAbilitySystemInterface — CLOSED by BP02 step 1. The seam is now a forward.
	// -------------------------------------------------------------------------

	/**
	 * §3.4: "the ASC lives on PlayerState; the pawn implements IAbilitySystemInterface by
	 * forwarding". This forwards to ABRPlayerState's ASC (§3.6: "ASC + set live here").
	 *
	 * STILL RETURNS NULL LEGITIMATELY, and every caller must still null-check: between spawn and
	 * PlayerState replication a client's pawn genuinely has no ASC, and so does a pawn mid-travel.
	 * That window is normal, not an error — and it is exactly why the init happens on BOTH
	 * PossessedBy and OnRep_PlayerState below, since the two orders of arrival are different on
	 * the server and on a client.
	 */
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// -------------------------------------------------------------------------
	// The GAS init dance (§3.4). BOTH overrides are required — see the .cpp.
	// -------------------------------------------------------------------------

	/** Server path: the pawn gets a controller, therefore a PlayerState, therefore an ASC. */
	virtual void PossessedBy(AController* NewController) override;

	/** Client path: the PlayerState replicates in AFTER the pawn. Without this, clients have no avatar. */
	virtual void OnRep_PlayerState() override;

	// -------------------------------------------------------------------------
	// Dual-mesh accessors (§3.4)
	// -------------------------------------------------------------------------

	/** Arms + weapon, OnlyOwnerSee, no shadow. Seen by the owning client only. */
	USkeletalMeshComponent* GetMesh1P() const { return Mesh1P; }

	/**
	 * Full body, OwnerNoSee, casts shadow — what everyone else and the death cam see.
	 *
	 * This is ACharacter's OWN inherited mesh, configured as the third-person mesh rather than a
	 * second skeletal mesh component created beside it. That is a deliberate reading of §3.4:
	 * the engine's crouch offset, root motion, ragdoll (§3.4's death path) and
	 * GetMesh()-based animation all target the inherited component, so introducing a rival
	 * "Mesh3P" component would leave the engine driving the wrong body. §3.4's name is honoured
	 * by this accessor; the identity is the engine's.
	 */
	USkeletalMeshComponent* GetMesh3P() const { return GetMesh(); }

	/** The first-person camera. Mesh1P hangs off it; the view is the pawn's control rotation. */
	UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }

	/** The movement component, already typed. Sprint (BP02) and grapple (BP06) will want this. */
	UBRCharacterMovementComponent* GetBRCharacterMovement() const;

protected:
	// -------------------------------------------------------------------------
	// PHASE 1 BASELINE — the four native verbs, named directly on the pawn.
	// -------------------------------------------------------------------------

	/**
	 * PHASE 1 BASELINE: the native verbs are bound from these five direct action references
	 * instead of by tag lookup through `UBRInputConfig`.
	 *
	 * WHAT IT BENDS. Nothing in law 3 — these are `TSoftObjectPtr` and ini-pinned, so there is no
	 * hard asset ref and no `ConstructorHelpers`, and the template's shape is matched WITHOUT
	 * matching its lawlessness (`AbreachpointCharacter` declares raw `UInputAction*` UPROPERTYs
	 * filled in from a Blueprint details panel — `breachpointCharacter.h:38-50`). What it bends is
	 * ARCHITECTURE.md §3.2's rule that the pawn learns its verbs from the input config and never
	 * names an asset: here it names four, so the tag-matching layer is out of the failure path.
	 *
	 * WHY THAT LAYER IS OUT OF THE PATH. Binding through the config requires, all at once: a
	 * non-null `ABRPlayerController`, a resolvable `InputConfig` soft ref, a `NativeInputActions`
	 * row per tag, and `Row.InputTag` matching the native tag exactly. Each is a separate way for
	 * WASD to die, and the previous log line reported success for all four unconditionally. The
	 * direct path needs a component and an asset, and that is all.
	 *
	 * PHASE 2 REPLACES IT by flipping `bBindNativeVerbsFromInputConfig` to true (one ini line, no
	 * recompile) and confirming the same PIE observation. Both paths are compiled, always; nothing
	 * was removed. See `docs/PHASE2-RELAYER.md` step 2.
	 */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Breachpoint|Input (Phase 1)")
	TSoftObjectPtr<UInputAction> MoveAction;

	/** PHASE 1 BASELINE: see MoveAction. Gamepad/keyboard look. Bent law: §3.2 only. */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Breachpoint|Input (Phase 1)")
	TSoftObjectPtr<UInputAction> LookAction;

	/**
	 * PHASE 1 BASELINE: see MoveAction. Mouse look is a SECOND action on IMC_MouseLook — verified
	 * against the assets: `IMC_Default` contains no `Mouse2D`/`MouseX` mapping at all, so without
	 * this the player cannot turn, and "cannot turn" reads to a playtester as "cannot move".
	 * Falls back to `ABRPlayerController::GetMouseLookAction()` when unset, so pinning it in
	 * either place works and neither double-binds.
	 */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Breachpoint|Input (Phase 1)")
	TSoftObjectPtr<UInputAction> MouseLookAction;

	/** PHASE 1 BASELINE: see MoveAction. Started/Completed pair. Bent law: §3.2 only. */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Breachpoint|Input (Phase 1)")
	TSoftObjectPtr<UInputAction> JumpAction;

	/** PHASE 1 BASELINE: see MoveAction. Started/Completed pair (hold to crouch). */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Breachpoint|Input (Phase 1)")
	TSoftObjectPtr<UInputAction> CrouchAction;

	/**
	 * PHASE 1 BASELINE: THE SWITCH. false (Phase 1) binds the native verbs from the five
	 * properties above; true (Phase 2) binds them by tag through `UBRInputConfig`, which is
	 * §3.2's shape and the permanent one.
	 *
	 * A SWITCH RATHER THAN A DELETION, deliberately: the founder's Phase 2 requires that every
	 * bypassed layer still be in the tree and re-enablable one at a time, and a bypass you can
	 * flip from an ini is a bypass whose removal is a one-line experiment instead of a merge.
	 * Ability verbs are NOT affected — they always route through the config, because there is no
	 * second path for them and they are not Phase 1 (they need GAS grants that do not exist yet,
	 * ticket BP19 step A0).
	 *
	 * Pinned from `[/Script/Breachpoint.BRCharacter]` in `Config/DefaultGame.ini`.
	 */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Breachpoint|Input (Phase 1)")
	bool bBindNativeVerbsFromInputConfig = false;

protected:
	// -------------------------------------------------------------------------
	// Input (§3.2 arrows three and four)
	// -------------------------------------------------------------------------

	/**
	 * Casts to UBRInputComponent, binds the four native verbs to the functions below, and binds
	 * EVERY ability row wholesale to the controller's two tag handlers.
	 *
	 * The asymmetry is the whole point of §3.2: the native verbs are named here because the pawn
	 * really does own them, while NO ability is named anywhere in this file. Adding Grapple is a
	 * row in DA_InputConfig plus a row in an ability set — never an edit here.
	 */
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	/** Native: planar movement, in control-rotation space. Bound to Triggered. */
	void Input_Move(const FInputActionValue& Value);

	/** Native: look/aim. Bound to Triggered. Axis inversion belongs to the IMC, not to code. */
	void Input_Look(const FInputActionValue& Value);

	/** Native: jump. Bound to Started / Completed — hold to hold the jump, release to cut it. */
	void Input_JumpStarted();
	void Input_JumpCompleted();

	/** Native: crouch, HOLD semantics (Started crouches, Completed stands). */
	void Input_CrouchStarted();
	void Input_CrouchCompleted();

	/**
	 * Point the PlayerState's ASC at this pawn as its avatar. Idempotent — InitAbilityActorInfo is
	 * safe to re-run, which is what lets both the server and the client path call it unconditionally
	 * without either one having to know whether the other already did.
	 *
	 * @param CallSite  which of the two paths called, for the log line that proves both fired.
	 */
	void InitializeAbilitySystem(const TCHAR* CallSite);

	/**
	 * PHASE 1 BASELINE: bind the four native verbs from this pawn's own direct action references.
	 * @return total bindings created, written per-verb into the four out params.
	 */
	int32 BindNativeVerbsDirect(UEnhancedInputComponent* EnhancedInput, const UInputAction* ResolvedMouseLook, int32& OutMove, int32& OutLook, int32& OutJump, int32& OutCrouch);

	/** PHASE 2 SHAPE: bind the four native verbs by tag through the input config (§3.2). */
	int32 BindNativeVerbsFromConfig(UEnhancedInputComponent* EnhancedInput, const UBRInputConfig* Config, const UInputAction* ResolvedMouseLook, int32& OutMove, int32& OutLook, int32& OutJump, int32& OutCrouch);

	/**
	 * Log, once at possession, whether this pawn can physically move.
	 *
	 * "Input never arrived" and "input arrived and the CMC has nowhere to take it" are the same
	 * observation from the chair and different bugs on disk. A CMC with MaxWalkSpeed 0 consumes
	 * every input vector and produces no displacement. This line separates them; it only READS
	 * numbers (law 3: it sets none) and runs once (law 4).
	 */
	void LogMovementSnapshot() const;

	/** Resolve one soft action ref, loading it if needed. Binding happens once, so blocking is fine here. */
	static const UInputAction* ResolveAction(const TSoftObjectPtr<UInputAction>& SoftAction);

private:
	/** Arms + weapon; owning client only. Attached to the camera so it rides the view exactly. */
	UPROPERTY(VisibleAnywhere, Category = "Breachpoint|Character")
	TObjectPtr<USkeletalMeshComponent> Mesh1P;

	/** The first-person view. bUsePawnControlRotation — the controller aims, the pawn carries. */
	UPROPERTY(VisibleAnywhere, Category = "Breachpoint|Character")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	/**
	 * Handles for the ability-tag bindings, so a loadout or possession change can drop them via
	 * UBRInputComponent::RemoveBinds without touching the native verbs. Nothing removes them in
	 * BP01 — the bindings live as long as the pawn's input component — but collecting them is
	 * free and re-deriving them later is not.
	 */
	TArray<uint32> AbilityInputBindHandles;

	/**
	 * One-shot log latches. Move and Look fire every frame a key is held, so the *arrival* of
	 * each is logged once at Log and the traffic thereafter at VeryVerbose. This is what lets a
	 * verifier prove BP01's Done-when box 4 from a default-verbosity log without drowning in it
	 * (law 4's spirit: nothing this class does is per-frame chatter).
	 */
	uint8 bLoggedFirstMoveInput : 1;
	uint8 bLoggedFirstLookInput : 1;
};
