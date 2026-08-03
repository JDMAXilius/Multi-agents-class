# TICKET — BP26: CommonUI foundation — the layers are built and nothing mounts them

> STATUS: open — cut from `docs/ui/ue-frontend/TICKETS.md` BP26 (the parent proposal) plus plan
> `curried-booping-waffle.md` P1, 2 Aug 2026. Deps: none. The first executable packet of the
> front-end board; every screen and component packet is behind it.

Founder directive: BP10 step 1 landed seven sound C++ units under `Source/Breachpoint/UI/` and
three WBPs under `Content/UI/`, and **nothing on disk connects them.**
`UBRUIManagerSubsystem::CreateLayoutForLocalPlayer` (`BRUIManagerSubsystem.cpp:233`) is called by
nothing; `UBRVM_Combat::BindToAbilitySystem` and `UBRVM_Match::SetTimeSource` are called by
nothing; the six `TSoftClassPtr` slots on `UBRUISettings` point at nothing. This packet puts one
pixel on screen and proves input routes through the layer stack. **It does not rewrite a single
existing class** — `UBRUIManagerSubsystem` + `UBRRootLayout` are the correct hand-roll of what
Lyra gets from plugins that are not in the engine (plan §1.4).

Law 3 binds hard: **config over asset.** `CommonUISettings`/`CommonInputSettings`,
`UserInterfaceSettings` and the six screen slots land as text in `Config/`, not as a Blueprint and
not as a data asset. Law 4 binds: every class stays `meta = (DisableNativeTick)`.

**Ordering law:** Step 1 (config) gates step 2 (the mount path) — `CreateLayoutForLocalPlayer`
short-circuits with empty soft slots, so mounting cannot be proven before the slots are filled.
Steps 1–2 gate the rung-3 exit in "Done when". Step 5 (`UCommonUIInputData`) is independent of
1–4 and may run in parallel; its **asset** half is BLOCKED — see Notes, standing question ①.

## Kickoff (machine-checkable — the tickets skill verifies these BEFORE a claim)

- requires: engine-installed
  <!-- The proposal noted "+ one editor-live step for the CommonInput controller data assets".
       That step is NOT in this packet: `UCommonInputBaseControllerData` is a data asset with no
       C++ path and is not on `BREACHPOINT-AUTHORING-MATRIX.md` Tier 4's closed list. Law 7's
       standing question has no answer on file. File it; do not create the asset and argue after.
       The whole packet therefore executes engine-installed, editor CLOSED. -->

- The editor is CLOSED at claim time and stays closed (R36/R21 — nothing else holds the build
  lock while this runs).
- `Tools/run-ubt.sh` is green on **all three targets** with an R19 timestamp proof, taken at claim
  time — this is the baseline this packet must not break.
- The mount gap is still open: `grep -rn "CreateLayoutForLocalPlayer" Source/` returns exactly the
  declaration (`BRUIManagerSubsystem.h:48`) and the definition (`BRUIManagerSubsystem.cpp:233`) and
  **no call site**. Same for `BindToAbilitySystem` (`BRViewModels.h:25`) and `SetTimeSource`
  (`BRViewModels.h:169`). If any already has a caller, re-read before writing — someone landed
  ahead of you.
- The three WBPs are real assets, not Git LFS pointer stubs: `Content/UI/WBP_RootLayout.uasset`,
  `WBP_HUDLayout.uasset`, `WBP_KillfeedEntry.uasset` each exceed 1 KB and open in the editor.
- owner_path: `Source/Breachpoint/UI/`, `Config/DefaultGame.ini`, `Config/DefaultInput.ini`,
  `Config/DefaultEngine.ini`

## Steps (in order)

