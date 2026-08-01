// Breachpoint. Weapons lying in the world, and the node that puts the Rocket there.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "Templates/SubclassOf.h"

#include "BRWeaponPickup.generated.h"

class APawn;
class ABRWeaponPickup;
class UPrimitiveComponent;
class USphereComponent;
class UStaticMeshComponent;
struct FBRWeaponRow;
struct FStreamableHandle;

/** Server-side: a pawn has taken this pickup and it is about to be consumed. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FBROnWeaponPickupCollected, ABRWeaponPickup* /*Pickup*/, APawn* /*Collector*/);

/**
 * BP06 SEAM — a pickup has been asked to move to a point under server authority.
 * See ABRWeaponPickup::AttractTo. Nothing in BP03 binds this.
 */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FBROnWeaponPickupAttractRequested,
	ABRWeaponPickup* /*Pickup*/, const FVector& /*TargetLocation*/, AActor* /*Requester*/);

/**
 * ABRWeaponPickup — one weapon lying in the world, waiting to be collected.
 *
 * Specification: ARCHITECTURE §3.5. Two ways one exists:
 *   - spawned by ABRPowerWeaponSpawner (the Rocket node), or
 *   - dropped by a player through SpawnDroppedWeapon (a dropped gun keeps its ammunition —
 *     ruling R4's scarcity is the whole Rocket design, and a half-empty Rocket on the floor
 *     is a contest, not a bug).
 *
 * AUTHORITY: the client never collects anything. It asks
 * (UBREquipmentComponent::ServerRequestPickup), the server checks THIS actor's own overlap
 * set and consumed flag, and only then does state change. Nothing here trusts a location, a
 * distance, or a "yes I'm touching it" sent by a client.
 *
 * NO TICK (law 4): overlap delegates, a collect call, and timers. The countdown lives on the
 * spawner as an absolute server timestamp, so nobody counts down per frame either.
 *
 * WHAT IS PUBLIC INFORMATION HERE, deliberately: a pickup's remaining ammunition IS
 * replicated to everyone. It is world state on a contested object that any player may walk
 * up to and read — unlike the magazine in an enemy's hands, which is COND_OwnerOnly on
 * UBRWeaponInstance. The two are different questions and get different answers.
 */
UCLASS()
class BREACHPOINT_API ABRWeaponPickup : public AActor
{
	GENERATED_BODY()

public:
	ABRWeaponPickup();

	//~ Begin AActor interface
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End AActor interface

	// -- Creation (server) -------------------------------------------------------------------

	/**
	 * Server-only factory for a weapon a player just dropped. Returns nullptr (and logs) when
	 * the class or the row is not supplied — it never substitutes a default weapon.
	 */
	static ABRWeaponPickup* SpawnDroppedWeapon(
		UWorld* World,
		TSubclassOf<ABRWeaponPickup> PickupClass,
		const FDataTableRowHandle& InWeaponRow,
		int32 InAmmoInMag,
		int32 InAmmoReserve,
		const FTransform& SpawnTransform,
		AActor* InInstigatorActor);

	/** Server-only. Binds the pickup to a row and the ammunition it carries. */
	void InitializePickup(const FDataTableRowHandle& InWeaponRow, int32 InAmmoInMag, int32 InAmmoReserve);

	// -- Queries ------------------------------------------------------------------------------

	const FDataTableRowHandle& GetWeaponRowHandle() const { return WeaponRow; }
	const FBRWeaponRow* GetRow() const;
	int32 GetAmmoInMag() const { return AmmoInMag; }
	int32 GetAmmoReserve() const { return AmmoReserve; }
	bool IsConsumed() const { return bConsumed; }

	/**
	 * The server's answer to "may this pawn take this?" — authority-only, and the ONLY
	 * geometric authority is this actor's own server-side overlap set, which is produced by
	 * the server's own collision simulation. A client-supplied position is never an input.
	 *
	 * Returns false (never ensures) for the ordinary rejections: consumed, dead pawn, pawn
	 * not actually overlapping. Rejection is an expected outcome of a contested pickup, not
	 * an error.
	 */
	bool CanBeCollectedBy(APawn* Collector) const;

	/**
	 * Server-only. Consumes the pickup for Collector: re-runs CanBeCollectedBy, marks
	 * consumed, broadcasts OnCollected (the spawner's cue to start its countdown), and
	 * destroys the actor.
	 *
	 * @return true when the pickup was consumed by this call. False means somebody else won
	 *         the race, which is a legal outcome the caller must handle.
	 */
	bool TryCollect(APawn* Collector);

