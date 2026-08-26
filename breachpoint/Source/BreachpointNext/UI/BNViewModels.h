#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "MVVMViewModelBase.h"
#include "UI/BNUITypes.h"
#include "BNViewModels.generated.h"

class AGameStateBase;
class UAbilitySystemComponent;
class UTexture2D;
struct FOnAttributeChangeData;

/** The killfeed VIEW ring changed (push or expiry). A native multicast, NOT a FieldNotify on
 *  the array — the compiled reference notifies its killfeed exactly this way, and §API's rule
 *  is transcription over invention. One broadcast per change; readers walk GetKillfeedEntries. */
DECLARE_MULTICAST_DELEGATE(FBNKillfeedViewChangedSignature);

/** The roster view changed — same shape, same reasoning, for the scoreboard's rows. */
DECLARE_MULTICAST_DELEGATE(FBNRosterViewChangedSignature);

/** Which attributes feed the combat ViewModel — INJECTED by the director, never named here.
 *  This struct is the decoupling that let the old module's combat VM port with zero gameplay
 *  includes: the VM knows "a health-like number and its max", not UBNAttributeSet. */
USTRUCT()
struct FBNCombatAttributeBindings
{
	GENERATED_BODY()

	FGameplayAttribute Health;
	FGameplayAttribute MaxHealth;
	FGameplayAttribute Shield;
	FGameplayAttribute MaxShield;

	/** R7.4 — the grenade pouch, injected like the rest: the VM counts, it does not know whose. */
	FGameplayAttribute Grenades;
	FGameplayAttribute MaxGrenades;
};

/**
 * The owning player's fight, as presentation state: vitals, the hand, death. Fed exclusively by
 * UBNHUDDirector; read exclusively through FieldNotify. Never touches a gameplay header.
 *
 * HONEST-UNKNOWN: VitalsState stays Unknown until BOTH a value arrived AND the denominators are
 * known — the old module shipped an empty health bar on a living player because Health landed
 * one bunch before MaxHealth, and this gate is that lesson (`bAnyFound && bDenominatorsKnown`).
 */
