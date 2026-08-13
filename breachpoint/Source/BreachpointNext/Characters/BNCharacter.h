#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayEffectTypes.h"
#include "UObject/SoftObjectPath.h"
#include "BNCharacter.generated.h"

class UCameraComponent;
class USkeletalMeshComponent;
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

	/** Links the resolved layer on the 3P mesh. Guards for a missing anim instance and links
	 *  once per class; public so the anim instance can re-trigger it — the character owns it. */
	void InitializeAnimLayer();

protected:
	// Camera height above the capsule centre, standing. Crouch shifts it down by the
	// capsule's own shrink, otherwise the view floats above the crouched head.
	static constexpr float CameraStandingHeight = 64.f;

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

	/** State.Movement.Crouching GE, applied by OnStartCrouch on the authority — the ONE owner
	 *  of the crouch tag; abilities only drive the engine crouch. */
	FActiveGameplayEffectHandle CrouchStateHandle;

	// The two-mesh FPS standard, per the founder's MyCharacter reference: capsule → camera →
	// Mesh1P (owner-only arms), while the inherited 3P Mesh is owner-no-see. Same subobject
	// and property name "Mesh1P" on purpose — a BP child must never create its own (the SCS
	// naming trap: a BP component may not shadow a parent C++ property).
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UCameraComponent> CameraComponent;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USkeletalMeshComponent> Mesh1P;

	FDelegateHandle MoveSpeedChangedHandle;
};
