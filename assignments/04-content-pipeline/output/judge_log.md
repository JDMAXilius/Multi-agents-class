# Judge log — the pool, the ranking, and what was left behind

`CREW_PLAYBOOK.md` §13: divergent work is **generate -> score -> select**.
The spotter generates ~10 per slot, the critic in JUDGE mode ranks them, and
**the pipeline** — not the model — slices the top K. That last part matters:
selection is arithmetic over a ranking, so it cannot drift.

## announcer — 40 candidates generated, 12 kept

### slot `Kill.Rocket.Multi`

**Winner:** `Kill.Rocket.Multi-04` — -04 'Rocket multi-kill.' pins directly to the TriggerId and the M4/weapon-radius sources with zero invented terminology, while -02 'Multi-kill. Rocket.' says the same thing with a structurally different (inverted, two-clause) build — real variation, not a reskin.

| # | id | text | kept |
|---:|---|---|---|
| 1 | `Kill.Rocket.Multi-04` | Rocket multi-kill. | ✅ |
| 2 | `Kill.Rocket.Multi-02` | Multi-kill. Rocket. | ✅ |
| 3 | `Kill.Rocket.Multi-08` | Group kill. Rocket. | ✅ |
| 4 | `Kill.Rocket.Multi-01` | Blast radius. |  |
| 5 | `Kill.Rocket.Multi-07` | Rocket connects. Multiple down. |  |
| 6 | `Kill.Rocket.Multi-09` | Splash kill, multiple. |  |
| 7 | `Kill.Rocket.Multi-05` | Direct hit. Multi-kill. |  |
| 8 | `Kill.Rocket.Multi-10` | One rocket, two kills. |  |
| 9 | `Kill.Rocket.Multi-03` | Two down. One rocket. |  |
| 10 | `Kill.Rocket.Multi-06` | Cluster kill. |  |

**Rejected outright:**

- `Kill.Rocket.Multi-06` — 'Cluster kill' implies a cluster-munition weapon type the sandbox never shipped (only one Rocket Launcher in the tuning table) — misstates the arsenal, not just a tone miss.
- `Kill.Rocket.Multi-05` — 'Direct hit' asserts a direct-vs-splash distinction the weapon data doesn't support — the Rocket Launcher entry is radius-based (4m), so most multi-kills are splash, not direct hits; the line contradicts its own trigger condition.

### slot `Kill.First`

**Winner:** `Kill.First-08` — -08 'Breach scored.' ties the exact medal name to an active verb in one clean beat, while -02 'Breach.' is the bare medal-name restatement (a pattern the table itself uses elsewhere) — distinct structures, both grounded, neither borrows an outside voice.

| # | id | text | kept |
|---:|---|---|---|
| 1 | `Kill.First-08` | Breach scored. | ✅ |
| 2 | `Kill.First-02` | Breach. | ✅ |
| 3 | `Kill.First-07` | First down. | ✅ |
| 4 | `Kill.First-04` | Opening kill. |  |
| 5 | `Kill.First-10` | First strike. |  |
| 6 | `Kill.First-03` | First kill secured. |  |
| 7 | `Kill.First-05` | First contact. Kill confirmed. |  |
| 8 | `Kill.First-06` | Match opened with a kill. |  |
| 9 | `Kill.First-01` | First blood. |  |
| 10 | `Kill.First-09` | Opening blood. |  |

**Rejected outright:**

- `Kill.First-09` — 'Opening blood' welds a clinical word to a borrowed stock phrase; the hybrid reads as an accident of generation, not a deliberate register choice, and is strictly worse than -01's plainer version of the same borrowed idiom.
- `Kill.First-01` — 'First blood' is a stock competitive-shooter cliché lifted wholesale from outside IP — none of the 22 retrieved shipped lines use a borrowed idiom like this; every other candidate builds from the medal name or an existing row instead.

### slot `Match.SuddenDeath.Win`

**Winner:** `Match.SuddenDeath.Win-01` — -01 'Sudden death. Match won.' is a near-verbatim splice of two existing shipped rows (S17c + S18a) and is the only top candidate that names both the phase and the result explicitly; -06 gets the same explicit win with a structurally different first clause ('Final kill' vs 'Sudden death'), giving real variation rather than a synonym swap.

