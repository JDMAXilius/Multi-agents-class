// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OSScoreboardComponent.generated.h"

class AOSPlayerState;
class UVerticalBox;
class UOSScoreboardInfoComponentWidget;
/**
 * 
 */
UCLASS()
class ONSIGHT_API UOSScoreboardComponent : public UUserWidget
{
	GENERATED_BODY()
public:
	void AddRow(AOSPlayerState* PS);
	void RemoveRow(AOSPlayerState* PS);
	void RemoveRowsNotIn(const TSet<TObjectPtr<AOSPlayerState>>& ActivePlayers);
	void ClearRows();

protected:
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UVerticalBox> ScoreboardRows;

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UOSScoreboardInfoComponentWidget> RowWidgetClass;

private:
	UPROPERTY()
	TMap<TObjectPtr<AOSPlayerState>, TObjectPtr<UOSScoreboardInfoComponentWidget>> RowByPS;
};
