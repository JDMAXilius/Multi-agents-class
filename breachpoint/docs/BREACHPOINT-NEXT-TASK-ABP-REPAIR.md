# TASK — the BN anim layer chain · CLOSED, by a different route than planned

**Cut:** 13 August 2026 by the cloud lead · **Closed:** 13 August 2026, terminal session
**Binds to:** the NEXT doc family only. **Owner path:** `Content/BN/Animation/` +
`Config/DefaultGame.ini` (as planned) — **what actually shipped wrote `Content/FPSTemplate/`.**

> **STATUS: done — terminal 13 Aug 2026 (`ae8d38e`). Read "What actually shipped" before
> anything else: the four steps below were NOT executed and must not be executed now.**
> Supersedes this file's 13 Aug `GroundDistance_0` task — the founder fixed that by hand.

## What actually shipped

The founder reparented **`ABP_Mannequin_Base` itself** — the FPSTemplate original — onto
`/Script/BreachpointNext.BNAnimInstance`. No BN duplicates were made. No cast was retargeted.
`Config/DefaultGame.ini` was not touched.

It works because the cast was never the thing to fix. `GetMainAnimBPThreadSafe` casts to
`ABP_Mannequin_Base`, and `ABP_Mannequin_Base` now **is-a** `UBNAnimInstance` — so the cast
resolves, the layer selects poses again, and everything already wired to the template main ABP
keeps working with no rewiring at all. In the founder's words: *everything else is connecting
to that.*

Confirmed from the bytes rather than the account: `ABP_Mannequin_Base` carries 158
`BNAnimInstance` references and `ABP_ItemAnimLayersBase` 57. The main ABP's −18 KB is its
Blueprint variables dropping out to re-resolve against the C++ properties — the same
substitution `30_reparent_abp.py` performs on a duplicate, applied to the original instead.
`ABP_ItemAnimLayersBase` (+23.7 KB) and `ABP_ItemAnimLayersBase_UE4` (+1.4 KB) moved as
dependents.

**Verified rung: founder-reported, editor standalone.** Upper body moving with the lower, and
crouch working. NOT PIE-multiplayer, NOT listen+client, not verified by anyone but the founder.
The three-way multiplayer claim is still owed.

### What this route costs, recorded so it is not rediscovered

- **`FPSTemplate/` is no longer read-only in practice.** That ruling existed to protect the
  founder's working `BP_FPSCharacter` setup. `BP_FPSCharacter` now inherits `UBNAnimInstance`
  through the shared main ABP. It was not separately exercised. **The ruling and this reality
  need reconciling** — that is a live question, not a settled one.
- **`Config/DefaultGame.ini:232` stays pointed at the FPSTemplate layer,** and under this route
  that is correct, not an oversight.
- **`Content/BN/Animation/ABP_BNMannequin.uasset` is now unused by this path.** Untouched since
  `8742c01`. Decide whether it is deleted or kept before it rots into a second source of truth.
- **`Tools/bn/45_bn_anim_layers.py` is unnecessary for this route** and was never run. It was
  repaired at `ecf7589` anyway (its cast target silently inherited `30_reparent_abp.py`'s
  `new_parent`, overriding the BP-class correction and landing as a false green) — so if the
  duplicate route is ever revived, the script is correct and unrun rather than broken.

## The four steps — NOT EXECUTED, kept only as the road not taken

Do not run these. They describe BN duplicates that do not exist and a cast retarget that was
never needed. They are preserved because the reasoning below them is still the best written
account of the bug, and because reviving them is a real option if sharing the main ABP with
`BP_FPSCharacter` turns out to be a problem.

1. Duplicate `ABP_ItemAnimLayersBase` → `/Game/BN/Animation/ABP_BNItemAnimLayersBase`.
2. Duplicate `ABP_UnarmedAnimLayers` → `/Game/BN/Animation/ABP_BNUnarmedAnimLayers`, then reparent
   it to `ABP_BNItemAnimLayersBase`.
3. In `ABP_BNItemAnimLayersBase` → `GetMainAnimBPThreadSafe`: retarget the cast to
   **`ABP_BNMannequin`** (`/Game/BN/Animation/ABP_BNMannequin.ABP_BNMannequin_C`).
