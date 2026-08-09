#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "GAS/Abilities/OSGameplayAbility.h"
#include "GA_OSGrab.generated.h"

class UAnimMontage;
class UGameplayEffect;
class AOSCharacter;
class UAbilityTask_PlayMontageAndWait;

/** Paired montage set for grab: attacker and victim montages that must play together. */
USTRUCT(BlueprintType)
struct FOSGrabMontageSet
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<UAnimMontage> AttackerMontage;

	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<UAnimMontage> VictimMontage;
};

/** Bidirectional socket pair: maps a grabber contact point to a victim contact point.
 *  Each pair registers TWO MW warp targets (one per actor) with computed reach offsets.
 *  Offset = distance from actor root to their socket in current pose (limb reach).
 *  Montages must contain MW notify windows with matching warp names. */
USTRUCT(BlueprintType)
struct FOSGrabSocketPair
{
	GENERATED_BODY()

	/** MW target name for the ATTACKER's warp (must match attacker montage MW notify). */
	UPROPERTY(EditDefaultsOnly, Category="Warp")
	FName AttackerWarpName;

	/** MW target name for the VICTIM's warp (must match victim montage MW notify). */
	UPROPERTY(EditDefaultsOnly, Category="Warp")
	FName VictimWarpName;

	/** Socket on the grabber's mesh — the grabber's contact point (e.g. hand_l). */
	UPROPERTY(EditDefaultsOnly, Category="Sockets")
	FName GrabberSocket;

	/** Socket on the victim's mesh — the victim's contact point (e.g. spine_03). */
	UPROPERTY(EditDefaultsOnly, Category="Sockets")
	FName VictimSocket;
};

/**
 * Command grab (Vicious Attack): server-authoritative grab with paired animation sync.
 *
 * Architecture — unified socket pair MW pipeline:
 *   1. Box trace finds victim
 *   2. GrabClaim GE prevents double-grab
 *   3. GrabSocketPairs define which body parts sync (bidirectional warp targets)
 *   4. Each pair computes offset from current pose: dist(root, socket) = limb reach
 *   5. Component-tracking MW warp targets injected on both attacker and victim
 *      (bFollowComponent = true: engine auto-tracks bone transforms per frame)
 *   6. Pawn collision disabled, GameplayEvent.GrabHit triggers GA_OSGrabReaction on victim
 *   7. Both montages play — MW notify windows warp actors to bone-tracked positions
 *   8. Teleport fallback for montages without MW windows (backwards compatible)
 *
 * Damage applied at visual impact frame via AN_OSDirectDamage notify + WaitGameplayEvent.
 * If montage is interrupted before the notify fires, no damage lands.
 */
UCLASS()
class ONSIGHT_API UGA_OSGrab : public UOSGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_OSGrab();

	const TArray<FOSGrabSocketPair>& GetGrabSocketPairs() const { return GrabSocketPairs; }

	/** Server-only. Restores victim collision/CMC, removes grab claim (by handle if valid, else by effect class),
	 *  cancels grab reaction, clears placement warp. Safe to call multiple times (e.g. EndAbility + EndPlay). */
	static void ServerRestoreGrabVictimAfterInterrupted(AOSCharacter* Victim,
		FActiveGameplayEffectHandle* OptClaimHandle,
		FName VictimPlacementWarpNameToClear);

	/** CDO warp name for orphan cleanup when the grab ability instance is gone (e.g. attacker EndPlay). */
	static FName GetDefaultVictimPlacementWarpName();

#if !UE_BUILD_SHIPPING
	AOSCharacter* GetGrabVictim() const { return GrabVictim.Get(); }
#endif

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

	virtual void OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

	// Paired montage set (attacker + victim). Victim montage sent via GameplayEvent.
	UPROPERTY(EditDefaultsOnly, Category="Grab")
	FOSGrabMontageSet DefaultGrabMontages;

	UPROPERTY(EditDefaultsOnly, Category="Grab", meta=(ClampMin="10", ClampMax="200"))
	float GrabTraceHalfExtent = 50.f;

	UPROPERTY(EditDefaultsOnly, Category="Grab", meta=(ClampMin="10", ClampMax="500"))
	float GrabTraceForwardDistance = 100.f;

	/** Montage section played on activation. Entry plays unconditionally; on confirm-trace hit,
	 *  the ability jumps to GrabSectionName. On miss, Entry plays through naturally as the whiff visual. */
	UPROPERTY(EditDefaultsOnly, Category="Grab|Montage Sections")
	FName EntrySectionName = TEXT("Entry");

	/** Montage section played after confirm-trace hit. Contains the paired-animation impact frame
	 *  with the AN_SendGameplayEvent notify firing GameplayEvent.DirectDamage for damage apply. */
	UPROPERTY(EditDefaultsOnly, Category="Grab|Montage Sections")
	FName GrabSectionName = TEXT("Grab");

	// MW warp target name for victim placement. If victim's montage has a matching
	// warp window, MW drives smooth positioning on top of the teleport fallback.
	// Phase 1 replicates this warp target via AOSCharacter::ReplicatedGrabVictimWarp —
	// producer is PrepareVictim on the server, consumer is OnRep_ReplicatedGrabVictimWarp
	// on owning client + simulated proxies.
	UPROPERTY(EditDefaultsOnly, Category="Grab|Motion Warping")
	FName VictimPlacementWarpName = "GrabPlacement";

	// --- Motion Warping: Socket Pairs ---
	// Each pair defines a bidirectional warp: attacker's GrabberSocket ↔ victim's VictimSocket.
	// Offset computed from current pose (dist from root to socket = limb reach).
	// First entry is conventionally the approach pair. If empty, teleport fallback is used.
	UPROPERTY(EditDefaultsOnly, Category="Grab|Motion Warping")
	TArray<FOSGrabSocketPair> GrabSocketPairs;

	// Base damage dealt on grab connect. Applied directly to victim via DamageEffectClass.
	UPROPERTY(EditDefaultsOnly, Category="Grab|Damage", meta=(ClampMin="0"))
	float GrabDamage = 40.f;

	// GE class used for grab damage. Defaults to GE_OSApplyDamage.
	UPROPERTY(EditDefaultsOnly, Category="Grab|Damage")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	// --- Replication priority elevation (Phase 0 primary fix) ---
	// Grab is a two-actor-coupled problem: paired poses need the two replication channels
	// (attacker + victim positions) to stay tight under multi-player load. Boost each pawn's
	// NetUpdateFrequency and NetPriority for grab duration, restore on end. Periodic
	// ForceNetUpdate compensates for the default cadence throttle.

	UPROPERTY(EditDefaultsOnly, Category="Grab|Network", meta=(ClampMin="30", ClampMax="120"))
	float GrabBoostedNetUpdateFrequency = 60.f;

	UPROPERTY(EditDefaultsOnly, Category="Grab|Network", meta=(ClampMin="1.0", ClampMax="10.0"))
	float GrabBoostedNetPriority = 3.f;

	UPROPERTY(EditDefaultsOnly, Category="Grab|Network", meta=(ClampMin="0.01", ClampMax="0.25"))
	float GrabForceNetUpdateInterval = 0.05f;

