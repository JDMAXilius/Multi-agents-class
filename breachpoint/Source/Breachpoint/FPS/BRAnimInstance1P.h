#pragma once

#include "CoreMinimal.h"

#include "FPS/BRAnimInstance.h"

#include "BRAnimInstance1P.generated.h"

/**
 * The first-person arms spine. `animation.md` law 2: one AnimInstance per mesh, shared C++ base.
 *
 * WHY IT IS THIN, AND WHY THAT IS THE POINT. Nearly everything the arms need is what the body
 * needs — velocity, cardinals, ADS, sway, the montage seam — and all of it is in `UBRAnimInstance`
 * already. The template's own answer to 1P-vs-3P was `bFPSMode` / `bFPSWalkMode`, two booleans
 * branched on inside ONE instance shared by both meshes (inventory, `ABP_Mannequin_Base`). That
 * is a mode flag standing in for a type, and it means every field is reachable from a graph that
 * has no business reading it.
 *
 * Splitting by CLASS instead of by flag means the difference is in the type system: the 1P ABP
 * cannot accidentally read a third-person-only field, because the class it is built on does not
 * have one. **The flags are deliberately not ported.**
 *
 * WHAT ACTUALLY DIFFERS, and it is a short list — which is the evidence the split is honest
 * rather than symmetry for its own sake:
 *   · the arms are always the owning client's, so `IsGameplayEventSource()` normally answers
 *     false here and this instance presents without ever raising events;
 *   · no aim offset. The camera IS the aim, so pitching the arms by `AimPitch` would double the
 *     rotation the camera already applied;
 *   · sway is felt, not observed, so it reads a separate scale.
 */
UCLASS(Config = Game)
class BREACHPOINT_API UBRAnimInstance1P : public UBRAnimInstance
{
	GENERATED_BODY()

public:

	UBRAnimInstance1P();

protected:

	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;

	/**
	 * Extra sway multiplier for the arms only.
	 *
	 * First-person sway is the primary feedback that the weapon has weight, and third-person sway
	 * is a detail nobody looks at. One shared number would be tuned for whichever view was open
	 * when someone tuned it.
	 */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Breachpoint|Tuning")
	float FirstPersonSwayScale = 1.35f;
};