UCLASS(BlueprintType, DisplayName = "BN Combat Viewmodel")
class BREACHPOINTNEXT_API UBNVM_Combat : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	virtual void BeginDestroy() override;

	/** Director only. Binds attribute-change delegates and reads every value ONCE — the
	 *  subscribe-then-read rule; changes before this call are not missed, they are the read. */
	void BindToAbilitySystem(UAbilitySystemComponent* InASC, const FBNCombatAttributeBindings& InBindings);
	void UnbindFromAbilitySystem();

	/** Director only: the hand changed. Empty name = unarmed; ammo INDEX_NONE = no magazine
	 *  (the knife, the empty hand) — the widget renders a dash, never a confident zero. The icon
	 *  is the design's silhouette slot and may be null; it notifies like every other field. */
	void SetEquippedWeapon(const FText& InName, int32 InMagAmmo, int32 InReserveAmmo, bool bKnown,
		const TSoftObjectPtr<UTexture2D>& InIcon = TSoftObjectPtr<UTexture2D>(),
		const TSoftObjectPtr<UTexture2D>& InReticle = TSoftObjectPtr<UTexture2D>());
	void SetAmmo(int32 InMagAmmo, int32 InReserveAmmo);

	/** Director only: R7.3's stowed slot — the weapon ONE swap press away, which is what "stowed"
	 *  can honestly mean in a five-slot carry. Empty name = nothing to swap to (a single-weapon
	 *  loadout), which the block renders as an empty slot, not a dash. */
	void SetStowedWeapon(const FText& InName, const TSoftObjectPtr<UTexture2D>& InIcon = TSoftObjectPtr<UTexture2D>());

	/** Director only: the dead state and its clock. RespawnAt <= 0 clears the countdown. The VM
	 *  owns the per-second update — one timer, aligned to the stamp, never a tick. */
	void SetDead(bool bInDead);
	/** The killer's line and, from R7.3, WHAT they used — the design's second line under the
	 *  name. An empty weapon is the honest answer for a cause with no row and for a death the
	 *  feed never explained; the screen then shows the name alone. */
	void SetKilledByLine(const FText& InLine, const FText& InWeapon = FText::GetEmpty(),
		const TSoftObjectPtr<UTexture2D>& InWeaponIcon = TSoftObjectPtr<UTexture2D>());
	void SetRespawnStamp(double InRespawnAtServerTime, AGameStateBase* InTimeSource);

	/** Travel and rebind reset: everything back to Unknown, timers down. */
	void ClearToUnknown();

	float GetHealthPercent() const { return HealthPercent; }
	float GetShieldPercent() const { return ShieldPercent; }
	int32 GetHealthValue() const { return HealthValue; }
	int32 GetShieldValue() const { return ShieldValue; }

	/** Does this mode HAVE shields at all? `BNGE_InitVitals` overrides MaxShield to 0 while
	 *  shields are paused (founder, 13 Aug: "I want to see the health perfectly working first"),
	 *  and that comment promises "everything downstream already gates itself on MaxShield > 0".
	 *  This is the getter that lets the vitals widget keep that promise. */
	bool HasShields() const { return RawMaxShield > 0.f; }
	EBNUIDataState GetVitalsState() const { return VitalsState; }
	FText GetWeaponName() const { return WeaponName; }
	int32 GetMagAmmo() const { return MagAmmo; }
	int32 GetReserveAmmo() const { return ReserveAmmo; }
	EBNUIDataState GetEquipmentState() const { return EquipmentState; }
	/** INDEX_NONE = unknown or none carried; the widget hides the slot rather than drawing 0. */
	int32 GetGrenadeCount() const { return GrenadeCount; }
	int32 GetGrenadeCapacity() const { return GrenadeCapacity; }
	/** By value, matching the compiled reference's soft-pointer getter shape. */
	TSoftObjectPtr<UTexture2D> GetWeaponIcon() const { return WeaponIcon; }

	TSoftObjectPtr<UTexture2D> GetWeaponReticle() const { return WeaponReticle; }
	FText GetStowedWeaponName() const { return StowedWeaponName; }
	TSoftObjectPtr<UTexture2D> GetStowedWeaponIcon() const { return StowedWeaponIcon; }
	bool IsDead() const { return bIsDead; }
	FText GetKilledByLine() const { return KilledByLine; }
	FText GetKilledByWeapon() const { return KilledByWeapon; }
	TSoftObjectPtr<UTexture2D> GetKilledByWeaponIcon() const { return KilledByWeaponIcon; }
	int32 GetRespawnSecondsRemaining() const { return RespawnSecondsRemaining; }

	/** 0 at the moment of death, 1 when the respawn is due — the design's ring, as a number any
	 *  bar or radial material can read. The TOTAL is measured at the stamp rather than read from
	 *  a config: the delay is the GameMode's and does not replicate, but the first remaining-time
	 *  the client computes IS that delay. INDEX_NONE-shaped emptiness is a 0 here, not a lie:
	 *  the widget hides the ring when RespawnSecondsRemaining says there is no respawn pending. */
	float GetRespawnFraction() const { return RespawnFraction; }

