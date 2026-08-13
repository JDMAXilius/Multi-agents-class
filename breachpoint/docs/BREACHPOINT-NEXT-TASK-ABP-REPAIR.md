# TASK — the BN-owned anim layer chain, through the Unreal MCP

**Cut:** 13 August 2026 by the cloud lead · **For:** the terminal session (editor + MCP in reach)
**Binds to:** the NEXT doc family only. **Owner path:** `Content/BN/Animation/` + `Config/DefaultGame.ini`.
**Supersedes** this file's 13 Aug `GroundDistance_0` task — the founder fixed that by hand.

**Founder's instruction:** do this through the Unreal MCP **if it is quick**. It is four small
steps and the founder can do them in ~2 minutes by hand, so **the MCP has to beat that or get
out of the way**. If any step fights back, stop and hand the remaining steps back as clicks —
do not burn twenty minutes automating two.

## The symptom this fixes

The founder's character walks with the **lower body only; the upper body is frozen**, and the log
spams `Accessed None trying to read (real) property CallFunc_GetMainAnimBPThreadSafe_ReturnValue`
from `ABP_ItemAnimLayersBase`.

The FPS graph splits at the spine: lower body from the main ABP's locomotion (working — it is fed
by the 27 ported C++ properties on `UBNAnimInstance`), upper body from the **linked layer**, which
picks its pose by reading the main ABP. `GetMainAnimBPThreadSafe` casts the owning anim instance
to `ABP_Mannequin_Base`. BN's main is `ABP_BNMannequin` — a **duplicate**, therefore a *sibling*
of that class, not a child — so the cast returns None, the layer selects no pose, and the upper
body falls to reference pose.

Evidence for the mechanism, from the founder's own working reference, `MyCharacter.cpp:1134`:
*"`ABP_Mannequin_Base` stores each into a variable; `ABP_ItemAnimLayersBase` and its per-weapon
children read those to choose the pose — `Sprinting` is what selects the `fPS_Sprint` pose slot."*

## The four steps

Everything happens on **BN copies**. `ABP_Mannequin_Base`, `ABP_ItemAnimLayersBase` and
`ABP_UnarmedAnimLayers` under `FPSTemplate/` are **read-only** — that ruling is what protects the
founder's working `BP_FPSCharacter` setup, and it is not up for renegotiation here.

1. Duplicate `ABP_ItemAnimLayersBase` → `/Game/BN/Animation/ABP_BNItemAnimLayersBase`.
2. Duplicate `ABP_UnarmedAnimLayers` → `/Game/BN/Animation/ABP_BNUnarmedAnimLayers`, then reparent
   it to `ABP_BNItemAnimLayersBase`.
3. In `ABP_BNItemAnimLayersBase` → `GetMainAnimBPThreadSafe`: retarget the cast to
   **`ABP_BNMannequin`** (`/Game/BN/Animation/ABP_BNMannequin.ABP_BNMannequin_C`).
4. `Config/DefaultGame.ini` → `UnarmedAnimLayer=/Game/BN/Animation/ABP_BNUnarmedAnimLayers.ABP_BNUnarmedAnimLayers_C`
   (line 232 today still points at the FPSTemplate layer).

`Tools/bn/45_bn_anim_layers.py` already implements all four with a 14-row read-back audit and is
idempotent — running it is likely faster than driving the MCP call-by-call. Its cast target
constant is already `ABP_BNMannequin`.

## Why the cast targets the BP class and NOT the C++ class

This was corrected once already; do not "improve" it back. The layer reads pose-selection
variables — `Sprinting`, `Unarmed`, the ADS bools — that live as **Blueprint variables** on
`ABP_BNMannequin` and were never ported to C++. `ABP_BNMannequin` **is-a** `UBNAnimInstance`, so
casting to the BP class resolves the 27 ported C++ properties *and* those BP variables: a superset.
Casting to `/Script/BreachpointNext.BNAnimInstance` would break every BP-only read. The cast moves
down to the C++ class only once C++ carries everything the layer reads.

## What will not be scriptable, and is not a failure

- **A cast node's pins are typed at construction**, and `UEdGraphPin` has not been a `UObject`
  since 4.15 — so a successful `TargetType` write can still leave a stale `As ABP Mannequin Base`
  output pin. **Delete and recreate the node** rather than editing it. The BN base's compile
  status is the only proof; treat it as the gate.
- **The function's return-value pin type** lives in `FUserPinInfo` — unreflected both ways. Eyeball
  it even when everything else reports clean.
- **Local variables typed to `ABP_Mannequin_Base`** can be read and flagged but not retyped.

## Done means

`ABP_BNItemAnimLayersBase` and `ABP_BNUnarmedAnimLayers` exist under `/Game/BN/Animation/`, the BN
unarmed layer's parent is the BN base, the cast reads `ABP_BNMannequin`, the ini points at the BN
copy, both layer assets compile clean, and the audit is pasted into the Log below.

Then the founder's checkpoint run is unblocked: **upper body moving with the lower**, look-down-
see-body, crouch toggle, crouch-jump — standalone, then listen-server + client.

## Standing rule that governs this task

Say up front when the founder doing it by hand is faster (Roadmap 1, operating rules). For this
task that judgement has already been made and it is finely balanced: four clicks versus an MCP
session. **Automate only what is genuinely quicker; hand back the rest as steps.**

## Roadmap 2 note, so it is not re-learned

Per-weapon layers parent to `ABP_BNItemAnimLayersBase`, **never** the template base — a weapon
layer on the template base inherits the template cast and reintroduces this exact Accessed-None
bug, one weapon at a time.

## Log

_(terminal session: append what the MCP did, what was handed back as manual, and the compile/audit
read-back here)_
