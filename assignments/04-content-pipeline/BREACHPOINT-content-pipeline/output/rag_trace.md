# RAG trace — query, retrieved chunk, generated output, side by side

One section per content type. The chunk text below is *verbatim* what the
spotter received; the rows beneath are what survived judging and review.
Every citation is `file:line-line` against this repo, so any claim here can
be opened and checked.

## announcer

**Query** (`scope=slice`, boost `{'DT_SpotterLines.csv': 2.5, 'DT_Medals.csv': 2.0}`)

```
announcer callout line medal killfeed rocket multi kill first kill of the match sudden death killing spree ended clipped military voice
```

### Retrieved chunks

<details open><summary><b>[1] breachpoint/Content/Data/DT_Medals.csv:2-12</b> — canon <code>slice</code>, BM25 39.7 — DT_Medals.csv</summary>

```text
DT_Medals.csv (existing shipped table)
RowName,MedalName,Description,TriggerId
M1,Double Kill,Two kills in quick succession.,Kill.Multi.Double
M2,Killing Spree,Multiple kills without dying.,Kill.Spree
M3,Grapple Kill,Killed an enemy using the Grappleshot.,Kill.Grapple
M4,Blast Radius,Two or more killed with one rocket.,Kill.Rocket.Multi
M5,Blindside,Killed an enemy with a melee from the rear arc.,Kill.Melee.Rear
M6,Headshot,Killed with a shot to the head.,Kill.Headshot
M7,Area Denial,Killed with a grenade.,Kill.Grenade
M8,Denial,Killed the enemy carrying the rocket.,Rocket.Denied
M9,Breach,Scored the first kill of the match.,Kill.First
M10,Last Word,Scored the kill that won sudden death.,Match.SuddenDeath.Win
M11,Spree Ender,Killed an enemy who was on a killing spree.,Kill.SpreeEnder
```

</details>

<details open><summary><b>[2] breachpoint/Content/Data/DT_SpotterLines.csv:50-64</b> — canon <code>slice</code>, BM25 23.41 — DT_SpotterLines.csv</summary>

```text
DT_SpotterLines.csv (existing shipped table)
RowName,TriggerId,Text,Audience,Weight,RepeatCooldown_s
S17a,Match.Phase.SuddenDeath,Sudden death. No respawns.,All,1.0,0
S17b,Match.Phase.SuddenDeath,Next kill wins.,All,1.0,0
S17c,Match.Phase.SuddenDeath,Sudden death.,All,1.0,0
S18a,Match.End.Win,Match won.,Team,1.0,0
S18b,Match.End.Win,Win confirmed.,Team,1.0,0
S18c,Match.End.Win,Match complete. Victory.,Team,1.0,0
S19a,Match.End.Loss,Match lost.,Team,1.0,0
S19b,Match.End.Loss,Loss recorded.,Team,1.0,0
S19c,Match.End.Loss,Match complete. Defeat.,Team,1.0,0
S20a,Score.Comeback,Lead retaken.,Team,1.0,8
S20b,Score.Comeback,Deficit closed.,Team,1.0,8
S20c,Score.Comeback,Score is level.,Team,1.0,8
S21a1,Score.Blowout.Lead,Wide lead.,Team,1.0,8
S21a2,Score.Blowout.Lead,Lead is holding.,Team,1.0,8
S21b1,Score.Blowout.Deficit,Down heavy.,Team,1.0,8
```

</details>

<details open><summary><b>[3] breachpoint/BREACHPOINT-GDD-VERTICAL-SLICE.md:213-227</b> — canon <code>slice</code>, BM25 23.06 — BREACHPOINT — VERTICAL SLICE > 2. Game Mechanics (Player-Facing Actions and Loop) > 2.9 What the Player Sees (HUD)</summary>

```text
BREACHPOINT — VERTICAL SLICE > 2. Game Mechanics (Player-Facing Actions and Loop) > 2.9 What the Player Sees (HUD)

- **Top-left:** shield bar over health bar — the two-layer read is the
  most important element on screen.
- **Bottom-right:** current weapon + ammo + spare mags; second weapon
  ghosted beneath.
- **Bottom-left:** grenade count; **Grappleshot cooldown ring**.
- **Center:** reticle with **distinct hit markers for shield hits vs.
  flesh hits** — the sandbox is unreadable without this distinction.
- **Top-center:** team score, match timer, **rocket respawn countdown**.
- **Feed:** killfeed + medals (Double Kill, Killing Spree, **Grapple
  Kill**, Rocket multi-kill) — **canned, instant, deterministic**.
- **Match end:** carnage report (K/D/A, accuracy, medals) + one Spotter
  coach line + rematch prompt.

---
```

</details>

<details open><summary><b>[4] breachpoint/BREACHPOINT-GDD-VERTICAL-SLICE.md:41-46</b> — canon <code>slice</code>, BM25 15.12 — BREACHPOINT — VERTICAL SLICE > 1. Executive Summary > 1.2 Win and Loss Conditions</summary>

```text
BREACHPOINT — VERTICAL SLICE > 1. Executive Summary > 1.2 Win and Loss Conditions

- **Win:** first team to **25 kills**, or the higher team score when the
  **8:00** timer expires.
- **Tiebreak:** sudden death — no respawns, first kill wins, **60-second
  cap**; if uncontested, higher team damage dealt wins.
- **Loss:** any other scoreline at the timer.
```

</details>

<details open><summary><b>[5] breachpoint/BREACHPOINT-GDD-VERTICAL-SLICE.md:282-298</b> — canon <code>slice</code>, BM25 14.8 — BREACHPOINT — VERTICAL SLICE > 3. AI Architecture (What Each Agent Does, Through Its Effect on Gameplay) > 3.3 The Runtime Agent — Spotter</summary>