private:
	void HandleAttributeChanged(const FOnAttributeChangeData& Data);
	void RefreshVitals();
	void RefreshGrenades();
	void UpdateRespawnClock();
	void ScheduleNextRespawnUpdate();
	void StopRespawnClock();
	UWorld* GetTimerWorld() const;

	// ---- vitals ----
	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetHealthPercent", Category = "BN|Combat", meta = (AllowPrivateAccess))
	float HealthPercent = 0.f;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetShieldPercent", Category = "BN|Combat", meta = (AllowPrivateAccess))
	float ShieldPercent = 0.f;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetHealthValue", Category = "BN|Combat", meta = (AllowPrivateAccess))
	int32 HealthValue = 0;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetShieldValue", Category = "BN|Combat", meta = (AllowPrivateAccess))
	int32 ShieldValue = 0;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetVitalsState", Category = "BN|Combat", meta = (AllowPrivateAccess))
	EBNUIDataState VitalsState = EBNUIDataState::Unknown;

	// ---- the hand ----
	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetWeaponName", Category = "BN|Combat", meta = (AllowPrivateAccess))
	FText WeaponName;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetMagAmmo", Category = "BN|Combat", meta = (AllowPrivateAccess))
	int32 MagAmmo = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetReserveAmmo", Category = "BN|Combat", meta = (AllowPrivateAccess))
	int32 ReserveAmmo = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetEquipmentState", Category = "BN|Combat", meta = (AllowPrivateAccess))
	EBNUIDataState EquipmentState = EBNUIDataState::Unknown;

	// ---- the pouch (R7.4). Both INDEX_NONE until a capacity is known: zero grenades and no
	// grenade system look identical on screen otherwise, and one of them is a lie.
	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetGrenadeCount", Category = "BN|Combat", meta = (AllowPrivateAccess))
	int32 GrenadeCount = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetGrenadeCapacity", Category = "BN|Combat", meta = (AllowPrivateAccess))
	int32 GrenadeCapacity = INDEX_NONE;

	/** A FieldNotify soft pointer — the compiled reference's own shape (its roster emblem is
	 *  declared and set exactly this way), so no counter is needed to force a notify. */
	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetWeaponIcon", Category = "BN|Combat", meta = (AllowPrivateAccess))
	TSoftObjectPtr<UTexture2D> WeaponIcon;

	/** The equipped weapon's reticle. Same shape and same reason as WeaponIcon. */
	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetWeaponReticle", Category = "BN|Combat", meta = (AllowPrivateAccess))
	TSoftObjectPtr<UTexture2D> WeaponReticle;

	// ---- the stowed slot (R7.3): the NEXT weapon in the swap cycle, not a second inventory ----
	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetStowedWeaponName", Category = "BN|Combat", meta = (AllowPrivateAccess))
	FText StowedWeaponName;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetStowedWeaponIcon", Category = "BN|Combat", meta = (AllowPrivateAccess))
	TSoftObjectPtr<UTexture2D> StowedWeaponIcon;

	// ---- death ----
	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "IsDead", Category = "BN|Combat", meta = (AllowPrivateAccess))
	bool bIsDead = false;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetKilledByLine", Category = "BN|Combat", meta = (AllowPrivateAccess))
	FText KilledByLine;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetKilledByWeapon", Category = "BN|Combat", meta = (AllowPrivateAccess))
	FText KilledByWeapon;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetKilledByWeaponIcon", Category = "BN|Combat", meta = (AllowPrivateAccess))
	TSoftObjectPtr<UTexture2D> KilledByWeaponIcon;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetRespawnFraction", Category = "BN|Combat", meta = (AllowPrivateAccess))
	float RespawnFraction = 0.f;

	/** INDEX_NONE = no respawn pending; 0 = imminent. */
	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetRespawnSecondsRemaining", Category = "BN|Combat", meta = (AllowPrivateAccess))
	int32 RespawnSecondsRemaining = INDEX_NONE;

	// ---- plumbing (no UPROPERTY: handles and weak refs, cleared on unbind) ----
	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
	FBNCombatAttributeBindings Bindings;
	TArray<TPair<FGameplayAttribute, FDelegateHandle>> AttributeHandles;

	/** Raw last-seen attribute values; the gate needs to distinguish "0.0" from "never arrived". */
	float RawHealth = 0.f;
	float RawMaxHealth = 0.f;
	float RawShield = 0.f;
	float RawMaxShield = 0.f;
	float RawGrenades = 0.f;
	float RawMaxGrenades = 0.f;
	bool bAnyVitalsSeen = false;

	TWeakObjectPtr<AGameStateBase> RespawnTimeSource;
	double RespawnAtServerTime = 0.0;
	/** The window this death was given, measured at the stamp — see GetRespawnFraction. */
	double RespawnTotalSeconds = 0.0;
	FTimerHandle RespawnTimerHandle;
};

