// BREACHPOINT — BP05 step 1. The grenade: cook, throw, and OUR radial damage.

#include "AbilitySystem/Abilities/BRGA_Grenade.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "AbilitySystem/BRAbilitySystemComponent.h"
#include "AbilitySystem/BRCombatCurves.h"
#include "AbilitySystem/Effects/BRGameplayEffects.h"
#include "Core/BRCore.h"
#include "Core/BRGameplayTags.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace
{
	/** Metres -> Unreal units. Structural; nobody balances this number. */
	constexpr float MetresToUU = 100.f;

	/**
	 * ============================================================================================
	 * WHERE THE GRENADE'S NUMBERS LIVE — a decision, taken here and filed, with its cost stated
	 * ============================================================================================
	 *
	 * **NONE OF THESE SIX ROWS EXISTS TODAY.** `CT_Combat.csv` has 11 rows and not one of them is a
	 * grenade row (verified 1 Aug 2026). Every caller below refuses loudly when its row is missing,
	 * exactly as `UBRGA_Grapple` does for `Grapple.RangeMetres` — a grenade that silently used a
	 * hardcoded 3 s fuse and a 5 m radius would be a law-3 violation wearing a working feature's
	 * costume, and it would be a BALANCE change that no CSV diff could show.
	 *
	 * WHY CURVES AND NOT A `DT_Weapons` ROW, since three of these six have obvious columns
	 * (`ProjectileSpeed`, `SplashRadius_m`, `SplashDamage` — the Rocket populates all three):
	 *
	 *   1. `DT_Weapons.csv` has no `Grenade` row. Three ship: AR, Magnum, Rocket.
	 *   2. **And the bigger one: there is no reachable path from an ability to an arbitrary
	 *      `DT_Weapons` row.** Every row read in this project goes through an `FDataTableRowHandle`
	 *      held by an `ABRWeaponPickup` or a `UBRWeaponInstance` — `BRGA_WeaponFire` reads its row
	 *      off the EQUIPPED weapon. A grenade is neither equipped nor picked up, so there is no
	 *      handle to follow. `BRCombatCurves` is the one data surface an ability can read with
	 *      nothing in its hands, and law 3 does not permit a hard `ConstructorHelpers` reference to
	 *      the table (the pre-tool hook blocks it outright) nor an `EditDefaultsOnly` row handle on
	 *      an ability that R18 says has no editor surface anyway.
	 *
	 * SO THIS IS A NAMED SECOND-BEST, NOT A PREFERENCE. Radius, centre damage and throw speed are
	 * per-weapon numbers and their long-term home is a `Grenade` row of `DT_Weapons`. Putting them
	 * there needs BOTH the row AND a table accessor an ability can reach — a data + schema packet,
	 * and neither `Content/Data/DT_Weapons.csv` nor `Source/Breachpoint/Data/BRDataRows.h` is this
	 * packet's to write. Cook and fuse have no column at all today and would need two new ones.
	 * Filed as a gap; when the accessor exists, `ResolveTuning` is the one function that changes.
	 *
	 * A NAME IS NOT A NUMBER. Nothing in this file decides what a grenade's fuse or radius IS; it
	 * decides where to read them from.
	 */
	const FName GrenadeCookSecondsCurve(TEXT("Grenade.CookSeconds"));
	const FName GrenadeFuseSecondsCurve(TEXT("Grenade.FuseSeconds"));
	const FName GrenadeThrowSpeedCurve(TEXT("Grenade.ThrowSpeedMetresPerSecond"));
	const FName GrenadeBlastRadiusCurve(TEXT("Grenade.BlastRadiusMetres"));
	const FName GrenadeBlastCentreDamageCurve(TEXT("Grenade.BlastCentreDamage"));

	/**
	 * The falloff, and the ONE curve here that is a curve rather than a constant.
	 *
	 * Evaluated at the NORMALISED distance (0 at the epicentre, 1 at the blast edge) and returning
	 * a damage SCALE. `BRCombatCurves::Evaluate` takes an input value precisely so a shape like
	 * this can live in data — which means the falloff's shape (linear, quadratic, a plateau then a
	 * cliff) is a CSV edit and zero lines of code. That is the property law 3 buys.
	 *
	 * Normalising by the radius rather than passing metres is deliberate: one authored shape then
	 * serves the grenade and BP09's rocket at different radii, which is the "grenade and rocket
	 * share it" the contract's law 3 asks for.
	 */
	const FName GrenadeBlastFalloffCurve(TEXT("Grenade.BlastFalloff"));

	/**
	 * Tag strings this packet needs and may not declare. See `UBRGA_Grenade::RequestOwedTag`.
	 * Constants rather than literals at three call sites: a misspelling then fails once, visibly.
	 */
	const TCHAR* AbilityGrenadeTagString = TEXT("Ability.Grenade");
	const TCHAR* GrenadeThrowCueTagString = TEXT("GameplayCue.Grenade.Throw");
	const TCHAR* GrenadeExplodeCueTagString = TEXT("GameplayCue.Grenade.Explode");
}

