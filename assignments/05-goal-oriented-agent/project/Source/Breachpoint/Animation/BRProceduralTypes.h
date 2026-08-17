#pragma once

#include "CoreMinimal.h"

#include "BRProceduralTypes.generated.h"

/**
 * The shared vocabulary of the procedural weapon-motion system, ported from the FPS template's
 * `S_Procedural_*` structs (`mcp-bp/bp_inventory.json`, `procedural_structs` set).
 *
 * WHY THESE EXIST AS C++ AT ALL, WHEN THE ORIGINALS ARE ASSETS. A `UserDefinedStruct` is the
 * worst of both worlds under our laws: it holds a SHAPE, which is code, in a binary file, which
 * is unreviewable. R18's reasoning applies exactly — no diff, no merge, no grep. Their field
 * lists are now here, in text, and the numbers that fill them belong in `Content/Data` rows
 * where a designer can tune them (law 3). Shape in code, values in data, neither in an asset.
 *
 * WHAT WAS NOT COPIED. The template runs all of this on **Tick**, from components on the pawn
 * (`primaryActorTick.bCanEverTick: true`, five `BPC_FPST_Procedural_*` components). Law 4
 * forbids gameplay Tick and law 1 puts anim maths on the worker thread, so BREACHPOINT computes
 * the same quantities inside `UBRAnimInstance`'s thread-safe pass. **These are the data shapes,
 * not the architecture** — the architecture was measured, judged, and deliberately not adopted.
 */

/**
 * One spring's constants. Directly `S_Procedural_SpringSetting` — `{spring_Stiffness, spring_Damping}`.
 *
 * Worth recording that this is the entire struct: two floats. `FBRSpring1D` in `BRAnimTypes.h`
 * was written before this was read and takes exactly the same pair, which is weak evidence that
 * the shape is simply correct rather than that either party was clever.
 */
USTRUCT(BlueprintType)
struct FBRSpringSetting
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breachpoint|Procedural")
	float Stiffness = 90.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breachpoint|Procedural")
	float Damping = 14.f;
};

/**
 * A transient impulse with an expiry. `S_Procedural_ForceInfo` — `{endTime, location, rotation}`.
 *
 * The template keeps an array of these (`forceInfoArray` on the recoil component) and sums the
 * live ones each Tick, which is how several shots fired quickly stack into one kick that decays
 * as a whole rather than restarting. That behaviour is worth keeping even though the Tick is not.
 *
 * `EndTime` is an absolute timestamp, not a remaining duration: a remaining-duration field has to
 * be decremented by every consumer, and a consumer that runs twice in a frame decrements twice.
 */
USTRUCT(BlueprintType)
struct FBRProceduralForce
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|Procedural")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|Procedural")
	FRotator Rotation = FRotator::ZeroRotator;

	/** World seconds at which this force is spent. Compared against, never counted down. */
	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|Procedural")
	float EndTime = 0.f;
};

/**
 * Per-bone weights for distributing a rotation along the spine.
 *
 * `S_Procedural_AimSpineInfoItem_UE5` is 8 named floats (`head`, `neck_01`, `neck_02`,
 * `spine_01`…`spine_05`); the `_UE4` variant is **5** (`head`, `neck_01`, `spine_01`…`spine_03`).
 * The template needed two structs and two code paths for one idea, because it named the bones in
 * the type.
 *
 * A map keyed by bone name is one type for both skeletons, and for any third. The lean variants
 * (`S_Procedural_LeanSpineInfoItem_*`) are the same shape again, differing only in that their
 * head entry is labelled "Opposite Angle Weight" — a comment about intent, not a different type.
 */
USTRUCT(BlueprintType)
struct FBRSpineWeights
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breachpoint|Procedural")
	TMap<FName, float> BoneWeights;

	/** Sum of all weights, for normalising a distribution that does not add to 1. */
	float TotalWeight() const
	{
		float Total = 0.f;
		for (const TPair<FName, float>& Pair : BoneWeights)
		{
			Total += Pair.Value;
		}
		return Total;
	}
};
