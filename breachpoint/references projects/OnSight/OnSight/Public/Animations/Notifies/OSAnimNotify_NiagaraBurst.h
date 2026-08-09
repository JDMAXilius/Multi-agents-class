#pragma once

#include "CoreMinimal.h"
#include "OSAnimNotify.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "OSAnimNotify_NiagaraBurst.generated.h"

class UNiagaraSystem;

/**
 * Spawns a Niagara system when the notify fires.
 * Intended for montage-authored attack VFX (swing trails, flashes, etc.).
 *
 * Multiplayer/GAS purity: cosmetic-only. Does not apply gameplay effects, tags, or attributes.
 */
UCLASS(meta=(DisplayName="OS Niagara Burst"))
class ONSIGHT_API UOSAnimNotify_NiagaraBurst : public UOSAnimNotify
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Niagara")
	TObjectPtr<UNiagaraSystem> System = nullptr;

	/** If set, spawn at this socket on the mesh; otherwise spawn at owner location. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Niagara")
	FName SocketName = NAME_None;

	/** If true, attach to the mesh (at SocketName if provided). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Niagara")
	bool bAttachToMesh = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Niagara")
	FVector LocationOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Niagara")
	FRotator RotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Niagara", meta=(ClampMin="0.01"))
	float Scale = 1.0f;

	/** If true, only the locally controlled pawn will play this VFX (useful for first-person/attacker-only). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Performance")
	bool bLocalOnly = false;

	/** Skip on dedicated servers (recommended). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Performance")
	bool bSkipOnDedicatedServer = true;

	/** Cull the VFX if farther than this from nearest local viewpoint. 0 = no culling. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Performance", meta=(ClampMin="0.0"))
	float MaxDistance = 0.0f;

	/** Per-owner rate limit. 0 = no rate limit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Performance", meta=(ClampMin="0.0"))
	float MinIntervalPerOwner = 0.0f;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};

