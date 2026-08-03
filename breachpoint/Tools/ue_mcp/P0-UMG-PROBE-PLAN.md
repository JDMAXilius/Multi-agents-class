# P0 probe plan — can the UE 5.8 MCP author a WBP?

**R37.1 artifact.** This file is the committed plan. Nothing may be called against a live editor
until it exists; the receipt that answers it is `docs/ui/receipts/P0-umg-probe-<UTC>.md`.

**Status:** written 2 Aug 2026, editor CLOSED, before the first call.
**Supersedes nothing.** It ANSWERS the three unfired probes in `TERMINAL-VS-EDITOR.md` §5.4 and
the standing unticked Kickoff item at `SURFACE.md:446`.

---

## 0. Why this exists

`TERMINAL-VS-EDITOR.md` §5.1 records, from enumeration against a live editor on 1 Aug 2026:

> **Can it author a WBP widget hierarchy?** **No.** *"**No Slate/UMG/widget toolset.** …
> **BP10's WBP layout assets stay Tier-4 human work.**"*

That was true of the server enumerated then. It is **not obviously true of the first-party UE 5.8
MCP** (`Engine/Plugins/Experimental/ModelContextProtocol`, "Anthropic MCP server implementation for
Unreal Engine", Epic Games), whose sibling toolset tree contains:

| Plugin | Mac binary | AI-callable surface, read from the header |
|---|---|---|
| `UMGToolSet` | present | 23 statics: `CreateWidgetBlueprint`, `AddWidget`, `MoveWidget`, `RemoveWidget`, `RenameWidget`, `WrapWidgets`, `SetNamedSlotContent`, `ToggleWidgetAsVariable`, `GetWidgets`, `GetWidgetDescription`, `ListWidgetClasses`, `CompileWidgetBlueprint`, … |
| `MVVMToolset` | present | `CreateViewModel`, `AddViewModelProperty`, `AddViewModelToWidget`, `CreateViewBinding`, `ListWidgetViewBindings` |
| `ConfigSettingsToolset` | present | `GetSectionSchema`, `GetSectionPropertyValues`, `SetSectionProperties`, `SaveSection` |
| `SlateInspectorToolset` | present | `Snapshot`, `Observe`, `Screenshot`, `Click`, `Hover`, `Type`, `PressKey`, `Drag`, `WaitFor`, `FillForm` |

All four were **disabled** in `Breachpoint.uproject` and were enabled in the same commit as this
plan. **Reading a header is not firing a tool.** This plan exists to convert the header reading
into a measured fact.

## 1. The question that actually decides the roadmap

Not *"can it create a WBP"* — `BlueprintTools.create` was already suspected of that.

> **Can it set a `UPanelSlot` property — anchors, offsets, alignment, padding — and a `UWidget`
> appearance property?**

`UMGToolSet` exposes **no `SetProperty`**. If no tool anywhere reaches a slot, `AddWidget` produces
an untuned tree: every child at the default anchor with zero offsets, which is not a layout and
cannot be made into one from a script. In that case the roadmap's P3 stays hand-authored and
`TERMINAL-VS-EDITOR.md` §5.1's verdict survives with a corrected reason.

**Everything downstream branches here.** ~40 WBPs are either generated from a committed plan
(reviewable text, regenerable, law-7-shaped) or hand-authored in an editor (binary, unreviewable,
one owner at a time). Do not schedule P3 before this receipt exists.

## 2. Preconditions, checked in this order

R36 forces the ordering; it is not satisfiable in any other sequence.

1. Editor CLOSED — `pgrep -fl "MacOS/UnrealEditor "` empty.
   **Note:** `run-ubt.sh:86` greps bare `UnrealEditor` and false-positives on the macOS
   `UnrealEditorServices` launch agent. Its warning is not evidence an editor is open.
2. `Tools/run-ubt.sh` run and its result recorded **verbatim, including failures**.
   **Known and accepted:** this is an Epic Launcher install (no
   `Engine/Build/SourceDistribution.txt`), so `BreachpointServer` fails with *"Server targets are
   not currently supported from this engine distribution"* and the script exits 1.
   **This is PARTIAL, not rung 1, and every claim resting on it says PARTIAL.** The editor target
   — the only one the MCP needs — is green with a touched-binary proof.
3. No Live Coding poison: no `Binaries/Mac/*.patch_*`.
4. Editor opened. **One instance, one driver** (R29.1/R29.2). This session is the driver.
5. Receipt file opened and its header written **before call #1**, and flushed per line.
   *"A receipt reconstructed from memory afterwards is not a receipt."*

## 3. The calls, in order, with what each answers

Every call records: name, arguments, return value, and **failures marked `**FAILED**`**.
A refusal is a result. Nothing here is retried silently.

### Group A — does the transport expose the toolsets at all?

| # | Call | Answers |
|---|---|---|
| A1 | `list_toolsets` | Do the four newly-enabled toolsets appear? `bEnableToolSearch=true` is the default, so `tools/list` returns only `list_toolsets`/`describe_toolset`/`call_tool` and the rest are discovered on demand. |
| A2 | `describe_toolset("UMGToolSet")` | Are all 23 functions surfaced, or does `AICallable` need a second gate? |

**If A1 does not list `UMGToolSet`, stop.** Everything below is void and the finding is
*"enabled in the .uproject, absent from the MCP"* — which is a different and equally important
answer.

### Group B — read before write, against an asset we did not create

| # | Call | Answers |
|---|---|---|
| B1 | `GetWidgets("/Game/UI/WBP_RootLayout")` | **The oldest open question in the front end.** `UBRRootLayout` declares four non-optional `BindWidget` `UCommonActivatableWidgetStack`s — `GameLayerStack`, `GameMenuLayerStack`, `MenuLayerStack`, `ModalLayerStack` (`BRRootLayout.h:35-45`). A WBP lacking them does not compile. BP18's only evidence is a bus message; **nobody has ever opened it.** |
| B2 | `GetWidgetDescription("/Game/UI/WBP_RootLayout")` | Richer per-widget detail — does it expose slot data (the §1 question, read-side)? |
| B3 | `GetWidgets` on `WBP_HUDLayout`, `WBP_KillfeedEntry` | Same, for the other two. `UBRHUDLayout` needs `KillfeedContainer` (`BindWidgetOptional` — a miss is a silent null, not a compile error, and therefore easier to miss). |

**B1–B3 are read-only and are worth the session on their own**, independent of everything below.

### Group C — the write probe, in a throwaway namespace

Everything lands under `/Game/UI/_Probe/`. **Nothing under `_Probe` is ever committed** — it is
deleted in step C7 and its absence is asserted. Names are chosen at creation because BP18 proved
**assets cannot be renamed** through the MCP (the rename modal auto-cancels).

| # | Call | Answers |
|---|---|---|
| C1 | `CreateWidgetBlueprint("/Game/UI/_Probe", "WBP_Probe", UBRActivatableWidget)` | Can it create a WBP **parented to one of OUR C++ classes**, not just `UUserWidget`? |
| C2 | `GetWidgets` on the result | What does an MCP-created WBP contain by default — is there a root panel, or nothing? |
| C3 | `AddWidget(WBP_Probe, CanvasPanel, "RootCanvas")` | Can it add a panel at the root? |
| C4 | `AddWidget(WBP_Probe, Border, "TestBorder", parent=RootCanvas)` | Can it parent into a panel — i.e. is a real tree constructible? |
| C5 | **The decisive one.** Attempt to set `TestBorder`'s `UCanvasPanelSlot`: `Anchors`, `Offsets`, `Alignment`; and the Border's `Brush.TintColor`. Try, in order: any property tool found in `describe_toolset` output; `SetNamedSlotContent`; `ToggleWidgetAsVariable` + a C++-side read. | **§1.** Record the exact failure text if there is no route. |
| C6 | `CompileWidgetBlueprint(WBP_Probe)` | Does an MCP-built tree compile? A tree that cannot compile is not a layout. |
| C7 | Delete `/Game/UI/_Probe` and re-`ListWidgetBlueprints` to prove it is gone | Leave no probe asset behind. |

### Group D — the adjacent toolsets, cheapest useful call each

| # | Call | Answers |
|---|---|---|
| D1 | `ConfigSettingsToolset.GetSectionSchema` for `[/Script/Breachpoint.BRUISettings]` | Can P1's ini wiring be scripted **and validated against the real schema** rather than typed blind? |
| D2 | `MVVMToolset.ListWidgetViewModels(WBP_HUDLayout)` | Read-only. Does the MVVM surface see our ViewModels? |
| D3 | `SlateInspectorToolset.Snapshot` on the editor window, `MaxDepth` small | Does the live Slate tree read back? If yes, **gamepad-parity and focus-routing acceptance tests become scriptable** — today they are "a human with the mouse unplugged". |

**D1–D3 are read-only.** `SetSectionProperties`/`SaveSection` are NOT called in this probe.

## 4. What this probe does NOT do

Stated so the receipt cannot be read as more than it is.

- **No asset is landed.** `_Probe` is created and deleted. `Content/UI/` is unchanged.
- **No layout is authored.** Proving `AddWidget` works is not proving a WBP can be built well.
  Whether a generated layout is *good* is a human judgment (`ui-presentation` §11) and no tool
  answers it.
- **No rung is claimed.** This is inspection. `SURFACE.md`'s own honesty note applies: a mark
  derived from a tool's declared schema is a plan, not a fact — only a fired call is evidence.
- **No `Content/Data/` write, ever.** `SURFACE.md:401-407`: `write_file` can overwrite
  `DT_Weapons.csv` with law 5 never firing, because an MCP call has no `file_path` for
  `guard_laws.py` to see. UI work does not write tuning data.
- **No write outside `<project>/Content` or `<project>/Saved`.** The MCP's write confinement
  enumerates ~80 allowed roots and most are inside the **engine install** — outside the repo,
  invisible to git, no hook, no diff.

## 5. The two outcomes, written before the answer is known

So the result cannot be rationalised after the fact.

**If C5 succeeds** — P3 becomes a generator in the split `Tools/render_weapons/` already proved:
`wbp_plan.py` (pure CPython, no engine, self-testable) → executor (imports the MCP surface) →
`selftest_no_editor.py` → an R21-guarded wrapper. The reviewable artifact is the plan, the asset is
regenerable, and law 7's *"generated by committed scripts, never hand-placed"* finally covers UMG.
`TERMINAL-VS-EDITOR.md` §5.1 is corrected with this receipt as the evidence.

**If C5 fails** — the roadmap is unchanged and better-grounded: WBPs stay Tier-4 human work, and
§5.1's verdict is re-confirmed with a specific reason (*"a widget tree is constructible; a slot is
not reachable"*) instead of *"no toolset exists"*, which is now false. The generator idea is
recorded as blocked-on-a-property-setter and does not get re-proposed every quarter.

**Either way the receipt is the deliverable**, and it is committed with any asset change in the
same commit.
