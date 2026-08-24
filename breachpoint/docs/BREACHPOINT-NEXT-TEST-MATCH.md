# TEST — the free-for-all match, proven in one session

**Cut:** 17 August 2026 by the cloud lead · covers Roadmap 4 (G1–G4), commits `070112e` →
`fe958a2`. **Revised 19 Aug 2026:** the match states are the ENGINE's own
(`WaitingToStart` / `InProgress` / `WaitingPostMatch` — the machine is `AGameMode`'s, not a BN
enum), so the state names below changed and the engine adds its own `LogGameMode: Match State
Changed from X to Y` lines beside ours. Filter the log to **`LogBN`**; everything below is
automatic.

## 0. Solo PIE — the match must start on its own

`MinPlayers=1`, so one player is a match. Press play and expect, in order:

```
BNGameState: match state -> WaitingToStart
BNGameState: match state -> InProgress
```

**If it says `WaitingToStart` and stays there**, the mode never satisfied its own start gate —
check `MinPlayers` in `DefaultGame.ini`. **If no `BNGameState:` line appears at all**, the world
is not running our mode at all (the GameState class is FORCED in `ABNGameMode::InitGame`, after
any Blueprint's serialisation, so a BP dropdown can no longer take it away — the old
`TASK-R4-GAMESTATE-CLASS` ticket is closed as superseded). Check the map's GameMode override.

## 1. Scoring — one line per elimination

Kill anything with a PlayerState (a second window, or a bot when they exist):

```
BNGameMode: PlayerA eliminated PlayerB. (PlayerA: 1 kills)
```

| Case | Expected |
|---|---|
| Normal kill | killer's count rises |
| **Suicide** (own grenade) | `eliminated themselves. (n deaths)` — a death, **no kill** |
| **World death** | `died. (n deaths)` — a death, **no kill** |
| Kill by someone who then **respawns before the damage lands** (grenade in flight) | still credited — the credit follows the PLAYER STATE, not the pawn that threw it |

That last row is the bug the critic caught in Wave 1. Worth one deliberate test: throw a grenade,
kill yourself, respawn, and let the grenade kill someone. The thrower must still be credited.

## 2. The end conditions

**By score** — reach `ScoreLimit` (25 shipped; drop it to 2 in the ini to test in a minute):

```
BNGameMode: match over. Winner: PlayerA
BNGameState: match state -> WaitingPostMatch
```

**By time** — set `TimeLimit=30` and wait. The sole leader wins; **a tie leaves the winner null**
and prints `Winner: none (tie)`. That is a legal outcome, not a bug.

## 3. The freeze — the part that is easy to get wrong

For `PostMatchDuration` seconds after the buzzer:

- Fire, melee, grenade: **nothing happens**, and each press prints
  `BNInput: Input.Weapon.Fire -> ... REFUSED`.
- **Hold the trigger down BEFORE the match ends and keep holding.** The firing must stop at the
  buzzer. If it keeps firing, the freeze is refusing activation but not cancelling the running
  ability — the exact bug fixed in `fe958a2`, and it would mean that fix regressed.
- A kill that lands after the buzzer (grenade in flight) still **kills and ragdolls** — but the
  score does not move. Death and hit-react deliberately ignore the freeze; everything a player
  chooses to do does not.

## 4. The restart

At the end of the post-match:

```
BNGameState: match state -> WaitingToStart
BNGameState: match state -> InProgress
```

The restart passes THROUGH warmup — that one-frame `WaitingToStart` is the machine re-running the
bot fill and re-asking the start gate, which is what made `MinPlayers` hold on every round instead
of only the first. **Server log only:** both transitions happen inside one server frame, so
clients replicate straight to `InProgress` and never see the hop — a client window printing only
the `InProgress` line at restart is correct, not a missed transition (critic-verified). Then:

- every score back to **0**
- everyone alive again, at a start point
- the clock restarted from the full `TimeLimit`
- **no loading screen** — the restart is in place, so a listen server keeps its connections