```text
BREACHPOINT — VERTICAL SLICE > 3. AI Architecture (What Each Agent Does, Through Its Effect on Gameplay) > 3.3 The Runtime Agent — Spotter

**Spotter Agent** — the only model call in the shipped game, and never
load-bearing.

- **Role:** one telemetry-grounded coach line per human player at match
  end; optional color commentary appended to notable killfeed events
  (sprees, rocket multi-kills, grapple kills).
- **Gameplay effect:** every player leaves with one specific, *earned*
  correction — *"You lost 6 fights below 40% shields — break off and let
  them recharge."*
- **Output:** `{coach_line (≤ 30 words, references ≥ 1 telemetry stat)}`;
  per event `{spotter_line (≤ 18 words) | null}`.
- **Never load-bearing:** medals and killfeed are **canned and instant** —
  Halo's announcer is deterministic by design, which is correct, not a
  limitation. Spotter lines only *append*. Host-side, async, ≤ 12
  calls/match, 3 s timeout, canned-line DataTable fallback shipped in the
  build. **No connectivity ⇒ the game is identical minus flavor.**
```

</details>

<details open><summary><b>[6] breachpoint/BREACHPOINT-GDD-VERTICAL-SLICE.md:528-544</b> — canon <code>slice</code>, BM25 12.32 — BREACHPOINT — VERTICAL SLICE > Appendix A — Combat Tuning (first pass)</summary>

```text
BREACHPOINT — VERTICAL SLICE > Appendix A — Combat Tuning (first pass)

| Parameter | Value |
|---|---|
| Shields / Health | 100 / 100 |
| Shield recharge | 60/s, begins 2.5 s after last damage |
| Health regeneration | none |
| Assault Rifle | 8 dmg/shot, 600 RPM, 32 mag, hitscan |
| Magnum | 22 dmg, ×2 headshot, 8 mag, hitscan |
| Rocket Launcher | 120 dmg, radius 4 m, 2 shots, **90 s respawn** |
| Frag grenade | 90 dmg centre, radius 5 m, 2 carried |
| Melee | 70 dmg; **rear = instant kill** |
| Grappleshot | 20 s cooldown, 20 m range |
| Sprint | +20% move speed, no cost; ends on fire/melee/grenade |
| Weapon swap | 0.4 s |
| Respawn | 5 s, scored spawn |
| Match | 25 kills or 8:00; sudden death 60 s cap |
| Map sightline cap | 35 m |
```

</details>

<details open><summary><b>[7] breachpoint/Content/Data/DT_SpotterLines.csv:2-25</b> — canon <code>slice</code>, BM25 11.09 — DT_SpotterLines.csv</summary>

```text
DT_SpotterLines.csv (existing shipped table)
RowName,TriggerId,Text,Audience,Weight,RepeatCooldown_s
S01a,Kill.Confirmed,Kill confirmed.,Self,1.0,20
S01b,Kill.Confirmed,Target down.,Self,1.0,20
S01c,Kill.Confirmed,Hostile down.,Self,1.0,20
S02a,Death.Self,You're down.,Self,1.0,20
S02b,Death.Self,Shields gone. Health gone.,Self,1.0,20
S02c,Death.Self,Killed in action.,Self,1.0,20
S03a,Kill.Multi.Double,Double kill.,Self,1.0,20
S03b,Kill.Multi.Double,Two down.,Self,1.0,20
S03c,Kill.Multi.Double,Back to back.,Self,1.0,20
S04a,Kill.Multi.Triple,Triple kill.,Self,1.0,20
S04b,Kill.Spree,Killing spree.,Self,1.0,20
S04c,Kill.Multi.Triple,Three down.,Self,1.0,20
S05a,Shield.Break.Enemy,Shields down.,Self,1.0,20
S05b,Shield.Break.Enemy,Shields stripped.,Self,1.0,20
S05c,Shield.Break.Enemy,Flesh exposed.,Self,1.0,20
S06a,Shield.Break.Self,Shields gone.,Self,1.0,20
S06b,Shield.Break.Self,You're on health.,Self,1.0,20
S06c,Shield.Break.Self,Last layer.,Self,1.0,20
S07a,Kill.Headshot,Headshot.,Self,1.0,20
S07b,Kill.Headshot,Precision kill.,Self,1.0,20
S07c,Kill.Headshot,Clean headshot.,Self,1.0,20
S08a,Kill.Melee.Rear,Rear kill.,Self,1.0,20
S08b,Kill.Melee.Rear,From behind.,Self,1.0,20
S08c,Kill.Melee.Rear,Blindside.,Self,1.0,20
```

</details>

### Sources the spotter cited across its pool

