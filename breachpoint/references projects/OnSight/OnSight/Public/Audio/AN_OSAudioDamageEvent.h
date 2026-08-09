// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"
#include "AN_OSAudioDamageEvent.generated.h"

/** Custom anim notify for Wwise Damage Events - decoupling from the damage pipeline
 * 
 */

class UAkAudioEvent;
class UAkComponent;
class UAkSwitchValue;

UCLASS(meta = (DisplayName = "OSWwiseDamageEvent"))
class ONSIGHT_API UAN_OSAudioDamageEvent : public UAnimNotify
{
	GENERATED_BODY()
	
public:
	UAN_OSAudioDamageEvent();
	
	virtual void Notify(USkeletalMeshComponent* SkelMeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;
	
protected:
	//Wwise event to post on the target's AkComp
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wwise|Damage")
	TObjectPtr<UAkAudioEvent> WwiseDamageEvent;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wwise|Damage")
	FName ImpactSocket = NAME_None;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wwise|Damage")
	FGameplayTag SeverityTag;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wwise|Damage|Block")
	FGameplayTag BlockingStateTag;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wwise|Damage|Switch")
	TObjectPtr<UAkSwitchValue> BlockedSwitch;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wwise|Damage|Switch")
	TObjectPtr<UAkSwitchValue> UnblockedSwitch;
	
	
};
