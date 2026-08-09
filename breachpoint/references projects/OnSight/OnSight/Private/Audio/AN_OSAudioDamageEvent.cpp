// Fill out your copyright notice in the Description page of Project Settings.


#include "Audio/AN_OSAudioDamageEvent.h"
#include "AkAudioEvent.h"
#include "AkSwitchValue.h"
#include "AkComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Utilities/BlueprintLibrary/OSGASBlueprintLibrary.h"

UAN_OSAudioDamageEvent::UAN_OSAudioDamageEvent()
{
#if WITH_EDITORONLY_DATA
	NotifyColor = FColor(220, 60, 60);
#endif
}

void UAN_OSAudioDamageEvent::Notify(USkeletalMeshComponent* SkelMeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(SkelMeshComp, Animation, EventReference);
	
	if (!SkelMeshComp || !WwiseDamageEvent) return;
	
	AActor* Target = SkelMeshComp->GetOwner();
	if (!Target) return;
	
	UAkComponent* TargetAk = Target->FindComponentByClass<UAkComponent>();
	if (!TargetAk) return;
	
	const bool bIsBlocking = UOSGASBlueprintLibrary::HasGameplayTagByString(Target, BlockingStateTag.ToString());
	
	
	UAkSwitchValue* ChosenSwitch = bIsBlocking ? BlockedSwitch : UnblockedSwitch;
	if (ChosenSwitch)
	{
		TargetAk->SetSwitch(ChosenSwitch);
		UE_LOG(LogTemp, Warning, TEXT("DamageNotify: bIsBlocking=%d, ChosenSwitch=%s"), bIsBlocking, *GetNameSafe(ChosenSwitch));
	}
	
	TargetAk->PostAkEvent(WwiseDamageEvent, 0, FOnAkPostEventCallback());
}

FString UAN_OSAudioDamageEvent::GetNotifyName_Implementation() const
{
	return WwiseDamageEvent ? WwiseDamageEvent->GetName() : TEXT("WwiseDamageEvent");
}
