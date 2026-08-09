// Copyright Zollpa LLC

#include "IM_Controller_InvertVertical.h"
#include "ZoransResistance/BlueprintFunctionLibraries/ZoransBlueprintFunctionLibrary.h"
#include "ZoransResistance/Game/ZoransWorldSubsystem.h"

FInputActionValue UIM_Controller_InvertVertical::ModifyRaw_Implementation(const UEnhancedPlayerInput* PlayerInput, FInputActionValue CurrentValue, float DeltaTime)
{
	FVector ModifiedValue = CurrentValue.Get<FVector>();
	
	if (!FMath::IsNearlyZero(ModifiedValue.Y))
	{
		if (const UWorld* PlayerWorld = UZoransBlueprintFunctionLibrary::GetWorldFromPlayerInput(PlayerInput))
		{
			if (const UZoransWorldSubsystem* ZoransWorldSubsystem = PlayerWorld->GetSubsystem<UZoransWorldSubsystem>())
			{
				if (ZoransWorldSubsystem->bControllerInvertedVertical)
				{
					ModifiedValue.Y *= -1.f;
				}
			}
		}
	}
	
	return ModifiedValue;	
}
