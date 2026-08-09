// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OSCommonUserWidget.h"
#include "OSAttributeWidget.generated.h"

struct FOnAttributeChangeData;
class UOSAttributeSet;
struct FOSDeathEventInfo;
class AOSPlayerState;
class AOSCharacter;
class UCommonTextBlock;
class UProgressBar;
/**
 * 
 */
UCLASS()
class ONSIGHT_API UOSAttributeWidget : public UOSCommonUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TObjectPtr<UCommonTextBlock> Name = nullptr;
	
	UPROPERTY()
	TObjectPtr<UProgressBar> HealthBar = nullptr;

	UPROPERTY()
	TObjectPtr<UProgressBar> ChaserHealthBar = nullptr;
	
	UPROPERTY()
	TObjectPtr<UProgressBar> StaminaBar = nullptr;

	UPROPERTY()
	TObjectPtr<UProgressBar> AuraBar = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attribute|Health")
	float ChaserLerpSpeed = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attribute|Health")
	float SmoothLerpSpeedIncrease = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attribute|Health")
	float ChaseDelay = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute|Health")
	FLinearColor HealthFullColor = FLinearColor(0.f, 1.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute|Health")
	FLinearColor HealthMidColor = FLinearColor(1.f, 1.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute|Health")
	FLinearColor HealthEmptyColor = FLinearColor(1.f, 0.5f, 0.f);

	UFUNCTION(Category = "Attribute|PlayerInfo")
	void SetPlayerName(const FText& InName);

	UFUNCTION(Category = "Attribute|Health")
	void OnHealthChanged(float Current, float Max, const FOSDeathEventInfo& DeathEvent);

	UFUNCTION(Category = "Attribute|Stamina")
	void OnStaminaChanged(float Current, float Max);

	UFUNCTION(Category = "Attribute|Aura")
	void OnAuraChanged(float Current, float Max);
	
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void OnGASReady_Implementation(UAbilitySystemComponent* ASC, UOSAttributeSet* AS) override;

	UFUNCTION(BlueprintCallable)
	void InitAttributeWidget(UCommonTextBlock* InName, UProgressBar* InHealthBar, UProgressBar* InChaser, UProgressBar* InStaminaBar, UProgressBar* InAuraBar);
	void FinalizeAttributeWidget();
	
private:
	float TargetChaserPercent = 1.f;
	float TargetHealthPercent = 1.f;
	float CurrentHealthPercent = 1.f;
	float TargetStaminaPercent = 1.f;
	bool bInitialized = false;
	bool bGASReady = false;
	bool bFriendly = false;
	FTimerHandle ChaserTimer;
	FTimerHandle ChaserDelayTimer;
	FTimerHandle HealthRegenTimer;
	FTimerHandle StaminaRegenTimer;

	void StartChaserTick();
	void ChaserTimerTick();
	void HealthRegenTimerTick();
	void StaminaRegenTimerTick();
};
