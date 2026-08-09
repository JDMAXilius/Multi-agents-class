#pragma once
// Player pawn: camera boom + follow camera, overrides InitializeGAS. Use for human-controlled character.

#include "CoreMinimal.h"
#include "Characters/OSCharacter.h"
#include "OSPlayer.generated.h"

class AOSPlayerController;
class USpringArmComponent;
class UCameraComponent;



UCLASS()
class ONSIGHT_API AOSPlayer : public AOSCharacter
{
	GENERATED_BODY()

public:
	AOSPlayer(const FObjectInitializer& ObjectInitializer);
	
	AOSPlayerController* GetOSPlayerController();
	
	bool IsDead() const;

	UFUNCTION(BlueprintCallable, Category="Loadout")
	void RequestSetLoadoutIndex(int32 NewIndex);

	FORCEINLINE USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	FORCEINLINE UCameraComponent* GetFollowCamera() const { return FollowCamera; }
protected:
	virtual void BeginPlay() override;
	virtual void InitializeGAS() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

	// ========================================
	// CAMERA SYSTEM
	// ========================================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FollowCamera;
	
private:
	void InitAbilitySystemComponent();

	UFUNCTION(Server, Reliable)
	void ServerRequestSetLoadoutIndex(int32 NewIndex);
	
	UPROPERTY()
	TObjectPtr<AOSPlayerController> PlayerController;

};