- `breachpoint/BREACHPOINT-GDD-VERTICAL-SLICE.md:41-46 (sudden death, first kill wins)`
- `breachpoint/BREACHPOINT-GDD-VERTICAL-SLICE.md:41-46 (tiebreak, first kill wins)`
- `breachpoint/BREACHPOINT-GDD-VERTICAL-SLICE.md:41-46 (win conditions)`
- `breachpoint/BREACHPOINT-GDD-VERTICAL-SLICE.md:528-544 (Rocket Launcher entry)`
- `breachpoint/BREACHPOINT-GDD-VERTICAL-SLICE.md:528-544 (Rocket Launcher radius entry)`
- `breachpoint/Content/Data/DT_Medals.csv:2-12 (M10 Last Word)`
- `breachpoint/Content/Data/DT_Medals.csv:2-12 (M11 Spree Ender)`
- `breachpoint/Content/Data/DT_Medals.csv:2-12 (M11)`
- `breachpoint/Content/Data/DT_Medals.csv:2-12 (M4 Blast Radius)`
- `breachpoint/Content/Data/DT_Medals.csv:2-12 (M4)`
- `breachpoint/Content/Data/DT_Medals.csv:2-12 (M9 Breach)`
- `breachpoint/Content/Data/DT_Medals.csv:2-12 (M9)`
- `breachpoint/Content/Data/DT_SpotterLines.csv:2-25 (S01a 'Kill confirmed.' register)`
- `breachpoint/Content/Data/DT_SpotterLines.csv:2-25 (S01a 'Kill confirmed.')`
- `breachpoint/Content/Data/DT_SpotterLines.csv:2-25 (S01c 'Hostile down.' register)`
- `breachpoint/Content/Data/DT_SpotterLines.csv:2-25 (S02a 'You're down.' register)`
- `breachpoint/Content/Data/DT_SpotterLines.csv:2-25 (S03b 'Two down.' multi-kill pattern)`
- `breachpoint/Content/Data/DT_SpotterLines.csv:2-25 (S03b 'Two down.')`
- `breachpoint/Content/Data/DT_SpotterLines.csv:2-25 (S04b 'Killing spree.')`
- `breachpoint/Content/Data/DT_SpotterLines.csv:50-64 (S17a 'Sudden death. No respawns.', S18a 'Match won.')`
- `breachpoint/Content/Data/DT_SpotterLines.csv:50-64 (S17a-c sudden death lines)`
- `breachpoint/Content/Data/DT_SpotterLines.csv:50-64 (S17c 'Sudden death.', S18c 'Match complete. Victory.')`
- `breachpoint/Content/Data/DT_SpotterLines.csv:50-64 (S18a 'Match won.')`
- `breachpoint/Content/Data/DT_SpotterLines.csv:50-64 (S18a, S17a)`
- `breachpoint/Content/Data/DT_SpotterLines.csv:50-64 (S18c 'Match complete. Victory.', S17c 'Sudden death.')`

### Generated output (post-judge, post-review, as landed)

```json
[
 {
  "RowName": "S22a",
  "TriggerId": "Kill.Rocket.Multi",
  "Text": "Rocket multi-kill.",
  "Audience": "Self",
  "Weight": "1.0",
  "RepeatCooldown_s": 20
 },
 {
  "RowName": "S22b",
  "TriggerId": "Kill.Rocket.Multi",
  "Text": "Multi-kill. Rocket.",
  "Audience": "Self",
  "Weight": "1.0",
  "RepeatCooldown_s": 20
 },
 {
  "RowName": "S22c",
  "TriggerId": "Kill.Rocket.Multi",
  "Text": "Group kill. Rocket.",
  "Audience": "Self",
  "Weight": "1.0",
  "RepeatCooldown_s": 20
 },
 {
  "RowName": "S23a",
  "TriggerId": "Kill.First",
  "Text": "Breach scored.",
  "Audience": "Self",
  "Weight": "1.0",
  "RepeatCooldown_s": 20
 },
 {
  "RowName": "S23b",
  "TriggerId": "Kill.First",
  "Text": "Breach.",
  "Audience": "Self",
  "Weight": "1.0",
  "RepeatCooldown_s": 20
 },
 {
  "RowName": "S23c",
  "TriggerId": "Kill.First",
  "Text": "First down.",
  "Audience": "Self",
  "Weight": "1.0",
  "RepeatCooldown_s": 20
 },
 {
  "RowName": "S24a",
  "TriggerId": "Match.SuddenDeath.Win",
  "Text": "Sudden death. Match won.",
  "Audience": "Team",
  "Weight": "1.0",
  "RepeatCooldown_s": 0
 },
 {
  "RowName": "S24b",
  "TriggerId": "Match.SuddenDeath.Win",
  "Text": "Final kill. Match won.",
  "Audience": "Team",
  "Weight": "1.0",
  "RepeatCooldown_s": 0
 },
 {
  "RowName": "S24c",
  "TriggerId": "Match.SuddenDeath.Win",
  "Text": "Match won in sudden death.",
  "Audience": "Team",
  "Weight": "1.0",
  "RepeatCooldown_s": 0
 },
 {
  "RowName": "S25a",
  "TriggerId": "Kill.SpreeEnder",
  "Text": "Spree ended.",
  "Audience": "Self",
  "Weight": "1.0",
  "RepeatCooldown_s": 20
 },
 {
  "RowName": "S25b",
  "TriggerId": "Kill.SpreeEnder",
  "Text": "Spree broken.",
  "Audience": "Self",
  "Weight": "1.0",
  "RepeatCooldown_s": 20
 },
 {
  "RowName": "S25c",
  "TriggerId": "Kill.SpreeEnder",
  "Text": "Spree ender.",
  "Audience": "Self",
  "Weight": "1.0",
  "RepeatCooldown_s": 20
 }
]
```

> **What the un-tweaked retriever would have returned instead:** `breachpoint/BREACHPOINT-GDD-FULL-CONCEPT.md:185-197`, `breachpoint/BREACHPOINT-GDD-FULL-CONCEPT.md:29-35`, `breachpoint/BREACHPOINT-GDD-FULL-CONCEPT.md:243-258` — see README §The retrieval tweak.

## coach

**Query** (`scope=slice`, boost `{'BRTelemetrySubsystem.h': 3.0}`)

```
telemetry fields recorded per player match kills deaths assists self inflicted friendly fire time in match spotter coach line canned fallback no connectivity
```

### Retrieved chunks

<details open><summary><b>[1] breachpoint/Source/Breachpoint/Telemetry/BRTelemetrySubsystem.h:53-90</b> — canon <code>slice</code>, BM25 31.71 — BRTelemetrySubsystem.h :: FBRPlayerMatchTelemetry</summary>

