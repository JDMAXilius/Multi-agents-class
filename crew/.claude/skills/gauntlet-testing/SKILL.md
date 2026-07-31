---
name: gauntlet-testing
description: Authoring the networked test rung for BREACHPOINT — Gauntlet test nodes, role configuration (dedicated server + 2 clients), assert-in-threes via role artifacts, net emulation, and BLOCKED reporting. Load for BP00 step 3, for any packet adding a rung-4 scenario, and for anything touching Tools/run-gauntlet.ps1. The contract (docs/contracts/testing.md) owns the ladder and is LAW; this skill is HOW rung 4 gets built.
---

# Gauntlet — building the rung that actually proves multiplayer

> ## ⚠️ UNVERIFIED DRAFT — correct it in the packet that first uses it
> **This skill has never been run against a build.** It was written from UE documentation and
> the project's own architecture, not from a Gauntlet test that compiled and went green. The
> *doctrine* (§1, §5, §7) is sound and follows from `testing.md`. The **exact C# API surface
> in §2–§4 is the part most likely to be wrong** — Gauntlet lives in the engine's automation
> tree, has no public API stability guarantee, and its shape has moved across 4.x/5.x.
>
> **BP00 step 3 is its first use. That packet owns correcting it**: whatever the real API
> turns out to be, fix this file in the same packet and note the delta in the ticket Log. A
> skill that disagrees with a working test is a finding against the skill (`gas-purity`
> precedent). Do not treat anything below as authority over what the compiler says.

**Authority note:** `docs/contracts/testing.md` defines the ladder, which rungs bind which
packets, and the BLOCKED law. It wins on every conflict. This skill only shows how rung 4 is
constructed.

## 1. What Gauntlet is, and why the shape surprises people

Gauntlet is **a process orchestrator and artifact harvester written in C#** — not an
in-process assertion framework. It builds/deploys the project, launches N roles (a dedicated
server, two clients), waits, tears them down, and hands you each role's artifacts.

The consequence that shapes every scenario we write: **Gauntlet cannot see gameplay state.**
It sees processes, exit codes, and logs. So "assert in threes" is not three `check()` calls in
one place — it is:

1. the game, on each role, **emits a structured log line** describing that role's view of the
   fact under test;
2. the Gauntlet node **parses all three roles' logs** after the run and asserts they agree.

Design the log line first; the test is the easy half. A line the parser can key on and diff —
one token per field, no prose:

```cpp
UE_LOG(LogBRNet, Display, TEXT("BRTEST|shield_hit|victim=%s|shields=%.1f|role=%s"),
       *VictimId, Shields, *RoleName);
```

This is also why rung 4 catches what PIE cannot: three *processes* independently reporting,
rather than one process inspecting itself.

## 2. Where the test lives

Gauntlet tests are C# compiled into the AutomationTool, not C++ in the game module. Expect a
project automation assembly — conventionally `Build/Scripts/<Project>.Automation.csproj` —
referencing the engine's `Gauntlet` and `AutomationUtils`. The class name is what
`-test=` resolves, which is why `testing.md` names the scenario `BRGauntlet.SmokeTS2C`
(namespace `BRGauntlet`, class `SmokeTS2C`).

> ⚠️ The csproj location, the reference set, and whether the assembly is discovered
> automatically or must be registered are the **first things to verify** — they vary by engine
> version and are the most common reason `-test=` reports "test not found".

## 3. Role configuration — dedicated server + 2 clients

The shape: subclass the test node, override the configuration, require the roles, put each
role's flags on its own command line.

