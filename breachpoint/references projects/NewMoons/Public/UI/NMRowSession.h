// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OnlineSessionSettings.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Blueprint/UserWidget.h"
#include "NMRowSession.generated.h"

class UButton;
class UTextBlock;
class UNMSessionsSubsystem;

UCLASS()
class NEWMOONS_API UNMRowSession : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	
	virtual void NativeConstruct() override;
	
	// Set the server data from the session search result
	void SetServerData(const FOnlineSessionSearchResult& SearchResult);

	// Get the last session settings
	const FOnlineSessionSearchResult GetSearchResult() const { return CurrentSessionResult; }
	
protected:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Server Menu", meta = (BindWidget))
	UTextBlock* ServerNameText;

	UPROPERTY(BlueprintReadWrite, Category = "Server Menu", meta = (BindWidget))
	UTextBlock* PlayerCountText;

	UPROPERTY(BlueprintReadWrite, Category = "Server Menu", meta = (BindWidget))
	UTextBlock* PingText;
	
	FOnlineSessionSearchResult CurrentSessionResult;

	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	// The subsystem designed to handle all online session functionality
	UNMSessionsSubsystem* ServerSessionsSubsystem;
};
