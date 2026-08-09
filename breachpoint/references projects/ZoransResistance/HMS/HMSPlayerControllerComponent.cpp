// Copyright Zollpa LLC


#include "ZoransResistance/HMS/HMSPlayerControllerComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetStringLibrary.h"
#include "ZoransResistance/HMS/HMSFunctions.h"
#include "GameFramework/PlayerController.h"
#include "ZoransResistance/HMS/HMSGMComponent.h"
#include "Net/UnrealNetwork.h"
#include "ZoransResistance/HMS/HMSFunctions.h"
#include "ZoransResistance/Game/ZoransGameInstance.h"
#include "Engine/GameInstance.h"
#include "GameFramework/GameStateBase.h"

// Sets default values for this component's properties
UHMSPlayerControllerComponent::UHMSPlayerControllerComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	PlayerID = "";
	Port = "7777";
	Spawned = false; 
	ReconnectTime = 7.f;
	Migrated = false;
	ReceivedStateBuffer.Empty();

}


// Called when the game starts
void UHMSPlayerControllerComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!(GetWorld()->GetNetMode() == NM_DedicatedServer))
	{
		if (Cast<APlayerController>(GetOwner())->IsLocalController())
		{
			GetMyID();
			if (GetOwner()->GetLocalRole() >= ROLE_Authority)
			{
				if (UGameplayStatics::GetGameMode(this)->FindComponentByClass<UHMSGMComponent>())
				{
					UGameplayStatics::GetGameMode(this)->FindComponentByClass<UHMSGMComponent>()->OnPlayerEnter.AddDynamic(this, &UHMSPlayerControllerComponent::OnPlayerEnter);
				}
			}
			Connect_Migrate();
			FTimerHandle handle;
			GetWorld()->GetTimerManager().SetTimer(handle, this, &UHMSPlayerControllerComponent::SetSpawned, 1.f, false);
		}
	}
}


// Called every frame
void UHMSPlayerControllerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void UHMSPlayerControllerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	DOREPLIFETIME(UHMSPlayerControllerComponent, PlayerID);
	DOREPLIFETIME(UHMSPlayerControllerComponent, Spawned);
	DOREPLIFETIME(UHMSPlayerControllerComponent, Migrated);
}

void UHMSPlayerControllerComponent::SetSpawned()
{
	Server_SetSpawned(true);
	GMEnter();
}

void UHMSPlayerControllerComponent::FinishPlayerEnter()
{
	UHMSGMComponent* gm = UGameplayStatics::GetGameMode(this)->FindComponentByClass<UHMSGMComponent>();
	UZoransGameInstance* gi = Cast<UZoransGameInstance>(UGameplayStatics::GetGameInstance(this));
	gm->CurrentStateHMS = gi->LastState;
	gm->Migrated = true;
	gm->LoadState();
}

void UHMSPlayerControllerComponent::Server_SetSpawned_Implementation(bool bSpawned){
	Spawned = bSpawned;
}
bool UHMSPlayerControllerComponent::Server_SetSpawned_Validate(bool bSpawned){
	return true;
}

void UHMSPlayerControllerComponent::Server_SetID_Implementation(const FString& MyID){
	PlayerID = MyID;
}
bool UHMSPlayerControllerComponent::Server_SetID_Validate(const FString& MyID){
	return true;
}

void UHMSPlayerControllerComponent::GMEnter_Implementation(){
	UHMSGMComponent* gm = UGameplayStatics::GetGameMode(this)->FindComponentByClass<UHMSGMComponent>();
	if (!gm)
		return;
	gm->OnPlayerEnter.Broadcast();
}
bool UHMSPlayerControllerComponent::GMEnter_Validate(){
	return true;
}

void UHMSPlayerControllerComponent::MigrateGameClient_Implementation(FName LevelName){
	RecreateSession.Broadcast(this);
}

void UHMSPlayerControllerComponent::GetMyID_Implementation(){
	bool valid = false;
	FString id = "";
	UHMSFunctions::GetPlayerUniqueNetID(Cast<APlayerController>(GetOwner()), id, valid);
	if (!valid)
	{
		FTimerHandle handle;
		GetWorld()->GetTimerManager().SetTimer(handle, this, &UHMSPlayerControllerComponent::GetMyID, .2f, false);
	}
	Server_SetID(id);

}
bool UHMSPlayerControllerComponent::GetMyID_Validate(){
	return true;
}

