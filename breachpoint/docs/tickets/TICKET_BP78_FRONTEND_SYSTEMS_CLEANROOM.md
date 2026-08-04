# TICKET — Clean-room the settings, key-rebind and loading-screen systems

> STATUS: in progress — cut by the lead 3 Aug 2026, executed same session. Rung 1 partial
> (Editor PASS); full three-target run and editor verification outstanding.

Founder directive: `vinceright3/FrontendUIProgramming` was proposed for wholesale adoption.
Its licence forbids commercial use (see **R41**), so the founder ruled **clean-room**: take the
capability, take none of the code or content. Everything below is written as `BR` C++ against
UE 5.8 and obeys R18 — nothing was adopted, so R40 never comes into it.

**Ordering law:** the licence read gates everything. R41 is the artifact of that read and it was
written before any implementation began.

## Kickoff

- requires: engine-installed (rung 1 needs a source-built UE 5.8); editor-live for step 6 only
- R41 exists in `docs/DESIGN-RULINGS.md` — the licence finding is recorded before code lands
- The reference clone is deleted from the working tree and was never committed
- owner_path: `Source/Breachpoint/UI/`, `Source/Breachpoint/Tests/`,
  `Source/Breachpoint/Breachpoint.Build.cs`, `Breachpoint.uproject`,
  `Config/DefaultEngine.ini`, `Config/DefaultGame.ini`, `THIRD-PARTY-NOTICES.md`
  (claimed but expected UNTOUCHED — a diff there means the clean-room boundary was crossed)

## Steps (in order)

1. **Licence read and ruling.** Clone to scratchpad, read `LICENSE.txt`, inventory Source and
   Content. Record the finding as R41. Delete the clone. — DONE
2. **Settings data model.** `UBRSettingsDataObject` / `UBRSettingsCollection`,
   `UBRSettingsValue_{Scalar,Discrete}`, `UBRGameUserSettings`, `UBRSettingsRegistry`.
   Typed lambda accessors, not reflection property paths. — DONE
3. **Key rebinding.** `UBRSettingsValue_KeyRemap` over Enhanced Input's
   `UEnhancedInputUserSettings`, with real conflict detection via
   `UEnhancedPlayerMappableKeyProfile::GetMappingNamesForKey`. — DONE
4. **Screens.** `UBRSettingsRow` (a `UBRMenuRow` subclass), `UBRScreen_Settings`,
   `UBRScreen_KeyRemap`. Tabs reuse `UBRNavBar`; rows build into a panel per the
   `UBRModal_Options` idiom. — DONE
5. **Loading screens.** `UBRLoadingScreenSubsystem` + `UBRLoadingScreenSettings`. MoviePlayer
   for the blocking load, viewport widget for post-load holds, named holds with a timeout. — DONE
6. **Verification.** Rung 1 (all three targets), rung 2 (`Breachpoint.Sim.Settings`), then a
   live editor pass over unreal-mcp to confirm the classes are reflected and the config seams
   engaged. — IN PROGRESS

## Done when

- [x] R41 recorded; reference clone deleted and never committed
- [x] `THIRD-PARTY-NOTICES.md` unchanged by this packet
- [ ] Rung 1: BreachpointEditor + Breachpoint + BreachpointServer all PASS in one run
- [ ] Rung 2: `Breachpoint.Sim.Settings` green
- [ ] Editor-live: `BRGameUserSettings` is the engaged settings class; the registry builds four
      tabs; Enhanced Input user settings are on
- [x] Findings + decisions written to this Log

## Notes

- Crew: lead, sole driver (R29.2). No subagents dispatched.
- Binary files this ticket OWNS: none. No `.uasset` or `.umap` was created or edited.
- Out of scope: WBP authoring for the new screens (`WBP_Screen_Settings`,
  `WBP_Modal_KeyRemap`, `WBP_SettingsRow`) — the C++ `BindWidget` contracts exist and are
  documented in each header, but no widget assets were generated. The screens will not render
  until `Tools/gen_ui/build_wbp.py` produces them. **This is the honest gap in this packet.**
- Also out of scope: seeking a commercial-use grant from the upstream author (R41 leaves it open).

## Log

**3 Aug 2026 — the licence, which changed the whole shape of the request.**
`vinceright3/FrontendUIProgramming` (UE 5.5, 77 C++ files, 356 uassets, 464 MB) was proposed for
wholesale adoption. `LICENSE.txt` is all-rights-reserved, non-commercial, informational use only.
Vendoring would have been infringement on ship. Founder ruled clean-room. Recorded as R41.

**Content inventory, so nobody re-opens it.** Of the 464 MB: ~300 assets are Epic template
content (Manny/Quinn, mannequin rigs) we already ship from Epic directly under the EULA; 117
ControllerIcons have NO licence file anywhere in the repo (provenance unknown — must be
re-sourced from a pack with a known licence, not taken from him); Din is a commercial Monotype
family; the two `.mp4` loading movies are his. Nothing was worth taking even setting licence
aside.

