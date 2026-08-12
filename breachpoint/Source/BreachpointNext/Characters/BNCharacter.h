#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "UObject/SoftObjectPath.h"
#include "BNCharacter.generated.h"

class UCameraComponent;
struct FOnAttributeChangeData;

UCLASS(Config=Game)
class BREACHPOINTNEXT_API ABNCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ABNCharacter();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

	UClass* GetCurrentWeaponAnimLayer() const;
	UClass* ResolveAnimLayerClass();

protected:
	UPROPERTY(Config)
	FSoftClassPath UnarmedAnimLayer;

	UPROPERTY(Transient)
	TObjectPtr<UClass> CachedUnarmedAnimLayer;

	bool bUnarmedAnimLayerResolveAttempted = false;

	void InitializeAbilitySystem();
	void OnMoveSpeedChanged(const FOnAttributeChangeData& Data);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UCameraComponent> CameraComponent;

	FDelegateHandle MoveSpeedChangedHandle;
};
