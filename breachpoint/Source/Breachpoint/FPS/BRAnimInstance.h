#pragma once

#include <atomic>

#include "Animation/AnimInstance.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "FPS/BRAnimTypes.h"

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

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|Weapon")
	FVector SwayLocation = FVector::ZeroVector;

	/**
	 * Seconds since the weapon last fired, saturating.
	 *
	 * The template's `timeSinceFiredWeapon`, default 9999 -- a sentinel meaning "not recently,
	 * and do not blend as if it were". Kept, including the saturation: an ever-growing float
	 * loses precision in a match that runs long enough.
	 */
	static constexpr float BR_NeverFired = 9999.f;

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

	/** How fast the held-back root bleeds back to neutral once the turn stops, in degrees/second. */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Breachpoint|Tuning")
	float RootYawOffsetBleedSpeed = 180.f;

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

public:

	/**
	 * The fire seam. Called by the weapon ability, never by the graph.
	 *
	 * Public because presentation is something the sim TELLS the animation; law 4 runs one way.
	 * An AnimInstance that noticed firing by itself would be deciding.
	 */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Weapon")
	void NotifyWeaponFired();

private:

	/** Pointer-to-member binding table. One row per tag; adding a state is one line, in code, greppable. */
	struct FBRTagBinding
	{
		FGameplayTag Tag;
		bool UBRAnimInstance::*Field;
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

	FBRSpring1D SwayYawSpring;
	FBRSpring1D SwayPitchSpring;

	float PreviousYaw = 0.f;
	FVector PreviousLocation = FVector::ZeroVector;
	bool bHasPreviousFrame = false;

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

	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
	TArray<FDelegateHandle> TagHandles;
};
