// Breachpoint. THE GameplayCue handler library — the classes that make a cue tag play something.

#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "GameplayTagContainer.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/SoftObjectPtr.h"

#include "BRGameplayCues.generated.h"

class UFXSystemAsset;
class USoundBase;

/**
 * ===========================================================================================
 * THE GAMEPLAYCUE HANDLER LIBRARY (law 7 / R18: cue handlers are C++ classes, never `.uasset`)
 * ===========================================================================================
 *
 * ONE FILE FOR ALL OF THEM, and — as with `BRGameplayEffects.h` — that is a decision, but the
 * reason here is sharper than "the pattern stays visible". A native cue handler and the code
 * that BINDS it to its tag are two halves of ONE mechanism, and only the second half is load
 * bearing. Split them across folders and you get the exact failure this file exists to prevent:
 * a handler that compiles, looks armed, and is never called. They are read together or the
 * reader learns the wrong thing.
 *
 * -------------------------------------------------------------------------------------------
 * HOW A NATIVE CUE HANDLER BINDS TO ITS TAG — the part that is easy to get silently wrong.
 *
 * `UGameplayCueNotify_Static` carries a `GameplayCueTag` UPROPERTY, and every tutorial says
 * "set the tag on the class". **For a C++ class that property does nothing.** The binding is
 * built by `UGameplayCueManager::InitObjectLibraries`, which fills the runtime `UGameplayCueSet`
 * from `UObjectLibrary::LoadBlueprintAssetDataFromPaths(...)` and then reads the
 * `GameplayCueName` asset-registry tag off each result. Two consequences:
 *
 *   1. It scans BLUEPRINT ASSETS under `GameplayCueNotifyPaths` (default `/Game`). A native
 *      UClass is not an asset and is not in that scan. It is never found, at any verbosity.
 *   2. `GameplayCueName` (the searchable mirror) is only written by
 *      `UAbilitySystemGlobals::DeriveGameplayCueTagFromClass`, whose body is `#if WITH_EDITOR`
 *      **and** only fires on save/load of an asset.
 *
 * So a native handler with `GameplayCueTag` set in its constructor is a mechanism that looks
 * armed and is not — the failure mode this project keeps cataloguing. The tag property is
 * therefore left EMPTY here (see also: it holds ONE tag, and the fire handler answers to
 * THREE), and the authoritative declaration is the virtual `GetHandledCueTags()`. The binding
 * is made explicitly by `UBRGameplayCueRegistrar` below, via `UGameplayCueSet::AddCues` with a
 * `FSoftObjectPath` to the native class (`/Script/Breachpoint.BRGameplayCue_WeaponFire`), which
 * `HandleGameplayCueNotify_Internal` resolves with `ResolveObject()` — always in memory for a
 * native class, so it never takes the async-load path.
 *
 * WHY NOT BIND TO THE PARENT `GameplayCue.Weapon` and let the cue set's child-to-parent
 * fallback (`BuildAccelerationMap_Internal`) catch all three? Because that binding is
 * OVER-BROAD: it would also silently swallow `GameplayCue.Weapon.AR.Reload` and every other
 * weapon cue a later packet declares, and the day that happens nobody will look here. Three
 * exact tags, registered by name, fail loudly when one is missing.
 *
 * -------------------------------------------------------------------------------------------
 * WHAT A CUE HANDLER MAY CONTAIN: FX. Muzzle flash, tracer, report, and nothing else.
 *
 * It does not apply damage, does not read or write an attribute, does not touch a state tag,
 * does not branch on health, and does not decide anything. `ExecuteGameplayCue` reaches every
 * client that can see the shooter — including simulated proxies under Mixed replication, which
 * receive tags and cues and NOT the effects behind them — so a decision made here would be made
 * differently on every machine. If a handler wants gameplay data, that is a design error in the
 * caller, not a missing parameter here.
 *
 * PREDICTION IS NOT THIS FILE'S PROBLEM. A cue raised inside a predicted ability plays
 * immediately for the predictor and the ASC suppresses the replicated echo (`gas-purity` §6).
 * There is deliberately no "did I already play this" bookkeeping below — hand-rolling it is how
 * a shooter ends up with a muzzle flash that plays twice on the host and zero times on a client.
 *
 * NO HARD ASSET REFERENCES (law 3). No `ConstructorHelpers`, no raw `UParticleSystem*`
 * UPROPERTY. Every FX reference is a `TSoftObjectPtr`, and **none of them are populated today,
 * because no FX asset exists yet.** That is not silent: see `ReportSilentCue`.
 */

