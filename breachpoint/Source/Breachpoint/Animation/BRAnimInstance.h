#pragma once

#include <atomic>

#include "Animation/AnimInstance.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "Animation/BRAnimTypes.h"
#include "Animation/BRRecoilTypes.h"
#include "Animation/BRSwayAndLagTypes.h"

#include "BRAnimInstance.generated.h"

class UAbilitySystemComponent;

/**
 * The one AnimInstance spine, shared by the first-person arms and the third-person body.
 *
 * WHERE THIS CAME FROM. `ABP_Mannequin_Base` in the bought FPS pack -- a Lyra animation strip --
 * carried 96 properties, and that property list IS an anim state model. It was read out of a
 * live editor into `mcp-bp/bp_inventory.json`, judged asset by asset in
 * `docs/ANIM-PORT-LEDGER.md`, and re-implemented here. **Re-implemented, not translated**: no
 * tool in this project can read a Blueprint graph, so what was ported is the state model and the
 * intent, against BREACHPOINT's laws rather than the seller's habits.
 *
 * THE ONE LAW THAT SHAPES EVERY LINE (`animation.md` law 1). Anim update runs on a worker
 * thread. So this class has exactly two update passes and they are not interchangeable:
 *
 *   NativeUpdateAnimation           -- GAME thread. Touches UObjects. Fills `Snapshot`. Computes
 *                                      nothing the graph reads.
 *   NativeThreadSafeUpdateAnimation -- WORKER thread. Touches `Snapshot` and its own members.
 *                                      Computes everything. Touches no UObject, ever.
 *
 * A field read by the graph is written in the worker pass. A UObject read is done in the game
 * pass. Mixing them "works" right up until it is a hitch under load, which is why law 1 calls a
 * game-thread anim hack a finding even when it works.
 *
 * AND THE LAW THAT SHAPES WHAT IS ABSENT (law 4). This class REQUESTS and PRESENTS. It never
 * decides. There is no damage here, no ammo, no hit confirmation, no branch on a gameplay
 * number -- notifies raise events and the sim decides on the authority. If a value here ever
 * changes what the game does rather than what it looks like, that is a finding.
 */
UCLASS(Config = Game)
class BREACHPOINT_API UBRAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:

	UBRAnimInstance();

	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;
	virtual void NativeUninitializeAnimation() override;