**The one to watch:** a player killed in the last seconds of a round must NOT blink out and
reappear a second or two into the new round. That was the stale-respawn-timer bug (`fe958a2`); it
is now generation-guarded.

## 5. Two windows — the rung that counts (honesty ladder §6)

Everything above is the server's truth. In a two-window PIE (listen server + client):

- both windows show the same match state and the same remaining time;
- kills and deaths agree on both, for **both** players;
- the freeze applies on the client too — the client's own presses print `REFUSED`, which means the
  server refused them, not that the client's input was disabled;
- **a window that joins mid-match** reads the correct remaining time on its first frame (the clock
  is an end stamp, not a countdown) and, if it joins during warmup or post-match, is frozen like
  everyone else.

## 5b. The bots (R5) — the fill and the fight

Solo PIE, `TargetPlayers=4`, after the terminal's `TASK-R5-ST-BNBOT` ticket:

- Startup: `BNBots: filled 3 bots to reach 4`, three named PlayerStates (Marcus, Vale, Ossian).
- Bots ROAM between the four placed points when nothing is visible; when one sees you — or
  another bot — it turns, closes, and fires **through the same fire ability** (`BNInput:` lines
  show its presses exactly like yours; `BNLoadout` applied to it at spawn).
- Bot kills and deaths score normally; a bot can win the match, and `match over. Winner: Vale`
  is a pass, not a bug.
- During warmup/post-match, bots are frozen like everyone else. A bot that still stares at you
  while frozen is correct (perception runs; the trigger is refused).
- `BNBots: no ABNPointOfInterest placed in this level` once = the points ticket step was skipped;
  bots will stand until they see a target.
- Two-window rung: on the CLIENT, bots move, pose, shoot and die exactly like remote humans —
  everything a client sees of a bot rides the same pawn/PlayerState replication (critic-verified).

## 5c. The brain (R6) — reading a bot's mind

One log line per ambition CHANGE, never per rescore:

```
BNBrain: Vale wants Fight (u=1.00) because target in sight.
BNBrain: Vale wants Survive (u=1.14) because health low.
```

