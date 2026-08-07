# HANDOFF — session of 4 August 2026

---

> ## UPDATE — 7 August 2026: there IS a board again, and it is the gameplay rework
>
> The statement below that "there is no ticket board" was true on 4 Aug and is **no longer
> true**. `docs/tickets/` now holds **twelve tickets, BP90–BP102**, cut from
> `docs/BREACHPOINT-GAMEPLAY-REWORK.md`. Read that document first — it is the architecture and
> the road in one place, and every ticket's steps assume it.
>
> **Start at `TICKET_BP90_DEMOLITION.md`.** It is the root: it deletes the 36 UE-template files
> still compiling inside the runtime module (`Variant_Horror/`, `Variant_Shooter/`,
> `breachpoint{Character,GameMode,PlayerController}` — verified as referenced by nothing in the
> `BR` tree), and it settles the three decisions (D-1 projectile Tick exception, D-2
> `Weapons/`→`Equipment/`, D-3 AI scope) that later tickets depend on. **No Phase-1 ticket
> starts until BP90 is DONE.**
>
> Scope of the rework is **gameplay only** — Core, Data, Input, AbilitySystem, Character,
> Equipment, Match. `UI/`, `AI/`, `Online/`, `Telemetry/` are untouched and meet the new layer
> at named seams.
>
> Everything below this block still stands: the ladder state (rung 1 PARTIAL by environment,
> and structurally cannot be more on an Epic Launcher install), the `guard_laws.py` shell
> bypass, and the unwired UI sounds. **BP90 is a shell-driven deletion packet, so its
> owner-path confinement is advisory** — that is the gap named below, firing for real.

---

> **NAMES CHANGED 4 Aug 2026, after this document was written.** `UBRMenuRow` is now
> `UBRButton`, `EBRMenuRowType` is `EBRButtonType`, and `BRMenuRow.h` / `BRSettingsRow.h` /
> `BRButtonStyles.h` / `BRHighlightButton.h` are one file: `Components/BRButton.h`. The old
> names are left below **deliberately** — this is a dated record and rewriting its findings
> would make it a different document. See `BUTTON-MODULE-LEDGER.md`.
Read this first. The previous handoff described the 31 Jul → 1 Aug world; almost everything it
routed you to no longer exists, so it has been replaced rather than amended.

**Read the board, do not trust a list of it.** This paragraph has now been wrong twice in
three days — first saying the board was empty (it had two tickets within hours), then saying it
had two (a thirteen-ticket gameplay rework landed the same afternoon). So: **`ls docs/tickets/`
is the board.** What is stable is the ORDERING, and only that is written here.

**Two independent tracks. They do not block each other.**

| Track | Tickets | Order |
|---|---|---|
| **UI / buttons** | `BP80` → `BP79` | BP80 step 1 is a compile gate and it gates BP79 too — BP79's `ApplyPlateMaterialState` pointer now lives in `BRButton.cpp`. `BP81` (glyph backfill) is **blocked** and released its claim. |
| **Gameplay rework** | `BP90` → `BP91` … `BP102` | `BP90` is the root: *"nothing in Phase 1+ starts until this is DONE."* Read `docs/BREACHPOINT-GAMEPLAY-REWORK.md` first — it supersedes `BREACHPOINT-ARCHITECTURE.md` §3.1–3.6 and §3.11 for the folders it names. |

The rework is scoped to gameplay only — Core, Data, Input, AbilitySystem, Character, Equipment,
Match. **`UI/` is explicitly untouched and meets the new layer at named seams**, so the button
work and the rework can run in parallel without a merge fight.

**`BP80` is the handoff for the whole 4 Aug button session.** Its STATUS block lists the eight
commits that landed files-only and unverified; its Log carries the three things to know before
touching anything: nothing has compiled, `WBP_ButtonDefault` will legitimately look different,
and the node counts must not be "optimised" because that reduction was retracted on evidence.

Also read `docs/DECISIONS-OWED.md` — six founder calls (B1–B6) from that session are waiting,
and all six are answerable from a desk with no engine.

**There is no claim file.** `.claude/active-packet.json` does not exist, so `guard_laws.py`
enforces no `owner_path` at all. Its banned-API checks (law 2/3 — `TakeDamage`, direct attribute
setters, loose tags) still fire on every `Edit`/`Write`. Write a claim on pickup only if you want
the confinement back.

---

## What landed this session

**The Menu Row button set — eight WBPs under `Content/UI/Components/Buttons/`.**
`WBP_ButtonDropDown` · `DigDown` · `IconOnly` · `Slider` · `Checkbox` · `Radio` · `MapVoting` ·
`Image`, measured 1:1 against Figma `12:724`, all parenting to `UBRMenuRow`. `Disabled` is
deliberately not an asset — it is a Status, not a body, so it would be a byte-identical copy.

