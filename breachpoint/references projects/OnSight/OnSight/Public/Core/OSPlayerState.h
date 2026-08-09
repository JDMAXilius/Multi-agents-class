#pragma once
// PlayerState owns ASC and AttributeSet (persists across respawn). TDM: team, kills/deaths/assists. InitAbilityActorInfo(PlayerState, Character) so avatar = pawn.

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "GenericTeamAgentInterface.h"
#include "GameplayAbilitySpec.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameplayTagContainer.h"
#include "OSPlayerState.generated.h"

class UOSAbilitySystemComponent;
class UOSAttributeSet;
class UAbilitySystemComponent;
class UOSLoadoutDataAsset;
struct FOSGameplayLoadoutDefinition;

USTRUCT(BlueprintType)
struct FOSStats
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) int32 Kills  = 0;
	UPROPERTY(BlueprintReadOnly) int32 Deaths = 0;
	UPROPERTY(BlueprintReadOnly) int32 Score  = 0;
};

// TODO: Change SavedColor from FLinear to an enum/int for replication.
/** Character type selection - stored in PlayerState, replicated to all clients */
UENUM(BlueprintType)
enum class EOSCharacterType : uint8
{
	Character1	UMETA(DisplayName = "Character 1"),
	Character2	UMETA(DisplayName = "Character 2"),
	Character3	UMETA(DisplayName = "Character 3")
};
DECLARE_MULTICAST_DELEGATE_OneParam(FOnStatsChanged, AOSPlayerState*);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRespawnCountdownChanged, float, TimeRemaining);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnTeamAssigned, AOSPlayerState*);

