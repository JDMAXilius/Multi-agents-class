// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Pawn.h"
#include "ZoransResistance/Game/ZoransGameState.h"
#include "ZoransLatentPlayerPawn.generated.h"

UCLASS()
class ZORANSRESISTANCE_API AZoransLatentPlayerPawn : public APawn, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:

	AZoransLatentPlayerPawn();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override { return AbilitySystemComponent.Get(); }
	
	UFUNCTION(BlueprintCallable)
	void FinishSpawnSequence();
	
	virtual void BeginPlay() override;
	
	void CheckGameState();

	UFUNCTION()
	void SpawnPlayerCharacter(const AZoransGameState* ZoransGameState);

	UFUNCTION(Server, Reliable)
	void Server_RequestRespawn();

	void RequestRespawn();

protected:

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Character")
	UDataTable* CharacterDataTable = nullptr;
	
	virtual void PossessedBy(AController* NewController) override;

	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;
};