	/** Server-side: fired from TryCollect, before the actor is destroyed. */
	FBROnWeaponPickupCollected OnCollected;

	// -- BP06 SEAM: server-authoritative attraction ------------------------------------------

	/**
	 * BP06 SEAM (the Grappleshot's weapon-attract mode). Asks this pickup to travel to
	 * TargetLocation under SERVER authority.
	 *
	 * What BP03 provides and guarantees:
	 *   - the authority gate (a client calling this changes nothing),
	 *   - the replicated in-flight state, so every client sees the same travel,
	 *   - movement replication switched on for the duration and off again after,
	 *   - a refusal to attract a consumed pickup,
	 *   - OnAttractRequested, which the grapple's driver binds.
	 *
	 * What BP03 deliberately does NOT provide: the motion itself. Speed, arc, interruption
	 * on line-of-sight loss and the catch radius are grapple rules and grapple numbers, and
	 * inventing them here would put grapple tuning in the weapons folder. If nothing is bound
	 * to OnAttractRequested, this logs an error and refuses — it does not silently no-op.
	 *
	 * @return true when the request was accepted and broadcast.
	 */
	virtual bool AttractTo(const FVector& InTargetLocation, AActor* InRequester);

	/** Server-only. Ends an in-flight attraction and restores the resting replication state. */
	virtual void CancelAttract();

	bool IsBeingAttracted() const { return bAttracting; }
	const FVector& GetAttractTargetLocation() const { return AttractTargetLocation; }
	AActor* GetAttractRequester() const { return AttractRequester; }

	/** BP06 binds its motion driver here. Broadcast on the server only, from AttractTo. */
	FBROnWeaponPickupAttractRequested OnAttractRequested;

protected:
	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UFUNCTION()
	void OnRep_WeaponRow();

	/** Kicks off the async load of the row's SOFT mesh reference. Cosmetic; skipped on a dedicated server. */
	void RequestMeshLoad();
	void OnMeshLoaded();

	/** Overlap collision + query volume. Root. */
	UPROPERTY(VisibleAnywhere, Category = "Pickup")
	TObjectPtr<USphereComponent> InteractionSphere;

	/** Cosmetic only — no collision, never consulted by a rule. */
	UPROPERTY(VisibleAnywhere, Category = "Pickup")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	/**
	 * CONTRACT_GAP (BP03 step 1): the interaction radius is a gameplay number and law 3 puts
	 * gameplay numbers in a CSV — but no table owns pickup interaction, and this packet may
	 * not invent a column. It is EditDefaultsOnly here as a PLACEHOLDER, not a balance call;
	 * it carries no design ruling (unlike the spawner's 90 s, which refuses to be guessed).
	 * Move it to data and this property becomes a read of that row.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Pickup", meta = (ClampMin = "1.0", ForceUnits = "cm"))
	float InteractionRadiusCm = 120.f;

private:
	/** Which DT_Weapons row this pickup grants. Public information. */
	UPROPERTY(ReplicatedUsing = OnRep_WeaponRow)
	FDataTableRowHandle WeaponRow;

	/** Ammunition carried into the collector's hands. Public information — see the class comment. */
	UPROPERTY(Replicated)
	int32 AmmoInMag = 0;

	UPROPERTY(Replicated)
	int32 AmmoReserve = 0;

	/**
	 * Server truth for "already taken". Replicated so a client stops drawing the prompt on
	 * the frame the race is lost rather than on the frame the actor is destroyed.
	 */
	UPROPERTY(Replicated)
	bool bConsumed = false;

	/** In-flight attraction state (BP06 seam). Replicated so every client sees one travel. */
	UPROPERTY(Replicated)
	bool bAttracting = false;

	UPROPERTY(Replicated)
	FVector AttractTargetLocation = FVector::ZeroVector;

	UPROPERTY(Replicated)
	TObjectPtr<AActor> AttractRequester;

	/** Server-only: pawns currently inside the sphere, per the SERVER's own collision. */
	TSet<TWeakObjectPtr<APawn>> OverlappingPawns;

	/** Keeps the async mesh load alive; released on EndPlay. */
	TSharedPtr<FStreamableHandle> MeshLoadHandle;
};