- Shoot a bot below **35% health**: it must break off — even mid-firefight, immediately, commit
  window or not (that interrupt was the critic's find: the old ratio rule could never fire) — and
  its roam flips to the point FARTHEST from you.
- A healthy bot that sees you commits to Fight for at least its CommitSeconds — visible
  commitment is the intended feel; instant flip-flopping is the bug.
- No `BNBrain:` spam under sustained fire = correct (rescores happen per hit, the LINE only on
  change).
- Tuning is `DT_BNBotAmbitions` (terminal ticket `TASK-R6-DT-AMBITIONS`) — until it lands, the
  C++ defaults drive and one warning says so.

## 5d. The R9 bot pass — four things to watch for

**Rebuild the tree first.** The strafe is a NEW StateTree node and a compiled tree does not grow
one by itself: run `Tools/bn/62_bot_assets.py` against the running editor. Its probe list now
carries `FBNStrafeTask`, so a stale build stops the script rather than quietly authoring a tree
without it. Everything else below works without the rebuild.

1. **Footwork.** A bot in a firefight sidesteps while it shoots instead of standing still. Short
   steps, side to side, aim staying on you — not a walk off mid-burst. A bot in a corridor with
   its back to a wall turns around instead of grinding into it (one Verbose line, once).
2. **It hears the shot.** Sneak up and shoot a bot in the back from outside its vision cone: it
   must go and LOOK — walk toward where you fired from — not carry on. It should NOT snap around
   and open fire instantly; that would be omniscience, and the reaction window is still what
   gates the first shot once it actually sees you. `LogBN` Verbose: `was hit by X and will go
   looking`.
3. **A fleeing bot flees.** Hurt one below 35% health while it can see you (`Survive` in the
   brain line). It must move AWAY. Before R9 a bot with a fresh memory of you would walk back
   toward where you were — if you see that, 9.1 regressed.
4. **A late joiner takes a seat.** Start a match, then join a second player mid-match: expect
   `BNBots: 1 bot(s) yielded seats to humans` and a lobby of exactly `TargetPlayers`. No bot may
   SPAWN mid-match — if one appears from nowhere, that is the rule broken.

### The jump (R9.5)

5. **In a fight**, a bot leaves the ground now and then mid-burst — a hop every third sidestep,
   not constantly. Constant hopping is a bug (the cooldown is not holding); never hopping while
   strafing works means the juke counter is not running.
6. **Stand on a crate or a low ledge** and let a bot try to reach you: it should JUMP at the
   obstruction partway through closing, and only give up if the jump does not help. Before R9.5
   it stood at the bottom until the watchdog wrote you off.
7. **Roaming past a lip:** a bot that stops short of a point of interest jumps once and re-tries
   before accepting it as arrived. `LogBN` Verbose: `stopped short of its point and jumped for it`.
8. **Corner one:** a strafe that cannot path jumps instead of grinding into the wall.

A bot that pogos in place is the failure mode to watch for — that is `JumpCooldownSeconds` not
being respected, not a tuning question.

### Tiers, ears and cover (R10)

**Rebuild the bot assets first** (`Tools/bn/62_bot_assets.py`): two new StateTree nodes and a new
table. Its probe list stops the script on a stale build rather than authoring a tree without them.

9. **Tiers.** `LogBN` prints one `fights at tier <name> (reaction …, aim ±…°, sight …uu)` per bot
   at possession. Set `BotTier=Recruit` in `DefaultGame.ini` and the difference must be obvious
   within one fight: long reaction, wide spray, no jumping, and they stand still to trade. Set
   `Spartan` and they should be genuinely hard. **Marine must feel exactly like yesterday's
   bots** — it is the same numbers, moved.
10. **Ears.** Stand where a bot cannot see you and fire a shot: it should come and LOOK, not snap
    round and shoot. `LogBN` Verbose: `heard something at … and will go looking`. A grenade blast
    should pull bots from further away than a rifle shot does.
11. **Cover.** Damage a bot below 60% while it can see you and keep shooting: it should break line
    of sight — move behind something, hold about a second and a half, then come back. In an open
    space with nothing to hide behind it must keep fighting instead, with one Verbose line
    (`wanted cover and found none`). A bot that shuttles in and out of cover repeatedly is the
    cooldown not holding.

12. **Grenades (R10.4).** Throw one at a bot's feet: it must MOVE — away from the grenade, with
    a jump, about a second before the bang. `LogBN` Verbose: `sees a grenade about to go off` then
    `is diving away from a grenade`. A bot that only had to take two steps should come straight
    back at you rather than running off. A bot boxed into a corner keeps fighting instead
    (`is cornered by a grenade`). **`BotTier=Recruit` must NOT dodge** — that is the tier working,
    not a bug. And a bot that dodges a grenade thrown at someone else across the room means the
    warn radius is reading wider than the blast.

## 6. Known and accepted

- ~~`MinPlayers` is only enforced on the session's FIRST match~~ — **closed 19 Aug 2026** by the
  native-machine migration: the restart re-enters `WaitingToStart` and the engine's own poll
  re-asks the gate, so a round only begins while `MinPlayers` humans are present. If every human
  leaves during the post-match, the mode now sits in warmup instead of restarting an empty match.
- **Bots yield seats.** The fill CONVERGES on `TargetPlayers`: a human joining a warmup that bots
  already filled despawns the newest bot (`BNBots: 1 bot(s) yielded seats to humans`). This closed
  R5's recorded fill-overshoot at `MinPlayers>1`. Mid-MATCH backfill/removal is still the named
  deferral it always was.
- **No scoreboard, no match HUD.** R4 landed the replicated state and the delegates a HUD will
  bind to; the widget is a later wave. Until then the log is the scoreboard.
- **Nothing here has been compiled.** Every line above is written-not-compiled per the honesty
  ladder — the first build is the first real test.