4. `Config/DefaultGame.ini` → `UnarmedAnimLayer=/Game/BN/Animation/ABP_BNUnarmedAnimLayers.ABP_BNUnarmedAnimLayers_C`.

## The symptom this fixed

The founder's character walked with the **lower body only; the upper body frozen**, and the log
spammed `Accessed None trying to read (real) property CallFunc_GetMainAnimBPThreadSafe_ReturnValue`
from `ABP_ItemAnimLayersBase`.

The FPS graph splits at the spine: lower body from the main ABP's locomotion (working — it is fed
by the 27 ported C++ properties on `UBNAnimInstance`), upper body from the **linked layer**, which
picks its pose by reading the main ABP. `GetMainAnimBPThreadSafe` casts the owning anim instance
to `ABP_Mannequin_Base`. Under the duplicate plan, BN's main was `ABP_BNMannequin` — a *sibling*
of that class, not a child — so the cast returned None. **The shipped fix removes the sibling
relationship instead of retargeting the cast.**

Evidence for the mechanism, from the founder's own working reference, `MyCharacter.cpp:1134`:
*"`ABP_Mannequin_Base` stores each into a variable; `ABP_ItemAnimLayersBase` and its per-weapon
children read those to choose the pose — `Sprinting` is what selects the `fPS_Sprint` pose slot."*

## Why a cast to the BP class, if one is ever added again

Under the duplicate route the cast had to target `ABP_BNMannequin`, the **Blueprint** class, not
`/Script/BreachpointNext.BNAnimInstance`. This was corrected once (`a3efbf8`) and then silently
un-corrected by the script's config inheritance (`ecf7589`) — it has now bitten twice, so it is
written down twice. The layer reads pose-selection variables — `Sprinting`, `Unarmed`, the ADS
bools — that live as **Blueprint variables** and were never ported to C++. The BP class is-a
`UBNAnimInstance`, so casting to it resolves the ported C++ properties *and* those BP variables: a
superset. Casting to the C++ class breaks every BP-only read. This applies to the shipped route
too — the same BP variables now live on the reparented `ABP_Mannequin_Base`.

## What was never scriptable, and was not a failure

- **A cast node's pins are typed at construction**, and `UEdGraphPin` has not been a `UObject`
  since 4.15 — a successful `TargetType` write can still leave a stale output pin. Delete and
  recreate the node rather than editing it.
- **The function's return-value pin type** lives in `FUserPinInfo` — unreflected both ways.
- **Local variables typed to `ABP_Mannequin_Base`** can be read and flagged but not retyped.

## OPEN DEFECT — the arms are stiff

Carried out of this task, not fixed by it. The founder's read was that the FPSTemplate character's
**procedural-animation components** are missing on the BN character. Half right, and the half that
is wrong matters, because it points at the wrong fix.

`MyCharacter.cpp:1124` is explicit: *"The template does **NOT** push state through a component and
it does NOT have the AnimBP cast back to the character and pull. It is a Blueprint **INTERFACE**
(`BPI_FPST_AnimInterface`) implemented by `ABP_Mannequin_Base`, and the character sends messages to
`Mesh->GetAnimInstance()`."* There is no component to add for the pose side. There is a set of
messages nobody sends.

**`BNCharacter` sends none of them.** Its only `GetAnimInstance()` reference is a null check at
`BNCharacter.cpp:127`. `MyCharacter`, by contrast, sends the bools that drive upper-body pose
selection:

| message | sent from | selects |
|---|---|---|
| `SetSprinting` | `MyCharacter.cpp:952`, `961` | the `fPS_Sprint` pose slot |
| `SetADS` / `SetADS_Upper` | `967-968`, `981-982` | aim-down-sights upper body |
| `SetUnarmed` | `1105`, `1118` | the unarmed layer's pose |

That is why **crouch works and the arms do not**. Crouch, velocity and falling are computed inside
`BNAnimInstance::NativeUpdateAnimation` straight off the CharacterMovementComponent and need no
interface. Upper-body pose selection comes *only* from those bools, so the layer sits on its
default pose.

Where the founder's instinct is right is the tier below. The struct-taking messages —
`SetAimAndLeanInfo`, `SetPoseTransform`, `SetProcApplyTransform` — carry the sway/lean/recoil
offsets and are unwired in `MyCharacter` **too**. `MyCharacter.cpp:1141` says why: they carry
`S_Procedural_*` user-defined structs *"whose layouts would have to be mirrored to be passed, and a
wrong mirror is silent corruption — they land with their procedural components."* Roadmap 1 line
280 already schedules that as post-R1 work.

