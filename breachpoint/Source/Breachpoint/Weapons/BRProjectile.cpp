// Breachpoint. The server-authoritative projectile — D6's fourth unit of ARCHITECTURE §3.5.

#include "Weapons/BRProjectile.h"

#include "AbilitySystemComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/BRCore.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "TimerManager.h"
#include "Weapons/BRExplosion.h"

namespace
{
	/**
	 * In-flight replication rate, in updates per second.
	 *
	 * STRUCTURAL, not gameplay — a BANDWIDTH figure, not a balance figure, and nobody tunes it in a
	 * CSV any more than they tune `MetresToUU`. It is low on purpose: `netcode.md` law 4, and the
	 * client's own `UProjectileMovementComponent` integrates the arc between updates. A grenade
	 * follows gravity, not a player's mind, so the interpolated path between two sparse updates is
	 * the SAME path the server took — which is exactly the property a pawn does not have and the
	 * reason a pawn's rate cannot be cut this way.
	 *
	 * The actor is dormant at rest, so this rate is paid only while it is actually moving.
	 */
	constexpr float ProjectileNetUpdateHz = 10.f;
	constexpr float ProjectileMinNetUpdateHz = 2.f;
}

ABRProjectile::ABRProjectile()
{
	// Law 4. The fuse is a timer and the rest event is a delegate; the actor itself needs no frame.
	// (`UProjectileMovementComponent` ticks — engine machinery, see the class comment.)
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	bReplicates = true;

	// Unlike ABRWeaponPickup, this actor's whole job is to MOVE. Movement replication stays on for
	// its entire life: it is switched off nowhere, because the moment it stops moving it goes
	// dormant instead, which costs less and cannot leave a client's copy a few centimetres short.
	SetReplicatingMovement(true);

	// Awake at birth — it is in flight from frame one. `HandleProjectileStop` puts it to sleep.
	// (Compare the pickup's DORM_Initial: a pickup is born asleep because it is born at rest.)
	NetDormancy = DORM_Awake;
	SetNetUpdateFrequency(ProjectileNetUpdateHz);
	SetMinNetUpdateFrequency(ProjectileMinNetUpdateHz);

	// ------------------------------------------------------------------------------------------
	// The hull. `BRCollision::Projectile` — THE ALIAS, never the raw ECC_GameTraceChannel1.
	// ------------------------------------------------------------------------------------------
	//
	// `BRCore.h` is explicit about why: the alias table and the ini's [CollisionProfile] must agree,
	// a mismatch compiles clean, and the failure mode is that things silently stop colliding. The
	// alias is the only spelling that a rename of the channel can follow.
	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	SetRootComponent(CollisionComponent);
	CollisionComponent->InitSphereRadius(CollisionRadiusCm);

	// QueryOnly, not QueryAndPhysics: `UProjectileMovementComponent` moves this component with swept
	// QUERIES and computes the bounce itself. Rigid-body simulation would put a second, disagreeing
	// integrator on the same actor and would not be deterministic across machines.
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComponent->SetCollisionObjectType(BRCollision::Projectile);
	CollisionComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionComponent->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	CollisionComponent->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);

	// PAWNS ARE IGNORED, and the class comment gives the three reasons. Spelled out here as well as
	// declared, because `SetCollisionResponseToAllChannels(ECR_Ignore)` already covers it and a
	// reader would otherwise not know whether the omission was a decision or an oversight.
	CollisionComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

	// Two grenades in the air do not deflect each other. Same reasoning: no ruling covers it, and
	// projectile-on-projectile bounce is a physics toy, not a mechanic anyone asked for.
	CollisionComponent->SetCollisionResponseToChannel(BRCollision::Projectile, ECR_Ignore);

	CollisionComponent->SetGenerateOverlapEvents(false);
	CollisionComponent->CanCharacterStepUpOn = ECB_No;

	// Cosmetic. It must never be able to answer a gameplay question, so it answers no query.
	// EMPTY until Tier 4 art exists — see the class comment's "what this class still owes".
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(CollisionComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetGenerateOverlapEvents(false);

	// ------------------------------------------------------------------------------------------
	// Ballistics.
	// ------------------------------------------------------------------------------------------
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));

	// D6: *"the grenade needs a projectile with bounce physics and a fuse."* PMC's own default is
	// false, so this line is the bounce. BP09's rocket clears it — one line, named, not written.
	ProjectileMovement->bShouldBounce = true;

	// THE TRAP, defused in the constructor rather than survived at runtime. PMC's own defaults are
	// `Velocity = (1,0,0)` and `bInitialVelocityInLocalSpace = true`, and its `InitializeComponent`
	// (which runs during the DEFERRED spawn's component registration, i.e. BEFORE
	// `InitializeProjectile`) rotates any non-zero velocity into world space. Starting from zero
	// makes that a no-op and lets `InitializeProjectile` set a plain world-space velocity that
	// nothing reinterprets.
	ProjectileMovement->Velocity = FVector::ZeroVector;
	ProjectileMovement->bInitialVelocityInLocalSpace = false;

	// InitialSpeed 0: the launch VELOCITY carries both direction and speed, and it comes from the
	// thrower's data. A non-zero InitialSpeed here would silently renormalise it — a hardcoded
	// throw speed hiding inside a component default.
	ProjectileMovement->InitialSpeed = 0.f;

	// MaxSpeed 0 = unlimited. Clamping to the launch speed would cap a long fall's terminal
	// velocity at the throw speed, which is a physics bug that reads as "grenades float".
	ProjectileMovement->MaxSpeed = 0.f;

	// Bounciness/Friction are DELIBERATELY not set here. They are tuning, they arrive at the spawn,
	// and `InitializeProjectile` explains what happens when the data does not exist yet.
}

