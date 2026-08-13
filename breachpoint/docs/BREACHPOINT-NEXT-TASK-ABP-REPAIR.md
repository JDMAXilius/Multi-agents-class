# TASK — repair ABP_BNMannequin through the Unreal MCP

**Cut:** 13 August 2026 by the cloud lead · **For:** the terminal session (editor + MCP in reach)
**Binds to:** the NEXT doc family only. **Owner path:** `Content/BN/Animation/` + `Tools/bn/`.

**The C++ is not in scope and needs no change.** Verified before writing this: `GroundDistance`
is declared `UPROPERTY(EditAnywhere, BlueprintReadWrite) double` at `BNAnimInstance.h:100` and
published unconditionally at `BNAnimInstance.cpp:196` — not behind `bNativeOwnsTurnState`, in the
same block as `isCrouching`/`IsJumping`/`IsFalling`, which resolve fine. Do not "fix" C++ here.

## The deduction that scopes this task

The founder's editor log shows **only `GroundDistance` failing**. A stale or broken module binary
cannot be selective — all 27 ported properties would fail together. 26 bind, one does not.
Therefore the running class is correct and **both remaining errors are serialized inside the
`.uasset`**:

| Error | What it actually is |
|---|---|
| `Could not find a variable named "GroundDistance_0"` (×2 nodes) | two **Get nodes** still store a reference to a variable name that no longer exists anywhere — the collision-rename artifact, orphaned when absorption deleted it |
| `The property associated with Ground Distance could not be found in '/Script/BreachpointNext.BNAnimInstance'` | an **AnimGraph property binding** (the space is a display name) on a node pin — same dead `_0` target, different storage |

`GroundDistance` is the only casualty precisely because the source ABP consumes it **both** ways —
plain Get nodes *and* a node binding — so one rename orphaned it twice. `Result was visible but
ignored` is benign fallout and clears with the compile.

## The task

Repair `/Game/BN/Animation/ABP_BNMannequin` through the **Unreal MCP server** so it compiles.

1. **Diagnose first, mutate second.** Print the ABP's current variable table and every node/binding
   that references a `*_0` name, before changing anything. Evidence in the Log, not assumptions.
2. **Re-point the two `Get GroundDistance_0` nodes** to the inherited C++ `GroundDistance`.
3. **Re-bind the anim-node property binding** whose pin targets Ground Distance to the same
   inherited property.
4. **Sweep, don't special-case.** `GroundDistance` is the one that surfaced; check for any other
   orphaned `*_0` reference in the same pass and repair it the same way.
5. **Compile + save, then read back.** The compile result and a zero-error read-back are the proof —
   "the call returned" proves nothing (`replace_variable_references` is void).

`Tools/bn/35_repair_abp.py` already implements 1–5 as a scripted, audited pass and refuses to run
against a stale class. Use it, or drive the MCP directly — either is fine, but the audit table is
the deliverable.

## Two MCP traps already paid for — do not re-learn them

- **`BlueprintTools.create`'s `asset_type` is the PARENT class, not the Blueprint kind.** Passing
  `/Script/Engine.Blueprint` raises `UBlueprintFactory`'s *Pick Parent Class* modal, which blocks
  the editor's game thread forever on an unattended call and wedges every later MCP request.
- **The session's MCP client may fail to attach at startup and does not re-bind mid-session.** The
  proven fallback is JSON-RPC straight over the server's HTTP transport (`.mcp.json` →
  `http://127.0.0.1:8000/mcp`); that is how the five BN assets were created on 12 Aug.

## Standing rule this task inherits

Enum-typed BP variables stay **Blueprint-owned** until graph-clear day, when the C++ `UENUM`s and
the graph retype land together in one change. `30_reparent_abp.py` enforces it: an enum pin is
never a match for absorption. Do not absorb one here.

## Done means

`ABP_BNMannequin` compiles with zero errors, saved, and the audit/read-back is pasted into this
file's Log. Then the founder's checkpoint run is unblocked: look-down-see-body, the mannequin
animating through the linked unarmed layer, crouch toggle, crouch-jump — standalone, then
listen-server + client.

## Log

_(terminal session: append the diagnosis table, what was repaired, and the compile read-back here)_
