// Copyright Zollpa LLC


#include "ZoransResistance/HMS/HMSFunctions.h"
#include "Engine/Engine.h"
#include "UObject/UObjectGlobals.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Engine/StaticMesh.h"
#include "Serialization/StructuredArchive.h"
#include "Kismet/GameplayStatics.h"
#include "ZoransResistance/Game/ZoransGameInstance.h"
#include "OnlineSubsystemUtils.h"
#include "HMSSavedStateComponent.h"
#include "Serialization/ArchiveLoadCompressedProxy.h"
#include "GameFramework/PlayerState.h"
#include "Serialization/ArchiveSaveCompressedProxy.h"
#include "GameFramework/GameStateBase.h"
#include "Containers/UnrealString.h"
#include "Serialization/Archive.h"
#include "CoreGlobals.h"
#include "HAL/UnrealMemory.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "SocketSubsystem.h"
#include "ZoransResistance/HMS/HMSSaveAsyncTask.h"


bool UHMSFunctions::MakeReference(AActor* Actor, FHMSActorReference& Reference)
{
	if (!Actor) return false;
	UHMSSavedStateComponent* comp = nullptr;
	comp = Cast<UHMSSavedStateComponent>(Actor->GetComponentByClass(UHMSSavedStateComponent::StaticClass()));
	if (!comp) return false;

	Reference = FHMSActorReference(comp->Tag, Actor->StaticClass());

	return true;
}



AActor* UHMSFunctions::GetReference(UObject* WorldContextObject, FHMSActorReference Reference)
{
	if (!WorldContextObject) return nullptr;
	return Reference.GetReference(WorldContextObject);
}

FString UHMSFunctions::ClassToString(UClass* inClass)
{
	if (!inClass) return FString("");

	return inClass->GetName();
}



UClass* UHMSFunctions::StringToClass(FString ClassName)
{
	return FindObject<UClass>(ANY_PACKAGE, *ClassName);
}

void UHMSFunctions::GetPlayerUniqueNetID(const APlayerController* PlayerController, FString& ID, bool& isValid)
{
	if (!PlayerController)
	{
		return;
	}

	if (const APlayerState* PlayerState = (PlayerController != NULL) ? PlayerController->PlayerState : NULL)
	{
#if (ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 1)
		ID = PlayerState->GetUniqueId().GetUniqueNetId()->ToString();
		isValid = PlayerState->GetUniqueId().GetUniqueNetId()->IsValid();
#else
		ID = PlayerState->UniqueId.GetUniqueNetId()->ToString();
		isValid = PlayerState->UniqueId.GetUniqueNetId()->IsValid();
#endif
	}
}

void UHMSFunctions::GetByteChunk(const TArray<uint8>& Bytes, int StartIndex, int ChunkSize, TArray<uint8>& Chunk)
{
	TArray<uint8> temp;
	temp.SetNum(ChunkSize);
	ChunkSize = FMath::Min(ChunkSize, Bytes.Num() - StartIndex);

	if (ChunkSize <= 0) return;
	if (!Bytes.IsValidIndex(StartIndex + ChunkSize - 1)) return;

	FMemory::Memcpy(temp.GetData(), Bytes.GetData() + StartIndex, ChunkSize);
	Chunk = temp;
}

bool UHMSFunctions::ShouldLoadActor(const TArray<FHMSActorState>& Actors, const AActor* ToCheck)
{
	if (!ToCheck || Actors.Num() < 1) return false;

	UHMSSavedStateComponent* savedState = ToCheck->FindComponentByClass<UHMSSavedStateComponent>();
	if (!savedState) return false;

	for (FHMSActorState state : Actors)
	{
		if (savedState->Tag == state.ActorSave.ActorID)
			return true;
	}

	return false;
}

void UHMSFunctions::IsPlayerControllerInSession(UObject* WorldContextObject, APlayerController* PlayerController, bool& InSession)
{
	if (!WorldContextObject) return;
	if (!PlayerController) return;

	UWorld* const World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World) return;

	IOnlineSessionPtr SessionInterface = Online::GetSessionInterface(World);

	if (!SessionInterface.IsValid())
	{
		InSession = false;
		return;
	}

#if (ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 1)
	InSession = SessionInterface->IsPlayerInSession(NAME_GameSession, *PlayerController->PlayerState->GetUniqueId().GetUniqueNetId());
#else
	InSession = SessionInterface->IsPlayerInSession(NAME_GameSession, *PlayerController->PlayerState->UniqueId.GetUniqueNetId());
#endif
}

FString UHMSFunctions::GetPublicIP(APlayerController* pc)
{
	if (!pc) return FString("");

	bool bruh = false;

	if (pc)
		return pc->NetConnection ? pc->NetConnection->RemoteAddressToString() : ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->GetLocalHostAddr(*GLog, bruh)->ToString(false);
	else
		return FString("");
}