```text
BRTelemetrySubsystem.h :: FBRPlayerMatchTelemetry  (the SHIPPED schema — what the game records today)
struct FBRPlayerMatchTelemetry
{
	GENERATED_BODY()

	UPROPERTY()
	int32 PlayerKey = 0;

	UPROPERTY()
	uint8 TeamId = 255;

	UPROPERTY()
	bool bIsBot = false;

	UPROPERTY()
	bool bJoinedInProgress = false;

	UPROPERTY()
	int32 Kills = 0;

	UPROPERTY()
	int32 Deaths = 0;

	UPROPERTY()
	int32 Assists = 0;

	UPROPERTY()
	int32 SelfInflictedDeaths = 0;

	UPROPERTY()
	int32 FriendlyFireKills = 0;

	UPROPERTY()
	float TimeInMatchSeconds = 0.f;

	UPROPERTY()
	bool bLeftEarly = false;
};
```

</details>

<details open><summary><b>[2] breachpoint/BREACHPOINT-GDD-VERTICAL-SLICE.md:556-562</b> — canon <code>slice</code>, BM25 30.79 — BREACHPOINT — VERTICAL SLICE > Appendix C — Telemetry Schema</summary>

```text
BREACHPOINT — VERTICAL SLICE > Appendix C — Telemetry Schema

`FBRMatchTelemetry` (per player, per match): kills, deaths, assists,
accuracy per weapon, TTK distribution, shield-break→kill conversion,
grenade kills, melee kills (front/rear), grapple uses and grapple kills,
rocket holds and rocket kills, **fights lost below 40% shields**, medals,
time alive. Consumed by: Spotter (coach lines), the tuning-curator
(TTK/win bands), Combat QA (regression baselines).
```

</details>

<details open><summary><b>[3] breachpoint/BREACHPOINT-GDD-VERTICAL-SLICE.md:282-298</b> — canon <code>slice</code>, BM25 30.52 — BREACHPOINT — VERTICAL SLICE > 3. AI Architecture (What Each Agent Does, Through Its Effect on Gameplay) > 3.3 The Runtime Agent — Spotter</summary>

```text
BREACHPOINT — VERTICAL SLICE > 3. AI Architecture (What Each Agent Does, Through Its Effect on Gameplay) > 3.3 The Runtime Agent — Spotter

**Spotter Agent** — the only model call in the shipped game, and never
load-bearing.

- **Role:** one telemetry-grounded coach line per human player at match
  end; optional color commentary appended to notable killfeed events
  (sprees, rocket multi-kills, grapple kills).
- **Gameplay effect:** every player leaves with one specific, *earned*
  correction — *"You lost 6 fights below 40% shields — break off and let
  them recharge."*
- **Output:** `{coach_line (≤ 30 words, references ≥ 1 telemetry stat)}`;
  per event `{spotter_line (≤ 18 words) | null}`.
- **Never load-bearing:** medals and killfeed are **canned and instant** —
  Halo's announcer is deterministic by design, which is correct, not a
  limitation. Spotter lines only *append*. Host-side, async, ≤ 12
  calls/match, 3 s timeout, canned-line DataTable fallback shipped in the
  build. **No connectivity ⇒ the game is identical minus flavor.**
```

</details>

<details open><summary><b>[4] breachpoint/BREACHPOINT-GDD-VERTICAL-SLICE.md:213-227</b> — canon <code>slice</code>, BM25 15.52 — BREACHPOINT — VERTICAL SLICE > 2. Game Mechanics (Player-Facing Actions and Loop) > 2.9 What the Player Sees (HUD)</summary>

```text
BREACHPOINT — VERTICAL SLICE > 2. Game Mechanics (Player-Facing Actions and Loop) > 2.9 What the Player Sees (HUD)

- **Top-left:** shield bar over health bar — the two-layer read is the
  most important element on screen.
- **Bottom-right:** current weapon + ammo + spare mags; second weapon
  ghosted beneath.
- **Bottom-left:** grenade count; **Grappleshot cooldown ring**.
- **Center:** reticle with **distinct hit markers for shield hits vs.
  flesh hits** — the sandbox is unreadable without this distinction.
- **Top-center:** team score, match timer, **rocket respawn countdown**.
- **Feed:** killfeed + medals (Double Kill, Killing Spree, **Grapple
  Kill**, Rocket multi-kill) — **canned, instant, deterministic**.
- **Match end:** carnage report (K/D/A, accuracy, medals) + one Spotter
  coach line + rematch prompt.

---
```

</details>

<details open><summary><b>[5] breachpoint/BREACHPOINT-GDD-VERTICAL-SLICE.md:426-440</b> — canon <code>slice</code>, BM25 12.14 — BREACHPOINT — VERTICAL SLICE > 5. Scope and Schedule > 5.3 Cut Order (pre-declared — used, not improvised)</summary>

```text
BREACHPOINT — VERTICAL SLICE > 5. Scope and Schedule > 5.3 Cut Order (pre-declared — used, not improvised)

If behind at any gate, cut **in this order** and log it:

1. **Rocket Launcher** → two weapons; power-weapon contest becomes a
   map-control point without a reward *(costs the sandbox trade — cut
   only if Week 5 is at risk)*
2. **Bot difficulty settings** → single "Marine" profile
3. **Medals** → killfeed only
4. **Spotter agent** → canned coach lines only *(the fallback already
   ships, so this is a config flip)*
5. **Front-end menus** → direct-to-match + Steam overlay invite only

**Never cut:** shields-over-health, the golden triangle, Grappleshot.
Those three *are* the game; cutting any of them means building a
different, worse project.
```

</details>

<details open><summary><b>[6] breachpoint/BREACHPOINT-GDD-VERTICAL-SLICE.md:329-342</b> — canon <code>slice</code>, BM25 11.38 — BREACHPOINT — VERTICAL SLICE > 4. Technical Strategy > 4.2 Token Budget (runtime, per match)</summary>

