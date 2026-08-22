# TICKET — does the Lyra aim path actually aim? (measure, change nothing)

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