void UHMSPlayerControllerComponent::Connect_Migrate_Implementation(){
	UZoransGameInstance* gi = Cast<UZoransGameInstance>(UGameplayStatics::GetGameInstance(this));
	if (!gi)
		return;
	if (UHMSFunctions::IsNewHost(this))
	{
		if (gi->LastState.LevelName != UGameplayStatics::GetCurrentLevelName(this))
		{
			MigrateGameClient(FName(*gi->LastState.LevelName));
			return;
		}
		return;
	}
	if (!gi->NewHostIP.IsEmpty())
	{
		bool valid = false;
		FString id = "";
		UHMSFunctions::GetPlayerUniqueNetID(Cast<APlayerController>(GetOwner()), id, valid);
		if (!valid)
		{
			FTimerHandle handle;
			GetWorld()->GetTimerManager().SetTimer(handle, this, &UHMSPlayerControllerComponent::Connect_Migrate, .2f, false);
		}
		AGameStateBase* gs = UGameplayStatics::GetGameState(this);
		if (!gs)
			return;
		if (gs->PlayerArray.Num() <= 1)
		{
			UKismetSystemLibrary::ExecuteConsoleCommand(this,BuildConsoleCommand(gi->NewHostIP));
			FTimerHandle handle;
			GetWorld()->GetTimerManager().SetTimer(handle, this, &UHMSPlayerControllerComponent::Connect_Migrate, .2f, false);
		}
	}
}

void UHMSPlayerControllerComponent::ReceiveGameStateChunk_Implementation(const TArray<uint8>& Chunk, bool bFinal, int32 TotalSize){
	if (Cast<APlayerController>(GetOwner())->IsLocalController())
	{
		ReceivedStateBuffer.Append(Chunk);
		if (bFinal)
		{
			ReceivedStateBuffer.SetNum(TotalSize);
			GI_SetState(UHMSFunctions::BytesToGameSave(ReceivedStateBuffer));
			ReceivedStateBuffer.Empty();
		}
	}
}

void UHMSPlayerControllerComponent::OnPlayerEnter(){
	if (GetOwner()->GetLocalRole() >= ROLE_Authority)
	{
		UZoransGameInstance* gi = Cast<UZoransGameInstance>(UGameplayStatics::GetGameInstance(this));
		if (!gi)
			return;
		UHMSGMComponent* gm = UGameplayStatics::GetGameMode(this)->FindComponentByClass<UHMSGMComponent>();
		if (!gm)
			return;
		if (UHMSFunctions::IsNewHost(this) && UHMSFunctions::ShouldLoadState(this))
		{
			if(gm->AllPCHMS.Num() == gi->LastState.Players.Num())
			{
				FinishPlayerEnter();
			}
			GetWorld()->GetTimerManager().ClearTimer(OnPlayerEnterHandle);
			GetWorld()->GetTimerManager().SetTimer(OnPlayerEnterHandle, this, &UHMSPlayerControllerComponent::FinishPlayerEnter, ReconnectTime, false);
		}
	}
}

void UHMSPlayerControllerComponent::ReceiveGameState_Implementation(const TArray<uint8>& Bytes){
	GI_SetState(UHMSFunctions::BytesToGameSave(Bytes));
}

void UHMSPlayerControllerComponent::GI_SetState(FHMSGameSave LastState){
	if (Spawned && Cast<APlayerController>(GetOwner())->IsLocalController())
	{
		UZoransGameInstance* gi = Cast<UZoransGameInstance>(UGameplayStatics::GetGameInstance(this));
		if (!gi)
			return;
		gi->LastState = LastState;
		gi->NewHostIP = "";
	}
}

void UHMSPlayerControllerComponent::GI_SetNewHostIP_Implementation(const FString& IP){
	if (!IP.IsEmpty() && Cast<APlayerController>(GetOwner())->IsLocalController())
	{
		UZoransGameInstance* gi = Cast<UZoransGameInstance>(UGameplayStatics::GetGameInstance(this));
		if (!gi)
			return;
		gi->NewHostIP = IP;
	}
}

FString UHMSPlayerControllerComponent::BuildConsoleCommand(FString HostIP)
{
	FString sub = UKismetStringLibrary::GetSubstring(HostIP, 0, UKismetStringLibrary::FindSubstring(HostIP, ":"));
	FString command = HostIP.Contains(".") ? sub : "Steam" + sub;
	return "open " + command + ":" + Port;
}