protected:

	// ---------------------------------------------------------------- locomotion (worker-written)

	/** Velocity rotated into the actor's frame. The graph's cardinal blends read this, not world velocity. */
	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|Locomotion")
	FVector LocalVelocity2D = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|Locomotion")
	float GroundSpeed = 0.f;

	/** Signed angle between facing and travel, -180..180. The number every cardinal decision comes from. */
	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|Locomotion")
	float LocalVelocityDirectionAngle = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|Locomotion")
	EBRAnimCardinal VelocityCardinal = EBRAnimCardinal::Forward;

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|Locomotion")
	EBRAnimCardinal AccelerationCardinal = EBRAnimCardinal::Forward;

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|Locomotion")
	FVector LocalAcceleration2D = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|Locomotion")
	bool bHasVelocity = false;

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|Locomotion")
	bool bHasAcceleration = false;

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|Locomotion")
	bool bIsOnGround = false;

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|Locomotion")
	bool bIsFalling = false;

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|Locomotion")
	bool bIsCrouched = false;

	/**
	 * Distance moved this frame, and that distance as a speed.
	 *
	 * From the template's `displacementSinceLastUpdate` / `displacementSpeed`. Distance matching
	 * needs the DISTANCE, not the velocity: a character sliding to a stop still has velocity long
	 * after it has stopped covering ground, and blending on velocity is what makes a stop foot-skate.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|Locomotion")
	float DisplacementSinceLastUpdate = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|Locomotion")
	float DisplacementSpeed = 0.f;

	/**
	 * Airborne state, split from `bIsFalling` because they are not the same question.
	 *
	 * `bIsFalling` is true the instant you walk off a ledge; `bIsJumping` is true only when you
	 * left the ground going UP. The graph needs both -- a jump start plays a launch pose, a ledge
	 * drop does not, and a single "in air" bool makes every walk-off look like a deliberate hop.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|Air")
	bool bIsJumping = false;

	/**
	 * Seconds until the apex, from the pack's `timeToJumpApex`.
	 *
	 * Derived, never a countdown: `-VelocityZ / GravityZ`. A ticking timer would need resetting
	 * on every launch, double jump and root-motion push, and would be wrong on precisely the
	 * frame it mattered. Zero once falling.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|Air")
	float TimeToJumpApex = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|Air")
	float VelocityZ = 0.f;

	// ---------------------------------------------------------------- pivots

	/**
	 * A pivot is a direction REVERSAL, and detecting it needs acceleration, not velocity.
	 *
	 * The pack carried `pivotDirection2D`, `pivotInitialDirection` and `lastPivotTime` for this.
	 * The tell is acceleration opposing velocity: the player has asked to go the other way while
	 * still travelling the old one. Velocity alone cannot see it -- by the time velocity flips,
	 * the plant has already been missed and the character skates through the turn.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|Locomotion")
	bool bIsPivoting = false;

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|Locomotion")
	FVector PivotDirection2D = FVector::ZeroVector;

	/** Seconds since the last pivot, saturating like `TimeSinceFired` and for the same reason. */
	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|Locomotion")
	float TimeSincePivot = BR_NeverFired;

	// ---------------------------------------------------------------- transition edges

	/**
	 * One-frame EDGES, not levels.
	 *
	 * The pack had `aDSStateChanged`, `crouchStateChange`, `wasADSLastUpdate`,
	 * `linkedLayerChanged`. A state machine that transitions on a LEVEL ("is crouched") re-enters
	 * its entry state for as long as the level holds; it needs the moment the level CHANGED.
	 * These are true for exactly one update and are cleared at the top of the next.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|Transitions")
	bool bCrouchStateChanged = false;

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|Transitions")
	bool bADSStateChanged = false;

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|Transitions")
	bool bLinkedLayerChanged = false;

	// ---------------------------------------------------------------- linked layer

	/**
	 * Which weapon row the currently linked layer serves -- asked through `IBRAnimLayer`.
	 *
	 * This is the payoff for the interface existing. The spine knows WHICH layer is up without
	 * ever naming a layer class, so `FPS/` stays free of asset references and adding a weapon
	 * stays a row plus an asset.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|Layer")
	FName LinkedLayerRow;

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|Layer")
	bool bLayerOverridesHandPose = false;

	// ---------------------------------------------------------------- additive weights

	/**
	 * Presentation alphas the graph multiplies its additive branches by.
	 *
	 * From `upperbodyDynamicAdditiveWeight`, `applyCrouchAlpha`, `applySwayAlpha`. Computed here
	 * rather than as graph maths for the reason Amendment A gives: a graph carrying computation
	 * is a design smell with a named home, and this is the home.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|Additive")
	float UpperBodyAdditiveWeight = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|Additive")
	float ApplyCrouchAlpha = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|Additive")
	float ApplySwayAlpha = 1.f;

	// ---------------------------------------------------------------- aim, lean, turn-in-place

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|Aim")
	float AimPitch = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|Aim")
	float AimYaw = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|Aim")
	float YawDeltaSinceLastUpdate = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|Aim")
	float YawDeltaSpeed = 0.f;

	/**
	 * How far the root is held back from the camera while the feet stay planted.
	 *
	 * Turn-in-place, and the only field here that is a genuine accumulator: it integrates yaw
	 * delta and bleeds off, so a wrong sign is invisible standing still and obvious in motion.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|Aim")
	float RootYawOffset = 0.f;

	/** Additive lean from turn rate. Presentation only -- leaning never moves the capsule. */
	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|Aim")
	float LeanAngle = 0.f;

	// ---------------------------------------------------------------- weapon feel (worker-written)

	/**
	 * Sway and bob, as ONE transform the graph applies with a stock node.
	 *
	 * Amendment A wants this in a custom `FAnimNode_*` and is right that the computation belongs
	 * in C++ -- it is here, on the worker thread, springs and all. What is NOT here is the node,
	 * and the reason is structural rather than a shortcut: a custom anim node needs a
	 * `UAnimGraphNode_*` in an EDITOR module to be placeable in a graph, and
	 * `Breachpoint.uproject` declares exactly one module (`Breachpoint`, Runtime).
	 *
	 * Filed as a contract_gap, not worked around. The law that matters -- "the graph reads
	 * fields, it never computes" -- holds either way: the graph places one Transform Bone node
	 * and reads this.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|Weapon")
	FRotator SwayRotation = FRotator::ZeroRotator;

	/** Positional weapon lag, actor frame. The graph applies it with the same node as the sway. */
	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|Weapon")
	FVector SwayLagOffset = FVector::ZeroVector;

	/**
	 * Weapon-transform recoil: the gun kicking in the hands. Presentation, and ours.
	 *
	 * **Camera recoil is deliberately absent** — `animation.md` A.6, founder ruling 8 Aug 2026.
	 * It moves where the next bullet goes, which makes it a gameplay decision with a netcode
	 * surface, and it belongs to BP98's fire path.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|Weapon")
	FVector RecoilOffsetLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|Weapon")
	FRotator RecoilOffsetRotation = FRotator::ZeroRotator;

	// There is no `SwayLocation`. An earlier revision published one and assigned it
	// `FVector::ZeroVector` unconditionally -- a BlueprintReadOnly field a graph could wire
	// translation from and get nothing, forever. Positional sway needs a weapon-socket reference
	// the spine does not have; when it does, this is where it lands. Deleted rather than left as
	// a plausible-looking zero.

	/**
	 * Seconds since the weapon last fired, saturating.
	 *
	 * The template's `timeSinceFiredWeapon`, default 9999 -- a sentinel meaning "not recently,
	 * and do not blend as if it were". Kept, including the saturation: an ever-growing float
	 * loses precision in a match that runs long enough.
	 */
	static constexpr float BR_NeverFired = 9999.f;

	/** Ceiling on queued recoil. A spine that stops draining must not grow an array forever. */
	static constexpr int32 BR_MaxPendingRecoil = 32;

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|Weapon")
	float TimeSinceFired = BR_NeverFired;

	// ---------------------------------------------------------------- ASC state (game-thread-written)

	/**
	 * Tag-driven state. Every one of these is written by the ASC callback and read by the graph.
	 *
	 * WHY THESE ARE BOOLS AND NOT TAG QUERIES. A tag query per frame in an anim update is a
	 * container walk on the worker thread. The engine's own answer is a registered callback that
	 * writes a cached bool, and a cached bool that the ASC keeps correct is the difference
	 * Amendment A was pointing at.
	 *
	 * These are published by the WORKER pass from `Snapshot.Tags`, not written by the callback
	 * directly. The callback writes `TagState`, a private game-thread copy. That indirection is
	 * not ceremony: the worker computes edges and products from tag state, and a value that can
	 * change between a compare and its store loses the edge permanently.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|State")
	bool bIsSprinting = false;

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|State")
	bool bIsReloading = false;

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|State")
	bool bIsSwapping = false;

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|State")
	bool bIsMeleeing = false;

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|State")
	bool bIsGrappling = false;

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|State")
	bool bIsThrowingGrenade = false;

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|State")
	bool bIsDead = false;

	/**
	 * ADS and firing: declared, bound to nothing, and deliberately left false.
	 *
	 * R23, and this is the honest half of it. `Core/` is CLOSED for `State.*` and neither
	 * `State.Weapon.ADS` nor `State.Weapon.Firing` exists. The template had both
	 * (`gameplayTag_IsADS`, `gameplayTag_IsFiring`) so the spine wants them and the graph will
	 * want them. They are `contract_gap`s filed against BP93 -- NOT tags added locally, and NOT
	 * a hand-rolled bool set from somewhere else, which is exactly the second source of truth
	 * this whole mechanism exists to prevent.
	 *
	 * They light up when BP93 declares the tags and the table below names them. Nothing else changes.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|State")
	bool bIsADS = false;

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|State")
	bool bIsFiring = false;

	// ---------------------------------------------------------------- tuning (config, not code)

	/**
	 * Feel numbers. Law 3 keeps them out of code; `Config` keeps them out of an asset.
	 *
	 * These are not gameplay numbers -- nothing here changes what the game DOES -- so they are
	 * not owed a CSV row. They are presentation constants, and `DefaultGame.ini` is the smallest
	 * thing that lets them be tuned without a recompile and still be diffed by a critic.
	 *
	 * Defaults carried over from the pack where it had an opinion (`cardinalDirectionDeadZone`
	 * 10, `rootYawOffsetAngleClamp` {-120,100}) so the spine starts where the animations expect.
	 */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Breachpoint|Tuning")
	float CardinalDeadZone = 10.f;

	UPROPERTY(EditDefaultsOnly, Config, Category = "Breachpoint|Tuning")
	float RootYawOffsetMin = -120.f;

	UPROPERTY(EditDefaultsOnly, Config, Category = "Breachpoint|Tuning")
	float RootYawOffsetMax = 100.f;

	UPROPERTY(EditDefaultsOnly, Config, Category = "Breachpoint|Tuning")
	float RootYawOffsetMinCrouched = -90.f;

	UPROPERTY(EditDefaultsOnly, Config, Category = "Breachpoint|Tuning")
	float RootYawOffsetMaxCrouched = 80.f;

	/** How fast the held-back root bleeds back to neutral once the character MOVES, in degrees/second. */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Breachpoint|Tuning")
	float RootYawOffsetBleedSpeed = 180.f;

	/**
	 * Recovery while STANDING and no longer turning. Slower than the moving bleed on purpose.
	 *
	 * A placeholder for a consumer that does not exist: in Lyra the turn-in-place animation eats
	 * this offset through a yaw curve, and this project has no such curve and no ABP. Without it
	 * the offset saturates at the clamp and stays there. `contract_gap BP82-4`.
	 */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Breachpoint|Tuning")
	float RootYawOffsetIdleRecoverySpeed = 90.f;

	UPROPERTY(EditDefaultsOnly, Config, Category = "Breachpoint|Tuning")
	float LeanScale = 0.06f;

	UPROPERTY(EditDefaultsOnly, Config, Category = "Breachpoint|Tuning")
	float LeanMaxAngle = 12.f;

	UPROPERTY(EditDefaultsOnly, Config, Category = "Breachpoint|Tuning")
	float SwayStiffness = 90.f;

	UPROPERTY(EditDefaultsOnly, Config, Category = "Breachpoint|Tuning")
	float SwayDamping = 14.f;

	UPROPERTY(EditDefaultsOnly, Config, Category = "Breachpoint|Tuning")
	float SwayYawScale = 0.35f;

	UPROPERTY(EditDefaultsOnly, Config, Category = "Breachpoint|Tuning")
	float SwayPitchScale = 0.35f;

	UPROPERTY(EditDefaultsOnly, Config, Category = "Breachpoint|Tuning")
	float SwayMaxAngle = 8.f;

	/** Acceleration must oppose velocity by more than this (cosine) to count as a pivot, not a drift. */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Breachpoint|Tuning")
	float PivotOpposingDot = -0.5f;

	/**
	 * Hard ceiling on the DeltaSeconds any spring or integrator sees.
	 *
	 * NOT cosmetic. A level-load hitch or an editor breakpoint hands the anim update a delta of
	 * hundreds of milliseconds, and semi-implicit Euler at that step overshoots hugely -- the
	 * weapon leaves the screen for a frame and snaps back. Clamping the STEP is correct here
	 * because animation is presentation: a spring that resolves slightly slow through a hitch is
	 * invisible, and one that explodes is not.
	 */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Breachpoint|Tuning")
	float MaxIntegrationStep = 0.05f;