**What the codebase already had, which shrank the work.** Two findings that removed planned work:
- `UBRMenuRow` already carries `Slider`, `Checkbox`, `Radio` and `DropDown` types plus a
  right-aligned `Selection` value. It IS the settings row; only a behaviour subclass was needed.
- `UBRNavBar` is already a complete tab bar with `SetTabs`, exclusive selection through a
  `UCommonButtonGroupBase`, and bumper navigation. No tab widget was written.
A third correction: `UBRModal_Options` is a generic Figma-specced row-CHOOSER, not a weak
settings screen. It is NOT retired; `UBRScreen_Settings` is a new screen beside it.

**Design decisions worth the next session's time.**
- *Accessors are typed lambdas, not reflection property paths.* A property-path typo is a
  runtime no-op; a lambda typo does not compile. It also lets the spec bind leaves to plain
  `TSharedRef<float>` stubs, so `Breachpoint.Sim.Settings` needs no `UGameUserSettings` at all
  and CANNOT corrupt the developer's own config by running.
- *Discrete settings are id-based, never index-based.* An index accessor silently repoints every
  saved value the moment a quality list is reordered or a driver reports different resolutions.
  An unrecognised id resolves to `INDEX_NONE` and reads as the em dash — "we do not recognise
  your saved value" is recoverable; "your value is now Low" is a lie. Pinned by two spec cases.
- *A bool is a discrete with two options,* not its own class. `MakeToggle` is the ergonomics.
- *Default and baseline are different snapshots.* Reset goes to the shipped value; cancel goes
  to what the player had when the screen opened. Conflating them is the classic data-loss bug.
- *`RefreshEditStates` has a re-entrancy guard.* With today's single rule (frame-rate limit vs
  V-Sync) it terminates anyway, because `SetEditState` early-returns on no-change — but that is
  an accident of there being one rule. Two mutually-disabling rules would ping-pong forever.
- *The loading screen needs BOTH mechanisms.* MoviePlayer is the only thing that renders while
  the game thread is blocked loading; a viewport widget is the only thing that covers the wait
  AFTER the map loads. Holds are named and explicit (no polling, law 4) with a force-release
  timeout, because a leaked `AddLoadingReason` is otherwise an input-less soft-lock.

**Rung 1 — three compile errors and one pre-existing one.**
First run FAILED (exit 6). Four `NAME_None` lambdas deduced `EName` instead of `FName`
(C3487/C2440) — fixed with explicit `-> FName` return types. Also surfaced
`BRNavBar.cpp(40): error C4458: declaration of 'Style' hides class member` — **pre-existing, not
mine**: `UCommonButtonBase` has a `Style` member and the local shadowed it. It only appeared now
because this was the first from-scratch build on this machine (all three artifacts were ABSENT).
Renamed to `BorderStyle`, matching `UBRMenuRow`'s spelling. Second run: BreachpointEditor PASS,
artifact 15.7s newer than start.
Third fix: `UEnhancedInputUserSettings::GetCurrentKeyProfile` is deprecated in 5.6 and the
warning states the project will not compile next release — moved to `GetActiveKeyProfile` (4
call sites).

**Config seams, which are the invisible half.** Two lines with no C++ substitute:
- `Config/DefaultEngine.ini` `[/Script/Engine.Engine] GameUserSettingsClassName=` — without it
  every `Cast` in `UBRGameUserSettings::Get()` returns null and the whole tree builds unbound.
  Nothing crashes and nothing logs; every row just shows the em dash.
- `[/Script/EnhancedInput.EnhancedInputDeveloperSettings] bEnableUserSettings=True` — without it
  the Controls tab builds EMPTY with no error.
Both failure modes are silent, which is why they are named here rather than left to be found.

**RUNG 1 PASS — all three targets, from scratch (run stamp 20260803-222656).**
```
BreachpointEditor  0  actions=5     seconds=7.5    verdict=PASS
Breachpoint        0  actions=1056  seconds=703.4  verdict=PASS
BreachpointServer  0  actions=1023  seconds=642.4  verdict=PASS
OVERALL RUNG 1 : PASS
```
Every artifact timestamp-asserted newer than its target's start (R19.5). Two later PARTIAL
Editor-only runs (225142, 225757) covered the config move and the console command; both PASS,
both explicitly NOT rung-1 results.

**RUNG 2 PASS — `Breachpoint.Sim.Settings`.**
`RUNG2|filters=Breachpoint.Sim.Settings|discovered=26|passed=26|failed=0|notrun=0|editor_exit=0|seconds=12.9|verdict=PASS`
The suite binds leaves to `TSharedRef<float>` / `TSharedRef<FName>` stubs, so it touches no
`UGameUserSettings` and cannot rewrite the developer's own config by running. Two cases pin the
id-vs-index decision directly: an unrecognised stored id reads as the em dash and the store is
asserted UNTOUCHED, and replacing the option list does not re-point the selection.