FGameplayTag UBRGA_Grenade::RequestOwedTag(const TCHAR* TagString)
{
	// ErrorIfNotFound=false: an undeclared leaf returns an INVALID tag rather than raising an
	// ensure at module load. Every caller checks validity and refuses out loud; nothing here
	// substitutes a fallback tag, because a cue that plays under the wrong tag is worse than a cue
	// that visibly does not play.
	return FGameplayTag::RequestGameplayTag(FName(TagString), /*ErrorIfNotFound=*/false);
}

UBRGA_Grenade::UBRGA_Grenade(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// LocalPredicted because the player FEELS the cook. The moment between the press and the throw
	// is the entire mechanic; a server round trip in front of it makes every grenade feel late,
	// and unlike the shot there is nothing to validate afterwards — see the header's note on why
	// there is no ValidateClaim in this file.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// ==========================================================================================
	// OnInputPressed, NOT WhileInputHeld — and the base class names this exact case.
	// ==========================================================================================
	//
	// `EBRAbilityActivationPolicy::WhileInputHeld` ENDS the ability on release. The grenade must
	// THROW on release, which is a different event that happens to share an edge. The base's own
	// enum comment on OnInputPressed says it in one line: *"an ability that wants the release must
	// ask for it (WaitInputRelease) because it means something specific to it (a cooked grenade)."*
	// So this ability owns its release watcher rather than inheriting one, and the base's watcher
	// never runs.
	ActivationPolicy = EBRAbilityActivationPolicy::OnInputPressed;

	// The ability's ASSET tag. `SetAssetTags` is the 5.5+ spelling; the `AbilityTags` property it
	// replaces is deprecated and documented as becoming private (established by BRGA_Sprint).
	//
	// *** TODO(BP05): `Ability.Grenade` is OWED. *** It is not declared in `Core/BRGameplayTags.h`
	// — `Ability.*` is an OPEN family (R23) so this is a one-line declaration and NOT a
	// contract_gap, but the exact-file grant on that header belongs to another owner this session,
	// so this packet requests the tag by name instead of declaring it. AddTag ignores an invalid
	// tag, so today this ability ships with NO asset tag, and the consequence is precise and worth
	// stating: nothing can cancel the grenade with `CancelAbilitiesWithTag`, and nothing can give
	// it a cooldown, because the generic-cooldown pattern grants the ability's own tag.
	//
	// ONE MORE REASON THE STRING FORM IS TEMPORARY, and it is not a style objection: this runs in a
	// CDO constructor at module load, which RACES the native tag registry. `BRGameplayTags::*` is a
	// handle that resolves after registration and is safe here; a name lookup may run before the
	// table is populated and come back invalid even once the leaf IS declared. When the declaration
	// lands, this MUST become `BRGameplayTags::Ability_Grenade` — a string that starts working
	// intermittently is worse than one that never works.
	// RESOLVED 1 Aug 2026: `Ability.Grenade` is now declared natively, so this uses the handle
	// rather than the string lookup. The packet was right to refuse the string here — a
	// constructor-time RequestGameplayTag races the registry at module load and would give a
	// grenade whose asset tag exists on some instances and not others.
	const FGameplayTag GrenadeTag = BRGameplayTags::Ability_Grenade;
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(GrenadeTag);
	SetAssetTags(AssetTags);

	// Throwing a grenade ends a sprint, the same way firing does, with zero code in either ability.
	// Note this lists the ABILITY's asset tag and not `State.Movement.Sprinting`, which is a tag on
	// the ACTOR — the correction BRGameplayTags.h records against §3.3.
	CancelAbilitiesWithTag.AddTag(BRGameplayTags::Ability_Sprint);

	// ==========================================================================================
	// NO COOLDOWN TAG, NO COST GE — and this is a gap, not a design position.
	// ==========================================================================================
	//
	// BP05 step 1 says the cost is a CARRIED COUNT ("2 carried, reset on respawn via the
	// `GE_InitStats` path"), not a cooldown. A count that GAS can predict and roll back is an
	// attribute plus a cost GE, and both live in `AbilitySystem/Effects/` — which this packet may
	// NOT write. BP05's own pre-filed contract_gap says so in as many words, and warns that owning
	// `Abilities/` does not grant the sibling `Effects/`.
	//
	// So **the grenade is free today.** Stated in a comment because "no cost" and "someone forgot
	// the cost" look identical in code — the same reason BRGA_Sprint spells out that sprint is free
	// by design. This one is NOT by design, and the difference is the whole point of writing it
	// down. Consequence for BP05's Done-when box 3 ("cook-cancel and whiff rollback leave zero
	// state (count, cooldown, cues)"): the COUNT leg cannot be proven until the cost exists, and no
	// amount of care in this file substitutes for it.
	bCommitOnActivate = true;
}

