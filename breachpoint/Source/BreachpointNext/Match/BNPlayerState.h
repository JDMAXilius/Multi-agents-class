#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "Templates/SubclassOf.h"
#include "BNPlayerState.generated.h"

class UBNAbilitySet;
class UBNAbilitySystemComponent;
class UBNAttributeSet;
class UGameplayEffect;

/** Broadcast on the AUTHORITY when this player's pawn dies. The seam law 7 asks for: the death
 *  ability announces, and whoever cares subscribes — instead of the ability reaching into the
 *  GameMode by name. The PlayerState is the broadcaster because it is the persistent object and
 *  the one the GameMode already tracks per player.
 *
 *  Victim first, then killer. Killer may be NULL (world damage, disconnected mid-flight) and may
 *  EQUAL the victim (their own grenade) — both are real cases the subscriber decides how to word.
 *
 *  This block sits ABOVE the UCLASS() macro on purpose: UnrealHeaderTool requires the class
 *  definition to immediately follow UCLASS(), and anything between them stops the build. */
class ABNPlayerState;
DECLARE_MULTICAST_DELEGATE_ThreeParams(FBNPlayerDeathSignature, ABNPlayerState* /*Victim*/, ABNPlayerState* /*Killer*/, FName /*SourceName*/);

/** R7 — fires wherever the numbers change: the authority from AddKill/AddDeath/ResetScore (no
 *  OnRep runs there), clients from the OnReps. The scoreboard and the match band both read
 *  GetKills/GetDeaths back rather than taking numbers by parameter — one delegate, no payload
 *  to keep honest. */
DECLARE_MULTICAST_DELEGATE_OneParam(FBNScoreChangedSignature, ABNPlayerState*);

/** R7 — the owner's respawn clock changed (stamped at death, cleared at the rebody). */
DECLARE_MULTICAST_DELEGATE_OneParam(FBNRespawnStampSignature, ABNPlayerState*);

UCLASS()
class BREACHPOINTNEXT_API ABNPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ABNPlayerState();

	/** Authority only, and deliberately NOT a UPROPERTY delegate: subscribers are C++ systems
	 *  (GameMode now; killfeed and scoring later), and a death is announced once per life. */
	FBNPlayerDeathSignature OnPlayerDeath;

	/** Called by UBNGA_Death, which is what reads the killer off the attribute set's capture.
	 *  Nothing else should broadcast this. */
	void BroadcastDeath(ABNPlayerState* Killer, FName SourceName = NAME_None);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** SEAMLESS TRAVEL builds a NEW PlayerState and copies the old one into it. The engine's own
	 *  override carries the name, the id and its float Score — and knows nothing about BN's two
	 *  integers, so without this a travel silently zeroed everyone's kills and deaths.
	 *
	 *  Dormant today by design: BN restarts a match IN PLACE (SetMatchState back to
	 *  WaitingToStart) precisely so listen-server connections survive, and that path never builds
	 *  a new PlayerState. This is here for the day a map rotation lands, because the failure it
	 *  prevents is silent — a scoreboard that reads zero after a travel looks like a scoreboard
	 *  bug, not like a lifecycle one. */
	virtual void CopyProperties(APlayerState* PlayerState) override;

	FBNScoreChangedSignature OnScoreChanged;
	FBNRespawnStampSignature OnRespawnStampChanged;

	/** Two integers, not the engine's float Score: an FFA scoreboard reads counts, and a float
	 *  invites rounding questions nobody wants to answer. */
	int32 GetKills() const { return Kills; }
	int32 GetDeaths() const { return Deaths; }

	/** THE THIRD INT (founder ruling, 26 Aug 2026 — the Hill): objective seconds, kept
	 *  SEPARATE from Kills on purpose — scoring hill time through AddKill would silently
	 *  poison the killfeed's and the scoreboard's meaning ("14 kills" for a player who
	 *  killed nobody). The match's win condition reads GetScore(); the columns stay
	 *  honest. Zero in Slayer, so nothing changes where no objective exists. */
	int32 GetObjectivePoints() const { return ObjectivePoints; }
	int32 GetScore() const { return Kills + ObjectivePoints; }

	/** 0 = no respawn pending. Non-zero is an ABSOLUTE server time (the match clock's own
	 *  pattern) — the owning client computes "respawning in N" locally against
	 *  GetServerWorldTimeSeconds, correct on its first frame at any join moment. */
	double GetRespawnAtServerTime() const { return RespawnAtServerTime; }

	/** Authority; the GameMode's respawn path is the only caller. */
	void SetRespawnAtServerTime(double InServerTime);

	/** Authority only. The GameMode's death handler is the only caller — the credit rules live
	 *  there, with the killer in hand, not here. */
	void AddKill();
	void AddDeath();
	/** Authority only. The GameMode's hill tick is the only caller. */
	void AddObjectivePoints(int32 Points);

	/** Authority only. The match restart is the only caller: without it a restarted match keeps
	 *  every score, and the first elimination of the new round instantly re-ends it. */
	void ResetScore();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UBNAbilitySystemComponent* GetBNAbilitySystemComponent() const { return AbilitySystemComponent; }
	UBNAttributeSet* GetAttributeSet() const { return AttributeSet; }

	void GrantDefaults();

	/** The ONE way attributes reach their starting numbers — first life and every respawn.
	 *  Authority only. Nothing anywhere hand-sets Health, Shield or MoveSpeed. */
	void ApplyInitAttributes();

protected:
	UFUNCTION()
	void OnRep_Kills();

	UFUNCTION()
	void OnRep_Deaths();

	UFUNCTION()
	void OnRep_RespawnAtServerTime();

	UPROPERTY(ReplicatedUsing = OnRep_Kills)
	int32 Kills = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Deaths)
	int32 Deaths = 0;

	UFUNCTION()
	void OnRep_ObjectivePoints();

	UPROPERTY(ReplicatedUsing = OnRep_ObjectivePoints)
	int32 ObjectivePoints = 0;

	/** COND_OwnerOnly: only the dead player's own screen counts down — nobody else renders it,
	 *  so nobody else pays for it. */
	UPROPERTY(ReplicatedUsing = OnRep_RespawnAtServerTime)
	double RespawnAtServerTime = 0.0;

	UPROPERTY()
	TObjectPtr<UBNAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UBNAttributeSet> AttributeSet;

	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<UBNAbilitySet> DefaultAbilitySet;

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UGameplayEffect> InitEffect;

	bool bDefaultsGranted = false;
};