| # | id | text | kept |
|---:|---|---|---|
| 1 | `Match.SuddenDeath.Win-01` | Sudden death. Match won. | ✅ |
| 2 | `Match.SuddenDeath.Win-06` | Final kill. Match won. | ✅ |
| 3 | `Match.SuddenDeath.Win-08` | Match won in sudden death. | ✅ |
| 4 | `Match.SuddenDeath.Win-03` | Kill wins it. |  |
| 5 | `Match.SuddenDeath.Win-02` | Last word. |  |
| 6 | `Match.SuddenDeath.Win-04` | Sudden death kill. Match over. |  |
| 7 | `Match.SuddenDeath.Win-07` | Sudden death ends here. |  |
| 8 | `Match.SuddenDeath.Win-09` | Decisive kill. |  |
| 9 | `Match.SuddenDeath.Win-05` | That's the match. |  |
| 10 | `Match.SuddenDeath.Win-10` | Match closed. Sudden death. |  |

**Rejected outright:**

- `Match.SuddenDeath.Win-10` — 'Match closed' uses a word the shipped table never uses for outcomes (Win rows say 'won'/'Victory', Loss rows say 'lost'/'Defeat'), and the line never states which side won — for a Win-specific trigger that's a real ambiguity, not just a style miss.
- `Match.SuddenDeath.Win-09` — 'Decisive kill' editorializes with a judgment adjective the table never uses, and still fails to state that the match was won — it would fire identically on any dramatic sudden-death kill regardless of outcome.

### slot `Kill.SpreeEnder`

**Winner:** `Kill.SpreeEnder-01` — -01 'Spree ended.' matches S04b's register exactly and states the outcome plainly; -04 'Spree broken.' delivers the same fact with a genuinely different verb rather than a synonym-swap of -01, earning the second slot on real variation.

| # | id | text | kept |
|---:|---|---|---|
| 1 | `Kill.SpreeEnder-01` | Spree ended. | ✅ |
| 2 | `Kill.SpreeEnder-04` | Spree broken. | ✅ |
| 3 | `Kill.SpreeEnder-02` | Spree ender. | ✅ |
| 4 | `Kill.SpreeEnder-03` | Killing spree stopped. |  |
| 5 | `Kill.SpreeEnder-07` | Their spree ends here. |  |
| 6 | `Kill.SpreeEnder-06` | Spree killer down. |  |
| 7 | `Kill.SpreeEnder-10` | Killing spree, ended. |  |
| 8 | `Kill.SpreeEnder-08` | Spree stopped cold. |  |
| 9 | `Kill.SpreeEnder-05` | Streak ended. |  |
| 10 | `Kill.SpreeEnder-09` | Run ended. |  |

**Rejected outright:**

- `Kill.SpreeEnder-09` — 'Run ended.' is vague enough to be misread against the game's own Sprint mechanic (+20% move speed, 'no cost; ends on fire/melee/grenade') — a spotter line that sounds like it's reporting a movement-system event, not a kill.
- `Kill.SpreeEnder-05` — 'Streak' is a synonym for 'spree' that appears nowhere in the shipped table (which consistently says 'killing spree' / 'spree'), breaking the one term of art this slot exists to reinforce.

## coach — 48 candidates generated, 12 kept

### slot `LowKillDeathRatio`

**Winner:** `LowKillDeathRatio-03` — -03 is the shortest line that states only the confirmed Deaths stat with generic, cause-agnostic advice, and -01 earns second by staying equally clean on correctness while leading with a structurally distinct 'Died X times' construction.

| # | id | text | kept |
|---:|---|---|---|
| 1 | `LowKillDeathRatio-03` | {Deaths} deaths this match. Peek less, hold cover more. | ✅ |
| 2 | `LowKillDeathRatio-01` | Died {Deaths} times. Disengage sooner — reposition before the fight, not after. | ✅ |
| 3 | `LowKillDeathRatio-05` | {Deaths} deaths — high count. Check your six before pushing forward. |  |
| 4 | `LowKillDeathRatio-08` | {Deaths} deaths. Stop contesting every fight — pick your engagements. |  |
| 5 | `LowKillDeathRatio-06` | {Deaths} deaths recorded. Trade less, retreat more — survive the fight you can't win. |  |
| 6 | `LowKillDeathRatio-04` | {Deaths} deaths and climbing. Rotate off contested ground instead of holding it. |  |
| 7 | `LowKillDeathRatio-02` | {Deaths} deaths logged. You're re-engaging into losing fights — break line of sight first. |  |
| 8 | `LowKillDeathRatio-07` | {Deaths} deaths this match. You're first through the door too often — let a teammate lead. |  |

