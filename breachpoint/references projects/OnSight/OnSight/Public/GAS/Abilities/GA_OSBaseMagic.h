// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
/* Base magic ability: shared montage-driven activation pattern for all magic abilities.
   Plays a cast montage, waits for DirectDamage anim notify, calls OnMagicEventReceived.
   Subclasses override OnMagicEventReceived to do their specific thing (spawn projectile, cone sweep, etc.).
   Designers can also set VFX cue tags and bone sockets directly on the ability. */

#include "CoreMinimal.h"
#include "GAS/Abilities/OSGameplayAbility.h"
#include "GA_OSBaseMagic.generated.h"

class UAnimMontage;
class UNiagaraComponent;

UCLASS(Abstract)
class ONSIGHT_API UGA_OSBaseMagic : public UOSGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_OSBaseMagic();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

	// --- Animation ---

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Magic|Animation")
	TObjectPtr<UAnimMontage> CastMontage;

	// --- Spell Origin ---

	/* Socket/bone on the caster mesh that this spell originates from.
	   Drives Activate VFX attach point, Cast VFX attach point, and projectile spawn location.
	   Leave empty to use the character's actor location + forward offset. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Magic|Origin")
	FName OriginSocket;

	/* Forward distance from the character used when OriginSocket is empty, OR added on top of the
	   socket position when bAddOffsetToSocket is true. Prevents VFX/projectiles from spawning inside
	   the capsule or inside a hand bone. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Magic|Origin")
	float OriginForwardOffset;

	/* When true, OriginForwardOffset is applied in front of OriginSocket (instead of only as the
	   no-socket fallback). Use this to push a projectile past the knuckles or nudge VFX forward
	   from a chest/hand socket. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Magic|Origin")
	bool bAddOffsetToSocket;

	// --- Activate VFX (persistent, Actor-based cues) ---

	/* Persistent VFX: added via AddGameplayCue, removed on ability end.
	   Use for effects that live for the cast duration (fire, shields, auras).
	   Must be an Actor-based cue (GameplayCueNotify_Actor). Do NOT use Static cues here.
	   By default, waits for the Event_DirectDamage anim notify before activating.
	   Set bActivateVFXImmediate = true to fire on ability start instead (charge-up/wind-up). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Magic|VFX")
	FGameplayTag ActivateVFXCueTag;

	/* When true, the persistent Activate VFX fires immediately when the ability starts (for charge-up/wind-up effects).
	   When false (default), waits for the Event_DirectDamage anim notify before activating (synced to animation). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Magic|VFX")
	bool bActivateVFXImmediate;

	/* When true, the cue actor is pre-spawned hidden at ability start and its Niagara components are deactivated.
	   On the anim notify, the actor is moved to the socket, unhidden, and the Niagara is activated fresh from the
	   new location. Use this for burst VFX that need to spawn AT a socket (not at the player root) so the initial
	   particle emission happens at the correct location.
	   Only applies when bActivateVFXImmediate is false (anim-synced path). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Magic|VFX")
	bool bPreSpawnHidden;

	// --- Cast VFX (one-shot, Static cues only) ---

	/* One-shot VFX: fired via ExecuteGameplayCue when the Event_DirectDamage anim notify triggers.
	   Use for instant burst effects (sparks, impact flashes).
	   Must be a Static cue (GameplayCueNotify_Static). Do NOT use Actor-based cues here: they won't clean up. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Magic|VFX")
	FGameplayTag CastVFXCueTag;

protected:
	/* Called on the server when the DirectDamage anim notify fires.
	   Override in C++ subclasses or Blueprint to implement the spell effect. */
	UFUNCTION(BlueprintNativeEvent, Category = "Magic")
	void OnMagicEventReceived(FGameplayEventData Payload);

	// --- Origin Resolution ---

	/* World transform for the spell origin. Rotation is always the character's actor quat (socket
	   bones can have baked rotations that would fire projectiles/VFX sideways). Location is the
	   socket position (if OriginSocket is set), plus OriginForwardOffset when bAddOffsetToSocket is
	   true or when the socket is missing. */
	FTransform ResolveOriginTransform() const;

	/* Convenience wrapper around ResolveOriginTransform().GetLocation() for VFX call sites. */
	FVector ResolveOriginLocation() const;

private:
	UFUNCTION()
	void OnCastEventReceived(FGameplayEventData Payload);

	UFUNCTION()
	void OnCastMontageFinished();

	// --- VFX Helpers ---

	void FireVFXCue(const FGameplayTag& CueTag, bool bPersistent);
	void RemoveVFXCue(const FGameplayTag& CueTag);

	void PreSpawnCueHidden(const FGameplayTag& CueTag);
	void RevealPreSpawnedCueAtSocket(AActor* Avatar, const FGameplayTag& CueTag) const;
	void MoveCueActorToSocket(AActor* Avatar, const FGameplayTag& CueTag) const;

	AActor* FindCueActorForTag(AActor* Avatar, const FGameplayTag& CueTag) const;
	void ResetNiagaraAtTransform(AActor* CueActor, const FVector& Location, const FQuat& Rotation) const;

	bool bActivateVFXActive;
};