// ===========================================================================================
// The base — what every Breachpoint cue handler gets for free
// ===========================================================================================

/**
 * UBRGameplayCue_Base — the cue family's base, standing to cue handlers as `UBRGameplayAbility`
 * stands to abilities: it implements no cue, and it guarantees the things no subclass should
 * have to remember.
 *
 * STATIC, NOT AN ACTOR — the deliberate choice (`gas-purity` §6: "static cues for stateless
 * one-shots; instanced cue actors only when the effect holds state").
 * `UGameplayCueNotify_Static` is a NON-INSTANCED handler: the cue set calls
 * `GetDefaultObject()->HandleGameplayCue(...)` and the CDO does the work. Zero actors spawned,
 * zero replication, zero lifetime to manage, and a rate-of-fire cue that spawned an actor per
 * shot would put an AR at 600 RPM into the actor-spawn budget for a muzzle flash.
 *
 * The price of static is the rule that follows from it: **the CDO is shared and the handlers
 * are `const`.** There is no per-invocation instance, so a handler may hold no state whatsoever.
 * A cue that genuinely needs state — a looping shield-break crackle that must be stopped on
 * `Removed`, an attached beam that follows a socket — is exactly the case §6 reserves for
 * `AGameplayCueNotify_Actor`, and it belongs in a NEW class here, not in a bool on this one.
 *
 * DEDICATED-SERVER SUPPRESSION IS NOT RE-CHECKED HERE. `UGameplayCueManager::
 * ShouldSuppressGameplayCues` already drops cues on a dedicated server before routing. A second
 * check in the handler would be a second source of truth for the same rule, and the two would
 * eventually disagree about a listen-server host — which DOES play its own cues and must.
 */
UCLASS(Abstract, meta = (DisplayName = "BR Gameplay Cue"))
class BREACHPOINT_API UBRGameplayCue_Base : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
	UBRGameplayCue_Base(const FObjectInitializer& ObjectInitializer);

	/**
	 * THE BINDING DECLARATION — the one place a handler names the tags it answers to, and the
	 * only thing `UBRGameplayCueRegistrar` reads. Not `GameplayCueTag`: that property is inert
	 * for a native class (see the file header) and holds one tag where a handler may serve many.
	 *
	 * A subclass that returns nothing is registered against nothing and would never run; the
	 * registrar reports that as a warning rather than letting it pass as "no cues today".
	 */
	virtual void GetHandledCueTags(FGameplayTagContainer& OutTags) const {}

	/**
	 * Every soft FX path this handler may want to play, so the registrar can warm them once per
	 * world instead of the first shot of the match hitting a cold synchronous load.
	 *
	 * Soft refs are resolved with `Get()` (never `LoadSynchronous()`) at play time — a cue is on
	 * the render-critical path and a blocking load there is a hitch the player feels on the one
	 * frame they were shooting.
	 */
	virtual void GetFXAssetPaths(TArray<FSoftObjectPath>& OutPaths) const {}

protected:
	/**
	 * Where the FX goes. `FGameplayCueParameters::Location` when the raiser set one, otherwise
	 * the target actor's location.
	 *
	 * The fallback is not politeness: `ExecuteGameplayCue`'s minimal-replication path can drop a
	 * zero-vector Location, and FX pinned to world origin is a bug report that reads as "the
	 * gun fires at the sky".
	 */
	static FVector ResolveCueLocation(const AActor* Target, const FGameplayCueParameters& Parameters);

	/**
	 * Play a soft-referenced sound if it is resident. Returns false when the reference is unset
	 * (nothing authored yet) or not yet loaded — the caller reports, this does not.
	 */
	bool PlaySoundSoft(const UObject* WorldContext, const TSoftObjectPtr<USoundBase>& SoftSound, const FVector& Location) const;

	/**
	 * Spawn a soft-referenced FX system if it is resident. Same contract as PlaySoundSoft.
	 *
	 * ONLY CASCADE (`UParticleSystem`) CAN ACTUALLY BE SPAWNED FROM THIS MODULE TODAY. The
	 * reference type is `UFXSystemAsset` (the Engine-module base that both Cascade and Niagara
	 * derive from) so the DATA shape is already right, but spawning a `UNiagaraSystem` needs
	 * `UNiagaraFunctionLibrary`, which needs the `Niagara` module in `Breachpoint.Build.cs` —
	 * outside this packet's owner path. Recorded as a contract_gap in the report rather than
	 * worked around; a Niagara asset arriving here is reported by name, not dropped.
	 */
	bool SpawnFXSoft(const UObject* WorldContext, const TSoftObjectPtr<UFXSystemAsset>& SoftFX, const FVector& Location, const FRotator& Rotation) const;

	/**
	 * THE ANTI-SILENCE VALVE. A cue that fired correctly, routed correctly, and had nothing to
	 * play is indistinguishable — from the player's chair and from the log — from a cue that was
	 * never bound at all. Those two need very different fixes, so they must not look the same.
	 *
	 * Logs ONCE per (cue tag, slot) at Warning on `LogBRCombat`, and in a non-shipping build
	 * draws a placeholder marker so the cue is visibly firing. `BR.Cues.DrawPlaceholders 0`
	 * turns the draw off for a clean capture; the once-per-tag log is not suppressible.
	 */
	void ReportSilentCue(const UObject* WorldContext, const FGameplayCueParameters& Parameters, const FVector& Location, const TCHAR* Slot) const;
};

