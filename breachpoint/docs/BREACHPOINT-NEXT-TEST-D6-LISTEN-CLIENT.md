# D6 — the two-window pass BREACHPOINT has never had

**Cut:** 13 August 2026 by the cloud lead · **For:** the founder, at the keyboard
**Pays:** debt D6 — *"the whole spine is founder-verified **standalone only**; listen+client is
still owed for everything."* Carried since Roadmap 1 and never paid.

## Why this exists as a document

Every feature since R1 has been verified in one PIE window. The honesty ladder says
**PIE ≠ multiplayer**, and the contract says a multiplayer claim comes in **threes** — server,
acting client, observing client. Standalone tests exactly one of those three, and it is the one
that hides every bug of the class BN is most exposed to.

Two of the four defects found in the last self-review — the grenade spawning at rest on clients,
the grenade bouncing off its thrower — are that class exactly. Neither would have appeared in a
single window. Both would have read as *"the grenade is broken"* with no obvious cause.

## Setup, once

PIE toolbar → **Number of Players: 2**, **Net Mode: Play As Listen Server**.

That gives you Window 1 = the **host** (server + a player) and Window 2 = a **client**. Every test
below is run twice — **once acting from the host, once acting from the client** — because those are
different code paths and the client one is where things break.

**Read the log, not just the screen.** Most of what follows is confirmed by a line, and the line
tells you *which machine* produced it. `BNProjectile: blast` on a client's log at all would be a
finding.

---

## The three questions, for every feature

For each row: **A** = does the actor see it · **B** = does the *other* window see it ·
**C** = does the server agree (health moved / the log line exists on the server).

| # | Do this | A — actor | B — observer | C — server truth |
|---|---|---|---|---|
| 1 | **Aim** — look up and down | weapon + arms follow | the *other character* pitches the same way | — |
| 2 | **Lean** — hold Q / E | torso tips sideways | same tilt on the other window | — |
| 3 | **Crouch, jump, sprint** | pose changes | pose changes on the other window | — |
| 4 | **Swap weapon** | mesh + anim layer change | same weapon in the other window's hands | — |
| 5 | **Fire** | muzzle flash, tracer, **sound** | flash and tracer visible on the observer | `BNGA_Fire: validated hit` on the **server** |
| 6 | **Fire at the other player** | hit registers | impact FX + decal at the same spot | victim's health drops |
| 7 | **Reload** | montage + ammo returns | montage plays on the observer | ammo restored server-side |
| 8 | **Melee (V)** | swing plays, connect lands *in* the swing | swing visible on the observer | `BNGA_Melee: validated connect` on the **server** |
| 9 | **Grenade (G)** | leaves the hand, flies where you looked | **same arc, same bounces** on the observer | `BNProjectile: blast` on the **server only** |
| 10 | **Die** | death + respawn | corpse then respawn on the observer | — |

**A goal is DONE only when all three columns pass from BOTH windows.** Column B from the client
acting is the one that has never been tested in this project.

---

## The specific things most likely to fail, and what they look like

These are predictions from the code, not observations — if one of them happens, it is a finding,
not a surprise.

| Symptom | Almost certainly |
|---|---|
| Aim works on your own screen but the other character stares straight ahead | `GetBaseAimRotation` is not decompressing `RemoteViewPitch` on the proxy — the replicated half of G1 |
| The grenade flies differently in each window | `InitialVelocity`'s OnRep is not landing before the movement component starts simulating |
| The grenade explodes twice, or damage is doubled | The overlap is billing a target twice, or the fuse is running on a client |
| Melee connects for the host but never for the client | The `AN_FPST_Melee` notify question — check whether the 0.25s fallback line appears in the log |
| Melee damage happens on press instead of mid-swing | The fallback fired instead of the notify. Working, but the timing is wrong |
| Tracer/impact visible to the observer but NOT to the shooter | The prediction-key trap — a multicast carrying the key is skipped on the machine that generated it |
| Sprint puts the client ~15-30cm ahead of where the host sees them | **Known — that is D1**, not a new finding. Converges within ~1 RTT |

## What is knowingly NOT covered

- **Dedicated server.** The ladder is `listen ≠ dedicated`; this pays the listen rung only.
- **Latency.** Everything above is on a loopback with ~0 RTT, which hides every timing bug.
  `Net PktLag=100` in the console is the cheap next step once the zero-latency pass is clean.
- **Late join.** *"Would this survive a second player joining late?"* is one of the critic's
  standing questions and none of it is exercised here.
- **Three or more players.** Two windows cannot show a bug that needs a third observer.

Say so plainly when reporting: *"listen+client, two players, zero latency"* is the rung this
buys — nothing above it.

## Reporting back

Per feature, one line: **what you did · which window · what each of the three columns did.**
A failure needs the window it happened in, or it cannot be diagnosed — *"melee doesn't work"* is
not actionable; *"melee from the client: swing plays on both windows, host's health never moves,
no `validated connect` line on the server"* points straight at the claim path.
