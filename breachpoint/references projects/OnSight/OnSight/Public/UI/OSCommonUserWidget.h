// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "OSCommonUserWidget.generated.h"

class AOSGameState;
class AOSCharacter;
class AOSPlayerState;
class UOSAttributeSet;
class UAbilitySystemComponent;

//Base class for OS widgets: Centralizes GAS related initializaation
UCLASS()
class ONSIGHT_API UOSCommonUserWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	
	UFUNCTION(BlueprintCallable, Category = "GAS")
	void NotifyGASReady();

	UFUNCTION(BlueprintNativeEvent, Category = "GAS")
	void OnGASReady(UAbilitySystemComponent* ASC, UOSAttributeSet* AS);
	
protected:
	virtual void OnGASReady_Implementation(UAbilitySystemComponent* ASC, UOSAttributeSet* AS) {};
	virtual void OnGameStateReady(AOSGameState* GS) {};
	
	UPROPERTY()
	TObjectPtr<AOSGameState> OwningGameState = nullptr;
	AOSGameState* GetOwningGameState() const { return OwningGameState.Get(); }
	
	UPROPERTY()
	TObjectPtr<AOSCharacter> OwningCharacter = nullptr;
	AOSCharacter* GetOwningCharacter() const { return OwningCharacter.Get(); }

	UPROPERTY()
	TObjectPtr<AOSPlayerState> OwningPlayerState = nullptr;
	AOSPlayerState* GetOwningPlayerState() const { return OwningPlayerState.Get(); }
	
	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> OwningASC = nullptr;
	UAbilitySystemComponent* GetOwningASC () const { return OwningASC.Get(); }

	UPROPERTY()
	TObjectPtr<UOSAttributeSet> OwningAttributeSet = nullptr;
	UOSAttributeSet* GetOwningAttributeSet () const { return OwningAttributeSet.Get(); }
	
private:
	bool bInitialized = false;

	void Init();
};
