// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ZoransResistance/Game/Data/ZoransAzurePlayFabData.h"
#include "ZoransChallengesBlueprintFunctionLibrary.generated.h"

DECLARE_DYNAMIC_DELEGATE_TwoParams(FWeaponNameToDisplayName, FString, WeaponName, FString&, WeaponDisplayName);

UCLASS()
class ZORANSRESISTANCE_API UZoransChallengesBlueprintFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Zorans Challenges Blueprint Function Library")
	static FText GetChallengeName(const FChallengeData& Challenge);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Zorans Challenges Blueprint Function Library")
	static FText GetConditionText(const FChallengeData& Challenge, FWeaponNameToDisplayName WeaponNameFunc);
};
