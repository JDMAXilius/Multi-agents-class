#pragma once

#include "CoreMinimal.h"

#include "Animation/BRProceduralTypes.h"

#include "BRAimAndLeanTypes.generated.h"

/**
 * Aim-offset and lean distribution, from `S_Procedural_AimAndLean`.
 *
 * WHAT THIS SOLVES, because "spine weights" undersells it. An aim offset is not one bone rotating
 * — it is a total rotation SHARED OUT along the spine, and the share decides whether a character
 * looking up reads as glancing (head-weighted) or as arching backward (spine-weighted). The
 * template ships the measurement: `aimSpineWeights_UE5` = head 0.1, neck_01 0.15, neck_02 0.2,
 * spine_01 0.15, spine_02..05 0.1 — noticeably neck-heavy, which is what makes an aiming pose
 * read as *aiming* rather than as bending.
 *
 * FOUR STRUCTS COLLAPSE TO TWO FIELDS HERE. The source carries
 * `{aim,lean}SpineWeights_{UE4,UE5}` as four separate named-float structs, because the bone
 * COUNT differs per skeleton (8 vs 5) and a named-float struct cannot vary. `FBRSpineWeights` is
 * a map, so the skeleton is data rather than a type — and a third skeleton costs nothing.
 *
 * The lean variants label their head entry "Opposite Angle Weight": the head counter-rotates
 * against the lean so the character keeps looking where they were looking while their body tips.
 * That is intent, not a different type, and it is preserved as a comment rather than a struct.
 */
USTRUCT(BlueprintType)
struct FBRAimAndLeanInfo
{
	GENERATED_BODY()

	/** How the aim rotation is shared along the spine. Neck-heavy in the source, deliberately. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breachpoint|Aim")
	FBRSpineWeights AimSpineWeights;

	/**
	 * How the lean rotation is shared. The head entry counter-rotates — the body tips, the gaze
	 * stays put.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breachpoint|Lean")
	FBRSpineWeights LeanSpineWeights;

	/** Maximum lean, degrees. Reached at full turn rate, never exceeded. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breachpoint|Lean")
	float LeanAngle = 12.f;

	/**
	 * How fast lean chases its target. Source `leaningSpeed` = 8, on the component rather than in
	 * the struct — an interp, not a spring, so lean settles without overshoot.
	 *
	 * Recorded here rather than left on a component because a per-weapon profile that cannot set
	 * its own response rate is only half a profile: a heavy weapon should lean lazily.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breachpoint|Lean")
	float LeanInterpSpeed = 8.f;
};
