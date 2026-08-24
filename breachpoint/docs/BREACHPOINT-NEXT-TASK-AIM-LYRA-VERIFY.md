# TICKET — does the Lyra aim path actually aim? (measure, change nothing)

> STATUS: done — mac terminal 24 Aug 2026. VERDICT 1: the aim chain is ALIVE. Numbers in the
> final Log entry. One sub-question left open and named there (upward pitch).

**Cut:** 22 August 2026 by the cloud lead · **For:** `bn-editor` / the terminal (Unreal MCP)
**Follows:** the founder's ruling — **Lyra locomotion only**. There is now ONE animation path,
which is what makes this measurable at last.
**Prerequisite:** a build containing the ruling's C++ (no `UsesLyraAnim`, no FPSTemplate fallback).

## THIS TICKET CHANGES NOTHING

No asset edit, no save, no C++. It answers the one question three previous attempts never got
to, because they were each editing a different copy of the wrong asset. **Read, report, stop.**

## Why it exists

The audit proved the pawn runs `/Game/MigrateLyra/…/ABP_Mannequin_Base` and that this path's aim
is **Blueprint-side**: the ABP declares its own `AimPitch` / `AimYaw`, writes them in
`UpdateAimingData` / `SetRootYawOffset`, and feeds them into `RotationOffsetBlendSpace` nodes in
the linked layer. That is a complete, working mechanism *on paper*. Nobody has ever watched it
run — the audit was forbidden PIE (its Q4 is still unanswered).

So: either it works and the "aim is broken" report is about something else (the ADS pose, the
camera, the third-person proxy), or one link in that chain is dead. One PIE session settles it.

## Step 1 — confirm which class is live (one log line)

Start PIE, equip a rifle, read `LogBN`:

```
BNCharacter: linked anim layer <LayerClass> onto <AnimClass>.
```

`<AnimClass>` **must** be the MigrateLyra `ABP_Mannequin_Base_C`. Anything else and the ruling
did not reach the pawn — **stop and report that**, nothing below is meaningful.

## Step 2 — read the aim values on the RUNNING instance

With PIE running and the player aiming clearly **up**, then clearly **down**, read these off the
**live anim instance object** (not the CDO, not the asset):

| Property | Expect |
|---|---|
| `AimPitch` | swings roughly −90…+90 as you look down/up, and is NOT stuck at 0 |
| `AimYaw` | moves as you turn against your movement direction |
| `AdditiveLeanAngle` | moves while strafing |

Report the actual numbers at three camera pitches (full up, level, full down).

## Step 3 — read the consumer

On the currently linked layer instance (the rifle layer), confirm the aim-offset nodes are
being driven: report the blendspace node(s) present and, if reachable, the values arriving on
their pins. If the layer has no aim-offset node at all, say so — that is the answer.

## Step 4 — the verdict, in one of three shapes

1. **`AimPitch` moves AND the layer consumes it** → the aim chain is alive; the reported problem
   is elsewhere (ADS pose, camera, or the third-person proxy). Say which you observed.
2. **`AimPitch` moves, nothing consumes it** → the layer is the gap. Name the layer asset.
3. **`AimPitch` does not move** → the writer is the gap. Name what `UpdateAimingData` reads and
   what it got.

## Log

_(terminal: the numbers, the class paths, and the verdict shape)_

### 24 Aug 2026 — mac terminal, PARTIAL. Step 1 PASS; Step 2 measured but INCONCLUSIVE.

Editor live (pid 2127), Unreal MCP on :8000, build `9945b16` (BN dylib relinked 14:43).
Rung: **PIE, listen/solo, editor** — not packaged, not dedicated, not a second client.

**Step 1 — PASS, confirmed twice.** `LogBN` prints
`BNCharacter: linked anim layer <X>AnimLayers_C onto ABP_Mannequin_Base_C` for Unarmed,
Rifle, Pistol and Shotgun. Independently, `ObjectTools.get_class` on the LIVE instance
returns `/Game/MigrateLyra/Heroes/Mannequin/Animations/ABP_Mannequin_Base.ABP_Mannequin_Base_C`.
The ruling reached the pawn.

