// Breachpoint. OUR radial damage — the one blast rule, shared by the grenade and the rocket.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

class AActor;
class UAbilitySystemComponent;

/**
 * ================================================================================================
 * BRExplosion — the project's ONE radial-damage rule, and the reason `ApplyRadialDamage` is not
 * merely banned but unnecessary.
 * ================================================================================================
 *
 * `gas-purity.md` law 3: *"Radial = our own overlap query -> per-target GE application (grenade and
 * rocket share it)."* Radial damage in this project is four steps, all of them in one function:
 *
 *   1. overlap a sphere for pawns, on the AUTHORITY
 *   2. drop anything the blast cannot see (an overlap alone damages through walls)
 *   3. scale the centre damage by the falloff curve at the normalised distance
 *   4. apply ONE `UBRGE_Damage` per surviving target, tagged by the caller — FLAT (R22)
 *
 * ------------------------------------------------------------------------------------------------
 * WHY THIS LIVES IN `Weapons/` — decision D6, taken, with the reasoning kept where it is useful
 * ------------------------------------------------------------------------------------------------
 *
 * This function was born on `UBRGA_Grenade` as a public static, and that file filed the problem
 * against itself in as many words: *"KNOWN ARROW PROBLEM, filed not hidden. If D6 rules the
 * projectile into `Weapons/` ... that folder calling into `AbilitySystem/Abilities/` reverses the
 * arrow `BRGA_WeaponFire` establishes (ability -> weapon)."*
 *
 * **The dependency arrow in this codebase is one-way today.** Four files in
 * `AbilitySystem/Abilities/` include `Weapons/` — `BRGA_WeaponFire`, `BRGA_WeaponUtility`,
 * `BRGA_Grapple`, `BRGA_Grenade` — and **nothing in `Weapons/` includes `Abilities/`**. D6 put the
 * projectile in `Weapons/` (option (a), `ARCHITECTURE` §3.5 gains a fourth unit), so the blast rule
 * had to come with it: a projectile in `Weapons/` calling `UBRGA_Grenade::ApplyExplosionDamage`
 * would have reversed that arrow on its first line. Moving it here preserves the direction AND
 * hands BP09's rocket the identical helper, which is the *"grenade and rocket share it"* the
 * contract asks for, rather than a second blast that drifts from the first.
 *
 * A new top-level `Explosions/` folder was rejected: it costs a new ARCHITECTURE §3 section and a
 * new §9 owner-path row for no benefit over a folder whose owner (sim-builder) is already correct
 * and which already holds actor-shaped weapon things.
 *
 * ------------------------------------------------------------------------------------------------
 * WHAT MOVED, AND WHAT DID NOT
 * ------------------------------------------------------------------------------------------------
 *
 * The body below is `UBRGA_Grenade::ApplyExplosionDamage` MOVED, not rewritten. Every decision it
 * carried is intact and each is load-bearing:
 *
 *   - the blast's LOS trace queries `WorldStatic`/`WorldDynamic` ONLY, structurally, so **a body can
 *     never shield a body**;
 *   - distance is measured to the target's ACTOR CENTRE, never the nearest capsule point, so
 *     crouching does not quietly buy blast damage;
 *   - **the thrower IS a target** — there is deliberately no self-skip, and its absence is the rule;
 *   - **friendly fire is deliberately undecided here**: that is a MATCH rule, not a property of an
 *     explosion.
 *
 * TWO THINGS BECAME PARAMETERS in the move, because "shared" means the rocket must not read the
 * grenade's constants:
 *
 *   - `FalloffCurveName` — the shape is authored against a NORMALISED distance precisely so one
 *     curve serves any radius, but its CSV row is called `Grenade.BlastFalloff` today. Renaming a
 *     row is a `Content/Data` edit no code packet may make, so the name arrives from the caller and
 *     BP09 changes an argument rather than this file.
 *   - `ExplodeCueTag` — `GameplayCue.Grenade.Explode` is a grenade leaf. A shared helper that
 *     hardcoded it would raise a grenade cue for a rocket.
 *
 * SERVER ONLY. Refuses loudly off the authority rather than half-applying a blast that the clients
 * would each compute differently.
 */
namespace BRExplosion
{
	/**
	 * Gather the blast's targets on the authority and apply the ONE damage GE to each.
	 *
	 * @param InstigatorASC       the THROWER's/FIRER's ASC — the source of the spec, so kill credit,
	 *                            instigator tags and the effect context all attribute correctly. A
	 *                            projectile that outlives its instigator passes null and is refused.
	 * @param InstigatorActor     the instigator's avatar, for the effect context's instigator pair.
	 * @param Epicentre           world-space detonation point.
	 * @param BlastRadiusMetres   from data, resolved by the caller. Zero or negative is refused.
	 * @param BlastCentreDamage   from data; damage at distance 0, before falloff and before the exec
	 *                            calc's `Damage.Explosive.Multiplier`.
	 * @param FalloffCurveName    the `CT_Combat` curve evaluated at the NORMALISED distance (0 at the
	 *                            epicentre, 1 at the edge) returning a damage SCALE. `NAME_None` is
	 *                            refused — this file computes no falloff of its own.
	 * @param ExplodeCueTag       the detonation's GameplayCue. An invalid tag logs and applies damage
	 *                            anyway: FX and gameplay are separate legs.
	 * @return the number of targets the blast actually damaged. Zero is a legitimate answer (a
	 *         grenade in an empty room) — the log distinguishes it from a refusal.
	 */
	BREACHPOINT_API int32 ApplyExplosionDamage(
		UAbilitySystemComponent* InstigatorASC,
		AActor* InstigatorActor,
		const FVector& Epicentre,
		float BlastRadiusMetres,
		float BlastCentreDamage,
		FName FalloffCurveName,
		const FGameplayTag& ExplodeCueTag);
}
