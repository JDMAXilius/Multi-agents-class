#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "OSNameplateWidget.generated.h"

class UOSAttributeSet;
class AOSPlayerState;
class AOSCharacter;
class UBorder;
class UCommonTextBlock;
class UProgressBar;

// Base class for the world-space nameplate widget above characters.
UCLASS()
class ONSIGHT_API UOSNameplateWidget : public UCommonUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintCallable)
	void InitNameplate(UBorder* InTeamBG, UCommonTextBlock* InName, UProgressBar* InHealthBar, UProgressBar* InChaser, UCommonTextBlock* InCurrentHealth, UCommonTextBlock* InTotalHealth);
	

	UFUNCTION()
	void UpdateNameplateHealth(float Health, float MaxHealth, const FOSDeathEventInfo& DeathEvent);

	UFUNCTION()
	void UpdateNameplateColor(AOSPlayerState* InPlayerState);
	
public:
	UPROPERTY()
	TObjectPtr<UBorder> Team_BG = nullptr;

	UPROPERTY()
	TObjectPtr<UCommonTextBlock> Name = nullptr;

	UPROPERTY()
	TObjectPtr<UProgressBar> HealthBar = nullptr;

	UPROPERTY()
	TObjectPtr<UProgressBar> ChaserHealthBar = nullptr;

	UPROPERTY()
	TObjectPtr<UCommonTextBlock> CurrentHealth = nullptr;

	UPROPERTY()
	TObjectPtr<UCommonTextBlock> TotalHealth = nullptr;

	UPROPERTY()
	TObjectPtr<AOSCharacter> OwningCharacter = nullptr;
	AOSCharacter* GetOwningCharacter() const { return OwningCharacter.Get(); }

	UPROPERTY()
	TObjectPtr<AOSPlayerState> OwningPlayerState = nullptr;
	AOSPlayerState* GetOwningPlayerState() const { return OwningPlayerState.Get(); }

	// --- Data Types ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Nameplate|Health")
	float ChaserLerpSpeed = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Nameplate|Health")
	float ChaseDelay = 0.15f;
	
	// --- Team Colors ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nameplate")
	FLinearColor FriendlyColor = FLinearColor(0.f, 1.f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nameplate")
	FLinearColor EnemyColor = FLinearColor(1.f, 0.f, 0.f);

	// --- Health Bar Colors ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nameplate|Health")
	FLinearColor HealthFullColor = FLinearColor(0.f, 1.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nameplate|Health")
	FLinearColor HealthMidColor = FLinearColor(1.f, 1.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nameplate|Health")
	FLinearColor HealthEmptyColor = FLinearColor(1.f, 0.5f, 0.f);

	UFUNCTION(BlueprintCallable, Category = "Nameplate")
	void SetPlayerName(const FText& InName);

	UFUNCTION(BlueprintCallable, Category = "Nameplate")
	void SetHealthPercent(float Percent);

	UFUNCTION(BlueprintCallable, Category = "Nameplate")
	void SetHealthValues(float Current, float Max);

	void SetOwningCharacter(AOSCharacter* InOwningCharacter);
	
	void FinalizeNameplate();
	
private:
	float TargetChaserPercent = 1.f;
	bool bInitialized = false;
	bool bCharacterSet = false;
	bool bFriendly = false;
	FTimerHandle ChaserTimer;
	FTimerHandle ChaserDelayTimer;

	UPROPERTY()
	TObjectPtr<UOSAttributeSet> AttributeSet = nullptr;

	void StartChaserTick();
	void ChaserTimerTick();
};
