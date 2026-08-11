# RAG trace — query, retrieved chunk, generated output, side by side

One section per content type. The chunk text below is *verbatim* what the
generator received; the rows below it are what came back. Every citation is
`file:line-line` against this repo, so any claim here can be opened and checked.

## announcer

**Query** (`scope=slice`, boost `{'DT_SpotterLines.csv': 2.5, 'DT_Medals.csv': 2.0}`)

```
announcer callout line medal killfeed rocket multi kill first kill of the match sudden death killing spree ended clipped military voice
```

### Retrieved chunks

<details open><summary><b>[1] breachpoint/Content/Data/DT_Medals.csv:2-12</b> — canon <code>slice</code>, BM25 38.41 — DT_Medals.csv</summary>

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

<details open><summary><b>[2] breachpoint/Content/Data/DT_SpotterLines.csv:50-64</b> — canon <code>slice</code>, BM25 22.54 — DT_SpotterLines.csv</summary>

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

<details open><summary><b>[3] breachpoint/BREACHPOINT-GDD-VERTICAL-SLICE.md:213-227</b> — canon <code>slice</code>, BM25 22.33 — BREACHPOINT — VERTICAL SLICE > 2. Game Mechanics (Player-Facing Actions and Loop) > 2.9 What the Player Sees (HUD)</summary>

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

<details open><summary><b>[4] breachpoint/BREACHPOINT-GDD-VERTICAL-SLICE.md:282-298</b> — canon <code>slice</code>, BM25 14.5 — BREACHPOINT — VERTICAL SLICE > 3. AI Architecture (What Each Agent Does, Through Its Effect on Gameplay) > 3.3 The Runtime Agent — Spotter</summary>

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

<details open><summary><b>[5] breachpoint/BREACHPOINT-GDD-VERTICAL-SLICE.md:41-46</b> — canon <code>slice</code>, BM25 14.5 — BREACHPOINT — VERTICAL SLICE > 1. Executive Summary > 1.2 Win and Loss Conditions</summary>

```text
BREACHPOINT — VERTICAL SLICE > 1. Executive Summary > 1.2 Win and Loss Conditions

- **Win:** first team to **25 kills**, or the higher team score when the
  **8:00** timer expires.
- **Tiebreak:** sudden death — no respawns, first kill wins, **60-second
  cap**; if uncontested, higher team damage dealt wins.
- **Loss:** any other scoreline at the timer.
```

</details>

<details open><summary><b>[6] breachpoint/BREACHPOINT-GDD-VERTICAL-SLICE.md:528-544</b> — canon <code>slice</code>, BM25 12.02 — BREACHPOINT — VERTICAL SLICE > Appendix A — Combat Tuning (first pass)</summary>

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

<details open><summary><b>[7] breachpoint/Content/Data/DT_SpotterLines.csv:2-25</b> — canon <code>slice</code>, BM25 11.01 — DT_SpotterLines.csv</summary>

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

### Generated output (post-review, as landed)

```json
[
 {
  "RowName": "S22a",
  "TriggerId": "Kill.Rocket.Multi",
  "Text": "Rocket kill. Multiple down.",
  "Audience": "Self",
  "Weight": 1.0,
  "RepeatCooldown_s": 20
 },
 {
  "RowName": "S22b",
  "TriggerId": "Kill.Rocket.Multi",
  "Text": "Blast radius.",
  "Audience": "Self",
  "Weight": 1.0,
  "RepeatCooldown_s": 20
 },
 {
  "RowName": "S22c",
  "TriggerId": "Kill.Rocket.Multi",
  "Text": "Two down. One rocket.",
  "Audience": "Self",
  "Weight": 1.0,
  "RepeatCooldown_s": 20
 },
 {
  "RowName": "S23a",
  "TriggerId": "Kill.First",
  "Text": "First kill.",
  "Audience": "Self",
  "Weight": 1.0,
  "RepeatCooldown_s": 20
 },
 {
  "RowName": "S23b",
  "TriggerId": "Kill.First",
  "Text": "Opening kill scored.",
  "Audience": "Self",
  "Weight": 1.0,
  "RepeatCooldown_s": 20
 },
 {
  "RowName": "S23c",
  "TriggerId": "Kill.First",
  "Text": "Match's first kill.",
  "Audience": "Self",
  "Weight": 1.0,
  "RepeatCooldown_s": 20
 },
 {
  "RowName": "S24a",
  "TriggerId": "Match.SuddenDeath.Win",
  "Text": "Sudden death won.",
  "Audience": "Team",
  "Weight": 1.0,
  "RepeatCooldown_s": 0
 },
 {
  "RowName": "S24b",
  "TriggerId": "Match.SuddenDeath.Win",
  "Text": "Final kill. Match won.",
  "Audience": "Team",
  "Weight": 1.0,
  "RepeatCooldown_s": 0
 },
 {
  "RowName": "S24c",
  "TriggerId": "Match.SuddenDeath.Win",
  "Text": "Last kill. Match complete.",
  "Audience": "Team",
  "Weight": 1.0,
  "RepeatCooldown_s": 0
 },
 {
  "RowName": "S25a",
  "TriggerId": "Kill.SpreeEnder",
  "Text": "Enemy spree ended.",
  "Audience": "Self",
  "Weight": 1.0,
  "RepeatCooldown_s": 20
 },
 {
  "RowName": "S25b",
  "TriggerId": "Kill.SpreeEnder",
  "Text": "Their spree, stopped.",
  "Audience": "Self",
  "Weight": 1.0,
  "RepeatCooldown_s": 20
 },
 {
  "RowName": "S25c",
  "TriggerId": "Kill.SpreeEnder",
  "Text": "Spree's over.",
  "Audience": "Self",
  "Weight": 1.0,
  "RepeatCooldown_s": 20
 }
]
```