/**
 * ABRPowerWeaponSpawner — the map node that puts the Rocket in the world on a timer.
 *
 * Specification: ARCHITECTURE §3.5 ("90 s node") and ruling R4 ("the Rocket is balanced by
 * scarcity, not by damage: map pickup, 90 s timer, 2 shots, ReserveMags 0").
 *
 * THE COUNTDOWN IS AN ABSOLUTE SERVER TIMESTAMP, not a ticking float. One replicated value
 * changes twice per cycle (collected, respawned) instead of every frame, and every client
 * computes its own remaining time against GetServerWorldTimeSeconds. This is what makes the
 * contest fair: two clients on different pings read the same deadline, not two local clocks.
 *
 * The respawn itself is a timer callback (law 4: no Tick).
 */
UCLASS()
class BREACHPOINT_API ABRPowerWeaponSpawner : public AActor
{
	GENERATED_BODY()

public:
	ABRPowerWeaponSpawner();

	//~ Begin AActor interface
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End AActor interface

	/**
	 * Server-only. Starts the node: spawns the pickup immediately if none is present.
	 * Refuses (with an error, changing nothing) when the node is not fully configured — see
	 * RespawnIntervalSeconds.
	 */
	void ArmSpawner();

	/** Server-only. Stops the cycle and clears any pending respawn. Leaves a present pickup alone. */
	void DisarmSpawner();

	bool IsPickupAvailable() const;

	/**
	 * Seconds until the next spawn, for the HUD/GameState mirror. Client-safe: computed from
	 * the replicated deadline against server world time.
	 * Returns 0 when a pickup is already available or the node is disarmed.
	 */
	float GetSecondsUntilRespawn() const;

	/** Absolute server world time of the next spawn; negative when not counting. */
	float GetRespawnAvailableServerTime() const { return RespawnAvailableServerTime; }

protected:
	void SpawnPickup();
	void HandlePickupCollected(ABRWeaponPickup* Pickup, APawn* Collector);

	/**
	 * Server world time, from the GameState. Returns a negative value when the GameState has
	 * not arrived yet (netcode.md law 7: a client's first frames have no GameState) — callers
	 * treat that as "unknown", never as time zero.
	 */
	float GetServerTimeSeconds() const;

	UFUNCTION()
	void OnRep_RespawnAvailableServerTime();

	/** Which weapon this node grants — the Rocket row of DT_Weapons. Set per placed instance. */
	UPROPERTY(EditInstanceOnly, Category = "Power Weapon")
	FDataTableRowHandle WeaponRow;

	/** Which pickup actor class to spawn. No default: an unset class is a refusal, not a guess. */
	UPROPERTY(EditInstanceOnly, Category = "Power Weapon")
	TSubclassOf<ABRWeaponPickup> PickupClass;

	/**
	 * Seconds between a collection and the next spawn.
	 *
	 * CONTRACT_GAP (BP03 step 1): ruling R4 fixes this at 90 s, and law 3 says a number that
	 * carries a design ruling lives in a table — DT_MatchRules is its home. That row struct
	 * (FBRMatchRulesRow) is being authored by BP02 and has no column for it yet, and this
	 * packet may not invent one.
	 *
	 * So the default is DELIBERATELY INVALID (-1). An unconfigured node refuses to arm and
	 * says so in the log. Defaulting to 90.f here would be a literal gameplay constant in sim
	 * code, and worse: a silently-correct number that nobody would ever notice was in the
	 * wrong place until a designer tried to change it and could not.
	 */
	UPROPERTY(EditInstanceOnly, Category = "Power Weapon", meta = (ForceUnits = "s"))
	float RespawnIntervalSeconds = -1.f;

	/** Arm on BeginPlay. Clear this when the GameMode's phase machine should own the cycle. */
	UPROPERTY(EditInstanceOnly, Category = "Power Weapon")
	bool bArmOnBeginPlay = true;

private:
	/** The pickup currently in the world, if any. Replicated so the HUD can point at it. */
	UPROPERTY(Replicated)
	TObjectPtr<ABRWeaponPickup> CurrentPickup;

	/**
	 * Absolute SERVER world time at which the next pickup appears; negative = not counting
	 * (a pickup is present, or the node is disarmed). ARCHITECTURE §6.1: COND_None.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_RespawnAvailableServerTime)
	float RespawnAvailableServerTime = -1.f;

	UPROPERTY(Replicated)
	bool bArmed = false;

	FTimerHandle RespawnTimerHandle;
};