// ================================================================================================
// Spawning
// ================================================================================================

bool ABRProjectile::ValidateSpawnParams(const FBRProjectileSpawnParams& InParams, FString& OutReason)
{
	// ONE success/failure answer, the shape `UBRGA_Grenade::ResolveTuning` uses for the same reason:
	// a projectile with four of five numbers is not "mostly configured", it is a refusal.
	if (!IsValid(InParams.InstigatorActor.Get()))
	{
		OutReason = TEXT("no instigator actor — a projectile with no thrower produces a kill with no killer");
		return false;
	}
	if (!InParams.InstigatorASC.IsValid())
	{
		OutReason = TEXT("no instigator ASC — the damage spec has no source and could not be attributed");
		return false;
	}
	if (InParams.LaunchVelocity.IsNearlyZero())
	{
		OutReason = TEXT("zero launch velocity — the projectile would be dropped on the thrower's boots");
		return false;
	}
	if (InParams.FuseSeconds < 0.f)
	{
		OutReason = FString::Printf(TEXT("fuse is %.3f s; negative is unset, and a projectile with no fuse never detonates"), InParams.FuseSeconds);
		return false;
	}
	if (InParams.BlastRadiusMetres <= 0.f || InParams.BlastCentreDamage <= 0.f)
	{
		OutReason = FString::Printf(TEXT("blast radius %.2f m / centre damage %.2f; both come from data and neither may be invented"),
			InParams.BlastRadiusMetres, InParams.BlastCentreDamage);
		return false;
	}
	if (InParams.BlastFalloffCurveName.IsNone())
	{
		OutReason = TEXT("no falloff curve name — the detonation would have no authored shape and this class may not compute one");
		return false;
	}

	// NOTE: `ExplodeCueTag` is NOT checked. An undeclared cue leaf makes the explosion invisible,
	// not wrong — FX and gameplay are separate legs, and refusing to spawn a grenade because its
	// FX tag is owed would trade a silent blast for no blast at all. The caller logs the gap.
	return true;
}

