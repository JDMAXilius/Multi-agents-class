---
name: cmc-prediction
description: Character Movement Component prediction patterns for BREACHPOINT — extending CMC without rewriting it, FSavedMove_BR compressed flags, root-motion sources for the Grappleshot, and what "rejection leaves zero state" costs to build. Load for BP06 (grapple), BP02's BRGA_Sprint, and anything adding movement state that must survive a server correction. The contracts (netcode.md, gas-purity.md) are LAW; this skill is HOW the predicted movement is built.
---

# CMC Prediction — extending the engine's machinery, not replacing it

> ## ⚠️ UNVERIFIED DRAFT — correct it in the packet that first uses it
> **This skill has never been run against a build.** It was written from UE documentation and
> this project's architecture, not from a grapple that predicted and reconciled correctly on
> two machines. The *doctrine* (§1, §5, §6, §7) follows from `netcode.md` and the GDD and is
> sound. The **API specifics in §2–§4 are the risk**: the compressed-flag path and the newer
> move-data path coexist in UE 5.x (§3), and which one 5.8 wants is exactly the question this
> file cannot answer without an engine.
>
> **BP02's `BRGA_Sprint` is the first real exercise** (it proves the input→tag→ASC→CMC chain);
> **BP06 is the hard one.** Whichever lands first owns correcting this file in the same packet,
> with the delta in the ticket Log. When this skill and a working build disagree, the build is
> right.

**Authority note:** `netcode.md` (prediction reconciles; rejection may not fork gameplay
state) and `gas-purity.md` (movement is a *named exception* — GAS decides, CMC moves) are law.
This skill only shows how the lawful version is built.

## 1. Why we extend CMC instead of writing prediction

`ARCHITECTURE §3.4` already rules on this: rewriting CMC means rewriting the most
battle-tested networked prediction code in the engine — saved moves, corrections, smoothing.
That is engine-tier work, like the renderer, and "100% our gameplay code" does not extend to
re-implementing engine subsystems. Epic's replacement (**Mover**) is experimental and rejected
on the same grounds as MassAI.

The payoff is concrete: anything carried in a saved move gets client prediction, server
correction, and replay **for free**. Anything carried outside one does not, and that is the
difference between a grapple that feels instant and one that rubber-bands.

## 2. The four pieces, and what each is for

| Piece | Job |
|---|---|
| `FSavedMove_BR : FSavedMove_Character` | one frame of intent, replayable |
| `FNetworkPredictionData_Client_BR` | the factory that hands out `FSavedMove_BR` |
| `UBRCharacterMovementComponent` | reads the flags back and applies motion |
| Root motion sources | the grapple pull itself |

Per `ARCHITECTURE §3.4` the first two live in the same files as the component.

**The saved move must implement all four hooks, and skipping any one is a bug that only shows
under packet loss:**

- `Clear()` — reset your fields. **Forgetting this is the classic bug**: moves are pooled and
  reused, so a stale flag from three moves ago replays as a phantom input.
- `SetMoveFor()` — capture current intent from the component into the move.
- `PrepMoveFor()` — push the move's state back into the component before replay.
- `CanCombineWith()` — return **false** when your flags differ from the other move. Combining
  moves that disagree is how a sprint frame silently disappears on correction.
- `GetCompressedFlags()` — pack your booleans into the custom bits.

## 3. Compressed flags — and the constraint nobody mentions until it bites

`FSavedMove_Character` exposes **only four custom bits** (`FLAG_Custom_0` … `FLAG_Custom_3`).
Breachpoint spends two immediately — sprint (BP02) and grapple (BP06) — leaving two for the
whole rest of the project.

```cpp
uint8 FSavedMove_BR::GetCompressedFlags() const
{
    uint8 Result = Super::GetCompressedFlags();
    if (bWantsToSprint)  Result |= FLAG_Custom_0;
    if (bWantsToGrapple) Result |= FLAG_Custom_1;
    return Result;
}

void UBRCharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
    Super::UpdateFromCompressedFlags(Flags);
    bWantsToSprint  = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
    bWantsToGrapple = (Flags & FSavedMove_Character::FLAG_Custom_1) != 0;
}
```

