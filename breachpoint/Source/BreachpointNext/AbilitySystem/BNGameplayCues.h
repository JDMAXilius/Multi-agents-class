#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "GameplayTagContainer.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/SoftObjectPtr.h"
#include "Chaos/ChaosEngineInterface.h"
#include "BNGameplayCues.generated.h"

class UFXSystemAsset;
class USoundBase;
class UForceFeedbackEffect;
class UCameraShakeBase;

/** One surface's answer, the template's ImpactEffectInfoMap row: which burst plays and which
 *  sound, keyed by the hit's physical surface. The decal system is shared across surfaces
 *  (the template points every row at NS_ImpactDecals) so it lives on the cue, not here. */
USTRUCT()
struct FBNImpactEffectRow
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "BN|Cue")
	TEnumAsByte<EPhysicalSurface> Surface = SurfaceType_Default;

	UPROPERTY(EditDefaultsOnly, Category = "BN|Cue")
	TSoftObjectPtr<UFXSystemAsset> Effect;

	UPROPERTY(EditDefaultsOnly, Category = "BN|Cue")
	TSoftObjectPtr<USoundBase> Sound;
};

/**
 * Weapon FX, law 2: every effect this game shows comes through a cue, and every cue handler is a
 * C++ class. Each one degrades SILENTLY when its Effect is unset — the FX assets are ANNOUNCED,
 * not created by this packet, and an unset one must cost nothing at runtime and log nothing.
 *
 * TSoftObjectPtr<UFXSystemAsset> rather than a Niagara type: UFXSystemAsset is the Engine-module
 * base of both systems, so no public header names a Niagara type and Niagara stays a PRIVATE
 * module dependency.
 */
UCLASS(Abstract)
class BREACHPOINTNEXT_API UBNGameplayCue_Base : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
	/** The tag this handler answers to. Assigned here rather than in the constructor because
	 *  native tags are not guaranteed registered while CDOs are being built. */
	virtual void PostInitProperties() override;

	/** The one native tag this class handles. Public because the registrar asks the CDO for it at
	 *  world begin-play — long after the tag tree exists — rather than trusting what PostInitProperties
	 *  managed to resolve during CDO construction. A cue that registered under an invalid tag is a
	 *  cue nothing ever calls, and it fails silently. */
	virtual FGameplayTag GetHandledCueTag() const { return FGameplayTag(); }

protected:
	/** Null when the asset is unset or fails to load — callers just do nothing. */
	static UFXSystemAsset* Resolve(const TSoftObjectPtr<UFXSystemAsset>& Soft);

	/** The WEAPON's own muzzle socket, never the character's (MyCharacter.h:120-124,
	 *  .cpp:1557-1580). Falls back to the target's transform when SourceObject is not a weapon. */
	static FTransform ResolveMuzzle(const AActor* Target, const FGameplayCueParameters& Parameters);

	static void SpawnAt(const UObject* WorldContext, UFXSystemAsset* Asset, const FVector& Location, const FRotator& Rotation);
};

UCLASS(Config = Game, meta = (DisplayName = "GC_BN_Weapon_MuzzleFlash"))
class BREACHPOINTNEXT_API UBNGameplayCue_MuzzleFlash : public UBNGameplayCue_Base
{
	GENERATED_BODY()

public:
	virtual FGameplayTag GetHandledCueTag() const override;
	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;

protected:

	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Cue")
	TSoftObjectPtr<UFXSystemAsset> Effect;