> **What the un-tweaked retriever would have returned instead:** `breachpoint/BREACHPOINT-GDD-FULL-CONCEPT.md:185-197`, `breachpoint/BREACHPOINT-GDD-FULL-CONCEPT.md:29-35`, `breachpoint/BREACHPOINT-GDD-FULL-CONCEPT.md:243-258` — see README §The retrieval tweak.

## coach

**Query** (`scope=slice`, boost `{'DT_SpotterLines.csv': 1.5}`)

```
spotter coach line match end telemetry stat fights lost below 40% shields accuracy grapple rocket holds shield break conversion canned fallback no connectivity
```

### Retrieved chunks

<details open><summary><b>[1] breachpoint/BREACHPOINT-GDD-VERTICAL-SLICE.md:556-562</b> — canon <code>slice</code>, BM25 46.19 — BREACHPOINT — VERTICAL SLICE > Appendix C — Telemetry Schema</summary>

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

<details open><summary><b>[2] breachpoint/BREACHPOINT-GDD-VERTICAL-SLICE.md:282-298</b> — canon <code>slice</code>, BM25 43.47 — BREACHPOINT — VERTICAL SLICE > 3. AI Architecture (What Each Agent Does, Through Its Effect on Gameplay) > 3.3 The Runtime Agent — Spotter</summary>

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

<details open><summary><b>[3] breachpoint/BREACHPOINT-GDD-VERTICAL-SLICE.md:213-227</b> — canon <code>slice</code>, BM25 21.26 — BREACHPOINT — VERTICAL SLICE > 2. Game Mechanics (Player-Facing Actions and Loop) > 2.9 What the Player Sees (HUD)</summary>

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

<details open><summary><b>[4] breachpoint/BREACHPOINT-GDD-VERTICAL-SLICE.md:409-423</b> — canon <code>slice</code>, BM25 14.91 — BREACHPOINT — VERTICAL SLICE > 5. Scope and Schedule > 5.2 Schedule — six weeks, each ending runnable</summary>

```text
BREACHPOINT — VERTICAL SLICE > 5. Scope and Schedule > 5.2 Schedule — six weeks, each ending runnable

> **`BREACHPOINT-ROADMAP.md` is authoritative for sequencing.** The table
> below is the original week-by-week sketch; the roadmap re-cut it into six
> parallel pods and moved work accordingly (Rocket and HUD v1 pull into W3,
> bots start W3 and complete at M4, Steam holds at W5). Where the two
> disagree, follow the roadmap.

| Wk | Deliverable | Gate |
|---|---|---|
| **1** | Ladder bootstrap (specs + Gauntlet skeleton); FPS template + module layout; **GAS core ported**; **shields/health**; AR firing; `IBRServerLifecycle` stub | Breaking a dummy's shields feels good |
| **2** | Magnum + two-weapon carry + swap; **grenades**; **melee + rear-kill** | ⚠️ **THE GOLDEN TRIANGLE FUN TEST** — see §7.2 |
| **3** | **Grappleshot** (netcode packet + REFUTER pass); map blockout from Arena Architect manifest | Traversal is fun; grapple is used offensively |
| **4** | Bots (StateTree + EQS + 3 scalars); Team Slayer scoring, teams, scored respawns; nightly soaks begin | ⚠️ **GO / NO-GO** — a full 4v4 match vs. bots plays end-to-end |
| **5** | Rocket Launcher + 90 s timer; CommonUI HUD; Steam listen server + host/invite; MetaSounds cues | Two humans + six bots, online, full match |
| **6** | Balance pass (crew telemetry); polish; Steam demo depot; capstone presentation | **Shipped** |
```

