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
/** R8 — one param, the PlayerState whose side changed; readers ask it for the new value. */
DECLARE_MULTICAST_DELEGATE_OneParam(FBNTeamChangedSignature, ABNPlayerState* /*Changed*/);

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

	FBNScoreChangedSignature OnScoreChanged;
	FBNRespawnStampSignature OnRespawnStampChanged;

	/** R8 — the side changed. Fires on EVERY machine (the authority by hand, clients from the
	 *  OnRep), the same discipline the score and killfeed feeds already use. */
	FBNTeamChangedSignature OnTeamChanged;

	/** BNTeams::Unassigned until the mode assigns one — a real state, not an error: a controller
	 *  exists for a frame before assignment, and a joining client's TeamId bunch can trail. */
	int32 GetTeamId() const { return TeamId; }

	/** Authority only, and the GameMode is the only caller: balance is a match rule, so it lives
	 *  where the roster does, never in the PlayerState that merely carries the answer. */
	void SetTeamId(int32 NewTeamId);

	/** Two integers, not the engine's float Score: an FFA scoreboard reads counts, and a float
	 *  invites rounding questions nobody wants to answer. */
	int32 GetKills() const { return Kills; }
	int32 GetDeaths() const { return Deaths; }

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

	UFUNCTION()
	void OnRep_TeamId();

	/** To EVERYONE, unlike the respawn stamp: every machine draws every player's side — the
	 *  killfeed's colours, the scoreboard's grouping, a nameplate over an ally. */
	UPROPERTY(ReplicatedUsing = OnRep_TeamId)
	int32 TeamId = INDEX_NONE;

	UPROPERTY(ReplicatedUsing = OnRep_Kills)
	int32 Kills = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Deaths)
	int32 Deaths = 0;

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
