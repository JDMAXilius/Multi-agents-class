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
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

	UClass* GetCurrentWeaponAnimLayer() const;
	UClass* ResolveAnimLayerClass();

protected:
	// Camera height above the capsule centre, standing. Crouch shifts it down by the
	// capsule's own shrink, otherwise the view floats above the crouched head.
	static constexpr float CameraStandingHeight = 64.f;

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
