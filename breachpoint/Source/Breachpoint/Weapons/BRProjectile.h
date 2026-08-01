// Breachpoint. The server-authoritative projectile — D6's fourth unit of ARCHITECTURE §3.5.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Templates/SubclassOf.h"
#include "UObject/WeakObjectPtr.h"

#include "BRProjectile.generated.h"

class UAbilitySystemComponent;
class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;

/**
 * Everything a projectile IS, handed to it once, at the spawn.
 *
 * A struct rather than nine arguments so the spawn has ONE success/failure answer, exactly as
 * `FBRGrenadeTuning` does for the grenade's activation: a projectile that knows its fuse but not
 * its blast radius is not a partially-configured projectile, it is a refusal.
 *
 * **THE NUMBERS TRAVEL WITH THE SPAWN AND ARE NEVER RE-READ.** The thrower resolved radius, centre
 * damage and the falloff curve's name from `CT_Combat` at activation; those exact values ride here
 * to the detonation seconds later. One grenade, one set of numbers. Re-reading the table at
 * detonation would let a mid-match table swap detonate a grenade with numbers its thrower never
 * agreed to, and would make two grenades in the air behave differently for no visible reason.
 *
 * NOTHING HERE IS REPLICATED. No client ever needs a blast number: the blast is server truth and
 * reaches clients as attribute changes and a GameplayCue. See `ABRProjectile`'s class comment.
 */
USTRUCT()
struct FBRProjectileSpawnParams
{
	GENERATED_BODY()

	// -- Attribution: kill credit, `Event.Kill`, the killfeed and the effect context all read these

	/** The thrower/firer's avatar. Becomes the actor's `Owner`, and the blast's instigator pair. */
	UPROPERTY()
	TObjectPtr<AActor> InstigatorActor = nullptr;

	/**
	 * The thrower's ASC — the source of the damage spec.
	 *
	 * WEAK, and that is deliberate: the ASC lives on the PlayerState, which outlives a death but not
	 * a disconnect. A hard reference from a live projectile would keep a leaver's PlayerState (and
	 * its whole attribute set) alive until the grenade went off. A stale pointer resolves to null
	 * and `BRExplosion::ApplyExplosionDamage` refuses out loud, which is the documented behaviour.
	 */
	UPROPERTY()
	TWeakObjectPtr<UAbilitySystemComponent> InstigatorASC;

	// -- Ballistics

	/** World-space launch velocity in uu/s. Direction AND speed; the caller owns both. */
	FVector LaunchVelocity = FVector::ZeroVector;

	/**
	 * Bounce restitution, 0..1 (1 = no velocity lost). **NEGATIVE MEANS "NOT AUTHORED"** — see
	 * `ABRProjectile::InitializeProjectile`, which then leaves `UProjectileMovementComponent`'s own
	 * value standing and says so in the log rather than typing a number here.
	 *
	 * Zero cannot be the sentinel the way it is in `FBRGrenadeTuning`: a restitution of zero is a
	 * meaningful authored value (a grenade that lands dead where it hits).
	 */
	float Bounciness = -1.f;

	/** Surface friction applied on bounce/slide. Negative means "not authored", as above. */
	float BounceFriction = -1.f;

	// -- Fuse

	/**
	 * Seconds from spawn to detonation. Zero is legal and means "detonate on the next tick" — a
	 * grenade cooked past its own fuse goes off in the hand, which `UBRGA_Grenade::ResolveTuning`
	 * deliberately permits the CSV to author. Negative is refused.
	 */
	float FuseSeconds = -1.f;

	// -- The detonation payload, resolved once by the caller

	/** Blast radius in METRES. The overlap sphere and the falloff curve's denominator. */
	float BlastRadiusMetres = 0.f;

	/** Damage at the epicentre, BEFORE falloff and before `BRDamageExecCalc`'s multipliers. */
	float BlastCentreDamage = 0.f;

	/** `CT_Combat` curve evaluated at the normalised distance. See `BRExplosion.h` on why it is a name. */
	FName BlastFalloffCurveName = NAME_None;

	/** The detonation cue. Invalid means the leaf is still undeclared; the blast is then silent. */
	FGameplayTag ExplodeCueTag;
};

