// Breachpoint. THE damage rule. Base magnitude x flat Damage.* multipliers, from data.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"

#include "BRDamageExecCalc.generated.h"

/**
 * UBRDamageExecCalc — the ONE place a damage number is computed (gas-purity.md law 3).
 *
 * THE RULE, in one line:
 *
 *     Final = SetByCaller.BaseDamage  x  product over every Damage.* tag on the spec of
 *             CT_Combat["<TagName>.Multiplier"]
 *
 * and the result is written to `UBRAttributeSet::IncomingDamage`, the meta attribute. This
 * execution does NOT split shields and health — the attribute set does, in one place, for every
 * source (see `UBRAttributeSet::ApplyIncomingDamageShieldsFirst`). BP02's packet describes the
 * pipeline end-to-end as "shields absorb -> overflow"; that half of the sentence belongs to the
 * attribute set and duplicating it here would give the shield seam two masters.
 *
 * WHY THE MULTIPLIERS COMPOSE INSTEAD OF SWITCHING. Ruling R22: `Damage.*` is FLAT. A hit carries
 * one TYPE tag (`Kinetic`/`Explosive`/`Melee`) plus zero or more MODIFIER tags (`Headshot`,
 * `Rear`), and the two axes are independent. A rear melee is the pair
 * `{Damage.Melee, Damage.Rear}` and multiplies both curves — there is no `Damage.Melee.Rear` to
 * look up and a grep for one will correctly find nothing.
 *
 * THE CURVE NAME IS DERIVED, NOT MAPPED. `Damage.Headshot` reads
 * `CT_Combat["Damage.Headshot.Multiplier"]`. There is no switch statement and no table of tag ->
 * curve pairs, so a new damage modifier is a tag plus a CSV row and zero lines of code — which is
 * the property that keeps balance a data change (law 3).
 *
 * A `Damage.*` TAG WITH NO CURVE ROW CONTRIBUTES 1.0. That is a decision, not a fallthrough: the
 * identity is the only value that cannot change an outcome, the miss is logged once per curve
 * name per process, and `CT_Combat` ships a row for all five tags so the reachable path is the
 * one the CSV states. The alternative (refuse the hit) turns a missing CSV row into an
 * invulnerable player, which is a worse failure and a harder one to diagnose.
 *
 * WHAT THIS CLASS DELIBERATELY DOES NOT DO:
 *   - It captures NO attributes. There is no armor, no resistance and no damage-reduction
 *     attribute in this game; adding a capture "for later" would make every damage event pay for
 *     an aggregator snapshot nothing reads.
 *   - It does not decide whether a hit WAS a headshot or a rear hit. The server-side ability
 *     validates the claim and stamps the tag (gas-purity.md §5); this execution trusts the spec
 *     it is handed, because by the time it runs the spec is server-authored.
 *   - It contains no randomness. Damage variance, if ever designed, arrives as a seeded stream
 *     passed in through the spec — never as a call to the global RNG inside a rule.
 */
UCLASS(meta = (DisplayName = "BR Damage Execution"))
class BREACHPOINT_API UBRDamageExecCalc : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	UBRDamageExecCalc();

	virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;

	/**
	 * THE RULE AS A PURE FUNCTION — no world, no ASC, no assets, no engine subsystem.
	 *
	 * Factored out so the sim's pinned suites can assert exact known cases and property-style
	 * invariants (a multiplier of 1 changes nothing; multipliers commute; zero base is zero out;
	 * damage is never negative) headlessly, in microseconds, with the coefficients stated by the
	 * test rather than loaded from content. Anything a spec cannot reach is a rule in the wrong
	 * place.
	 *
	 * @param BaseDamage   the SetByCaller.BaseDamage magnitude. Negative is refused (returns 0).
	 * @param DamageTags   the spec's tags; only those under `Damage.*` are consulted.
	 * @param CurveLookup  resolves a curve name to a multiplier. Returns false when the curve does
	 *                     not exist, which this function reads as the identity 1.0.
	 * @param OutMissingCurves optional: receives every curve name the lookup could not resolve, so
	 *                     the caller (and a spec) can report the data gap rather than infer it.
	 */
	static float ComputeFinalDamage(float BaseDamage, const FGameplayTagContainer& DamageTags, TFunctionRef<bool(FName /*CurveName*/, float& /*OutValue*/)> CurveLookup, TArray<FName>* OutMissingCurves = nullptr);

	/** `Damage.Headshot` -> `Damage.Headshot.Multiplier`. The whole tag->curve mapping. */
	static FName MakeMultiplierCurveName(const FGameplayTag& DamageTag);
};
