# RAG trace — query, retrieved chunk, generated output, side by side

One section per content type. The chunk text below is *verbatim* what the
generator received; the rows below it are what came back. Every citation is
`file:line-line` against this repo, so any claim here can be opened and checked.

## announcer

**Query** (`scope=all`, boost `{}`)

```
announcer callout line medal killfeed rocket multi kill first kill of the match sudden death killing spree ended clipped military voice
```

### Retrieved chunks

<details open><summary><b>[1] breachpoint/BREACHPOINT-GDD-VERTICAL-SLICE.md:213-227</b> — canon <code>slice</code>, BM25 22.33 — BREACHPOINT — VERTICAL SLICE > 2. Game Mechanics (Player-Facing Actions and Loop) > 2.9 What the Player Sees (HUD)</summary>

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

<details open><summary><b>[2] breachpoint/Content/Data/DT_Medals.csv:2-12</b> — canon <code>slice</code>, BM25 19.21 — DT_Medals.csv</summary>

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

<details open><summary><b>[3] breachpoint/BREACHPOINT-GDD-FULL-CONCEPT.md:185-197</b> — canon <code>phase2</code>, BM25 15.7 — BREACHPOINT > 2. Game Mechanics (Player-Facing Actions and Loop) > 2.9 What the Player Sees (HUD)</summary>

```text
BREACHPOINT > 2. Game Mechanics (Player-Facing Actions and Loop) > 2.9 What the Player Sees (HUD)

- **Top-left:** shield bar over health bar (the two-layer read).
- **Bottom-right:** current weapon, ammo, spare mags; second weapon
  ghosted beneath.
- **Bottom-left:** motion tracker; grenade count; Grappleshot cooldown.
- **Center:** reticle with hit markers — *shield hit* and *flesh hit* are
  visually distinct (the sandbox is unreadable without this).
- **Top-center:** team score, match timer, power-weapon respawn countdown.
- **Feed:** killfeed + **medals** (Double Kill, Killing Spree, Grapple
  Kill) — canned, instant, deterministic.
- **Match end:** carnage report (K/D/A, accuracy, medals), rematch prompt.

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

<details open><summary><b>[6] breachpoint/BREACHPOINT-GDD-FULL-CONCEPT.md:29-35</b> — canon <code>phase2</code>, BM25 14.05 — BREACHPOINT > 1. Executive Summary > 1.2 Win and Loss Conditions</summary>

```text
BREACHPOINT > 1. Executive Summary > 1.2 Win and Loss Conditions

- **Win (Team Slayer):** first team to **50 kills**, or the higher team
  score when the **10:00** timer expires.
- **Win (FFA Slayer):** first player to **25 kills**, or highest at 10:00.
- **Tiebreak:** sudden death — no respawns, first kill wins, **60-second
  cap**; if uncontested, higher damage dealt wins.
- **Loss:** any other scoreline at the timer.
```

</details>

<details open><summary><b>[7] breachpoint/BREACHPOINT-GDD-FULL-CONCEPT.md:243-258</b> — canon <code>phase2</code>, BM25 13.91 — BREACHPOINT > 3. AI Architecture (What Each Agent Does, Through Its Effect on Gameplay) > 3.3 The Runtime Agent — Spotter</summary>

```text
BREACHPOINT > 3. AI Architecture (What Each Agent Does, Through Its Effect on Gameplay) > 3.3 The Runtime Agent — Spotter

**Spotter Agent** — the only model call in the shipped game, and never
load-bearing.

- **Role:** one telemetry-grounded coach line per human player at match
  end, plus optional color commentary appended to notable killfeed events
  (sprees, rocket multi-kills, grapple kills).
- **Gameplay effect:** every player leaves with one specific, earned
  correction — *"You lost 6 fights at under 40% shields — break off and
  let them recharge."*
- **Output:** `{coach_line (≤ 30 words, references ≥ 1 telemetry stat)}`;
  per event `{spotter_line (≤ 18 words) | null}`.
- **Never load-bearing:** medals and killfeed are **canned and instant**
  (Halo's announcer is deterministic by design — that's correct, not a
  limitation). Spotter lines *append*. Host-side, async, ≤ 12 calls/match,
  3 s timeout, canned fallback shipped in a DataTable.
```

</details>

### Generated output (post-review, as landed)

```json
[
 {
  "RowName": "S22a",
  "TriggerId": "Kill.Rocket.Multi",
  "Text": "One rocket. Two down.",
  "Audience": "All",
  "Weight": 1.0,
  "RepeatCooldown_s": 8
 },
 {
  "RowName": "S22b",
  "TriggerId": "Kill.Rocket.Multi",
  "Text": "Direct hit. Multiple down.",
  "Audience": "All",
  "Weight": 1.0,
  "RepeatCooldown_s": 8
 },
 {
  "RowName": "S22c",
  "TriggerId": "Kill.Rocket.Multi",
  "Text": "Rocket connects. Blast Radius.",
  "Audience": "All",
  "Weight": 1.0,
  "RepeatCooldown_s": 8
 },
 {
  "RowName": "S23a",
  "TriggerId": "Kill.First",
  "Text": "First blood. Breach secured.",
  "Audience": "All",
  "Weight": 1.0,
  "RepeatCooldown_s": 0
 },
 {
  "RowName": "S23b",
  "TriggerId": "Kill.First",
  "Text": "Opening kill. Breach.",
  "Audience": "All",
  "Weight": 1.0,
  "RepeatCooldown_s": 0
 },
 {
  "RowName": "S23c",
  "TriggerId": "Kill.First",
  "Text": "First kill of the match.",
  "Audience": "All",
  "Weight": 1.0,
  "RepeatCooldown_s": 0
 },
 {
  "RowName": "S24a",
  "TriggerId": "Match.SuddenDeath.Win",
  "Text": "Sudden death. Match over.",
  "Audience": "All",
  "Weight": 1.0,
  "RepeatCooldown_s": 0
 },
 {
  "RowName": "S24b",
  "TriggerId": "Match.SuddenDeath.Win",
  "Text": "That's the Last Word.",
  "Audience": "All",
  "Weight": 1.0,
  "RepeatCooldown_s": 0
 },
 {
  "RowName": "S24c",
  "TriggerId": "Match.SuddenDeath.Win",
  "Text": "One shot. Match won.",
  "Audience": "All",
  "Weight": 1.0,
  "RepeatCooldown_s": 0
 },
 {
  "RowName": "S25a",
  "TriggerId": "Kill.SpreeEnder",
  "Text": "Spree ended. Confirmed kill.",
  "Audience": "All",
  "Weight": 1.0,
  "RepeatCooldown_s": 8
 },
 {
  "RowName": "S25b",
  "TriggerId": "Kill.SpreeEnder",
  "Text": "Killing spree stopped cold.",
  "Audience": "All",
  "Weight": 1.0,
  "RepeatCooldown_s": 8
 },
 {
  "RowName": "S25c",
  "TriggerId": "Kill.SpreeEnder",
  "Text": "Spree Ender. Down they go.",
  "Audience": "All",
  "Weight": 1.0,
  "RepeatCooldown_s": 8
 }
]
```