ABRProjectile* ABRProjectile::SpawnProjectile(
	UWorld* World,
	TSubclassOf<ABRProjectile> ProjectileClass,
	const FTransform& SpawnTransform,
	const FBRProjectileSpawnParams& InParams)
{
	if (!World)
	{
		UE_LOG(LogBRCombat, Error, TEXT("ABRProjectile::SpawnProjectile: no world; nothing spawned."));
		return nullptr;
	}

	// `gas-purity` §4: authoritative actors are spawned on the authority and nowhere else. The
	// caller gates on `HasAuthority` already; this is the backstop that makes a future caller's
	// mistake loud instead of desyncing.
	if (World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogBRCombat, Error,
			TEXT("ABRProjectile::SpawnProjectile called on a CLIENT. A predictively spawned projectile "
				 "has no rollback path; refused."));
		return nullptr;
	}

	// Refuse, never substitute. The pickup's factory takes the same position about its class.
	if (!ProjectileClass)
	{
		UE_LOG(LogBRCombat, Error, TEXT("ABRProjectile::SpawnProjectile: no ProjectileClass supplied; nothing spawned."));
		return nullptr;
	}

	FString Reason;
	if (!ValidateSpawnParams(InParams, Reason))
	{
		UE_LOG(LogBRCombat, Error, TEXT("ABRProjectile::SpawnProjectile refused: %s."), *Reason);
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	// ATTRIBUTION. Kill credit, `Event.Kill`, the killfeed and the effect context all read from
	// these; a projectile with no instigator produces a kill with no killer.
	SpawnParams.Owner = InParams.InstigatorActor.Get();
	SpawnParams.Instigator = Cast<APawn>(InParams.InstigatorActor.Get());

	// AlwaysSpawn, and NOT the pickup's AdjustIfPossible: a grenade leaves the hand from the
	// thrower's eye point, and nudging it to find clear space would teleport it out of the ray the
	// ability computed — the one thing the client's ghost and the server's projectile must agree on.
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// DEFERRED: configure, then finish. `BeginPlay` arms the fuse, so the payload must be in place
	// before it runs — a projectile that lived even one frame without its payload could detonate
	// without one.
	ABRProjectile* Projectile = World->SpawnActorDeferred<ABRProjectile>(
		ProjectileClass,
		SpawnTransform,
		SpawnParams.Owner,
		SpawnParams.Instigator,
		SpawnParams.SpawnCollisionHandlingOverride);

	if (!Projectile)
	{
		UE_LOG(LogBRCombat, Error, TEXT("ABRProjectile::SpawnProjectile: deferred spawn of '%s' failed; nothing is in the air."),
			*GetNameSafe(ProjectileClass));
		return nullptr;
	}

	Projectile->InitializeProjectile(InParams);
	Projectile->FinishSpawning(SpawnTransform);
	return Projectile;
}

void ABRProjectile::InitializeProjectile(const FBRProjectileSpawnParams& InParams)
{
	if (!ensureMsgf(HasAuthority(), TEXT("ABRProjectile::InitializeProjectile is server-only.")))
	{
		return;
	}
	if (bInitialized)
	{
		UE_LOG(LogBRCombat, Error,
			TEXT("%s: InitializeProjectile called twice. The second payload was DISCARDED — one "
				 "projectile carries one thrower's numbers."),
			*GetName());
		return;
	}

	FString Reason;
	if (!ValidateSpawnParams(InParams, Reason))
	{
		// Reachable only when someone bypasses the factory. Loud, and the projectile stays
		// uninitialised so `BeginPlay` destroys it rather than flying with a hollow payload.
		UE_LOG(LogBRCombat, Error, TEXT("%s: InitializeProjectile refused: %s."), *GetName(), *Reason);
		return;
	}

	Params = InParams;
	bInitialized = true;

	if (!ProjectileMovement)
	{
		UE_LOG(LogBRCombat, Error, TEXT("%s: no ProjectileMovementComponent; it will not move."), *GetName());
		return;
	}

	// World-space, straight from the thrower. See the constructor on why nothing reinterprets it.
	ProjectileMovement->Velocity = InParams.LaunchVelocity;

	// ------------------------------------------------------------------------------------------
	// BOUNCE — and a CONTRACT_GAP stated rather than papered over
	// ------------------------------------------------------------------------------------------
	//
	// `CT_Combat` has **no `Grenade.Bounciness` and no `Grenade.BounceFriction` row** (verified
	// 1 Aug 2026; the eight `Grenade.*` rows are cook, fuse, throw speed, blast radius, centre
	// damage, the falloff curve, the carried max and the per-throw cost). Restitution is tuning and
	// its home is a CSV row, which no code packet may add.
	//
	// So the sentinel is negative and this is what a negative means: **we do not write the value at
	// all.** `UProjectileMovementComponent`'s own default stands, and the log names both the owed
	// rows and the number actually in force, so a designer reads the real figure rather than
	// guessing. Nothing in this project's source types a bounce number.
	//
	// WHY THIS IS NOT THE USUAL REFUSAL. `BRCombatCurves::Evaluate`'s contract is explicit that
	// "every caller decides what a missing coefficient means for IT and says so at the call site",
	// and the five numbers `ResolveTuning` refuses over are all meaningless at their sentinel — a
	// zero fuse, a zero radius, a zero throw speed. A grenade with the engine's restitution is not
	// meaningless: it flies, bounces and kills, and it is visibly wrong to a designer within one
	// throw. Refusing here would take the entire feature down over a physics constant, which is a
	// worse trade than a loud warning. Add the two rows and this branch stops firing.
	if (InParams.Bounciness >= 0.f)
	{
		ProjectileMovement->Bounciness = InParams.Bounciness;
	}
	if (InParams.BounceFriction >= 0.f)
	{
		ProjectileMovement->Friction = InParams.BounceFriction;
	}
	if (InParams.Bounciness < 0.f || InParams.BounceFriction < 0.f)
	{
		UE_LOG(LogBRCombat, Warning,
			TEXT("%s: bounce tuning is OWED. No usable 'Grenade.Bounciness'/'Grenade.BounceFriction' "
				 "value arrived (the CT_Combat row is absent, or authored negative), so this projectile "
				 "is bouncing on UProjectileMovementComponent's own values (restitution %.2f, friction "
				 "%.2f). No bounce number was invented in code — author the rows and they take over."),
			*GetName(), ProjectileMovement->Bounciness, ProjectileMovement->Friction);
	}
}

