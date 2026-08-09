#include "Core/OSPlayerState.h"

#include "Characters/OSCharacter.h"
#include "GAS/Components/OSAbilitySystemComponent.h"
#include "GenericTeamAgentInterface.h"
#include "Net/UnrealNetwork.h"
#include "GAS/Attributes/OSAttributeSet.h"
#include "Engine/LocalPlayer.h"
#include "AbilitySystemComponent.h"
#include "TimerManager.h"
#include "Data/OSLoadoutDataAsset.h"
#include "Data/OSGameplayTags.h"

AOSPlayerState::AOSPlayerState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bReplicates = true;
	
	SetNetUpdateFrequency(100.0f);
	
	// Create ability system component, and set it to be explicitly replicated
	AbilitySystemComponent = CreateDefaultSubobject<UOSAbilitySystemComponent>("AbilitySystemComponent");
	if (AbilitySystemComponent)
    {
		AbilitySystemComponent->SetIsReplicated(true);
		// Mixed mode means we only replicate GEs to the owning client, not to simulated proxies.
		// Attributes, GameplayTags, and GameplayCues will still replicate to everyone.
		AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	}
	// Create the attribute set, this replicates by default
	// Adding it as a subobject of the owning actor of an AbilitySystemComponent
	// automatically registers the AttributeSet with the AbilitySystemComponent
	AttributeSet = CreateDefaultSubobject<UOSAttributeSet>("AttributeSet");

}

void AOSPlayerState::BeginPlay()
{
	Super::BeginPlay();
	TryBroadcastGASReady();
}

void AOSPlayerState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(GASReadyRetryTimer);
	}
	Super::EndPlay(EndPlayReason);
}

void AOSPlayerState::TryBroadcastGASReady()
{
	if (bGASReadyBroadcast) { return; }

	UAbilitySystemComponent* PSASC = GetAbilitySystemComponent();
	if (PSASC)
	{
		OnGASReady.Broadcast(PSASC);
		bGASReadyBroadcast = true;
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(GASReadyRetryTimer);
		}
		return;
	}

	// Client: ASC replicates after PlayerState; retry on timer
	if (!HasAuthority())
	{
		GASReadyRetryElapsed += GASReadyRetryInterval;
		if (GASReadyRetryElapsed >= GASReadyRetryTimeout)
		{
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().ClearTimer(GASReadyRetryTimer);
			}
			UE_LOG(LogTemp, Warning, TEXT("OSPlayerState: ASC not ready after %.1fs. OnGASReady not broadcast."), GASReadyRetryTimeout);
			return;
		}
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(GASReadyRetryTimer, this, &AOSPlayerState::TryBroadcastGASReady, GASReadyRetryInterval, false);
		}
	}
}

UAbilitySystemComponent* AOSPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

UOSAttributeSet* AOSPlayerState::GetAttributeSet() const
{
	return AttributeSet;
}


void AOSPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(AOSPlayerState, Stats, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(AOSPlayerState, SelectedCharacterType, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME(AOSPlayerState, TeamId);
	DOREPLIFETIME(AOSPlayerState, Assists);
	DOREPLIFETIME(AOSPlayerState, SavedColor);
	DOREPLIFETIME_CONDITION(AOSPlayerState, RespawnTimeRemaining, COND_OwnerOnly);
	DOREPLIFETIME(AOSPlayerState, CurrentLoadoutIndex);
}

FGenericTeamId AOSPlayerState::GetGenericTeamId() const
{
	return TeamId;
}

void AOSPlayerState::SetGenericTeamId(const FGenericTeamId& NewTeamId)
{
	if (HasAuthority())
	{
		TeamId = NewTeamId;
		OnRep_TeamId();
	}
}

int32 AOSPlayerState::GetTeamIndex() const
{
	return TeamId.GetId();
}

void AOSPlayerState::AddKill()
{
	if (HasAuthority())
	{
		Stats.Kills++;
		OnRep_Stats();
	}
}

void AOSPlayerState::AddDeath()
{
	if (HasAuthority())
	{
		Stats.Deaths++;
		OnRep_Stats();
	}
}

void AOSPlayerState::AddAssist()
{
	if (HasAuthority())
	{
		Assists++;
	}
}

UOSAbilitySystemComponent* AOSPlayerState::GetOSAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AOSPlayerState::OnRep_Stats()
{
	OnStatsChanged.Broadcast(this);
}

void AOSPlayerState::OnRep_Assists()
{
	OnStatsChanged.Broadcast(this);
}

void AOSPlayerState::OnRep_PlayerName()
{
	Super::OnRep_PlayerName();
	OnStatsChanged.Broadcast(this);
}

void AOSPlayerState::OnRep_SavedColor()
{
	ApplyColorToSelfAvatar(SavedColor);
}

void AOSPlayerState::ApplyColorToSelfAvatar( const FLinearColor& Color ) const
{
	if (!GetPawn()) return;
	if (auto chara = Cast<AOSCharacter>(GetPawn()))
		chara->ApplyColorToMesh(Color);
}

void AOSPlayerState::OnRep_TeamId()
{
	OnTeamAssigned.Broadcast(this);
}

void AOSPlayerState::UpdateKillScore(int32 NewScore)
{
	if (GetNetMode() < NM_Client)
	{
		Stats.Kills = NewScore;
		//OnRep_Kills(0); // OldKills parameter not used, but OnRep broadcasts the delegate
		OnRep_Stats();
	}
}

void AOSPlayerState::UpdateDeathScore(int32 NewScore)
{
	if (GetNetMode() < NM_Client)
	{
		//Deaths = NewScore;
		//OnRep_Deaths(0); // OldDeaths parameter not used, but OnRep broadcasts the delegate
		
		Stats.Deaths = NewScore;
		OnRep_Stats();
	}
}

void AOSPlayerState::UpdateScore( int32 NewScore )
{
	if (GetNetMode() < NM_Client)
	{
		Stats.Score = NewScore;
		OnRep_Stats();
	}
}

FUniqueNetIdRepl AOSPlayerState::GetUniqueId() const
{
	FUniqueNetIdRepl Result = Super::GetUniqueId();
	
	// If base class returns invalid (Steam not connected), return empty but valid UniqueIdRepl
	// This prevents crashes when base class tries to access UniqueId during construction
	if (!Result.IsValid())
	{
		// Return empty UniqueIdRepl - safe for non-Steam users
		return FUniqueNetIdRepl();
	}
	
	return Result;
}

void AOSPlayerState::OnRep_SelectedCharacterType()
{
	if (APawn* P = GetPawn())
	{
		if (AOSCharacter* Char = Cast<AOSCharacter>(P))
		{
			Char->ApplyCharacterType(SelectedCharacterType);
		}
	}
}

void AOSPlayerState::SetSelectedCharacterType(EOSCharacterType NewType)
{
	if (HasAuthority())
	{
		SelectedCharacterType = NewType;
		OnRep_SelectedCharacterType();
	}
}

void AOSPlayerState::SetCurrentLoadoutIndex(int32 NewIndex)
{
	if (!HasAuthority())
	{
		ServerSetCurrentLoadoutIndex(NewIndex);
		return;
	}

	CurrentLoadoutIndex = FMath::Max(0, NewIndex);
	GiveStartupLoadout();
}

void AOSPlayerState::ServerSetCurrentLoadoutIndex_Implementation(int32 NewIndex)
{
	SetCurrentLoadoutIndex(NewIndex);
}

void AOSPlayerState::GiveStartupLoadout()
{
	if (!HasAuthority() || !AbilitySystemComponent)
	{
		return;
	}

	const FOSGameplayLoadoutDefinition* LoadoutDefinition = GetSelectedLoadoutDefinition();
	if (!LoadoutDefinition)
	{
		return;
	}

	for (const FGameplayAbilitySpecHandle& Handle : GrantedLoadoutAbilityHandles)
	{
		if (Handle.IsValid())
		{
			AbilitySystemComponent->ClearAbility(Handle);
		}
	}
	GrantedLoadoutAbilityHandles.Reset();

	for (const FActiveGameplayEffectHandle& Handle : GrantedLoadoutEffectHandles)
	{
		if (Handle.IsValid())
		{
			AbilitySystemComponent->RemoveActiveGameplayEffect(Handle);
		}
	}
	GrantedLoadoutEffectHandles.Reset();

	if (CachedLoadoutLooseTags.Num() > 0)
	{
		AbilitySystemComponent->RemoveLooseGameplayTags(CachedLoadoutLooseTags);
		CachedLoadoutLooseTags.Reset();
	}

	// GA_OSDeath applies GE_OSDeathState to this PlayerState-owned ASC; it survives pawn destruction.
	// Clear IsDead before re-granting loadout so new-life abilities are not blocked (InstancedPerActor GA_OSDeath).
	const FOSGameplayTags& OSTags = FOSGameplayTags::Get();
	if (OSTags.IsDead.IsValid())
	{
		FGameplayTagContainer GrantedDeathTag;
		GrantedDeathTag.AddTag(OSTags.IsDead);
		AbilitySystemComponent->RemoveActiveEffectsWithGrantedTags(GrantedDeathTag);
		AbilitySystemComponent->RemoveLooseGameplayTag(OSTags.IsDead);
	}

	AActor* const AvatarActor = GetPawn();
	LoadoutDefinition->GiveToAbilitySystemComponent(
		AbilitySystemComponent,
		AvatarActor,
		this,
		GrantedLoadoutAbilityHandles,
		GrantedLoadoutEffectHandles,
		CachedLoadoutLooseTags,
		1.f);
}

const FOSGameplayLoadoutDefinition* AOSPlayerState::GetSelectedLoadoutDefinition() const
{
	if (LoadoutPresets.IsValidIndex(CurrentLoadoutIndex) && IsValid(LoadoutPresets[CurrentLoadoutIndex]))
	{
		return &LoadoutPresets[CurrentLoadoutIndex]->Definition;
	}

	if (IsValid(DefaultLoadoutPreset))
	{
		return &DefaultLoadoutPreset->Definition;
	}

	return nullptr;
}

void AOSPlayerState::SetRandomizedCharacterColor( const TArray<FLinearColor>& ColorsToUse )
{
	if (ColorsToUse.IsEmpty()) return;
	int i = FMath::RandRange(0, ColorsToUse.Num() - 1);
	const FLinearColor& Color = ColorsToUse[i];
	Server_SetCharacterColor(Color);
}

void AOSPlayerState::Server_SetCharacterColor_Implementation(const FLinearColor& Color)
{
	if (!HasAuthority()) return;
	SavedColor = Color;
	OnRep_SavedColor();
}

void AOSPlayerState::SetRespawnCountdown(float Duration)
{
	RespawnTimeRemaining = Duration;
	ForceNetUpdate();
	OnRep_RespawnCountdown();
}

void AOSPlayerState::ClearRespawnCountdown()
{
	RespawnTimeRemaining = -1.f;
	/*if (auto pawn = GetPawn())
	{
		if (auto chara = Cast<AOSCharacter>(pawn))
			chara->ResetAttributes();
	}*/
	ForceNetUpdate();
	OnRep_RespawnCountdown();
}

void AOSPlayerState::OnRep_RespawnCountdown()
{
	OnRespawnCountdownChanged.Broadcast(RespawnTimeRemaining);
}
