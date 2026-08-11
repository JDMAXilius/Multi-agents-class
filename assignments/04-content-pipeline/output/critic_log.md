# Critic log — what was caught, and the correction

`before` and `after` are the generator's own rows, captured on either side of
the review. The diff is computed by the pipeline, not written by hand.

## announcer — verdict `FINDINGS`, 4 finding(s), 2 blocking

### `S25a` — lore-break (high)

- **objected to:** `Spree ended.`
- **canon cited:** M11: Spree Ender,Killed the enemy who was on a killing spree.,Kill.SpreeEnder; contrast S04b: Kill.Spree,"Killing spree."
- **why:** unqualified 'Spree ended' plays for killing an enemy's spree but reads identically to a player's own spree being interrupted, misrepresenting the positive event M11 actually describes.
- **proposed fix:** `Enemy spree ended.`

### `S25b` — lore-break (high)

- **objected to:** `Killing spree stopped.`
- **canon cited:** M11: Spree Ender,Killed the enemy who was on a killing spree.,Kill.SpreeEnder; contrast S04b: Kill.Spree,"Killing spree."
- **why:** this is nearly a negation of S04b's 'Killing spree.' line and will read as the player's own spree being stopped rather than the enemy's, contradicting the trigger's actual meaning.
- **proposed fix:** `Their spree, stopped.`

### `S25c` — lore-break (medium)

- **objected to:** `Spree's over.`
- **canon cited:** M11: Spree Ender,Killed the enemy who was on a killing spree.,Kill.SpreeEnder
- **why:** same whose-spree ambiguity as S25a/S25b, and the apostrophe contraction is not used elsewhere in the canon's terse Self-line register.
- **proposed fix:** `Ended their spree.`

### `S23c` — redundancy (medium)

- **objected to:** `Match's first kill.`
- **canon cited:** S23a: Kill.First,"First kill."
- **why:** conveys the same single fact as S23a with no new information, so the trio does not provide the three-way variation the table pattern (e.g. S03a/S03b/S03c) relies on.
- **proposed fix:** `Blood first drawn.`

### Correction applied

| Row | Before | After |
|---|---|---|
| `S25a`.Text | `Spree ended.` | `Enemy spree ended.` |
| `S25b`.Text | `Killing spree stopped.` | `Their spree, stopped.` |

## coach — verdict `FINDINGS`, 1 finding(s), 0 blocking

### `C02` — schema-risk (medium)

- **objected to:** `You broke shields but only converted {shield_break_to_kill_conversion} of those breaks into kills — finish the fight before shields reset.`
- **why:** The value is interpolated raw with no unit (no '%' as used for accuracy_ar/accuracy_magnum) and the Threshold (0.5) is on a 0–1 scale while the table's other ratio-type fields (accuracy_ar: 20, accuracy_magnum: 25) are on a 0–100 scale, so either the line will read as an unnatural decimal ('converted 0.42 of those breaks') or the threshold comparison is on the wrong scale and the condition fires almost every match, undermining the 'one specific, earned correction' design.
- **proposed fix:** `You broke shields but only converted {shield_break_to_kill_conversion}% of those breaks into kills — finish the fight before shields reset. (store/compare the backing stat on the same 0–100 scale as the accuracy fields, or explicitly document it as a 0–1 fraction and set Threshold accordingly)`

_No blocking finding, so no row changed._

## callsigns — verdict `FINDINGS`, 3 finding(s), 1 blocking

### `B09` — lore-break (high)

- **objected to:** `matches Marine's 0.65 push_threshold, steady not sharp.`
- **canon cited:** DT_BotTuning.csv:3 — Marine,320,20,80,0.45,5.0,0.20,900,300,0.60,0.95,0.65,... (push_threshold=0.95; 0.65 is cover_preference)
- **why:** The note cites 0.65 as Marine's push_threshold, but the shipped table has push_threshold=0.95 for Marine — 0.65 is actually cover_preference, so the row misstates a canonical data value.
- **proposed fix:** `matches Marine's 0.65 cover_preference, steady not sharp.`

### `B08` — redundancy (medium)

- **objected to:** `Dependable middle ground — no edge, no gap, just Marine's default numbers.`
- **canon cited:** B06 LOCKSTEP note: "Moves with the squad, nothing flashy — the baseline profile in a word."
- **why:** IRONCLAD and LOCKSTEP both exist only to say 'this is the unmodified baseline profile,' so the two callsigns give Marine no distinct identities between them.
- **proposed fix:** `Reground IRONCLAD in a specific tuning trait (e.g. its 900ms commit_window or 0.20 switch_margin) instead of restating 'default, nothing special.'`

### `B04` — redundancy (medium)

- **objected to:** `Named for retreating, not engaging — matches Recruit's dulled StateTree behavior.`
- **canon cited:** B03 BACKSTOP note: "Last line, not first contact — suits the slowest, softest tuning row."
- **why:** FALLBACK and BACKSTOP both encode the identical 'avoids first contact / falls back' persona, so the two Recruit callsigns don't provide distinguishable variation.
- **proposed fix:** `Differentiate FALLBACK via a different tuning axis (e.g. its 120ms reaction_jitter or 2.0s target_memory_s) rather than repeating BACKSTOP's 'avoids engagement' theme.`

### Correction applied

| Row | Before | After |
|---|---|---|
| `B09`.Note | `Holds a post competently — matches Marine's 0.65 push_threshold, steady not sharp.` | `Holds a post competently — matches Marine's 0.65 cover_preference, steady not sharp.` |
