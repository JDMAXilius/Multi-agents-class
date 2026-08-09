// Copyright Zollpa LLC


#include "ZoransResistance/HMS/HMSGMComponent.h"
#include "ZoransResistance/HMS/HMSPlayerControllerComponent.h"
#include "ZoransResistance/HMS/HMSSavedStateComponent.h"
#include "ZoransResistance/HMS/HMSFunctions.h"
#include "ZoransResistance/HMS/HMSSavedState.h"
#include "Engine.h"
#include "EngineUtils.h"
#include "Kismet/KismetMathLibrary.h"
#include "GameFramework/GameStateBase.h"

// Sets default values for this component's properties
UHMSGMComponent::UHMSGMComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UHMSGMComponent::BeginPlay()
{
	Super::BeginPlay();
	StartStateUpdateLoop();
	
}


// Called every frame
void UHMSGMComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void UHMSGMComponent::LoadState(){
	AGameStateBase* gs = UGameplayStatics::GetGameState(this);
	UHMSPlayerControllerComponent* pcc = nullptr;
	UHMSFunctions::DeserializeObject(this, CurrentStateHMS.GameModeSave, true, false);
	UHMSFunctions::DeserializeObject(gs, CurrentStateHMS.GameStateSave, true, false);
	if (GetOwner()->GetClass()->ImplementsInterface(UHMSSavedState::StaticClass()))
		IHMSSavedState::Execute_LoadActorState(GetOwner());
	if (gs->GetClass()->ImplementsInterface(UHMSSavedState::StaticClass()))
		IHMSSavedState::Execute_LoadActorState(gs);
	for (auto& player : CurrentStateHMS.Players)
	{
		for (auto& pc : AllPCHMS)
		{
			pcc = pc->FindComponentByClass<UHMSPlayerControllerComponent>();
			if (!pcc)
				continue;
			pcc->Migrated = Migrated;
			if (pcc->PlayerID == player.PlayerID)
			{
				UHMSFunctions::DeserializeObject(pc, player.PlayerControllerData, true, false);
				UHMSFunctions::DeserializeObject(pc->PlayerState, player.PlayerStateData, true, false);
				if (pc->GetClass()->ImplementsInterface(UHMSSavedState::StaticClass()))
					IHMSSavedState::Execute_LoadActorState(pc);
				if (pc->PlayerState->GetClass()->ImplementsInterface(UHMSSavedState::StaticClass()))
					IHMSSavedState::Execute_LoadActorState(pc->PlayerState);
			}
		}
	}
	UHMSSavedStateComponent* comp = nullptr;
	for (TActorIterator<AActor> itr(GetWorld()); itr; ++itr)
	{
		comp = itr->FindComponentByClass<UHMSSavedStateComponent>();
		if (UHMSFunctions::ShouldLoadActor(CurrentStateHMS.Actors,*itr))
		{
			if (!comp)
			{
				Cast<APlayerController>(Cast<APawn>(*itr)->GetController());
				itr->Destroy();
			}
			itr->Destroy();
		}
	}
	AActor* spawned = nullptr;
	for (auto& actor : CurrentStateHMS.Actors)
	{
		spawned = GetWorld()->SpawnActor<AActor>(UHMSFunctions::StringToClass(actor.ActorClass), actor.ActorSave.ActorTransformation);
		comp = spawned->FindComponentByClass<UHMSSavedStateComponent>();
		if (actor.ControllerID.IsEmpty() || !Cast<APawn>(spawned))
		{
			if (comp)
			{
				comp->LoadActorState(actor.ActorSave);
			}
		}
		for (auto& pc2 : AllPCHMS)
		{
			pcc = pc2->FindComponentByClass<UHMSPlayerControllerComponent>();
			if (!pcc)
				continue;
			if (pcc->PlayerID == actor.ControllerID)
				pc2->Possess(Cast<APawn>(spawned));
		}
	}
}


