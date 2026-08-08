#pragma once

#include "CoreMinimal.h"

#include "BRSwayAndLagTypes.generated.h"

/**
 * Per-weapon sway and lag tuning. Ported from `S_ProceduralSwayAndLagInfo` (10 fields).
 *
 * SWAY vs LAG, because the template distinguishes them and the distinction is not obvious:
 *   · **Sway** is ROTATIONAL — the weapon rotates behind the camera as you turn and settles.
 *   · **Lag** is POSITIONAL — the weapon trails in space as you move, then catches up.
 * They have separate multipliers and separate interp speeds because they are separate physical
 * intuitions: sway is the weight of the gun resisting a turn, lag is the body carrying it.
 *
 * THE FINDING THAT MATTERS MOST is not a field, it is how the source computes the input:
 * `swayPrevControlRotation` → `swayControlRotDelta` on `BPC_FPST_Procedural_SwayAndLag`. Sway is
 * driven by the **rate of change of the control rotation**, never by the rotation itself. That is
 * independent corroboration of the pitch defect fixed earlier in this packet, where the pitch
 * spring was fed an angle while the yaw spring was fed a rate — and it is the second such
 * corroboration, after `ABP_Mannequin_Base`'s own `camRotRate`.
 *
 * `weaponName` in the source is the map key of `infoMap`, keyed by anim layer class name
 * (`"ABP_FPSMT_AssaultRifleAnimLayers"`). We key by **weapon row** instead: C++ knows tags and
 * row handles, never an asset (rework governing idea 5), and `UBRAnimLayerInstance::WeaponRow`
 * already carries that handle.
 */
USTRUCT(BlueprintType)
struct FBRSwayAndLagInfo
{
	GENERATED_BODY()

	/**
	 * How fast rotational sway chases its target.
	 *
	 * **3, and the correction matters more than the number.** This shipped as 10 with the comment
	 * "Source default 10" — which was `lag_InterpSpeed`, one line below it in the same struct.
	 * The inventory is unambiguous: `defaultInfo.sway_InterpSpeed` is 3 and **all nine `infoMap`
	 * entries** are 3. No weapon in the pack has 10.
	 *
	 * A transcription error is cheap; a transcription error wearing a provenance citation is not,
	 * because the citation is what stops the next reader checking.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breachpoint|Sway", meta = (ClampMin = "0.01"))
	float SwayInterpSpeed = 3.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breachpoint|Sway")
	float SwayMultiplyDefault = 1.f;

	/**
	 * Sway while aiming down sights. Separate, and it is the field that decides whether a scope
	 * is usable: full hip-fire sway on a magnified optic makes the target unhittable.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breachpoint|Sway")
	float SwayMultiplyADS = 0.5f;

	/** How far the weapon may trail the camera positionally, in cm. Source default 1.6. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breachpoint|Lag")
	float LagDistance = 1.6f;

	/** Any positive value is stable at any timestep -- `VInterpTo` clamps its own alpha. But ZERO
	 *  snaps to the target instantly, which is the opposite of what "no interpolation" suggests. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breachpoint|Lag", meta = (ClampMin = "0.01"))
	float LagInterpSpeed = 10.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breachpoint|Lag")
	float LagMultiplyDefault = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breachpoint|Lag")
	float LagMultiplyADS = 0.5f;

	/**
	 * Vertical lag while airborne. The source documents this one in the asset itself —
	 * *"You can adjust the value of moving up and down when in the air"* — the only field of the
	 * ten the original author felt needed explaining.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breachpoint|Lag")
	float LagMultiplyAir = 0.5f;

	/**
	 * How much of the lag is expressed as rotation rather than translation. Source default **8**,
	 * and per-weapon 5 (knife, unarmed), 11 and 12 — the widest real spread in the whole profile,
	 * which is the clearest single argument that this struct belongs in a per-weapon table.
	 * Shipped as 1.0 here, an eightfold understatement.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breachpoint|Lag")
	float LagRotationMultiply = 8.f;
};
