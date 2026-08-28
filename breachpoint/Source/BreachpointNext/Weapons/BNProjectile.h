#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/TimerHandle.h"
#include "BNProjectile.generated.h"

class UFXSystemAsset;
class UProjectileMovementComponent;
class USphereComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * The thrown grenade, in C++, and it exists for exactly one reason: purity law 3.
 *
 * The template's `BP_FPST_Grenade` is a Blueprint whose explosion is (almost certainly) the engine
 * damage API — `ApplyRadialDamage` — which the contract BANS outright. Spawning it would have
 * imported a second damage pipeline that bypasses attributes, shields and death entirely: damage
 * that BNDamage never logged and GAS never saw. The contract names the cure in the same breath —
 * *"Radial = our own overlap query -> per-target GE application"* — and this is that query.
 *
 * Multiplayer: the projectile is the SERVER'S. It replicates its movement, so every machine watches
 * one simulation rather than guessing at its own; the fuse, the overlap and every point of damage
 * are authority-only. Nothing here is predicted, which is the same call UBNGA_Grenade makes and for
 * the same reason — two simulated projectiles diverge on the first bounce with nothing to
 * reconcile them.
 *
 * Every asset it wears is the template's, referenced through Config: SM_grenade, NS_Grenade_Trail,
 * and the explosion cue's own NS_Grenade_Explosion + MSS_Explosions_Grenade.
 */
UCLASS(Config = Game)
class BREACHPOINTNEXT_API ABNProjectile : public AActor
{
	GENERATED_BODY()

public:
	ABNProjectile();

	/** The thrower's aim, applied as launch velocity. Authority only — called by UBNGA_Grenade
	 *  immediately after spawn, before the fuse can matter. */
	void Launch(const FVector& Direction);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** The launch velocity, replicated so a client reproduces the same arc instead of watching a
	 *  grenade spawn at rest and get dragged along by movement corrections. */
	UPROPERTY(ReplicatedUsing = OnRep_InitialVelocity)
	FVector InitialVelocity = FVector::ZeroVector;

	UFUNCTION()
	void OnRep_InitialVelocity();

	/** Authority only. Overlap, dedupe, line-of-sight, falloff, one GE per survivor. */
	void Explode();

	/**
	 * R10.4 — TELL THE BOTS, a beat before the bang. The single most-missed behaviour in every
	 * review of Halo: Campaign Evolved was Elites no longer dodging grenades, and BN had none of
	 * it: we threw grenades and nothing reacted to one landing at its feet.
	 *
	 * PUSHED from the grenade, not polled by the bots (law 4). One overlap on one timer, and only
	 * the bots actually inside the blast radius ever hear about it — a poll would have every bot
	 * in the level asking every evaluation whether anything is about to explode.
	 *
	 * The warning is sent from the grenade's CURRENT position, which is where it will detonate
	 * unless it is still rolling. A grenade still in the air warns about where it is, and the bot
	 * that moves out of that circle has done the right thing either way.
	 */
	void WarnNearbyBots();

	/** The bang, sent from the PROJECTILE rather than routed through the thrower's ASC — that route
	 *  early-returns wherever the thrower's AvatarActor is null on the receiving machine, so an
	 *  observer who had culled the thrower would take the blast in silence. This actor is relevant
	 *  to everyone who can see the explosion by definition: it is standing where it happens.
	 *
	 *  RELIABLE, per the critic: Unreliable meant one dropped packet and a client saw the victim's
	 *  replicated Health fall and the grenade vanish with no bang and no way to recover inside the
	 *  0.5s lifespan. Reliable multicasts are rationed, and a grenade detonation — rare, one-shot,
	 *  gameplay-legible — is exactly what the ration is for. */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastExplosion(const FVector Center);

	UPROPERTY(VisibleAnywhere, Category = "BN|Projectile")
	TObjectPtr<USphereComponent> CollisionComponent;

	UPROPERTY(VisibleAnywhere, Category = "BN|Projectile")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	UPROPERTY(VisibleAnywhere, Category = "BN|Projectile")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	// ---------------------------------------------------------------- tuning, all Config
	/** Seconds from leaving the hand to the bang. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Projectile")
	float FuseTime = 3.f;

	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Projectile")
	float LaunchSpeed = 1600.f;

	/** Full damage inside InnerRadius, falling linearly to zero at Radius. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Projectile")
	float Damage = 90.f;

	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Projectile")
	float InnerRadius = 150.f;

	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Projectile")
	float Radius = 500.f;

	/** How long before the bang the bots are told. Long enough to cross the radius at walking
	 *  speed, short enough that a bot does not abandon a fight over a grenade thrown elsewhere:
	 *  at 600uu/s, 1.2s covers 720uu against a 500uu radius. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Projectile")
	float BotWarnLeadSeconds = 1.2f;

	/** A wall stops a blast. Server-side, same discipline as the fire confirm trace — without it
	 *  a grenade on the far side of cover kills through it. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Projectile")
	bool bRequireLineOfSight = true;

	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Projectile")
	float Bounciness = 0.3f;

	// ---------------------------------------------------------------- assets, all template, soft
	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Projectile")
	TSoftObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Projectile")
	TSoftObjectPtr<UFXSystemAsset> TrailEffect;

	/** The trail's team-colour parameter. NS_Grenade_Trail is one of only four systems in the
	 *  whole project that declares a colour USER parameter at all, and it declares this one —
	 *  so the grenade in FLIGHT can read as ally or threat exactly like its explosion already
	 *  does. Same contract as UBNGameplayCue_Base::TintParameter: a name the system does not
	 *  declare is a SILENT no-op, so this is config, not a literal. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Team")
	FName TrailTintParameter = TEXT("User.Team_Color");

	/** Tint YOUR OWN grenade's trail too. Matches UBNGameplayCue_Explosion's bTintOwnEffects,
	 *  and must: a trail that stays neutral while its own blast turns blue is a grenade that
	 *  changes colour the instant it lands. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Team")
	bool bTintOwnTrail = true;

	FTimerHandle FuseTimer;

	/** The bot warning's own timer — see WarnNearbyBots. */
	FTimerHandle WarnTimer;

	/** WHO threw this, captured at BeginPlay while the pawn link is alive (BN15 REFUTER
	 *  N1): a thrower killed mid-flight is unpossessed — pawn PlayerState nulled — before
	 *  the fuse runs, and RespawnDelay equals the fuse today, so the race is a knife-edge
	 *  every match. The blast passes THIS to the damage door when the pawn route reads
	 *  null, and the FF gate's ladder accepts a PlayerState directly. Weak: a leaver's
	 *  PlayerState dying mid-flight honestly degrades to the old (world-damage) answer. */
	TWeakObjectPtr<class APlayerState> ThrowerPlayerState;
};
