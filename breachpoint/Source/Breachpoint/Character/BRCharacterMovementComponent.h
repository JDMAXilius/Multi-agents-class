// Breachpoint. The CMC subclass: sprint intent, the saved move that replays it, the speed rule.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "BRCharacterMovementComponent.generated.h"

/**
 * FSavedMove_BR — one frame of Breachpoint intent, replayable.
 *
 * =========================================================================================
 * RULING (BP02, 1 Aug 2026) — COMPRESSED FLAGS, not the structured move-data container.
 * =========================================================================================
 * The `cmc-prediction` skill left this open and asked the first packet that needed it to decide,
 * on the grounds that "switching later touches every movement state at once". That premise is
 * false in the direction we care about, and that is why the decision is cheap:
 *
 *   `FCharacterNetworkMoveData` — the structured path — CARRIES `CompressedMoveFlags` as one of
 *   its own fields. Adopting the container later is ADDITIVE: every boolean already riding a
 *   custom bit keeps riding it, untouched, and the container adds room for typed fields beside
 *   them. The painful migration is the other direction (typed fields down into 4 bits), and
 *   nobody performs that one.
 *
 * So: boolean movement INTENT rides the custom bits, starting now. The container arrives, in its
 * own packet, the first time a movement state needs a field that is not a boolean — BP06's
 * grapple is the likely trigger (a target point or a surface class will not fit in a bit) — and
 * that packet does not have to touch sprint.
 *
 * THE BIT BUDGET, recorded here because there are only four and running out silently is the
 * failure mode:
 *   FLAG_Custom_0  sprint   (BP02, this file)
 *   FLAG_Custom_1  RESERVED for grapple (BP06) — reserved, not used; do not take it
 *   FLAG_Custom_2  free
 *   FLAG_Custom_3  free
 *
 * ALL FOUR HOOKS ARE IMPLEMENTED BELOW and none is optional. Skipping `Clear()` reuses a pooled
 * move with a stale flag (a phantom sprint three moves later); skipping `CanCombineWith` lets two
 * moves that disagree merge and a sprint frame vanish on correction. Both bugs only appear under
 * loss, which is to say: not in the editor.
 */
struct FSavedMove_BR : public FSavedMove_Character
{
	using Super = FSavedMove_Character;

	/** Sprint intent as it was at the START of this move. Set by BRGA_Sprint, replayed from here. */
	uint8 bSavedWantsToSprint : 1;

	FSavedMove_BR()
		: bSavedWantsToSprint(0)
	{
	}

	/** Moves are POOLED. Every custom field resets here or it leaks into an unrelated frame. */
	virtual void Clear() override;

	/** Capture the component's current intent into this move (client, before the move simulates). */
	virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, class FNetworkPredictionData_Client_Character& ClientData) override;

	/** Push this move's intent back into the component before a replay. */
	virtual void PrepMoveFor(ACharacter* C) override;

	/** Refuse to combine moves whose intent differs — a merged disagreement is a lost frame. */
	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override;

	/** Pack the intent into the custom bits for the wire. */
	virtual uint8 GetCompressedFlags() const override;
};

/** The factory that hands out FSavedMove_BR instead of FSavedMove_Character. Nothing else. */
class FNetworkPredictionData_Client_BR : public FNetworkPredictionData_Client_Character
{
public:
	using Super = FNetworkPredictionData_Client_Character;

	explicit FNetworkPredictionData_Client_BR(const UCharacterMovementComponent& ClientMovement)
		: Super(ClientMovement)
	{
	}

	virtual FSavedMovePtr AllocateNewMove() override;
};

