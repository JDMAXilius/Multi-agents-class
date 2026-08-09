// Copyright Zollpa LLC.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameplayEffectTypes.h"
#include "GameFramework/PlayerState.h"
#include "ZoransResistance/AbilitySystem/ZoransAbilitySystemComponent.h"
#include "ZoransResistance/Characters/Components/ComponentInterface.h"
#include "ZoransResistance/Teams/TeamStateInterface.h"
#include "ZoransResistance/Teams/Data/TeamData.h"
#include "ZoransResistance/Player/Data/LocalPlayerData.h"
#include "ZoransPlayerState.generated.h"

class UZoransGameplayAbility;
class UMovementAttributes;
class UHealthAttributes;
class UCharacterAttributes;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPlayerScoreChangeDelegate, AZoransPlayerState*, InPlayerState, EScoreStat, ChangedScore);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPlayerTeamChangeDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPlayerProfileDataUpdated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FEmoteListUpdated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPlayerLoadoutsUpdated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPlayerProfileFrameLoaded, UMaterialInstance*, FrameMaterial, float, Radius);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDataTagsLoaded);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FModularZoranMergeComplete);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPlayerBeginEmote);


USTRUCT(BlueprintType)
struct FPlayerAssist
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Assist")
	AZoransPlayerState* Player = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Assist")
	float Damage = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Assist")
	float ExpirationTime = 0.f;

	FPlayerAssist() {}

	FPlayerAssist(float InDamage)
	{
		Damage = InDamage;
	}
	
};

UCLASS()
class ZORANSRESISTANCE_API AZoransPlayerState : public APlayerState, public IAbilitySystemInterface, public ITeamStateInterface, public IComponentInterface
{
	GENERATED_BODY()

public:
	AZoransPlayerState();

	virtual void BeginPlay() override;
	
	virtual void SetTeamAssignment(int32 InIndex) override;

	virtual int32 GetTeamAssignment() const override;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = "OnRep_PlayerKills")
	int32 PlayerKills = 0;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = "OnRep_PlayerDeaths")
	int32 PlayerDeaths = 0;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = "OnRep_PlayerAssists")
	int32 PlayerAssists = 0;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = "OnRep_PlayerObjectives")
	int32 PlayerObjectives = 0;
	
	UPROPERTY(BlueprintReadWrite, ReplicatedUsing = "OnRep_DataTags")
	TArray<FName> DataTags;
	
	UPROPERTY(BlueprintAssignable)
	FPlayerScoreChangeDelegate OnPlayerScoreChange;

	UPROPERTY(BlueprintAssignable)
	FPlayerTeamChangeDelegate OnPlayerTeamChange;

	UPROPERTY(BlueprintAssignable)
	FPlayerLoadoutsUpdated OnPlayerProfileDataUpdated;

	UPROPERTY(BlueprintAssignable)
	FPlayerLoadoutsUpdated OnEmoteListUpdated;
	
	UPROPERTY(BlueprintAssignable)
	FPlayerLoadoutsUpdated OnPlayerLoadoutsUpdated;

	UPROPERTY(BlueprintCallable, BlueprintAssignable)
	FPlayerProfileFrameLoaded OnPlayerProfileFrameLoaded;

	UPROPERTY(BlueprintCallable, BlueprintAssignable)
	FModularZoranMergeComplete OnModularZoranMergeComplete;

	UPROPERTY(BlueprintAssignable)
	FDataTagsLoaded OnDataTagsLoaded;

	UPROPERTY(BlueprintAssignable)
	FPlayerBeginEmote OnBeginEmote;
		
	void UpdateKillScore(int32 NewScore);

	void UpdateDeathScore(int32 NewScore);

	void UpdateAssistScore(int32 NewScore);

	void UpdateObjectiveScore(int32 NewScore);
	
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintCallable, BlueprintPure)
	UZoransAbilitySystemComponent* GetZoransAbilitySystemComponent() { return AbilitySystemComponent.Get(); }
	
	virtual UZoransHealthComponent* GetHealthComponent() const override;

	virtual UZoransHeroComponent* GetHeroComponent() const override;

	virtual UZoransWeaponComponent* GetWeaponComponent() const override;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Playfab")
	FString PlayfabId = FString();

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Assists")
	float AssistTime = 3.f;

	UFUNCTION(BlueprintAuthorityOnly, BlueprintCallable, Category = "Assists")
	void AddPlayerAssist(AZoransPlayerState* InPlayerState, const float InDamage);

	UFUNCTION(BlueprintAuthorityOnly, BlueprintCallable, Category = "Assists")
	void ClearPlayerAssists();

	UFUNCTION(BlueprintAuthorityOnly, BlueprintCallable, Category = "Assists")
	TArray<FPlayerAssist> GetAllPlayerAssists() { return CurrentPlayerAssists; }

	UFUNCTION(BlueprintAuthorityOnly, BlueprintCallable, Category = "Assists")
	TArray<FPlayerAssist> GetRelevantPlayerAssists();

	UFUNCTION(BlueprintAuthorityOnly, BlueprintCallable, Category = "Assists")
	FPlayerAssist GetHighestPlayerAssist(const float Threshold = -1.f);

	UFUNCTION(BlueprintAuthorityOnly, BlueprintImplementableEvent, Category = "Loadout System")
	void OnPlayfabIdSet(const FString &NewPlayfabId);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "LoadoutSystem")
	UMaterialInstance* GetPlayerProfileFrameMaterial() { return ProfileFrameMaterial; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "LoadoutSystem")
	float GetPlayerProfileFrameRadius() { return ProfileFrameRadius; }

	UFUNCTION(BlueprintCallable, Category = "Loadout System")
	void SetPlayerProfileFrameMaterial(UMaterialInstance* InFrameMaterial, const float InFrameRadius = 1.f);

	UFUNCTION(BlueprintAuthorityOnly, BlueprintCallable, Category = "Weapon System")
	void SetWeaponLoadouts(const FPlayerProfileData InProfileData, const FEmoteList InEmoteList, const FPlayerLoadout InLoadout1, const FPlayerLoadout InLoadout2, const FPlayerLoadout InLoadout3);
		
	UFUNCTION(BlueprintCallable, Category = "Loadout System")
	FPlayerLoadout& GetCurrentLoadout();

	UFUNCTION(BlueprintCallable, Category = "Loadout System")
	void SetLoadoutIndex(const int32 NewIndex);

	UFUNCTION(Server, Reliable)
	void SRV_SetLoadoutIndex(const int32 NewIndex);

	bool HasCharacterSelectionChanged();

	UFUNCTION(BlueprintImplementableEvent, Category = "Loadout System")
	void PlayerProfileUpdated(const FPlayerProfileData& UpdatedPlayerProfile);
	
	UFUNCTION(BlueprintImplementableEvent, Category = "Loadout System")
	void PlayerLoadoutsUpdated(const FPlayerLoadoutContainer& UpdatedPlayerLoadouts);

	UFUNCTION(BlueprintCallable, BlueprintPure, BlueprintAuthorityOnly, Category = "End Of Match")
	float GetPlayerTimeInMatch() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, BlueprintAuthorityOnly, Category = "End Of Match")
	float GetTotalEmoteTime();

	void OnEmoteTagChanged(const FGameplayTag GameplayTag, int32 StackCount);
	void OnEmoteActive();
	void OnEmoteEnded();

	UFUNCTION(BlueprintCallable, BlueprintPure, BlueprintAuthorityOnly, Category = "End Of Match")
	float GetTotalHologramTime();

	void OnHologramTagChanged(const FGameplayTag GameplayTag, int32 StackCount);
	void OnHologramActive();
	void OnHologramEnded();

	UFUNCTION(BlueprintImplementableEvent, BlueprintAuthorityOnly, Category = "Player Initialization")
	void GrantModularCharacterAbilities();

	UFUNCTION(BlueprintImplementableEvent, BlueprintAuthorityOnly, Category = "Player Initialization")
	void InitializeCharacter();