/**
 * ================================================================================================
 * ABRProjectile — the server-authoritative projectile. **This class closes D6.**
 * ================================================================================================
 *
 * `docs/DECISIONS-OWED.md` **D6** asked which §3 folder owns the projectile and recommended option
 * (a): *"`Weapons/BRProjectile`, owned by sim-builder for ballistics with netcode-builder on
 * spawn/replication/dormancy."* That is this file. §3.5 goes from three units to four; the shared
 * blast rule came with it and lives in `Weapons/BRExplosion.h`, which explains why.
 *
 * ITS SPECIFICATION WAS ALREADY WRITTEN. `UBRGA_Grenade::RequestProjectileSpawn` spent a packet as a
 * blocked seam whose comment enumerated what the projectile owed. Each of those six lines is a
 * section below, and the seam is now a call.
 *
 * ------------------------------------------------------------------------------------------------
 * AUTHORITY — spawned on the server, by the server, from the ability
 * ------------------------------------------------------------------------------------------------
 *
 * `gas-purity` §4 enumerates what may run inside a prediction window and names this exact case:
 * *"rocket projectile spawns server-side; the client shows a cue ghost."* A predictively spawned
 * actor has no rollback path — GAS does not know it exists, so a rejected activation leaves a live
 * grenade on one machine and nothing on the other. Clients never spawn one; they saw the throw cue.
 *
 * `bReplicates = true` with replicated movement, so every machine watches ONE grenade travel.
 *
 * ------------------------------------------------------------------------------------------------
 * BANDWIDTH — a low update rate while it flies, and dormancy the moment it stops
 * ------------------------------------------------------------------------------------------------
 *
 * `netcode.md` law 4 (minimum replication), and 4v4 x N grenades is exactly the multiplier that
 * makes it matter. Two measures, both structural:
 *
 *   - a deliberately low `NetUpdateFrequency` in flight. A grenade is a ballistic arc; the client's
 *     own `UProjectileMovementComponent` reproduces the curve between updates far better than it
 *     could reproduce a player's intent, so this is one of the few actors where a low rate costs
 *     nothing visible.
 *   - `DORM_DormantAll` at rest. A grenade that has stopped moving changes nothing until it
 *     detonates; an actor that keeps replicating "still here, still here" for the rest of its fuse
 *     is pure waste.
 *
 * **NOTHING IS REPLICATED BUT THE TRANSFORM.** There is no `GetLifetimeReplicatedProps` override in
 * this class and that is the design, not an omission: the payload (radius, damage, falloff, fuse) is
 * consumed only on the authority, so replicating it would be sending every client a set of numbers
 * it must never act on. The pickup's discipline — *"EVERY mutator paired with `FlushNetDormancy`"* —
 * therefore has nothing to guard here, and the one flush below is on the destruction path.
 *
 * ------------------------------------------------------------------------------------------------
 * COLLISION — `BRCollision::Projectile`, and a NAMED decision about pawns
 * ------------------------------------------------------------------------------------------------
 *
 * The hull's object type is `BRCollision::Projectile` (the alias, never the raw
 * `ECC_GameTraceChannel*` — `BRCore.h` says a mismatch there is silent and things simply stop
 * colliding). That alias table already named grenades as this channel's user; the channel was
 * waiting for this actor.
 *
 * **It blocks the world and IGNORES pawns.** Three reasons, pointing the same way:
 *   1. it is born inside the thrower's own capsule at eye height — a blocking pawn response would
 *      bounce every grenade off its own thrower on frame one;
 *   2. whether a grenade skips off an enemy body is a design ruling nobody has made, and inventing
 *      one here would be law 5's improvisation;
 *   3. it agrees with the blast: `BRExplosion` queries world geometry ONLY for line of sight,
 *      structurally, so *a body never shields a body*. A projectile that stopped dead on a body
 *      would contradict the rule its own detonation obeys.
 *
 * ------------------------------------------------------------------------------------------------
 * NO GAMEPLAY TICK (law 4), and the one thing that does tick
 * ------------------------------------------------------------------------------------------------
 *
 * `PrimaryActorTick` is off. The fuse is a TIMER. The rest event is a delegate. Nothing polls
 * gameplay state per frame.
 *
 * `UProjectileMovementComponent` ticks, and that is ENGINE MACHINERY, not gameplay: it integrates a
 * position and sweeps for blocking hits. It reads no attribute, evaluates no rule and decides no
 * outcome. When it stops, it unregisters its own tick (`StopSimulating`), so a grenade waiting out
 * its fuse on the floor costs nothing per frame either.
 *
 * ------------------------------------------------------------------------------------------------
 * WHAT THIS CLASS STILL OWES
 * ------------------------------------------------------------------------------------------------
 *
 *   - **A MESH.** `MeshComponent` is created and empty. A grenade model is Tier 4 of the authoring
 *     matrix (sourced art) and does not exist, and there is no data row naming a soft path for one
 *     the way `DT_Weapons.MeshSoftPath` does for a pickup. Until both land, the projectile is
 *     invisible: it flies, bounces and detonates correctly and you cannot see it. Named here so
 *     "the grenade is invisible" is a known missing asset rather than a rendering bug.
 *   - **BOUNCE NUMBERS.** `CT_Combat` has no `Grenade.Bounciness`/`Grenade.BounceFriction` row —
 *     see `InitializeProjectile`, which refuses to type one.
 *   - **BP09's THREE LINES.** A rocket is this class with (1) a blocking pawn response, (2)
 *     `bShouldBounce = false`, and (3) detonate-on-impact. All three are one parameter or one
 *     constructor line each, and none is written today because there is no rocket to consume them —
 *     writing them now would be speculative shape with no caller to prove it.
 */