**So it is two jobs, not one:**

1. **Next** — port `SendAnimInterfaceBool` and its four callers into `BNCharacter`. One function
   plus the input hooks; no new components. **Trap:** `MyCharacter.cpp:1162-1167` — the
   interface's Blueprint display names contain spaces (`SetADS` → `"Set ADS"`), so the send
   mangles the string to resolve `FindFunction`. Get it wrong and it fails **silently**.
   **Open question before building it:** are these sends local-only presentation, or must they be
   driven off replicated state so an observing client sees the right upper body? That decision is
   netcode-builder's, and it gates the packet.
2. **Later** — the `S_Procedural_*` structs with their components, per Roadmap 1 line 280.

This diagnosis is from reading code, not from the editor. It is unverified — but it predicts
exactly what the founder observes, including crouch working.

## Roadmap 2 note — INVERTED by the shipped route

The original note said per-weapon layers must parent to `ABP_BNItemAnimLayersBase` and **never**
the template base. **That is now backwards.** There is no BN base; the template base is the BN
base. Per-weapon layers parent to `ABP_ItemAnimLayersBase`, whose cast resolves because
`ABP_Mannequin_Base` is-a `UBNAnimInstance`. The hazard the note guarded against — a weapon layer
inheriting a cast that returns None — cannot occur while every layer shares one main ABP. It comes
back the moment a second main ABP exists, so if `ABP_BNMannequin` is ever revived, revive this note
with it.

## Log

**13 Aug 2026 · terminal session**

- Pulled `9b1de57`. Build was already current — the pull carried only `.py` and `.md`; newest
  source (`BNCharacter.cpp`, 01:15) predates the DLLs (01:17). Nothing was rebuilt.
- `8742c01` — committed five founder on-disk asset edits that predated the session, to give the
  editor work a clean revert point.
- `ecf7589` — repaired `Tools/bn/45_bn_anim_layers.py`. `main()` read `cast_target` from
  `30_reparent_abp.py`'s `new_parent`, so `a3efbf8`'s BP-class correction was dead code and every
  run would have retargeted the cast at the C++ class. The is-a audit row compared two values that
  were both then the C++ class, so it would have passed: a **false green**, which the script's own
  docstring calls worse than a red. Manual steps 7–9 carried the same regression and are what the
  founder would have followed, since the graph edit was expected to come back `MANUAL-REQUIRED`.
  Fixed, syntax-checked, **never run** — the founder took a different route.
- `guard_laws.py` blocked the first attempt at that repair, correctly: the claim still named the
  finished compile-repair packet. Re-claimed rather than routed around.
- `ae8d38e` — the founder's reparent route, landed. See "What actually shipped".
- Not done, and deliberately: `Config/DefaultGame.ini` untouched, no BN duplicates, `45_*.py`
  unrun.
- Carried forward: the stiff-arms defect above, the `FPSTemplate/` read-only reconciliation, the
  fate of `ABP_BNMannequin.uasset`, and the owed listen+client verification.
- 13 Aug 2026 (mac terminal) — **contract_gap, filed against the R2-G4 fire/cheat packet.**
  Rung 1 found two compile breaks in the pulled R2-G4 work (committed from a cloud session
  with no engine):
  1. `BNGA_Fire.cpp` included `Abilities/Tasks/AbilityTask_ServerWaitClientTargetData.h` — an
     ActionRPG-sample class the engine does not ship. FIXED under a founder-granted extension
     (`Source/BreachpointNext/AbilitySystem/`): new `UBNAbilityTask_ServerWaitClientTargetData`
     in `AbilitySystem/Tasks/`, include + call site corrected.
  2. `BNCheatManager.cpp:23` calls `APlayerController::ServerCheat`, which UE 5.8 removed
     (`ServerExec` is the replacement channel). NOT fixed — `Utilities/` is outside this
     packet's owner_path, the guard blocked the write, and the founder ruled "stop here".
     **Rung 1 stays RED until the R2-G4 owner lands the one-line `ServerExec` swap.**
  3. `BreachpointServer` cannot link on this Launcher install — expected PARTIAL per
     `run-ubt.sh`, not a defect.
