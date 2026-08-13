# TICKET — R3 aim: verify the ABP consumes `Pitch`, then the FX assets
> STATUS: in-progress — mac terminal 13 Aug 2026 (ad6d383). Claimed on the founder's 'yes pick it up'; building first (BNAnimInstance changed at ad6d383), then editor+MCP.

**Cut:** 13 August 2026 by the cloud lead · **For:** the terminal session (editor + MCP)
**Binds to:** the NEXT doc family. **Read [`BREACHPOINT-NEXT-ASSET-RULES.md`](BREACHPOINT-NEXT-ASSET-RULES.md) FIRST** —
its §1 (reuse, never author) and §5 (do only what this ticket lists) govern every step here.

**Owner path:** `Content/BN/` + `Config/DefaultGame.ini`. **Never `Source/`.**

---

## Part 1 — the ONE question that decides whether R3 G1 worked

C++ now publishes `Pitch` on `UBNAnimInstance` (landed this session). The root cause it fixes:
**`SetControllerPitch` has never fired in either module**, so the ABP's `Pitch` was never written
and the weapon never followed the look.

**But publishing a property only helps if something reads it.** If the ABP's aim consumers hang
off the un-fired `SetControllerPitch`/`SetAimAndLeanInfo` interface events rather than off the
variables, the pose still will not move.

**Do this and report — it is one look, not a project:**

1. Open `ABP_Mannequin_Base`. Find every consumer of the variable **`Pitch`** (right-click →
   Find References). Report: does the **AnimGraph** read it (an aim-offset / rotation node), or
   is its only reader inside the event graph?
2. Same for **`LeanRotation`** and **`LeanOppRotation`**.
3. Confirm `AimSpineWeights_UE5` and `LeanSpineWeights_UE5` still hold their measured values on
   the CDO (C++ deliberately does NOT write these — they are UserDefinedStruct-typed and cannot
   be declared in C++; the asset owns the distribution, C++ owns the totals).

**Report the answer. Do NOT fix the graph.** If the consumers are event-graph-only, that is a
finding the lead turns into a packet — a graph edit is not in this ticket's scope.

## Part 2 — the FX assets, reuse only

**Author nothing.** Every item below exists in the FPSTemplate; find it and point at it. If
something genuinely does not exist, say so with what you searched — do not create a substitute.

| # | Item | Detail |
|---|---|---|
| 2.1 | **Tracer cue FX** | The R2 ticket wrongly specified a `BeamEnd` vector. The template's tracer takes **`User.ImpactPositions[]` (array) + `User.Trigger`** — confirmed from the template's own FireEffect graph. **Report the system's exact path and its full user-parameter list**; the C++ cue needs a code change to write those params, which is the LEAD's packet, not yours. Leave the cue's Effect ref unset until then |
| 2.2 | **Impact decal — the bullet hole** | Find the template's impact decal material/asset and the surface-typed impact FX `MyCharacter::ImpactEffect` uses. **Report paths only.** The decal cue class does not exist yet — that is C++ the lead owes |
| 2.3 | **Weapon sounds** | Find the template's fire and reload sounds for the rifle and pistol. **Report paths only** — the cue classes have no sound field yet; that is C++ the lead owes |

Part 2 is **discovery, not wiring.** Three C++ changes depend on what you find, and guessing the
parameter names is what broke the tracer the first time.

## Part 3 — one deletion

`/Game/BN/Animation/ABP_BNMannequin` is **dead** — the shipped reparent route orphaned it (open
item E2/D2, founder-agreed). Delete it and confirm nothing references it first. If anything does,
stop and report instead.

---

## Done means

Part 1's three answers, Part 2's paths and parameter lists, Part 3 deleted or reported, and the
read-back pasted into the Log. **That is the whole ticket.**

## Scope, restated because it has been a problem

- Do only what is listed. Extra work found is a **Log entry**, not a licence.
- **No `Source/` edits.** Three items here need C++; they are the lead's, and this ticket exists
  to give the lead the facts it needs.
- No asset creation. No graph edits.
- Do not touch assets this ticket does not name — the accidental `BPC_FPST_Lyra_FireEffectComp`
  dirty-save is the precedent for why.
- The read-back is the deliverable.

## Log

_(terminal: append the three answers, the asset paths and parameter lists, and the deletion result)_
