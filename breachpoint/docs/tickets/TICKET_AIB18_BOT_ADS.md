# TICKET — AIB18: the bots aim down sights properly

> STATUS: landed cloud-side 27 Aug 2026 — ~~WRITTEN, NOT COMPILED~~ **COMPILED, 28 Aug
> 2026: all targets clean, AIBot suite 119/119/0.** Founder: "let's have the aibot doing
> ADS properly." The host's ADS was fully built and granted to bots (a PlayerState body
> verb) — nothing ever pressed it. **UNPROVEN LIVE: every box below is eyes-on or a log
> count, and no run has produced either — `ads_ins` has never been read off a real match.**

## What landed

- **The verb**: `AIBot.Verb.Aim` — held like Fire; what aiming DOES (spread, the speed
  multiply, descope-on-hit, the per-weapon `bCanADS` refusal) is entirely the host's.
  Adapter maps it to `Input.Weapon.ADS`; the ability's own refusals stand unchanged.
- **The read**: `IAIBAvatarInterface::IsAiming()` — the host's `State.Weapon.ADS` tag,
  never a mirror of the press: the host DESCOPES on a landed hit, self-cancelling the
  aim behind the bot's back, and a presser trusting its own flag believes in a zoom it
  lost a second ago.
- **The discipline** (fire task): sights up with a visible target in the
  [AimRangeUU=500, AimMaxRangeUU=900] band, 80uu hysteresis. The upper edge mirrors
  FightRangeUU on purpose — inside it the mover released sprint, so the host's
  sprint-exclusion can never bounce the press; beyond it the bot is sprint-closing and
  should not be asking. Below 500 hip-fire tracks better and the speed penalty costs a
  duel. Aim-skill gated: a Novice hip-fires, Trained+ aims (the ladder's shape).
- **The descope is FELT**: after the host cancels the aim, the re-press waits
  ReAimSeconds (0.75) — an instant re-scope erases the descope's whole point and reads
  as a machine. The press is release-then-press: after a descope the old press is still
  logically down, and the ability activates on the press EDGE.
- **Sights drop with the hands**: reload, melee, the grenade throw, target loss, death
  (the corpse gate), and task exit all release the aim — every early-return that
  releases fire releases aim beside it.
- **Instrument**: `aimed in at Nuu` / `let the aim go at Nuu` (Verbose, formats
  frozen); harness counts `ads_ins` + mean range (proven on synthetic lines).

## Done when (terminal)

- [ ] Rung 1; eyes-on: bots visibly slow and raise sights in mid-range duels, hip-fire
      up close; a hit knocks them out and they take a beat to re-aim
- [ ] `ads_ins` > 0 with mean range in the 500-900 band; Recruit-tier logs zero
- [ ] REFUSED count for Input.Weapon.ADS stays ~0 (the band gating keeps presses out
      of the sprint-exclusion; a spike here means the band and the mover disagree)

## Log

_(outputs verbatim)_

### 2026-08-28 — board-hygiene pass: compiled, unobserved

Corrected, not measured. "WRITTEN, NOT COMPILED" was false — all targets build clean and
the AIBot suite reads 119/119/0, so the verb, the `IsAiming()` read, the band gating and
the release paths are all real code.

**No box moves.** All three are observations of a running match:

- the eyes-on (sights up mid-range, hip-fire close, a beat after a descope) — nobody has
  watched a match for it;
- `ads_ins` > 0 with mean range inside 500-900 — the harness counter exists and has only
  ever been fed synthetic lines;
- REFUSED for `Input.Weapon.ADS` staying ~0 — the interesting one, because it is the
  falsifier: a spike means the band and the mover disagree about sprint. Worth reading in
  the same pass as BN20's warning counts, which come off the same log.

Recorded so the next agent does not read "landed" as "working": this is a compiled press
with no evidence any bot has ever raised its sights.