// ================================================================================================
// Data
// ================================================================================================

bool UBRGA_Grenade::ResolveTuning(FBRGrenadeTuning& OutTuning, FString& OutReason) const
{
	// One local helper, so six reads cannot drift into six slightly different failure behaviours.
	// `Evaluate` returns false when the table or the curve is missing and writes NOTHING — there is
	// deliberately no EvaluateOrDefault in BRCombatCurves, and this is why.
	auto ReadPositive = [&OutReason](FName CurveName, float& OutValue) -> bool
	{
		float Value = 0.f;
		if (!BRCombatCurves::Evaluate(CurveName, Value))
		{
			OutReason = FString::Printf(TEXT("CT_Combat has no '%s' row"), *CurveName.ToString());
			return false;
		}
		if (Value <= 0.f)
		{
			// Zero is not "unset" here, it is "authored absurd": a zero fuse detonates in the hand
			// on the frame of the press, a zero radius damages nobody, a zero throw speed drops the
			// grenade on the thrower's boots. Refusing beats shipping any of the three.
			OutReason = FString::Printf(TEXT("CT_Combat row '%s' is %.3f; every grenade number must be positive"), *CurveName.ToString(), Value);
			return false;
		}
		OutValue = Value;
		return true;
	};

	FBRGrenadeTuning Resolved;
	if (!ReadPositive(GrenadeCookSecondsCurve, Resolved.CookSeconds)
		|| !ReadPositive(GrenadeFuseSecondsCurve, Resolved.FuseSeconds)
		|| !ReadPositive(GrenadeThrowSpeedCurve, Resolved.ThrowSpeedMetresPerSecond)
		|| !ReadPositive(GrenadeBlastRadiusCurve, Resolved.BlastRadiusMetres)
		|| !ReadPositive(GrenadeBlastCentreDamageCurve, Resolved.BlastCentreDamage))
	{
		return false;
	}

	// The falloff is checked HERE, at activation, even though it is not consumed until detonation
	// seconds later and possibly by a different object. A grenade that flies beautifully and then
	// refuses to explode is a bug report nobody can reproduce; refusing at the press is loud, cheap
	// and points at the missing CSV row.
	float FalloffProbe = 0.f;
	if (!BRCombatCurves::Evaluate(GrenadeBlastFalloffCurve, 0.f, FalloffProbe))
	{
		OutReason = FString::Printf(TEXT("CT_Combat has no '%s' curve, so the blast has no falloff shape"), *GrenadeBlastFalloffCurve.ToString());
		return false;
	}

	// NOTE, deliberately NOT clamped: nothing here asserts CookSeconds < FuseSeconds. A data set
	// where the cook ceiling meets or exceeds the fuse is a grenade that goes off in your hand at
	// the end of a long hold — which is a DESIGN POSITION the curator may want to author, not a
	// defect for this file to silently correct. `ThrowGrenade` computes max(0, Fuse - Cooked) and
	// hands the result on; the arithmetic states the rule and the CSV states the intent.
	OutTuning = Resolved;
	return true;
}

// ================================================================================================
// Activation and the cook
// ================================================================================================