void UHMSGMComponent::UpdateState(){
	if (NewHostHMS)
	{
		SaveState(true);
		return;
	}
	ChooseNewHost();
	UHMSPlayerControllerComponent* pcc = nullptr;
	for (auto& pc : AllPCHMS)
	{
		pcc = pc->FindComponentByClass<UHMSPlayerControllerComponent>();
		if (!pcc)
		{
			continue;
		}
		if (pcc->Spawned)
		{
			if (!(pc->IsLocalController() || pc == NewHostHMS))
			{
				pcc->GI_SetNewHostIP(UHMSFunctions::GetPublicIP(NewHostHMS));
			}
		}
	}
}


void UHMSGMComponent::ChooseNewHost(){
	APlayerController* candidate = nullptr;
	int32 smallestavgping = 9999;
	int32 avgping = 0;
	int32 result = 0;
	for (auto& pc1 : AllPCHMS)
	{
		if (!pc1->IsLocalController())
		{
			avgping += (pc1->PlayerState->GetCompressedPing() * 4);
		}
	}
	for (auto& pc2 : AllPCHMS)
	{
		if (!pc2->IsLocalController())
		{
			result = FMath::Abs((pc2->PlayerState->GetCompressedPing() * 4) - avgping);
			if (result < smallestavgping)
			{
				smallestavgping = result;
				candidate = pc2;
			}
		}
	}
	NewHostHMS = candidate;
}


TArray<FHMSPlayerSave> UHMSGMComponent::SavePlayers(){
	TArray<FHMSPlayerSave> players;
	UHMSPlayerControllerComponent* pcc;
	for (auto& pc : AllPCHMS)
	{
		pcc = pc->FindComponentByClass<UHMSPlayerControllerComponent>();
		if (!pcc)
			continue;
		if (Cast<APlayerController>(pcc->GetOwner())->IsLocalController())
			if (IgnoreHostPawn)
				continue;
		if (pc->GetClass()->ImplementsInterface(UHMSSavedState::StaticClass()))
			IHMSSavedState::Execute_PreSaveActorState(pc);
		if (pc->PlayerState->GetClass()->ImplementsInterface(UHMSSavedState::StaticClass()))
			IHMSSavedState::Execute_PreSaveActorState(pc->PlayerState);
		players.Add(FHMSPlayerSave(pcc->PlayerID, UHMSFunctions::SerializeObject(pc->PlayerState, true, false), UHMSFunctions::SerializeObject(pc, true, false)));
	}
	return players;
}

void UHMSGMComponent::StartStateUpdateLoop_Implementation() {
	if (!(GetWorld()->GetNetMode() == NM_DedicatedServer))
	{
		if (GetOwner()->GetLocalRole() >= ROLE_Authority)
		{
			FTimerHandle handle;
			bool session = false;
			UHMSFunctions::IsPlayerControllerInSession(this, UGameplayStatics::GetPlayerController(this, 0),session);
			if (session)
			{
				GetWorld()->GetTimerManager().SetTimer(handle, this, &UHMSGMComponent::UpdateState, UpdatePeriod, true);
				return;
			}
			GetWorld()->GetTimerManager().SetTimer(handle, this, &UHMSGMComponent::StartStateUpdateLoop, .2f, false);
		}
	}
}
bool UHMSGMComponent::StartStateUpdateLoop_Validate() {
	return true;
}


void UHMSGMComponent::PlayerLogin(APlayerController* NewPlayer) {
	AllPCHMS.Add(NewPlayer);
}


void UHMSGMComponent::PlayerLogout(AController* ExitingController) {
	AllPCHMS.Remove(Cast<APlayerController>(ExitingController));
}