</details>

<details open><summary><b>[5] breachpoint/BREACHPOINT-GDD-VERTICAL-SLICE.md:426-440</b> — canon <code>slice</code>, BM25 14.73 — BREACHPOINT — VERTICAL SLICE > 5. Scope and Schedule > 5.3 Cut Order (pre-declared — used, not improvised)</summary>

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

<details open><summary><b>[6] breachpoint/BREACHPOINT-GDD-VERTICAL-SLICE.md:399-406</b> — canon <code>slice</code>, BM25 13.37 — BREACHPOINT — VERTICAL SLICE > 5. Scope and Schedule > 5.1 Shipped Scope</summary>

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

<details open><summary><b>[7] breachpoint/BREACHPOINT-GDD-VERTICAL-SLICE.md:329-342</b> — canon <code>slice</code>, BM25 9.94 — BREACHPOINT — VERTICAL SLICE > 4. Technical Strategy > 4.2 Token Budget (runtime, per match)</summary>

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

<details open><summary><b>[8] breachpoint/BREACHPOINT-GDD-VERTICAL-SLICE.md:113-123</b> — canon <code>slice</code>, BM25 8.19 — BREACHPOINT — VERTICAL SLICE > 2. Game Mechanics (Player-Facing Actions and Loop) > 2.3 Shields Over Health — the signature system</summary>

```text
BREACHPOINT — VERTICAL SLICE > 2. Game Mechanics (Player-Facing Actions and Loop) > 2.3 Shields Over Health — the signature system

Two damage layers, both GAS attributes on the PlayerState-owned ASC:

- **Shields (100)** — recharge at 60/s beginning **2.5 s** after the last
  damage taken. Distinct audio + visual cue on break and on recharge.
- **Health (100)** — **never regenerates.** The only reset is death.

Consequences that define the pacing: fights have a natural break-off
point; a wounded player is a hunted player; re-engaging is a real
decision, not a reflex. **Built first (Week 1)** because every other
number is tuned against it.
```

</details>

### Generated output (post-review, as landed)

```json
[
 {
  "RowName": "C01",
  "ConditionId": "LostFightsLowShields",
  "TelemetryField": "fights_lost_below_40_shields",
  "Threshold": 4,
  "Text": "You lost {fights_lost_below_40_shields} fights below 40% shields \u2014 break off and let them recharge before re-engaging.",
  "Priority": 1
 },
 {
  "RowName": "C02",
  "ConditionId": "LowShieldConversion",
  "TelemetryField": "shield_break_to_kill_conversion",
  "Threshold": 0.5,
  "Text": "You broke shields but only converted {shield_break_to_kill_conversion} of those breaks into kills \u2014 finish the fight before shields reset.",
  "Priority": 2
 },
 {
  "RowName": "C03",
  "ConditionId": "ShortTimeAlive",
  "TelemetryField": "time_alive",
  "Threshold": 60,
  "Text": "You were only alive for {time_alive}s total \u2014 you're pushing into fights before shields recharge, hold cover longer.",
  "Priority": 3
 },
 {
  "RowName": "C04",
  "ConditionId": "LowARAccuracy",
  "TelemetryField": "accuracy_ar",
  "Threshold": 20,
  "Text": "Your AR accuracy was {accuracy_ar}% \u2014 you're spraying at range, switch to Magnum or burst-fire past 15 meters.",
  "Priority": 4
 },
 {
  "RowName": "C05",
  "ConditionId": "LowMagnumAccuracy",
  "TelemetryField": "accuracy_magnum",
  "Threshold": 25,
  "Text": "Your Magnum accuracy was {accuracy_magnum}% \u2014 take the extra half-second to land the precision shot instead of spamming triggers.",
  "Priority": 5
 },
 {
  "RowName": "C06",
  "ConditionId": "RocketNeglected",
  "TelemetryField": "rocket_holds",
  "Threshold": 1,
  "Text": "You held the Rocket only {rocket_holds} times \u2014 it respawns every 90s, path through it to control the map.",
  "Priority": 6
 },
 {
  "RowName": "C07",
  "ConditionId": "RearMeleeFlanker",
  "TelemetryField": "melee_kills_rear",
  "Threshold": 3,
  "Text": "You landed {melee_kills_rear} rear melee kills \u2014 your flanking angles are working, keep rotating behind fights to punish backs.",
  "Priority": 7
 },
 {
  "RowName": "C08",
  "ConditionId": "GrappleAggressor",
  "TelemetryField": "grapple_kills",
  "Threshold": 3,
  "Text": "You landed {grapple_kills} grapple kills \u2014 closing distance before the shot is working, keep opening fights with the Grappleshot.",
  "Priority": 8
 },
 {
  "RowName": "C09",
  "ConditionId": "GrenadeDenial",
  "TelemetryField": "grenade_kills",
  "Threshold": 3,
  "Text": "You landed {grenade_kills} grenade kills \u2014 you're clearing angles before pushing, keep leading fights with a frag.",
  "Priority": 9
 },
 {
  "RowName": "C10",
  "ConditionId": "HighAssistsLowKills",
  "TelemetryField": "assists",
  "Threshold": 8,
  "Text": "You racked up {assists} assists \u2014 you're softening shields for teammates to finish, start closing more of those fights yourself.",
  "Priority": 10
 }
]
```