Built by `mcp-ui/gen_ui/wbp_plan.py` + `build_wbp.py`, which gained two capabilities: a
`class_defaults` hook (for `RowType` and `Style`, which live on the CDO, not on any tree node)
and per-node `properties` (exact text, SizeBox dimensions).

`UBRMenuRow` gained five things, each closing a defect an audit found — none speculative:
- `ApplyInversionToSubtree` + `TypeBody` — per-type bodies were staying white on the plate that
  had just gone white, i.e. vanishing exactly when the row is focused.
- `TypeCheckMark` — the checkbox tick and radio fill were painted permanently; measured, Idle
  and Hover are an EMPTY box and only Active carries the mark.
- `InversionExempt` — the slider handle keeps a white ring on the inverted plate.
- `DesignSizeMode = Desired`, set in the constructor. That is the ONLY place it can be set: it
  is `WITH_EDITORONLY_DATA` with a bare `UPROPERTY()`, invisible to the details panel, to
  `ObjectTools.set_properties`, and to config.
- A design-time-only width of 250 so the preview matches the component board, cleared at runtime
  so the row still fills its 349/536 rail.

**Five textures** generated by `mcp-ui/gen_ui/gen_menurow_art.py` through the committed
`svg_pillow` rasteriser: hatch, disclosure triangle, Dig Down arrows, slider handle, slider tick.
Numbers measured, not derived — the hatch is 110 wide because per-column stddev on the reference
render decays to zero at exactly x=110.

---

## What was removed, so you do not go looking for it

| Gone | What it was |
|---|---|
| the whole ticket board | 46 tickets + archive |
| `Tools/architect/`, `audit_blueprints/`, `audit_ui/`, `validate_data_tags/`, `bus/`, `data-crew/` | ~8,000 lines nothing invoked |
| `Tools/verify_notices.py`, the notices records, `Content/Legal/` | attribution apparatus; R41 withdrawn with it |
| `Tools/ue_mcp/`, `Tools/rename_r26/` | planning docs; a finished one-shot migration |
| `docs/BUILD-STATE.md`, `docs/WORK-ROUTING.md`, `docs/bus/`, `ue-frontend/TICKETS.md` | snapshots of a board and tooling that no longer exist |
| `docs/ui/receipts/` | 2.7 MB of build logs; now gitignored, still written locally |

**R41 is withdrawn.** Content adoption is not gated in-repo. No reviewer raises a licence
question and no finding is filed on those grounds.

---

## The state of the ladder

**Rung 1 is PARTIAL, and structurally cannot be more on this machine.** `BreachpointEditor`
compiles and links. This is an Epic Launcher install with no server binaries, so
`BreachpointServer` cannot link and `run-ubt.sh` reports PARTIAL rather than a false green. A
source-built engine is what changes that.

**Rung 2 runs again.** It was briefly broken this session — `testing.md` invoked a script that
had just been deleted — and that row is now gone from the gate table.

**No rung-4 claim exists for the button work, and none is owed yet.** These are standalone
component assets with no host screen, so hover / press / click have never been exercised at
runtime. The 8/8 audit that passed is a DATA audit — tree membership, exact label strings,
`RowType`, `Style` — read back out of the running editor. Six of the eight were also rendered
and looked at.

---

## Two things genuinely open

**1. `guard_laws.py` is a tool-call guard, not an owner-path guard.** It hooks
`Edit|Write|MultiEdit|NotebookEdit` only, so a shell `rm`, `mv` or `>` redirect bypasses it
entirely — demonstrated accidentally this session. Either hook `Bash` and parse the command, or
say plainly in `CLAUDE.md` that law 5 is advisory for anything shell-driven. Full write-up in
`docs/DECISIONS-OWED.md`.

**2. Sounds are unwired.** `UBRButtonStyle_MenuRow` carries `PressedSlateSound` /
`ClickedSlateSound` / `HoveredSlateSound`, and the project has no UI audio at all — the only
`S_*` in `Content/` is a controller-icon texture. Nothing was invented to fill the gap.

---

## Where the UI work continues

`docs/ui/COMPONENT-SPECS.md` and `Content/UI/Components/Buttons/Assets/02-MenuRow.md` are the
measured sources; the `ui-presentation` and `ue5-ui-architecture` skills are the method.

Three findings in `02-MenuRow.md` remain open: Map Voting's "Winning" state (a C++ gap — there
is no winning state and no bind for one), Drop Down's Active hatch (same reason — the hatch has
no bind, so C++ cannot show or hide it per state; Dig Down gets it statically because Active is
its only state), and `Slider Row Wide`, whose palette sits outside the token system entirely.
