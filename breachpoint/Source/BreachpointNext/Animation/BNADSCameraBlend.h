#pragma once

#include "CoreMinimal.h"
#include "BNADSCameraBlend.generated.h"

class UCameraComponent;

/**
 * The ADS lens, in ONE place because two anim instances need it.
 *
 * RESEARCH-ADS §3 settled where this lives and why: the blend needs a per-frame interp, law 4
 * bans a gameplay Tick, and `NativeUpdateAnimation` is the project's established presentation
 * brain — the camera already rides the mesh, so the mesh's brain adjusting the lens is the same
 * ownership. What §3 did not anticipate is that BN ships TWO anim instances: `UBNAnimInstance`
 * (the production spine) and `UBNLAnimInstance` (the Lyra/MigrateLyra parent that ABP_Mannequin_Base
 * actually runs). They are SIBLINGS off UAnimInstance, not parent and child, so the FOV blend
 * written into one was simply absent from the other — and the pawn runs the other. Hence a struct
 * both own rather than a base-class method neither can inherit.
 *
 * The base FOV is captured from the camera on first sight, never hardcoded: a designer changing
 * the camera's FOV must not have it silently overwritten by a constant that predates the change.
 */
USTRUCT()
struct FBNADSCameraBlend
{
	GENERATED_BODY()

	/** The aimed field of view. The camera's own FOV is 90, so this IS the zoom factor:
	 *  90/ADSFOV. 80 was 1.125x — a change most players cannot see, which is why aiming
	 *  read as "the gun moved" rather than "I zoomed" (founder, 29 Aug).
	 *
	 *  60 is 1.5x: unmistakably a zoom, still an arena-rifle sight rather than a scope.
	 *  Lower is tighter — 55 is 1.64x, 50 is 1.8x and starts to feel like a marksman
	 *  optic on a weapon that does not have one. */
	UPROPERTY(EditDefaultsOnly, Category = "BN|Tuning")
	float ADSFOV = 60.f;

	/** The reference's measured AimPoseChangeSpeed. */
	UPROPERTY(EditDefaultsOnly, Category = "BN|Tuning")
	float InterpSpeed = 18.f;

	/**
	 * Blend the camera toward the ADS FOV, or back to the captured default.
	 *
	 * @param Camera            the owning character's first-person camera; null is a no-op
	 * @param bADS              the State.Weapon.ADS tag snapshot
	 * @param bOwnerFirstPerson locally controlled AND player controlled. The camera is owner-only
	 *                          cosmetics — sim proxies and bots get their ADS look from the pose,
	 *                          and zooming a lens nobody looks through is pure waste.
	 */
	void Update(UCameraComponent* Camera, bool bADS, bool bOwnerFirstPerson, float DeltaSeconds);

	/** Forget the captured base FOV, so the next Update re-reads it. */
	void Reset() { DefaultFOV = -1.f; }

private:
	/** The camera's own FOV, captured on first sight; negative = not yet captured. */
	float DefaultFOV = -1.f;
};
