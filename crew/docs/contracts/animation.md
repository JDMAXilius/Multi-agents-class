# Contract — Animation (feel is measured; threads are law)

Status: v1 · Owner: anim-builder · Binds every packet touching `Source/SR/Animation`, the AL
Framework, AnimGraph nodes, montages, or motion warping.
The boundary in one line: **animation requests and presents; it never decides gameplay.**

## Laws

1. **Worker-thread discipline.** Anim node update/evaluate runs on the worker thread where the
   pattern allows. Inside evaluate: no allocation, no locks, no UObject mutation, no
   game-thread reads outside the proxy's cached data. A game-thread anim hack is a finding
   even when it "works" — it is a hitch that hasn't happened yet.
2. **The AL Framework is the spine.** New animation behavior goes through the linked-anim-layer
   architecture and the PoseSearch schema. Per-feature AnimInstance forks, hand-picked clip
   bypasses, and "temporary" state machines outside the framework are findings with a named
   home to move to.
3. **Warp targets come from replicated truth.** Motion warping on strikes targets
   `SoftLockTarget` (replicated, netcode-owned). A warp target sourced from a client-only
   trace or a local guess is a netcode finding, not a feel tweak.
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

## Slash Roller specifics

- Framework: AL Framework (KLS-derivative, 16 files), 7 custom C++ AnimGraph nodes,
  Lyra-style linked layers — locomotion / traversal / combat.
- Strike alignment: soft-lock (`SoftLockTarget`) → score-based target assist → motion warping,
  in that order; warping layers on top, never replaces, target assist.
- Montage → gameplay seam: notify windows raise `GameplayEvent.Combat.*`; hit confirmation is
  server-side in the damage GE execution (see `netcode.md` law 3).