```text
BREACHPOINT — VERTICAL SLICE > 4. Technical Strategy > 4.2 Token Budget (runtime, per match)

| Call | Model | Calls / match | Tokens in | Tokens out | Total |
|---|---|---|---|---|---|
| Spotter (events, batched ≥ 10 s) | `claude-haiku-4-5` | ≤ 12 | 600 | 60 | ≈ 7,900 |
| Coach (match end, per human) | `claude-haiku-4-5` | ≤ 8 | 900 | 80 | ≈ 7,800 |
| **Total** | | | | | **≈ 15,700 ≈ $0.021 / match** |

Priced at Haiku 4.5's current rates — **$1.00 / 1M input, $5.00 / 1M output** — the split
matters: 14,400 input tokens cost $0.0144 and 1,360 output tokens cost $0.0068. The model ID
is pinned (`claude-haiku-4-5`, 200K context) because "Claude Haiku" alone is ambiguous across
generations and the price differs by generation. Worst case at the caps is ~2¢ a match; the
caps, not the price, are the control. Re-check both against current pricing at BP11.

Caps enforced server-side; every failure path falls back to canned lines.
```

</details>

<details open><summary><b>[7] breachpoint/Source/Breachpoint/Telemetry/BRTelemetrySubsystem.h:141-218</b> — canon <code>slice</code>, BM25 10.62 — BRTelemetrySubsystem.h :: BREACHPOINT_API</summary>

```text
BRTelemetrySubsystem.h :: BREACHPOINT_API  (the SHIPPED schema — what the game records today)
class BREACHPOINT_API UBRTelemetrySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	void RecordEvent(const FBRTelemetryEvent& Event);

	void RecordPlayerEvent(FName EventId, const APlayerState* Subject, const APlayerState* Object,
		float Value, FName Detail);

	int32 GetPlayerKey(const APlayerState* Player);

	const FBRMatchTelemetryRecord& GetMatchRecord() const { return Record; }

	bool IsFinalized() const { return bFinalized; }

	FBRMatchTelemetryFinalizedSignature OnMatchTelemetryFinalized;
	FBRTelemetryEventRecordedSignature OnTelemetryEventRecorded;

protected:
	UPROPERTY(Config)
	int32 MaxRetainedEvents = 2048;

	UPROPERTY(Config)
	bool bTelemetryEnabled = true;

private:
	bool HasTelemetryAuthority() const;

	void TryBindMatchSources();

	void TryBindServerLifecycle();

	void HandlePlayerKilled(APlayerState* Killer, APlayerState* Victim, const TArray<APlayerState*>& Assists);
	void HandleMatchPhaseChanged(EBRMatchPhase OldPhase, EBRMatchPhase NewPhase);
	void HandleKillFeedEntryAdded(const FBRKillFeedEntry& Entry);
	void HandlePostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer);
	void HandleLogout(AGameModeBase* GameMode, AController* Exiting);
	void HandleHostingEnding(const FBRHostingEndNotice& Notice);

	FBRPlayerMatchTelemetry* FindOrAddPlayerRow(const APlayerState* Player);

	FBRPlayerMatchTelemetry* FindPlayerRowByKey(int32 PlayerKey);

	void FinalizeRecord(EBRMatchTelemetryOutcome Outcome);

	float GetServerTime() const;

	FBRMatchTelemetryRecord Record;

	TMap<FObjectKey, int32> PlayerKeys;

	int32 NextPlayerKey = 1;

	bool bFinalized = false;
	bool bMatchSourcesBound = false;
	bool bLifecycleBound = false;

	float LiveStartServerTime = 0.f;
	float WarmupStartServerTime = 0.f;

	bool bMatchIsLive = false;

	FDelegateHandle PlayerKilledHandle;
	FDelegateHandle PhaseChangedHandle;
	FDelegateHandle KillFeedHandle;
	FDelegateHandle PostLoginHandle;
	FDelegateHandle LogoutHandle;
	FDelegateHandle HostingEndingHandle;

	UPROPERTY()
	TScriptInterface<IBRServerLifecycle> BoundLifecycle;
};
```

</details>

<details open><summary><b>[8] breachpoint/Content/Data/DT_SpotterLines.csv:26-49</b> — canon <code>slice</code>, BM25 9.27 — DT_SpotterLines.csv</summary>

```text
DT_SpotterLines.csv (existing shipped table)
RowName,TriggerId,Text,Audience,Weight,RepeatCooldown_s
S09a,Kill.Melee,Melee kill.,Self,1.0,20
S09b,Kill.Melee,Contact kill.,Self,1.0,20
S09c,Kill.Melee,Melee finish.,Self,1.0,20
S10a,Kill.Grenade,Grenade kill.,Self,1.0,20
S10b,Kill.Grenade,Nowhere to go.,Self,1.0,20
S10c,Kill.Grenade,Frag kill.,Self,1.0,20
S11a,Rocket.Spawned,Rocket up at the Core.,All,1.0,8
S11b,Rocket.Spawned,Rocket is live at the Core.,All,1.0,8
S11c,Rocket.Spawned,The Core is contested.,All,1.0,8
S12b,Rocket.PickedUp.Enemy,Rocket taken.,Team,1.0,8
S12c,Rocket.PickedUp.Enemy,Power weapon lost.,Team,1.0,8
S12f,Rocket.PickedUp.Enemy,Rocket off the pad.,Team,1.0,8
S13a,Rocket.Denied,Rocket denied.,Team,1.0,8
S13b,Rocket.Denied,Denied at the Core.,Team,1.0,8
S13c,Rocket.Denied,Carrier down.,Team,1.0,8
S14a,Kill.Grapple,Grapple kill.,Self,1.0,20
S14b,Kill.Grapple,Pulled in and killed.,Self,1.0,20
S14c,Kill.Grapple,Grapple closed the gap.,Self,1.0,20
S15a,Match.Phase.Warmup,Warmup.,All,1.0,0
S15b,Match.Phase.Warmup,Bots filling empty slots.,All,1.0,0
S15c,Match.Phase.Warmup,Standing by.,All,1.0,0
S16a,Match.Phase.Live,Match live.,All,1.0,0
S16b,Match.Phase.Live,Weapons hot.,All,1.0,0
S16c,Match.Phase.Live,Team Slayer. Live.,All,1.0,0
```