UCLASS()
class BREACHPOINT_API ABRProjectile : public AActor
{
	GENERATED_BODY()

public:
	ABRProjectile();

	//~ Begin AActor interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End AActor interface

	/**
	 * SERVER-ONLY factory. The one way a projectile comes into the world.
	 *
	 * Deferred spawn -> `InitializeProjectile` -> `FinishSpawning`, so the projectile is fully
	 * configured before `BeginPlay` arms its fuse. The alternative (spawn, then configure) leaves a
	 * one-frame window in which a projectile exists with no payload, and the frame it detonates in
	 * is the frame nobody is watching.
	 *
	 * Refuses and returns nullptr — never a half-configured actor — when the world, the class or any
	 * payload field is unusable. Every refusal names the field.
	 *
	 * @param ProjectileClass which class to spawn. `ABRProjectile::StaticClass()` is the ordinary
	 *                        answer; the parameter exists so BP09 can pass a subclass without this
	 *                        file learning about rockets.
	 */
	static ABRProjectile* SpawnProjectile(
		UWorld* World,
		TSubclassOf<ABRProjectile> ProjectileClass,
		const FTransform& SpawnTransform,
		const FBRProjectileSpawnParams& InParams);

	/**
	 * Server-only. Binds the projectile to its payload and its ballistics. Called between
	 * `SpawnActorDeferred` and `FinishSpawning`; calling it twice is refused out loud.
	 */
	void InitializeProjectile(const FBRProjectileSpawnParams& InParams);

	/** Validate a payload without spawning anything. Shared by the factory and by specs. */
	static bool ValidateSpawnParams(const FBRProjectileSpawnParams& InParams, FString& OutReason);

	bool HasDetonated() const { return bDetonated; }
	bool IsAtRest() const { return bAtRest; }
	const FBRProjectileSpawnParams& GetSpawnParams() const { return Params; }

protected:
	/** `UProjectileMovementComponent::OnProjectileStop` — dynamic delegate, hence UFUNCTION. */
	UFUNCTION()
	void HandleProjectileStop(const FHitResult& ImpactResult);

	/** The fuse's one callback. A TIMER, never a Tick (law 4). */
	void HandleFuseExpired();

	/**
	 * Authority only. Applies the blast through `BRExplosion` and destroys the actor.
	 * Guarded against running twice — a fuse and a future impact path CAN land on one frame, the
	 * same race `UBRGA_Grenade::ThrowGrenade` guards with `bThrowResolved`.
	 */
	void Detonate();

	/** The physical hull. Root, query-only, object type `BRCollision::Projectile`. */
	UPROPERTY(VisibleAnywhere, Category = "Projectile")
	TObjectPtr<USphereComponent> CollisionComponent;

	/** Cosmetic only — no collision, never consulted by a rule. Empty until Tier 4 art exists. */
	UPROPERTY(VisibleAnywhere, Category = "Projectile")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	/** Engine machinery: integration, gravity and bounce. See the class comment on law 4. */
	UPROPERTY(VisibleAnywhere, Category = "Projectile")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	/**
	 * CONTRACT_GAP (D6 close): the hull's radius is a gameplay-adjacent number and law 3 puts
	 * gameplay numbers in a CSV — but no table owns projectile geometry and this packet may not
	 * invent a column. It is `EditDefaultsOnly` here as a PLACEHOLDER, exactly as
	 * `ABRWeaponPickup::InteractionRadiusCm` is, and it carries no design ruling.
	 *
	 * Sized small on purpose: a hull SMALLER than any grenade model will ever be means the physical
	 * grenade can never be bigger than the visible one, which is the failure players notice.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Projectile", meta = (ClampMin = "1.0", ForceUnits = "cm"))
	float CollisionRadiusCm = 8.f;

private:
	/** The payload, resolved once by the thrower. UPROPERTY for the object refs it carries. */
	UPROPERTY()
	FBRProjectileSpawnParams Params;

	/** The fuse. A timer handle, cleared on every exit including EndPlay. */
	FTimerHandle FuseTimerHandle;

	bool bInitialized = false;
	bool bDetonated = false;
	bool bAtRest = false;
};