1. **Config, all of it text.** Owner: **ui-builder**. Contracts: `data-and-assets.md` (soft class
   refs; config over asset), R26's closing preference (`Config/DefaultGame.ini` wherever it works).
   a. `[/Script/Breachpoint.BRUISettings]` in `Config/DefaultGame.ini` — the six `TSoftClassPtr`
      screen slots pointed at real classes. This is the live short-circuit: three WBPs exist and
      nothing points at them.
   b. `CommonUISettings` / `CommonInputSettings` in `Config/DefaultGame.ini` — **config, not a
      Blueprint.**
   c. `[/Script/Engine.UserInterfaceSettings]` in `Config/DefaultEngine.ini`:
      `UIScaleRule=ShortestSide` + curve `720→1.0, 1080→1.5, 1440→2.0` (plan P1.2). Every asset is
      authored once at 1280×720 and no ×1.5 arithmetic ever appears in a WBP.
   d. **Verify, do not re-add:** `GameViewportClientClassName=/Script/CommonUI.CommonGameViewportClient`
      is **already present** at `Config/DefaultEngine.ini:143` (landed under BP18). Confirm it is
      still there and that `LogUIActionRouter` emits no "without a CommonGameViewportClient" error;
      that is the whole of the proposal's viewport-class item.
2. **The mount path (plan P1.3, decision D2).** Owner: **ui-builder**. `UBRUIManagerSubsystem`
   subscribes to `ULocalPlayer::OnPlayerControllerChanged` and creates the layout when a PC
   arrives. **Keep the fix inside `UI/`** — calling `CreateLayoutForLocalPlayer` from
   `BRPlayerController` is outside this owner path and is a `contract_gap`, not an edit.
3. **Feed the ViewModels (plan P1.4).** Owner: **ui-builder**. Pull the ASC via
   `UAbilitySystemGlobals` from inside `UI/` and call `UBRVM_Combat::BindToAbilitySystem`; call
   `UBRVM_Match::SetTimeSource` from the same arrival path. Contracts: `gas-purity.md` as a
   **boundary only** — the VM is a read-only consumer of ASC delegates, it applies no effect and
   mutates no attribute. No polling, no Tick (law 4). While here, confirm the subsystem is not
   calling the **deprecated** `UMVVMSubsystem::GetGlobalViewModelCollection()`; the live API is
   `UMVVMGameSubsystem::GetViewModelCollection()`.
4. **Tokens onto `UBRUISettings` (plan P1.5)** — `config = Game, defaultconfig`, so they land in
   `Config/DefaultGame.ini` as diffable text, and the style classes under `UI/Styles/` read them.
   Owner: **ui-builder**, values proposed by **tuning-curator**. **Read the overlap note in Notes
   before starting this step** — the parent proposal's BP27 claims the palette/type ramp, in a
   different shape (`DT_UIPalette.csv`). Land the *accessor and settings shape* here; do not
   author a colour value that BP27 would author differently, and do not create
   `Content/Data/DT_UIPalette.csv` from this packet.
5. **Back, and a glyph to draw (plan P1.6).** Owner: **ui-builder**. A `UCommonUIInputData`
   subclass **in C++** plus its `CommonInputSettings` entry in `Config/DefaultGame.ini`. Without
   it the activatable stack has no Back action and `UBRButtonPrompt` (BP28+) has no action to draw
   a glyph for. **Back pops the stack** (`ue5-ui-architecture` §6) — it is never hand-rolled.
   `UCommonInputBaseControllerData` (the per-device glyph *asset*) is OUT: see standing question ①.
6. **Verify.** Owner: **verifier**. Contracts: `testing.md`.
   - Rung 1: `Tools/run-ubt.sh` green on all three targets under an R19 timestamp proof.
   - Rung 2: the `ue5-ui-architecture` §8 greps over `Source/Breachpoint/UI/` — **zero
     `NativeTick`, zero UMG property bindings**, zero `ConstructorHelpers`, zero hard widget-class
     `UPROPERTY`, no `SetInputMode*` outside `GetDesiredInputConfig`. Plus a spec asserting
     push/pop ordering and that `Layer.Modal` suppresses `Layer.Game` input.
   - Rung 3: a root layout mounts for LP0 and the HUD layer receives the HUD widget; a modal
     pushes over the HUD and pops back; resize 720→1080 moves nothing relative to the grid.
   - Rung 4a: a second client gets its **own** layout and its **own** VM instance.
