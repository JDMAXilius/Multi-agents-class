#pragma once

#include "AbilitySystemInterface.h"
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "UObject/SoftObjectPtr.h"

#include "BPFPSCharacter.generated.h"

class UAbilitySystemComponent;
class UInputAction;
struct FInputActionValue;

/**
 * A reparent target for `BP_FPST_Character`, and deliberately almost nothing else.
 *
 * WHY IT CREATES NO COMPONENTS, which is the entire design and the only thing to preserve.
 * `BP_FPST_Character` parents to `/Script/Engine.Character` and brings its OWN tree:
 * `CapsuleComponent`, `CharacterMesh0`, `CameraComponent`, `SpringArmComponent`, `CameraBoom`,
 * `FollowCamera`. A C++ parent that also spawned a camera would leave the pawn with two, and
 * `APawn::CalcCamera` takes the first `UCameraComponent` it finds — so which one you look
 * through would depend on component search order, not on intent. The same argument applies to a
 * second skeletal mesh. `ABPCharacter` creates both and is therefore the wrong parent for this
 * Blueprint; it is the right parent for a pawn whose tree is defined in C++.
 *
 * So the constructor sets one flag and touches nothing else. The template's camera-mode toggle,
 * its spring-arm lag, its recoil curves and its mesh assignments all keep working because
 * nothing here has an opinion about them.
 *
 * WHAT IT ADDS to a pure Blueprint character: the ASC lookup so the pawn is legible to the BP
 * stack (`ABPGameMode` / `ABPPlayerController` / `ABPPlayerState`), and Move/Look bound in C++
 * from soft config paths so movement does not depend on an editor graph. Jump and the ability
 * verbs stay on `ABPPlayerController` — same split as `ABPCharacter`, so the two pawns are
 * interchangeable from the controller's side.
 *
 * No Tick (law 4). No asset named in C++ (law 3).
 */
UCLASS(config = Game, meta = (DisplayName = "BP FPS Character (reparent target)"))
class BREACHPOINT_API ABPFPSCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:

	ABPFPSCharacter();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

	/** Idempotent: possession runs again on respawn and on seamless travel. */
	void InitializeAbilitySystem();

	UFUNCTION(BlueprintCallable, Category = "Properties | Input")
	virtual void DoAim(float Yaw, float Pitch);

	UFUNCTION(BlueprintCallable, Category = "Properties | Input")
	virtual void DoMove(float Right, float Forward);

protected:

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	void MoveInput(const FInputActionValue& Value);
	void LookInput(const FInputActionValue& Value);

	/**
	 * Soft and config-pinned, same section shape as `ABPCharacter` so the two can be swapped by
	 * changing one line of `DefaultPawnClassOverride` without touching any other config.
	 */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Properties | Input")
	TSoftObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, Config, Category = "Properties | Input")
	TSoftObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, Config, Category = "Properties | Input")
	TSoftObjectPtr<UInputAction> MouseLookAction;

private:

	/** Unset is a warning, set-but-unloadable is an error. Silence is what makes input bugs cost days. */
	static const UInputAction* ResolveAction(const TSoftObjectPtr<UInputAction>& SoftAction, const TCHAR* Name);
};
