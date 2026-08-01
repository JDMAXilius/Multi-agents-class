# Design Rulings — the ledger the critic judges against

A REFUTER with an attack surface but no statement of intent attacks the design itself,
escalates every round, and never converges — the first data-crew run proved it (three rounds
of "findings" against intended Halo design before rulings were injected). This file is the
fix, made permanent: **rulings already made.** Every critic pass loads it. Attacking a ruling
is out of scope for review; a finding must show a hard-constraint violation or an internal
contradiction. Re-opening a ruling is a *founder/lead* decision, logged here with a date —
never a review outcome.

The lead appends; nobody else writes here. A doubt this ledger closes is closed.

## Combat sandbox (from the 29 Jul 2026 data-crew run)

- **R1. Precision weapons reward aim.** The Magnum's all-headshot TTK beating every other
  path is the intended fantasy, paid for by an 8-round mag and no forgiveness on a miss.
  "Skilled Magnum play is strong" is the design working.
- **R2. The AR is the shield-stripper, not a finisher.** Slower solo TTK is intended; its
  headshot multiplier stays 1.0 — headshot bonuses belong to precision weapons.
- **R3. The Magnum is the finisher, not a self-sufficient primary.** It is NOT required to
  solo a full-200-EHP target from one mag on body shots — the intended line is AR-strip →
  0.4 s swap → Magnum finish.
- **R4. The Rocket is balanced by scarcity, not by damage.** Map pickup, 90 s timer, 2 shots,
  ReserveMags 0 (reload intentionally unreachable). It is SUPPOSED to win the fight it is
  present for. HeadshotMult stays 1.0 (a 240-damage headshot one-shot is a defect — caught
  in the run).
- **R5. Shields-first, no health regen, 2.5 s recharge delay** are pillars, not tunables.
  Proposals touching them go to the founder, not through the pipeline.

## Arena (from the same run)

- **R6. The slice ships one compact map**, not a competitive-ranked layout. Imperfect spawn
  distribution is acceptable when the hard constraints hold (≥ 8 spawns, ≥ 8 m spacing,
  ≤ 35 m sightlines) and the imbalance is documented in the manifest's doubts.
- **R7. Geometry claims are editor-rung.** Mutual visibility, occlusion, and 5 m LOS-breakage
  cannot be settled from coordinates; a manifest that records them in `doubts[]` with that
  caveat has handled them correctly. Only coordinate-provable contradictions are findings.

## Bots & AI (BP08 domain)

- **R8. No LLM in the hot path — structural, not preferential.** Bot decisions in a live
  match are deterministic code reading data. LLMs shape bots offline (tuning rows, ambition
  weights via the curator pipeline) and decorate post-hoc (Spotter strings). There is no
  mid-match model call that a game outcome waits on.
- **R9. One brain: GOAP-style goal layer over a StateTree execution spine** (see
  `BREACHPOINT-AI-BOTS.md`). No second BT asset running in parallel — behavior-tree
  patterns live as selector-shaped tasks *inside* StateTree states. A dual-brain proposal
  is a finding against itself.
- **R10. Plans are short and disposable**: ≤ 3 steps, replanned on event, never per-tick.
  A bot that "thinks" every frame is a perf finding; a plan that survives a world
  contradiction is a correctness finding.
- **R11. The 200 ms reaction floor is a law, not a difficulty knob.** No tier, scalar, or
  ambition weight may produce sub-human reaction; `Breachpoint.Bots.*` pins it.
- **R12. Bots are legible before they are optimal** (the Halo lesson). A bot whose state
  change players can't read (break-off on shield-crack, rocket contest on timer, tier
  fantasy) fails review even if it wins more.

## Crew & pipeline

- **R13. Only `high` severity blocks a landing.** Medium/low land in the risk register with
  the artifact. A reviewer with no ship gate never ships — the run deadlocked to prove it.
- **R14. The orchestrator holds no opinions.** Managers are deterministic (script + lead
  session); intelligence lives inside the boxes. A proposal to add an LLM manager reargues
  a closed ruling.

## Tags & engine seams (31 Jul 2026, from the contract audit)

- **R17. Montage-raised gameplay events extend `Event.*`; there is no `GameplayEvent.*`.**
  The authoritative native-tag header (`ARCHITECTURE §3.1`, landed by BP01) already owns
  `Event.*` for sim-facing gameplay events (`Event.Death`, `Event.Kill`). The
  montage→gameplay notify seam joins it rather than opening a parallel namespace — a second
  top-level namespace meaning the same thing is how a header stops being authoritative.
  `GameplayEvent.Combat.*`, which `animation.md` previously used, is **rejected**: it existed
  in no header, no ticket, and no other document.

  **The four tags, landed by BP01 step 2:**

  | Tag | The moment it announces | Consumed by |
  |---|---|---|
  | `Event.Melee.WindowBegin` | melee trace window opens | `BRGA_Melee` (BP05) |
  | `Event.Melee.WindowEnd` | melee trace window closes | `BRGA_Melee` (BP05) |
  | `Event.Weapon.ReloadCommit` | the point ammo actually moves | `BRGA_WeaponUtility` (BP03) |
  | `Event.Weapon.SwapCommit` | the point the active slot flips | `BRGA_WeaponUtility` (BP03) |

  **Extension rule (so the next one needs no ruling):** montage-raised events are
  `Event.<Verb>.<Moment>` — the verb is the ability's noun (`Melee`, `Weapon`, `Grapple`),
  the moment is what just happened in the animation, never what should result from it.
  A new tag under this rule is a normal `Core/` change, not a re-opening of R17.

  **The boundary this preserves (`animation.md` law 4):** a notify announces a *moment*; the
  sim decides the *consequence*, on the authority. A notify never carries a number, a branch,
  or a damage call. `Event.Weapon.ReloadCommit` says "the hands reached the magazine" — it
  does not say how many rounds, which is a `DT_Weapons` row, and it does not move them, which
  is the ability's job on the server.

  *Why four and not two:* BP03 builds reload **and** swap in one ability (`BRGA_WeaponUtility`)
  and `animation.md` law 3 authors both timings to the sourced pack, so both need a commit
  moment. An unused native tag costs nothing; a missing one costs a `contract_gap` and a stop
  after `Core/` has closed — the asymmetry decides it.

