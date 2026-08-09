// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Characters/Components/NMComponentInterface.h"
#include "GameFramework/PlayerState.h"
#include "NMPlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPlayerScoreChangeDelegate, ANMPlayerState*, InPlayerState);

UCLASS()
class NEWMOONS_API ANMPlayerState : public APlayerState, public INMComponentInterface
{
	GENERATED_BODY()

public:
	ANMPlayerState();
	
	virtual UNMHealthComponent* GetHealthComponent() const override;

	virtual UNMWeaponComponent* GetWeaponComponent() const override;

	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const override;

	virtual void OnRep_Score() override;

	int32 GetKills() const {return PlayerKills;}
	int32 GetDeaths() const {return PlayerDeaths;}
	
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = "OnRep_PlayerKills")
	int32 PlayerKills = 0;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = "OnRep_PlayerDeaths")
	int32 PlayerDeaths = 0;

	UPROPERTY(BlueprintAssignable)
	FPlayerScoreChangeDelegate OnPlayerScoreChange;
	
	void UpdateKillScore(int32 NewScore);

	void UpdateDeathScore(int32 NewScore);
	
protected:
                      
	UFUNCTION()
	void OnRep_PlayerKills();
            
	UFUNCTION()
	void OnRep_PlayerDeaths();

	float MatchStartTime = 0.f;
	
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Attribute System")
	TObjectPtr<UNMHealthComponent> HealthComponent = nullptr;
	
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Weapon System")
	TObjectPtr<UNMWeaponComponent> WeaponComponent = nullptr;
};
