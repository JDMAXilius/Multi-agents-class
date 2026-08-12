#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"

#include "Animations/BRAimAndLeanTypes.h"
#include "Animations/BRPoseOffsetTypes.h"
#include "Animations/BRRecoilTypes.h"
#include "Animations/BRSwayAndLagTypes.h"

#include "BRProceduralAnimComponent.generated.h"

class UBRAnimInstance;
class USkeletalMeshComponent;

/**
 * ONE component where the template had six, and **it does not tick**.
 *
 * WHAT THE TEMPLATE HAD, and what is actually wrong with it. `BP_FPST_Character` carried
 * `BPC_FPST_Procedural_{Manager, Recoil, SwayAndLag, AimAndLean, PoseOffsets, Base}` — six
 * `UActorComponent`s, driven by `Tick`, with `bAllowTickOnDedicatedServer: true`. Six tick
 * registrations, six replication surfaces, and every one of them doing animation maths on the
 * **game thread**, which `animation.md` law 1 forbids outright. The `Manager`'s entire content is
 * `proceduralCompAry` — a fan-out that exists only because the work was split across five objects
 * in the first place.
 *
 * WHAT THIS IS INSTEAD. The split was never between *recoil* and *sway*; those are two functions
 * over the same data. The real split is **data and seam** (which needs an object, a replication
 * story and an owner) against **maths** (which needs neither, and must not run here at all).
 *
 *   · the maths lives in `BRProceduralSolver` — free functions, worker thread, no UObject;
 *   · the per-weapon DATA and the seam to the spines live here, in one component, on the game
 *     thread where equipment can talk to it.
 *
 * So there is no tick to register. This component is asked questions and told things; it never
 * wakes up on its own. That is the difference between six ticking components and none.
 *
 * IT OWNS THE WHOLE SEAM, not half of it. Layer linking used to live on `ABRFPSCharacter` while
 * the profiles lived here, which split one concern -- "what weapon is up, and how does it move?"
 * -- across two objects that had to agree. Two owners of one fact is the defect this packet has
 * now hit three times (tag bools, linked-layer state, sway profile). Equipment talks to exactly
 * one object; the character forwards and holds nothing.
 *
 * WHY A COMPONENT AND NOT MORE CHARACTER CODE. Composition survives the pawn changing shape. A
 * bot, a spectator-possessed dummy, or a mounted-turret pawn each need weapon presentation without
 * inheriting a first-person character; and `Character/` is BP96's folder, so every line that lands
 * here instead is a line this packet does not have to negotiate for.
 */
UCLASS(ClassGroup = (Breachpoint), meta = (BlueprintSpawnableComponent), Config = Game)
class BREACHPOINT_API UBRProceduralAnimComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UBRProceduralAnimComponent();

	/**
	 * Adopt a weapon's procedural profile. **Called by equipment (BP97) on equip.**
	 *
	 * Takes the profile by value rather than a row name because this component must not know what
	 * a `DataTable` is: `BRGameData` resolves the row, equipment hands over the result, and no
	 * animation code acquires a data dependency it would then have to load, cache and invalidate.
	 */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Procedural")
	void SetWeaponProfile(const FBRSwayAndLagInfo& InSwayAndLag, const FBRRecoilInfo& InRecoil,
		const FBRAimAndLeanInfo& InAimAndLean);

	/** Drop back to the unarmed profile. Equip of `None` and death both land here. */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Procedural")
	void ClearWeaponProfile();

	/**
	 * Link a weapon's anim layer on every mesh that has one of our spines.
	 *
	 * **Verified per mesh, never recorded optimistically.** `LinkAnimClassLayers` returns `void`
	 * and silently no-ops when a mesh has no anim instance yet -- routine on a join-in-progress
	 * client where the third-person mesh initialises after the first. Recording success for a link
	 * that did not happen, then guarding the retry on that record, leaves the arms posing a rifle
	 * and the body posing the base layer permanently. So the mesh is asked, not trusted.
	 *
	 * Idempotent per mesh, because relinking an already-linked class tears the layer instance down
	 * and rebuilds it -- discarding every blend in flight at the moment a player swaps under fire.
	 */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Procedural")
	void LinkWeaponAnimLayer(TSubclassOf<UAnimInstance> LayerClass);

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Procedural")
	void UnlinkWeaponAnimLayer();

	/**
	 * Fire one recoil kick into both spines.
	 *
	 * `Alpha` is the FIRE ABILITY's roll and is passed through untouched — see `animation.md` A.6.
	 * A component that rolled its own would put the client's crosshair and the server's cone on
	 * different numbers for the same shot, which is arithmetic rather than a bug. And this is
	 * **weapon-transform recoil only**: camera recoil is BP98's by that same ruling.
	 *
	 * Picks the hip or ADS envelope from the profile, because those are not a multiplier apart —
	 * recoil is one of the few things a player judges a weapon by, and both envelopes are authored.
	 */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Procedural")
	void FireRecoil(float Alpha, bool bIsADS);

	const FBRSwayAndLagInfo& GetSwayAndLag() const { return SwayAndLag; }
	const FBRRecoilInfo& GetRecoil() const { return Recoil; }
	const FBRAimAndLeanInfo& GetAimAndLean() const { return AimAndLean; }

protected:

	virtual void BeginPlay() override;

	/**
	 * The unarmed profile, and the reason it is `Config` rather than a row.
	 *
	 * Unarmed is a real state with real feel, not the absence of a weapon — a player running with
	 * empty hands still has arms that sway. Giving it a table row would mean equipment had to
	 * publish a row for "nothing equipped", which is a row that exists to describe its own absence.
	 */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Breachpoint|Procedural")
	FBRSwayAndLagInfo DefaultSwayAndLag;

	UPROPERTY(EditDefaultsOnly, Config, Category = "Breachpoint|Procedural")
	FBRAimAndLeanInfo DefaultAimAndLean;

	UPROPERTY(Transient)
	FBRSwayAndLagInfo SwayAndLag;

	UPROPERTY(Transient)
	FBRRecoilInfo Recoil;

	UPROPERTY(Transient)
	FBRAimAndLeanInfo AimAndLean;

	/**
	 * Both spines, resolved on demand rather than cached.
	 *
	 * Caching them would mean invalidating on every mesh re-init, anim class change and respawn —
	 * three events that already caused a stale-state defect in the layer-linking path earlier in
	 * this packet. Resolving costs two `Cast`s on a shot, which is not a budget anyone is short of.
	 */
	void ForEachSpine(TFunctionRef<void(UBRAnimInstance&)> Fn) const;

	/**
	 * What each mesh actually has linked, keyed by the mesh itself.
	 *
	 * Per mesh rather than per character, and weak-keyed so a destroyed mesh cannot hold an entry
	 * alive. A single character-wide record was the M18 defect: it could not represent "1P linked,
	 * 3P not yet", which is the exact state a join-in-progress client passes through.
	 */
	UPROPERTY(Transient)
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, TSubclassOf<UAnimInstance>> LinkedLayers;

	/** The layer equipment last asked for, retried per mesh until each actually holds it. */
	UPROPERTY(Transient)
	TSubclassOf<UAnimInstance> DesiredLayerClass;
};