/** Broadcast once when this PlayerState's ASC is valid (server: at BeginPlay; client: when ASC replicates). Lets consumers register tag listeners without polling. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGASReadySignature, UAbilitySystemComponent*, ASC);

UCLASS()
class ONSIGHT_API AOSPlayerState : public APlayerState, public IAbilitySystemInterface, public IGenericTeamAgentInterface
{
	GENERATED_BODY()

public:
	AOSPlayerState(const FObjectInitializer& ObjectInitializer);
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual UOSAttributeSet* GetAttributeSet() const;
	
	/** Override to provide safe fallback when Steam is not connected */
	virtual FUniqueNetIdRepl GetUniqueId() const;

	// ------------------------------------------------------------------
	// IGenericTeamAgentInterface (TDM)
	// ------------------------------------------------------------------
	virtual FGenericTeamId GetGenericTeamId() const override;
	virtual void SetGenericTeamId(const FGenericTeamId& NewTeamId) override;
	/** Team index 0, 1, ...; -1 if no team. */
	int32 GetTeamIndex() const;

	/** Update kill score (server-only, encapsulates replication logic) */
	void UpdateKillScore(int32 NewScore);

	/** Update death score (server-only, encapsulates replication logic) */
	void UpdateDeathScore(int32 NewScore);
	
	void UpdateScore(int32 NewScore);

	/** Server: increment kill (Stats.Kills). */
	void AddKill();
	/** Server: increment death (Stats.Deaths). */
	void AddDeath();
	/** Server: increment assist count. */
	void AddAssist();
	/** Typed ASC access for GAS/TDM code. */
	UOSAbilitySystemComponent* GetOSAbilitySystemComponent() const;

	// ------------------------------------------------------------------
	// Loadout authority (single source of truth)
	// ------------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "Loadout")
	int32 GetCurrentLoadoutIndex() const { return CurrentLoadoutIndex; }

	UFUNCTION(BlueprintCallable, Category = "Loadout")
	void SetCurrentLoadoutIndex(int32 NewIndex);

	UFUNCTION(Server, Reliable)
	void ServerSetCurrentLoadoutIndex(int32 NewIndex);

	/** Server: clear tracked loadout grants then apply the current preset to this player's ASC. */
	void GiveStartupLoadout();

	/** Get the selected character type (replicated) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Character Selection")
	EOSCharacterType GetSelectedCharacterType() const { return SelectedCharacterType; }

	/** Set the selected character type (server-only) */
	void SetSelectedCharacterType(EOSCharacterType NewType);
	
	UFUNCTION(BlueprintCallable)
	void SetRandomizedCharacterColor( const TArray<FLinearColor>& ColorsToUse );
	
	UPROPERTY(ReplicatedUsing=OnRep_RespawnCountdown, BlueprintReadOnly, Category="Respawn")
	float RespawnTimeRemaining = -1.f;

	UPROPERTY(BlueprintAssignable, Category="Respawn")
	FOnRespawnCountdownChanged OnRespawnCountdownChanged;

	UFUNCTION()
	void OnRep_RespawnCountdown();

	void SetRespawnCountdown(float Duration);
	void ClearRespawnCountdown();

	UPROPERTY(ReplicatedUsing=OnRep_Stats, BlueprintReadOnly, Category = "Stats")
	FOSStats Stats;

	/** Assist count (TDM). Replicated to all clients. */
	UPROPERTY(ReplicatedUsing=OnRep_Assists, BlueprintReadOnly, Category = "Stats")
	int32 Assists = 0;

	/** Selected character type - replicated to all clients, persists through respawn. OnRep pushes to pawn via AOSCharacter::ApplyCharacterType. */
	UPROPERTY(ReplicatedUsing=OnRep_SelectedCharacterType, BlueprintReadOnly, Category = "Character Selection")
	EOSCharacterType SelectedCharacterType = EOSCharacterType::Character1;
	
	/** Native delegate for stat replication updates (bind with AddUObject). */
	FOnStatsChanged OnStatsChanged;

	/** Broadcast once when ASC is ready (server immediately; client after ASC replicates). Bind with AddDynamic for camera, HUD, etc. */
	UPROPERTY(BlueprintAssignable, Category = "Ability")
	FOnGASReadySignature OnGASReady;
	
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_SavedColor, Category = "Character Selection")
	FLinearColor SavedColor;
	
	UFUNCTION()
	void OnRep_Stats();
	UFUNCTION()
	void OnRep_Assists();
	UFUNCTION()
	void OnRep_SelectedCharacterType();
	virtual void OnRep_PlayerName() override;
	
	FOnTeamAssigned OnTeamAssigned;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Client: ASC replicates after PlayerState. Retry until valid, then broadcast OnGASReady once. */
	void TryBroadcastGASReady();

	UFUNCTION()
	void OnRep_SavedColor();

	UFUNCTION()
	void ApplyColorToSelfAvatar(const FLinearColor& Color) const;
	
	UPROPERTY()
	TObjectPtr<UOSAbilitySystemComponent> AbilitySystemComponent;
	UPROPERTY()
	TObjectPtr<UOSAttributeSet> AttributeSet;

	/** Replicated selection state; grant/remove is server-only. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Loadout")
	int32 CurrentLoadoutIndex = 0;

	/** Handles managed by PlayerState to prevent duplicate grants on respawn. */
	UPROPERTY()
	TArray<FGameplayAbilitySpecHandle> GrantedLoadoutAbilityHandles;

	UPROPERTY()
	TArray<FActiveGameplayEffectHandle> GrantedLoadoutEffectHandles;

	/** Loose tags last applied from the active loadout asset; removed before next reapply. */
	UPROPERTY()
	FGameplayTagContainer CachedLoadoutLooseTags;

	/** Authoritative loadout DataAssets. CurrentLoadoutIndex selects one. */
	UPROPERTY(EditDefaultsOnly, Category="Loadout")
	TArray<TObjectPtr<UOSLoadoutDataAsset>> LoadoutPresets;

	/** Fallback when index is out of range or no preset selected. */
	UPROPERTY(EditDefaultsOnly, Category="Loadout")
	TObjectPtr<UOSLoadoutDataAsset> DefaultLoadoutPreset;

	const FOSGameplayLoadoutDefinition* GetSelectedLoadoutDefinition() const;
	
	UFUNCTION(Server, Reliable)
	void Server_SetCharacterColor(const FLinearColor& Color);

	FTimerHandle GASReadyRetryTimer;
	bool bGASReadyBroadcast = false;
	static constexpr float GASReadyRetryInterval = 0.05f;
	static constexpr float GASReadyRetryTimeout  = 3.f;
	float GASReadyRetryElapsed = 0.f;

	UPROPERTY(ReplicatedUsing=OnRep_TeamId)
	FGenericTeamId TeamId = FGenericTeamId::NoTeam;
	
	UFUNCTION()
	void OnRep_TeamId();
};

