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
