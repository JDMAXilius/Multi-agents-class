#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayEffectTypes.h"
#include "UObject/SoftObjectPath.h"
#include "BNCharacter.generated.h"

class UBNEquipmentComponent;
class UBNHealthComponent;
class UCameraComponent;
struct FOnAttributeChangeData;

UCLASS(Config=Game)
class BREACHPOINTNEXT_API ABNCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ABNCharacter();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

	UClass* GetCurrentWeaponAnimLayer() const;
	UClass* ResolveAnimLayerClass();

	UBNEquipmentComponent* GetEquipmentComponent() const { return EquipmentComponent; }

	/** Links the resolved layer on the 3P mesh. Guards for a missing anim instance and links
	 *  once per class; public so the anim instance can re-trigger it — the character owns it. */
	void InitializeAnimLayer();

protected:
	/** Bone/socket the camera rides on the mesh. True first person: the animated body carries
	 *  the view, so crouch and head motion need no manual camera offset. */
	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	FName CameraAttachSocket = TEXT("head");

	UPROPERTY(Config)
	FSoftClassPath UnarmedAnimLayer;

	UPROPERTY(Transient)
	TObjectPtr<UClass> CachedUnarmedAnimLayer;

	bool bUnarmedAnimLayerResolveAttempted = false;

	/** The layer class currently linked on the 3P mesh; the character is the ONE owner of linking. */
	UPROPERTY(Transient)
	TObjectPtr<UClass> LinkedAnimLayerClass;

	void InitializeAbilitySystem();
	void OnMoveSpeedChanged(const FOnAttributeChangeData& Data);

	/** The health component's verdict, turned into the death VERB on the authority. The component
	 *  knows nothing about abilities and the ability knows nothing about attributes. */
	void HandleDeath(UBNHealthComponent* Component);

	/** State.Movement.Crouching GE, applied by OnStartCrouch on the authority — the ONE owner
	 *  of the crouch tag; abilities only drive the engine crouch. */
	FActiveGameplayEffectHandle CrouchStateHandle;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UCameraComponent> CameraComponent;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UBNEquipmentComponent> EquipmentComponent;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UBNHealthComponent> HealthComponent;

	FDelegateHandle MoveSpeedChangedHandle;

	/** Cached at init. APawn::UnPossessed() nulls PlayerState before the corpse is destroyed,
	 *  so EndPlay cannot reach the ASC through a fresh GetPlayerState<>() lookup. */
	TWeakObjectPtr<UAbilitySystemComponent> CachedAbilitySystem;
};