public:

	/**
	 * The fire seam. Called by the weapon ability, never by the graph.
	 *
	 * Public because presentation is something the sim TELLS the animation; law 4 runs one way.
	 * An AnimInstance that noticed firing by itself would be deciding.
	 */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Weapon")
	void NotifyWeaponFired();

	/**
	 * Queue a recoil kick. Called by the fire ability on the machines that own the shot.
	 *
	 * `Alpha` is the caller's roll in 0..1 between the impulse's min and max envelope, and it is
	 * an ARGUMENT rather than something rolled here — see `animation.md` A.6. Recoil randomised
	 * independently per machine makes the client's crosshair and the server's cone disagree by
	 * arithmetic, so whoever owns the shot owns the seed. This method takes the number it is
	 * given and asks no questions.
	 *
	 * Game thread in, worker thread out: the force lands in a queue that the next thread-safe
	 * pass drains, for the same reason `NotifyWeaponFired` uses an atomic.
	 */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Weapon")
	void AddRecoilImpulse(const FBRRecoilImpulse& Impulse, float Alpha);

	/**
	 * Raise one seam event, with both gates applied. The single door every notify path uses.
	 *
	 * Public because `UBRAnimNotify_GameplayEvent` and `UBRAnimNotifyState_GameplayEventWindow`
	 * call it — and those exist so the tag arrives as a **C++-declared `FGameplayTag` property**
	 * instead of a magic string matched against a table. That closes the hole this packet filed
	 * against itself: a windowed notify authored as a point notify silently restored a trace
	 * window that opens and never closes, and nothing could detect it. A notify that carries its
	 * own tag cannot be mis-named, because there is no name to get wrong.
	 */
	void SendSeamGameplayEvent(FGameplayTag EventTag);

