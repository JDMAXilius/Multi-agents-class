#pragma once

#include "CoreMinimal.h"

#include "FPS/BRProceduralTypes.h"

#include "BRRecoilTypes.generated.h"

class UCurveFloat;

/**
 * Recoil shapes, from `S_Procedural_RecoilInfo` and `S_Procedural_RecoilItemInfo`.
 *
 * **A RULING IS OWED BEFORE ANY OF THIS RUNS, and it is not this packet's to make.** The source
 * component (`BPC_FPST_Procedural_Recoil`) drives the **camera**: `cameraRecoil_Multiply`,
 * `cameraReturnRotationVector`, `cameraSpringStiffness_RotationVector`,
 * `cameraRecoilFireCount`. Camera kick moves where the player is AIMING, which means it changes
 * where their next shot goes — that is a gameplay decision with a netcode surface, and
 * `animation.md` law 4 is explicit that animation *requests and presents, it never decides*.
 *
 * So the split this file assumes, subject to that ruling:
 *   · **weapon-transform recoil** — the gun kicking in the hands — is presentation, and the
 *     AnimInstance may compute it;
 *   · **camera recoil** is the fire ability's, applied server-authoritatively on the owning
 *     client, and belongs to **BP98**. These shapes are declared here because they were measured
 *     here, not because this folder should own the behaviour.
 *
 * Filed as `contract_gap BP82-9`.
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

	/** Source default for the ADS envelope is 0.1 s. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breachpoint|Recoil")
	float MinForceDuration = 0.1f;

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

	/**
	 * Camera kick over the burst, sampled by shot count. **SOFT** — law 3 admits no hard asset
	 * reference in C++, and a curve on a weapon profile would otherwise pull every weapon's
	 * curve into memory with the table.
	 *
	 * Sampled by `cameraRecoilFireCount` in the source, so sustained fire climbs a designed path
	 * rather than accumulating randomly. This is the field the BP82-9 ruling is really about.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breachpoint|Recoil")
	TSoftObjectPtr<UCurveFloat> CameraRecoilCurve;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breachpoint|Recoil")
	float CameraRecoilADSMultiply = 1.f;
};
