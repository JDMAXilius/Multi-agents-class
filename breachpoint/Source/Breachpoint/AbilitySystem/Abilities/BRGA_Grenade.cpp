// BREACHPOINT — BP05 step 1. The grenade: cook, throw, and the server-only projectile spawn.
// (OUR radial damage moved to Weapons/BRExplosion.h when D6 closed — see the header.)

#include "AbilitySystem/Abilities/BRGA_Grenade.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "AbilitySystem/BRAbilitySystemComponent.h"
#include "AbilitySystem/BRCombatCurves.h"
#include "AbilitySystem/Effects/BRGameplayEffects.h"
#include "Core/BRCore.h"
#include "Core/BRGameplayTags.h"
#include "Engine/World.h"
#include "TimerManager.h"

// THE DEPENDENCY ARROW, and it points one way. `AbilitySystem/Abilities/` includes `Weapons/`;
// nothing in `Weapons/` includes `Abilities/`. This one include is the whole of D6's cost, and it
// is why the blast rule moved to `Weapons/BRExplosion.h` instead of staying here — the projectile
// calls the blast, so the blast had to be on the projectile's side of the arrow. Note this file
// does NOT include `BRExplosion.h`: the ability never detonates anything.
#include "Weapons/BRProjectile.h"

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
	 * BOUNCE, and the two rows that DO NOT EXIST YET.
	 *
	 * `CT_Combat.csv` carries eight `Grenade.*` rows as of 1 Aug 2026 — cook, fuse, throw speed,
	 * blast radius, centre damage, the falloff curve, the carried max and the per-throw cost — and
	 * neither of these two. They are read OPTIONALLY (see `ResolveTuning`): a missing bounce row
	 * leaves the tuning's negative sentinel in place and the projectile then leaves
	 * `UProjectileMovementComponent`'s own value standing and warns, rather than this file typing a
	 * restitution.
	 *
	 * That is a WEAKER position than the five required rows get, and it is taken deliberately rather
	 * than by omission: `ABRProjectile::InitializeProjectile` argues it in full. The short form is
	 * that a grenade with the engine's restitution is playable and visibly wrong within one throw,
	 * whereas a grenade with a zero radius is nothing at all — so the two failures do not deserve
	 * the same answer. Add the rows and this softness disappears.
	 */
	const FName GrenadeBouncinessCurve(TEXT("Grenade.Bounciness"));
	const FName GrenadeBounceFrictionCurve(TEXT("Grenade.BounceFriction"));

	/**
	 * Tag strings this packet needs and may not declare. See `UBRGA_Grenade::RequestOwedTag`.
	 * Constants rather than literals at three call sites: a misspelling then fails once, visibly.
	 */
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
	// THE COST — a CARRIED COUNT, not a cooldown. Gap closed 1 Aug 2026.
	// ==========================================================================================
	//
	// BP05 step 1 specifies "2 carried, reset on respawn via the `GE_InitStats` path". That is an
	// attribute plus a cost GE, and both now exist: `UBRAttributeSet::Grenades`/`MaxGrenades` and
	// `UBRGE_GrenadeCost`. This ability was written while they did not and said so loudly rather
	// than quietly shipping a free grenade.
	//
	// **Setting `CostGameplayEffectClass` ALONE WOULD BE A TRAP**, and it is the trap a reader is
	// most likely to walk into, so it is named here: the engine's default `CheckCost`/`ApplyCost`
	// build their OWN spec and set no SetByCaller. `UBRGE_GrenadeCost`'s magnitude IS a
	// SetByCaller, so it would evaluate to **0** — the check would pass at zero grenades and the
	// apply would deduct nothing. A cost that is wired, looks wired, and costs nothing.
	//
	// So both are overridden below and routed through `UBRGE_GrenadeCost::MakeSpec`. It is still
	// the GE cost path — same prediction, same automatic rollback on server rejection, same
	// atomicity with `CommitAbility` — it only supplies the number the engine's convenience path
	// cannot know.
	CostGameplayEffectClass = UBRGE_GrenadeCost::StaticClass();

	bCommitOnActivate = true;
}

