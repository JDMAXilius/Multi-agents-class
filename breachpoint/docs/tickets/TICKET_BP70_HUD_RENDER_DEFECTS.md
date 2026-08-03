# TICKET — BP70: Three HUD render defects, found in a founder screenshot

> STATUS: **open — partially landed, 3 Aug 2026.** Cut from a founder-supplied render of
> `WBP_HUDLayout` in PIE. **Editor/MCP lane.** All three are visible in one frame; none was caught
> by a static check, which is itself the finding.
>
> **Where each defect stands after the HUD audit (see the Log — two of this ticket's own
> prescriptions were wrong):**
> - **D1** — the `--verify` gate landed (`0118b2e`); **running it is BP72 step 1**, and that run
>   is the only chance to read the pre-existing assets before a rebuild overwrites the evidence.
> - **D2** — reclassified: not produced by the current plan. Interim fix landed (killfeed
>   `WeaponIcon` ships Collapsed); the real fix is **blocked on `T_UI_Weapon_Unknown`**, art pass.
> - **D3** — **C++ FIX LANDED** in `BRProgressBar.cpp` (visibility moved onto the widget, not two
>   named children). Unverified: needs BP71 to compile and BP72 to render.
> - **Three FOUNDER DECIDEs still open** — D3's gate predicate, the killfeed corner, the tray
>   split. All three are cheap to reverse after BP72 renders; none should be guessed before.

Founder directive: the HUD render is structurally correct — motion tracker bottom-left with the
`20 m / CANAL` callout, match state bottom-centre, reticle and damage arc centred, `42 m`
waypoint, interaction prompt. **These three defects are what stands between it and a screenshot
we can call final.**

**Ordering law:** D1 gates nothing but is the largest; D2 and D3 are independent. All three are
one editor window.

## Kickoff (machine-checkable)

- requires: **editor-live** — every fix is an asset edit or a value read back off an asset
- `WBP_HUDLayout` exists in `Content/UI/HUD/` and opens
- `Tools/gen_ui/wbp_plan.py` parses (the plan is the source of truth for what the tray contains)
- owner_path: `Content/UI/HUD/`, `Tools/gen_ui/wbp_plan.py`, `Source/Breachpoint/UI/HUD/`

## The three defects

### D1 — The weapon tray renders the ammo readout TWICE

**What the render shows.** Bottom-right carries `36 | 108` in cyan at the specified size, and
**beneath it a second, faded `ASSAULT RIFLE — 36`** overlapping a blank rectangle. Two widgets,
one value.

**The likely cause, and why a static check missed it.** This is the shape of the defect
`04efb2a` already fixed once in the killfeed:

> *"THE DOUBLE KILLFEED WAS LIVE, NOT LATENT. I had this wrong… the widget IS still in the
> asset — stale from an earlier generation — and `BindWidgetOptional` binds BY NAME, so the
> early return never fired."*

**`BindWidgetOptional` binds by name.** A stale widget left in the asset from an earlier
generation binds happily and renders alongside the current one. The plan is correct; the
*asset* carries a leftover.

**Fix:** read the asset's real widget list (`GetWidgets`, as the P0 probe did), diff it against
`wbp_plan.py`'s tray subtree, and delete every widget the plan does not name. **Do not
hand-delete in the editor** — regenerate the tray from the plan so the asset and the plan agree,
which is the only way this stays fixed.

**Then close the class of bug, not the instance:** `build_wbp.py` should refuse to finish when
the built asset contains a widget the plan does not name. A generator that only adds is a
generator that accumulates.

### D2 — An empty brush renders as a blank rectangle

**What the render shows.** A flat, untextured rectangle sits beside the ammo numerals in the
tray — an `Image`/`CommonLazyImage` whose brush has no texture.

**Why it matters more than it looks.** `LAYOUT-DOCTRINE.md` §3.3 exists for exactly this: **an
empty slot and a broken binding look identical.** Right now nobody can tell whether that
rectangle is a weapon silhouette that failed to resolve, or a widget that was never given art.

**Fix:** give every dynamic slot in the tray its default brush in the plan —
`T_UI_Weapon_Unknown` at the slot's authored dimensions. A missing path then renders a visible
placeholder that says *"this binding ran and found nothing"*, which is a different and much
more useful statement than a blank box.

**Note this is BP25-adjacent:** the real silhouette needs `FBRWeaponRow::SilhouetteSoftPath`,
which does not exist yet. **The placeholder is not blocked on BP25** — land it now.

### D3 — The health bar shows at full shields

**What the render shows.** The top-centre vitals block draws the gold health bar full-width
while shields are intact.

**The rule it breaks.** `UI-DESIGN-SYSTEM.md` §1, sourced from Halo Support: *health appears
only after damage*. The two-layer read is the most important element on screen (GDD §2.9) and
it works because the second bar **arriving** is itself the signal.

