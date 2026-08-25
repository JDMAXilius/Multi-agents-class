# TICKET — The HUD's new optional slots: feed parts, death screen, score bars

> STATUS: open — cut 24 Aug 2026 by the cloud lead. Needs a LIVE EDITOR. Additive only: every
> widget below already exists and already loads; nothing here can break a HUD that works today.

Founder directive: R7.6 and R7.7 closed four of the seven C++ gaps the terminal handed back on
22 Aug (`TASK-R7-WBP-HUD` Log, "C++ gaps — the design needs these and the bind contract cannot
carry them"). The ViewModel fields now exist and are compiling; the WBPs have nowhere to put
them. Each slot below is a `BindWidgetOptional`, so a WBP that skips one still loads — which is
also why a missed slot is invisible rather than loud, and why the read-backs matter.

**Ordering law:** none between the three widgets. Do them in any order, or one at a time.

## Kickoff (machine-checkable)

- requires: editor-live
- Rung 1 PASS for `BreachpointEditor` (it was, 24 Aug) — the ViewModel getters these bind
  against are `UBNVM_Combat::GetKilledByWeaponIcon / GetRespawnFraction` and
  `UBNVM_Match::GetSelfScoreFraction / GetTopScoreFraction`
- The eleven WBPs from `TASK-R7-WBP-HUD` exist and load clean
- owner_path: `Content/BN/UI/`
  <!-- ASSETS ONLY, and layout only within them: no graph nodes, no variables, no bindings, no
       colours (every colour is C++'s — a WBP that sets one is a finding). ASSET-RULES §5. -->

## Steps (in order)

1. **`WBP_BNKillfeedEntry` — the feed's three parts** (design node `30:22`). Add two optional
   `TextBlock`s beside the existing `LineText` and `WeaponIcon`:
   **`KillerText`** at x8 and **`VictimText`** at x110, with the 22×8 glyph between them at x78.
   **Bind BOTH or NEITHER.** With both present and both names non-empty the row draws the three
   parts and hides `LineText`; with either missing it draws `LineText` alone. `LineText` stays
   REQUIRED — it is the only correct render for "X died" and for a suicide, which have no killer
   to lay out.
2. **`WBP_BNScreen_Death` — three optional additions** (nodes `36:9`, `36:11/12/13`, `36:6`):
   - `Image` **`WeaponIcon`**, brush EMPTY — C++ fills it from the same `Icon` column the tray
     and the feed glyph read, so one filled DT row lights three places.
   - `TextBlock` **`CountdownText`**, the bare numeral, LARGE, beside the existing sentence.
   - `ProgressBar` **`RespawnRing`**. **Read the note below before building this as a ring.**
3. **`WBP_BNMatchBand` — the two score bars.** The self bar becomes a `ProgressBar` named
   **`SelfScoreBar`** and the leader bar a `ProgressBar` named **`TopScoreBar`**, at their
   measured slots (x24 y7 60×8 and x240 y7 44×8). C++ fills both as fractions of the score limit
   and HIDES them until match data is live — an empty bar claims the score is zero, which is the
   lie the band's dashes already exist to avoid.
4. **Read back** each of the three WBPs from a fresh load: parent class plus the full child tree
   with exact `BindWidget` names.
5. **PIE, solo.** Expect no new `did not resolve` lines and no `placed no rows` warnings; the
   HUD must look exactly as it does today except where a new slot has something to say.

## THE RESPAWN RING, and why it ships as a bar

The design draws a radial sweep. A radial sweep is a MATERIAL (Tier 4), and the combat ViewModel
updates the respawn countdown **once a second** because law 4 forbids the per-frame push a smooth
sweep would need from C++. So the C++ binds a `UProgressBar` and publishes `RespawnFraction`: a
bar that steps once a second is honest, a ring that stutters is not. When the radial material
lands it reads the same fraction under the same bind name and no C++ changes.

## Done when

- [x] `WBP_BNKillfeedEntry` read back with `KillerText` and `VictimText` (or with neither, stated
      as a deliberate choice in the Log)
- [x] `WBP_BNScreen_Death` read back with `WeaponIcon`, `CountdownText`, `RespawnRing`
- [x] `WBP_BNMatchBand` read back with `SelfScoreBar` and `TopScoreBar` as `ProgressBar`s
- [ ] PIE: no new unresolved-bind lines, HUD unchanged where nothing was added

## Handed back to the FOUNDER, not to this ticket

Four read-backs from `TASK-R7-WBP-HUD` Step 5 have never been run because they need a hand on the
keyboard: the **death overlay**, **hold-Tab scoreboard**, **post-match pin**, and the **pause
menu in Standalone** (Escape is Stop-PIE in the editor — the pause menu cannot be tested in a PIE
window at all). `docs/BREACHPOINT-NEXT-TEST-HUD.md` §4b–5 is the protocol. The R7.3 cause-of-death
line and the stowed slot are in the same set.

## Log

_(terminal: the read-backs, and anything handed back)_

### 25 August 2026 — the three slots landed; PIE clean; the ring is a bar

Live editor (pid 16538) over Unreal MCP, one widget at a time, each compiled, saved and read
back before the next was touched. Every value was written with `ObjectTools.set_properties`
and re-read with `get_properties` — the before/after pairs are in the driver scripts' output;
the trees below are the fresh read after all three were saved.

**Assets modified (three, all additive except one cited slot move):**
`Content/BN/UI/WBP_BNKillfeedEntry.uasset` · `Content/BN/UI/WBP_BNScreen_Death.uasset` ·
`Content/BN/UI/WBP_BNMatchBand.uasset`. Nothing created, nothing deleted. All three still
parent to their `BN` C++ class, still declare **zero variables**, and their EventGraphs still
hold only the three disconnected UMG default stubs (PreConstruct/Construct/Tick) they had
before — R26 intact. **No colour was set anywhere**; every tint remains C++'s.

**Geometry came from the referee, not from the eye** —
`Source/BreachpointNext/UI/Content/BN/UI/Assets/00-HUD-MEASURED.md`, nodes `30:21`, `36:2`,
`42:6`. Two places it could not be transcribed literally are recorded under "Deviations".

#### Read-back (verbatim, `UMGToolSet.GetWidgetDescription`, after compile + save)

```
### WBP_BNKillfeedEntry  parent=/Script/BreachpointNext.BNKillfeedEntry  dirty=False
[0] SizeBox RootSizeBox  HeightOverride:20.000000  bOverride_HeightOverride:True
  [1] HorizontalBox EntryBox  Slot:/Script/UMG.SizeBoxSlot'/Game/BN/UI/WBP_BNKillfeedEntry.WBP_BNKillfeedEntry:WidgetTree.RootSizeBox.SizeBoxSlot_0'
    [2] CommonTextBlock LineText  Text:NSLOCTEXT("UMG", "TextBlockDefaultValue", "Text Block")  Font:(Size=14.000000)  Slot:/Script/UMG.HorizontalBoxSlot'/Game/BN/UI/WBP_BNKillfeedEntry.WBP_BNKillfeedEntry:WidgetTree.EntryBox.HorizontalBoxSlot_0'  slot:(VerticalAlignment:VAlign_Center)
    [3] CommonTextBlock KillerText  MinDesiredWidth:64.000000  Font:(Size=14.000000)  Slot:/Script/UMG.HorizontalBoxSlot'/Game/BN/UI/WBP_BNKillfeedEntry.WBP_BNKillfeedEntry:WidgetTree.EntryBox.HorizontalBoxSlot_2'  Visibility:Collapsed  slot:(Padding:(Left=8.000000), VerticalAlignment:VAlign_Center)
    [4] Image WeaponIcon  Brush:(ImageSize=(X=22.000000,Y=8.000000))  Slot:/Script/UMG.HorizontalBoxSlot'/Game/BN/UI/WBP_BNKillfeedEntry.WBP_BNKillfeedEntry:WidgetTree.EntryBox.HorizontalBoxSlot_1'  Visibility:Collapsed  slot:(Padding:(Left=6.000000,Right=6.000000), VerticalAlignment:VAlign_Center)
    [5] CommonTextBlock VictimText  Font:(Size=14.000000)  Slot:/Script/UMG.HorizontalBoxSlot'/Game/BN/UI/WBP_BNKillfeedEntry.WBP_BNKillfeedEntry:WidgetTree.EntryBox.HorizontalBoxSlot_3'  Visibility:Collapsed  slot:(Padding:(Left=4.000000), VerticalAlignment:VAlign_Center)

### WBP_BNScreen_Death  parent=/Script/BreachpointNext.BNScreen_Death  dirty=False
[0] Overlay DeathRoot
  [1] Image DeathScrim  ColorAndOpacity:(R=0.000000,G=0.000000,B=0.000000,A=0.550000)  Slot:/Script/UMG.OverlaySlot'/Game/BN/UI/WBP_BNScreen_Death.WBP_BNScreen_Death:WidgetTree.DeathRoot.OverlaySlot_0'  Visibility:HitTestInvisible  slot:(HorizontalAlignment:HAlign_Fill, VerticalAlignment:VAlign_Fill)
  [2] CanvasPanel DeathCanvas  Slot:/Script/UMG.OverlaySlot'/Game/BN/UI/WBP_BNScreen_Death.WBP_BNScreen_Death:WidgetTree.DeathRoot.OverlaySlot_1'  slot:(HorizontalAlignment:HAlign_Fill, VerticalAlignment:VAlign_Fill)
    [3] CommonTextBlock KilledByText  Text:NSLOCTEXT("UMG", "TextBlockDefaultValue", "Text Block")  Font:(Size=30.000000)  Justification:Center  Slot:/Script/UMG.CanvasPanelSlot'/Game/BN/UI/WBP_BNScreen_Death.WBP_BNScreen_Death:WidgetTree.DeathCanvas.CanvasPanelSlot_0'  slot:(LayoutData:(Offsets=(Top=276.000000,Right=0.000000,Bottom=59.000000),Anchors=(Maximum=(X=1.000000,Y=0.000000))))
    [4] CommonTextBlock WeaponText  Text:NSLOCTEXT("UMG", "TextBlockDefaultValue", "Text Block")  Font:(Size=15.000000)  Justification:Center  Slot:/Script/UMG.CanvasPanelSlot'/Game/BN/UI/WBP_BNScreen_Death.WBP_BNScreen_Death:WidgetTree.DeathCanvas.CanvasPanelSlot_1'  slot:(LayoutData:(Offsets=(Left=55.000000,Top=348.000000,Right=118.000000,Bottom=19.000000),Anchors=(Minimum=(X=0.500000,Y=0.000000),Maximum=(X=0.500000,Y=0.000000)),Alignment=(X=0.500000,Y=0.000000)))
    [5] CommonTextBlock RespawnText  Text:NSLOCTEXT("UMG", "TextBlockDefaultValue", "Text Block")  Font:(Size=17.000000)  Justification:Center  Slot:/Script/UMG.CanvasPanelSlot'/Game/BN/UI/WBP_BNScreen_Death.WBP_BNScreen_Death:WidgetTree.DeathCanvas.CanvasPanelSlot_2'  slot:(LayoutData:(Offsets=(Top=528.000000,Right=0.000000,Bottom=17.000000),Anchors=(Maximum=(X=1.000000,Y=0.000000))))
    [6] Image WeaponIcon  Brush:(ImageSize=(X=84.000000,Y=20.000000))  Slot:/Script/UMG.CanvasPanelSlot'/Game/BN/UI/WBP_BNScreen_Death.WBP_BNScreen_Death:WidgetTree.DeathCanvas.CanvasPanelSlot_3'  Visibility:Collapsed  slot:(LayoutData:(Offsets=(Left=-58.000000,Top=348.000000,Right=84.000000,Bottom=20.000000),Anchors=(Minimum=(X=0.500000,Y=0.000000),Maximum=(X=0.500000,Y=0.000000)),Alignment=(X=0.500000,Y=0.000000)))
    [7] ProgressBar RespawnRing  FillColorAndOpacity:(R=0.000000,G=0.500000,B=1.000000,A=1.000000)  Slot:/Script/UMG.CanvasPanelSlot'/Game/BN/UI/WBP_BNScreen_Death.WBP_BNScreen_Death:WidgetTree.DeathCanvas.CanvasPanelSlot_4'  Visibility:Hidden  slot:(LayoutData:(Offsets=(Top=410.000000,Right=104.000000,Bottom=8.000000),Anchors=(Minimum=(X=0.500000,Y=0.000000),Maximum=(X=0.500000,Y=0.000000)),Alignment=(X=0.500000,Y=0.000000)))
    [8] CommonTextBlock CountdownText  Font:(Size=56.000000)  Justification:Center  Slot:/Script/UMG.CanvasPanelSlot'/Game/BN/UI/WBP_BNScreen_Death.WBP_BNScreen_Death:WidgetTree.DeathCanvas.CanvasPanelSlot_5'  Visibility:Hidden  slot:(LayoutData:(Offsets=(Top=432.000000,Right=104.000000,Bottom=71.000000),Anchors=(Minimum=(X=0.500000,Y=0.000000),Maximum=(X=0.500000,Y=0.000000)),Alignment=(X=0.500000,Y=0.000000)))

### WBP_BNMatchBand  parent=/Script/BreachpointNext.BNMatchBand  dirty=False
[0] SizeBox RootSizeBox  WidthOverride:302.000000  HeightOverride:22.000000  bOverride_WidthOverride:True  bOverride_HeightOverride:True
  [1] CanvasPanel BandCanvas  Slot:/Script/UMG.SizeBoxSlot'/Game/BN/UI/WBP_BNMatchBand.WBP_BNMatchBand:WidgetTree.RootSizeBox.SizeBoxSlot_0'
    [2] CommonTextBlock MyKillsText  Text:NSLOCTEXT("UMG", "TextBlockDefaultValue", "Text Block")  Font:(Size=14.000000)  Slot:/Script/UMG.CanvasPanelSlot'/Game/BN/UI/WBP_BNMatchBand.WBP_BNMatchBand:WidgetTree.BandCanvas.CanvasPanelSlot_4'  slot:(LayoutData:(Offsets=(Left=90.000000,Top=1.000000,Right=34.000000,Bottom=20.000000)))
    [3] CommonTextBlock ClockText  Text:NSLOCTEXT("UMG", "TextBlockDefaultValue", "Text Block")  Font:(Size=14.000000)  Justification:Center  Slot:/Script/UMG.CanvasPanelSlot'/Game/BN/UI/WBP_BNMatchBand.WBP_BNMatchBand:WidgetTree.BandCanvas.CanvasPanelSlot_5'  slot:(LayoutData:(Offsets=(Left=138.000000,Top=1.000000,Right=43.000000,Bottom=20.000000)))
    [4] CommonTextBlock TopKillsText  Text:NSLOCTEXT("UMG", "TextBlockDefaultValue", "Text Block")  Font:(Size=14.000000)  Slot:/Script/UMG.CanvasPanelSlot'/Game/BN/UI/WBP_BNMatchBand.WBP_BNMatchBand:WidgetTree.BandCanvas.CanvasPanelSlot_6'  slot:(LayoutData:(Offsets=(Left=200.000000,Top=1.000000,Right=34.000000,Bottom=20.000000)))
    [5] BRRule SepLeft  Orientation:Vertical  HairlineStyle:(Edges=4)  Slot:/Script/UMG.CanvasPanelSlot'/Game/BN/UI/WBP_BNMatchBand.WBP_BNMatchBand:WidgetTree.BandCanvas.CanvasPanelSlot_7'  bOverride_Cursor:True  bCanChildrenBeAccessible:False  AccessibleBehavior:Auto  slot:(LayoutData:(Offsets=(Left=128.000000,Top=2.000000,Right=4.000000,Bottom=18.000000)))
    [6] BRRule SepRight  Orientation:Vertical  HairlineStyle:(Edges=4)  Slot:/Script/UMG.CanvasPanelSlot'/Game/BN/UI/WBP_BNMatchBand.WBP_BNMatchBand:WidgetTree.BandCanvas.CanvasPanelSlot_8'  bOverride_Cursor:True  bCanChildrenBeAccessible:False  AccessibleBehavior:Auto  slot:(LayoutData:(Offsets=(Left=190.000000,Top=2.000000,Right=4.000000,Bottom=18.000000)))
    [7] CommonTextBlock ScoreLimitText  Text:NSLOCTEXT("UMG", "TextBlockDefaultValue", "Text Block")  Font:(Size=7.000000)  Slot:/Script/UMG.CanvasPanelSlot'/Game/BN/UI/WBP_BNMatchBand.WBP_BNMatchBand:WidgetTree.BandCanvas.CanvasPanelSlot_9'  slot:(LayoutData:(Offsets=(Left=240.000000,Top=14.000000,Right=44.000000,Bottom=9.000000)))
    [8] ProgressBar SelfScoreBar  FillColorAndOpacity:(R=0.000000,G=0.500000,B=1.000000,A=1.000000)  Slot:/Script/UMG.CanvasPanelSlot'/Game/BN/UI/WBP_BNMatchBand.WBP_BNMatchBand:WidgetTree.BandCanvas.CanvasPanelSlot_0'  Visibility:Hidden  slot:(LayoutData:(Offsets=(Left=24.000000,Top=7.000000,Right=60.000000,Bottom=8.000000)))
    [9] ProgressBar TopScoreBar  FillColorAndOpacity:(R=0.000000,G=0.500000,B=1.000000,A=1.000000)  Slot:/Script/UMG.CanvasPanelSlot'/Game/BN/UI/WBP_BNMatchBand.WBP_BNMatchBand:WidgetTree.BandCanvas.CanvasPanelSlot_1'  Visibility:Hidden  slot:(LayoutData:(Offsets=(Left=240.000000,Top=7.000000,Right=44.000000,Bottom=8.000000)))
```

`is_dirty` is **False** on all three: what is on disk is what is above. Compile returned
`True` for each and the editor log shows no `required widget binding`, no `did not resolve`
and no Blueprint-compiler line at all in the window — the only warnings logged while this
work ran were `LogJson: Property "OnAllFontFacesFinishLoading" ... unhandled during Json
schema generation`, which is the MCP `list_properties` call describing a delegate, not us.

#### PIE, solo — the binds resolve

Ran PIE in-viewport on the loaded level (`/Game/Maps/BR_Arena01`, the only map in the project
— `/Game/BN/Maps/L_BNTest` from `Tools/bn/setup_r1_testmap.py` does not exist here), then
stopped it; the editor is back where it was, PIE not running, level not changed. The whole
log delta for the run:

```
[19.27.25:371] LogAudioMixerAudioUnit: Warning: Error querying Sample Rate: 2003332927
[19.27.25:397] LogBN: BNUI: root layout up for LocalPlayer_2.
[19.27.25:398] LogBN: BNUI: HUD up for LocalPlayer_2.
```

Zero `did not resolve`, zero `placed no rows`, zero binding warnings; the audio line is
CoreAudio and predates this ticket. **This proves the six new slots bind — it does not prove
they look right.** `EditorAppToolset.CaptureViewport` returns the LEVEL EDITOR viewport, not
the PIE client, so the capture came back as grey blockout with a gizmo and no HUD. The fourth
box stays unticked: nobody has seen the death overlay or the band's bars on a screen.

#### Deviations from a literal reading of the step list, both deliberate

1. **The killfeed's x8 / x78 / x110 are reached with padding, not a canvas.** `EntryBox` is a
   `HorizontalBox`; converting it to a `CanvasPanel` to get absolute x would have rebuilt the
   one widget the pool instantiates most, and a `CanvasPanel` under a `SizeBox` with no width
   override collapses to zero desired width. Instead: `KillerText` slot pad L8 +
   `MinDesiredWidth 64` puts the existing 6px-padded 22-wide glyph at **x78**, and
   `VictimText` slot pad L4 puts it at **x110** — the measured columns, and a name longer than
   64 pushes the row instead of overlapping the glyph. Child order is
   `LineText · KillerText · WeaponIcon · VictimText`; in the fallback layout C++ collapses
   Killer/Victim, and a Collapsed child takes zero space in an `SBoxPanel`, so the row still
   draws exactly `[LineText][6][glyph]` — today's HUD, unchanged. **`WeaponIcon`'s slot was
   not touched.**
2. **`WeaponText` moved 59 px right, from centred-ON x636 to starting AT x636.** This is the
   one non-additive edit and it is cited: `36:8` measures the killing-weapon group as
   silhouette `36:9` at **x540 84×20** + name `36:10` at **x636 118×19**, the pair centred on
   640. The WBP had the name centred on 636 (i.e. spanning 577..695), which was fine while it
   was alone but overlaps the incoming silhouette. Slot offset Left `-4` → `55` under the
   asset's existing centre-anchor idiom. **To revert: set that one offset back to `-4`.**

#### The respawn ring, as built

`RespawnRing` is a `UProgressBar` **104 × 8 at y410** — the ring box's own top edge, inside its
104×104 footprint (`36:12/11`) and clear of the numeral at y432 (`36:13`). When the radial
material lands it reclaims the full box under the same bind name with no layout churn and no
C++ change, exactly as the ticket's note describes.

#### Handed back / new findings

- **`ProgressBar` fill colour is nobody's.** Both new bars and the ring read back
  `FillColorAndOpacity:(R=0, G=0.5, B=1, A=1)` — the ENGINE default blue, not `--shield
  #35D0F2`. A WBP may not type a colour (ASSET-RULES §5), and neither `UBNScreen_Death` nor
  `UBNMatchBand` sets a fill on the bars it binds. Left engine-default deliberately rather
  than smuggling a hex into an asset. **This is a C++ gap in the same family as gap 7** and it
  will be visibly wrong the first time a human sees a score bar.
- **`CountdownText` font size 56 is INFERRED.** `36:13` measures the box (104×71) and the
  design says LARGE; the referee records no type size for it. 56 fits the box against the
  30 pt killer line above.
- **contract_gap (non-blocking, no work was blocked):** this packet's `owner_path` is
  `Content/BN/AI/ Content/BN/Data/ Content/BN/UI/ docs/tickets/`, which excludes `Tools/bn/`
  — so the three idempotent driver scripts that produced the trees above could not be
  committed where every other editor script in this project lives. They were not smuggled in
  through a different tool. They sit in this session's scratchpad as `bn11_lib.py`,
  `bn11_killfeed.py`, `bn11_death.py`, `bn11_matchband.py` and want a `git mv` into
  `Tools/bn/` by whoever holds that path. Each re-runs clean: `ensure()` skips a widget that
  is already in the tree and every `set_properties` is a fixed target value.
- **MCP tool defect worth writing down:** `ObjectTools.set_properties` declares `values` as a
  **JSON string**, not an object. Passing a dict returns `False` and writes NOTHING — no
  error, no log line. The first pass of the killfeed script did exactly that; only the
  read-back caught it. Anything driving this toolset must `json.dumps` the values and must
  never trust the return code alone.
- Still handed to the FOUNDER, untouched by this ticket: the four unrun read-backs named in
  the section above (death overlay, hold-Tab scoreboard, post-match pin, pause menu in
  Standalone), plus now the **on-screen** confirmation of these six slots.