**Budget the bits deliberately** — a third and fourth movement state will want them, and
running out means moving to the richer path below, mid-project.

> ⚠️ **The open question this skill cannot settle.** UE 5.x also offers a structured
> move-data path (`FCharacterNetworkMoveData` / `FCharacterMoveResponseDataContainer`) that
> replicates typed fields instead of four bits. It is the better long-term answer and the
> likely direction in 5.8. Compressed flags are simpler and well-documented. **BP02's packet
> should decide which, and record the decision as a ruling** — switching later touches every
> movement state at once.

## 4. The Grappleshot — root motion through the CMC

The pull is **a root motion source**, not a manual position write. That is the whole design:
an RMS rides the saved-move pipeline, so it predicts and reconciles through the same machinery
as walking, with no bespoke reconciliation code to get wrong.

The division of labour, straight out of `gas-purity.md`'s movement exception:

```
BRGA_Grapple (ability)          →  decides WHETHER: trace, classify surface,
                                    server-validate target (range/LOS/rate),
                                    commit cooldown via GE_Cooldown
UBRCharacterMovementComponent   →  executes MOTION: applies the RMS, owns detach
                                    rules (arrival radius, jump-cancel, timeout)
```

The ability never moves the character; the component never decides whether to grapple. When
in doubt about where a line of code goes, ask which of those two sentences it belongs to.

Apply the source on **both** the predicting client and the server — client-only means no
authority, server-only means no prediction and a visible snap. Keep the returned handle so
detach can remove exactly that source rather than clearing all of them (a blanket clear will
also kill an unrelated knockback later).

## 5. "Rejection leaves zero state" — the acceptance criterion, and what it costs

BP06's Done-when: a rejected grapple leaves **no position fork, no cooldown consumed, no cue
residue**. Each of those is a different mechanism, and they are easy to get individually right
and collectively wrong:

| Residue | What prevents it |
|---|---|
| Position | the RMS was never applied server-side, and the client's replay drops it on correction |
| Cooldown | the cooldown is a **predicted GE** — GAS rolls it back automatically on rejection, *but only if it went through the GE path* (`gas-purity.md` law 4). A hand-tracked float never rolls back. |
| Cue | rope/impact are `OnActive` cues inside the predicted window, which GAS removes on rollback. A cue spawned from ability body code stays on screen forever. |

**This is the strongest argument for purity in the whole project**: three different rollback
behaviours come free from three different lawful choices. Break any one and you hand-build its
correction path — badly, under time pressure, in week 3.

## 6. Testing — where prediction bugs actually live

**Single-process PIE cannot see prediction bugs.** Prediction and correction only exist when
client and server are separate processes; in one process the "client" reads server state
directly and everything looks perfect.

The floor for any claim here:

1. **Multi-process PIE** — *Run Under One Process* OFF, Net Mode = Play As Client, 2+ players.
   Report it as "editor multi-process", never as the Gauntlet rung (`game-lead` honesty laws).
2. **Rung 4 under emulation** — `-PktLag=120 -PktLoss=5`. Without loss you are not testing
   correction, you are testing the happy path with extra steps.
3. **A forced correction** — BP02's Done-when already requires sprint to replay correctly
   through one. Provoke it deliberately rather than waiting to observe one.

Watch the **simulated proxy** view specifically: the observing client is where smoothing and
replication artifacts show, and it is the view most often skipped.

## 7. Self-check before handoff

`Clear()` resets every custom field · `CanCombineWith()` returns false on differing flags ·
flags packed and unpacked symmetrically, bit budget recorded · RMS applied on client *and*
server, handle retained for targeted removal · the ability decides, the component moves — no
position writes in ability code · cooldown is a GE, cues are cues (both roll back for free) ·
tested multi-process at minimum, rung 4 + emulation for the landing claim · every claim names
its rung, and "works in PIE" is not one of them.