private:
	TWeakObjectPtr<AOSCharacter> GrabVictim;
	bool bDamageApplied = false;

	/** True once the Entry-section active-frame notify fired a successful confirm trace and the
	 *  hit-path (commit + claim GE + victim reaction + section jump + net-boost) has run.
	 *  Defense-in-depth guard against a duplicate active-frame notify re-entering the hit-path
	 *  (e.g. animator authoring mistake, or montage loop). Cannot be replaced by GrabVictim.IsValid()
	 *  since a victim disconnect invalidates the weak ptr while the ability is still executing. */
	bool bConfirmTraceSucceeded = false;

	/** Per-grab session identifier for cross-machine log correlation. Generated on ActivateAbility.
	 *  Same ID appears in every diag log line for this activation so post-playtest grep can
	 *  extract a single grab's full trace with:
	 *    grep "[GrabDiag]" Saved/Logs/*.log | grep "id=0x<ID>"
	 *  Per-machine uniqueness is sufficient — cross-machine correlation uses timestamps +
	 *  attacker/victim names + proximity in log time. */
	uint32 GrabSessionId = 0;

	// --- Net-priority elevation state (paired with Grab|Network UPROPERTYs above) ---
	// Defaults snapshotted on ActivateAbility (server only) so EndAbility restores exactly.
	// bNetBoostApplied gates the restore — true only when the boost actually fired, so a
	// trace-failure early-out or non-authority path doesn't spuriously restore.

	/** Cached attacker NetUpdateFrequency at grab start (server only). */
	float CachedAttackerNetUpdateFrequency = 0.f;

	/** Cached attacker NetPriority at grab start (server only). */
	float CachedAttackerNetPriority = 0.f;

	/** Cached victim NetUpdateFrequency at grab start (server only). */
	float CachedVictimNetUpdateFrequency = 0.f;

	/** Cached victim NetPriority at grab start (server only). */
	float CachedVictimNetPriority = 0.f;

	/** True while the server-side net-priority boost is active. Guards EndAbility restore. */
	bool bNetBoostApplied = false;

	/** Timer handle for the periodic ForceNetUpdate cycle on attacker+victim. */
	FTimerHandle GrabNetUpdateTimerHandle;

	/** Diagnostic counter — number of ForceNetUpdate ticks fired this grab. */
	int32 GrabForceNetUpdateTickCount = 0;

	FActiveGameplayEffectHandle GrabClaimHandle;

	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	void PrepareVictim(AOSCharacter* Attacker, AOSCharacter* Victim);
	void InjectAttackerWarps(AOSCharacter* Attacker, AOSCharacter* Victim);
	void CleanupAllWarpTargets();
	void ApplyGrabDamage();

	/** Called by WaitGameplayEvent when GameplayEvent.Grab.ActiveFrame fires from an
	 *  AN_SendGameplayEvent notify on the attacker montage's Entry section. Runs the confirm
	 *  trace; on hit, commits cost + applies claim GE + triggers victim reaction + elevates
	 *  net priority + jumps montage to GrabSectionName. On miss, logs and returns (Entry
	 *  plays through naturally as the whiff visual). */
	UFUNCTION()
	void OnGrabActiveFrameEvent(FGameplayEventData Payload);

	/** Called by WaitGameplayEvent when GameplayEvent.DirectDamage fires from an
	 *  AN_SendGameplayEvent (or AN_OSDirectDamage) notify on the attacker montage's Grab
	 *  section at the visual impact frame. Applies grab damage server-side (CRIT-1 gate). */
	UFUNCTION()
	void OnDirectDamageEvent(FGameplayEventData Payload);

	/** Fallback: if montage completes without the notify, apply damage on blend out. */
	UFUNCTION()
	void OnGrabMontageEnd();

	/** Auth-gated wrapper for montage interrupt/cancel callbacks. LocalPredicted means the
	 *  client ticks its own montage; a client-local interrupt (e.g. another ability kicks in
	 *  only on the client) would otherwise replicate a cancel and kill a valid server grab.
	 *  Only the server decides the grab is cancelled. */
	UFUNCTION()
	void AuthOnlyCancelAbility();
};
