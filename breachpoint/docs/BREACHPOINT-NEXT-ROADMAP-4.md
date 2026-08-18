# BREACHPOINT NEXT — Roadmap 4: The Match (Free-For-All)

**Cut:** 17 August 2026 by the cloud lead · **Crew:** `bn-builder`, `bn-critic`, `bn-editor`
(no new agents — the BN trio already covers write / review / editor)

## The one-line goal

**A match starts, is scored, ends, and starts again — free-for-all, server-authoritative, and
identical on every machine.**

## Founder rulings that bound this roadmap

- **FREE-FOR-ALL ONLY.** No teams, no friendly fire, no team spawns. Teams are a later mode and
  nothing in R4 may anticipate them (17 Aug ruling, recorded in RESEARCH-DAMAGE-REFERENCES §3a).
- **Less is more.** One new class. Everything else is an edit to a file that already exists.
  A helper "for later" is a finding, not a contribution.
- **C++ first.** Defaults live in C++ and in `Config/DefaultGame.ini`. Asset references are soft.
  Anything that must be set in the editor is a `bn-editor` ticket, never a manual step.

## What R4 is NOT

Sessions, lobbies, Steam, matchmaking services, a scoreboard widget, spectating, killcams,
Lyra-style experiences or phase-abilities. Testing is PIE multi-window + listen server, which
needs **zero** session code. Those are later roadmaps and naming them here is how they stay out.

---

## G1 — The match exists, and every machine sees the same one

The hole R4 opens on: there is no GameState class. The match has nowhere to live that a client
can read, so nothing about it can be shown, tested, or trusted.

