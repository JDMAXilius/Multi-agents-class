#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/BNGameplayAbility.h"
#include "Animation/AnimInstance.h"
#include "Engine/TimerHandle.h"
#include "BNGA_Melee.generated.h"

struct FGameplayAbilityTargetDataHandle;

/**
 * Melee — hitting someone with the gun. LocalPredicted, input tag Input.Melee.
 *
 * Granted by the PLAYER STATE, not by the weapon's ability set, for the reason sprint and lean are:
 * the swing belongs to the body and must survive every weapon swap. Granting it per weapon would
 * also make it unreachable until both DA_BNAbilitySet assets are edited, and would leave a row
 * with a null AbilitySet unable to melee at all. It still reads its montage, damage and reach from
 * the CURRENT weapon's row — so swinging while genuinely unarmed is a no-op today rather than a
 * punch, which is honest until an unarmed row exists.
 *
 * The swing is NOT the input event. `MyCharacter::MeleeAttack` (.cpp:1916-1930) plays the montage
 * and then does nothing until `HandleMontageNotifyBegin` sees the notify named `AN_FPST_Melee`
 * (.cpp:1267-1276) — the connect lands in the animation's own window, not on the key press. That
 * is the behaviour this reproduces, because a melee that damages on press hits before the arm has
 * moved and reads as a lag exploit to everyone watching.
 *
 * Roles are Fire's, deliberately: the LOCALLY-CONTROLLED machine owns the swing's timing and sends
 * one claim; the AUTHORITY re-traces from its own view and is the only judge. Damage goes through
 * BNDamage — the one door — so there is no second path here either.
 */
UCLASS(Config = Game)
class BREACHPOINTNEXT_API UBNGA_Melee : public UBNGameplayAbility
{
	GENERATED_BODY()

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** The montage's own connect window. Only the notify with ConnectNotifyName is ours. */
	UFUNCTION()
	void OnMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& Payload);

	/** Server side: the claim arrives here and is judged, never trusted. */
	UFUNCTION()
	void OnTargetDataReceived(const FGameplayAbilityTargetDataHandle& TargetData);

	/** Local machine only: trace the swing and send the one claim it produces. */
	void SwingTrace();

	/** The notify the template actually ships. Config so a re-authored montage renames it in ini
	 *  rather than in code — the record notes AN_FPST_Melee is the ONE melee notify that exists. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Melee")
	FName ConnectNotifyName = TEXT("AN_FPST_Melee");

	/** Cosmetic backstop. The swing ends with the montage; without a montage this is how long the
	 *  ability lives, and with one it is the timeout that stops a montage whose notify never fires
	 *  from holding the ability open forever. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Melee")
	float SwingDuration = 1.f;

	/** THE SAFETY NET, and it is not optional. `OnPlayMontageNotifyBegin` fires only for notifies
	 *  of the "Play Montage Notify" type — a plain UAnimNotify subclass, which is what an
	 *  `AN_`-prefixed asset usually is, does NOT raise it. If `AN_FPST_Melee` turns out to be the
	 *  latter, the binding never fires and melee would swing the weapon and damage nothing, silently.
	 *  So the connect also has a clock: if the notify has not landed by here, swing anyway. bSwung
	 *  means whichever arrives first wins and the second is a no-op, so a real notify still owns
	 *  the timing when it exists. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Melee")
	float FallbackConnectTime = 0.25f;

	/** The authority's own way out. Fire can leave its remote instance running because the WEAPON's
	 *  ability set is revoked on pawn destruction and clearing the spec cancels it; melee is granted
	 *  by the PlayerState and has no such backstop, so any path that loses the client's EndAbility
	 *  would leave the spec Active forever — and AbilityInputTagPressed only activates when
	 *  !Spec.IsActive(), so melee would be dead for that player for the rest of the match.
	 *  Generous on purpose: a claim arrives within ~1 RTT of the swing, so seconds cannot cut off
	 *  an honest one. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Melee")
	float AuthorityTimeout = 5.f;

	FTimerHandle SwingTimer;
	FTimerHandle ConnectTimer;

	/** One connect per activation: a montage carrying two notifies must not swing twice. */
	bool bSwung = false;

	/** Bound only on the machine that owns the timing, and only while this ability runs. */
	TWeakObjectPtr<UAnimInstance> BoundAnimInstance;
};
