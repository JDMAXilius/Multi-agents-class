// Copyright Zollpa LLC.


#include "ZoransPlayerState.h"
#include "Net/UnrealNetwork.h"
#include "PlayFabHelper/Subsystem/PlayfabHelperLocalUserSubsystem.h"
#include "ZoransResistance/ZoransResistance.h"
#include "ZoransResistance/AbilitySystem/ZoransAbilitySystemComponent.h"
#include "ZoransResistance/AbilitySystem/Data/ZoransGameplayTags.h"
#include "ZoransResistance/Characters/Components/ZoransHealthComponent.h"
#include "ZoransResistance/Characters/Components/ZoransHeroComponent.h"
#include "ZoransResistance/Characters/Components/ZoransWeaponComponent.h"
#include "ZoransResistance/Game/ZoransGameState.h"

AZoransPlayerState::AZoransPlayerState()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	NetUpdateFrequency = 60.0f;

	AbilitySystemComponent = CreateDefaultSubobject<UZoransAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->ReplicationMode = EGameplayEffectReplicationMode::Mixed;
	AbilitySystemComponent->SetIsReplicated(true);

	HealthComponent = CreateDefaultSubobject<UZoransHealthComponent>(TEXT("HealthComponent"));
	HeroComponent = CreateDefaultSubobject<UZoransHeroComponent>("HeroComponent");

	WeaponComponent = CreateDefaultSubobject<UZoransWeaponComponent>(TEXT("WeaponComponent"));

	PlayerLoadouts.PlayerLoadout1.PrimaryWeapon = "Rifle";
	PlayerLoadouts.PlayerLoadout1.SecondaryWeapon = "Pistol";
	PlayerLoadouts.PlayerLoadout1.Hologram = "Drako";
	
	PlayerLoadouts.PlayerLoadout2.PrimaryWeapon = "Shotgun";
	PlayerLoadouts.PlayerLoadout2.SecondaryWeapon = "Pistol";
	PlayerLoadouts.PlayerLoadout2.Hologram = "PelvisWrexley";
	
	PlayerLoadouts.PlayerLoadout3.PrimaryWeapon = "Sniper";
	PlayerLoadouts.PlayerLoadout3.SecondaryWeapon = "Pistol";
	PlayerLoadouts.PlayerLoadout3.Hologram = "Pacho";
}

void AZoransPlayerState::BeginPlay()
{
	Super::BeginPlay();

	if (GetNetMode() < NM_Client)
	{
		if (GetWorld() && GetWorld()->GetGameState())
		{
			MatchStartTime = GetWorld()->GetGameState()->GetServerWorldTimeSeconds();

			if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
			{
				ASC->RegisterGameplayTagEvent(GameplayTag_Emote, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &AZoransPlayerState::OnEmoteTagChanged);
				ASC->RegisterGameplayTagEvent(GameplayTag_Hologram, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &AZoransPlayerState::OnHologramTagChanged);
			}
		}
	}
	
	if (GetNetMode() < NM_Client && GetPlayerController() && GetPlayerController()->IsLocalController())
	{
		if (UPlayfabHelperLocalUserSubsystem* PlayfabSystem = GetWorld()->GetGameInstance()->GetSubsystem<UPlayfabHelperLocalUserSubsystem>())
		{
			UE_LOG(ZoransLog, Warning, TEXT("AZoransPlayerState::BeginPlay -> PlayfabSystem Valid"));
			if (PlayfabSystem->GetIsPlayerAuthenticated())
			{
				UE_LOG(ZoransLog, Warning, TEXT("AZoransPlayerState::BeginPlay -> Playfab Authenticated"));
				PlayfabId = PlayfabSystem->GetPlayfabID();
				if (!PlayfabId.IsEmpty())
				{
					UE_LOG(ZoransLog, Warning, TEXT("AZoransPlayerState::BeginPlay -> Setting PlayfabId"));
					OnPlayfabIdSet(PlayfabId);
				}
			}
		}

		if (PlayfabId.IsEmpty())
		{
			const FString TravelURL = GetWorld()->URL.ToString();

			TArray<FString> OptionsArray;
	
			TravelURL.ParseIntoArray(OptionsArray, TEXT("?"), true);

			for (auto String : OptionsArray)
			{
				if (String.StartsWith(TEXT("InPlayfabId=")))
				{
					UE_LOG(ZoransLog, Warning, TEXT("ZoransPlayerController->ZoransPlayerState->InPlayfabId = %s"), *String.RightChop(12));
					PlayfabId = String.RightChop(12);
					OnPlayfabIdSet(PlayfabId);
				}
			}
		}
	}
}

void AZoransPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AZoransPlayerState, CurrentLoadoutIndex);
	
	DOREPLIFETIME_CONDITION_NOTIFY(AZoransPlayerState, TeamAssignment, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(AZoransPlayerState, PlayerKills, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(AZoransPlayerState, PlayerDeaths, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(AZoransPlayerState, PlayerAssists, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(AZoransPlayerState, PlayerObjectives, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(AZoransPlayerState, PlayfabId, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(AZoransPlayerState, PlayerProfileData, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(AZoransPlayerState, EmoteList, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(AZoransPlayerState, PlayerLoadouts, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(AZoransPlayerState, DataTags, COND_None, REPNOTIFY_Always);
	
}

void AZoransPlayerState::SetTeamAssignment(const int32 InIndex)
{
	if (GetNetMode() < NM_Client && TeamAssignment != InIndex)
	{ 
		TeamAssignment = InIndex;

		if (AZoransGameState* ZoransGameState = Cast<AZoransGameState>(GetWorld()->GetGameState()))
		{
			ZoransGameState->AssignPlayerStateToTeam(this, TeamAssignment);
		}

		OnRep_TeamAssignment();
	}
}

int32 AZoransPlayerState::GetTeamAssignment() const
{
	return TeamAssignment;
}

void AZoransPlayerState::UpdateKillScore(const int32 NewScore)
{
	if (GetNetMode() < NM_Client)
	{
		PlayerKills = NewScore;

		OnRep_PlayerKills();
	}
}

void AZoransPlayerState::UpdateDeathScore(const int32 NewScore)
{
	if (GetNetMode() < NM_Client)
	{
		PlayerDeaths = NewScore;

		OnRep_PlayerDeaths();
	}
}

void AZoransPlayerState::UpdateAssistScore(const int32 NewScore)
{
	if (GetNetMode() < NM_Client)
	{
		PlayerAssists = NewScore;

		OnRep_PlayerAssists();
	}
}

void AZoransPlayerState::UpdateObjectiveScore(const int32 NewScore)
{
	if (GetNetMode() < NM_Client)
	{
		PlayerObjectives = NewScore;

		OnRep_PlayerObjectives();
	}
}

UAbilitySystemComponent* AZoransPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent.Get();
}

UZoransHealthComponent* AZoransPlayerState::GetHealthComponent() const
{
	return HealthComponent.Get();	
}

UZoransHeroComponent* AZoransPlayerState::GetHeroComponent() const
{
	return HeroComponent.Get();
}

UZoransWeaponComponent* AZoransPlayerState::GetWeaponComponent() const
{
	return WeaponComponent.Get();
}

void AZoransPlayerState::AddPlayerAssist(AZoransPlayerState* InPlayerState, const float InDamage)
{
	if (!IsValid(InPlayerState)) return;

	const float CurrentTime = GetWorld()->GetGameState() ? GetWorld()->GetGameState()->GetServerWorldTimeSeconds() : -9.f;
	const float AssistExpirationTime = CurrentTime + AssistTime;
	
	for (auto & [Player, Damage, ExpirationTime] : CurrentPlayerAssists)
	{
		if (Player == InPlayerState)
		{
			if (ExpirationTime < CurrentTime)
			{
				Damage = InDamage;
			}
			else
			{
				Damage += InDamage;
			}
			
			ExpirationTime = AssistExpirationTime;
	
			return;
		}
	}

	FPlayerAssist NewAssist;
	NewAssist.Player = InPlayerState;
	NewAssist.Damage = InDamage;
	NewAssist.ExpirationTime = AssistExpirationTime;

	CurrentPlayerAssists.Add(NewAssist);
}

void AZoransPlayerState::ClearPlayerAssists()
{
	CurrentPlayerAssists.Empty();
}

TArray<FPlayerAssist> AZoransPlayerState::GetRelevantPlayerAssists()
{
	TArray<FPlayerAssist> OutAssists;

	const float CurrentTime = GetWorld()->GetGameState() ? GetWorld()->GetGameState()->GetServerWorldTimeSeconds() : -1.f;
	
	for (auto & [Player, Damage, ExpirationTime] : CurrentPlayerAssists)
	{
		if (ExpirationTime >= CurrentTime)
		{
			FPlayerAssist NewAssist;
			NewAssist.Player = Player;
			NewAssist.Damage = Damage;
			NewAssist.ExpirationTime = ExpirationTime;

			OutAssists.Add(NewAssist);
		}
	}

	return OutAssists;
}

FPlayerAssist AZoransPlayerState::GetHighestPlayerAssist(const float Threshold)
{
	FPlayerAssist HighestAssist = FPlayerAssist(Threshold);
	TArray<FPlayerAssist> CurrentAssists = GetRelevantPlayerAssists();
	if (!CurrentAssists.IsEmpty())
	{
		for (auto const &x : CurrentAssists)
		{
			if (x.Damage >= HighestAssist.Damage)
			{
				HighestAssist.Damage = x.Damage;
				HighestAssist.Player = x.Player;
			}
		}
	}
	
	return HighestAssist;
}

FPlayerLoadout& AZoransPlayerState::GetCurrentLoadout()
{
	switch (CurrentLoadoutIndex)
	{
	case 0:
		return PlayerLoadouts.PlayerLoadout1;
	case 1:
		return PlayerLoadouts.PlayerLoadout2;
	case 2:
		return PlayerLoadouts.PlayerLoadout3;
	default:
		return PlayerLoadouts.PlayerLoadout1;
	}
}

void AZoransPlayerState::SetLoadoutIndex(const int32 NewIndex)
{
	const int32 ClampedIndex = FMath::Clamp<int32>(NewIndex, 0, 2);
	if (ClampedIndex != CurrentLoadoutIndex)
	{
		if (GetNetMode() < NM_Client)
		{
			CurrentLoadoutIndex = ClampedIndex;
		}
		else
		{
			SRV_SetLoadoutIndex(ClampedIndex);
		}
	}
}

void AZoransPlayerState::SRV_SetLoadoutIndex_Implementation(const int32 NewIndex)
{
	SetLoadoutIndex(NewIndex);
}

bool AZoransPlayerState::HasCharacterSelectionChanged()
{
	bool bCharacterSelectionChanged = false;

	const FName CurrentCharacterSelection = GetCurrentLoadout().CharacterSelection;
	if (CurrentCharacterSelection != PreviousCharacterSelection)
	{
		//UE_LOG(ZoransLog, Warning, TEXT("CurrentCharacterSelection / PreviousCharacterSelection -> %s / %s"), *CurrentCharacterSelection.ToString(), *PreviousCharacterSelection.ToString());
		bCharacterSelectionChanged = true;
		PreviousCharacterSelection = CurrentCharacterSelection;
	}
	
	return bCharacterSelectionChanged;
}

float AZoransPlayerState::GetPlayerTimeInMatch() const
{
	float PlayTime = 0.f;

	if (GetWorld() && GetWorld()->GetGameState())
	{
		const float CurrentTime = GetWorld()->GetGameState()->GetServerWorldTimeSeconds();
		PlayTime = CurrentTime - MatchStartTime;
	}

	return PlayTime;
}

float AZoransPlayerState::GetTotalEmoteTime()
{
	if (GetWorld() && GetWorld()->GetGameState())
	{
		if (const UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
		{
			if (ASC->HasMatchingGameplayTag(GameplayTag_Emote))
			{
				OnEmoteEnded();
			}
		}
	}
	
	return TotalEmoteTime;
}

void AZoransPlayerState::OnEmoteTagChanged(const FGameplayTag GameplayTag, int32 StackCount)
{
	if (StackCount > 0)
	{
		OnEmoteActive();
	}
	else
	{
		OnEmoteEnded();
	}
}

void AZoransPlayerState::OnEmoteActive()
{
	if (GetWorld() && GetWorld()->GetGameState())
	{
		EmoteStartTime = GetWorld()->GetGameState()->GetServerWorldTimeSeconds();
		OnBeginEmote.Broadcast();
	}
}

void AZoransPlayerState::OnEmoteEnded()
{
	if (GetWorld() && GetWorld()->GetGameState())
	{
		const float CurrentTime = GetWorld()->GetGameState()->GetServerWorldTimeSeconds();
		const float AddedEmoteTime = CurrentTime - EmoteStartTime;
		EmoteStartTime = CurrentTime;

		TotalEmoteTime += AddedEmoteTime;
	}
}

float AZoransPlayerState::GetTotalHologramTime()
{
	if (GetWorld() && GetWorld()->GetGameState())
	{
		if (const UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
		{
			if (ASC->HasMatchingGameplayTag(GameplayTag_Hologram))
			{
				OnHologramEnded();
			}
		}
	}
	
	return TotalHologramTime;
}

void AZoransPlayerState::OnHologramTagChanged(const FGameplayTag GameplayTag, int32 StackCount)
{
	if (StackCount > 0)
	{
		OnHologramActive();
	}
	else
	{
		OnHologramEnded();
	}
}

void AZoransPlayerState::OnHologramActive()
{
	if (GetWorld() && GetWorld()->GetGameState())
	{
		HologramStartTime = GetWorld()->GetGameState()->GetServerWorldTimeSeconds();
	}
}

void AZoransPlayerState::OnHologramEnded()
{
	if (GetWorld() && GetWorld()->GetGameState())
	{
		const float CurrentTime = GetWorld()->GetGameState()->GetServerWorldTimeSeconds();
		const float AddedHologramTime = CurrentTime - HologramStartTime;
		HologramStartTime = CurrentTime;

		TotalHologramTime += AddedHologramTime;
	}
}

void AZoransPlayerState::OnRep_PlayerKills()
{
	if (OnPlayerScoreChange.IsBound())
	{
		OnPlayerScoreChange.Broadcast(this, EScoreStat::Kills);
	}
}

void AZoransPlayerState::OnRep_PlayerDeaths()
{
	if (OnPlayerScoreChange.IsBound())
	{
		OnPlayerScoreChange.Broadcast(this, EScoreStat::Deaths);
	}
}

void AZoransPlayerState::OnRep_PlayerAssists()
{
	if (OnPlayerScoreChange.IsBound())
	{
		OnPlayerScoreChange.Broadcast(this, EScoreStat::Assists);
	}
}

void AZoransPlayerState::OnRep_PlayerObjectives()
{
	if (OnPlayerScoreChange.IsBound())
	{
		OnPlayerScoreChange.Broadcast(this, EScoreStat::Objective);
	}
}

void AZoransPlayerState::OnRep_TeamAssignment()
{
	if (OnPlayerTeamChange.IsBound())
	{
		OnPlayerTeamChange.Broadcast();
	}
}

void AZoransPlayerState::CopyProperties(APlayerState* PlayerState)
{
	Super::CopyProperties(PlayerState);

	if (AZoransPlayerState* NewZoransPlayerState = Cast<AZoransPlayerState>(PlayerState))
	{
		NewZoransPlayerState->TeamAssignment = -1;
		NewZoransPlayerState->PlayerKills = 0;
		NewZoransPlayerState->PlayerDeaths = 0;
		NewZoransPlayerState->PlayerAssists = 0;
		NewZoransPlayerState->PlayerObjectives = 0;

		NewZoransPlayerState->CurrentPlayerAssists = {};
		
		NewZoransPlayerState->PlayfabId = PlayfabId;
		NewZoransPlayerState->CurrentLoadoutIndex = CurrentLoadoutIndex;
		NewZoransPlayerState->PlayerProfileData = PlayerProfileData;
		NewZoransPlayerState->EmoteList = EmoteList;
		NewZoransPlayerState->PlayerLoadouts = PlayerLoadouts;

		NewZoransPlayerState->DataTags = DataTags;
	}
}

void AZoransPlayerState::OnRep_PlayerProfileData()
{
	if (OnPlayerProfileDataUpdated.IsBound())
	{
		OnPlayerProfileDataUpdated.Broadcast();
	}

	PlayerProfileUpdated(PlayerProfileData);
}

void AZoransPlayerState::OnRep_EmoteList()
{
	if (OnEmoteListUpdated.IsBound())
	{
		OnEmoteListUpdated.Broadcast();
	}
}

void AZoransPlayerState::OnRep_PlayerLoadouts()
{
	if (OnPlayerLoadoutsUpdated.IsBound())
	{
		OnPlayerLoadoutsUpdated.Broadcast();
	}

	PlayerLoadoutsUpdated(PlayerLoadouts);
}

void AZoransPlayerState::OnRep_DataTags()
{
	if (OnDataTagsLoaded.IsBound())
	{
		OnDataTagsLoaded.Broadcast();
	}
}

void AZoransPlayerState::SetPlayerProfileFrameMaterial(UMaterialInstance* InFrameMaterial, float InFrameRadius)
{
	ProfileFrameMaterial = InFrameMaterial;
	ProfileFrameRadius = InFrameRadius;
}

void AZoransPlayerState::SetWeaponLoadouts(const FPlayerProfileData InProfileData, const FEmoteList InEmoteList, const FPlayerLoadout InLoadout1, const FPlayerLoadout InLoadout2, const FPlayerLoadout InLoadout3)
{
	PlayerProfileData = InProfileData;
	EmoteList = InEmoteList;
	PlayerLoadouts.PlayerLoadout1 = InLoadout1;
	PlayerLoadouts.PlayerLoadout2 = InLoadout2;
	PlayerLoadouts.PlayerLoadout3 = InLoadout3;

	OnRep_PlayerProfileData();
	OnRep_EmoteList();
	OnRep_PlayerLoadouts();
}