**Rejected outright:**

- `LowKillDeathRatio-07` — Asserts the player is 'first through the door too often' — an entry-fragging pattern the struct (raw Deaths count only) cannot confirm.
- `LowKillDeathRatio-02` — States 're-engaging into losing fights' as the mechanism behind the death count; the struct has no fight-outcome field to support that claim.

### slot `HighAssistsFewKills`

**Winner:** `HighAssistsFewKills-08` — -08 is the shortest line that sticks to what Assists actually proves (damage without a kill), while -01 gives real structural variation via its 'not closing them' contrast and remains equally correctness-clean.

| # | id | text | kept |
|---:|---|---|---|
| 1 | `HighAssistsFewKills-08` | {Assists} assists. Damage without kills wins nothing — commit to the last hit. | ✅ |
| 2 | `HighAssistsFewKills-01` | {Assists} assists this match. You're softening kills, not closing them — finish what you start. | ✅ |
| 3 | `HighAssistsFewKills-05` | {Assists} assists recorded. You open fights well — hold the engagement to the end. |  |
| 4 | `HighAssistsFewKills-03` | {Assists} assists. Good target ID, no follow-through — stay on target after the trade. |  |
| 5 | `HighAssistsFewKills-02` | {Assists} assists logged. Damage's landing but someone else banks the kill — commit to the finish. |  |
| 6 | `HighAssistsFewKills-04` | {Assists} assists this match. Chase the kill instead of peeling off after first contact. |  |
| 7 | `HighAssistsFewKills-06` | {Assists} assists. High tag count, low kill count — one more shot before you disengage. |  |
| 8 | `HighAssistsFewKills-07` | {Assists} assists this match. You're doing the work; someone else gets the finish — press it. |  |

**Rejected outright:**

- `HighAssistsFewKills-06` — Claims a 'low kill count' comparison the Assists-only trigger condition never actually evaluates — no Kills field is checked.
- `HighAssistsFewKills-07` — Self-flagged tone risk: reads as editorializing about how the player should feel rather than a flat statement of fact, against doctrine.

### slot `SelfInflicted`

**Winner:** `SelfInflicted-03` — -03 is the shortest line that never invents a cause of death the struct can't support (no weapon-of-death field exists), and -08 is the structurally distinct runner-up (direct address vs. instruction) that stays equally cause-agnostic.

| # | id | text | kept |
|---:|---|---|---|
| 1 | `SelfInflicted-03` | {SelfInflictedDeaths} self-inflicted deaths. Check your range before the shot, not after. | ✅ |
| 2 | `SelfInflicted-08` | {SelfInflictedDeaths} self-inflicted deaths. Stop firing point-blank — you're not the only one in range. | ✅ |
| 3 | `SelfInflicted-04` | {SelfInflictedDeaths} self-inflicted deaths recorded. You're killing yourself with your own damage — hold back a beat. |  |
| 4 | `SelfInflicted-01` | {SelfInflictedDeaths} self-inflicted deaths. Your own explosive killed you — increase throw or fire distance. |  |
| 5 | `SelfInflicted-06` | {SelfInflictedDeaths} self-inflicted deaths this match. That's damage you dealt yourself — respect your own blast radius. |  |
| 6 | `SelfInflicted-02` | {SelfInflictedDeaths} self-inflicted deaths this match. Splash reaches further than the blast radius looks — back off before firing. |  |
| 7 | `SelfInflicted-05` | {SelfInflictedDeaths} self-inflicted deaths. Close-range explosives cut both ways — swap weapons at close quarters. |  |
| 8 | `SelfInflicted-07` | {SelfInflictedDeaths} self-inflicted deaths. Your splash killed you as often as it killed them — space the shot out. |  |

**Rejected outright:**