> **What the un-tweaked retriever would have returned instead:** `breachpoint/BREACHPOINT-GDD-FULL-CONCEPT.md:414-419`, `breachpoint/BREACHPOINT-GDD-FULL-CONCEPT.md:243-258`, `breachpoint/BREACHPOINT-GDD-FULL-CONCEPT.md:288-299` — see README §The retrieval tweak.

## callsigns

**Query** (`scope=slice`, boost `{'DT_BotTuning.csv': 2.0}`)

```
bots fill every unfilled slot difficulty profiles Recruit Marine Veteran killfeed scoreboard player name team slayer
```

### Retrieved chunks

<details open><summary><b>[1] breachpoint/Content/Data/DT_BotTuning.csv:2-4</b> — canon <code>slice</code>, BM25 27.02 — DT_BotTuning.csv</summary>

```text
DT_BotTuning.csv (existing shipped table)
Name,reaction_ms,reaction_quantum_ms,reaction_jitter_ms,accuracy_pct,aim_error_deg,switch_margin,commit_window_ms,commit_jitter_ms,rocket_contest,push_threshold,cover_preference,sight_radius_m,sight_fov_deg,target_memory_s,engage_update_ms,StateTreeSoftPath
Recruit,500,20,120,0.25,8.0,0.30,1400,400,0.30,1.00,0.45,35.0,90.0,2.0,600,/Game/AI/ST_Bot.ST_Bot
Marine,320,20,80,0.45,5.0,0.20,900,300,0.60,0.95,0.65,35.0,90.0,3.5,350,/Game/AI/ST_Bot.ST_Bot
Veteran,220,20,60,0.65,3.0,0.15,600,200,1.00,0.85,0.85,35.0,90.0,5.0,220,/Game/AI/ST_Bot.ST_Bot
```

</details>

<details open><summary><b>[2] breachpoint/BREACHPOINT-GDD-VERTICAL-SLICE.md:195-210</b> — canon <code>slice</code>, BM25 20.82 — BREACHPOINT — VERTICAL SLICE > 2. Game Mechanics (Player-Facing Actions and Loop) > 2.8 Bots — one brain, dialed</summary>

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

<details open><summary><b>[3] breachpoint/BREACHPOINT-GDD-VERTICAL-SLICE.md:49-55</b> — canon <code>slice</code>, BM25 19.94 — BREACHPOINT — VERTICAL SLICE > 1. Executive Summary > 1.3 Mode</summary>

```text
BREACHPOINT — VERTICAL SLICE > 1. Executive Summary > 1.3 Mode

| Mode | Players | Description |
|---|---|---|
| **Team Slayer** | 4v4 (humans + bots in any mix) | The only shipped mode. Bots fill every unfilled slot. |

Solo play is Team Slayer with seven bots. **PvE is not a separate
game** — it is the same code with the roster changed.
```

</details>

<details open><summary><b>[4] breachpoint/BREACHPOINT-GDD-VERTICAL-SLICE.md:399-406</b> — canon <code>slice</code>, BM25 15.3 — BREACHPOINT — VERTICAL SLICE > 5. Scope and Schedule > 5.1 Shipped Scope</summary>

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

