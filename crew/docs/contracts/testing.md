# Contract — Testing (the validation ladder)

Status: v1 (filled for BREACHPOINT) · Runner: verifier (read-only) · A rung skipped is a lie
waiting to surface. Every packet names which rungs apply; netcode packets always include rung 4.

## The ladder

**1. Clean compile.** UBT for the packet's targets (Editor + Game + Server) from a clean
state. Live-coding/hot-reload state is discarded first — stale patching lies.
```
<UE>/Engine/Build/BatchFiles/RunUBT.(bat|sh) <Project>Editor Win64 Development -project=<uproject>
# + the <Project>Server target for netcode packets
```

**2. Headless unit specs** (Automation Framework, no rendering):
```
UnrealEditor-Cmd <uproject> -ExecCmds="Automation RunTests <Project>.<SuitePrefix>; Quit" \
  -unattended -nullrhi -nosplash -log -ReportExportPath=<dir>
```
Sim suites are PINNED (exact values + invariants). A suite asserting only "no crash" is a
reportable finding. Any pinned number a packet moves must be moved loudly, with the reason in
the ticket.

**3. Functional tests** (level-based, single-instance) for the packet's maps — interaction,
spawning, triggers. Honest scope note: engine functional tests are effectively single-process;
they prove PIE behavior, NOT multiplayer behavior.

**4. Networked smoke — the rung that actually proves multiplayer.** Dedicated server + 2
clients via **Gauntlet** (the engine's C# automation driver; the ShooterGame sample is the
reference implementation). Minimum scenario per netcode packet: both clients join → client A
performs the packet's core action → assert server state, client A view, and client B view
AGREE → run once more under net emulation (e.g. `-PktLag=120 -PktLoss=5` or the Network
Emulation profile) for anything timing-sensitive. Single-process PIE is NOT a substitute; if
Gauntlet isn't wired yet, this rung reports BLOCKED — never quietly passed.
```
RunUAT RunUnreal -project=<uproject> -platform=Win64 -configuration=Development \
  -build=editor -test=<YourGauntletTestName>
```

**5. Perf spot-checks** the packet names: `stat unit`, `stat net`, per-class bandwidth and
tick-count deltas for new actors/widgets. **The budget numbers live in
`docs/BREACHPOINT-QUALITY-BARS.md` §1–2 — that document is the single home for rung-5
thresholds.** `netcode.md` states the replication *laws*; it does not restate the numbers.

## Grep gates (run on every rung-2 pass — a hit is a reported finding)

The gates are part of the ladder, not a separate ritual, and the verifier runs them because
this contract says so. **`BREACHPOINT-QUALITY-BARS.md` §3 item 4 is the canonical list**;
it is reproduced here so the runner never has to leave its own contract to know what to run:

| Gate | Law behind it |
|---|---|
| `TakeDamage` · `ApplyRadialDamage` · `ApplyPointDamage` · `FDamageEvent` | `gas-purity.md` law 3 — the engine damage API is banned |
| Direct attribute setter calls outside the AttributeSet's own hooks | `gas-purity.md` law 1 |
| `AddLooseGameplayTag` outside cosmetic-marked sites | `gas-purity.md` law 5 |
| `ConstructorHelpers` · hard `UPROPERTY` asset refs | `data-and-assets.md` — soft refs at the data boundary |
| A gameplay literal next to a gameplay noun | `data-and-assets.md` — numbers are rows |
| `NativeTick` in widgets · UMG property bindings | `CLAUDE.md` law 4; `ue5-ui-architecture` skill §8 |
| Gameplay Tick outside timers/delegates/events/cue notifies | `CLAUDE.md` law 4 |

A gate that has never fired has not been tested — prove each one once against a deliberate
violation, the same way rung 2 is proven red-then-green, and record it in the ticket Log.

## Cross-cutting rules

- **Multiplayer assertions come in threes**: server truth, acting client's view, observing
  client's view. A server-only assertion is how "works for the host" ships.
- **Packaged sanity** before any release-facing milestone: cook + package + the smoke scenario
  on the packaged build. Editor ≠ packaged (cook strips, config layering).
- Every "works" claim in any report names its rung: `works (rung 4: Gauntlet 2-client,
  +emulation)` — anything else is an opinion, labeled as one.
- CI runs rungs 1–3 on every push; rung 4 on netcode-touching branches (or nightly if runner
  capacity demands); rung 5 tracked per milestone.

## Fill-ins — BREACHPOINT (refilled 2026-07-29; supersedes the Slash Roller fill)

- Engine path / UAT wrapper scripts: **UE 5.8** install rooted at `<ENGINE_ROOT>` (set per
  machine in `Tools/env.local`, never committed); wrappers: `Tools/run-ubt.ps1`,
  `Tools/run-specs.ps1`, `Tools/run-gauntlet.ps1`. Targets: `BreachpointEditor`,
  `Breachpoint`, `BreachpointServer` — **all three compile on every rung-1 run.**
- Gauntlet test project + first smoke scenario: **`BRGauntlet.SmokeTS2C`** — dedicated server
  + 2 clients join `BR_Arena01`; client A shoots client B; assert in threes (server truth,
  A's view, B's view agree); repeat under `-PktLag=120 -PktLoss=5`.
  Bootstrapping this IS the first crew ticket — see `docs/tickets/TICKET_BP00_LADDER.md`.
- Headless spec suite prefix: `Breachpoint.Sim.*` (pinned combat/shield/match suites — values
  asserted against `DT_Weapons`/`CT_Combat`, never literals) + `Breachpoint.Bots.*`
  (determinism: same seed + tuning row ⇒ identical action trace).
- CI runner realities: rungs 1–3 on every push (`-nullrhi`); rung 4 nightly and on any branch
  touching a replicated header or `Server` RPC; rung 5 per milestone. Overnight bot-vs-bot
  soak (20 matches, seeds logged) runs with the nightly rung 4 once bots land.
- ⚠️ **Every rung needs an engine — "cloud runner" is not a cheap tier.** Rung 1 is UBT across
  three targets and rung 2 is `UnrealEditor-Cmd`; both require a full UE 5.8 install, and the
  `BreachpointServer` target requires a **source-built** engine, not the launcher install
  (`BREACHPOINT-GAMELIFT-PLAN.md` §1). So there is no runner tier that skips the engine —
  only runners that skip the *second process* (rung 4). Provisioning and cost for a
  source-built-UE runner are **unpriced**; BP11 step 4 owns that call, and its Done-when
  ("CI posts ladder results without a human") is not satisfiable until it is made. Until then
  CI is a local/self-hosted runner and the ticket says so, rather than assuming a cloud tier
  that cannot compile the project.
