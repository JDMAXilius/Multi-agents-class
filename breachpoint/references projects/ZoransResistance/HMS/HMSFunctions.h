// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "Engine.h"
#include "Engine/NetConnection.h"
#include "ImageUtils.h"

#include "OnlineSubsystem.h"

#include "Interfaces/OnlineFriendsInterface.h"
#include "Interfaces/OnlineUserInterface.h"
#include "Interfaces/OnlineMessageInterface.h"
#include "Interfaces/OnlinePresenceInterface.h"
#include "Engine/GameInstance.h"
#include "FindSessionsCallbackProxy.h"
#include "GameFramework/PlayerController.h"
#include "Serialization/StructuredArchive.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ZoransResistance/HMS/FHMSActorState.h"
#include "ZoransResistance/HMS/FHMSActorSave.h"
#include "ZoransResistance/HMS/FHMSGameSave.h"
#include "ZoransResistance/HMS/FHMSActorReference.h"
#include "HMSFunctions.generated.h"

UCLASS()
class ZORANSRESISTANCE_API UHMSFunctions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	//ObjectReference
	UFUNCTION(BlueprintPure, Category = "ActorReference")
	static bool MakeReference(AActor* Actor, FHMSActorReference& Reference);

	UFUNCTION(BlueprintPure, Category = "ActorReference", meta = (WorldContext = "WorldContextObject"))
	static AActor* GetReference(UObject* WorldContextObject, FHMSActorReference Reference);

	//ClassConversions
	UFUNCTION(BlueprintPure, Category = "ClassConversion")
	static FString ClassToString(UClass* inClass);

	UFUNCTION(BlueprintPure, Category = "ClassConversion")
	static UClass* StringToClass(FString ClassName);

	//Networking
	UFUNCTION(BlueprintPure, Category = "Networking")
	static void GetPlayerUniqueNetID(const APlayerController* PlayerController, FString& ID, bool& isValid);

	UFUNCTION(BlueprintCallable, Category = "Networking", meta = (WorldContext = "WorldContextObject"))
	static void IsPlayerControllerInSession(UObject* WorldContextObject, APlayerController* PlayerController, bool& InSession);

	UFUNCTION(BlueprintPure, Category = "Networking")
	static FString GetPublicIP(APlayerController* pc);

	//Serialization
	UFUNCTION(BlueprintCallable, Category = "Serialization", meta = (WorldContext = "WorldContextObject"))
	static void StartSaveGameActors(UObject* WorldContextObject, AActor* GameMode, bool IgnoreHostPawn = true);

	UFUNCTION(BlueprintPure, Category = "Networking")
	static void GetByteChunk(const TArray<uint8>& Bytes, int StartIndex, int ChunkSize, TArray<uint8>& Chunk);

	UFUNCTION(BlueprintPure, Category = "Serialization")
	static bool ShouldLoadActor(const TArray<FHMSActorState>& Actors, const AActor* ToCheck);

	UFUNCTION(BlueprintPure, Category = "Serialization")
	static FHMSActorSave SerializeActor(AActor* Actor, TArray<FString> ComponentNames, TArray<FString> CustomPropertyNames);

	UFUNCTION(BlueprintPure, Category = "Serialization")
	static TArray<uint8> SerializeObject(UObject* Object, bool SaveGameTagOnly = true, bool SaveDefaults = false);

	UFUNCTION(BlueprintCallable, Category = "Serialization")
	static void DeserializeObject(UObject* Object, TArray<uint8> bytes, bool SaveGameTagOnly = true, bool SaveDefaults = false);

	UFUNCTION(BlueprintPure, Category = "Serialization")
	static TArray<uint8> GameSaveToBytes(FHMSGameSave GameSave);

	UFUNCTION(BlueprintPure, Category = "Serialization")
	static UActorComponent* GetComponentByName(AActor* parent, FString name);

	UFUNCTION(BlueprintPure, Category = "Serialization")
	static FHMSGameSave BytesToGameSave(TArray<uint8> bytes);

	UFUNCTION(BlueprintCallable, Category = "Serialization")
	static void DeserializeActor(AActor* Actor, FHMSActorSave State, TArray<FString> ComponentNames, TArray<FString> CustomPropertyNames);
	
	UFUNCTION(BlueprintPure,Category = "HMS", meta = (WorldContext = "WorldContextObject"))
	static bool ShouldRehost(UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable,Category = "HMS", meta = (WorldContext = "WorldContextObject"))
	static void ResetHostMigration(UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "HMS", meta = (WorldContext = "WorldContextObject"))
	static bool IsNewHost(UObject* WorldContextObject);

	UFUNCTION(BlueprintPure,Category = "HMS", meta = (WorldContext = "WorldContextObject"))
	static bool ShouldLoadState(UObject* WorldContextObject);

};
