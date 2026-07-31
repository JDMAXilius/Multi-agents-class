# Contract — Animation (feel is measured; threads are law)

Status: v1 (filled for BREACHPOINT) · Owner: anim-builder · Binds every packet touching the
character's AnimBlueprints, AnimGraph nodes, montages, notify seams, or motion warping —
i.e. `Content/Characters/` anim assets plus the anim-facing members of
`Source/Breachpoint/Character/`.
The boundary in one line: **animation requests and presents; it never decides gameplay.**

> **Owner-path note (`Character/` is shared).** `ARCHITECTURE §9` assigns `Source/Breachpoint/
> Character/` to **builder**; anim-builder joins at anim. So a packet that touches this folder
> names ONE of them as its owner and the other as a consult — never both as writers. In BP07
> step 3 the writer is **anim-builder** (ABPs + the C++ base's anim members); `BRCharacter`'s
> non-anim surface stays builder's. If a packet needs both, it is two packets with a handoff
> (game-lead skill, "decompose before dispatching").

## Laws

1. **Worker-thread discipline.** Anim node update/evaluate runs on the worker thread where the
   pattern allows. Inside evaluate: no allocation, no locks, no UObject mutation, no
   game-thread reads outside the proxy's cached data. A game-thread anim hack is a finding
   even when it "works" — it is a hitch that hasn't happened yet.
2. **One AnimInstance spine per mesh.** First-person arms and third-person body each get ONE
   ABP built on shared C++ base state; per-feature AnimInstance forks, hand-picked clip
   bypasses, and "temporary" state machines outside it are findings with a named home to
   move to.
3. **Sourced sets are the fixed constraint.** Ability timings (fire cadence, reload length,
   melee windows, grapple pull) are authored TO the acquired animation sets — never stretch
   or hand-retime a sourced clip to fit a number; change the number (it lives in a table).
4. **Notifies raise events; the sim decides.** Montage notify windows emit gameplay events;
   damage, cost, and hit decisions happen in sim code on the authority. Any gameplay number or
   branch living in a notify, AnimInstance, or AnimBP graph is a violation
   (`data-and-assets.md` applies: logic in C++, numbers in tables).
5. **Rollback leaves no residue.** Predicted montages, warps, and their cues must cancel
   clean: no flags, counters, cooldowns, or audio surviving a rejected prediction. Cancel
   cleanliness is part of every anim packet's acceptance criteria.
6. **Feel changes are stated in numbers.** Blend times, warp windows, trajectory weights,
   distance-match ranges: before/after values in the ticket, judged in a build. "Feels
   better" without numbers is an opinion and is labeled as one.
7. **Proxy honesty.** Owning-client, server, and simulated-proxy views can differ; every anim
   claim names which view it was verified on. The floor for "warp looks right" is a
   networked check with a simulated proxy (rung 4 scenario or editor multi-process, reported
   as which).

## Breachpoint specifics

- Framework: **sourced FPS animation sets** (arms + weapons, marketplace) on one first-person
  ABP + one third-person ABP, shared C++ base; the anim pack is chosen in Week 1 and becomes
  the timing constraint for every ability (GDD risk register).
- Grapple presentation: the pull is a **root-motion source through the CMC** — animation
  layers reaction poses on top; it never drives the movement itself.
- Montage → gameplay seam: notify windows raise gameplay events (melee trace window, reload
  commit point); hit confirmation is server-side in the damage GE execution (see `netcode.md`).
  Reload cancel before the commit notify refunds nothing and costs nothing — cancel-clean by
  construction.

**The seam's tags (ruling R17, closed 31 Jul 2026 — do not re-litigate):**

| Tag | The moment it announces |
|---|---|
| `Event.Melee.WindowBegin` / `Event.Melee.WindowEnd` | melee trace window opens / closes |
| `Event.Weapon.ReloadCommit` | the point ammo actually moves |
| `Event.Weapon.SwapCommit` | the point the active slot flips |

All four are declared in `BRGameplayTags` by **BP01 step 2** — `Core/` closes with that
ticket, so a packet needing a *new* seam tag files a `contract_gap` rather than adding one.
Extension rule for that follow-up: `Event.<Verb>.<Moment>`, where the moment is what just
happened in the animation, never what should result from it. (This contract previously wrote
these as `GameplayEvent.Combat.*` — a namespace that existed in no header; R17 rejected it.)