private:

	/**
	 * Montage notify -> gameplay event. The seam `animation.md` law 4 is entirely about.
	 *
	 * "Notifies raise events; the sim decides." The animation announces that a moment ARRIVED --
	 * the melee trace window opened, the reload reached the point ammo moves -- and an ability
	 * waiting on that tag decides what it means. No damage, no ammo, no branch on a gameplay
	 * number happens here or anywhere in this class.
	 *
	 * The tags are R17's, closed 31 Jul 2026 and not re-litigated: `Event.Melee.WindowBegin` /
	 * `WindowEnd`, `Event.Weapon.ReloadCommit`, `Event.Weapon.SwapCommit`.
	 *
	 * NETCODE GATE, and it is the part that would have been wrong. Montages play on EVERY
	 * machine, including simulated proxies watching someone else. Forwarding unconditionally
	 * would raise a reload-commit event on each observer's copy of a remote player -- an event
	 * an ability could act on, on a machine with no authority over that pawn. So it forwards only
	 * where an ability legitimately runs: the authority, and the locally-controlled predicting
	 * client. This is invisible in PIE with one player and wrong the moment there are two.
	 */
	UFUNCTION()
	void HandleMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& Payload);

	UFUNCTION()
	void HandleMontageNotifyEnd(FName NotifyName, const FBranchingPointNotifyPayload& Payload);

	/**
	 * `bIsEnd` is not a convenience flag; it is the whole correctness of the seam.
	 *
	 * `AnimNotify_PlayMontageNotifyWindow` broadcasts **the identical `NotifyName`** to the begin
	 * and end delegates. A single name->tag map therefore makes a window's CLOSE emit the tag its
	 * OPEN emitted, and `Event.Melee.WindowEnd` unreachable by construction: the trace window
	 * opens and is never told to close, which is free hits. Two maps, selected by this flag.
	 */
	void ForwardNotifyAsGameplayEvent(FName NotifyName, bool bIsEnd);

	/**
	 * Does THIS instance speak for the pawn?
	 *
	 * A character has two meshes and law 2 gives each its own `UBRAnimInstance`. Both resolve
	 * `TryGetPawnOwner()` to the same pawn, so both would send the same gameplay event to the
	 * same ASC -- doubling every event, on every machine. The netcode gate cannot see this: it is
	 * per-machine, and this duplication is per-mesh.
	 *
	 * The mesh **GAS is using** is the one that speaks — asked, never assumed. An earlier
	 * revision picked the third-person mesh on the reasoning that it exists everywhere, which is
	 * true and irrelevant: GAS resolves the montage's mesh with
	 * `FindComponentByClass<USkeletalMeshComponent>()`, and `OwnedComponents` is a `TSet`. Guessing
	 * wrong does not double an event, it **deletes the entire seam** on every machine at once.
	 */
	bool IsGameplayEventSource() const;

	/** The owner's ASC, resolved fresh. Used before `BoundASC` exists and by the seam's gate. */
	UAbilitySystemComponent* ResolveAbilitySystem() const;

	/** Refresh `LinkedLayerRow` from whatever layer is currently linked. Game thread; asks the interface. */
	void RefreshLinkedLayer();

	/**
	 * Pointer-to-member binding table. One row per tag; adding a state is one line, in code, greppable.
	 *
	 * It points into `FBRAnimTagState`, NOT at the public bools. The callback writes the private
	 * game-thread copy; the worker publishes the public ones. That keeps every graph-read field
	 * worker-written, which is the invariant the whole class rests on.
	 */
	struct FBRTagBinding
	{
		FGameplayTag Tag;
		bool FBRAnimTagState::*Field;
	};

	/**
	 * Bind every tag in the table to its bool through the ASC's own callback.
	 *
	 * WHY NOT `FGameplayTagBlueprintPropertyMap`, which Amendment A names as the highest-value
	 * finding. Because of what it costs, not what it does. Its `PropertyMappings` array is
	 * `protected` and `EditAnywhere` (`GameplayEffectTypes.h:1480`), so the tag->property table
	 * is authored ON THE ABP ASSET -- invisible to the critic, no diff, no merge, no grep. That
	 * is precisely the condition R18 exists to prevent, and it would put the most important
	 * wiring in this class inside a binary file.
	 *
	 * `RegisterGameplayTagEvent` (`AbilitySystemComponent.h:720`) is the same engine mechanism
	 * one layer down, gives the identical no-drift guarantee, and keeps the table in C++ where
	 * `git diff` reads it. Amendment A's stated intent -- "the graph can never drift from what
	 * the ASC actually says" -- is met in full. Filed as a finding against its wording.
	 */
	void BindAbilitySystem();
	void UnbindAbilitySystem();
	void OnStateTagChanged(const FGameplayTag Tag, int32 NewCount);

	const TArray<FBRTagBinding>& GetTagBindings() const;

	/** Game-thread scratch, worker-thread input. The only channel between the two passes. */
	FBRAnimSnapshot Snapshot;

	/**
	 * Written by the ASC callback, read only by the game-thread pass that latches it into
	 * `Snapshot.Tags`. The worker never touches this.
	 */
	FBRAnimTagState TagState;

	/** Sway is now one interpolated rotator, not two springs -- see BRProceduralSolver::SolveSway. */
	FRotator SwayState = FRotator::ZeroRotator;
	bool bHasPreviousAim = false;

	float PreviousYaw = 0.f;
	FVector PreviousLocation = FVector::ZeroVector;
	bool bHasPreviousFrame = false;

	/** Previous-frame levels, kept solely so the one-frame edges above can be derived. */
	bool bWasCrouchedLastUpdate = false;
	bool bWasADSLastUpdate = false;
	FName PreviousLayerRow;
	float PreviousAimPitch = 0.f;
	bool bHasPreviousAimPitch = false;

	/** Suppresses first-pass transition edges, whose "previous" values are defaults, not history. */
	bool bHasPublishedOnce = false;

	/**
	 * The fire stamp crosses threads, so it is atomic and nothing else here needs to be.
	 *
	 * `NotifyWeaponFired` is called from gameplay on the GAME thread; `TimeSinceFired` is
	 * accumulated on the WORKER thread. Every other field is written by exactly one thread and
	 * needs no synchronisation -- this one has two writers, and a torn float here would be a
	 * one-frame wrong pose that never reproduces. The worker consumes the flag with an exchange,
	 * so a shot fired between two anim updates is stamped exactly once and never lost.
	 */
	std::atomic<bool> bFirePending { false };

	/**
	 * Recoil kicks queued on the game thread, drained by the worker pass.
	 *
	 * A lock, and it is the only one in this class -- justified because the producer is gameplay
	 * firing a weapon at an arbitrary moment and the consumer is the anim worker. A queue that
	 * two threads push and pop without one is not a race that shows up in testing; it is a
	 * corrupted TArray that crashes days later. The critical section is held for the length of an
	 * Append and a Reset, never across the solve.
	 */
	/**
	 * Queued as (envelope, roll) -- NOT as a built force.
	 *
	 * Building the force game-side would need the clock, and the clock is accumulated on the
	 * worker thread; reading it from gameplay is a cross-thread read of a float being written
	 * every frame. Handing over the two inputs instead lets the worker stamp the expiry with its
	 * OWN time, and the race stops existing rather than being made safe.
	 */
	struct FBRPendingRecoil
	{
		FBRRecoilImpulse Impulse;
		float Alpha = 0.f;
	};

	TArray<FBRPendingRecoil> PendingForces;
	TArray<FBRProceduralForce> ActiveForces;
	FCriticalSection PendingForcesLock;

	FVector LagOffset = FVector::ZeroVector;

	/**
	 * Wall clock accumulated on the worker thread.
	 *
	 * Not read from a world: a world is a UObject and law 1 forbids reaching one from this pass.
	 * It uses the REAL delta rather than the clamped step, because it measures elapsed time and
	 * not motion — the same distinction `TimeSinceFired` makes.
	 */
	float ProceduralTimeSeconds = 0.f;

	/**
	 * The profile used when NO layer is linked — unarmed, or before equipment has spoken.
	 *
	 * Named `Default…` and not `SwayAndLag` on purpose. The previous field claimed to be "the
	 * per-weapon profile in force" and was nothing of the kind: no layer, no interface member and
	 * no writer existed, so it was a single profile for every weapon read from an ini instead of
	 * from hardcoded scalars — a different source for the same defect. The real per-weapon value
	 * now arrives through `IBRAnimLayer::GetSwayAndLagInfo` and lands in the snapshot.
	 */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Breachpoint|Tuning")
	FBRSwayAndLagInfo DefaultSwayAndLag;

	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
	TArray<FDelegateHandle> TagHandles;
};
