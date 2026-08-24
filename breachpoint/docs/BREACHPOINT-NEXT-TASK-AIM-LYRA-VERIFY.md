# TICKET — does the Lyra aim path actually aim? (measure, change nothing)

> STATUS: in-progress — mac terminal 24 Aug 2026 (9945b16). Editor live pid 2127, MCP on :8000.

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

