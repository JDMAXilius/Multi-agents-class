# Critic log — what was caught, and the correction

This is the REFUTER pass, which runs on the survivors of the JUDGE pass
(`output/judge_log.md`). `before` and `after` are the spotter's own rows,
captured either side of review; the diff is computed by the pipeline.

## announcer — verdict `FINDINGS`, 2 finding(s), 0 blocking

### `S22b` — redundancy (medium)

- **objected to:** `Multi-kill. Rocket.`
- **canon cited:** Compare to established triads e.g. DT_SpotterLines.csv S03a-c: "Double kill." / "Two down." / "Back to back." — three lexically distinct phrasings for one trigger.
- **why:** S22a ("Rocket multi-kill.") and S22b ("Multi-kill. Rocket.") are the same two words reordered, so of the three lines meant to give spoken variety for Kill.Rocket.Multi, only two distinct ideas actually exist — a player will hear what sounds like the same callout twice.
- **proposed fix:** `Replace S22b with a genuinely distinct phrasing, e.g. "Two with one rocket." (echoing the Blast Radius medal description "Two or more killed with one rocket").`

### `S22c` — tone-drift (low)

- **objected to:** `Group kill. Rocket.`
- **why:** "Group kill" is not terminology used anywhere in the shipped medal or spotter tables, which consistently say "Multi" (Kill.Multi.Double, Kill.Rocket.Multi) or the medal name "Blast Radius" — introducing a new synonym breaks the established callout vocabulary the other rows in this table follow.
- **proposed fix:** `"Blast radius." (directly echoing the M4 medal name, matching how S04b echoes "Killing Spree" and S08c echoes "Blindside").`

_No blocking finding, so no row changed._

## coach — verdict `FINDINGS`, 3 finding(s), 3 blocking

### `C02` — redundancy (high)

- **objected to:** `Died {Deaths} times. Disengage sooner — reposition before the fight, not after.`
- **why:** C02 (Deaths>=15) is a strict subset of C01 (Deaths>12), and C01's Priority (1) beats C02's Priority (2) under the standard lower-number-wins convention required to keep the SelfInflictedDeaths/FriendlyFire/TimeInMatch/Kills pairs functional — so C02 never fires for any Deaths value; a player with 20 deaths still gets C01's milder 'Peek less, hold cover more' line.
- **proposed fix:** `Swap the Priority values: give C02 the lower/dominant number (e.g. C01=2, C02=1) so the more-severe row wins the overlap, matching the ordering convention used by every other field-pair.`

### `C04` — redundancy (high)

- **objected to:** `{Assists} assists this match. You're softening kills, not closing them — finish what you start.`
- **why:** C04 (Assists>=10) is a strict subset of C03 (Assists>=5), and C03's Priority (3) beats C04's Priority (4) under the same convention, so C04 never fires for any Assists value — a player with 14 assists still gets C03's generic line instead of the escalated one written for them.
- **proposed fix:** `Swap the Priority values: give C04 the lower/dominant number (e.g. C03=4, C04=3) so the more-severe row wins the overlap, matching the ordering convention used by every other field-pair.`

### `table-wide` — schema-risk (high)

- **objected to:** `Priority: 1..12 (sequential, one per row)`
- **why:** The Priority column encodes two opposite severity-ordering conventions: for SelfInflictedDeaths/FriendlyFireKills/TimeInMatchSeconds/Kills, the narrower more-severe row has the lower Priority number (correct under a lower-wins resolver); for Deaths/Assists it's reversed — the narrower more-severe row (C02/C04) has the higher number. No single consistent tie-break rule makes all 12 rows reachable: lower-wins kills C02/C04, higher-wins kills C05/C07/C09/C11 instead, silently substituting the milder sibling line at thresholds meant to trigger the severe one.
- **proposed fix:** `Renumber Priority so that, within every pair, the row whose condition range is the subset (the more specific/severe threshold) always gets the priority value that wins ties under whatever resolver is implemented — pick one direction and apply it uniformly across all six pairs.`

### Correction applied

| Row | Before | After |
|---|---|---|
| `C01`.Priority | `1` | `2` |
| `C02`.Priority | `2` | `1` |
| `C03`.Priority | `3` | `4` |
| `C04`.Priority | `4` | `3` |

## callsigns — verdict `FINDINGS`, 2 finding(s), 0 blocking

### `B02` — tone-drift (medium)

- **objected to:** `Softaim / Shakygrip / Slowdraw / Wideshot / Midpace / Evenkeel / Steadyaim / Coverwise / Tightline / Tightaim / Coverlock / Longtrack / Deadeye`
- **canon cited:** [1] DT_BotTuning.csv row schema: Name,reaction_ms,reaction_quantum_ms,... — the only Name values that exist in the shipped table are 'Recruit', 'Marine', 'Veteran'. No Callsign/personality field exists anywhere in the retrieved canon.
- **why:** 13 of the 15 rows invent generic-shooter nickname tropes (Deadeye, Wideshot, Slowdraw, Shakygrip, Softaim, etc.) for a table whose canon precedent is three sober tier names and a spreadsheet of tuning scalars — nothing in the shipped data or the GDD prose establishes a callsign-naming convention, so this register doesn't match what actually ships. (B01 'Dulledge' and B11 'Honedline' are the exception — those are tightly grounded in the GDD's literal 'dulled'/'sharpened' descriptors and are fine.)
- **proposed fix:** `Either drop the Callsign field entirely and key rows by Name+index (e.g. Recruit_01..05), or if callsigns are wanted, derive all 15 the way B01/B11 were derived — from the GDD's own descriptor language for that tier — rather than inventing unrelated FPS nickname tropes.`

### `B01` — schema-risk (medium)

- **objected to:** `"RowName": "B01", "Callsign": "Dulledge", "ProfileHint": "Recruit"`
- **canon cited:** [1] DT_BotTuning.csv header: Name,reaction_ms,reaction_quantum_ms,reaction_jitter_ms,accuracy_pct,aim_error_deg,switch_margin,commit_window_ms,commit_jitter_ms,rocket_contest,push_threshold,cover_preference,sight_radius_m,sight_fov_deg,target_memory_s,engage_update_ms,StateTreeSoftPath
- **why:** None of RowName, Callsign, ProfileHint, or Note exist in the shipped DT_BotTuning schema. If this content is meant to land as new rows in that table it breaks import (unknown columns / no numeric scalars at all); if it's meant to be a separate roster table, the reviewed content never names that table or its relationship to the 3 canonical tuning rows, so there's no defined destination for this data.
- **proposed fix:** `State explicitly which table/asset this roster targets and how ProfileHint resolves to the DT_BotTuning row it inherits scalars from (e.g. a foreign-key-style lookup), rather than leaving it implicit.`

_No blocking finding, so no row changed._