- `SelfInflicted-07` — Invents an unsupported kill-parity comparison ('killed you as often as it killed them') — no such stat exists in the struct.
- `SelfInflicted-05` — Stacks two unconfirmed assumptions in one line: an explosive-specific cause of death and a weapon-swap option not evidenced by this record.

### slot `FriendlyFire`

**Winner:** `FriendlyFire-02` — -02 is the only candidate that grounds its advice in an actual struct field (TeamId), and -01 is the cleanest, shortest generic fallback that invents no specific cause.

| # | id | text | kept |
|---:|---|---|---|
| 1 | `FriendlyFire-02` | {FriendlyFireKills} friendly fire kills this match. Check team ID before you fire in a scrum. | ✅ |
| 2 | `FriendlyFire-01` | {FriendlyFireKills} friendly fire kills. Confirm the target before you commit the shot. | ✅ |
| 3 | `FriendlyFire-06` | {FriendlyFireKills} friendly fire kills this match. That's damage your own team ate — ID before you engage. |  |
| 4 | `FriendlyFire-03` | {FriendlyFireKills} friendly fire kills. Slow down in the crowd — that's a teammate you're shooting. |  |
| 5 | `FriendlyFire-08` | {FriendlyFireKills} friendly fire kills. Check your target twice in close-quarters chaos. |  |
| 6 | `FriendlyFire-05` | {FriendlyFireKills} friendly fire kills. Hold fire until the target clears — don't spray into a mixed group. |  |
| 7 | `FriendlyFire-07` | {FriendlyFireKills} friendly fire kills. You're trading team damage for a faster shot — take the extra half-second. |  |
| 8 | `FriendlyFire-04` | {FriendlyFireKills} friendly fire kills recorded. Splash and grenades don't discriminate by team color — watch your throws. |  |

**Rejected outright:**

- `FriendlyFire-04` — Attributes the friendly-fire kill to grenade splash specifically — the struct has no weapon-of-death field, so this is inferred from grenades existing elsewhere in the table, not confirmed for this stat.

### slot `ShortMatchTime`

**Winner:** `ShortMatchTime-05` — -05 is the best-grounded line, hedging between two causes and tying 'arriving late' to the actual bJoinedInProgress field, while -02 is the safest fully generic fallback that names no single unconfirmed cause.

| # | id | text | kept |
|---:|---|---|---|
| 1 | `ShortMatchTime-05` | {TimeInMatchSeconds}s of match time logged. Low presence — you're either dying fast or arriving late. | ✅ |
| 2 | `ShortMatchTime-02` | {TimeInMatchSeconds}s of time in match. Short match presence — check your spawn-to-fight routing. | ✅ |
| 3 | `ShortMatchTime-08` | {TimeInMatchSeconds}s of match time. Low count — you spent more time dead or away than fighting. |  |
| 4 | `ShortMatchTime-07` | {TimeInMatchSeconds}s recorded in match. Short time on the field — tighten your spawn-to-engage path. |  |
| 5 | `ShortMatchTime-04` | {TimeInMatchSeconds}s in match this round. Time out of the fight is time your team plays a man down. |  |
| 6 | `ShortMatchTime-01` | Only {TimeInMatchSeconds}s in the match. You're dying and not getting back into the fight fast enough. |  |
| 7 | `ShortMatchTime-03` | {TimeInMatchSeconds}s alive and in play. That's a lot of match spent not fighting — reduce the walk back. |  |
| 8 | `ShortMatchTime-06` | {TimeInMatchSeconds}s in play. Get back to the fight faster after a death — every second off is your team short-handed. |  |

**Rejected outright:**

- `ShortMatchTime-01` — Asserts dying and slow respawn-return as the confirmed cause of low match presence; the struct only records a raw time total, not a cause.
- `ShortMatchTime-06` — Same overclaim as -01 ('after a death'), plus an unconfirmed 'short-handed' team-size framing.

### slot `LowKills`

**Winner:** `LowKills-08` — -08 is the shortest line in the entire pool and stays purely prescriptive without claiming to know why kills are low, while -02 is equally non-diagnostic with a distinct clause structure ('close distance before you commit').

