# TICKET — which writer wins the aim surface: an AUDIT, not a fix

**Cut:** 17 August 2026 by the cloud lead · **For:** `bn-editor` / the terminal session (Unreal MCP)
**Read [`ASSET-RULES`](BREACHPOINT-NEXT-ASSET-RULES.md) §5 first.**

## THE SCOPE — read this twice

This ticket **changes nothing**. It answers four questions and reports. It does not edit a graph,
does not delete a variable, does not recompile anything to "see if that fixes it", and does not
touch `Source/`. **If you find the cause, you still stop and report it.** The decision on what to
change is the lead's, and the last three attempts at this bug were lost precisely to changes made
before the cause was known.

The read-back IS the deliverable.

## Why this exists

The aim chain has survived four rounds of C++ fixes. `DIAGNOSTICS.md` §2 records the current
measured state, and it leaves exactly two suspects:

> `ABP_Mannequin_Base` still carries its twenty Lyra update functions and still runs them from
> `BlueprintThreadSafeUpdateAnimation`, which the engine invokes **after** the native thread-safe
> pass. **Where both write, the graph wins.**

and the bone-space axis (§3.3). Both live inside the asset. Nothing further can be settled from
C++ source, which is why this is an audit and not another patch.

## Q1 — which of the ABP's own functions write the aim surface?

On **`ABP_Mannequin_Base`**, list every function called from `BlueprintThreadSafeUpdateAnimation`,
and for each one report whether it writes any of:

`AimPitch` · `AimYaw` · `Pitch` · `PitchRotator` · `bFPSMode` · `LeanRotation` · `LeanOppRotation`

Report as a table: function name → which of those it writes (or "none"). **This is the whole
question.** If any function writes one of those seven, the graph is overwriting C++ every frame and
the aim fix is to stop that function running — a decision, not your edit.

## Q2 — what do the aim nodes actually READ?

In the AnimGraph, find the Transform(Modify)Bone nodes that bend the spine for aim. For each,
report the exact binding on its Rotation pin — the property name AND which object it resolves
against (`this`, or `GetMainAnimBPThreadSafe`, or a layer-local variable).

This settles the question no source read can: whether the pose consumes the main ABP's copy or the
layer's own copy of these values.

## Q3 — the axis defaults

Report the Class Defaults values of `AimPitchAxis` and `LeanAxis` on `ABP_Mannequin_Base`
(`Roll` / `Pitch` / `Yaw`). These are C++ properties with editor defaults; the founder's playtest
described lean as "a little up and down", which is the signature of aim and lean sharing one axis.

## Q4 — one live reading, if PIE is reachable

Possess a BN pawn, look up and down, and report — from the LIVE instance, not the asset:

| Read | Value |
|---|---|
| `AimPitch` on the main ABP | |
| `Pitch` on the main ABP | |
| `PitchRotator` on the main ABP | |
| `bFPSMode` on the main ABP | |
| the same four on the linked weapon layer instance | |

If the main ABP's values move with the camera and the layer's do not, the break is delivery. If
both move and the body still does not, the break is the node bindings from Q2. If the main ABP's
own values do not move at all, the graph is overwriting them and Q1 names the culprit.

## Done means

Q1's table, Q2's binding list, Q3's two values, and Q4's readings if PIE was reachable — pasted
into the Log below. **Nothing changed.** Anything you were tempted to fix goes in the Log as a
sentence, and the lead decides.

## Log

_(terminal: the four answers, and anything handed back)_