void UHMSGMComponent::SendGameSaveInChunks() {
	if (!SendingChunkHMS)
	{
		TArray<uint8> chunks = UHMSFunctions::GameSaveToBytes(CurrentStateHMS);
		SendingChunkHMS = true;
		UHMSPlayerControllerComponent* pcc = NewHostHMS->FindComponentByClass<UHMSPlayerControllerComponent>();
		if (!pcc)
			return;
		bool finished = SendChunkIndexHMS == FMath::CeilToFloat(float(chunks.Num()) / float(SendChunkSizeHMS));
		TArray<uint8> outchunks;
		UHMSFunctions::GetByteChunk(chunks, SendChunkIndexHMS * SendChunkSizeHMS, SendChunkSizeHMS,outchunks);
		pcc->ReceiveGameStateChunk(outchunks, finished, chunks.Num());
		if (finished)
		{
			SendChunkIndexHMS = 0;
			SendingChunkHMS = false;
			return;
		}
		SendChunkIndexHMS++;
		FTimerHandle handle;
		GetWorld()->GetTimerManager().SetTimer(handle, this, &UHMSGMComponent::SendGameSaveInChunks, .001f, false);
	}
}


void UHMSGMComponent::SaveState(bool bCalledOnUpdate){
	CalledOnUpdate = bCalledOnUpdate;
	OnActorsSaved.AddDynamic(this, &UHMSGMComponent::EventActorsSaved);
	if (GetOwner()->GetClass()->ImplementsInterface(UHMSSavedState::StaticClass()))
		IHMSSavedState::Execute_PreSaveActorState(GetOwner());
	if (UGameplayStatics::GetGameState(this)->GetClass()->ImplementsInterface(UHMSSavedState::StaticClass()))
		IHMSSavedState::Execute_PreSaveActorState(UGameplayStatics::GetGameState(this));
	UHMSFunctions::StartSaveGameActors(this,GetOwner(),IgnoreHostPawn);
}


void UHMSGMComponent::EventActorsSaved(const TArray<FHMSActorState>& ActorStates){
	CurrentStateHMS = FHMSGameSave(SavePlayers(), ActorStates, UHMSFunctions::SerializeObject(this, true, false), 
		UHMSFunctions::SerializeObject(UGameplayStatics::GetGameState(this),true, false), 
		UGameplayStatics::GetCurrentLevelName(this));
	FTimerHandle handle;
	GetWorld()->GetTimerManager().SetTimer(handle, this, &UHMSGMComponent::FinishSave, .2f, false);
}

void UHMSGMComponent::FinishSave()
{
	OnSaveComplete.Broadcast(CalledOnUpdate);
	if (CalledOnUpdate)
	{
		UHMSPlayerControllerComponent* pcc = NewHostHMS->FindComponentByClass<UHMSPlayerControllerComponent>();
		if (!pcc)
			return;
		if (pcc->Spawned)
		{
			switch (SendTypeHMS)
			{
			case EHMSTransferType::EGameNetDriverChunks:
				SendGameSaveInChunks();
				break;
			case EHMSTransferType::EGameNetDriverFull:
				pcc->ReceiveGameState(UHMSFunctions::GameSaveToBytes(CurrentStateHMS));
				break;
			}
		}
	}
}

void UHMSGMComponent::SimulateHostMigration(){
	SendTypeHMS = EHMSTransferType::EGameNetDriverFull;
	IgnoreHostPawn = false;
	NewHostHMS = AllPCHMS[0];
	OnSaveComplete.AddDynamic(this, &UHMSGMComponent::OnSaveComplete_Testing);
	SaveState(false);
}

void UHMSGMComponent::OnSaveComplete_Testing(bool bCalledOnUpdate)
{
	if (!bCalledOnUpdate)
	{
		UHMSPlayerControllerComponent* pcc = UGameplayStatics::GetPlayerController(this,0)->FindComponentByClass<UHMSPlayerControllerComponent>();
		if (!pcc)
			return;
		pcc->ReceiveGameState(UHMSFunctions::GameSaveToBytes(CurrentStateHMS));
	}
}