void UBRGA_Grenade::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// The base commits (free today — see the constructor's cost note) and will have ended us if the
	// commit failed. Skipping it is the subclass-contract violation BRGameplayAbility.h warns about.
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive())
	{
		return;
	}

	bThrowResolved = false;
	Tuning = FBRGrenadeTuning();

	FString Reason;
	if (!ResolveTuning(Tuning, Reason))
	{
		// REFUSED BEFORE THE COOK OPENS. Opening a cook window that can never produce a throw is
		// worse than refusing: the player holds a key, nothing happens, and the failure surfaces as
		// "grenades don't work" instead of as a named missing row.
		UE_LOG(LogBRCombat, Warning,
			TEXT("UBRGA_Grenade refused: %s. The grenade's numbers are owed by BP13/curator; a "
				 "literal fuse or radius here would be a law-3 violation."),
			*Reason);
		// Cancelled, not completed — bWasCancelled=true is what rolls back anything the commit
		// applied on the predicting client.
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		return;
	}

	// ==========================================================================================
	// THE COOK: a release watcher and a timer, racing. Whichever fires first throws.
	// ==========================================================================================
	//
	// **DEPENDENCY, stated because it fails silently.** `gas-purity` §4: *"Press/release must reach
	// the ASC (`AbilityInputTagPressed/Released`) or `WaitInputRelease` (sprint end, cook throw)
	// silently never fires."* It names the cook throw specifically. If the input layer relays only
	// the press edge, the release half of this cook is dead and every grenade is thrown by the
	// timer at maximum cook — which looks like a tuning problem, not a wiring problem, and would be
	// chased in the CSV for a day.
	//
	// The timer is therefore not merely a cook ceiling: it is also the backstop that keeps a
	// grenade from being held forever if the release never arrives. Both halves are needed and
	// neither is redundant.
	CookReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, /*bTestAlreadyReleased=*/true);
	if (!CookReleaseTask)
	{
		UE_LOG(LogBRCombat, Error,
			TEXT("UBRGA_Grenade: could not create its WaitInputRelease task; refusing rather than "
				 "cooking a grenade that only a timer can throw."));
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		return;
	}
	// bTestAlreadyReleased=true closes the tap case: at 60 Hz a quick tap CAN release before a
	// LocalPredicted activation finishes, and a tapped grenade must still be thrown immediately
	// rather than cooked to the ceiling.
	CookReleaseTask->OnRelease.AddDynamic(this, &UBRGA_Grenade::HandleCookReleased);
	CookReleaseTask->ReadyForActivation();

	if (UWorld* World = GetWorld())
	{
		// A TIMER, never a Tick (law 4). It runs on the predicting client AND on the authority,
		// unbranched — both are running the same ability with the same number out of the same CSV,
		// which is what makes the predicted throw and the authoritative throw the same throw.
		World->GetTimerManager().SetTimer(CookTimerHandle, this, &UBRGA_Grenade::HandleCookExpired, Tuning.CookSeconds, /*bLoop=*/false);
	}
	else
	{
		UE_LOG(LogBRCombat, Error, TEXT("UBRGA_Grenade: no world, so the cook has no ceiling; refusing."));
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
	}
}

void UBRGA_Grenade::HandleCookReleased(float TimeHeld)
{
	// The normal path: the player let go. TimeHeld comes from the task, so client and server each
	// measure their own hold — see the header on why a divergence here is cosmetic and a
	// client-supplied hold time would not be.
	ThrowGrenade(TimeHeld);
}

void UBRGA_Grenade::HandleCookExpired()
{
	// The ceiling. The hand lets go by itself, and the cook is by definition exactly CookSeconds.
	ThrowGrenade(Tuning.CookSeconds);
}

// ================================================================================================
// The throw
// ================================================================================================

