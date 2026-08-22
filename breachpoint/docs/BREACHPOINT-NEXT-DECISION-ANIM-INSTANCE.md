# DECISION — the aim chain: which layer set carries it — **RULED: LYRA ONLY**

**Cut:** 22 August 2026 · **RULED the same day by the founder: option B — "forget the
FPSTemplate, we are just going to concentrate on the Lyra locomotion from now on."**

## The ruling, and what it already changed

Lyra locomotion is the project's animation path. There is no second one.

**Landed in code immediately** (the ruling's whole point is that a dormant second path is what
let three aim fixes land on assets the game never loads):

- `ABNCharacter::UsesLyraAnim()` — **deleted**. `ResolveAnimLayerClass` no longer branches; the
  layer is always the Lyra set, chosen by the weapon's row name from the ini.
- The FPSTemplate fallback chain — **deleted**: the `UnarmedAnimLayer` Config property, its
  cache, its resolve-once flag, and the ini key that fed it. `DT_BNWeapons.AnimLayerClass` no
  longer participates in linking.
- `UBNLAnimInstance`'s header said *"`UBNAnimInstance` stays the production spine"* — **that
  sentence was the bug**, and it is now the opposite, with the history written beside it so
  nobody re-derives it.
- The link log now names the **anim class** as well as the layer, so which of the three
  `ABP_Mannequin_Base` copies is live is readable from the log alone.

`UBNAnimInstance` is left **dormant, not deleted**: the FPSTemplate ABP is still parented to it
and deleting the class would break that asset on load. Retiring both is a clean-up ticket, not
a rush — nothing routes to it any more.

**What is NOT resolved, and is now the only aim question left:** does the Lyra path's own aim
actually work in play? See `TASK-AIM-LYRA-VERIFY`. The rest of this document is the analysis
the ruling came from, kept for the record.

---

## (record) The question is NOT "which anim instance"

That was my framing and it was too shallow. `TASK-AIM-GRAPH-AUDIT` measured the real shape from
the live editor, and the blocker is one layer down:

| | publishes the aim surface | consumes it (aim ModifyBone chain) |
|---|---|---|
| `UBNAnimInstance` + FPSTemplate ABP | **YES** — all seven properties | `ABP_ItemAnimLayersBase`: **32 aim nodes** on spine_01..05 / neck / head, bound to `GetMainAnimBPThreadSafe.PitchRotator`, `.LeanRotation`, `.LeanOppRotation` |
| `UBNLAnimInstance` + MigrateLyra ABP ← **the pawn runs this** | **NO** — declares none of the seven | **ZERO aim ModifyBone nodes** across 53 graphs |

So publishing the aim surface on the running class fixes nothing on its own: the layers the pawn
links have no bone chain to move. That is why every fix landed for a week and nothing turned.

The FOV half of ADS was already solved this way and is the precedent worth copying — the
terminal moved the lens into `FBNADSCameraBlend`, a struct **both** instances own, so the zoom
works regardless of which one runs. The aim half cannot take that shortcut, because the missing
piece is asset-side, not code-side.

## The three real options

**A — run the FPSTemplate ABP on the pawn.** One asset edit: point `BP_BNCharacter`'s mesh
`AnimClass` at `/Game/FPSTemplate/…/ABP_Mannequin_Base` and let `UsesLyraAnim()` fall false.
*Aim works the same day.* **Cost:** the MigrateLyra locomotion look goes with it.
*Best if:* you want aim fixed now and were never attached to the Lyra pass.

**B — give the MigrateLyra layers the aim chain.** Keep what the pawn runs; add the aim
ModifyBone nodes to the MigrateLyra layer ABPs, and publish the surface from `UBNLAnimInstance`
(inherit it, or the shared-struct shape `FBNADSCameraBlend` already proves).
*Best if:* the Lyra locomotion is the look you want to keep. **Cost:** a real terminal ticket —
32 nodes' worth of graph work plus the spine weights, and the `AimPitchAxis`/`LeanAxis` levers
have to be re-measured for that rig.

**C — hybrid: MigrateLyra locomotion, FPSTemplate item layers.** Both ride the same Manny
skeleton, so the link is probably legal. *Cheapest if it works.* **Cost:** unverified — one
editor session to prove the layer interface matches before anyone commits to it.

## (record) What was asked

**"A", "B", or "C"** — answered B.

One caution the audit paid for: `ABP_Mannequin_Base` exists **three times** in this project, and
every previous aim attempt edited a copy the game does not load. Whichever way you rule, the
ticket will name the full object path and the read-back will prove which one the pawn actually
runs.
