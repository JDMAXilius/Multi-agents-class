// Fill out your copyright notice in the Description page of Project Settings.


#include "Animations/Notifies/OSAnimNotifyState.h"

FString UOSAnimNotifyState::GetNotifyName_Implementation() const
{
	if (CustomName != NAME_None)
		return CustomName.ToString();
	
	if (DefaultName != NAME_None)
		return DefaultName.ToString();
	
	return Super::GetNotifyName_Implementation();
}