void UBRGA_Grenade::ThrowGrenade(float SecondsCooked)
{
	if (bThrowResolved)
	{
		// Release and timeout CAN land on the same frame — a player who holds to exactly the
		// ceiling produces both. One press throws one grenade.
		return;
	}
	bThrowResolved = true;

	// Tear the cook down BEFORE anything else can re-enter: the losing half of the race must not
	// fire into an ability that has already thrown.
	ClearCook();

	UBRAbilitySystemComponent* ASC = GetBRAbilitySystemComponent();
	FVector ViewLocation, ViewDirection;
	if (!ASC || !GetViewPoint(ViewLocation, ViewDirection))
	{
		UE_LOG(LogBRCombat, Warning, TEXT("UBRGA_Grenade: no ASC or no view point at the throw; nothing left the hand."));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		return;
	}

	// COOKING IS THIS SUBTRACTION. Time spent holding is time the fuse already burned.
	const float RemainingFuseSeconds = FMath::Max(Tuning.FuseSeconds - SecondsCooked, 0.f);

	// Straight down the view ray, at the authored speed. There is no throw ARC number here on
	// purpose: an arc is gravity acting on a projectile over time, and gravity, drag and bounce
	// belong to the projectile. This ability supplies a direction and a speed, which are the two
	// things only it knows.
	const FTransform ReleaseTransform(ViewDirection.Rotation(), ViewLocation);
	const FVector LaunchVelocity = ViewDirection * (Tuning.ThrowSpeedMetresPerSecond * MetresToUU);

	// ==========================================================================================
	// THE GHOST — a CUE, never a client-spawned actor.
	// ==========================================================================================
	//
	// §3.3 asks for a "client ghost for feel". It is a GameplayCue for two reasons that point the
	// same way: law 6 puts all FX in cues, and a cue raised inside the prediction window is REMOVED
	// by GAS on rollback. A client-spawned ghost ACTOR would have no rollback path at all — it is
	// the third leg of "rejection leaves zero state" that BRGA_Grapple names for its rope.
	//
	// *** CUE TAG OWED (BP05): `GameplayCue.Grenade.Throw` is not declared. *** `GameplayCue.*` is
	// an OPEN family (R23) so the tag itself is a one-line declaration — but the cue HANDLER is a
	// C++ class (law 7, R18) that does not exist either, so declaring the tag alone would produce a
	// cue that silently never plays. Filed as ONE unit of work rather than half-done, the same call
	// BRGA_Grapple made about `GameplayCue.Grapple.Rope`.
	const FGameplayTag ThrowCueTag = RequestOwedTag(GrenadeThrowCueTagString);
	if (ThrowCueTag.IsValid())
	{
		FGameplayCueParameters CueParams;
		CueParams.Location = ViewLocation;
		CueParams.Normal = ViewDirection;
		CueParams.Instigator = GetAvatarActorFromActorInfo();
		ASC->ExecuteGameplayCue(ThrowCueTag, CueParams);
	}
	else
	{
		UE_LOG(LogBRCombat, Warning,
			TEXT("UBRGA_Grenade: '%s' is not declared and has no C++ handler, so the client ghost "
				 "does not exist. The throw is invisible until the tag and its cue class land "
				 "together."),
			GrenadeThrowCueTagString);
	}

	// ==========================================================================================
	// THE SPAWN. Authority only, and it is `gas-purity` §4 that says so, not taste.
	// ==========================================================================================
	if (HasAuthority(&CurrentActivationInfo))
	{
		RequestProjectileSpawn(ReleaseTransform, LaunchVelocity, RemainingFuseSeconds);
	}

	// The ability ends at the throw, not at the detonation. The fuse belongs to the projectile: an
	// ability instance that stayed alive for it would hold blast state across a death and respawn,
	// which BRGameplayAbility.h forbids in as many words ("anything an ability remembers across
	// lives is a bug waiting for a respawn"). This is a COMPLETION, so bWasCancelled=false.
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
}

void UBRGA_Grenade::RequestProjectileSpawn(const FTransform& ReleaseTransform, const FVector& LaunchVelocity, float RemainingFuseSeconds) const
{
	// ==========================================================================================
	// *** THE SEAM. THIS IS THE BLOCKER, AND IT IS THE WHOLE OF IT. ***
	// ==========================================================================================
	//
	// There is no projectile class, and ARCHITECTURE §3 has no home for one:
	// `docs/DECISIONS-OWED.md` **D6** (RULING, unanswered) — §3.5 `Weapons/` enumerates three units
	// and none is a projectile; no other §3 folder claims one. It blocks BP05 step 1 and all of
	// BP09, and BP05's Log states why it cannot be routed around at claim time: *"you cannot grant
	// a path to a file the architecture never named."*
	//
	// This function is deliberately the ONLY place that would change. When D6 lands, its body
	// becomes a `SpawnActorDeferred` + `FinishSpawning` and nothing else in this file moves.
	//
	// WHAT THE PROJECTILE OWES — written out so the ruling arrives at a specification rather than
	// at a blank page. Each line is a requirement this packet already knows and would otherwise
	// lose:
	//
	//   AUTHORITY     Spawned on the server only, from here. `bReplicates = true`, replicated
	//                 movement. Clients never spawn it; they saw the cue ghost.
	//   ATTRIBUTION   `Instigator` = the thrower's pawn, `Owner` = the thrower. Kill credit,
	//                 `Event.Kill`, the killfeed and the effect context all read from these; a
	//                 projectile with no instigator produces a kill with no killer.
	//   BANDWIDTH     Low NetUpdateFrequency and NETWORK DORMANCY once it comes to rest
	//                 (`netcode.md`, named by BP05's ticket). A grenade at rest that keeps
	//                 replicating is 4v4 x N grenades of nothing.
	//   COLLISION     `BRCollision::Projectile` (ECC_GameTraceChannel1, ini Name "Projectile").
	//                 `BRCore.h`'s alias table already names grenades as its user — the channel
	//                 exists and is waiting for the actor.
	//   PHYSICS       Bounce. Restitution/friction are TUNING and belong in data, not in the
	//                 projectile's constructor — the same law that keeps them out of this file.
	//   FUSE          `RemainingFuseSeconds`, as a TIMER (law 4). It is already net-corrected by
	//                 construction: the server's copy carries the server's own cook time.
	//   DETONATION    On the authority, at rest or at fuse-out, call
	//                 `UBRGA_Grenade::ApplyExplosionDamage(ThrowerASC, Thrower, GetActorLocation(),
	//                 RadiusMetres, CentreDamage)` — the radial rule below, which is written,
	//                 static, and does not need an ability instance precisely so this call works.
	//
	// The two data numbers the projectile needs at detonation (radius, centre damage) are read by
	// this ability at activation and would travel with the spawn. They are NOT re-read at
	// detonation from a different place — one grenade, one set of numbers, resolved once.
	UE_LOG(LogBRCombat, Error,
		TEXT("UBRGA_Grenade: BLOCKED at the projectile spawn. Throw was authorised on the authority "
			 "(release at %s, launch %.0f uu/s, %.2f s of fuse left) but NO PROJECTILE CLASS EXISTS "
			 "— ARCHITECTURE §3 allocates no home for one (DECISIONS-OWED D6, unanswered; blocks "
			 "BP05 step 1 and all of BP09). Nothing was spawned and no actor was invented."),
		*ReleaseTransform.GetLocation().ToCompactString(), LaunchVelocity.Size(), RemainingFuseSeconds);
}

