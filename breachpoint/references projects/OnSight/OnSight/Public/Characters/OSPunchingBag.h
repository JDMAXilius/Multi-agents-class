#pragma once
// Training-dummy AI character. Presentation (mesh, AnimBP, posture, collision) is authored on
// BP_OSPunchingBag so this class stays debug/gameplay-only. In non-shipping builds the bag
// auto-respawns on death and accepts a debug team override from the cheat menu.
// Architecture: behaves as a full AOSAICharacter; debug-only behavior is guarded and additive
// so a future AIController swap promotes the dummy into production use cleanly.

#include "CoreMinimal.h"
#include "GenericTeamAgentInterface.h"
#include "Characters/AI/OSAICharacter.h"
#include "OSPunchingBag.generated.h"

class AController;

UCLASS()
class ONSIGHT_API AOSPunchingBag : public AOSAICharacter
{
	GENERATED_BODY()

public:
	AOSPunchingBag(const FObjectInitializer& ObjectInitializer);
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** When true (set on spawn from debug UI), team queries use TdmDebugTeamId instead of PlayerState (bag has none). */
	virtual FGenericTeamId GetGenericTeamId() const override;

	/** Server-only: bEnemyTeam = opposite of spawner's TDM team; false = same team (friendly). FFA (NoTeam): enemy uses team 1, friendly clears debug team. */
	void SetDebugTdmTeamFromSpawner(AController* SpawnerController, bool bEnemyTeam);

protected:
	virtual void BeginPlay() override;
	virtual void Local_ResetStateForRespawn() override;

	/** RESPOND TO NETWORK: Clients must also reset physics/posture to end the ragdoll. */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_Respawn();

	/** Replicated so listen-server clients see the same team as the server for debug UI / traces. */
	UPROPERTY(Replicated)
	bool bUseTdmDebugTeam = false;

	UPROPERTY(Replicated)
	FGenericTeamId TdmDebugTeamId = FGenericTeamId::NoTeam;

	/** Seconds from death until the bag respawns. Must be >= the death montage duration or the
	 *  respawn will cancel the animation mid-play. Only consumed in non-shipping builds. */
	UPROPERTY(EditAnywhere, Category = "OnSight|Debug", meta = (ClampMin = "0.1",
		ToolTip = "Seconds between death and respawn. Keep this at or above the longest death montage length; shorter values cancel the death animation before it finishes playing."))
	float AutoRespawnDelay = 4.0f;

	/** Optional loadout applied to the bag's Character-owned ASC at BeginPlay on the server.
	 *  Without a loadout the ASC has no abilities granted, so grab reactions, hit reacts,
	 *  attack/dodge cheat-menu actions, and death montages all no-op. Set this to the same
	 *  DA_OSLoadout players use to get full behavioral parity, or to a bag-specific subset. */
	UPROPERTY(EditAnywhere, Category = "OnSight|Debug",
		meta = (ToolTip = "Loadout asset applied to this bag's ASC on spawn. Only consumed in non-shipping builds."))
	TObjectPtr<class UOSLoadoutDataAsset> DebugLoadout;

private:
#if !UE_BUILD_SHIPPING
	/** Controller-less CMC adjustment. CMC skips PerformMovement on unpossessed pawns unless
	 *  bRunPhysicsWithNoController is true, which would silently drop root motion from the
	 *  death/hit-react montages. A future AIController path would not call this. */
	void ConfigureForUnpossessed();

	void Respawn();

	/** Fires whenever IsDead is added/removed on our ASC; schedules respawn on death. Event-driven
	 *  so any death path (GA_OSDeath, direct GE apply, future AI death flow) triggers respawn
	 *  without the bag having to know about each entry point. */
	void OnIsDeadTagChanged(const FGameplayTag ChangedTag, int32 NewCount);

	FTimerHandle RespawnTimerHandle;

	/** Cached at BeginPlay — restored by Local_ResetStateForRespawn after any reset path so the
	 *  mesh returns to its authored BP relative transform regardless of what moved it. */
	FTransform InitialMeshRelativeTransform;
#endif
};
