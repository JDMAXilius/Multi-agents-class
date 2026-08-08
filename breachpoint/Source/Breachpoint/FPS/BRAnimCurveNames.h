#pragma once

#include "CoreMinimal.h"

/**
 * Every animation curve name the spine reads, in one place, as C++ constants.
 *
 * WHY A HEADER OF NAMES IS NOT BUREAUCRACY HERE. A curve is a contract between an animation
 * asset and the code that samples it, and the contract is a **string**. `GetCurveValue("TurnYaw")`
 * typed into two files with one typo is a curve that silently reads 0.0 forever — no error, no
 * warning, and a turn-in-place that simply never turns. That failure mode is the same one the
 * notify seam already shipped once with magic strings, and this is the same fix applied before
 * it can happen rather than after.
 *
 * These are `FName` constants and not a `UENUM` because the engine's curve API takes `FName`;
 * an enum would need a lookup table, which is a second place to be wrong.
 *
 * NOTHING READS THESE YET, and that is stated rather than hidden. `TurnYaw` is the consumer
 * `contract_gap BP82-4` describes: `RootYawOffset` accumulates with nothing to eat it, because
 * the turn-in-place ANIMATION is what eats it in Lyra and no ABP exists. When that ABP is
 * authored, its curve must be named exactly this, and the constant is here so the two halves
 * cannot be spelled differently.
 */
namespace BRAnimCurves
{
	/**
	 * Turn-in-place yaw consumed by the animation, in degrees.
	 *
	 * The template sampled the same idea as `turnYawCurveValue` (inventory, `ABP_Mannequin_Base`).
	 * The animation reports how far it has ALREADY turned the body; the spine subtracts that from
	 * `RootYawOffset`. Without it the offset only ever grows and pins to its clamp.
	 */
	static const FName TurnYaw = TEXT("TurnYaw");

	/**
	 * Distance remaining in a start/stop animation, in centimetres.
	 *
	 * Distance matching plays an animation by DISTANCE COVERED rather than by time, which is what
	 * removes foot-skate on a stop. It is a curve and not a computed value because only the
	 * animator knows how far each authored clip travels.
	 */
	static const FName DistanceToMarker = TEXT("DistanceToMarker");

	/**
	 * 0..1 mask for how much additive the upper body will accept on this frame.
	 *
	 * Authored on montages that own the upper body outright (reload, weapon swap) so the clip
	 * itself can say "stop layering sway on me here" at a moment the animator picks, rather than
	 * the spine guessing from a state bool.
	 */
	static const FName UpperBodyAdditiveMask = TEXT("UpperBodyAdditiveMask");

	/** Non-zero on the frames a foot is planted. The gate any foot-lock or IK pass keys off. */
	static const FName FootPlantLeft = TEXT("FootPlantLeft");
	static const FName FootPlantRight = TEXT("FootPlantRight");
}
