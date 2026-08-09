// Fill out your copyright notice in the Description page of Project Settings.


#include "Animations/Notifies/OSAnimNotify.h"

FString UOSAnimNotify::GetNotifyName_Implementation() const
{
	if (CustomName != NAME_None)
		return CustomName.ToString();
	
	return Super::GetNotifyName_Implementation();
}
