#pragma once
// Debug CheatManager: centralizes all non-shipping debug tools (GAS cheats, visual overlays, punching bag, character swap).
// UHT constraint: UFUNCTION/UPROPERTY cannot be inside #if guards. Declarations exist in all builds; implementations are guarded.

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "GameplayTagContainer.h"
#include "OSLogCategories.h"
#include "OSCheatManager.generated.h"

class UOSAbilitySystemComponent;

UCLASS()
class ONSIGHT_API UOSCheatManager : public UCheatManager
{
	GENERATED_BODY()

public:
	virtual void InitCheatManager() override;
	
	UPROPERTY(BlueprintReadOnly)
	APlayerController* CachedPlayerController;

	// --- Menu Toggle ---

	UFUNCTION(Exec, BlueprintCallable)
	void ToggleDebugMenu();

	// --- Player Cheats ---

	UFUNCTION(Exec)
	void NoHealthDamage();

	UFUNCTION(Exec)
	void NoStaminaDrain();

	UFUNCTION(Exec)
	void BlockHealthRegen();

	UFUNCTION(Exec)
	void BlockStaminaRegen();

	UFUNCTION(Exec)
	void ToggleHitReactRetrigger();

	// --- Visual Overlays ---

	UFUNCTION(Exec)
	void ShowHealthBars();

	UFUNCTION(Exec)
	void ShowNameplates();

	UFUNCTION(Exec)
	void ShowMantleDebug();

	UFUNCTION(Exec)
	void ShowAttackTraceDebug();

	UFUNCTION(Exec)
	void ShowRotationDebug();

	UFUNCTION(Exec)
	void ShowTargetingDebug();

	UFUNCTION(Exec)
	void ShowGrabDebug();

	UFUNCTION(BlueprintImplementableEvent)
	void OnToggleDebugCamera(APlayerController* PreviousController);
	
	void ToggleDebugCameraFromUI();

	// --- Punching Bag ---

	UFUNCTION(Exec)
	void TogglePunchingBag();

	// --- Bag Controller (cheats applied to bag's ASC) ---

	UFUNCTION(Exec)
	void TogglePunchingBagBlock();

	UFUNCTION(Exec)
	void ToggleBagNoHealthDamage();

	UFUNCTION(Exec)
	void ToggleBagNoStaminaDrain();

	UFUNCTION(Exec)
	void ToggleBagBlockHealthRegen();

	UFUNCTION(Exec)
	void ToggleBagBlockStaminaRegen();

	UFUNCTION(Exec)
	void ToggleBagHitReactRetrigger();

	UFUNCTION(Exec)
	void BagLightAttack();

	UFUNCTION(Exec)
	void BagHeavyAttack();

	UFUNCTION(Exec)
	void BagDodge();

	// --- Domination ---

	UFUNCTION(Exec)
	void DomForceCapture(FString PointName, int32 TeamIndex);

	UFUNCTION(Exec)
	void DomSetTeamScore(int32 TeamIndex, int32 Score);

	UFUNCTION(Exec)
	void DomForceWin(int32 TeamIndex);

	UFUNCTION(Exec)
	void DomResetPoint(FString PointName);

	UFUNCTION(Exec)
	void DomSetScoreLimit(int32 NewLimit);

	UFUNCTION(Exec)
	void DomStatus();

	// --- Host Menu ---

	UFUNCTION(Exec)
	void ShowHostMenu();

	UFUNCTION(Exec)
	void ShowCommonHostMenu();

	// --- Character Swap ---

	UFUNCTION(Exec)
	void SwapCharacter(int32 Index);

	/** Re-apply active debug GEs to the current pawn's ASC (e.g. after character swap). */
	void ReapplyDebugState();

	/** Class spawned by the debug cheat menu. Defaults to BP_OSPunchingBag so the dummy's
	 *  mesh / AnimBP / collision are authored in the BP. Falls back to the raw C++ class
	 *  if the soft reference can't resolve. UPROPERTY must live outside the shipping guard;
	 *  only the usage sites in AOSPlayerController are debug-gated. */
	UPROPERTY(EditAnywhere, Category = "OnSight|Debug")
	TSoftClassPtr<class AOSPunchingBag> PunchingBagClass {
		FSoftObjectPath(TEXT("/Game/Blueprints/Character/BP_OSPunchingBag.BP_OSPunchingBag_C"))
	};

#if !UE_BUILD_SHIPPING
	/** Authoritative reference to the spawned punching bag.
	 *  Set by AOSPlayerController on the server. */
	TWeakObjectPtr<AActor> PunchingBagActor;
#endif

private:
#if !UE_BUILD_SHIPPING
	UOSAbilitySystemComponent* GetPlayerASC() const;

	void BuildDebugMenuWidget();
	void ShowDebugMenu();
	void HideDebugMenu();

	TSharedPtr<SWidget> DebugMenuWidget;
	TSharedPtr<SWidget> DebugMenuWeakWrapper;
	bool bDebugMenuVisible = false;

	UOSAbilitySystemComponent* GetBagASC() const;
	void BagAction(FGameplayTag AbilityTag);
	void StopBagActionLoop();

	// Per-player cheat state. NOT global CVars — each player's CheatManager
	// tracks its own GE toggles so listen server works with multiple players.
	bool bNoHealthDamage = false;
	bool bNoStaminaDrain = false;
	bool bBlockHealthRegen = false;
	bool bBlockStaminaRegen = false;
	bool bHitReactRetrigger = false;

	// Per-bag cheat state.
	bool bBagNoHealthDamage = false;
	bool bBagNoStaminaDrain = false;
	bool bBagBlockHealthRegen = false;
	bool bBagBlockStaminaRegen = false;
	bool bBagHitReactRetrigger = false;

	// Bag action loop state.
	bool bLoopActions = false;
	float BagActionLoopInterval = 2.0f;
	FTimerHandle BagActionLoopHandle;
	FGameplayTag LoopingActionTag;

	/** When spawning: checked = bag on enemy TDM team; unchecked = same team as you (friendly fire should block damage). */
	bool bPunchingBagEnemyTeam = true;
	
	bool bDebugCameraOn = false;
#endif

	UPROPERTY()
	TObjectPtr<UUserWidget> HostMenuWidget;
};