</details>

### Sources the spotter cited across its pool

- `[1] BRTelemetrySubsystem.h (TeamId field confirms team identity is tracked)`
- `[1] BRTelemetrySubsystem.h (bJoinedInProgress confirms late arrival is a tracked case)`
- `[1] BRTelemetrySubsystem.h:53-90 (FBRPlayerMatchTelemetry.Assists)`
- `[1] BRTelemetrySubsystem.h:53-90 (FBRPlayerMatchTelemetry.Deaths)`
- `[1] BRTelemetrySubsystem.h:53-90 (FBRPlayerMatchTelemetry.FriendlyFireKills)`
- `[1] BRTelemetrySubsystem.h:53-90 (FBRPlayerMatchTelemetry.Kills)`
- `[1] BRTelemetrySubsystem.h:53-90 (FBRPlayerMatchTelemetry.SelfInflictedDeaths)`
- `[1] BRTelemetrySubsystem.h:53-90 (FBRPlayerMatchTelemetry.TimeInMatchSeconds)`
- `[3] GDD:282-298 (Spotter coach-line pattern)`
- `[3] GDD:282-298 (Spotter coach-line pattern: stat — correction)`
- `[8] DT_SpotterLines.csv (Kill.Grenade / Rocket triggers confirm explosive weapons exist)`
- `[8] DT_SpotterLines.csv (Kill.Grenade / Rocket triggers)`
- `[8] DT_SpotterLines.csv (Kill.Grenade trigger confirms grenades exist)`

### Generated output (post-judge, post-review, as landed)

```json
[
 {
  "RowName": "C01",
  "ConditionId": "RepeatedDeaths",
  "TelemetryField": "Deaths",
  "Comparison": ">",
  "Threshold": 12,
  "Text": "{Deaths} deaths this match. Peek less, hold cover more.",
  "Priority": 2
 },
 {
  "RowName": "C02",
  "ConditionId": "HighDeathCount",
  "TelemetryField": "Deaths",
  "Comparison": ">=",
  "Threshold": 15,
  "Text": "Died {Deaths} times. Disengage sooner \u2014 reposition before the fight, not after.",
  "Priority": 1
 },
 {
  "RowName": "C03",
  "ConditionId": "MinorAssistGap",
  "TelemetryField": "Assists",
  "Comparison": ">=",
  "Threshold": 5,
  "Text": "{Assists} assists. Damage without kills wins nothing \u2014 commit to the last hit.",
  "Priority": 4
 },
 {
  "RowName": "C04",
  "ConditionId": "HighAssistLowFinish",
  "TelemetryField": "Assists",
  "Comparison": ">=",
  "Threshold": 10,
  "Text": "{Assists} assists this match. You're softening kills, not closing them \u2014 finish what you start.",
  "Priority": 3
 },
 {
  "RowName": "C05",
  "ConditionId": "SelfInflictedRepeat",
  "TelemetryField": "SelfInflictedDeaths",
  "Comparison": ">",
  "Threshold": 1,
  "Text": "{SelfInflictedDeaths} self-inflicted deaths. Check your range before the shot, not after.",
  "Priority": 5
 },
 {
  "RowName": "C06",
  "ConditionId": "SelfInflictedAny",
  "TelemetryField": "SelfInflictedDeaths",
  "Comparison": ">=",
  "Threshold": 1,
  "Text": "{SelfInflictedDeaths} self-inflicted deaths. Stop firing point-blank \u2014 you're not the only one in range.",
  "Priority": 6
 },
 {
  "RowName": "C07",
  "ConditionId": "FriendlyFireElevated",
  "TelemetryField": "FriendlyFireKills",
  "Comparison": ">=",
  "Threshold": 3,
  "Text": "{FriendlyFireKills} friendly fire kills this match. Check team ID before you fire in a scrum.",
  "Priority": 7
 },
 {
  "RowName": "C08",
  "ConditionId": "FriendlyFirePresent",
  "TelemetryField": "FriendlyFireKills",
  "Comparison": ">=",
  "Threshold": 2,
  "Text": "{FriendlyFireKills} friendly fire kills. Confirm the target before you commit the shot.",
  "Priority": 8
 },
 {
  "RowName": "C09",
  "ConditionId": "MinimalMatchPresence",
  "TelemetryField": "TimeInMatchSeconds",
  "Comparison": "<=",
  "Threshold": 120,
  "Text": "{TimeInMatchSeconds}s of match time logged. Low presence \u2014 you're either dying fast or arriving late.",
  "Priority": 9
 },
 {
  "RowName": "C10",
  "ConditionId": "BriefMatchPresence",
  "TelemetryField": "TimeInMatchSeconds",
  "Comparison": "<",
  "Threshold": 240,
  "Text": "{TimeInMatchSeconds}s of time in match. Short match presence \u2014 check your spawn-to-fight routing.",
  "Priority": 10
 },
 {
  "RowName": "C11",
  "ConditionId": "ExtremelyLowKills",
  "TelemetryField": "Kills",
  "Comparison": "<",
  "Threshold": 3,
  "Text": "{Kills} kills. Low count \u2014 contest more, retreat less.",
  "Priority": 11
 },
 {
  "RowName": "C12",
  "ConditionId": "VeryLowKillCount",
  "TelemetryField": "Kills",
  "Comparison": "<",
  "Threshold": 4,
  "Text": "{Kills} kills logged. Low finish count \u2014 close distance before you commit to the fight.",
  "Priority": 12
 }
]
```