protected:
	UFUNCTION()
	void OnRep_PlayerKills();

	UFUNCTION()
	void OnRep_PlayerDeaths();

	UFUNCTION()
	void OnRep_PlayerAssists();
	
	UFUNCTION()
	void OnRep_PlayerObjectives();

	UFUNCTION()
	void OnRep_PlayerProfileData();

	UFUNCTION()
	void OnRep_EmoteList();

	UFUNCTION()
	void OnRep_PlayerLoadouts();

	UFUNCTION()
	void OnRep_DataTags();
	
	float MatchStartTime = 0.f;

	float EmoteStartTime = 0.f;
	float TotalEmoteTime = 0.f;

	float HologramStartTime = 0.f;
	float TotalHologramTime = 0.f;
	
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	TSoftObjectPtr<UZoransAbilitySystemComponent> AbilitySystemComponent = nullptr;
	
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Ability System")
	TObjectPtr<UZoransHealthComponent> HealthComponent = nullptr;
	
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Ability System")
	TObjectPtr<UZoransHeroComponent> HeroComponent = nullptr;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Weapon System")
	TObjectPtr<UZoransWeaponComponent> WeaponComponent = nullptr;
		
	UPROPERTY(ReplicatedUsing = "OnRep_TeamAssignment")
	int32 TeamAssignment = -1;
	
	UFUNCTION()
	void OnRep_TeamAssignment();

	virtual void CopyProperties(APlayerState* PlayerState) override;
	
	UPROPERTY()
	TArray<FPlayerAssist> CurrentPlayerAssists;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Loadout System")
	int32 CurrentLoadoutIndex = 0;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = "OnRep_PlayerProfileData", Category = "Loadout System")
	FPlayerProfileData PlayerProfileData;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = "OnRep_EmoteList", Category = "Loadout System")
	FEmoteList EmoteList;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = "OnRep_PlayerLoadouts", Category = "Loadout System")
	FPlayerLoadoutContainer PlayerLoadouts;

	UPROPERTY()
	UMaterialInstance* ProfileFrameMaterial = nullptr;
	float ProfileFrameRadius = 1.f;
	
	FName PreviousCharacterSelection = "HerfDerf";
};