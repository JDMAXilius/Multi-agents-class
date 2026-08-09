// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Engine/Engine.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "AN_Footstep.generated.h"

UENUM(BlueprintType)
enum class EFootstepType : uint8
{
	LeftFoot		UMETA(DisplayName = "Left Foot"),
	RightFoot		UMETA(DisplayName = "Right Foot")
};

USTRUCT(BlueprintType)
struct FFootstepSoundData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UPhysicalMaterial> PhysicalMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	USoundBase* Sound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float VolumeMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float PitchMultiplier = 1.0f;
};

/**
 * Anim Notify for playing footstep sounds based on surface type
 */
UCLASS()
class ZORANSRESISTANCE_API UAN_Footstep : public UAnimNotify
{
	GENERATED_BODY()

public:
	UAN_Footstep();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep")
	EFootstepType FootstepType = EFootstepType::LeftFoot;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep")
	FName FootSocketName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep")
	float TraceDistance = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep")
	TArray<FFootstepSoundData> SurfaceSounds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep")
	USoundBase* DefaultSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep")
	bool bAttachToMesh = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float VolumeMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float PitchMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep")
	bool bReplicateToOtherClients = true;

protected:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

private:
	USoundBase* GetSoundForSurface(UPhysicalMaterial* PhysicalMaterial) const;
	void PlayFootstepSound(USkeletalMeshComponent* MeshComp, USoundBase* Sound, const FVector& Location, const FRotator& Rotation, float InVolumeMultiplier = 1.0f, float InPitchMultiplier = 1.0f) const;
};