// ================================================================================================
// OUR radial damage — the one thing here that is finished
// ================================================================================================

int32 UBRGA_Grenade::ApplyExplosionDamage(UAbilitySystemComponent* InstigatorASC, AActor* InstigatorActor, const FVector& Epicentre, float BlastRadiusMetres, float BlastCentreDamage)
{
	if (!InstigatorASC || !InstigatorActor)
	{
		// A projectile that outlives its thrower (disconnect, respawn, travel) lands here. Refusing
		// is right: damage with no source cannot be attributed, and an unattributed kill is a
		// scoring bug that surfaces three systems away.
		UE_LOG(LogBRCombat, Warning, TEXT("UBRGA_Grenade::ApplyExplosionDamage refused: the blast has no instigator ASC or actor."));
		return 0;
	}

	UWorld* World = InstigatorActor->GetWorld();
	if (!World)
	{
		return 0;
	}

	if (InstigatorASC->GetOwnerRole() != ROLE_Authority)
	{
		// SERVER ONLY BY CONTRACT (`BRGameplayEffects.h`: the exec calc is server truth). A client
		// running this would apply a GE that replication then contradicts, and every client would
		// compute a slightly different overlap set from its own approximate positions.
		UE_LOG(LogBRCombat, Error, TEXT("UBRGA_Grenade::ApplyExplosionDamage called off the authority; refused."));
		return 0;
	}

	if (BlastRadiusMetres <= 0.f || BlastCentreDamage <= 0.f)
	{
		UE_LOG(LogBRCombat, Warning,
			TEXT("UBRGA_Grenade::ApplyExplosionDamage refused: radius %.2f m / centre damage %.2f. "
				 "Both come from data and neither may be invented here."),
			BlastRadiusMetres, BlastCentreDamage);
		return 0;
	}

	const float RadiusUU = BlastRadiusMetres * MetresToUU;

	// ------------------------------------------------------------------------------------------
	// 1. THE OVERLAP. This is what "radial damage" means in this project.
	// ------------------------------------------------------------------------------------------
	//
	// `AActor::TakeDamage`, `UGameplayStatics::ApplyRadialDamage` and `ApplyPointDamage` are BANNED
	// (law 2/3, and a PreToolUse hook blocks them outright). The ban is not a stylistic preference:
	// the engine's radial helper computes its own damage number, applies it through its own event,
	// and lands OUTSIDE `BRDamageExecCalc` — so shields would not absorb it, `GE_RecentDamage`
	// would not gate regen after it, and `Damage.Explosive.Multiplier` would not touch it. Radial
	// damage means gathering the targets ourselves and applying the ONE GE per target. That is all
	// this function is.
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams OverlapParams(SCENE_QUERY_STAT(BRGrenadeBlast), /*bTraceComplex=*/false);
	World->OverlapMultiByObjectType(
		Overlaps,
		Epicentre,
		FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeSphere(RadiusUU),
		OverlapParams);

	// The world blockers for the line-of-sight step, built once. STATIC AND DYNAMIC WORLD ONLY, and
	// that is a stated design position reached structurally rather than by a filter: **a body never
	// shields another body from a blast.** Querying only world object types makes it impossible to
	// change that by accident later — a `Visibility` trace would have silently made teammates into
	// cover, which is a real gameplay rule nobody would have decided to add.
	FCollisionObjectQueryParams BlastBlockers;
	BlastBlockers.AddObjectTypesToQuery(ECC_WorldStatic);
	BlastBlockers.AddObjectTypesToQuery(ECC_WorldDynamic);

	const FGameplayTag ExplodeCueTag = RequestOwedTag(GrenadeExplodeCueTagString);
	if (ExplodeCueTag.IsValid())
	{
		// A confirmed one-shot on the server path, which is what `gas-purity` §6 asks for: the
		// detonation is not predicted by anyone, so there is nothing to roll back and an `Executed`
		// cue is correct.
		FGameplayCueParameters CueParams;
		CueParams.Location = Epicentre;
		CueParams.RawMagnitude = BlastCentreDamage;
		CueParams.Instigator = InstigatorActor;
		InstigatorASC->ExecuteGameplayCue(ExplodeCueTag, CueParams);
	}
	else
	{
		// *** CUE TAG OWED (BP05): `GameplayCue.Grenade.Explode`, plus its C++ handler class. ***
		// Same one-unit-of-work filing as the throw cue above.
		UE_LOG(LogBRCombat, Warning,
			TEXT("UBRGA_Grenade: '%s' is not declared and has no C++ handler; the explosion is "
				 "silent and invisible. Damage still applies — FX and gameplay are separate legs."),
			GrenadeExplodeCueTagString);
	}

	// One actor produces many overlap hits (capsule, mesh, hitboxes). Without this set, a target
	// takes one grenade's damage once per collider it happens to own — which is a damage bug whose
	// size depends on art.
	TSet<AActor*> Considered;
	int32 DamagedCount = 0;

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Target = Overlap.GetActor();
		if (!Target || Considered.Contains(Target))
		{
			continue;
		}
		Considered.Add(Target);

		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
		if (!TargetASC)
		{
			// No ASC means it is scenery that happens to sit on the Pawn channel, not a fighter.
			continue;
		}

		// ----------------------------------------------------------------------------------
		// SELF-DAMAGE: the thrower is a target like anyone else. STATED, per BP05 step 5.
		// ----------------------------------------------------------------------------------
		//
		// `BRGA_WeaponFire` drops self-hits (`TargetASC == SourceASC`) because a hitscan shot that
		// hits its own shooter is a bug. An explosion that hits its own thrower is the mechanic:
		// the risk of standing near your own grenade is half of what makes grenade play a decision,
		// and it is the arena convention this game is built on. There is deliberately NO
		// `TargetASC == InstigatorASC` skip here, and its absence is the rule.
		//
		// NOT DECIDED HERE, and named rather than assumed: FRIENDLY FIRE. Nothing in this function
		// asks which team the target is on, because that is a MATCH rule (`DT_MatchRules` /
		// GameMode), not a property of an explosion, and inventing a team check here would put a
		// second owner on it. A blast currently damages teammates. If the match rules say
		// otherwise, the filter belongs where the rule lives, not in this loop.

		// ----------------------------------------------------------------------------------
		// 2. LINE OF SIGHT. Without this, an overlap sphere damages through walls.
		// ----------------------------------------------------------------------------------
		//
		// BP05 step 5's critic case is literally "grenade through walls (overlap vs LOS)". A sphere
		// overlap answers "is it within R" and nothing else; the wall between is invisible to it.
		// Re-deriving reachability from the epicentre is what makes this a BLAST rather than a
		// radius, and it happens on the authority for the same reason the whole function does.
		const FVector TargetCentre = Target->GetActorLocation();
		FHitResult Blocker;
		FCollisionQueryParams LosParams(SCENE_QUERY_STAT(BRGrenadeBlastLOS), /*bTraceComplex=*/false);
		LosParams.AddIgnoredActor(Target);
		if (World->LineTraceSingleByObjectType(Blocker, Epicentre, TargetCentre, BlastBlockers, LosParams))
		{
			UE_LOG(LogBRCombat, Verbose,
				TEXT("UBRGA_Grenade blast: '%s' is in radius but '%s' blocks the path; no damage."),
				*GetNameSafe(Target), *GetNameSafe(Blocker.GetActor()));
			continue;
		}

		// ----------------------------------------------------------------------------------
		// 3. FALLOFF, from the curve. Not from an expression typed here.
		// ----------------------------------------------------------------------------------
		//
		// Distance is measured to the target's ACTOR CENTRE, not to the nearest point on its
		// capsule. That is a decision: nearest-point would quietly reward crouching and lying down
		// with extra blast damage, and it would make the authored falloff shape mean something
		// different for a tall pawn than a short one. One distance per target, one curve.
		const float Distance = FVector::Dist(Epicentre, TargetCentre);
		const float NormalisedDistance = FMath::Clamp(Distance / RadiusUU, 0.f, 1.f);

		float FalloffScale = 0.f;
		if (!BRCombatCurves::Evaluate(GrenadeBlastFalloffCurve, NormalisedDistance, FalloffScale))
		{
			// REFUSE, do not substitute. A linear falloff typed here would be a law-3 violation and
			// a second source of truth that silently outranks the CSV. The ability already refuses
			// at activation when this curve is missing; reaching here means the table changed
			// mid-match, which is worth its own line.
			UE_LOG(LogBRCombat, Warning,
				TEXT("UBRGA_Grenade blast: CT_Combat has no '%s' curve. No falloff shape means no "
					 "damage number this file is allowed to compute; blast aborted."),
				*GrenadeBlastFalloffCurve.ToString());
			return DamagedCount;
		}

		const float BaseDamage = BlastCentreDamage * FalloffScale;
		if (BaseDamage <= 0.f)
		{
			// A curve may legitimately author zero at the edge. That is "out of the blast", not an
			// error, and applying a zero-damage GE would still fire the RecentDamage regen gate.
			continue;
		}

		// ----------------------------------------------------------------------------------
		// 4. THE ONE PIPELINE. One GE per target, one type tag, FLAT (R22).
		// ----------------------------------------------------------------------------------
		//
		// `{Damage.Explosive}` — never a nested leaf, and never `Damage.Explosive.Something`.
		// This file does NOT read `Damage.Explosive.Multiplier`: `BRDamageExecCalc` derives that
		// curve name from the tag itself and composes it, so the explosive multiplier is applied
		// exactly once, in the one place that owns the damage rule. Reading it here as well would
		// square it — the D8 headshot defect in a different costume.
		FGameplayTagContainer DamageTags;
		DamageTags.AddTag(BRGameplayTags::Damage_Explosive);

		FGameplayEffectContextHandle Context = InstigatorASC->MakeEffectContext();
		Context.AddInstigator(InstigatorActor, InstigatorActor);
		// The epicentre travels on the context so impact cues and any future knockback know which
		// way "away" is, without a second parameter channel.
		Context.AddOrigin(Epicentre);

		const FGameplayEffectSpecHandle Spec = UBRGE_Damage::MakeSpec(InstigatorASC, BaseDamage, DamageTags, Context);
		if (UBRGE_Damage::ApplyToTarget(Spec, InstigatorASC, TargetASC))
		{
			++DamagedCount;
		}
	}

	UE_LOG(LogBRCombat, Verbose,
		TEXT("UBRGA_Grenade blast at %s: %d of %d overlapped actors damaged (radius %.2f m)."),
		*Epicentre.ToCompactString(), DamagedCount, Considered.Num(), BlastRadiusMetres);

	return DamagedCount;
}