7. **Critic.** Owner: **critic**. Attack surface named: the mount path firing twice (a second
   `OnPlayerControllerChanged` on seamless travel creating a second root layout); a VM instance
   shared across local players; a soft class slot that resolves to nothing shipping green; input
   mode left in a menu state after a pop; and `SetInputMode*` reached from anywhere other than
   `GetDesiredInputConfig`.

## Done when

- [ ] Rung 1 green on **all three targets**, under an R19 timestamp proof pasted into the Log
- [ ] Rung 2: grep proves **zero `NativeTick` and zero UMG property bindings** across
      `Source/Breachpoint/UI/`; a spec asserts push/pop ordering **and** that `Layer.Modal`
      suppresses `Layer.Game` input
- [ ] Rung 3: a root layout **mounts for LP0** and the HUD layer **receives the HUD widget** —
      stated as rung 3, PIE only (law 6)
- [ ] Rung 3: a modal pushes over the HUD and **Back pops the stack** rather than being
      hand-rolled; 720→1080 resize moves nothing relative to the grid
- [ ] Rung 4a: a **second client** gets its own layout and its own VM instance — not the host's
- [ ] The six `TSoftClassPtr` slots resolve, and **zero Blueprint classes and zero new data assets
      were created** by this packet (laws 3 and 7)
- [ ] Critic findings addressed or waived in the Log
- [ ] Findings + decisions written to this ticket's Log

## Notes

- Crew: **ui-builder** steps 1–5 · **tuning-curator** proposes any token value in step 4 ·
  **verifier** step 6 · **critic** step 7
- Contracts: `data-and-assets.md` (soft class refs; config over asset) · `testing.md` (rungs 1–3,
  grep gates) · skills `ue5-ui-architecture`, `ui-presentation` §8 ·
  `docs/ui/ue-frontend/SCREEN-MANIFEST.md` §3 (layer routing)
- Binary files this ticket OWNS (lock before editing): **none.** All three existing WBPs are read
  and verified, never edited. If a WBP must change, that is a separate `editor-live` packet under
  R37, not a step here.
- Out of scope: any component class (BP28+), any screen (BP43+), any WBP authoring or edit (BP36),
  gamepad **parity** (BP54), `Content/Data/DT_UIPalette.csv` (BP27), and every one of the six
  contract gaps below — a session that "just fixes" one has broken law 5.
- **The six contract gaps are separate tickets and are NOT this packet's work:** BP60
  (`architect.py` budget), BP61 (undeclared UI units / D11), BP62 (`LogBRUI`), BP63
  (icons path conflict), BP64 (`guard_laws.py` doc fix), BP65 (motion tracker feed). BP62 is the
  one a builder will feel here: **`LogBRUI` does not exist** — `Source/Breachpoint/Core/BRCore.h:8`
  declares only `LogBRAbility`. Do not mint a log channel inside `UI/`; use what exists or say
  nothing, and let BP62 land the channel.