```csharp
public class SmokeTS2C : UnrealTestNode<UnrealTestConfig>
{
    public SmokeTS2C(UnrealTestContext c) : base(c) { }

    public override UnrealTestConfig GetConfiguration()
    {
        UnrealTestConfig Config = base.GetConfiguration();

        // Dedicated server — NOT a listen server. The slice ships listen, but the rung
        // exists to prove authority independent of a local player (see netcode.md).
        var Server = Config.RequireRole(UnrealTargetRole.Server);
        Server.CommandLine += " -BR_Arena01 -nosteam";

        // Two clients: the acting client and the observing client. Two is the minimum
        // that can catch "works for the host" and "works for the shooter".
        var Clients = Config.RequireRoles(UnrealTargetRole.Client, 2);
        foreach (var Client in Clients) Client.CommandLine += " -nosteam";

        Config.MaxDuration = 10 * 60;   // fail loudly rather than hang CI
        return Config;
    }
}
```

> ⚠️ `RequireRole` vs `RequireRoles`, whether the config generic is `UnrealTestConfig` or a
> project subclass, and how controllers attach to a role are the specific names to check
> against the engine tree. The *structure* — require roles, per-role command lines, bounded
> duration — is stable; the identifiers may not be.

## 4. Driving the scenario and harvesting the result

Two ways to make something happen in-game, and the choice matters:

- **Gauntlet controllers** (a `UGauntletTestController` subclass in C++, attached to a role)
  drive the client from inside the game — this is how client A actually shoots client B.
- **Command line + autonomous behavior** — simpler, but only works when the scenario needs
  no input.

For `SmokeTS2C` the acting client needs a controller: it waits for both clients to be
possessed and replicated, fires at the victim, and logs its view. The server and observer log
theirs from ordinary gameplay code — **no test-only gameplay paths** (a test that exercises a
special path proves the special path).

After the run, get each role's artifacts, parse, and compare. Report by *disagreement*, not
by "did it crash":

```csharp
public override void StopTest(StopReason Reason)
{
    base.StopTest(Reason);
    // Per-role artifacts → log parse → extract every BRTEST| line → assert the
    // shield_hit records for the same victim agree across server / actor / observer.
}
```

**Assert agreement, not values.** The server's number is truth by definition; the test's job
is that the two clients *match it*. A test asserting "shields == 92" on the server alone is a
rung-2 spec wearing a rung-4 costume.

## 5. Net emulation — the second run, not a different test

Every timing-sensitive scenario runs **twice**: clean, then degraded. Per `testing.md` the
profile is `-PktLag=120 -PktLoss=5`, appended to the *client* roles' command lines.

Run them as two executions of the same scenario, not one test with a flag — the clean run
tells you the logic is right, the degraded run tells you the prediction is. A scenario that
only ever passes clean has not tested reconciliation, and grapple (BP06) is exactly where that
matters.

## 6. Wiring `Tools/run-gauntlet.ps1`

The wrapper is what the verifier and CI actually call, so it takes the scenario name and
resolves everything else from `Tools/env.local`:

```
RunUAT RunUnreal -project=<uproject> -platform=Win64 -configuration=Development \
  -build=editor -test=BRGauntlet.SmokeTS2C
```

The wrapper must **propagate the real exit code** and print the failing output verbatim.
`testing.md` and BP14 both require the verifier to report command, exit code, and failing
output — a wrapper that swallows stderr or returns 0 on failure defeats the whole ladder, and
it is the single easiest place to accidentally lie.

## 7. BLOCKED is a real result

Until this rung runs end to end, packets naming rung 4 report **BLOCKED with the reason** —
never "passed", never silently skipped (`testing.md`, and CLAUDE.md law 6). BP00's own
Done-when requires proving the rung can *fail*: break replication deliberately once, watch the
smoke go red, record it. A gate that has never failed is a gate nobody has tested.

## 8. Self-check before handoff

Structured `BRTEST|` log lines emitted by **ordinary gameplay code**, not test-only paths ·
three roles reporting, assertions comparing them · dedicated server, not listen · clean run
AND emulation run, both recorded · `MaxDuration` set so a hang fails instead of wedging CI ·
wrapper propagates exit code and verbatim output · the deliberate-break proof recorded in the
Log · every claim names its rung.