// ================================================================================================
// Teardown and helpers
// ================================================================================================

void UBRGA_Grenade::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// EVERY path, including the cancelled one. A leaked release task fires against the NEXT
	// activation and throws a grenade the player never cooked; a leaked cook timer fires into a
	// dead ability. This is the cook-cancel residue BP05 step 5 asks the critic to hunt for, and it
	// is prevented here rather than detected there.
	ClearCook();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UBRGA_Grenade::ClearCook()
{
	if (CookReleaseTask)
	{
		// EndTask unbinds the delegate as it tears down, so there is no separate RemoveDynamic.
		CookReleaseTask->EndTask();
		CookReleaseTask = nullptr;
	}

	if (CookTimerHandle.IsValid())
	{
		if (const UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(CookTimerHandle);
		}
		CookTimerHandle.Invalidate();
	}
}

bool UBRGA_Grenade::GetViewPoint(FVector& OutLocation, FVector& OutDirection) const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		return false;
	}

	// The same seam `BRGA_WeaponFire` uses, for the same reason: on the server for a remote pawn,
	// `GetActorEyesViewPoint` returns the replicated control rotation — which is precisely the
	// server's own opinion of where the thrower is looking, and this ability accepts no other.
	FRotator ViewRotation;
	Avatar->GetActorEyesViewPoint(OutLocation, ViewRotation);
	OutDirection = ViewRotation.Vector();
	return true;
}