## Online services & Phase 2 (from the GameLift plan, 29 Jul 2026)

- **R15. Identity is Steam-derived; there is no first-party account creation.** Phase-2 auth
  validates the client's Steam session ticket server-side and issues our own short-lived
  token. Any flow that asks a Steam player to sign up (Cognito login UI included) is a
  defect, not an option. Cognito may serve as token machinery only.
- **R16. Managed fleets (real money) are telemetry-triggered, never date-triggered.**
  GL-3 opens only when demo telemetry shows the listen-server pain: host-quit abandonment,
  NAT/join failure rate, host-advantage complaints. Low numbers = the fleet money stays
  unspent and listen + GL-2 keeps shipping. Billing alarms and fleet caps are acceptance
  criteria on any fleet ticket (denial-of-wallet is an exploit class).

## Authoring policy (29 Jul 2026)

- **R18. Zero Blueprint classes; an engine asset exists only where UE 5.8 has no C++ path,
  and every such asset is named in `BREACHPOINT-AUTHORING-MATRIX.md`.** The complete
  Tier-4 list is: AnimBlueprint graphs, materials/instances, Niagara, MetaSounds, UMG
  layout (WBP), the `ST_Bot` StateTree asset, EQS query assets, and sourced art. Anything
  not on that list does not get an asset — it is C++ (Tier 1), text data (Tier 2), or
  generated by a committed script (Tier 3).
  Corollaries that are findings, not preferences: GameplayEffects are **C++ classes**, not
  assets (SetByCaller + dynamic tags make six of them cover the game); GameplayCue handlers
  are **C++ classes** registered by tag (the asset is only the VFX/SFX they play); an
  AnimGraph carrying a gameplay number or decision is a contract violation; a WBP carrying
  anything but layout is a contract violation.
  The reason is reviewability, not taste: **binary assets are invisible to the critic** —
  no diff, no merge, no grep. Adding a Tier-4 asset means adding a part of the game only a
  human staring at the editor can review, so the standing question for any new asset is
  *"which tier, and if Tier 4, why can't C++ express it?"* No crisp answer ⇒ no asset.

## Verification policy (31 Jul 2026 — paid for by a false PASS, not theorized)

- **R19. A build claim requires a timestamp proof. A file existing is not evidence that you
  built it.** Every compile-rung report — verifier, builder observation, CI — carries five
  items per target: (1) wall-clock time printed BEFORE the command, (2) verbatim output tail
  including `Result:` and `Total execution time:`, (3) exit code captured from `$?`,
  (4) the produced binary's mtime, and (5) **an explicit assertion that mtime > start time.**
  Item 5 is the load-bearing one — it is the only check a pre-existing artifact cannot satisfy.
  A report missing it is not a rung result; it is an opinion about a directory listing.

  *Origin:* BP01's first V1 pass reported PASS on three targets having rebuilt one, presenting
  the size and mtime of binaries a **builder** had produced an hour earlier as its evidence for
  the other two — one of which had since been deleted. See BP01's Log.

  *The structural lesson, which generalizes past builds:* the crew's separation of powers
  capability-limits the verifier (no write tools) so that "quietly fixed the test" is
  impossible. That defends against a reviewer **changing** an artifact. It does nothing against
  a reviewer **misreading** one — `ls`, `git diff` and `grep` are all read-only, and a stale
  artifact is indistinguishable from a fresh one unless something forces the time question.
  Read-only is not the same as honest. Where a rung's evidence could be satisfied by state that
  already existed before the check ran, the check must be redesigned, not trusted harder.

- **R20. `-Rebuild` on a monolithic UE target is not safely retryable, and "from scratch" must
  be witnessed, not assumed.** UBT deletes a target's binaries before recompiling, so an
  interrupted `-Rebuild` leaves the tree strictly worse than before it started — BP01 lost its
  game executable this way while the report read PASS. Prefer an incremental build that
  demonstrably completes (under R19's timestamp proof) over a `-Rebuild` that may not. When a
  Done-when clause says "from scratch" and only an incremental build completed, the honest
  verdict is **INCONCLUSIVE**, never PASS — and a build reporting "up to date" with zero
  actions proves nothing at all about compilation.
