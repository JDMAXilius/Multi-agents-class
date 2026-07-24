# Contract — Testing (the validation ladder)

Status: v1 template · Runner: verifier (read-only) · A rung skipped is a lie waiting to
surface. Every packet names which rungs apply; netcode packets always include rung 4.

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

**5. Perf spot-checks** the packet names: `stat unit`, `stat net`, per-class bandwidth vs the
budget in `netcode.md`, tick-count deltas for new actors/widgets.

## Cross-cutting rules

- **Multiplayer assertions come in threes**: server truth, acting client's view, observing
  client's view. A server-only assertion is how "works for the host" ships.
- **Packaged sanity** before any release-facing milestone: cook + package + the smoke scenario
  on the packaged build. Editor ≠ packaged (cook strips, config layering).
- Every "works" claim in any report names its rung: `works (rung 4: Gauntlet 2-client,
  +emulation)` — anything else is an opinion, labeled as one.
- CI runs rungs 1–3 on every push; rung 4 on netcode-touching branches (or nightly if runner
  capacity demands); rung 5 tracked per milestone.

## Fill-ins — SLASH ROLLER (filled 2026-07-22)

- Engine path / UAT wrapper scripts: **UE 5.8** install rooted at `<ENGINE_ROOT>` (set per
  machine in `Tools/env.local`, never committed); wrappers: `Tools/run-ubt.ps1`,
  `Tools/run-specs.ps1`, `Tools/run-gauntlet.ps1`. Targets: `SlashRollerEditor`,
  `SlashRoller`, `SlashRollerServer`.
- Gauntlet test project + first smoke scenario: **`SRGauntlet.SmokeDM2C`** — dedicated server
  + 2 clients join `SR_Arena01`; client A lands a light attack on client B; assert in threes
  (server HP truth, A's view, B's view agree); repeat under `-PktLag=120 -PktLoss=5`.
  Bootstrapping this IS the first crew ticket — see `docs/tickets/TICKET_BOOTSTRAP_LADDER.md`.
- Headless spec suite prefix: `SlashRoller.Sim.*` (pinned combat/stamina suites) +
  `SlashRoller.Bots.*` (determinism: same seed + tuning row ⇒ identical action trace).
- CI runner realities: rungs 1–3 on every push (cloud runner, `-nullrhi`); rung 4 nightly and
  on any branch touching `Source/SR/Net/**` or a replicated header (local runner with engine
  install); rung 5 per milestone. Overnight bot-vs-bot soak (20 matches, seeds logged) runs
  with the nightly rung 4.