- **`docs/ui/ue-frontend/TICKETS.md` remains the parent proposal and is NOT edited by this
  ticket** (law 5 — it is not this packet's owner path). This file is the cut of its BP26 block;
  the other 33 blocks stay proposals until approved. BP10 remains the parent ticket and closes by
  reference, also not ours to edit.
- **Overlap with BP27 (proposal), unresolved.** BP27 proposes the palette/type ramp as
  `Content/Data/DT_UIPalette.csv` + `DT_UIType.csv`; plan §1.3 argues the same tokens belong on
  `UBRUISettings` as config text ("one source, greppable, one commit to rebrand"). **Two documents
  point opposite ways and neither has been ruled on.** Step 4 lands the settings *shape* only, so
  that whichever way the board rules, no value has to be authored twice. If the board rules for
  BP27's CSV, step 4's accessor still stands and reads the table instead.
- **Standing question ① — do not resolve it by creating the asset.**
  `UCommonInputBaseControllerData` is a data asset with **no C++ path** and it is **not on
  `BREACHPOINT-AUTHORING-MATRIX.md` Tier 4's closed list**. Law 7's standing question ("which tier,
  and if Tier 4, why can't C++ express it?") has no answer on file. **File a `contract_gap` in this
  Log and stop that thread** — the C++ `UCommonUIInputData` half in step 5 lands without it; only
  the per-device glyph art waits.
- **Standing risk — the skill has never met a compiler.** `ue5-ui-architecture` carries an explicit
  *"UNVERIFIED DRAFT — never run against a build"* banner and this packet is its first contact with
  one. Budget a correction pass; the skill says BP10 owns it. Plan §1.4 already closed the
  MVVM-macro half (`FieldNotify`/`Getter`/`Setter` are top-level `UPROPERTY` specifiers, **not**
  inside `meta=()`; bitfields need `UE_MVVM_SET_PROPERTY_VALUE_INLINE`).

## Log

(append findings here, dated, newest last — this is what the next session reads)

**2 Aug 2026 — cut. Three of the proposal's claims were checked against disk; two changed the
ticket.**

*Verified true, unchanged:*
- `CreateLayoutForLocalPlayer` is declared (`BRUIManagerSubsystem.h:48`) and defined
  (`BRUIManagerSubsystem.cpp:233`) and **called by nothing** in `Source/`. Same for
  `UBRVM_Combat::BindToAbilitySystem` and `UBRVM_Match::SetTimeSource`. The proposal's central
  claim holds exactly.
- Seven units on disk in `Source/Breachpoint/UI/` (13 files), three WBPs in `Content/UI/`, all
  three real assets (~20 KB each), no LFS pointer stubs.

*Changed the ticket — ①: the viewport class is ALREADY SET.* The proposal lists
"`UCommonGameViewportClient` as the viewport class" as work.
`Config/DefaultEngine.ini:143` already carries
`GameViewportClientClassName=/Script/CommonUI.CommonGameViewportClient`, landed under BP18 with a
dated comment quoting the exact `LogUIActionRouter` error it fixes. Step 1d is therefore a
**verification**, not an edit. Do not re-add the line.

*Changed the ticket — ②: risk ③ (D5) is DEAD.* The proposal's third risk is *"HANDOFF's D5 (two
one-line fixes that stop the module compiling) is unresolved on the register while HANDOFF says
the module builds; one of the two is stale and nobody has checked."* Someone has checked.
`docs/DECISIONS-OWED.md` (Verification addendum, 1 Aug 2026) states **"D5 is STALE ON BOTH HALVES.
It is not a decision the founder owes"** — `"SlateCore"` is present at `Breachpoint.Build.cs:32`,
and the `FBRKillfeedEntry`/`FBRKillFeedEntry` UHT collision was resolved in commit `890230c`. Risk
③ is dropped from this ticket rather than carried forward as a phantom.

*Widened the owner_path, deliberately, and it should be reviewed.* The proposal names
`Source/Breachpoint/UI/`, `Config/DefaultGame.ini`, `Config/DefaultInput.ini`. Plan P1.2 puts
`[/Script/Engine.UserInterfaceSettings]` in **`Config/DefaultEngine.ini`**, which the proposal's
grant does not cover — and that setting is called "the highest-leverage line in the plan", because
without it every WBP carries ×1.5 arithmetic forever. `Config/DefaultEngine.ini` is added to the
grant above. **If the board will not grant it, drop step 1c and file it as a gap — do not edit
`DefaultEngine.ini` without the grant** (law 5).

*Not carried, by instruction:* plan P1.7 (`DECLARE_LOG_CATEGORY(LogBRUI)`) is gap G3 and is now
**BP62**. It is not a step here even though `BRCore.h` is one line away, because `Core/` is not
this owner path. Verified on disk: `Source/Breachpoint/Core/BRCore.h:8` declares exactly one
channel, `LogBRAbility`, and `LogBRUI` appears nowhere in `Source/`.