// ===========================================================================================
// The weapon-fire cue — one handler, three tags, one shot
// ===========================================================================================

/**
 * The FX slots for one weapon's fire cue. A struct rather than three parallel maps so that
 * "the AR's muzzle flash, tracer and report" is one row that cannot be filled in half.
 *
 * SOFT REFS ONLY (law 3). This struct is NOT a DataTable row and does not belong in
 * `BRDataRows.h` — but it is the shape a `DT_Weapons` row would carry, and that is where it
 * should eventually live: the weapon row already names the cue TAG (`FireCueTag`), so the FX it
 * plays belongs beside it in the same CSV. Filed in the report as the durable home; the map
 * below is the interim surface, and it is `Config` precisely so it stays diffable text
 * (`BREACHPOINT-AUTHORING-MATRIX.md` §1: prefer `Config/DefaultGame.ini` under
 * `[/Script/Breachpoint.<Class>]` over anything that needs the editor opened).
 */
USTRUCT()
struct FBRWeaponFireCueFX
{
	GENERATED_BODY()

	/** Burst at the muzzle. */
	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Cue")
	TSoftObjectPtr<UFXSystemAsset> MuzzleFlash;

	/** Beam from muzzle to impact. Needs an END point — see UBRGameplayCue_WeaponFire's note. */
	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Cue")
	TSoftObjectPtr<UFXSystemAsset> Tracer;

	/** The report. A MetaSound hooks here (`gas-purity` §6); cue params become audio params. */
	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Cue")
	TSoftObjectPtr<USoundBase> Report;
};

/**
 * UBRGameplayCue_WeaponFire — muzzle flash, tracer and report for every weapon that fires.
 *
 * ONE-SHOT, so `Executed` and nothing else (`gas-purity` §6's first distinction). `HandlesEvent`
 * says so explicitly rather than inheriting the base's "yes to everything": a fire cue that
 * answered `OnActive`/`WhileActive` would fire a second muzzle flash for any duration effect
 * that happened to carry the same tag, and a late-joining client would get a muzzle flash for a
 * shot taken before they connected.
 *
 * ONE CLASS, THREE TAGS. `GameplayCue.Weapon.AR.Fire`, `.Magnum.Fire` and `.Rocket.Fire` are the
 * three leaves that exist (`BRGameplayTags.h`), and they differ ONLY in which assets they play —
 * which is data, keyed by the matched tag. Three near-identical handler classes would be the cue
 * equivalent of the `GE_RocketCooldown` that `GE_Cooldown` exists to prevent.
 *
 * WHERE THE TAG COMES FROM AT PLAY TIME: `FGameplayCueParameters::MatchedTagName`, which
 * `UGameplayCueSet::HandleGameplayCueNotify_Internal` stamps with the registered tag before
 * calling us. Not `OriginalTag` — that is the tag as RAISED, and it differs from the matched one
 * the moment a cue translator or a parent fallback is involved.
 *
 * THE TRACER CANNOT BE DRAWN TODAY, and the reason is upstream. `BRGA_WeaponFire::
 * OnTargetDataReady` builds `FGameplayCueParameters` with `Location` (the trace START) and
 * `Instigator`, and nothing else — no `EffectContext`, so no hit result, so no impact point. A
 * beam needs both ends. This handler reads the end point from
 * `Parameters.EffectContext.GetHitResult()` and, finding no context, reports the tracer slot as
 * silent rather than inventing an end point from the shooter's current aim: that would diverge
 * from the shot actually taken, which is worse than no tracer. Fixing it is a one-line change in
 * the ability (attach the hit result to a context on the cue params) and needs NO change here —
 * it is filed as a cross-lane finding, not patched around.
 */
