# TICKET — BP25: The weapon render pipeline has nowhere to put its output

> STATUS: open — cut from `docs/ui/WEAPON-RENDER-PLAN.md` §9.3, 2 Aug 2026. The fifth C++ gap,
> and the only one not in `UI-DESIGN-SYSTEM.md` §6. R37's committed render plan produces PNGs
> that no field can reference. Crosses three owner paths — read Notes before claiming.

Founder directive: `WEAPON-RENDER-PLAN.md` is a complete, committed plan for rendering weapon
silhouettes from the meshes the player is actually holding. It ends at a wall: there is no icon
field on the row, no icon column in the table, and no icon on the ViewModel. **Rendering the PNGs
does not put them on screen.** This ticket builds the consumer.

**Law 3 settles the shape before anyone opens an editor:** the reference is **SOFT**
(`TSoftObjectPtr<UTexture2D>`), it lives in the DataTable, and there is no `ConstructorHelpers`
anywhere in it. A hard ref here would drag every weapon texture into memory with the row struct.

**Ordering law:** 1 → 2 → 3 strictly. The C++ field must exist before the CSV column, or the
reimport warns on an unknown column. Step 4 (the widget) needs 3 only.

## Kickoff (machine-checkable — the tickets skill verifies these BEFORE a claim)

- requires: engine-installed
- `Source/Breachpoint/Data/BRDataRows.h` compiles and `FBRWeaponRow::ValidateSchema()` passes on
  all three shipped rows
- `Content/Data/DT_Weapons.csv` re-imports clean against `FBRWeaponRow` today (the baseline this
  ticket must not break)
- The soft-ref precedent is live: `FBRWeaponRow::MeshSoftPath` (`TSoftObjectPtr<UStaticMesh>`,
  `BRDataRows.h:80`) resolves for all three rows — this ticket adds a second field of the same
  kind beside it
- owner_path: `Source/Breachpoint/Data/`, `Content/Data/DT_Weapons.csv`,
  `Source/Breachpoint/UI/`

## Steps (in order)

