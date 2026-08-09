// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/NMRowSession.h"
#include "Components/ListView.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

void UNMRowSession::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);

	if (!ListItemObject) return;

	if (const UNMRowSession* Entry = Cast<UNMRowSession>(ListItemObject))
	{
		SetServerData(Entry->CurrentSessionResult);
		ServerNameText->SetText(FText::FromString(CurrentSessionResult.Session.OwningUserName));
		PlayerCountText->SetText(FText::Format(
			FText::FromString("{0}/{1}"),
			CurrentSessionResult.Session.SessionSettings.NumPublicConnections - CurrentSessionResult.Session.NumOpenPublicConnections,
			CurrentSessionResult.Session.SessionSettings.NumPublicConnections
		));
		PingText->SetText(FText::AsNumber(CurrentSessionResult.PingInMs));
	}
}

void UNMRowSession::NativeConstruct()
{
	Super::NativeConstruct();
}

void UNMRowSession::SetServerData(const FOnlineSessionSearchResult& SearchResult)
{
	CurrentSessionResult = SearchResult;
}

FReply UNMRowSession::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Cast<UListView>(GetOwningListView())->SetSelectedItem(this);
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}