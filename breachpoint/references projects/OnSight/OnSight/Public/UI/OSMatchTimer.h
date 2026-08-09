#pragma once

#include "CoreMinimal.h"
#include "OSCommonUserWidget.h"
#include "OSMatchTimer.generated.h"

class AOSGameState;
class UCommonTextBlock;

UCLASS()
class ONSIGHT_API UOSMatchTimer : public UOSCommonUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void OnGameStateReady(AOSGameState* GS) override;
	
	UPROPERTY()
	TObjectPtr<UCommonTextBlock> MatchTimeText;

	UFUNCTION(BlueprintCallable)
	void InitMatchTimer(UCommonTextBlock* InMatchTimeText);
	void FinalizeMatchTimerWidget();
	
private:
	bool bInitialized = false;
	bool bGameStateReady = false;
	
	UFUNCTION()
	void UpdateMatchTime(int32 NewMatchTime);
};
