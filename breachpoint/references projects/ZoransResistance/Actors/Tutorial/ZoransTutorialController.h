// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "ZoransResistance/Game/Data/ZoransTutorialData.h"

#include "ZoransTutorialController.generated.h"

class AZoransTutorialBotSpawner;
class AZoransPlayerState;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FEndTutorial);

UCLASS()
class ZORANSRESISTANCE_API AZoransTutorialController : public AActor
{
	GENERATED_BODY()

public:
	AZoransTutorialController();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>&) const override;

	virtual void PostRepNotifies() override;

	void AddTutorialBotSpawner(const AZoransTutorialBotSpawner* Spawner) const;

	UPROPERTY(BlueprintAssignable)
	FEndTutorial EndTutorial;

protected:
	UPROPERTY(BlueprintReadWrite)
	AZoransPlayerState* PlayerState;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing="OnRep_CurrentLessonName")
	FName CurrentLessonName;
	
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing="OnRep_LessonProgress")
	TArray<int32> LessonProgress;

	UPROPERTY(BlueprintReadOnly)
	FTutorialLesson CurrentLesson;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	void SetActiveLesson(const FName LessonNameIn);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	void UpdateObjectiveProgress(int32 Index, int32 Progress);

	UFUNCTION(BlueprintCallable)
	void TryUpdateProgress(const ETutorialObjectiveType ObjectiveType, int32 Progress);
	
	UFUNCTION(BlueprintImplementableEvent)
	void OnLessonReplicated();

	UFUNCTION(BlueprintImplementableEvent)
	void OnProgressReplicated();
	
	UFUNCTION(BlueprintImplementableEvent)
	void GetLessonFromName(const FName LessonNameIn, FTutorialLesson& LessonOut);

private:
	bool bLessonNameChanged = false;
	bool bLessonProgressChanged = false;

	bool bIsPendingNextLesson = false;

	FTimerHandle LessonChangeTimerHandle;

	UFUNCTION()
	void BeginNextLesson();
	
	UFUNCTION()
	void OnRep_CurrentLessonName() { bLessonNameChanged = true; }

	UFUNCTION()
	void OnRep_LessonProgress() { bLessonProgressChanged = true; }
};
