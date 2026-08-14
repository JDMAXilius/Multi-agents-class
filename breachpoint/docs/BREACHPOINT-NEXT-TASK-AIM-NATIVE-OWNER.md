# TICKET — native becomes the default aim owner: ONE checkbox, nothing else

**Cut:** 14 August 2026 by the cloud lead · **For:** the terminal session (editor + Unreal MCP)
**Founder ruling:** "Do all" on the reference-research recommendations
([RESEARCH-AIM-REFERENCES](BREACHPOINT-NEXT-RESEARCH-AIM-REFERENCES.md)) — the
components-own-aim configuration matches none of the six reference projects; native owns the
aim surface by default.
**Read [`ASSET-RULES`](BREACHPOINT-NEXT-ASSET-RULES.md) §5 first.**

## THE SCOPE, stated before the work because it has been a problem

This ticket changes **exactly one property default on exactly one asset** and executes
**one other already-written ticket**. It does not remove the procedural components, does not
touch any layer asset, does not touch `Source/`, does not touch any other Blueprint, does not
"improve" anything it notices. Extra work found is a **Log entry, not a licence**.

## Step 1 — the one checkbox

On the **same shared ABP whose Class Defaults you set `bNativeOwnsAimSurface = false` in your
14 Aug pass** (your own log entry in
[TASK-R3-W3 §Log](BREACHPOINT-NEXT-TASK-R3-W3-MELEE-GRENADE.md)): set
**`bNativeOwnsAimSurface` back to `true`** (equivalently: clear the override so the C++
default, which is true, stands). Compile, save — mind the stale-registry save quirk your own
log documents (`load_asset` first, or Ctrl+Shift+S).

Why: the C++ native path now delivers the aim surface to the linked LAYER instances every
frame (the missing half found 14 Aug), and the reference research is six-for-six that pose
data is pulled from the pawn's own state — the component/interface-event path owns nothing by
default anymore. The components STAY on BP_BNCharacter for what they are uniquely good at
(sway, recoil, pose offsets); they just no longer own Pitch/PitchRotator/bFPSMode.

## Step 2 — execute the shotgun/knife rows ticket

[TASK-DT-SHOTGUN-KNIFE](BREACHPOINT-NEXT-TASK-DT-SHOTGUN-KNIFE.md), exactly as written, no
substitutions. It is the only blocker for the founder's "all four weapons in the swap" and
its Log already says so.

## Step 3 — read back

1. Reload the ABP fresh; print `bNativeOwnsAimSurface` from its Class Defaults — must be `true`.
2. If PIE is available: possess, run `BNAimDebug`, paste the output. The first line must read
   `owner NATIVE`, and each `BNAim layer` line should show a live `PitchRotator` while the
   camera pitches. If any layer line stays zeroed while the main line moves, paste it and STOP
   — that is a finding for the lead, not something to fix here.
3. The DT ticket's own read-back table, per its spec.

## Done means

Checkbox true and read back, both DT rows landed cell-perfect, read-backs pasted below.
**That is the whole ticket.**

## Log

_(terminal: read-backs, and anything handed back)_
