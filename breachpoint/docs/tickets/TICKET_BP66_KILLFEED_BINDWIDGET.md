# TICKET — BP66: The killfeed row has no bindable surface, and D1 is why

> STATUS: **C++ DONE 3 Aug 2026 (HUD-C, `20a9b24`) — asset build owed by BP72, compile owed by
> BP71.** Cut 2 Aug 2026 from the `gen_ui` authoring pass. Found by building the
> asset: `WBP_KillfeedEntry` now has a real widget tree and **not one widget in it can be
> reached from C++**. This is the first concrete instance of decision **D1**, so it is filed
> as a ticket rather than left as a roadmap paragraph.

Founder directive is implicit in R18/R26 and needs no re-litigation: a WBP carries layout,
anchors and animation only. The problem is that `UBRKillfeedEntryWidget` was written on the
opposite assumption.

**What the code says today** (`Source/Breachpoint/UI/BRHUDLayout.h:14-43`):

- `UBRKillfeedEntryWidget` declares **zero** `meta = (BindWidget)` members.
- It exposes `SetEntry()` plus five `BlueprintCallable` getters (`GetKillerName`,
  `GetVictimName`, `GetSpotterLine`, `IsLocalPlayerInvolved`, `GetEntry`).
- Its only push path to the visual layer is
  `UFUNCTION(BlueprintImplementableEvent) void BP_OnEntrySet()`.

**Why that cannot ship as designed.** Implementing a `BlueprintImplementableEvent` requires an
event-graph node in the WBP. R26 condition 2 is *"EventGraph and ConstructionScript empty, zero
nodes"* and `ui-presentation` §8.2 applies R26 to a WBP exactly as to a `BP_BR*` container. The
alternative route — a UMG **property binding** onto the `BlueprintCallable` getters — is
explicitly a rung-2 grep gate (`contracts/testing.md`, *"UMG property bindings"*) because it is a
per-frame poll wearing a different hat, and law 4 forbids it. **Both available routes are
findings.** The widget is unreachable by construction.

`UBRHUDLayout` carries **seven more** `BlueprintImplementableEvent`s with the same problem
(`BP_OnShieldHit`, `BP_OnFleshHit`, `BP_OnHeadshotHit`, `BP_OnKillConfirmed`,
`BP_OnVitalsStateChanged`, `BP_OnMatchStateChanged`, `BP_OnKillfeedRebuilt`) and C++ calls all of
them. This ticket settles the pattern once; BP43 inherits the answer.

**Ordering law:** step 1 (the D1 ruling) gates 2 and 3. Step 4 needs 3. Nothing here needs an
editor until step 4.

## Kickoff (machine-checkable — the tickets skill verifies these BEFORE a claim)

- requires: `engine-installed` for steps 1–3; `editor-live` for step 4
- `Source/Breachpoint/UI/BRHUDLayout.h` compiles at HEAD and still declares zero `BindWidget`
  members on `UBRKillfeedEntryWidget` (if this is false the ticket is already done — verify,
  do not assume)
- `Content/UI/WBP_KillfeedEntryWidget.uasset` exists with a real widget tree — six widgets:
  `RootSizeBox`, `Row`, `KillerNameText`, `WeaponIcon`, `VictimNameText`, `SpotterLineText`
  (landed `327f505`; `Tools/gen_ui/wbp_plan.py` is the source of truth for the names)
- `python3 Tools/gen_ui/selftest_no_editor.py` exits 0
- owner_path: `Source/Breachpoint/UI/BRHUDLayout.h`, `Source/Breachpoint/UI/BRHUDLayout.cpp`,
  `Tools/gen_ui/wbp_plan.py`

## Steps (in order)

1. **Rule D1.** The roadmap's proposal (`ROADMAP.md` §1, D1) is: replace every
   `BlueprintImplementableEvent` with a named UMG animation driven from C++ —
   `UPROPERTY(Transient, meta = (BindWidgetAnimOptional)) TObjectPtr<UWidgetAnimation>` and
   `PlayAnimation(...)` from the C++ side. The WBP then carries layout + anchors + a named
   animation and **zero graph nodes**, which is squarely inside the contract's own wording,
   and the *trigger* stays in diffable C++ while only the *curve* lives in the binary.
   Record the ruling in `docs/DESIGN-RULINGS.md`. **This is a founder call, not a builder's.**
