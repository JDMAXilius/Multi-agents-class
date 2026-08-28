# TICKET — BN20: bots press reload and no granted ability carries it

> STATUS: **FIXED 28 Aug 2026 — open on one box only.** Reload is granted in
> `GrantDefaults` as a body verb; the warning fell **142 → 1 per match**, and the single
> survivor is **Fire, not Reload**. What is still owed is this ticket's own sample-size
> bar: the re-measure is a per-match count, not the five-match mean-and-spread step 3
> demands (the AIB8 rule, quoted below). Not archived for that reason alone.
> Original: open — cut 26 Aug 2026 by mac terminal. Found during AIB4's live proof,
> re-observed in every match since, and owed a ticket since then.

## The measurement

One four-minute AIB match:

```
308 x  LogBN: Warning: BNASC: input tag Input.Weapon.Reload reached the ASC but NO
               granted ability carries it — the grant is missing (or defaults are not
               granted yet).
 11 x  LogBN: BNInput: Input.Weapon.Reload -> Default__BNGA_Reload : ACTIVATED
```

Bots press reload constantly; it lands 11 times and fails 308. Still present in the most
recent matches (26 Aug), so it is not transient.

## Why it is probably NOT an AI bug

The warning's own second clause names the likely cause: *"or defaults are not granted
yet"*. A freshly possessed respawn pawn appears to press reload before its default ability
set is granted. That makes it a GRANT-TIMING bug on the BN side — the AI is pressing a
button the avatar has not finished wiring.

Evidence for that reading: reload DOES activate 11 times, so the ability exists and the
tag routing works. A missing grant would fail 100% of the time, not 96%.

## What this is not

Not the AIB adapter's fault: the adapter presses `Input.Weapon.Reload` exactly as it
presses Sprint/Crouch/Grenade, all of which activate normally in the same matches
(Sprint 1635 ACTIVATED, Crouch 162, Grenade 68). One tag out of eight failing points at
that tag's grant, not at the presser.

## Kickoff

- requires: engine-installed
- owner_path: `Source/BreachpointNext/AbilitySystem/`, this ticket

## Steps

1. Log the grant moment and the first reload press with timestamps on one respawn — is the
   press genuinely before the grant, or is the grant absent for that pawn entirely?
2. If it is timing: either defer the press until the ability set is granted, or grant
   defaults before the avatar door opens. Name which and why.
3. Re-measure: the warning count must fall to ~0 across five matches (the AIB8 rule — one
   match cannot tell 0.04 from 1.67).

## Done when

- [x] The cause named — by a stronger instrument than the box imagined, see the 28 Aug
      entry (the fix's own 142 → 1 delta, not a timestamp correlation)
- [x] Fixed at the cause, not by silencing the warning — the grant was ADDED, and the
      warning still fires (once, for Fire), which is how we know it was not muzzled
- [ ] Five-match re-measure with the warning at ~0

## Log

### 2026-08-26 — cloud lead: step 1's instrument WRITTEN, and one gate that is
correct regardless of the verdict. WRITTEN, NOT COMPILED; harness half proven.

- The warning now carries its separating facts: `dead=yes|no avatar=<name>` — dead=yes
  classifies a corpse-window press, dead=no with a fresh avatar a grant race; the log's
  own timestamps against the equip line give step 1 its correlation without a second
  instrument. Harness counts `no_grant_presses` and splits dead/alive/by-tag (proven on
  synthetic lines, asserts green) — the done-when bar reads straight off it.
- STRUCTURAL SUSPECT, named but NOT concluded (the ticket's own rule): the module's
  fire task pressed reload on ammo facts alone — `bHasReserveAmmo && AmmoNorm <= frac`,
  retried every ReloadRetrySeconds — with no alive gate anywhere in the branch, while
  the controller possesses the corpse for the whole RespawnDelay and the ASC outlives
  the body on the PlayerState. A bot that died mid-fight (low magazine, the common
  case) kept pressing from the corpse. Whether that is all 308 is what the dead=yes
  split now answers.
- THE GATE (module, `FAIBFireWhenAbleTask::Tick`): a corpse presses nothing — dead
  avatar releases a held trigger and stops pressing fire/reload/swap. Correct on its
  own terms whatever the split says (the host's fire gate already refuses dead verbs;
  this stops asking). If the split comes back dead-dominated, this gate IS the fix and
  the re-measure closes the ticket; if alive-dominated, the grant race is real and the
  BN-side fix (defer the press or grant before the avatar door opens) is still owed.

### 2026-08-28 — FIXED, and the ticket's own hypothesis was wrong

Recorded from this session's verified result; not re-measured here.

**The fix: Reload is granted in `GrantDefaults` as a body verb.** Warnings per match
**142 → 1**, and the survivor is **Fire**, not Reload.

**Where reality diverged from this ticket, written down because the reasoning is the
lesson.** The section above is titled "Why it is probably NOT an AI bug" and argues for a
GRANT-TIMING race: a freshly possessed respawn pawn pressing before its default set is
granted. It was not a race. The grant was simply not there to win — Reload was missing
from `GrantDefaults`, and adding it as a body verb beside its siblings is the whole fix.
The 26 Aug entry's structural suspect (the fire task pressing reload from a corpse for the
whole RespawnDelay) is likewise not what closed this, though the corpse gate it landed is
correct on its own terms and stays.

**One thing this ticket argued that is still unexplained, and is left open rather than
tidied away:** "reload DOES activate 11 times, so the ability exists and the tag routing
works. A missing grant would fail 100% of the time, not 96%." Against a missing
`GrantDefaults` row, those 11 activations should not have happened. Something else granted
Reload on some path some of the time. Nobody has run that down; it is not blocking, and it
is not resolved either. Do not read this entry as explaining it.

**Why the ticket is not archived:** step 3 says *"the warning count must fall to ~0 across
five matches (the AIB8 rule — one match cannot tell 0.04 from 1.67)"*. The 142 → 1 delta
is far too big to be the ambiguity AIB8 was about, and no reasonable reader doubts the
fix — but the box asks for a mean and a spread and there is one number. Under-claiming is
the cheaper error here. Five matches close it; the same logs also carry AIB18's `ads_ins`
and REFUSED counts, so it is one run for two tickets.