/**
 * The match, as presentation state: phase, clock, scores, winner, and the killfeed view ring.
 * Fed exclusively by UBNHUDDirector. The clock is COMPUTED, phase-locked to the whole server
 * second so every readout shows the same digit — one replicated stamp, zero ticking properties.
 */
UCLASS(BlueprintType, DisplayName = "BN Match Viewmodel")
class BREACHPOINTNEXT_API UBNVM_Match : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	virtual void BeginDestroy() override;

	/** Director only: the phase FName (the engine's own MatchState names) and, when ended, the
	 *  winner line. Composes the banner here so the widget renders text and decides nothing. */
	void SetMatchPhase(FName InMatchState, const FText& InWinnerLine,
		EBNMatchOutcome InOutcome = EBNMatchOutcome::Undecided);

	/** Director only: my kills, the current leader's, and the limit — read back by widgets. */
	void SetScores(int32 InMyKills, int32 InTopKills, int32 InScoreLimit);

	/** TEAMS (BN16), director only: the team ledger, RELATIVE — my team's points and the
	 *  enemy team's, never a team id (the relative-presentation law; two teams are hard,
	 *  so "the enemy team" is one number). bInTeamsMode false clears both to zero and is
	 *  the FFA/unknown state: the match band renders today's kills bars and no team strip.
	 *  Fractions recompute here AND in SetScores — whichever of limit and ledger lands
	 *  second, the bars agree. */
	void SetTeamScores(bool bInTeamsMode, int32 InMyTeamScore, int32 InEnemyTeamScore);

	/** Director only: the end stamp and the time authority. Restarts the clock updates. */
	void SetMatchClock(double InMatchEndServerTime, AGameStateBase* InTimeSource);

	/** Director only, once per NEW ring entry (dedupe by Sequence is the CALLER's job via
	 *  GetLastKillfeedSequence). Stamps the LOCAL expiry on the entry and trims the view pool.
	 *  TEAMS (BN16): the two parties' relations ride the same push — the director computes
	 *  them (only it knows whose screen this is), defaulted to None so every FFA call site
	 *  reads unchanged. */
	void PushKillfeedEntry(const FText& InLine, int32 InSequence, bool bInvolvesSelf,
		const TSoftObjectPtr<UTexture2D>& InWeaponIcon = TSoftObjectPtr<UTexture2D>(),
		const FText& InKillerText = FText::GetEmpty(), const FText& InVictimText = FText::GetEmpty(),
		EBNUITeamRelation InKillerRelation = EBNUITeamRelation::None,
		EBNUITeamRelation InVictimRelation = EBNUITeamRelation::None);
	int32 GetLastKillfeedSequence() const { return LastKillfeedSequence; }

	FBNKillfeedViewChangedSignature OnKillfeedViewChanged;

	/** Director only: the whole sorted roster, replaced. Broadcasts only when something the
	 *  scoreboard renders actually changed — a rebuild that produces the same rows is silent. */
	void SetRoster(TArray<FBNScoreRowView>&& InRoster);
	const TArray<FBNScoreRowView>& GetRoster() const { return Roster; }

	FBNRosterViewChangedSignature OnRosterViewChanged;

	void ClearToUnknown();

	FName GetMatchStateName() const { return MatchStateName; }
	FText GetPhaseBannerText() const { return PhaseBannerText; }

	EBNMatchOutcome GetOutcome() const { return Outcome; }
	FText GetMatchClockText() const { return MatchClockText; }
	int32 GetMyKills() const { return MyKills; }
	int32 GetTopKills() const { return TopKills; }
	int32 GetScoreLimit() const { return ScoreLimit; }

	/** The design's two score bars, as fractions of the limit — 0..1, clamped. Zero when there is
	 *  no limit to be a fraction OF: an unlimited match draws no bar rather than a full one. */
	float GetSelfScoreFraction() const { return SelfScoreFraction; }
	float GetTopScoreFraction() const { return TopScoreFraction; }
	bool IsTeamsMode() const { return bTeamsMode; }
	int32 GetMyTeamScore() const { return MyTeamScore; }
	int32 GetEnemyTeamScore() const { return EnemyTeamScore; }
	float GetMyTeamScoreFraction() const { return MyTeamScoreFraction; }
	float GetEnemyTeamScoreFraction() const { return EnemyTeamScoreFraction; }
	EBNUIDataState GetMatchDataState() const { return MatchDataState; }
	const TArray<FBNKillfeedViewEntry>& GetKillfeedEntries() const { return KillfeedEntries; }