**WBP GENERATION — the three assets exist and compile.**
`WBP_SettingsRow` (8 widgets), `WBP_Modal_KeyRemap` (5), `WBP_Screen_Settings` (7). All three
PASS with every slot and art write verified by read-back, and `CompileWidgetBlueprint` green —
which a `BindWidget` mismatch would have failed. Receipts: `docs/ui/receipts/gen-ui-20260804T0253*`
and `...0254*`.

*Content was REUSED rather than re-authored, on the founder's instruction.* `WBP_SettingsRow`
does not copy `WBP_MenuRow`'s tree — the tree was extracted to `MENU_ROW_TREE` and both assets
now reference the one definition, so a COMPONENT-SPECS correction cannot land in one and miss
the other. `WBP_Screen_Settings` hosts the existing `WBP_NavBar` instead of a second tab strip,
and both confirm prompts reuse `WBP_Modal_Warning`. `WBP_MenuRow` was regenerated afterwards and
still builds its 8 widgets, which is what makes the shared-tree refactor non-regressive rather
than merely plausible.

*A new helper, `unbound()`.* `UBRSettingsRow` inherits every `BindWidget` from `UBRMenuRow`, and
`required_bind_widgets` parses ONE header sliced to ONE class — it cannot see a base's binds
(the limitation `WBP_GearDetail` and `WBP_ItemTitle` already document inline). The widgets are
therefore created by exact name and claimed by none. Written as a function so the next
inherited-bind case does not re-derive it.

**A latent generator bug, fixed.** `build_wbp.py`'s receipt file was opened with no `encoding`,
so it defaulted to the host locale — cp1252 here. The run died with `UnicodeEncodeError` on the
em dash in its own first heading, AFTER the plan had validated and the editor connection was
live. Host-locale dependent, so it would not reproduce on a machine with `PYTHONUTF8` set. Now
`encoding="utf-8"`.

**EDITOR-LIVE VERIFICATION (raw HTTP to the in-editor MCP server).**
- `Engine.GameUserSettingsClassName` -> `/Script/Breachpoint.BRGameUserSettings`. The config
  seam is engaged; this is the line whose failure mode is a fully-built tree of unbound leaves.
- `EnhancedInputDeveloperSettings.bEnableUserSettings` -> `true`.
- `UBRUISettings` CDO: all four BP78 soft classes resolve to real assets
  (`WBP_Screen_Settings_C`, `WBP_Modal_KeyRemap_C`, `WBP_Modal_Warning_C`, `WBP_SettingsRow_C`).
- `UBRLoadingScreenSettings` CDO: 2.0 / 30.0 / 1000, as authored.
- `UBRGameUserSettings`' own properties could NOT be read by `get_properties` — they are
  `UPROPERTY(config)` with no `EditAnywhere`, so the details-panel reader does not surface them.
  Not a defect; the class itself resolved (the error names it) and its values are reached
  through the C++ getters. Recorded so the next session does not read it as a failure.

**A gap this packet closed as it was found: nothing could REACH the screen.** No front-end
surface navigates to settings, so the WBPs existing would still have left it unrenderable.
`BR.ShowSettings` is a `FAutoConsoleCommandWithWorld` in `BRUIManagerSubsystem.cpp` that creates
the layout if absent and pushes the screen for the first local player. It is a development
affordance, not a shipping entry point — the main menu's settings row is still owed.

**FINDING (medium) — `.gitignore` makes a cook claim about `Content/Reference/` that is false.**
Found while siting the FrontendUIProgramming clone as reference material under R41.
`.gitignore:40-41` says *"Content/Reference is also in DirectoriesToNeverCook, so it cannot reach
a packaged build."* It is not. `grep -rn DirectoriesToNeverCook Config/` returns **zero hits** in
the entire Config tree. `Content/Reference/README.md` states the truth — *"There is no
DirectoriesToNeverCook entry"* — so the two documents directly contradict each other and the
wrong one is the one a reader hits first, in the file they check before dropping licensed
content in.

What is actually true: `Content/Reference/*` IS gitignored (verified with `git check-ignore -v`),
so the distribution-by-push failure mode is genuinely prevented. Cook exclusion is NOT
configured; the only guard is `Tools/verify_notices.py --ship`, which fails on a hard reference
from shipping content into `/Game/Reference/`. That is a real guard but it is a `--ship`-only
check, not the structural exclusion the comment promises.

NOT FIXED HERE: `.gitignore` is outside this packet's owner_path (law 5). Either correct the
comment to match the README, or add the `DirectoriesToNeverCook` entry and make the comment true
— the second is better, and it is a one-line `Config/DefaultGame.ini` change. Filed rather than
edited.

**Honesty ladder, stated exactly.** Rung 1 (three targets) and rung 2 (26 specs) are real and
green. The WBP receipts prove each asset MATCHES ITS PLAN and compiles — they do not prove the
layout is correct on screen, and the generator's own receipt says so. No rung-4 claim is owed
or made: nothing here replicates.