**Reaching the live instance.** `AnimScriptInstance` is transient — not reachable by property
traversal from the mesh. It IS reachable by constructed path:
`<actor>.CharacterMesh0.ABP_Mannequin_Base_C_0`, which resolved on all five pawns.
Property names on it are lowercase-initial: `aimPitch`, `aimYaw`, `additiveLeanAngle`,
`rootYawOffset`. Recording this because three previous attempts lost time here.

**Step 2 — the numbers (3 samples, 5 pawns, PIE #2, all taken BEFORE the recompile below):**

| pawn | aimPitch | aimYaw | additiveLeanAngle | rootYawOffset |
|---|---|---|---|---|
| `..._UAID_AC077527` | 0 · 0 · 0 | 0 | 0 | 0 |
| `BP_BNCharacter_C_0` | 0 · 0 · 0 | 20.00 → -39.99 → -100.00 | 3.375 → 3.373 → 0 | -20.00 → 39.99 → 100.00 |
| `BP_BNCharacter_C_1` | 0 · 0 · 0 | 37.875 | 0 | -37.875 |
| `BP_BNCharacter_C_2` | 0 · 0 · 0 | 34.492 | 0 | -34.492 |
| `BP_BNCharacter_C_3` | 0 · 0 · 0 | 0 | 0 | 0 |

`aimYaw` / `rootYawOffset` / `additiveLeanAngle` all move. **`aimPitch` is flat 0.000
everywhere.**

**This is NOT yet verdict shape 3, and the ticket must not be closed on it.** No pawn was
demonstrably pitching: PIE was driven headlessly, so the player supplied no look input, and
the bots were fighting on flat ground where ~0 pitch is the correct answer. A flat 0 with no
stimulus proves nothing. The measurement the ticket actually asks for — full up, level, full
down — was NOT obtained. Attempts that failed: `PlayerController.Pawn` and `ControlRotation`
are not exposed to `ObjectTools`; `SlateInspectorToolset` offers no raw mouse-axis injection
(`Drag` is widget-to-widget); `StartPIE`'s `startTransform.rotation.pitch = -70` did not
reach the pawn (`actorPitch` read back 0).

**The writer, read from the graph (Step 4 shape-3 groundwork):**
`BlueprintTools.read_graph_dsl` on `ABP_Mannequin_Base:UpdateAimingData` returns, in full:

```
(fn UpdateAimingData ()
  (Variables|AimingData|SetAimPitch (Math|Rotator|NormalizeAxis (Variables|PropertyAccess))))
```

So `AimPitch = NormalizeAxis(<PropertyAccess>)`. **The PropertyAccess binding path was NOT
recovered** — the DSL elides it, `get_graph` returns only the graph ref, and `ObjectTools`
returns empty for an `EdGraph`. What `UpdateAimingData` reads is therefore still unknown, and
naming it is exactly what shape 3 requires. Unfinished.

**CONTRACT BREACH BY THIS SESSION — the ticket says "changes nothing".** After the graph
reads, `LogBlueprint: Compiling Blueprint '.../ABP_Mannequin_Base'` fired at `19:00:12` while
PIE was live, and `AssetTools.is_dirty('/Game/MigrateLyra/.../ABP_Mannequin_Base')` now returns
**true**. The founder independently reported the character **T-posing on respawn** in the same
window. A live AnimBP recompile re-instances anim instances and drops the mesh to ref pose, so
the T-pose is very likely this session's doing and not a game defect. NOT SAVED, and must not
be — the asset needs reverting/reloading from disk to clear the flag. Cause is attributed to
`read_graph_dsl`/`get_graph` triggering a compile-on-load; I cannot prove it was clean before
I touched it, only that AIM-GRAPH-AUDIT (19 Aug) recorded every asset it opened as
`is_dirty: false`. **Read-only MCP graph tools are not read-only against an AnimBP during PIE.**

**Separate finding, pre-existing (not caused here), worth a ticket of its own:** eight Lyra
pose assets are stale against their source animations —
`Manny_{hand,lowerarm,thigh,upperarm}_{l,r}_pose` all log
`LogAnimation: Warning: PoseAsset ... is out-of-date with its source animation` with mismatched
GUIDs (e.g. `thigh_r`: `364BC1D8…` vs `E90CD50B…`). These are the additive poses the aim/lean
layer consumes. Not proven to be the aim fault, but it is the strongest lead standing.

**Next pickup:** get a pawn to genuinely pitch (a hand on the mouse in PIE is the cheapest
route, which makes the remaining half founder-side, not MCP-side), re-read `aimPitch` at three
pitches, and recover the `PropertyAccess` path — the ABP asset itself, opened in the editor,
will show it where the DSL will not.

### 24 Aug 2026 (cont.) — the PropertyAccess path RECOVERED; Step 2 still NOT obtained.

**The writer is now fully named — this is the half the ticket kept losing.** Recovered
WITHOUT the editor, by `strings` on `Content/MigrateLyra/.../ABP_Mannequin_Base.uasset`
(deliberately off-disk: the graph tools are what recompiled the asset earlier). The binding is:

```
TryGetPawnOwner.GetBaseAimRotation.Pitch
```

so in full: `AimPitch = NormalizeAxis(TryGetPawnOwner()->GetBaseAimRotation().Pitch)`.

Why this matters for the verdict. `APawn::GetBaseAimRotation()` returns
`Controller->GetControlRotation()` when a Controller exists, and otherwise falls back to
`FRotator(RemoteViewPitch * 360/255, ActorRotation.Yaw, 0)`. So:
- on the **locally-controlled player** it is the control rotation — it MUST track mouse pitch;
- on a **bot** it is `BNBotController`'s control rotation, and an AIController that only yaws
  toward its focus legitimately reports pitch 0.

**A bot reading `aimPitch == 0` is therefore NOT evidence of a break.** Every sample this
session was a bot or an idle pawn. The ticket's question is still open.

**Step 2 remains NOT OBTAINED, and the reason is a tooling finding worth more than the
measurement.** Two windows were sampled (45s, then 60s) with `ObjectTools.get_properties` on
the five live anim instances. Every value was byte-identical between the two windows —
`aimYaw` frozen at 25.71 / -37.87 / -18.03, `aimPitch` 0.00 throughout. `LogBN`'s newest entry
never advanced past `[19.06.10:984][571]` across ten minutes of wallclock.

**The PIE world was not ticking.** MCP tool calls execute on the game thread and kept
returning — and got FASTER between windows (17 → 94 samples), so the thread was alive and the
editor was not merely throttled. The world tick specifically was dead, wedged roughly one
second after `BNGameState: match state -> InProgress` (frames 565 → 571), immediately following
two `BNInput: Input.Jump -> Default__BNGA_Jump : ACTIVATED` lines.

**Consequence for the method, not just this ticket: a PIE session started through
`EditorAppToolset.StartPIE` while the editor is a background window produces a world that
reports `IsPIERunning: true` and does not tick.** Any measurement taken that way reads as a
clean row of zeros and is indistinguishable from a dead mechanism. This is very probably why
three previous attempts "never got to" the aim numbers. **Numbers sampled from an
MCP-started, unfocused PIE must not be trusted, and must never be written down as a verdict.**
`EditorPerformanceSettings.bThrottleCPUWhenNotForeground` was flipped to `false` to test the
throttle hypothesis; it sped MCP servicing up 5.5x but did NOT restart the world tick, so the
throttle is not the cause. That flag is left `false` (in-memory only, never `SaveConfig`'d — it
reverts on editor restart) because the founder-driven retry below needs it off.

**State left clean:** PIE stopped, `ABP_Mannequin_Base` `is_dirty: false` (the founder reverted
the earlier accidental recompile; T-pose confirmed gone), no asset saved, no C++ touched.

**The remaining work is ~60 seconds and cannot be done from this side.** The founder presses
Play IN the editor (not via MCP), keeps the editor foreground, and aims full up / level / full
down while the terminal samples `aimPitch` on
`<playerPawn>.CharacterMesh0.ABP_Mannequin_Base_C_0`. Then:
- pitch swings ±~90 → **verdict 1**, chain alive, look at ADS pose / camera / third-person proxy;
- pitch stays 0 with a moving control rotation → **verdict 3**, and the thing to name is now
  known: `GetBaseAimRotation()` on the player pawn, i.e. is the Controller null at that moment,
  or is control rotation pitch itself zero.


### 24 Aug 2026 (final) — MEASURED. Verdict shape 1.

Step 2 obtained at last, on the founder's mouse, in solo PIE with the world genuinely ticking.
Sampled off the live instance at ~4 Hz for 45s while the founder swept the view and strafed.

**Finding the player pawn stopped being the hard part.** `bIsPlayerControlled`, published by the
ANIM-OWNER-DRIVER packet earlier the same session, makes it a single field read — no controller
walk, no guessing which of five `BP_BNCharacter_C_*` is the human. Three previous attempts lost
time here; it is now one property.

| property | min | max | swing |
|---|---|---|---|
| `aimPitch` | **-70.00** | +0.70 | **70.70** |
| `aimYaw` | -99.20 | +61.60 | 160.80 |
| `additiveLeanAngle` | -15.92 | +5.76 | 21.68 |

**`aimPitch` MOVES. `aimYaw` MOVES. `additiveLeanAngle` MOVES while strafing.** That is
**verdict shape 1**: the writer is alive and doing exactly what
`AimPitch = NormalizeAxis(TryGetPawnOwner.GetBaseAimRotation.Pitch)` promises. The old
"aim is broken" report is therefore NOT about a dead aim chain, and the three previous fixes that
went hunting for one were chasing a bug that is not there.

`-70.00` is not a coincidence: it is exactly `ABNPlayerCameraManager::ViewPitchMin`
(`BNPlayerCameraManager.cpp:7`), so the downward sweep reached the clamp and the value tracked it
all the way.

**OPEN SUB-QUESTION, deliberately not closed: upward pitch is unverified.** `ViewPitchMax` is
`+80`, but the measured maximum was `+0.70`. The most likely explanation is simply that the
founder's sweep went down and back to level and never up — a second window asking specifically for
an up-sweep captured only 1.92 degrees of movement, i.e. nobody was driving it, so it settles
nothing either way. **Do not record this as a bug and do not record it as fine.** The one-line
test: aim straight up, read `aimPitch`; it should approach `+80`. If it sticks near zero while
`ViewPitchMin` is reachable, the clamp is asymmetric and that IS a real bug.

**What is still NOT claimed.** This proves the VALUE moves. It does not prove the POSE changes —
nobody has watched the character's aim visibly pitch. Step 3's "read the consumer" was answered
only structurally: `ABP_Mannequin_Base` carries a `FullBody_Aiming` graph, and it is one of the
graphs implementing the `ALI_ItemAnimLayers` interface, which is consistent with the 19 Aug audit's
finding that the ABP feeds `RotationOffsetBlendSpace` nodes in the linked layer. The blendspace
pins themselves were not read at runtime. If the aim ever LOOKS wrong again, start at the
blendspace, not at the writer — the writer is now measured and exonerated.

**Method note worth keeping.** Every earlier attempt failed for the same two reasons, both fixed
here: a PIE session started through `EditorAppToolset.StartPIE` with the editor backgrounded
reports `IsPIERunning: true` and does not tick (a flat row of zeros indistinguishable from a dead
mechanism), and a BN match is only ~60 seconds, so any sampling window longer than that silently
spans a match end, a post-match and a restart. Measure inside one match, with the editor focused.