private:
	void UpdateClocks();
	void ScheduleNextClockUpdate();
	void StopClockUpdates();
	void ExpireKillfeedEntries();
	void ScheduleKillfeedExpiry();
	UWorld* GetTimerWorld() const;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetMatchStateName", Category = "BN|Match", meta = (AllowPrivateAccess))
	FName MatchStateName;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetPhaseBannerText", Category = "BN|Match", meta = (AllowPrivateAccess))
	FText PhaseBannerText;

	/** Victory / Defeat / Draw, from the reader's own point of view. Composed by the director;
	 *  the screen renders a word and a tint and decides nothing. */
	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetOutcome", Category = "BN|Match", meta = (AllowPrivateAccess))
	EBNMatchOutcome Outcome = EBNMatchOutcome::Undecided;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetMatchClockText", Category = "BN|Match", meta = (AllowPrivateAccess))
	FText MatchClockText;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetMyKills", Category = "BN|Match", meta = (AllowPrivateAccess))
	int32 MyKills = 0;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetTopKills", Category = "BN|Match", meta = (AllowPrivateAccess))
	int32 TopKills = 0;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetScoreLimit", Category = "BN|Match", meta = (AllowPrivateAccess))
	int32 ScoreLimit = 0;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetSelfScoreFraction", Category = "BN|Match", meta = (AllowPrivateAccess))
	float SelfScoreFraction = 0.f;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetTopScoreFraction", Category = "BN|Match", meta = (AllowPrivateAccess))
	float TopScoreFraction = 0.f;

	// ---- TEAMS (BN16): the relative team ledger — see SetTeamScores ----
	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "IsTeamsMode", Category = "BN|Match", meta = (AllowPrivateAccess))
	bool bTeamsMode = false;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetMyTeamScore", Category = "BN|Match", meta = (AllowPrivateAccess))
	int32 MyTeamScore = 0;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetEnemyTeamScore", Category = "BN|Match", meta = (AllowPrivateAccess))
	int32 EnemyTeamScore = 0;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetMyTeamScoreFraction", Category = "BN|Match", meta = (AllowPrivateAccess))
	float MyTeamScoreFraction = 0.f;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetEnemyTeamScoreFraction", Category = "BN|Match", meta = (AllowPrivateAccess))
	float EnemyTeamScoreFraction = 0.f;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetMatchDataState", Category = "BN|Match", meta = (AllowPrivateAccess))
	EBNUIDataState MatchDataState = EBNUIDataState::Unknown;

	/** Plain member, notified by OnKillfeedViewChanged above — see the delegate's comment. */
	UPROPERTY(Transient)
	TArray<FBNKillfeedViewEntry> KillfeedEntries;

	UPROPERTY(Transient)
	TArray<FBNScoreRowView> Roster;

	/** How many lines the view keeps. Presentation, not replication (the GameState ring is the
	 *  record; this is what's on screen). Linger time is BNUITiming's — shared with the
	 *  director's join-age filter so "too old to show" means one thing. */
	static constexpr int32 KillfeedMaxVisibleEntries = 5;

	int32 LastKillfeedSequence = INDEX_NONE;
	double MatchEndServerTime = 0.0;
	TWeakObjectPtr<AGameStateBase> TimeSource;
	FTimerHandle ClockTimerHandle;
	FTimerHandle KillfeedExpiryTimerHandle;
};