// ================================================================================================
// Cost — the GE path, with the magnitude the engine's default cannot supply
// ================================================================================================

namespace
{
	/** `CT_Combat` row for what one throw costs. Absent = refuse, never a default (law 3). */
	const FName GrenadeCostPerThrowCurve(TEXT("Grenade.CostPerThrow"));
}

FGameplayEffectSpecHandle UBRGA_Grenade::MakeCostSpec() const
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC || !UBRGE_GrenadeCost::IsOperational())
	{
		return FGameplayEffectSpecHandle();
	}

	float CostPerThrow = 0.f;
	if (!BRCombatCurves::Evaluate(GrenadeCostPerThrowCurve, CostPerThrow) || CostPerThrow <= 0.f)
	{
		// REFUSE rather than assume 1. A grenade that costs nothing because a CSV row is missing
		// is unlimited grenades, which is the loudest possible balance defect and the quietest
		// possible code path.
		UE_LOG(LogBRCombat, Error,
			TEXT("UBRGA_Grenade: CT_Combat has no '%s' row, so the throw cost is unknown. Refusing "
				 "rather than defaulting — a free grenade is not a safe fallback."),
			*GrenadeCostPerThrowCurve.ToString());
		return FGameplayEffectSpecHandle();
	}

	// MakeSpec takes a POSITIVE count and applies the sign itself, in one place, because a
	// forgotten minus does not cost a grenade — it grants one.
	return UBRGE_GrenadeCost::MakeSpec(ASC, CostPerThrow, ASC->MakeEffectContext());
}

bool UBRGA_Grenade::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	const FGameplayEffectSpecHandle Spec = MakeCostSpec();
	if (!Spec.IsValid())
	{
		// No usable cost spec means we cannot prove the player can pay. Refusing is the safe
		// direction: the alternative is a free throw whenever the data is broken.
		return false;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	// This is where "cannot throw at zero grenades" actually comes from — the modifier is
	// AddBase with a negative magnitude, so CanApplyAttributeModifiers refuses when the result
	// would go below zero. No branch in CanActivateAbility, and nothing for anyone to forget.
	return ASC && ASC->CanApplyAttributeModifiers(
		*Spec.Data.Get(), 1.f, ASC->MakeEffectContext());
}