> **What the un-tweaked retriever would have returned instead:** `breachpoint/BREACHPOINT-GDD-FULL-CONCEPT.md:243-258`, `breachpoint/BREACHPOINT-GDD-FULL-CONCEPT.md:414-419`, `breachpoint/BREACHPOINT-GDD-FULL-CONCEPT.md:288-299` — see README §The retrieval tweak.

## callsigns

**Query** (`scope=slice`, boost `{'DT_BotTuning.csv': 2.5}`)

```
bots fill every unfilled slot difficulty profiles Recruit Marine Veteran reaction accuracy cover preference killfeed scoreboard
```

### Retrieved chunks

<details open><summary><b>[1] breachpoint/Content/Data/DT_BotTuning.csv:2-4</b> — canon <code>slice</code>, BM25 26.97 — DT_BotTuning.csv</summary>

```text
DT_BotTuning.csv (existing shipped table)
Name,reaction_ms,reaction_quantum_ms,reaction_jitter_ms,accuracy_pct,aim_error_deg,switch_margin,commit_window_ms,commit_jitter_ms,rocket_contest,push_threshold,cover_preference,sight_radius_m,sight_fov_deg,target_memory_s,engage_update_ms,StateTreeSoftPath
Recruit,500,20,120,0.25,8.0,0.30,1400,400,0.30,1.00,0.45,35.0,90.0,2.0,600,/Game/AI/ST_Bot.ST_Bot
Marine,320,20,80,0.45,5.0,0.20,900,300,0.60,0.95,0.65,35.0,90.0,3.5,350,/Game/AI/ST_Bot.ST_Bot
Veteran,220,20,60,0.65,3.0,0.15,600,200,1.00,0.85,0.85,35.0,90.0,5.0,220,/Game/AI/ST_Bot.ST_Bot
```

</details>

<details open><summary><b>[2] breachpoint/BREACHPOINT-GDD-VERTICAL-SLICE.md:195-210</b> — canon <code>slice</code>, BM25 24.2 — BREACHPOINT — VERTICAL SLICE > 2. Game Mechanics (Player-Facing Actions and Loop) > 2.8 Bots — one brain, dialed</summary>

```text
BREACHPOINT — VERTICAL SLICE > 2. Game Mechanics (Player-Facing Actions and Loop) > 2.8 Bots — one brain, dialed

Bots occupy every unfilled slot. The slice ships **one authored behavior
profile** scaled by a difficulty multiplier — giving three player-facing
difficulties without authoring three behavior trees:

| Setting | Accuracy | Reaction | Grenade use | Behavior |
|---|---|---|---|---|
| Recruit | 25% | 500 ms | Rare | Same StateTree, dulled |
| Marine *(default)* | 45% | 320 ms | Situational | Baseline profile |
| Veteran | 65% | 220 ms | Tactical | Same StateTree, sharpened |

All settings drive the **same** StateTree and EQS queries — only
`DT_BotTuning` scalars change. Bots activate abilities through the same
input path a human uses: no aimbot hooks, no privileged state, no
side-channel damage. This is what makes bot-vs-bot soak tests exercise
the real combat code (§4.5).
```

</details>

<details open><summary><b>[3] breachpoint/BREACHPOINT-GDD-VERTICAL-SLICE.md:49-55</b> — canon <code>slice</code>, BM25 13.76 — BREACHPOINT — VERTICAL SLICE > 1. Executive Summary > 1.3 Mode</summary>

```text
BREACHPOINT — VERTICAL SLICE > 1. Executive Summary > 1.3 Mode

| Mode | Players | Description |
|---|---|---|
| **Team Slayer** | 4v4 (humans + bots in any mix) | The only shipped mode. Bots fill every unfilled slot. |

Solo play is Team Slayer with seven bots. **PvE is not a separate
game** — it is the same code with the roster changed.
```

</details>

<details open><summary><b>[4] breachpoint/BREACHPOINT-GDD-VERTICAL-SLICE.md:399-406</b> — canon <code>slice</code>, BM25 11.17 — BREACHPOINT — VERTICAL SLICE > 5. Scope and Schedule > 5.1 Shipped Scope</summary>

```text
BREACHPOINT — VERTICAL SLICE > 5. Scope and Schedule > 5.1 Shipped Scope

Team Slayer 4v4 (bots filling any slot, 3 difficulty settings); **1**
three-level arena map; **3** weapons (AR, Magnum, Rocket on a 90 s
timer); frag grenades; melee including rear-kill; **Grappleshot**;
shields-over-health; scored respawns; CommonUI HUD + minimal front end;
canned medals + killfeed; Spotter agent with canned fallback; MetaSounds
combat and footstep audio; Steam listen server + demo depot.
**Nothing else.**
```

</details>

<details open><summary><b>[5] breachpoint/BREACHPOINT-GDD-VERTICAL-SLICE.md:24-38</b> — canon <code>slice</code>, BM25 9.68 — BREACHPOINT — VERTICAL SLICE > 1. Executive Summary > 1.1 Concept</summary>