1. `TSoftObjectPtr<UTexture2D> IconSoftPath` on `FBRWeaponRow`, immediately after
   `MeshSoftPath`, same `UPROPERTY` shape. **`ValidateSchema()` must NOT reject an empty icon** —
   the art does not exist yet (there are zero `.png` files in the repo) and a schema that fails on
   a missing icon blocks every weapon row today. Empty = "no icon, fall back to text".
   Owner: **sim-builder** (this is `Data/`, which the BP03 Log already records as outside the
   weapons packet's grant — see Notes).
2. `IconSoftPath` column in `Content/Data/DT_Weapons.csv` for all three rows (`AR`, `Magnum`,
   `Rocket`), left **empty** until the render pass produces textures, then reimport. Owner:
   **builder** (data crew).
3. `UBRVM_Combat`: FieldNotify `TSoftObjectPtr<UTexture2D> ActiveWeaponIcon` (and
   `StowedWeaponIcon` — see the open question) plus `SetWeaponIcons()` beside the existing
   `SetWeaponNames()`, pushed from `UBREquipmentComponent`'s active-slot RepNotify along the same
   path that already pushes the names. Async-load the soft ref; **never block on it** — the tray
   shows the name until the texture resolves. Owner: **ui-builder**.
4. The widget honours a null icon: `HUD-CAMPAIGN-MEASURED` measures one silhouette slot; with no
   texture it renders `ActiveWeaponName` in the same box. This is what lets steps 1–3 land before
   any art exists. Owner: **ui-builder**.
5. Verify: rung 1 on all three targets; rung 2 asserts (a) the CSV re-imports clean with the new
   column, (b) `ValidateSchema()` still passes on an empty icon, (c) **every non-empty
   `IconSoftPath` resolves against the asset registry** — the validator `BP03`'s Log proposed
   after `FireCueTag` named three tags that nothing declared and passed every gate for three
   days. Owner: **verifier**.
6. **Critic:** an unresolvable soft path shipping green; the async load completing after a swap
   (icon of the previous weapon on the current slot); memory — three textures resident because
   something took a hard ref on load. Owner: **critic**.

## Done when

- [ ] `FBRWeaponRow::IconSoftPath` exists as `TSoftObjectPtr<UTexture2D>`, rung 1 green on all
      three targets, and **grep proves zero `ConstructorHelpers` and zero hard `UTexture2D*`
      members** anywhere in the change (law 3)
- [ ] `DT_Weapons.csv` carries the column and re-imports **clean** — zero warnings, all three
      rows still `ValidateSchema()`-passing with the icon empty
- [ ] `UBRVM_Combat` exposes the icon as FieldNotify and the tray renders the *name* when the
      icon is empty or unresolved — proven with the column empty, i.e. **before any art exists**
- [ ] A non-empty path that names a missing asset **fails rung 2**, not the player's screen
- [ ] Weapon swap shows the correct icon in the two rendering contexts a tray has — the
      **listen-server host** and a **remote client** — after a fresh join, not only from map
      start. *Stated honestly under law 6: there is no third view for this claim, because the
      weapon tray is owner-only UI and no observing client renders another player's tray. A
      three-view claim here would be theatre; the rung that matters is join-in-progress.*
- [ ] Critic findings addressed or waived in the Log
- [ ] Findings + decisions written to this ticket's Log

## Notes

- Crew: sim-builder step 1 · builder step 2 · ui-builder steps 3–4 · verifier · critic
- Contracts: `data-and-assets.md` (**the governing one** — one source of truth per kind, text
  over binary, soft refs in data; and R18/R26, the WBP holds layout only) · `netcode.md` (law 7 —
  equipment state arrives late on a joining client; the icon is a consumer of it) ·
  `gas-purity.md` (only as a boundary: the icon is cosmetic and touches nothing in the ammo
  named exception) · `testing.md` (rungs 1 + 2)
- **Blocks:** the HUD weapon tray silhouette. Figma page `HUD / Elements`, component set
  `SET Weapon / *` — `HUD-AUDIT.md` §3.8 records **"Weapon silhouettes (6) — all pass"**, so the
  design is measured, built and correct in Figma, and has nowhere to land in the game. Also
  blocks the whole of `WEAPON-RENDER-PLAN.md` from being worth executing: §10's definition of
  done produces PNGs, and without this ticket they are files in a folder.
- **Bindable today without this:** the tray's *text* half. `UBRVM_Combat` already serves
  `ActiveWeaponName` and `StowedWeaponName` as `FText`, plus `MagazineAmmo`/`ReserveAmmo` — a
  fully functional, un-illustrated weapon block binds now. Everything else on the in-match HUD
  binds too except reticle colour state (`UI-DESIGN-SYSTEM.md` §6, and BP22).
- Binary files owned: `Content/Data/DT_Weapons.csv` — **shared with BP03 and BP13**; lock it, and
  do not claim this ticket while either is in flight (law 7, one owner per binary per ticket)
- Out of scope: rendering the icons (`WEAPON-RENDER-PLAN.md` is its own execution), the
  silhouette reduction rules (`ICON-CONSTRUCTION-SPEC.md` §3.5), the tint token, muzzle
  direction, and the WBP tray layout (BP10)
- **Owner-path warning (read before claiming).** This ticket spans three grants that today belong
  to three different packets: `Source/Breachpoint/Data/` and `Content/Data/DT_Weapons.csv`
  (BP03/BP13) and `Source/Breachpoint/UI/` (BP10). `WEAPON-RENDER-PLAN.md` §9.3 already filed
  this as a `contract_gap` rather than fixing it. The grant above is the request; **if the board
  will not grant all three, split it 1+2 / 3+4 and serialize — do not edit another owner's file
  to unblock (law 5).**

## Log

(append findings here, dated, newest last — this is what the next session reads)

**2 Aug 2026 — filed. One correction to the source document, and it matters for grepping.**

**`FBRHUDCombatVM` DOES NOT EXIST.** `WEAPON-RENDER-PLAN.md` names it three times — §0
("*`FBRHUDCombatVM` (`Source/Breachpoint/UI/BRViewModels.h`) exposes `ActiveWeaponName` /
`StowedWeaponName`*"), §9.3 and §9.7 — and there is no such type anywhere in `Source/`. The real
class is **`UBRVM_Combat`**: a `UCLASS` deriving from `UMVVMViewModelBase`, not a `USTRUCT`. The
plan's *description* is exactly right — the two `FText` fields are there at `BRViewModels.h:132`
and `:136`, and there is no icon field — but a packet that greps for `FBRHUDCombatVM` finds
nothing and may conclude the file is wrong or the type was deleted.

This is the same defect class BP03's Log named on 1 Aug: two documents pointing opposite ways,
and the one a reader hits *first* wins. **Recorded here rather than fixed in place** —
`docs/ui/` is not this ticket's owner path, and whoever holds it should correct §0/§9.3/§9.7 to
`UBRVM_Combat`.

*Verified on disk, everything else in §9.3 is accurate:* `FBRWeaponRow` (`BRDataRows.h:27`)
carries `DisplayName` (FText), `AbilitySet` (`TSoftObjectPtr<UBRAbilitySet>`, :77) and
`MeshSoftPath` (`TSoftObjectPtr<UStaticMesh>`, :80) — **and no icon of any kind.**
`Content/Data/DT_Weapons.csv` has no icon column. `UBRVM_Combat` has the two names and no
texture. All four claims in the plan hold.

*Open questions — stated, not guessed:*

1. **Does the stowed weapon get an icon?** `WEAPON-RENDER-PLAN.md` §9.7 raises this and leaves it
   open: `HUD-CAMPAIGN-MEASURED.md` measures **one** silhouette slot, while `UBRVM_Combat` carries
   **both** names. If the stowed weapon gets one it likely needs a second, smaller size — which
   changes the render plan from one master to two, or forces a min-feature re-check at the smaller
   size. **Unmeasured.** Step 3 above adds `StowedWeaponIcon` on the assumption it is cheap to
   expose and free to ignore; if that assumption is wrong, drop it — but say so here.
2. **One texture per weapon, or a sprite atlas / material?** This ticket assumes
   `TSoftObjectPtr<UTexture2D>` because §9.3 names that type. An atlas would change the field to a
   name-plus-region and is a different data shape. Nobody has decided; three weapons is small
   enough that the simple answer is probably right, and that is a judgement, not a finding.
3. **The art does not exist.** Zero `.png`, `.svg` or `.tga` files in the entire tree
   (`WEAPON-RENDER-PLAN.md` §0). Steps 1–4 are deliberately built to land and pass with the column
   empty, so this ticket does **not** block on the render pass and the render pass does not block
   on it. That ordering is the point of the null-safe requirement in step 4 — do not "simplify" it
   away.
4. **All three meshes are Epic template placeholders** (§0, §9.4). Whatever the pipeline first
   produces is an honest picture of a weapon we are going to replace. Review the plumbing this
   ticket builds; do not review the first icons as art.
