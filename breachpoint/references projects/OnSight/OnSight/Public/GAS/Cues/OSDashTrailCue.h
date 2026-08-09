// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
/* Spawns a Niagara system oriented along a segment encoded in cue parameters.
   Used by FlameDash / Ice Dash / Wind Dash trail VFX.
   Cue params contract:
     Location      = trail start (world)
     Normal        = unit direction from start to end
     RawMagnitude  = segment length in cm
   End location is reconstructed as Location + Normal * RawMagnitude. */

#include "CoreMinimal.h"
#include "GAS/Cues/OSGameplayCueNotifyStatic.h"
#include "OSDashTrailCue.generated.h"

class UNiagaraSystem;

UCLASS(Abstract)
class ONSIGHT_API UOSDashTrailCue : public UOSGameplayCueNotifyStatic
{
	GENERATED_BODY()

public:
	UOSDashTrailCue();

	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;

protected:
	// Niagara system to spawn at the trail start, rotated to face the end location.
	UPROPERTY(EditDefaultsOnly, Category = "Trail")
	TObjectPtr<UNiagaraSystem> TrailVFX;

	/* When true, the Niagara is attached to MyTarget (the instigator) so ribbon emitters
	   naturally trail behind the moving actor. Use this for "flame trail behind me while I dash".
	   When false, the Niagara is spawned at the world Location and stays there (stretch-beam style). */
	UPROPERTY(EditDefaultsOnly, Category = "Trail")
	bool bAttachToTarget;

	/* Socket/bone on MyTarget's mesh to attach to. Only used when bAttachToTarget is true.
	   Leave NAME_None to attach to root. */
	UPROPERTY(EditDefaultsOnly, Category = "Trail", meta=(EditCondition="bAttachToTarget"))
	FName AttachSocketName;

	/* Optional Niagara Vector user-parameter that receives the trail end location.
	   Stretch-beam use only: leave NAME_None for ribbon-follow / attached designs (the ribbon
	   emerges from emitter motion and doesn't need a precomputed end). */
	UPROPERTY(EditDefaultsOnly, Category = "Trail|Stretch-Beam Params")
	FName EndLocationParamName;

	/* Optional Niagara Float user-parameter that receives the trail length in cm.
	   Stretch-beam use only: see EndLocationParamName note. */
	UPROPERTY(EditDefaultsOnly, Category = "Trail|Stretch-Beam Params")
	FName LengthParamName;
};