/**
 * UBRCharacterMovementComponent — our subclass of the engine CMC (§3.4, "subclass, NOT from
 * scratch": rewriting CMC means rewriting the most battle-tested networked prediction code in the
 * engine, and "100% our gameplay code" does not extend to re-implementing engine subsystems).
 *
 * WHAT IT OWNS TODAY: the sprint half of `BRGA_Sprint` — the intent flag, the saved move that
 * replays it, and the speed rule that consumes it. The DECISION to sprint is the ability's
 * (gas-purity.md's movement exception: GAS decides, CMC moves). Grapple's root-motion source is
 * BP06's and is deliberately absent.
 *
 * THE SPEED MULTIPLIER IS DATA. `GetMaxSpeed()` multiplies the engine's answer by
 * `CT_Combat["Movement.Sprint.SpeedMultiplier"]`. No number appears in this file, and a balance
 * change to sprint is a one-cell CSV diff.
 *
 * ----------------------------------------------------------------------------------------------
 * NAMED, NOT HIDDEN — the trust gap in the sprint bit.
 *
 * On the server, `UpdateFromCompressedFlags` accepts the client's sprint bit as intent, and
 * `GetMaxSpeed` acts on it without asking whether the client actually has an active sprint
 * ability. A modified client can therefore assert the bit and move at sprint speed without ever
 * activating `BRGA_Sprint`. This is the standard UE arrangement (`bWantsToCrouch` has exactly the
 * same property) and it is a real, if bounded, exploit: a flat movement-speed multiplier.
 *
 * The obvious closure — require `State.Movement.Sprinting` on the owner before applying the
 * multiplier — was tried on paper and REJECTED here, not overlooked: the ability-activation RPC
 * (PlayerState channel) and the ServerMove (Character channel) are not ordered relative to each
 * other, so the server would routinely process a sprint move before the activation that
 * authorises it and correct the client at the start of every sprint. Trading a guaranteed hitch on
 * every honest press for a cheat that costs a cheater a modified binary is the wrong trade to make
 * silently, so it is made loudly, here, and routed to netcode-builder: the gate belongs in
 * `IsSprintIntentValid()` below, which exists precisely so the closure is one function.
 */
UCLASS(meta = (DisplayName = "BR Character Movement Component"))
class BREACHPOINT_API UBRCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UBRCharacterMovementComponent(const FObjectInitializer& ObjectInitializer);

	/**
	 * Set (or clear) sprint intent. THE ONLY caller is `UBRGA_Sprint` — on activation and on every
	 * end path. Not a gameplay decision: by the time this is called the decision is made, blocked
	 * tags were checked, and the ability is running.
	 */
	void SetSprintIntent(bool bNewWantsToSprint);

	/** Raw intent, as carried by the saved move. */
	bool WantsToSprint() const { return bWantsToSprint; }

	/**
	 * Does the current sprint intent actually earn the multiplier?
	 *
	 * Today: intent + on the ground. THE SEAM for the server-side trust gate described in the class
	 * comment — one function, one place, no movement state moves when it changes.
	 */
	virtual bool IsSprintIntentValid() const;

	/** Walking speed x the CT_Combat sprint multiplier while sprint intent is valid. */
	virtual float GetMaxSpeed() const override;

	/** Unpack the custom bits on the server / on replay. Symmetric with GetCompressedFlags(). */
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;

	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;

protected:
	/**
	 * Sprint intent for the move currently being simulated.
	 *
	 * NOT gameplay state and never read as such: the replicated state is
	 * `State.Movement.Sprinting`, granted by the ability. This is the per-move input bit that makes
	 * the ability's decision survive a correction, and it is exactly as authoritative as
	 * `bWantsToCrouch` — which is to say, it is the client's claim about its own intent.
	 */
	uint8 bWantsToSprint : 1;

private:
	/**
	 * The sprint speed multiplier from `CT_Combat`, resolved once per process.
	 *
	 * Cached because `GetMaxSpeed()` is called several times per simulated frame per character and
	 * a curve evaluation per call is waste, not because the value is expected to change: it cannot,
	 * within a process, and caching a value that could change would be a determinism bug. A missing
	 * curve resolves to 1.0 (no sprint) and says so once, loudly — walking when you pressed sprint
	 * is a visible failure; guessing a multiplier would not be.
	 */
	float GetSprintSpeedMultiplier() const;

	mutable float CachedSprintSpeedMultiplier = -1.f;

	friend struct FSavedMove_BR;
};