| # | Task |
|---|---|
| 1.1 | `Match/BNGameState.{h,cpp}` — `ABNGameState : AGameStateBase`. `EBNMatchState` {`WaitingToStart`, `InProgress`, `PostMatch`} declared **in that header**, not in a types file of its own. Replicated `MatchState` with `OnRep_MatchState`, plus one multicast delegate for later readers (HUD). |
| 1.2 | **The clock replicates as an END TIME, never as a countdown.** Replicated `double MatchEndServerTime`; `GetRemainingSeconds()` computes locally against `AGameStateBase::GetServerWorldTimeSeconds()`. A ticking replicated counter is a per-second update to every client forever, and late joiners would see a stale value; an end stamp is one replication and is correct for a client that joins at any moment. |
| 1.3 | Replicated `ScoreLimit` (mirrored from the mode's config so clients can render "12 / 25") and `Winner` (`TObjectPtr<ABNPlayerState>`, replicated, null until decided). |
| 1.4 | `GameStateClass = ABNGameState::StaticClass()` in `ABNGameMode`'s constructor. |
| 1.5 | `GetLeaders(TArray<ABNPlayerState*>&)` — sort `PlayerArray` by kills, return everyone tied at the top. Ties are a real FFA state and the caller must be able to see them (shape borrowed from the UE5_Multiplayer_FPS reference's `UpdateLeader`). |

**Done when:** two PIE windows print the same `MatchState` and a remaining time that agrees within
a frame; a window that joins late prints the correct remaining time immediately.

---

## G2 — Scoring

| # | Task |
|---|---|
| 2.1 | `BNPlayerState`: `int32 Kills` and `int32 Deaths`, replicated, with OnReps. Not the engine's float `Score` — an FFA scoreboard reads two integers and a float invites rounding questions nobody wants to answer. |
| 2.2 | `ABNGameMode::HandlePlayerDeath` already receives victim AND killer: increment there. No new delegate, no new subscriber. |
| 2.3 | **Credit rules, stated so they are testable:** a suicide or a world death costs the victim a death and awards NO kill. A kill by a player who has since disconnected still costs the victim a death. Killer == victim is never a kill. |
| 2.4 | Extend the existing kill line with the running score, e.g. `BNMatch: X eliminated Y (X: 7)`. |

**Done when:** kills and deaths agree on both machines; `BNKillSelf`-shaped self-damage never
scores; the log names the score after every elimination.

---

## G3 — The rules

| # | Task |
|---|---|
| 3.1 | Start: when `MinPlayers` are present (config, default **1** so solo PIE always runs), `SetMatchState(InProgress)` and stamp `MatchEndServerTime = GetServerWorldTimeSeconds() + TimeLimit`. |
| 3.2 | End on score: after a kill, if the killer's `Kills >= ScoreLimit` → `Winner` = killer → `PostMatch`. |
| 3.3 | End on time: **one timer, no Tick** (law 4). On fire, the winner is the sole leader from `GetLeaders`; a tie leaves `Winner` null and that is a legal, renderable outcome. |
| 3.4 | Restart in place after `PostMatchDuration`: zero every score, clear `Winner`, respawn everyone, restamp the clock, back to `InProgress`. **No map travel** — a seamless reset is less code, no loading screen, and keeps the listen server's connections alive. |
| 3.5 | `RespawnPlayer` gated to `InProgress` only. A corpse must not stand up during the post-match. |

**Done when:** both end conditions fire correctly, the restart leaves no stale score or corpse,
and nobody respawns outside `InProgress`.

---

## G4 — The freeze, as a tag

Players must be unable to fight during warmup and post-match. Disabling input per client is a
lie the server cannot verify; a tag is state the server owns and every machine can read.

| # | Task |
|---|---|
| 4.1 | `State.Match.Frozen` in `BNGameplayTags`. |
| 4.2 | The mode applies/removes it on every player's ASC when the state changes, through the existing `UBNGE_State` spec-tag pattern (the same one crouch and ADS use). Construction-order rule applies: the tag rides the SPEC, never a CDO container. |
| 4.3 | **One line** in `UBNGameplayAbility::CanActivateAbility` — refuse while frozen, exactly as it already refuses while `State.Dead`. No ability file is edited. |

**Done when:** during warmup and post-match no weapon, melee or grenade activates on the host or
on a client, and the refusal is the server's (`BNInput: … REFUSED` appears on both).

---

## Waves

| Wave | Goals | Agent | Then |
|---|---|---|---|
| 1 | G1 + G2 | `bn-builder` | `bn-critic` on the diff |
| 2 | G3 + G4 | `bn-builder` | `bn-critic` on the diff |
| — | editor ticket (below) | `bn-editor` | runs in parallel, blocks nothing |

Two waves, not four: G1+G2 and G3+G4 are each one coherent diff. Splitting further buys review
passes nobody needs.

## The editor ticket (`bn-editor`)

`BP_BNGameMode` may out-serialise `GameStateClass` the way `BP_BNGameMode`'s pawn class already
out-serialises its dropdown. One check, one read-back: confirm the Blueprint's GameState Class is
`BNGameState` or empty (so the C++ default stands). Nothing else in the asset is touched.

## Config, so none of these numbers is a code change

```ini
[/Script/BreachpointNext.BNGameMode]
ScoreLimit=25
TimeLimit=600
MinPlayers=1
PostMatchDuration=10
RespawnDelay=3
```

## Known limitation, accepted with its trigger written down

**`MinPlayers` is only enforced on the FIRST match of a session.** `TryStartMatch` is called from
`OnPostLogin`, and `RestartMatch` goes PostMatch → InProgress directly, so round two onward begins
regardless of how many players remain. On a dedicated server configured `MinPlayers > 1` this means
an emptied server loops empty matches forever and a lone joiner lands in a running match, unfrozen.

**Unreachable on the shipped config** (`MinPlayers=1`), which is why it is accepted rather than
fixed: the honest fix routes the restart back through `WaitingToStart`, and that interacts with
`RespawnPlayer`'s InProgress gate — players would sit as corpses in a warmup that nobody can end.
That is a rework of the restart path for a configuration R4 does not ship. **Reopen this the moment
`MinPlayers` is raised above 1, or a dedicated server enters the picture.**

## Deferred beyond R4, deliberately

Scoreboard and match HUD (the state and delegates R4 lands are what they will bind to) · sessions,
lobby and Steam · spectate and killcam · teams and everything that follows from them · map
rotation and travel · bots to fill a match.