UCLASS(Config = Game, meta = (DisplayName = "GC_Weapon_Fire"))
class BREACHPOINT_API UBRGameplayCue_WeaponFire : public UBRGameplayCue_Base
{
	GENERATED_BODY()

public:
	UBRGameplayCue_WeaponFire(const FObjectInitializer& ObjectInitializer);

	// -- UBRGameplayCue_Base --------------------------------------------------------------------

	virtual void GetHandledCueTags(FGameplayTagContainer& OutTags) const override;
	virtual void GetFXAssetPaths(TArray<FSoftObjectPath>& OutPaths) const override;

	// -- UGameplayCueNotify_Static --------------------------------------------------------------

	/** `Executed` only. See the class comment — this is the one-shot half of §6's distinction. */
	virtual bool HandlesEvent(EGameplayCueEvent::Type EventType) const override;

	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;

protected:
	/**
	 * Per-cue-tag FX. EMPTY BY DEFAULT and empty today, because no FX asset exists in the project
	 * yet — every entry would be a hard-coded path to a file that is not there.
	 *
	 * `Config` (not `EditDefaultsOnly` alone) is the authoring surface: R18 forbids a Blueprint
	 * child of a cue class outright, and here it would be actively harmful — a BP child DOES get
	 * picked up by the cue manager's asset scan, so it would bind ITSELF to a tag derived from
	 * its asset name and quietly become a second, competing handler.
	 */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Breachpoint|Cue")
	TMap<FGameplayTag, FBRWeaponFireCueFX> FXByCueTag;
};

// ===========================================================================================
// The registrar — the half that actually arms everything above
// ===========================================================================================

/**
 * UBRGameplayCueRegistrar — binds every native `UBRGameplayCue_Base` subclass to the tags it
 * declares, in the GameplayCueManager's runtime cue set.
 *
 * WHY THIS EXISTS AT ALL: see the file header. The engine's own discovery path scans Blueprint
 * assets and cannot see a native class, so without this subsystem every handler above is dead
 * code that compiles.
 *
 * WHY A `UWorldSubsystem`, AND WHY `OnWorldBeginPlay`:
 *
 *  - `UGameplayCueManager::InitializeRuntimeObjectLibrary()` calls `CueSet->Empty()` before it
 *    repopulates. Anything registered BEFORE that call is erased. The manager is created lazily
 *    on the first `GetGameplayCueManager()` and initialises synchronously
 *    (`ShouldDeferScanningRuntimeLibraries()` is false by default), so registering at
 *    `OnWorldBeginPlay` is safely after it — while a `UGameInstanceSubsystem::Initialize` would
 *    race it, and would additionally FORCE the manager into existence earlier than the engine
 *    intended just by asking for it.
 *  - Re-asserting once per world also survives PIE restarts, seamless travel, and a Live Coding
 *    reload — and it is free, because `UGameplayCueSet::AddCues` skips a tag it already holds.
 *
 * IT REGISTERS BY DISCOVERY, NOT BY LIST: `GetDerivedClasses(UBRGameplayCue_Base)` finds every
 * concrete handler and asks it for its own tags. A new cue class is therefore one file edit, not
 * two — the alternative is a hand-maintained list in this file whose first omission produces a
 * handler that silently never runs, which is the exact bug class this whole file is about.
 */
UCLASS()
class BREACHPOINT_API UBRGameplayCueRegistrar : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Game and PIE worlds only. Editor preview/inactive worlds route no cues and need no set. */
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/**
	 * Register every native handler with the runtime cue set. Idempotent — safe to call again.
	 *
	 * @return the number of (tag -> handler class) bindings VERIFIED present in the cue set
	 *         afterwards, which is not the number attempted: a tag that failed to land is the
	 *         difference, and it is logged. Public and static so an automation spec can assert
	 *         `RegisterNativeCueHandlers() == 3` without a world.
	 */
	static int32 RegisterNativeCueHandlers();

	/** Kick an async load of every soft FX path the handlers declare. No-op while none exist. */
	static void WarmCueFXAssets();
};