	/** The gunshot. There was no sound field on this cue at all, which is the whole reason the
	 *  weapon has been silent while reload and footsteps were audible — those ride montage and
	 *  locomotion anim notifies, which never went through a cue. Attached to the muzzle rather
	 *  than played at a world location so it follows a moving shooter. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Cue")
	TSoftObjectPtr<USoundBase> Sound;
};

UCLASS(Config = Game, meta = (DisplayName = "GC_BN_Weapon_Impact"))
class BREACHPOINTNEXT_API UBNGameplayCue_Impact : public UBNGameplayCue_Base
{
	GENERATED_BODY()

public:
	virtual FGameplayTag GetHandledCueTag() const override;
	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;

protected:

	/** The fallback burst when no row matches the surface (or the hit carries no phys mat) —
	 *  the template's own fallback is its SurfaceType2 (concrete) row. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Cue")
	TSoftObjectPtr<UFXSystemAsset> Effect;

	/** The bullet hole. One system for every surface, exactly as the template's map has it;
	 *  told which look to wear via User.ImpactSurfaces. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Cue")
	TSoftObjectPtr<UFXSystemAsset> Decal;

	/** Per-surface burst + sound, the template's ImpactEffectInfoMap flattened for ini. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Cue")
	TArray<FBNImpactEffectRow> SurfaceRows;
};

UCLASS(Config = Game, meta = (DisplayName = "GC_BN_Weapon_Tracer"))
class BREACHPOINTNEXT_API UBNGameplayCue_Tracer : public UBNGameplayCue_Base
{
	GENERATED_BODY()

public:
	virtual FGameplayTag GetHandledCueTag() const override;
	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;

protected:

	/** The template's own tracer contract, read from BPC_FPST_Lyra_FireEffectComp's
	 *  FireTracerEffect graph: spawn at the muzzle, write the hit into the vector ARRAY
	 *  `User.ImpactPositions`, then fire `User.Trigger`. The earlier single-vector BeamEnd
	 *  contract was this cue's invention and matched no shipped system. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Cue")
	TSoftObjectPtr<UFXSystemAsset> Effect;
};

/**
 * The corpse. A ragdoll is a mesh state change, which is presentation — so law 6 puts it here, and
 * that placement is also what makes it VISIBLE: UBNGA_Death is ServerOnly and nothing in it runs on
 * a client, so an executed cue is the only route by which the machines watching a death see one.
 *
 * Before this existed, UBNHealthComponent::OnDeath fired on every machine and its one listener
 * discarded it everywhere but the server — a signal broadcast everywhere and consumed nowhere.
 */
/**
 * BN23 — THE GRAPPLE'S LAUNCH. Everything a player feels in the instant the hook leaves:
 * the report, the recoil in the camera, and the pad kick. Deliberately NOT the rope and
 * NOT the anchor bite — those are their own cues, because they happen at different times
 * and a single cue would have to guess which moment it was serving.
 *
 * Shake and haptics are LOCAL-PLAYER ONLY. A cue runs on every client that can see the
 * actor, so applying feedback to `MyTarget` without that check would shake the camera of
 * everyone watching a teammate grapple — the classic multiplayer cue bug, and invisible
 * in PIE where you are the only viewer.
 *
 * Every asset is soft and unset by default: this packet ANNOUNCES the hooks, it does not
 * author Tier-4 content. An unset field must cost nothing and log nothing.
 */
UCLASS(Config = Game, meta = (DisplayName = "GC_BN_Grapple_Fire"))
class BREACHPOINTNEXT_API UBNGameplayCue_GrappleFire : public UBNGameplayCue_Base
{
	GENERATED_BODY()

public:
	virtual FGameplayTag GetHandledCueTag() const override;
	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;

protected:
	/** The launch report, attached to the muzzle so it follows a moving shooter. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Cue")
	TSoftObjectPtr<USoundBase> Sound;

	/** Muzzle burst at the hook's exit point. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Cue")
	TSoftObjectPtr<UFXSystemAsset> Effect;

	/** Camera kick. TSoftClassPtr because a shake is a CLASS the manager instantiates,
	 *  not an asset instance — the distinction that silently does nothing if confused. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Cue")
	TSoftClassPtr<UCameraShakeBase> Shake;

	/** Pad haptics. Local player only; see the class comment. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Cue")
	TSoftObjectPtr<UForceFeedbackEffect> Haptic;
};

/**
 * BN23 — THE ROPE, and the only cue here with a LIFETIME. OnActive spawns the beam and
 * OnRemove stops it, so the rope exists exactly as long as the pull does.
 *
 * It follows the TRACER's contract rather than inventing one: a beam FX told where to end
 * via the `User.ImpactPositions` vector array, which is the shape the shipped template's
 * systems already read. Anything else compiles and draws nothing.
 */
UCLASS(Config = Game, meta = (DisplayName = "GC_BN_Grapple_Rope"))
class BREACHPOINTNEXT_API UBNGameplayCue_GrappleRope : public UBNGameplayCue_Base
{
	GENERATED_BODY()

public:
	virtual FGameplayTag GetHandledCueTag() const override;
	virtual bool OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;
	virtual bool OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;

protected:
	/** The rope itself. Unset until an artist authors it — the hook is what lands here. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Cue")
	TSoftObjectPtr<UFXSystemAsset> Effect;

	/** The taut-line loop, started with the rope and stopped with it. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Cue")
	TSoftObjectPtr<USoundBase> Loop;
};

/**
 * BN23 — THE ANCHOR BITE at the far end: the hook striking geometry. Fires at the cue's
 * Location (the trace hit), never at the shooter, so it reads as a thing that happened
 * over there.
 */
UCLASS(Config = Game, meta = (DisplayName = "GC_BN_Grapple_Hit"))
class BREACHPOINTNEXT_API UBNGameplayCue_GrappleHit : public UBNGameplayCue_Base
{
	GENERATED_BODY()

public:
	virtual FGameplayTag GetHandledCueTag() const override;
	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;

protected:
	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Cue")
	TSoftObjectPtr<UFXSystemAsset> Effect;

	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Cue")
	TSoftObjectPtr<USoundBase> Sound;
};

UCLASS(Config = Game, meta = (DisplayName = "GC_BN_Character_Death"))
class BREACHPOINTNEXT_API UBNGameplayCue_Death : public UBNGameplayCue_Base
{
	GENERATED_BODY()

public:
	virtual FGameplayTag GetHandledCueTag() const override;
	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;

protected:

