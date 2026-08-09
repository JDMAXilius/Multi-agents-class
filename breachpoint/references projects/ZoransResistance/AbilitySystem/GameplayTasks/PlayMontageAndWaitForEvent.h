// Copyright Zollpa LLC.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraSystem.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "PlayMontageAndWaitForEvent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPlayMontageAndWaitForEventDelegate, FGameplayTag, EventTag, FGameplayEventData, EventData);

USTRUCT(BlueprintType)
struct FMontageCosmeticData
{
	GENERATED_BODY()

	// The name of the AnimNotify to set the values in.
	UPROPERTY(BlueprintReadWrite)
	FName EventName = NAME_None;

	UPROPERTY(BlueprintReadWrite)
	FName MeshSocketName = NAME_None;

	// The NiagaraSystem that will play during the event.
	UPROPERTY(BlueprintReadWrite)
	UNiagaraSystem* NiagaraSystem = nullptr;

	UPROPERTY(BlueprintReadWrite)
	bool bAttachEffectToMesh = false;

	// The sound that will be played during this event.
	UPROPERTY(BlueprintReadWrite)
	USoundWave* SoundEffect = nullptr;

	UPROPERTY(BlueprintReadWrite)
	bool bAttachSoundToMesh = false;
};

// USTRUCT()
// struct FGameplayAbilityRepCosmeticAnimMontage : public FGameplayAbilityRepAnimMontage
// {
// 	GENERATED_USTRUCT_BODY()
//
// 	UPROPERTY()
// 	TArray<FMontageCosmeticData> CosmeticEventData;
// 	
// 	FGameplayAbilityRepCosmeticAnimMontage() {}
// 	
// 	//FGameplayAbilityRepCosmeticAnimMontage(const FGameplayAbilityRepAnimMontage& InRepData, const TArray<FMontageCosmeticData>& InCosmeticData)
// 	FGameplayAbilityRepCosmeticAnimMontage(const FGameplayAbilityRepAnimMontage& InRepData)
// 	{
// 		AnimMontage = InRepData.AnimMontage;
// 		PlayRate = InRepData.PlayRate;
// 		Position = InRepData.Position;
// 		BlendTime = InRepData.BlendTime;
// 		NextSectionID = InRepData.NextSectionID;
// 		PlayInstanceId = InRepData.PlayInstanceId;
// 		bRepPosition = InRepData.bRepPosition;
// 		IsStopped = InRepData.IsStopped;
// 		SkipPositionCorrection = InRepData.SkipPositionCorrection;
// 		bSkipPlayRate = InRepData.bSkipPlayRate;
// 		PredictionKey = InRepData.PredictionKey;
// 		SectionIdToPlay = InRepData.SectionIdToPlay;
// 		//CosmeticEventData = InCosmeticData;
// 	}
// };

UCLASS()
class ZORANSRESISTANCE_API UPlayMontageAndWaitForEvent : public UAbilityTask
{
	GENERATED_BODY()

public:

	// Constructor and overrides
	UPlayMontageAndWaitForEvent(const FObjectInitializer& ObjectInitializer);

	/**
	* The Blueprint node for this task, PlayMontageAndWaitForEvent, has some black magic from the plugin that automagically calls Activate()
	* inside of K2Node_LatentAbilityCall as stated in the AbilityTask.h. Ability logic written in C++ probably needs to call Activate() itself manually.
	*/
	virtual void Activate() override;
	virtual void ExternalCancel() override;
	virtual FString GetDebugString() const override;
	virtual void OnDestroy(bool AbilityEnded) override;

	/** The montage completely finished playing */
	UPROPERTY(BlueprintAssignable)
	FPlayMontageAndWaitForEventDelegate OnCompleted;

	/** The montage started blending out */
	UPROPERTY(BlueprintAssignable)
	FPlayMontageAndWaitForEventDelegate OnBlendOut;

	/** The montage was interrupted */
	UPROPERTY(BlueprintAssignable)
	FPlayMontageAndWaitForEventDelegate OnInterrupted;

	/** The ability task was explicitly cancelled by another ability */
	UPROPERTY(BlueprintAssignable)
	FPlayMontageAndWaitForEventDelegate OnCancelled;

	/** One of the triggering gameplay events happened */
	UPROPERTY(BlueprintAssignable)
	FPlayMontageAndWaitForEventDelegate EventReceived;

	/**
	 * Play a montage and wait for it end. If a gameplay event happens that matches EventTags (or EventTags is empty), the EventReceived delegate will fire with a tag and event data.
	 * If StopWhenAbilityEnds is true, this montage will be aborted if the ability ends normally. It is always stopped when the ability is explicitly cancelled.
	 * On normal execution, OnBlendOut is called when the montage is blending out, and OnCompleted when it is completely done playing
	 * OnInterrupted is called if another montage overwrites this, and OnCancelled is called if the ability or task is cancelled
	 *
	 * @param TaskInstanceName Set to override the name of this task, for later querying
	 * @param MontageToPlay The montage to play on the character
	 * @param EventTags Any gameplay events matching this tag will activate the EventReceived callback. If empty, all events will trigger callback
	 * @param Rate Change to play the montage faster or slower
	 * @param bStopWhenAbilityEnds If true, this montage will be aborted if the ability ends normally. It is always stopped when the ability is explicitly cancelled
	 * @param AnimRootMotionTranslationScale Change to modify size of root motion or set to 0 to block it entirely
	 */
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE", AutoCreateRefTerm = "CosmeticEvents"))
	static UPlayMontageAndWaitForEvent* PlayMontageAndWaitForEvent(
			UGameplayAbility* OwningAbility,
			FName TaskInstanceName,
			UAnimMontage* MontageToPlay,
			FGameplayTagContainer EventTags,
			//TArray<FMontageCosmeticData> CosmeticEvents,
			float Rate = 1.f,
			FName StartSection = NAME_None,
			bool bStopWhenAbilityEnds = false,
			float AnimRootMotionTranslationScale = 1.f,
			float StartTimeSeconds = 0.f);

protected:
	UPROPERTY()
	float StartTimeSeconds;

private:

	/** Montage that is playing */
	UPROPERTY()
	UAnimMontage* MontageToPlay;

	/** List of tags to match against gameplay events */
	UPROPERTY()
	FGameplayTagContainer EventTags;

	/** List of AnimNotify / AnimNotifyState parameters to set in this montage */
	//TArray<FMontageCosmeticData> CosmeticEvents;
	
	/** Playback rate */
	UPROPERTY()
	float Rate;

	/** Section to start montage from */
	UPROPERTY()
	FName StartSection;

	/** Modifies how root motion movement to apply */
	UPROPERTY()
	float AnimRootMotionTranslationScale;

	/** Rather montage should be aborted if ability ends */
	UPROPERTY()
	bool bStopWhenAbilityEnds;

	/** Checks if the ability is playing a montage and stops that montage, returns true if a montage was stopped, false if not. */
	bool StopPlayingMontage();

	/** Returns our ability system component */
	UAbilitySystemComponent* GetTargetASC();

	void OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted);
	void OnAbilityCancelled();
	void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	void OnGameplayEvent(FGameplayTag EventTag, const FGameplayEventData* Payload);

	FOnMontageBlendingOutStarted BlendingOutDelegate;
	FOnMontageEnded MontageEndedDelegate;
	FDelegateHandle CancelledHandle;
	FDelegateHandle EventHandle;
};