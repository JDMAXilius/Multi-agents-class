// Breachpoint. The CMC subclass. Registered and empty — sprint and grapple arrive later.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "BRCharacterMovementComponent.generated.h"

/**
 * UBRCharacterMovementComponent — our subclass of the engine CMC.
 *
 * Specification: BREACHPOINT-ARCHITECTURE.md §3.4, "subclass, NOT from scratch". Rewriting CMC
 * means rewriting the most battle-tested networked prediction code in the engine (saved moves,
 * corrections, smoothing); "100% our gameplay code" does not include re-implementing engine
 * subsystems.
 *
 * WHAT THIS FILE IS TODAY: nothing but the type, registered as ACharacter's movement component
 * by ABRCharacter's constructor (FObjectInitializer::SetDefaultSubobjectClass). That is the
 * whole of BP01 step 4's deliverable, and the emptiness is deliberate — the type must exist and
 * be the one actually instantiated BEFORE anything derives behaviour from it, because swapping
 * a pawn's movement component class later invalidates every saved-move assumption already built
 * on the old one.
 *
 * WHAT IT WILL OWN (§3.4 — named here so the next packet does not have to re-derive it, and so
 * nobody adds any of it under a different ticket):
 *   - FSavedMove_BR + FNetworkPredictionData_Client_BR in these same files: compressed flags for
 *     sprint/grapple so our state replays correctly through CMC's prediction/reconciliation.
 *   - Sprint: apply the CT_Combat speed multiplier while State.Movement.Sprinting is present
 *     (the tag is set by BRGA_Sprint and carried in the saved-move flag so corrections replay
 *     it). Owner: BP02.
 *   - Grapple: the root-motion source for the pull, and the detach rules (arrival, jump-cancel).
 *     The *decision* to grapple stays in BRGA_Grapple. Owner: BP06, critic REFUTER gate.
 *
 * Both of those are authority/prediction work and are explicitly NOT this packet's to guess at.
 *
 * Halo-feel movement numbers (walk/air speed, air control, jump velocity, gravity scale) are
 * §3.4 "config on CMC defaults, not code" — this constructor sets NONE of them. A number typed
 * here would be a law-3 violation waiting to be found; tuning-curator proposes them.
 */
UCLASS(meta = (DisplayName = "BR Character Movement Component"))
class BREACHPOINT_API UBRCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UBRCharacterMovementComponent(const FObjectInitializer& ObjectInitializer);
};
