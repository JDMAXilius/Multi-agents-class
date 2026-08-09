// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OSAnimNotify.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "FX/Data/OSFXTypes.h"
#include "OSAnimNotify_WwiseEvent.generated.h"

class UAkAudioEvent;

/**
 * Plays a Wwise event when the notify fires.
 * Use for attack swing/weapon whoosh (separate from impact cue).
 *
 * Multiplayer/GAS purity: cosmetic-only. Does not apply gameplay effects, tags, or attributes.
 * Performance: optional distance cull + per-owner rate limit.
 */

UCLASS(meta=(DisplayName="OS Wwise Event"))
class ONSIGHT_API UOSAnimNotify_WwiseEvent : public UOSAnimNotify
{
	GENERATED_BODY()

public:
	/** Wwise event to post */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wwise")
	FOSWwiseEventPair AkEventPair;

	/** If true, only the locally controlled pawn will play this sound (recommended for swing). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wwise")
	EOSFXPlayerContext PlayerContext = EOSFXPlayerContext::AUTO;

	/** If set, post at this socket location on the mesh; otherwise post at owner location. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wwise")
	FName SocketName = NAME_None;

	/** Skip on dedicated servers (recommended). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Performance")
	bool bSkipOnDedicatedServer = true;

	/** Cull the sound if farther than this from nearest local viewpoint. 0 = no culling. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Performance", meta=(ClampMin="0.0"))
	float MaxDistance = 0.0f;

	/** Per-owner rate limit. 0 = no rate limit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Performance", meta=(ClampMin="0.0"))
	float MinIntervalPerOwner = 0.0f;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	
private:
	UAkAudioEvent* ResolveEvent(APawn* Owner);
};