<details open><summary><b>[5] breachpoint/BREACHPOINT-GDD-VERTICAL-SLICE.md:24-38</b> — canon <code>slice</code>, BM25 10.78 — BREACHPOINT — VERTICAL SLICE > 1. Executive Summary > 1.1 Concept</summary>

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

<details open><summary><b>[6] breachpoint/BREACHPOINT-GDD-VERTICAL-SLICE.md:264-279</b> — canon <code>slice</code>, BM25 9.15 — BREACHPOINT — VERTICAL SLICE > 3. AI Architecture (What Each Agent Does, Through Its Effect on Gameplay) > 3.2 The Dev-Time Crew (Claude + Unreal MCP)</summary>

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

### Generated output (post-review, as landed)

```json
[
 {
  "RowName": "B01",
  "Callsign": "HOLDFAST",
  "ProfileHint": "Recruit",
  "Note": "Reads as a bot that holds a line rather than presses it \u2014 fits Recruit's 500ms reaction and dulled aim."
 },
 {
  "RowName": "B02",
  "Callsign": "TRIPWIRE",
  "ProfileHint": "Recruit",
  "Note": "An alarm, not a shooter \u2014 matches Recruit's rare grenade use and 25% accuracy."
 },
 {
  "RowName": "B03",
  "Callsign": "BACKSTOP",
  "ProfileHint": "Recruit",
  "Note": "Last line, not first contact \u2014 suits the slowest, softest tuning row."
 },
 {
  "RowName": "B04",
  "Callsign": "FALLBACK",
  "ProfileHint": "Recruit",
  "Note": "Named for retreating, not engaging \u2014 matches Recruit's dulled StateTree behavior."
 },
 {
  "RowName": "B05",
  "Callsign": "STANDBY",
  "ProfileHint": "Recruit",
  "Note": "Waits before it acts \u2014 pairs with the 500ms reaction floor."
 },
 {
  "RowName": "B06",
  "Callsign": "LOCKSTEP",
  "ProfileHint": "Marine",
  "Note": "Moves with the squad, nothing flashy \u2014 the baseline profile in a word."
 },
 {
  "RowName": "B07",
  "Callsign": "FIREWALL",
  "ProfileHint": "Marine",
  "Note": "Solid and situational, like the 45% accuracy / situational grenade row."
 },
 {
  "RowName": "B08",
  "Callsign": "IRONCLAD",
  "ProfileHint": "Marine",
  "Note": "Dependable middle ground \u2014 no edge, no gap, just Marine's default numbers."
 },
 {
  "RowName": "B09",
  "Callsign": "WARDEN",
  "ProfileHint": "Marine",
  "Note": "Holds a post competently \u2014 matches Marine's 0.65 cover_preference, steady not sharp."
 },
 {
  "RowName": "B10",
  "Callsign": "ANCHOR",
  "ProfileHint": "Marine",
  "Note": "Keeps the line where the default profile keeps it \u2014 the baseline callsign."
 },
 {
  "RowName": "B11",
  "Callsign": "DEADBOLT",
  "ProfileHint": "Veteran",
  "Note": "Locks on and doesn't let go \u2014 fits 65% accuracy and the tightest aim_error."
 },
 {
  "RowName": "B12",
  "Callsign": "CROSSFIRE",
  "ProfileHint": "Veteran",
  "Note": "Aggressive engagement fits the sharpened StateTree and 1.00 rocket_contest."
 },
 {
  "RowName": "B13",
  "Callsign": "NIGHTFALL",
  "ProfileHint": "Veteran",
  "Note": "Quiet until it commits \u2014 matches Veteran's tactical grenade use."
 },
 {
  "RowName": "B14",
  "Callsign": "REDLINE",
  "ProfileHint": "Veteran",
  "Note": "Pushed to the fastest setting \u2014 pairs with Veteran's 220ms reaction."
 },
 {
  "RowName": "B15",
  "Callsign": "GRIDLOCK",
  "ProfileHint": "Veteran",
  "Note": "Shuts an area down cleanly \u2014 fits the 0.85 cover_preference and short commit_window."
 }
]
```

> **What the un-tweaked retriever would have returned instead:** `breachpoint/BREACHPOINT-GDD-FULL-CONCEPT.md:38-47`, `breachpoint/BREACHPOINT-GDD-FULL-CONCEPT.md:171-182` — see README §The retrieval tweak.
