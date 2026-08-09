// Copyright Zollpa LLC.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ZoransConfiguratorFunctionLibrary.generated.h"

UCLASS()
class ZORANSRESISTANCE_API UZoransConfiguratorFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/**
	 * Toggle between FSR and Deferred Rendering.
	*/
	UFUNCTION(BlueprintCallable,Category = "Rendering", meta = (WorldContext = "WorldContextObject"))
	static void SetFSR(UObject* WorldContextObject, bool bFSREnabled);
	
	/**
	 * Toggle between FSR and Deferred Rendering.
	*/
	UFUNCTION(BlueprintCallable,Category = "Rendering", meta = (WorldContext = "WorldContextObject"))
	static void ToggleFSR(UObject* WorldContextObject);

	/**
	 * Get the state of FSR.
	*/
	UFUNCTION(BlueprintPure,Category = "Console")
	static bool IsFSREnabled();

	/**
	 * Set the state of a CVar as string.
	*/
	UFUNCTION(BlueprintCallable, Category = "Console")
	static void SetCustomCVar(const FString& IniPath, const FString& SettingsPath, const FString& Key, const FString& Value);

	/**
	 * Get the state of a CVar as string.
	*/
	UFUNCTION(BlueprintPure,Category = "Console")
	static void GetCVar(const FString& IniPath, const FString& SettingsPath, const FString& Key, FString& Value);

};