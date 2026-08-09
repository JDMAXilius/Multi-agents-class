// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"
#include "Characters/Data/CharacterData.h"
#include "Components/NMComponentInterface.h"
#include "NMCharacterBase.generated.h"

class ANMPlayerState;
class UCameraComponent;
class UNMHealthComponent;
class UNMWeaponComponent;
class USpringArmComponent;

// Delegate to notify about tag changes
DECLARE_MULTICAST_DELEGATE_OneParam(FOnGameplayTagChanged, const FSGameplayTag);

UCLASS(NotBlueprintable, Abstract)
class NEWMOONS_API ANMCharacterBase : public ACharacter, public INMComponentInterface
{
	GENERATED_BODY()

public:
	ANMCharacterBase(const FObjectInitializer& ObjectInitializer);
	
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Properties | Camera")
	UCameraComponent* GetCamera() const
	{
		return CameraComponent.Get();
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Properties | Character")
	ANMPlayerState* GetNMPlayerState();
	
	/** Returns Health Component sub-object */
	FORCEINLINE virtual UNMHealthComponent* GetHealthComponent() const override { return HealthComponent; }

	/** Returns Weapon Component sub-object */
	FORCEINLINE virtual UNMWeaponComponent* GetWeaponComponent() const override { return WeaponComponent; }
	
	UPROPERTY(VisibleAnywhere, ReplicatedUsing = OnRep_GameplaytoTag, Category = "Properties | GameplayTags")
	FSGameplayTag GameplaytoTag; //TODO:Replication client to server

	FOnGameplayTagChanged OnGameplayTagChanged;

	void ScheduleFinishDeath();
	// Enable ragdoll physics
	void EnableRagdoll();
	FVector CalculateDeathImpulse();

	UFUNCTION()
	void CharacterHitReaction(const FVector InDirection, const FVector HitLocation);
	
	UFUNCTION()
	void OnHealthChanged(UNMHealthComponent* inHealthComponent, float OldValue, float NewValue, float ScalarValue,AActor* DamageInstigator, AActor* DamageCauser);
	
protected:
	virtual void PossessedBy(AController* NewController) override;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Properties | Camera")
	TObjectPtr<USpringArmComponent> SpringArmComponent = nullptr;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Properties | Camera")
	TObjectPtr<UCameraComponent> CameraComponent = nullptr;

	TWeakObjectPtr<ANMPlayerState> NMPlayerState;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Properties | Components")
	UNMHealthComponent* HealthComponent = nullptr;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Properties | Components")
	UNMWeaponComponent* WeaponComponent = nullptr;
	
	/** The hand socket, used for location to spawn the weapon at */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Properties | Properties | Combat", meta = (AllowPrivateAccess = "true"))
	FName RightHandSocket;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Property | Combat")
	float DeathTime = 4.0f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Properties | Properties | Combat", meta = (AllowPrivateAccess = "true"))
	float DeathTtime = 4.f ;
	
	// Death animation montage
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Properties | Animation")
	TObjectPtr<UAnimMontage> DeathMontage = nullptr;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Properties | Animation")
	TObjectPtr<UAnimMontage> HitReactMontage = nullptr;
	
	// Begins the death sequence for the character (disables collision, disables movement, etc...)
	UFUNCTION()
	virtual void OnDeathStarted(APlayerState* KillerState, APlayerState* VictimState);

	// Ends the death sequence for the character (detaches controller, destroys pawn, etc...)
	UFUNCTION()
	virtual void OnDeathFinished();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayAnimation(UAnimMontage* AnimMontage, const FName& InFunctionName);
	
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;
	
	virtual float InternalTakePointDamage(float Damage, FPointDamageEvent const& PointDamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	UFUNCTION()
	void OnRep_GameplaytoTag();
	
private:

	// Called after the death animation montage ends
	UFUNCTION()
	void OnDeathMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	// Called after possessing the character
	UFUNCTION(Client, Reliable)
	void ClientShowInGameMenu(AController* NewController);
};
