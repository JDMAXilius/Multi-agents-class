// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OSAnimNotify.h"
#include "OSAnimNotify_Footstep.generated.h"

UENUM(BlueprintType)
enum class EOSFootstepType : uint8
{
	LeftFoot		UMETA(DisplayName = "Left Foot"),
	RightFoot		UMETA(DisplayName = "Right Foot")
};

/**
 * Anim Notify for playing footstep sounds based on surface type
 */
UCLASS()
class ONSIGHT_API UOSAnimNotify_Footstep : public UOSAnimNotify
{
	GENERATED_BODY()

public:
	UOSAnimNotify_Footstep();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep")
	EOSFootstepType FootstepType = EOSFootstepType::LeftFoot;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep")
	FName FootSocketName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep")
	float TraceDistance = 50.0f;

protected:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};