	/** Off = the corpse simply stops, which is what shipped before this. A switch because a ragdoll
	 *  is the kind of thing that looks wrong in ways only a playtest reveals. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Cue")
	bool bRagdoll = true;

	/** The mesh's profile while simulating. "Ragdoll" is the engine's stock physics-body profile;
	 *  a corpse must stop blocking the living, which is what it does. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Cue")
	FName RagdollCollisionProfile = TEXT("Ragdoll");

	/** Optional. FPSTemplate ships no death sound; left unset it costs nothing. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Cue")
	TSoftObjectPtr<USoundBase> Sound;
};

UCLASS(Config = Game, meta = (DisplayName = "GC_BN_Grenade_Explode"))
class BREACHPOINTNEXT_API UBNGameplayCue_Explosion : public UBNGameplayCue_Base
{
	GENERATED_BODY()

public:
	virtual FGameplayTag GetHandledCueTag() const override;
	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;

protected:

	/** The bang. Spawned at the blast's own location, NOT at the target — the projectile is gone by
	 *  the time this runs, so Parameters.Location is the only record of where it went off. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Cue")
	TSoftObjectPtr<UFXSystemAsset> Effect;

	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Cue")
	TSoftObjectPtr<USoundBase> Sound;
};

/**
 * Native cue handlers are NOT discovered on their own: UGameplayCueManager builds its runtime set
 * by scanning CONTENT paths for cue assets, and a C++ class lives in no content path. Without this
 * every cue above would be a class nothing ever calls. One pass at world begin-play registers them.
 */
UCLASS()
class BREACHPOINTNEXT_API UBNGameplayCueRegistrar : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
};