void UBRGA_Grenade::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	const FGameplayEffectSpecHandle Spec = MakeCostSpec();
	if (!Spec.IsValid())
	{
		return;
	}

	// Applied through the ability's own path so it lands under the activation's prediction key:
	// that is what makes a cook-cancel or a server rejection refund the grenade automatically,
	// which is BP05's Done-when box 3 with no custom rollback code.
	ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
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

	// The two bounce numbers, read OPTIONALLY. `Evaluate` writes nothing when the row is missing, so
	// a failed read leaves the struct's negative sentinel exactly where the constructor put it —
	// which is the value that means "unauthored" all the way down to the projectile. There is no
	// `OutReason` write and no early return here on purpose; see the curve names above for why these
	// two do not refuse the throw the way the five above do.
	BRCombatCurves::Evaluate(GrenadeBouncinessCurve, Resolved.Bounciness);
	BRCombatCurves::Evaluate(GrenadeBounceFrictionCurve, Resolved.BounceFriction);

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
	// The base commits — which now COSTS A GRENADE through the overridden ApplyCost — and will
	// have ended us if the
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
	// THE SEAM. It was the blocker for a packet; D6 closed and it is now four lines and a log.
	// ==========================================================================================
	//
	// `Weapons/BRProjectile.h` is the far side, and its class comment IS the specification this
	// function used to carry — authority, attribution, bandwidth/dormancy, `BRCollision::Projectile`,
	// bounce, the fuse timer, the detonation call. Nothing was lost in the move; it was relocated to
	// the object that has to honour it.
	//
	// THE NUMBERS TRAVEL WITH THE SPAWN. Radius, centre damage, the falloff curve's NAME and the
	// explode cue tag are all resolved on THIS side — the first two at activation, by
	// `ResolveTuning`, out of the same read that the cook and the throw used. The projectile does
	// not re-read `CT_Combat` at detonation and must not: a mid-match table swap would otherwise
	// detonate a grenade with numbers its thrower never agreed to, and two grenades in the air would
	// behave differently for no visible reason.
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogBRCombat, Error,
			TEXT("UBRGA_Grenade: no world at the spawn; the throw was authorised and nothing left the hand."));
		return;
	}

	AActor* Thrower = GetAvatarActorFromActorInfo();
	UAbilitySystemComponent* ThrowerASC = GetAbilitySystemComponentFromActorInfo();
	if (!Thrower || !ThrowerASC)
	{
		// A projectile with no thrower produces a kill with no killer. Refuse rather than spawn one
		// that cannot attribute its own damage.
		UE_LOG(LogBRCombat, Error,
			TEXT("UBRGA_Grenade: no avatar actor or no ASC at the spawn (avatar '%s'); nothing was "
				 "spawned, because an unattributable grenade is a scoring bug three systems away."),
			*GetNameSafe(Thrower));
		return;
	}

	// *** CUE TAG OWED (BP05): `GameplayCue.Grenade.Explode`, plus its C++ handler class. ***
	// Resolved HERE, at the spawn, rather than at the detonation — same rule as the numbers, and it
	// also makes the gap audible seconds earlier. An invalid tag does NOT stop the throw: the blast
	// still applies damage and is merely silent, because FX and gameplay are separate legs.
	const FGameplayTag ExplodeCueTag = RequestOwedTag(GrenadeExplodeCueTagString);
	if (!ExplodeCueTag.IsValid())
	{
		UE_LOG(LogBRCombat, Warning,
			TEXT("UBRGA_Grenade: '%s' is not declared and has no C++ handler, so this grenade's "
				 "detonation will be silent and invisible. Damage is unaffected."),
			GrenadeExplodeCueTagString);
	}

	FBRProjectileSpawnParams Params;
	Params.InstigatorActor = Thrower;
	Params.InstigatorASC = ThrowerASC;
	Params.LaunchVelocity = LaunchVelocity;
	Params.FuseSeconds = RemainingFuseSeconds;
	Params.Bounciness = Tuning.Bounciness;
	Params.BounceFriction = Tuning.BounceFriction;
	Params.BlastRadiusMetres = Tuning.BlastRadiusMetres;
	Params.BlastCentreDamage = Tuning.BlastCentreDamage;
	Params.BlastFalloffCurveName = GrenadeBlastFalloffCurve;
	Params.ExplodeCueTag = ExplodeCueTag;

	// `ABRProjectile::StaticClass()` and not an `EditDefaultsOnly TSubclassOf`: R18 means there is
	// no Blueprint projectile to point at, and R26's default-only BP child would carry nothing this
	// grenade needs. This is a CLASS reference, not an ASSET reference, so law 3's soft-ref rule and
	// the `ConstructorHelpers` ban do not reach it. BP09 passes its own class through the same
	// parameter without this file learning about rockets.
	const ABRProjectile* Spawned = ABRProjectile::SpawnProjectile(
		World, ABRProjectile::StaticClass(), ReleaseTransform, Params);

	if (!Spawned)
	{
		// The factory already named the field it refused over; this line says which throw it was, so
		// a log reader can tell "one grenade failed" from "grenades are broken".
		UE_LOG(LogBRCombat, Error,
			TEXT("UBRGA_Grenade: the projectile spawn FAILED (release at %s, launch %.0f uu/s, %.2f s "
				 "of fuse left). The throw was authorised on the authority and nothing is in the air; "
				 "the refusal above names the reason."),
			*ReleaseTransform.GetLocation().ToCompactString(), LaunchVelocity.Size(), RemainingFuseSeconds);
		return;
	}

	UE_LOG(LogBRCombat, Verbose,
		TEXT("UBRGA_Grenade: thrown by '%s' — release at %s, launch %.0f uu/s, %.2f s of fuse, blast "
			 "%.2f m / %.1f centre."),
		*GetNameSafe(Thrower), *ReleaseTransform.GetLocation().ToCompactString(), LaunchVelocity.Size(),
		RemainingFuseSeconds, Tuning.BlastRadiusMetres, Tuning.BlastCentreDamage);
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