2. **Add the `BindWidget` members** to `UBRKillfeedEntryWidget`, using the names the asset
   already ships so no re-authoring is needed:

   | Member | Type | Fed from |
   |---|---|---|
   | `KillerNameText` | `TObjectPtr<UCommonTextBlock>` | `Entry.KillerName` |
   | `VictimNameText` | `TObjectPtr<UCommonTextBlock>` | `Entry.VictimName` |
   | `SpotterLineText` | `TObjectPtr<UCommonTextBlock>` | `Entry.SpotterLine` |
   | `WeaponIcon` | `TObjectPtr<UImage>` | BP25's `IconSoftPath` — **soft**, resolved at set time |

   `SetEntry()` pushes into them directly. **`BindWidgetOptional` for `WeaponIcon`** until BP25
   lands the icon field, so a missing icon is a null, not a failed asset load.
3. **Delete `BP_OnEntrySet`** and replace it per the step-1 ruling. If a treatment genuinely
   cannot be a widget animation, it becomes a C++ state enum the WBP reflects — never a graph
   branch.
4. **Re-run the generator** (`python3 Tools/gen_ui/build_wbp.py --asset WBP_KillfeedEntryWidget`).
   The plan's validator now cross-checks the header, so step 2's names are enforced
   mechanically: a mismatch is a plan-time error, not a silent empty widget in PIE.

## Done when

- [ ] D1 is ruled and written in `docs/DESIGN-RULINGS.md`
- [ ] `UBRKillfeedEntryWidget` declares the four `BindWidget` members above
- [ ] `BP_OnEntrySet` is gone, or the ruling explicitly preserves it with a stated reason
- [ ] `python3 Tools/gen_ui/wbp_plan.py` reports `PLAN OK` with the header cross-check passing
      for `WBP_KillfeedEntryWidget` (currently it passes only because the plan marks nothing `bind`)
- [ ] Rung 1 on the targets this machine can build, stated as PARTIAL if the server target
      cannot link (launcher install — see `docs/ui/receipts/P0-umg-probe-20260802.md` §0)
- [ ] A killfeed row renders in PIE with real text. **Rung 3 only** — a killfeed is replicated
      state and a real claim needs rung 4b (server + acting client + observing client)

## Notes

- **Do not "fix" this with a property binding.** It is the exact artifact the rung-2 grep gate
  exists to catch, and it would pass review only until someone ran the gate.
- The `SpotterLineText` slot renders **empty** when the string is empty. It never collapses
  layout and never waits on the LLM — offline means an identical HUD minus flavour
  (`ue5-ui-architecture` §5).
- `UBRHUDLayout`'s seven sibling events are the same defect at larger scale. This ticket does
  not fix them; it fixes the *pattern*, and BP43 applies it.
- The widget names in `Tools/gen_ui/wbp_plan.py` are a **proposal to C++**, not a fait accompli.
  If step 1 chooses different names, change the plan and re-run the generator — the asset is
  regenerable, which is the whole point of it being generated.

## Log

**2 Aug 2026 — cut.** Found while authoring `WBP_KillfeedEntryWidget` from `Tools/gen_ui/wbp_plan.py`.
The asset built and compiled cleanly (six widgets, `AssetTools.save_assets` → true), and that is
precisely what exposed the gap: the tree is real, and no C++ member can reach any of it. Before
this pass the asset was an empty shell, so the defect was invisible.

**3 Aug 2026 — the contract landed (HUD-CPP-AUDIT packet C, `20a9b24`).**

`UBRKillfeedEntryWidget` now declares exactly the four members this plan pre-committed, so the
asset that was already built is correct with zero re-authoring:

| Member | Bind | Note |
|---|---|---|
| `KillerNameText` | `BindWidget` | `SetEntry` writes it |
| `VictimNameText` | `BindWidget` | `SetEntry` writes it |
| `SpotterLineText` | `BindWidgetOptional` | written, and RESERVES its slot — never collapsed, so a late LLM line appears without reflowing the feed |
| `WeaponIcon` | `BindWidgetOptional` | ships **Collapsed** from C++: no glyph art exists, and a brushless Image is BP70 D2's blank rectangle |

`BP_OnEntrySet` and the five dead accessors are gone. `wbp_plan.py` marks all four `bind: True`
and still prints `PLAN OK`, so `validate()` now enforces the header/plan match it was written for.

**The whole-row tint stays.** The original filing expected per-element colour once binds landed,
but the plan authors every leaf WHITE precisely so one `SetColorAndOpacity` on the row is the
single owner of the VISR channel — going per-element would be three writes per row per refresh
for identical pixels. Recorded so the next reader does not "finish" it.

**Still owed, and by whom:** BP71 compiles it; BP72 rebuilds the asset at the current plan digest
and proves the four binds resolve. This ticket closes when BP72's receipt shows the row rendering
two names.
