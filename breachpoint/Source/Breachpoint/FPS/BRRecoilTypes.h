#pragma once

#include "CoreMinimal.h"

#include "FPS/BRProceduralTypes.h"

#include "BRRecoilTypes.generated.h"

class UCurveFloat;

/**
 * Recoil shapes, from `S_Procedural_RecoilInfo` and `S_Procedural_RecoilItemInfo`.
 *
 * **THE RULING IS CLOSED** — founder, 8 Aug 2026, recorded as `animation.md` A.6. The source
 * component (`BPC_FPST_Procedural_Recoil`, 32 properties) drove both the weapon transform and the
 * camera. The test is *"does it move where the next bullet goes?"*:
 *
 *   · **weapon-transform recoil** — the gun kicking in the hands — changes what the player SEES
 *     and nothing about where the shot lands. Presentation. `FBRRecoilInfo`, computed by the
 *     AnimInstance, owned here.
 *   · **camera recoil** changes where the player is AIMING, therefore where their next shot goes.
 *     Gameplay, with a netcode surface. `FBRCameraRecoilInfo`, owned by **BP98's fire path**,
 *     declared here only because it was measured here.
 *
 * `contract_gap BP82-9` is closed by that ruling and is not re-litigated.
 */

/**
 * One kick, expressed as a MIN and MAX envelope rather than a single value.
 *
 * `S_Procedural_RecoilItemInfo`: `{min,max}_ForceLocation`, `{min,max}_ForceRotation`,
 * `{min,max}_ForceDuration`. The randomisation between the two is what stops a weapon feeling
 * like a metronome — and it is also why recoil must be **seeded and server-validated** rather
 * than rolled independently on each machine, or the client's crosshair and the server's cone
 * disagree by design. That is BP98's problem and it is a real one.
 */
USTRUCT(BlueprintType)
struct FBRRecoilImpulse
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breachpoint|Recoil")
	FVector MinForceLocation = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breachpoint|Recoil")
	FVector MaxForceLocation = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breachpoint|Recoil")
	FRotator MinForceRotation = FRotator::ZeroRotator;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breachpoint|Recoil")
	FRotator MaxForceRotation = FRotator::ZeroRotator;

	/** Source ADS envelope is 0.08 s min / 0.1 s max. (Shipped as 0.1/0.1 -- min was wrong.) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breachpoint|Recoil")
	float MinForceDuration = 0.08f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breachpoint|Recoil")
	float MaxForceDuration = 0.1f;
};

/**
 * A weapon's complete recoil profile. `S_Procedural_RecoilInfo`.
 *
 * Two envelopes, hip and ADS, because recoil is one of the few things a player judges a weapon by
 * and the two states are not a multiplier apart. Plus two spring settings, because the location
 * kick and the rotation kick settle at different rates — a gun that punches back and twists is
 * two motions, and one spring for both makes it read as a single lurch.
 */
USTRUCT(BlueprintType)
struct FBRRecoilInfo
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breachpoint|Recoil")
	FBRRecoilImpulse Default;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breachpoint|Recoil")
	FBRRecoilImpulse ADS;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breachpoint|Recoil")
	FBRSpringSetting ForceLocationSpring;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breachpoint|Recoil")
	FBRSpringSetting ForceRotationSpring;

	// Camera recoil is NOT here. See `FBRCameraRecoilInfo` below and `animation.md` A.6.
};

/**
 * Camera recoil — **BP98's, not this folder's.** Declared here only because it was measured here.
 *
 * `contract_gap BP82-9` is CLOSED by founder ruling, 8 Aug 2026, recorded as `animation.md` A.6.
 * The test the ruling applies is *"does it move where the next bullet goes?"* — weapon-transform
 * recoil does not and is presentation; camera recoil does, and is therefore a gameplay decision
 * with a netcode surface. Law 4: animation requests and presents, it never decides.
 *
 * **Why an AnimInstance could not do this correctly even if the law allowed it.** Recoil is
 * randomised between a min and a max envelope. Rolled independently on each machine, the client's
 * crosshair and the server's cone disagree **by construction** — that is arithmetic, not a bug.
 * Camera recoil has to be seeded and server-validated, and an AnimInstance has no prediction key
 * and no authority. The struct is inert data; whoever owns the fire path owns the behaviour.
 *
 * **BP98 may move this struct out of `FPS/` without consulting `animation.md`.** It is a handoff,
 * left in one piece so the measurements are not lost between packets.
 */
USTRUCT(BlueprintType)
struct FBRCameraRecoilInfo
{
	GENERATED_BODY()

	/**
	 * Kick over the burst, sampled by shot count so sustained fire climbs a DESIGNED path rather
	 * than accumulating randomly — `cameraRecoilFireCount` in the source.
	 *
	 * **SOFT**, per law 3: a hard curve reference on a per-weapon profile pulls every weapon's
	 * curve into memory the moment one row is touched.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breachpoint|Recoil")
	TSoftObjectPtr<UCurveFloat> CameraRecoilCurve;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breachpoint|Recoil")
	float CameraRecoilMultiply = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breachpoint|Recoil")
	float CameraRecoilADSMultiply = 1.f;

	/** How the view returns to centre after the kick. Source: `cameraSpring*_RotationVector`. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breachpoint|Recoil")
	FBRSpringSetting CameraReturnSpring;
};
