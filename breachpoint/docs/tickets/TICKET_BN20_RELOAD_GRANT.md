# TICKET — BN20: bots press reload and no granted ability carries it

> STATUS: open — cut 26 Aug 2026 by mac terminal. Found during AIB4's live proof,
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

- [ ] The cause named with timestamps, not inferred
- [ ] Fixed at the cause, not by silencing the warning
- [ ] Five-match re-measure with the warning at ~0

## Log
