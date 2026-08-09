#pragma once
// Player controller: forwards GetAbilitySystemComponent to PlayerState/Pawn, owns HUD and input. ASC lives on PlayerState.

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "AbilitySystemInterface.h"
#include "UI/OSUserInterfaceWidget.h"
#include "OSPlayerController.generated.h"

class UOSBaseLayerWidget;
class AOSPlayer;
class UOSHUDWidget;
class UOSCameraComponent;
class UInputMappingContext;
class UInputAction;
class UGameplayEffect;
class UOSSessionsSubsystem;
struct FInputActionValue;
struct FGameplayTag;

UCLASS()
class ONSIGHT_API AOSPlayerController : public APlayerController, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AOSPlayerController(const FObjectInitializer& ObjectInitializer);
	
	// Allow BP helpers (GetAbilitySystemComponent) to work on the controller by forwarding to PlayerState/Pawn.
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	
	AOSPlayer* GetOSPlayer() const;

	/** Chooser-based camera (preset blending from ASC tags). Attach to this PC; tick-based caching like OSCameraAutoFollowComponent. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "OnSight|Camera")
	UOSCameraComponent* GetCameraComponent() const { return CameraComponent; }

	/** Returns the MoveAction input action (for reading raw stick input outside the controller). */
	const UInputAction* GetMoveAction() const { return MoveAction; }

	/** Gets the HUD widget (local player only) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "OnSight|UI")
	UOSHUDWidget* GetHUDWidget() const { return HUDWidget; }
	
	UFUNCTION(BlueprintCallable)
	void SwapToMappingContext(UInputMappingContext* OriginalContext, UInputMappingContext* NewContext, int32 Priority);
	
	UFUNCTION(BlueprintCallable)
	void ToggleBetweenMappingContexts(UInputMappingContext* ContextA, UInputMappingContext* ContextB, bool bFromAToB, int32 Priority = 0);

	/** Server RPC: apply or remove a debug GE on the requesting player's ASC.
	 *  Client-side ASC writes don't replicate to server in Mixed mode — this ensures the server sees the tag.
	 *  WithValidation: debug RPCs are dev-only; validation rejects calls in shipping builds. */
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_ApplyDebugEffect(TSubclassOf<UGameplayEffect> EffectClass, bool bEnable);

	/** Server RPC: swap character on the server (requires GameMode access).
	 *  WithValidation: debug RPCs are dev-only; validation rejects calls in shipping builds and
	 *  sanity-bounds the Index in dev builds to prevent clamp-overflow shenanigans. */
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_SwapCharacter(int32 Index);

	/** Server RPC: toggle debug punching bag spawn on the server. bBagEnemyTeam: true = opposite TDM team, false = same team as you (friendly-fire test).
	 *  WithValidation: debug RPCs are dev-only; validation rejects calls in shipping builds and
	 *  validates SpawnLoc/SpawnRot are finite + within sane world bounds in dev. */
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_TogglePunchingBag(bool bEnable, const FVector& SpawnLoc, const FRotator& SpawnRot, bool bBagEnemyTeam);

	/** Owning client: mirror punching bag actor for debug UI (CheatManager ref is not replicated). */
	UFUNCTION(Client, Reliable)
	void Client_SetDebugPunchingBag(AActor* Bag);

	/** Server RPC: trigger an action on the punching bag (e.g. light attack). */
	UFUNCTION(Server, Reliable)
	void Server_PunchingBagAction(FGameplayTag AbilityTag);

	/** Server RPC: cancel an active ability on the bag's ASC by tag (e.g. Block).
	 *  Pairs with Server_PunchingBagAction so toggleable abilities like Block can be
	 *  live-toggled from the cheat UI without the UI having to model bag state. */
	UFUNCTION(Server, Reliable)
	void Server_PunchingBagCancelAbility(FGameplayTag AbilityTag);

	//UFUNCTION(Server, Reliable)
	//void Server_SubmitDisplayName(const FString& InName);
	
	//void OnPauseMenuPressed();
	// TODO: Refactor this further once more menus with different mapping contexts are added. This if fine until then.
	// Close Current UI Domain, returning Mapping Context to default.
	//void CloseMenuDomain();
	// Opens UI Domain with a class as its root, pushing it into the HUD's LayerStack
	//void OpenMenuDomain(TSubclassOf<UUserWidget> RootClass);
	
	void OpenMenuDomain();
	void CloseMenuDomain();
	void OnPauseMenuPressed();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void OnRep_PlayerState() override;
	virtual void BeginPlayingState() override;
	//virtual void ReceivedPlayer() override;

	// Graceful leave: host waits for session destroy, client leaves immediately
	virtual void ClientReturnToMainMenuWithTextReason_Implementation(const FText& ReturnReason) override;

	/** Creates and displays the HUD widget (local player only) */
	void CreateHUDWidget();

	/** Removes the HUD widget from viewport */
	void RemoveHUDWidget();

	// ========================================
	// INPUT (GAS-style: input -> gameplay tag -> TryActivateAbilitiesByTag)
	// ========================================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputMappingContext> DefaultMappingContext;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputMappingContext> UIMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> SprintAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> LightAttackAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> HeavyAttackAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> BlockAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> DodgeAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> TargetLockAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> Ability01;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> Ability02;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> Ability03;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> Ability04;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> TargetCycleUpAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> TargetCycleDownAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> PrimaryUtilityAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> SecondaryUtilityAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> WildcardUtilityAction;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> ScoreboardAction;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> PauseAction;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> ChargedAttackAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> GrabAction;

	/** Debug menu toggle — set in editor via IA_DebugMenu InputAction asset (e.g. Tab key).
	 *  Null-safe: if the asset isn't assigned, binding is silently skipped (same pattern as all other actions). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Debug", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> DebugMenuAction;

	// ========================================
	// HUD WIDGET
	// ========================================

	/** Class of the HUD widget to create */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OnSight|UI", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UOSHUDWidget> HUDWidgetClass;

	/** Reference to the created HUD widget */
	UPROPERTY(BlueprintReadOnly, Category = "OnSight|UI", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UOSHUDWidget> HUDWidget;
	
	void BindHUD();

private:
	// Input handlers (bound in SetupInputComponent)
	void Move(const FInputActionValue& Value);
	void OnMoveStarted(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void OnSprint();
	
	UFUNCTION(BlueprintCallable)
	void OnLightAttack();
	
	void OnHeavyAttack();
	void OnBlockStarted();
	void OnBlockCompleted();
	void OnDodge();
	void OnJump();
	void OnTargetLock();
	void OnAbility01();
	void OnAbility02();
	void OnAbility03();
	void OnAbility04();
	void OnTargetCycleUp();
	void OnTargetCycleDown();
	void OnPrimaryUtility();
	void OnSecondaryUtility();
	void OnWildcardUtility();
	void OnScoreboardPressed();
	void OnScoreboardReleased();

	// TODO DELETE: OnComboAttack/OnComboAttackHeavy removed — HandleComboInput replaces both.
	void OnChargedAttackPressed();
	void OnChargedAttackReleased();
	void OnGrab();
	void OnDebugMenu();

	/** Server RPC: replicate combo input so server can advance combo (WaitGameplayEvent only receives local events).
	 *  Carries the requested attack type so the server ability can detect cross-type switches. */
	UFUNCTION(Server, Reliable)
	void ServerComboInputPressed(const FGameplayTag& RequestedType);

	// GAS: activate/cancel by tag (no gameplay logic here)
	void ActivateAbility(const FGameplayTag& AbilityTag) const;
	void CancelAbilitiesWithTag(const FGameplayTag& AbilityTag) const;

	/** Dual-path combo input: sends Event_ComboInputPressed to feed active ability,
	 *  then activates by tag only if not already attacking. Replaces the old
	 *  separate OnComboAttack/OnComboAttackHeavy with a shared parameterized path. */
	void HandleComboInput(const FGameplayTag& AbilityTag);
	bool IsAlive() const;

	// Called once session is destroyed on the host side before returning to main menu
	UFUNCTION()
	void OnHostReadyToLeave(bool bWasSuccessful);

	UPROPERTY()
	mutable TObjectPtr<AOSPlayer> _player;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UOSCameraComponent> CameraComponent;

};