**Fix:** bind the health bar's `Visibility` to a ViewModel field —
`UBRVM_Combat::GetHealthPercent() < 1.0` or an explicit `IsHealthDamaged()` — and default it to
`Collapsed`. **`Collapsed`, not `Hidden`**: hidden still occupies layout and the vitals block
would keep a gap where the bar will appear.

**Check whether this is a debug state first.** If something forces the bar visible for testing,
the defect is the debug override, not the binding.

## Also confirm while the editor is open

- **Shield bar colour.** The render is silver/white; the token is `Shield #35D0F2`. Either the
  widget is not reading the palette, or the silver is a deliberate change — in which case it is
  a **token** change (`UI-DESIGN-SYSTEM.md` §2), not a per-widget value, and "cyan means you"
  needs restating across HUD, feed, scoreboard and lobby.
- **Killfeed position.** The render places it mid-left. `LAYOUT-DOCTRINE.md` §7 says top-right;
  Infinite uses far-left. Mid-left is neither and sits in the sightline. **Founder call** — but
  it should match one of the two, and the doc should say which.
- **What the render gets right, so nobody "fixes" it:** `YOU` renders **white** in the killfeed
  while others are team-coloured. That is Infinite's convention and it is correct.

## Done when

- [ ] `WBP_HUDLayout`'s widget list matches `wbp_plan.py`'s tray subtree exactly — proven by
      reading the asset back, not by looking at it
- [ ] `build_wbp.py` fails when the built asset contains a widget the plan does not name,
      proven once against a deliberately stale asset
- [ ] Every dynamic tray slot has a default brush; a missing path renders the placeholder
- [ ] Health bar is `Collapsed` at full shields and appears on first damage — proven in PIE
- [ ] Shield colour reads from the palette, or the token changed and the doc says so
- [ ] Killfeed position ruled and recorded
- [ ] A fresh PIE screenshot in this ticket's Log showing all three clear

## Notes

- Crew: **builder** (asset + plan), **ui-builder** consults on the vitals binding.
- Binary files this ticket OWNS: `Content/UI/HUD/WBP_HUDLayout.uasset` and the tray subtree.
- Out of scope: BP25's real silhouette art; the art pass on HUD strings (BP71); any rung claim
  above 3 — PIE is single-process.
- **The generator lesson is the durable part.** Two of these three are "the asset drifted from
  the plan" wearing different costumes. R37's receipt requirement catches what a call *did*; it
  does not catch what an earlier call *left behind*.

## Log

(append findings here, dated, newest last)

**3 Aug 2026 — HUD-CPP-AUDIT re-judged all three defects; two of this ticket's
prescriptions were wrong. Provisional rulings below stand unless the founder vetoes.**

- **D1:** the `build_wbp.py` gate this ticket demanded EXISTED and was unreachable — the
  build deletes the asset before comparing, so it can only see what it just created. A
  `--verify` mode landed (reads the ON-DISK tree, no delete, no writes); the "proven against
  a deliberately stale asset" box is now provable. **Still owed by the editor lane:** run
  `--verify`, then rebuild at the current plan digest and commit the receipt — no committed
  receipt matches the plan on disk.
- **D2:** the blank rectangle is NOT produced by the current plan (neither tray widget has an
  Image child) — it is a stale widget (D1's class) or an unrebuilt asset. `T_UI_Weapon_Unknown`
  does not exist; the default-brush fix is **blocked on that one texture**. Interim: the
  killfeed entry's `WeaponIcon` now ships Collapsed from C++, so the one brushless Image in
  the plan can never render a blank box.
- **D3:** this ticket's prescription ("bind Visibility to `GetHealthPercent() < 1.0`") is what
  the code already did — it would have changed nothing. The real defect: visibility landed on
  only `Fill`+`Track`, leaving the frame and readouts drawing at full health (the likely "gold
  bar"). Fixed: `ApplyBarVisibility` now hides THE WIDGET. **Ruling on Hidden-vs-Collapsed:
  in a canvas slot they are identical (no reflow is possible), so the ticket's `Collapsed`
  demand is amended — `Hidden` stands** per the code's documented reservation reasoning.
  **Open FOUNDER DECIDE:** the gate is health-damage (current code, per UI-DESIGN-SYSTEM §1)
  — if the intent is shield-state gating instead, `AreShieldsBroken()` is already observed by
  the widget and the change is one predicate.
- **Killfeed position (the §"Also confirm" item):** doctrine said top-right, the render showed
  mid-left, the built plan says bottom-left (60, −189) from Figma — the authority for
  position. **Provisional: bottom-left stands and the doctrine was corrected to match.**
  FOUNDER DECIDE if the render's placement was the intent.
- **Tray split:** Figma measures ONE 280×110 "Loadout Tray"; the code ships EquipmentTray +
  AmmoBlock as siblings. FOUNDER DECIDE — it gates the doctrine's InvalidationBox (one box
  needs one common parent). `INVALIDATION` class constant staged in the plan either way.