// ================================================================================================
// Life
// ================================================================================================

void ABRProjectile::BeginPlay()
{
	Super::BeginPlay();

	// The editable radius is the truth; the constructor's InitSphereRadius only seeds the CDO.
	// (Same pattern, same reason, as ABRWeaponPickup's interaction sphere.)
	if (CollisionComponent)
	{
		CollisionComponent->SetSphereRadius(CollisionRadiusCm);
	}

	if (ProjectileMovement)
	{
		// The rest event, on EVERY machine: the server uses it to go dormant, clients use nothing —
		// but PMC unregisters its own tick on stop, so binding here also means neither side keeps
		// integrating a grenade that has stopped.
		ProjectileMovement->OnProjectileStop.AddDynamic(this, &ABRProjectile::HandleProjectileStop);
	}

	if (!HasAuthority())
	{
		// A client's copy is a spectator: it watches the replicated transform, runs PMC between
		// updates, and detonates nothing. No fuse is armed here on purpose.
		return;
	}

	if (!bInitialized)
	{
		// Only reachable when something spawned this class directly instead of through the factory
		// (a level-placed instance, a Blueprint spawn node). An uninitialised projectile has no
		// thrower and no payload, so it cannot detonate and must not sit in the world pretending.
		UE_LOG(LogBRCombat, Error,
			TEXT("%s: reached BeginPlay with NO payload — spawned outside ABRProjectile::SpawnProjectile. "
				 "Destroying it rather than leaving an inert grenade in the level."),
			*GetName());
		Destroy();
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogBRCombat, Error, TEXT("%s: no world at BeginPlay, so the fuse cannot be armed; destroying."), *GetName());
		Destroy();
		return;
	}

	// ------------------------------------------------------------------------------------------
	// THE FUSE. A TIMER, never a Tick (law 4).
	// ------------------------------------------------------------------------------------------
	//
	// It is already net-corrected by construction: `Params.FuseSeconds` is what the SERVER's own
	// copy of the ability computed (`FuseSeconds - SecondsCooked`), so a laggy client's cook cannot
	// buy or lose fuse time. There is nothing to reconcile because the client never had a number.
	//
	// `Max(fuse, KINDA_SMALL_NUMBER)`: a fully-cooked grenade arrives with zero remaining fuse and
	// must go off immediately — but a zero-length timer fires on the NEXT tick, not inside
	// BeginPlay, and that matters. Detonating inside BeginPlay would apply damage before the actor
	// finished spawning and before any client had heard of it.
	World->GetTimerManager().SetTimer(
		FuseTimerHandle, this, &ABRProjectile::HandleFuseExpired,
		FMath::Max(Params.FuseSeconds, UE_KINDA_SMALL_NUMBER), /*bLoop=*/false);

	UE_LOG(LogBRCombat, Verbose,
		TEXT("%s: armed. %.2f s of fuse, %.0f uu/s launch, blast %.2f m / %.1f centre, thrower '%s'."),
		*GetName(), Params.FuseSeconds, Params.LaunchVelocity.Size(),
		Params.BlastRadiusMetres, Params.BlastCentreDamage, *GetNameSafe(Params.InstigatorActor.Get()));
}

void ABRProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// EVERY path, including a level teardown mid-flight. A leaked fuse fires into a dead actor.
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FuseTimerHandle);
	}
	FuseTimerHandle.Invalidate();

	if (ProjectileMovement)
	{
		ProjectileMovement->OnProjectileStop.RemoveDynamic(this, &ABRProjectile::HandleProjectileStop);
	}

	Super::EndPlay(EndPlayReason);
}

void ABRProjectile::HandleProjectileStop(const FHitResult& ImpactResult)
{
	bAtRest = true;

	UE_LOG(LogBRCombat, Verbose, TEXT("%s: came to rest on '%s'."),
		*GetName(), *GetNameSafe(ImpactResult.GetActor()));

	if (!HasAuthority())
	{
		// Dormancy is a server-side concept. A client reaching here has simply stopped simulating.
		return;
	}

	// ------------------------------------------------------------------------------------------
	// DORMANCY AT REST — `netcode.md` law 4, and the whole reason the class comment budgets for it.
	// ------------------------------------------------------------------------------------------
	//
	// A grenade that has stopped moving changes nothing until it detonates. Going dormant costs the
	// server nothing per client for the rest of the fuse.
	//
	// THE FINAL TRANSFORM STILL ARRIVES: `SetNetDormancy` marks the channel PENDING dormant, and the
	// channel replicates once more before it sleeps. That is what makes it safe to sleep here rather
	// than a beat later — the grenade does not freeze on a client a few centimetres short of where
	// the server left it. Movement replication is deliberately NOT switched off (compare
	// `ABRWeaponPickup::CancelAttract`, which does): with the actor dormant it sends nothing either
	// way, and leaving it on means that pending update carries the resting position.
	SetNetDormancy(DORM_DormantAll);
}

// ================================================================================================
// Detonation
// ================================================================================================

void ABRProjectile::HandleFuseExpired()
{
	// The fuse is the ONLY thing that detonates a projectile today. Detonating at rest instead would
	// contradict the dormancy above (an actor that slept for one frame and then died) and would make
	// a bounced grenade a different weapon from a thrown one. Impact detonation is BP09's, named in
	// the class comment, and not written speculatively here.
	Detonate();
}

void ABRProjectile::Detonate()
{
	if (!HasAuthority())
	{
		// A client that somehow reached here would compute its own blast from its own approximate
		// positions and disagree with the server about who died.
		UE_LOG(LogBRCombat, Error, TEXT("%s: Detonate called off the authority; refused."), *GetName());
		return;
	}
	if (bDetonated)
	{
		// One projectile, one blast. Guarded for the same reason `UBRGA_Grenade::ThrowGrenade`
		// guards with `bThrowResolved`: two paths CAN land on one frame the moment BP09 adds impact
		// detonation next to this fuse.
		return;
	}
	bDetonated = true;

	// Stop the fuse before anything else can re-enter, and stop moving: a grenade must not travel a
	// further half-metre between the epicentre being read and the overlap being run.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FuseTimerHandle);
	}
	if (ProjectileMovement)
	{
		ProjectileMovement->StopMovementImmediately();
	}

	const FVector Epicentre = GetActorLocation();

	// THE PAYLOAD IS THE ONE THE THROWER RESOLVED, not a fresh read of the table. See
	// `FBRProjectileSpawnParams`.
	//
	// `Params.InstigatorASC` is WEAK: a thrower who disconnected mid-flight resolves to null and
	// `BRExplosion::ApplyExplosionDamage` refuses out loud, which is its documented behaviour and
	// the right one — damage with no source cannot be attributed. Note the ordinary death case does
	// NOT land there: the ASC lives on the PlayerState, which survives a death and a respawn, so a
	// grenade thrown by someone who dies before it goes off still kills and still credits them.
	const int32 Damaged = BRExplosion::ApplyExplosionDamage(
		Params.InstigatorASC.Get(),
		Params.InstigatorActor.Get(),
		Epicentre,
		Params.BlastRadiusMetres,
		Params.BlastCentreDamage,
		Params.BlastFalloffCurveName,
		Params.ExplodeCueTag);

	UE_LOG(LogBRCombat, Verbose, TEXT("%s: detonated at %s, %d target(s) damaged."),
		*GetName(), *Epicentre.ToCompactString(), Damaged);

	// Wake it for the destruction. The actor is dormant if it detonated at rest, and the pickup's
	// discipline — every state change on a dormant actor paired with a flush — applies to the last
	// one as much as to any other.
	FlushNetDormancy();
	Destroy();
}
