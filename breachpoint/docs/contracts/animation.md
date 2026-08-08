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

---

# Amendment A — the reference audit, and the C++/graph boundary

**Added 7 Aug 2026** on founder direction: *"take reference from Zorans, ShooterCore, Variant
Shooter, Lyra … if possible transfer everything to C++ instead of blueprints like Lyra does."*
This amendment closes an ambiguity the original contract left open — law 2 mandates "one
AnimInstance spine per mesh **built on shared C++ base state**" and that shared C++ base **does
not exist**, while `BREACHPOINT-GAMEPLAY-REWORK.md` §3.5 budgets `Character/` at two units and
adds none. Amendment A does not create the unit; it fixes what the unit must be when it is cut.

## A.1 What to take from each reference, and what not to

The gameplay rework audited three of these across 33k lines and its verdict was that the audit
**validated the existing laws rather than replacing them**. The same discipline applies here:
read for *patterns*, never for code to paste.

| Source | Verdict | What to take | What to refuse |
|---|---|---|---|
| **Lyra / ShooterCore** (one lineage) | ✅ **primary reference** | The thread-safe update shape; `FGameplayTagBlueprintPropertyMap`; the Anim Layer Interface; inertialisation as the default blend | Its **graphs**. Lyra's ABPs carry large node networks — that is exactly what R18 forbids here |
| **Variant_Shooter** (in-repo, deleted by BP90) | ⏳ **read before deletion** — see A.3 | Proof that per-weapon AnimInstance swapping works, and what it costs | The pattern itself. Layers supersede it |
| **ZoransResistance** | ❌ **no** | — | The rework already found 5 damage paths, one bug copy-pasted into two, ~600 lines to move a hit client→server. Nothing suggests its animation is better-disciplined |
| **UE5_Multiplayer_FPS** | ❌ not unless a question survives Lyra | — | Same tier |

**The single highest-value finding is `FGameplayTagBlueprintPropertyMap`** (GameplayAbilities
plugin — confirm the exact include at first compile; it is not resolvable from a cloud
container). It binds gameplay tags directly to `bool` properties on the AnimInstance through one
registered callback. It **replaces hand-written tag→bool caching entirely**, which means the
graph can never drift from what the ASC actually says. Using it is not copying Lyra; it is using
the engine properly, and it is the difference between a cached bool and a *correct* cached bool.

## A.2 The C++/graph boundary — how far "everything in C++" actually goes

**R18 names `AnimBlueprint graphs` as Tier 4**: the node network is the one thing UE 5.8 has no
C++ authoring path for. That is a fact about the engine, not a preference, and this amendment
does not pretend otherwise. **What it does is shrink the graph to the smallest thing that can
still be called a graph.**

| Concern | Where it lives | Note |
|---|---|---|
| Tag → bool state | **C++**, via `FGameplayTagBlueprintPropertyMap` | zero graph nodes, zero hand-written caching |
| Locomotion maths, aim offsets, lean, turn-in-place | **C++**, `NativeThreadSafeUpdateAnimation` | graph reads fields; it never computes |
| Sway · bob · recoil · spring damping | **C++ custom `FAnimNode_*`** | the graph places ONE node; the code does the work. Lyra does not do this and it is where we go further |
| Per-weapon poses | **C++ interface** + thin layer assets | `IBRAnimLayers`; the layer asset is data, the contract is code |
| Montage playback + notify forwarding | **C++** | already law 4 |
| State machines, blend spaces, the layer stack | **ASSET — unavoidable** | no C++ path exists in 5.8 |

**The reviewability test, restated from R18's own reasoning:** binary assets are invisible to the
critic — no diff, no merge, no grep. So the standing question for every node added to an AnimGraph
is *"why can't C++ express this?"* A graph that carries a **decision** is already a contract
violation under R18's corollary; this amendment adds that a graph carrying **computation** is a
design smell with a named home to move to (a custom anim node).

**What this buys, concretely:** the ABP becomes reviewable by reading C++, and the asset becomes
a wiring diagram. That is the same trade the UI layer already made — a WBP carries layout and
nothing else, and every value it shows is computed in `BRButton.cpp` where a critic can diff it.

## A.3 The Variant_Shooter record — captured because BP90 deletes it

`BREACHPOINT-GAMEPLAY-REWORK.md` §9 marks `Variant_Shooter/` **DELETED** (36 files). It is
currently the only 1P/3P + per-weapon animation example in the repository, so its shape is
recorded here before it goes:

```
ShooterCharacter.cpp:256   GetFirstPersonMesh()->SetAnimInstanceClass(Weapon->GetFirstPersonAnimInstanceClass());
ShooterCharacter.cpp:257   GetMesh()          ->SetAnimInstanceClass(Weapon->GetThirdPersonAnimInstanceClass());
ShooterWeapon.h:58/61      TSubclassOf<UAnimInstance> FirstPersonAnimInstanceClass / ThirdPersonAnimInstanceClass
Content/Variant_Shooter/Anims/   ABP_FP_Weapon · ABP_FP_Pistol · ABP_TP_Rifle · ABP_TP_Pistol
```

**It swaps the whole AnimInstance class per weapon.** That works, and it is the wrong shape for
us for three reasons, each checkable:

1. **State is destroyed on every swap.** A new AnimInstance means new cached state, so any
   blend, additive or inertialised transition in flight is lost at exactly the moment a player
   swaps weapons under fire.
2. **N weapons = N × 2 full ABPs**, each re-implementing locomotion. That is the same
   "one class per variant" mistake the button module just spent a session undoing.
3. **The C++ hard-refs an AnimInstance class per weapon**, which collides with governing idea 5
   of the rework — *C++ knows tags and row handles, never an asset*.

**The ruling: Linked Anim Layers, not AnimInstance swapping.** One spine per mesh (law 2 already
says so), `LinkAnimClassLayers()` on equip, the layer class resolved from the **weapon row** as a
soft reference through `BRGameData`. Adding a weapon is then a row plus a layer asset and **zero
C++**.

## A.4 What is still blocked, and by what

- **Every timing number** — fire cadence, reload length, melee windows, grapple pull. Law 3:
  authored TO the acquired set, never retimed to fit. **No sourced pack has landed**; the 113
  animation assets on disk are Epic template. This blocks tuning, not structure.
- **Every feel value** — blend times, warp windows, distance-match ranges. Law 6: stated in
  numbers, judged in a build.
- **Thread-safety proof** — the failure mode is a hitch under load, not a wrong pose. Needs a
  profiler on a real build, not a review.

The C++ spine is **not** blocked by any of the above and can be written now. It is the piece
`BP96_CHARACTER` will otherwise build around, leaving someone to bolt an anim seam on afterwards.

## A.5 Motion warping — a gap the original contract implied but never bounded

Law 6 lists "warp windows" as a feel number, so warping is in scope, but nothing bounds it.
**Bound here:** motion warping *is* displacement, and the animation laws forbid anim-driven
displacement outside the CMC. Therefore the warp target is supplied by the **ability's
server-validated trace**, never chosen by the AnimBP. An AnimGraph that picks a warp target is
selecting a victim, which is a gameplay decision, which is law 4.
