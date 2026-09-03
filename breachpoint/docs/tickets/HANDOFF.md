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

> ## UPDATE — 24 August 2026: BN work is ON THIS BOARD now
>
> **The defect this fixes:** `/tickets list` globs `docs/tickets/*.md` and nothing else, while
> every BreachpointNext ticket has lived at `docs/BREACHPOINT-NEXT-TASK-*.md` since R2. So BN
> work has never appeared on the board at all. Two consequences, both real and both recent:
>
> 1. The terminal has been claiming BN work under invented names — `active-packet.json` on
>    24 Aug read `BREACHPOINT-NEXT-TASK-TOOLING-AND-R10-DOCS`, a ticket that does not exist as a
>    file. A claim that names no file cannot be listed, handed over, or closed.
> 2. **R10's blocker went undetected for two days.** Four packets of bot behaviour were
>    compiled and switched off (a stale `ST_BNBot`), and the only place that said so was a
>    roadmap page nobody was told to read. It was found by the terminal reading `docs/` on its
>    own initiative, which is not a process.
>
> **From now on: BN tickets are cut into `docs/tickets/` as `TICKET_BN<n>_<NAME>.md`**, in the
> template every other ticket uses, with a `requires:` line so the board says up front whether a
> session can even take it (`files-only` / `engine-installed` / `editor-live`).
>
> **CORRECTED 25 Aug 2026.** This block originally said the older
> `docs/BREACHPOINT-NEXT-TASK-*.md` files were NOT being moved. They have since been moved, at
> the founder's request, to **`docs/archive/`** — all seventeen, by `git mv`, so every Log and
> every line of history is intact and `git log --follow` still reaches it. Nothing was rewritten;
> only the folder changed. Treat them as **closed or historical**; anything still live from one
> of them gets a new ticket here that names it.
>
> **Open BN tickets as of 24 Aug:** `TICKET_BN10_BOT_ASSETS` (editor-live — the R10 blocker, and
> the highest-value action outstanding) and `TICKET_BN11_HUD_SLOTS` (editor-live, additive).
>
> **Added 25 Aug:** `TICKET_BN12_NAVLINK_PROBE` (**engine-installed, no editor**) — two engine
> questions that gate all bot traversal work: the fields of `FNavLinkGenerationJumpDownConfig`,
> and whether `ARecastNavMesh::GetDebugGeometry` is callable from a non-editor target. Founder
> ruling 25 Aug: bots derive traversal **from the navmesh**, and **nothing is placed in the level**
> beyond the nav bounds volume. BN12 writes no code — it answers and stops, and BN13 cannot be
> written honestly until its Log exists.
>
> **All seventeen old BN task docs were audited the same day and every one is CLOSED.** Five had
> no status line at all — `AIM-GRAPH-AUDIT`, `CUE-BLUEPRINTS`, `HIT-REACTIONS`,
> `R3-W3-MELEE-GRENADE`, `R7-WBP-HUD` — so their state was unknowable at a glance even though
> each Log said plainly it was finished. They now carry one, stamped from the evidence already in
> their own Logs plus a check that the assets exist at the ini paths. Nothing was marked done on
> a guess: where a Log recorded no read-back, the status line says which check stood in for it.
>
> **So the audit found nothing still live** beyond what BN10 and BN11 already cover — with one
> exception that is the FOUNDER's, not a ticket's: four `R7-WBP-HUD` read-backs need a hand on a
> keyboard (death overlay, hold-Tab scoreboard, post-match pin, and the pause menu, which can only
> be tested in STANDALONE because Escape is Stop-PIE in the editor).

---

> ## UPDATE — 25 Aug 2026: the AIBOT track exists, with its own crew
>
> Founder directive: rework the bot AI **from scratch** as a self-contained module
> `Source/AIBot/` (prefix `AIB`, log `LogAIBot`) — Halo Infinite's bot architecture 1:1,
> plugin-shaped, with BreachpointNext as test harness only. The roadmap, prime decisions,
> phases, five proofs and file tree are `docs/AIBOT-ROADMAP.md` — read that before touching
> anything AIB. The BN bot track (`Source/BreachpointNext/AI/`) stays live behind the
> coming `BotSystem=AIB|BN` switch until AIB beats it in an A/B match.
>
> **New crew, one owner per artifact:** `aib-builder` (Source/AIBot/ only, boundary grep is
> law), `aib-critic` (containment · fairness · utility pathologies · server-only; opus),
> `aib-editor` (Content/AIBot/ + Tools/aib/, probe gates build), `aib-verifier` (specs, PIE
> counting protocols, the A/B match). The adapter folder
> `Source/BreachpointNext/AIBotAdapter/` is **bn-builder's**, not aib-builder's.
>
> **Wave dispatch is standing doctrine** (`docs/AIBOT-WAVES.md`, 25 Aug): reads parallelize,
> writes serialize unless the packet names disjoint file lists; aib-editor is wave-exempt
> (editor state is global); every wave ends at a barrier with a real merge step. The wave
> map is phase-by-phase in that doc — Phase 4's four skill policies are the W-BUILD showcase.

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

## Decisions the founder must make (3 Sep 2026, from the Phase 16 audit and the phase reviews)

These sit outside every AIB owner path or behind a closed ruling; nothing lands until one of
them is answered.

1. **Retire the BR-era bot system** (`Source/Breachpoint/AI/`, `Content/AI/ST_Bot`,
   `Content/Data/DT_BotAmbitions|DT_BotTuning` + csv, the `BRDataRows.h` row structs): dead and
   unreferenced, but under `ai-builder`'s owner path and DESIGN-RULINGS BP103. Needs a dated
   ruling + a named owner. Recommended: yes, same shape as the BN retirement (AIB27).
2. **Retire `ai-builder.md`** (describes code that does not exist) and re-route the 20
   documents/agents that name it. Recommended: yes.
3. **Test contract naming**: `testing.md:108` and `QUALITY-BARS:60` say `Breachpoint.Bots.*`;
   the real suites are `AIBot.Sim.*` (19 specs today). Recommended: amend the two contract lines
   (one edit each) rather than renaming 19 suites and every citation.
4. **Gauntlet**: `BRGauntlet.SmokeTS2C` was never authored, the wrapper is Win64-only, and the
   dedicated-server rung cannot build on the launcher engine (BreachpointServer: "Server targets
   are not currently supported from this engine distribution"). Decide: source-built engine
   machine for the server rung, or amend the ladder to listen-server only for now.
5. **Phase 14 lane geometry**: lane volumes are placed by script from the blockout's corridor
   names; the founder should confirm the 3–6 lanes per map read as the intended routes (a
   10-minute PIE look with `show Navigation`).