```text
BREACHPOINT — VERTICAL SLICE > 1. Executive Summary > 1.1 Concept

**Breachpoint** is a **4v4 team arena first-person shooter** built on the
Halo sandbox model. Recharging **shields over finite health** give every
fight a break-off point. A **two-weapon carry limit** and one contested
power weapon make the map itself an objective. The **golden triangle** —
shoot, grenade, melee — is available in every engagement, and a
**Grappleshot** turns a three-level arena into a traversal playground:
pull to a ledge, yank a rocket off the floor, close on a reloading enemy.

Bots fill every empty slot and fight through the same input path a human
uses, so a match always starts.

This document specifies a **vertical slice**: the complete Halo feel at
narrow breadth. Everything cut is listed in §6 with its Phase-2 restore
path — nothing is silently dropped.
```

</details>

<details open><summary><b>[6] breachpoint/BREACHPOINT-GDD-VERTICAL-SLICE.md:264-279</b> — canon <code>slice</code>, BM25 9.14 — BREACHPOINT — VERTICAL SLICE > 3. AI Architecture (What Each Agent Does, Through Its Effect on Gameplay) > 3.2 The Dev-Time Crew (Claude + Unreal MCP)</summary>

```text
BREACHPOINT — VERTICAL SLICE > 3. AI Architecture (What Each Agent Does, Through Its Effect on Gameplay) > 3.2 The Dev-Time Crew (Claude + Unreal MCP)

The formulated 12-agent crew carries over unchanged; only its products
change. Every output is reviewable data (JSON/DataTables), refuted by the
critic, landed by a builder. **Agents never commit directly.**

The content crew is **three agents** (consolidated per the S03 scope
red-flag — see the Assignment #2 GDD §5.4):

| Agent | Product for the slice |
|---|---|
| **Arena Architect** *(the One Wow agent)* | `arena_manifest.json` — three-level layout, grapple points (≥2 per upper position), scored spawns, rocket node, 35 m sightline cap |
| **Tuning Curator** | ALL gameplay numbers: `DT_Weapons` rows with TTK analysis, `DT_BotTuning` profiles + difficulty scalars, and balance diffs (proposes only outside the 45–55% band) |
| **Spotter** | The runtime agent (§3.3) — coach + killfeed strings, moderated before display |

*(Nightly bot-vs-bot soaks — stuck navmesh, spawn-kill loops, TTK
regressions — are the verifier's automated testing, not a content agent.)*
```

</details>

### Sources the spotter cited across its pool

- `[1] breachpoint/Content/Data/DT_BotTuning.csv:2-4`
- `[2] breachpoint/BREACHPOINT-GDD-VERTICAL-SLICE.md:195-210`

### Generated output (post-judge, post-review, as landed)

```json
[
 {
  "RowName": "B01",
  "Callsign": "Dulledge",
  "ProfileHint": "Recruit",
  "Note": "GDD: 'Same StateTree, dulled' is the exact descriptor for the Recruit behavior column"
 },
 {
  "RowName": "B02",
  "Callsign": "Softaim",
  "ProfileHint": "Recruit",
  "Note": "accuracy_pct=0.25, lowest of the three rows"
 },
 {
  "RowName": "B03",
  "Callsign": "Shakygrip",
  "ProfileHint": "Recruit",
  "Note": "reaction_jitter_ms=120, highest jitter of the three rows"
 },
 {
  "RowName": "B04",
  "Callsign": "Slowdraw",
  "ProfileHint": "Recruit",
  "Note": "reaction_ms=500 (slowest of the three profiles) \u2014 the callsign names the delay itself"
 },
 {
  "RowName": "B05",
  "Callsign": "Wideshot",
  "ProfileHint": "Recruit",
  "Note": "aim_error_deg=8.0, widest cone of the three rows, hence a shot that misses wide"
 },
 {
  "RowName": "B06",
  "Callsign": "Midpace",
  "ProfileHint": "Marine",
  "Note": "reaction_ms=320, exact middle of 500/320/220 across the three rows"
 },
 {
  "RowName": "B07",
  "Callsign": "Evenkeel",
  "ProfileHint": "Marine",
  "Note": "reaction_jitter_ms=80, mid value between Recruit's 120 and Veteran's 60"
 },
 {
  "RowName": "B08",
  "Callsign": "Steadyaim",
  "ProfileHint": "Marine",
  "Note": "accuracy_pct=0.45, the middle value of the three rows"
 },
 {
  "RowName": "B09",
  "Callsign": "Coverwise",
  "ProfileHint": "Marine",
  "Note": "cover_preference=0.65, mid value between Recruit's 0.45 and Veteran's 0.85"
 },
 {
  "RowName": "B10",
  "Callsign": "Tightline",
  "ProfileHint": "Marine",
  "Note": "aim_error_deg=5.0, mid value between Recruit's 8.0 and Veteran's 3.0"
 },
 {
  "RowName": "B11",
  "Callsign": "Honedline",
  "ProfileHint": "Veteran",
  "Note": "GDD: 'Same StateTree, sharpened' is the exact descriptor for the Veteran behavior column"
 },
 {
  "RowName": "B12",
  "Callsign": "Tightaim",
  "ProfileHint": "Veteran",
  "Note": "aim_error_deg=3.0, lowest (tightest) of the three rows"
 },
 {
  "RowName": "B13",
  "Callsign": "Coverlock",
  "ProfileHint": "Veteran",
  "Note": "cover_preference=0.85, highest of the three rows"
 },
 {
  "RowName": "B14",
  "Callsign": "Longtrack",
  "ProfileHint": "Veteran",
  "Note": "target_memory_s=5.0, highest of the three rows"
 },
 {
  "RowName": "B15",
  "Callsign": "Deadeye",
  "ProfileHint": "Veteran",
  "Note": "accuracy_pct=0.65, highest of the three rows"
 }
]
```

> **What the un-tweaked retriever would have returned instead:** `breachpoint/BREACHPOINT-GDD-FULL-CONCEPT.md:171-182`, `breachpoint/BREACHPOINT-GDD-FULL-CONCEPT.md:38-47` — see README §The retrieval tweak.