void UHMSFunctions::StartSaveGameActors(UObject* WorldContextObject, AActor* GameMode, bool IgnoreHostPawn /*= true*/)
{
	if (!WorldContextObject->GetWorld()) return;
	if (!IsInGameThread()) return;

	TArray<AActor*> FoundActors;
	TArray<const AActor*>* ConstFoundActors = new TArray<const AActor*>();
	UGameplayStatics::GetAllActorsOfClass(WorldContextObject->GetWorld(), AActor::StaticClass(), FoundActors);

	for (AActor* actor : FoundActors)
	{
		if (!actor) continue;
		if (actor->Implements<UHMSSavedState>())
			IHMSSavedState::Execute_PreSaveActorState(actor);

		ConstFoundActors->Add(actor);
	}

	const TArray<const AActor*>* const ConstConstFoundActors = ConstFoundActors;
	FTimerHandle handle;
	WorldContextObject->GetWorld()->GetTimerManager().SetTimer(handle, [ConstConstFoundActors, WorldContextObject, GameMode, IgnoreHostPawn, &handle]()
		{
			//fuck this man	
			(new FAutoDeleteAsyncTask<SaveAsyncTask>(WorldContextObject->GetWorld(), GameMode, IgnoreHostPawn, ConstConstFoundActors))->StartBackgroundTask();
			handle.Invalidate();
		}, FMath::Max(0.1f, WorldContextObject->GetWorld()->DeltaTimeSeconds), false, -1.f);
}

FHMSActorSave UHMSFunctions::SerializeActor(AActor* Actor, TArray<FString> ComponentNames, TArray<FString> CustomPropertyNames)
{
	return FHMSActorSave(Actor, ComponentNames, CustomPropertyNames);
}

TArray<uint8> UHMSFunctions::SerializeObject(UObject* Object, bool SaveGameTagOnly, bool SaveDefaults)
{
	TArray<uint8> bytes;
	FMemoryWriter ActorWriter = FMemoryWriter(bytes, true);
	FObjectAndNameAsStringProxyArchive ar(ActorWriter, false);
	ar.ArIsSaveGame = SaveGameTagOnly;
	ar.ArNoDelta = true;
	ar.ArSerializingDefaults = SaveDefaults ? 1 : -1;
	Object->Serialize(ar);
	ActorWriter.Flush();

	return bytes;
}

void UHMSFunctions::DeserializeObject(UObject* Object, TArray<uint8> bytes, bool SaveGameTagOnly, bool SaveDefaults)
{
	FMemoryReader ActorReader = FMemoryReader(bytes, true);
	FObjectAndNameAsStringProxyArchive Archive(ActorReader, false);
	Archive.ArIsSaveGame = SaveGameTagOnly;
	Archive.ArNoDelta = true;
	Archive.ArSerializingDefaults = SaveDefaults ? 1 : -1;
	Object->Serialize(Archive);
	ActorReader.Flush();
}

TArray<uint8> UHMSFunctions::GameSaveToBytes(FHMSGameSave GameSave)
{
	TArray<uint8> RawData;
	FMemoryWriter Writer(RawData);
	Writer << GameSave;

	TArray<uint8> CompressedBytes;
	FArchiveSaveCompressedProxy Compressor(CompressedBytes, NAME_Zlib);
	Compressor << RawData;
	Compressor.Flush();
	Compressor.Close();

	return CompressedBytes;
}

UActorComponent* UHMSFunctions::GetComponentByName(AActor* parent, FString name)
{
	return Cast<UActorComponent>(parent->GetDefaultSubobjectByName(*name));
}

FHMSGameSave UHMSFunctions::BytesToGameSave(TArray<uint8> bytes)
{
	TArray<uint8> DecompressedBytes;
	FArchiveLoadCompressedProxy Decompressor(bytes, NAME_Zlib);
	Decompressor << DecompressedBytes;
	Decompressor.Flush();
	Decompressor.Close();


	FHMSGameSave temp;
	FMemoryReader Reader(DecompressedBytes);
	Reader << temp;

	return temp;
}

void UHMSFunctions::DeserializeActor(AActor* Actor, FHMSActorSave State, TArray<FString> ComponentNames, TArray<FString> CustomPropertyNames)
{
	State.Deserialize(Actor, ComponentNames, CustomPropertyNames);
}

bool UHMSFunctions::ShouldRehost(UObject* WorldContextObject)
{
	UZoransGameInstance* gi = Cast<UZoransGameInstance>(UGameplayStatics::GetGameInstance(WorldContextObject));
	if (!gi)
		return false;
	return !UGameplayStatics::GetCurrentLevelName(WorldContextObject).IsEmpty() && !gi->LastState.LevelName.IsEmpty();
}

void UHMSFunctions::ResetHostMigration(UObject* WorldContextObject)
{
	UZoransGameInstance* gi = Cast<UZoransGameInstance>(UGameplayStatics::GetGameInstance(WorldContextObject));
	if (!gi)
		return;
	gi->NewHostIP = "";
	gi->LastState = FHMSGameSave();
}

bool UHMSFunctions::IsNewHost(UObject* WorldContextObject)
{
	UZoransGameInstance* gi = Cast<UZoransGameInstance>(UGameplayStatics::GetGameInstance(WorldContextObject));
	if (!gi)
		return false;
	return !gi->LastState.LevelName.IsEmpty() && gi->NewHostIP.IsEmpty();
}

bool UHMSFunctions::ShouldLoadState(UObject* WorldContextObject)
{
	UZoransGameInstance* gi = Cast<UZoransGameInstance>(UGameplayStatics::GetGameInstance(WorldContextObject));
	if (!gi)
		return false;
	return UGameplayStatics::GetCurrentLevelName(WorldContextObject) == gi->LastState.LevelName;
}