| # | id | text | kept |
|---:|---|---|---|
| 1 | `LowKills-08` | {Kills} kills. Low count — contest more, retreat less. | ✅ |
| 2 | `LowKills-02` | {Kills} kills logged. Low finish count — close distance before you commit to the fight. | ✅ |
| 3 | `LowKills-04` | {Kills} kills this match. Seek engagements — map presence alone doesn't end fights. |  |
| 4 | `LowKills-07` | {Kills} kills this match. You're trading shots, not ending them — follow up after the first hit. |  |
| 5 | `LowKills-01` | {Kills} kills this match. Push fights instead of trailing them — get in range before they disengage. |  |
| 6 | `LowKills-03` | {Kills} kills. You're present but not finishing — take the fight instead of shadowing it. |  |
| 7 | `LowKills-06` | {Kills} kills. Few fights finished — commit earlier instead of disengaging on first contact. |  |
| 8 | `LowKills-05` | {Kills} kills recorded. Low output — hold sightlines that actually see targets. |  |

**Rejected outright:**

- `LowKills-05` — Invents an oddly specific 'hold sightlines' cause not evidenced by a raw Kills count, and reads as the weakest-phrased candidate in the slot.

## callsigns — 30 candidates generated, 15 kept

### slot `Recruit`

**Winner:** `Recruit-08` — Dulledge lifts the GDD's own descriptor for the Recruit column ('Same StateTree, dulled') verbatim, giving zero interpretive risk, while Softaim is equally clean but is the team's own gloss on accuracy_pct rather than canon's own language.

| # | id | text | kept |
|---:|---|---|---|
| 1 | `Recruit-08` | Dulledge | ✅ |
| 2 | `Recruit-03` | Softaim | ✅ |
| 3 | `Recruit-05` | Shakygrip | ✅ |
| 4 | `Recruit-01` | Slowdraw | ✅ |
| 5 | `Recruit-02` | Wideshot | ✅ |
| 6 | `Recruit-06` | Openflank |  |
| 7 | `Recruit-04` | Longwait |  |
| 8 | `Recruit-09` | Wideswing |  |
| 9 | `Recruit-10` | Longfuse |  |
| 10 | `Recruit-07` | Rareflash |  |

**Rejected outright:**

- `Recruit-07` — 'Rareflash' reads as a flashbang/flash-grenade gadget, a system the shipped scope explicitly excludes (frag grenades only, GDD §5.1) — names a thing that isn't in the slice, which ranks last regardless of phrasing per the correctness rule.

### slot `Marine`

**Winner:** `Marine-02` — Midpace is the shortest zero-risk candidate and names the exact stat the GDD headlines for Marine (reaction_ms, the literal mid value of 500/320/220), beating Evenkeel which is equally clean but grounded in the less central jitter field.

| # | id | text | kept |
|---:|---|---|---|
| 1 | `Marine-02` | Midpace | ✅ |
| 2 | `Marine-10` | Evenkeel | ✅ |
| 3 | `Marine-01` | Steadyaim | ✅ |
| 4 | `Marine-05` | Coverwise | ✅ |
| 5 | `Marine-03` | Tightline | ✅ |
| 6 | `Marine-09` | Longmemory |  |
| 7 | `Marine-06` | Baseline |  |
| 8 | `Marine-07` | Fieldcall |  |
| 9 | `Marine-04` | Quickcall |  |
| 10 | `Marine-08` | Halfclaim |  |

### slot `Veteran`

**Winner:** `Veteran-06` — Honedline quotes the GDD's own descriptor for the Veteran column ('Same StateTree, sharpened') with no interpretive risk, edging out Tightaim, which is equally clean but is the team's own gloss on a cone-radius stat rather than canon's own words.

| # | id | text | kept |
|---:|---|---|---|
| 1 | `Veteran-06` | Honedline | ✅ |
| 2 | `Veteran-03` | Tightaim | ✅ |
| 3 | `Veteran-05` | Coverlock | ✅ |
| 4 | `Veteran-09` | Longtrack | ✅ |
| 5 | `Veteran-02` | Deadeye | ✅ |
| 6 | `Veteran-01` | Quickdraw |  |
| 7 | `Veteran-08` | Rocketlock |  |
| 8 | `Veteran-10` | Fastlock |  |
| 9 | `Veteran-07` | Calltactic |  |
| 10 | `Veteran-04` | Fastcommit |  |
