# TEST — the free-for-all match, proven in one session

**Cut:** 17 August 2026 by the cloud lead · covers Roadmap 4 (G1–G4), commits `070112e` →
`fe958a2`. Filter the log to **`LogBN`**. There are no console commands; everything below is
automatic.

## 0. Solo PIE — the match must start on its own

`MinPlayers=1`, so one player is a match. Press play and expect, in order:

```
BNGameState: match state -> InProgress
```

**If it says `WaitingToStart` and stays there**, the mode never satisfied its own start gate —
check `MinPlayers` in `DefaultGame.ini`. **If no `BNGameState:` line appears at all**, the
GameState class is not ours: that is the `TASK-R4-GAMESTATE-CLASS` editor ticket, and every score
in the session is landing on an object nothing reads.

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
BNGameState: match state -> PostMatch
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
BNGameState: match state -> InProgress
```

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

## 6. Known and accepted

- **`MinPlayers` is only enforced on the session's FIRST match** — the restart goes straight back
  to InProgress. Unreachable at `MinPlayers=1`; reopens the moment it is raised or a dedicated
  server appears (ROADMAP-4, "Known limitation").
- **No scoreboard, no match HUD.** R4 landed the replicated state and the delegates a HUD will
  bind to; the widget is a later wave. Until then the log is the scoreboard.
- **Nothing here has been compiled.** Every line above is written-not-compiled per the honesty
  ladder — the first build is the first real test.
