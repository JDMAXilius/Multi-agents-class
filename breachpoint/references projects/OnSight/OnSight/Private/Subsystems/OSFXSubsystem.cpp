// Fill out your copyright notice in the Description page of Project Settings.


#include "Subsystems/OSFXSubsystem.h"

#include "Core/Settings/OSGASDeveloperSettings.h"

void UOSFXSubsystem::Initialize( FSubsystemCollectionBase& Collection )
{
	Super::Initialize(Collection);

	const auto* S = GetDefault<UOSGASSettings>();
	if (!S) return;

	if (!S->DefaultLibrary.IsNull())
	{
		Library = S->DefaultLibrary.LoadSynchronous();
	}
}
