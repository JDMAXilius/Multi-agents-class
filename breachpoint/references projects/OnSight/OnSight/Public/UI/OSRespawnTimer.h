#pragma once

#include "CoreMinimal.h"
#include "UI/OSCommonUserWidget.h"
#include "OSRespawnTimer.generated.h"

class UAkAudioEvent;
class UHorizontalBox;
class UCommonTextBlock;

UCLASS()
class ONSIGHT_API UOSRespawnTimer : public UOSCommonUserWidget
{
	GENERATED_BODY()
public:
	
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void OnGASReady_Implementation(UAbilitySystemComponent* ASC, UOSAttributeSet* AS) override;
	
	UPROPERTY()
	TObjectPtr<UHorizontalBox> RespawnHorizontalBox;
	
	UPROPERTY()
	TObjectPtr<UCommonTextBlock> RespawnCountdownText;

	UFUNCTION(BlueprintCallable)
	void InitRespawnWidget(UHorizontalBox* InRespawnHorizontalBox, UCommonTextBlock* InRespawnText);
	void FinalizeRespawnWidget();

	UFUNCTION(BlueprintNativeEvent, Category="Respawn")
	void OnRespawnCountdownBegan(float TimeRemaining);

	UFUNCTION(BlueprintNativeEvent, Category="Respawn")
	void OnRespawnCountdownEnd();

	UFUNCTION(BlueprintNativeEvent, Category="Respawn")
	void OnRespawnCountdownTick(float TimeRemaining);

	UFUNCTION(BlueprintNativeEvent, Category="Respawn")
	void OnRespawnSecondChanged(int32 SecondsRemaining);
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Respawn|Audio")
	TObjectPtr<UAkAudioEvent> TickSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Respawn|Audio")
	TObjectPtr<UAkAudioEvent> CrescendoSound;
	
private:
	bool bInitialized = false;
	bool bGameStateReady = false;
	float LocalRespawnTimeRemaining = -1.f;
	
	FTimerHandle RespawnCountdownTickHandle;
	
	UFUNCTION()
	void OnRespawnCountdownChanged(float TimeRemaining);
	void TickRespawnCountdown();
	void UpdateRespawnCountdownDisplay();
	void StartRespawnCountdownTick();
	void StopRespawnCountdownTick();
};
