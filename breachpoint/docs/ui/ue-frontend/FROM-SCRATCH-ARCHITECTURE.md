# The from-scratch UI architecture — the fewest files that don't cost quality

**Status:** v1, 4 Aug 2026. Founder directive: *"complete from scratch architecture framework …
advanced native programming, best practices and principles. Less is more — the absolute
necessary least files without sacrificing the quality of the code."*

**What this is:** the answer to "if we rebuilt `Source/Breachpoint/UI/` today, knowing
everything the last week cost us, what is the smallest correct shape?" Every rule below is paid
for — it cites the scar that taught it. Nothing here is aspiration.

**What this is not:** a demolition order. §6 says what a migration would actually do; most of
the win is available incrementally.

---

## 1. The baseline, measured on disk (4 Aug 2026)

| | Today |
|---|---|
| Headers | **54** (≈104 files with .cpp) |
| Classes | **71** |
| Lines (h+cpp) | **20,646** |
| Folders | 8 (`UI/`, `Components/`, `Screens/`, `HUD/`, `ViewModels/`, `Styles/`, `Settings/`, `Loading/`) |

The important prior fact: **the class-level minimization already happened.** The inventory audit
cut 9 of 17 planned menu classes; `UBRMenuRow` serves ten asset variants from one class;
`UBRFeatureCard` serves two. The remaining fat is not classes — it is **files, includes, and
split token sources.** A from-scratch pass that "merges classes" would be re-fighting a won war.

---

## 2. The seven principles (each one already paid for)

1. **A file is an ownership seam, not a class.** One header = the set of classes ONE ticket
   edits together under law 5. `BRRosterPanel.h` (3 classes), `BRNavBar.h` (2),
   `BRTextStyles.h` (5) already work this way; the generator slices headers by class, so
   tooling is indifferent. *Scar: 54 headers for 71 classes means the average header carries
   1.3 classes — the seams were drawn per-class, and the include graph paid for it.*
2. **Variants are data, not classes.** `RowType` enum + CDO defaults gave ten button assets
   from one class. The class count follows *behaviour*; the asset count follows *appearance*.
   *Scar: 9 of the 17 originally-specified menu classes duplicated existing behaviour.*
3. **The design system is defined once, and generated.** Colors, type, geometry live in
   `figma_tokens.json` (read from Figma variables) and land in ONE generated header. Never
   two token headers, never a literal in a widget. *Scar: `BRUITokens.h` + `BRComponentTokens.h`
   split one system across two files; three measured fonts turned out off-system (E3) and no
   single place could say so; `MenuRowSlotInsetRight = 22` sat unread and wrong.*
4. **Procedural before art.** An `SLeafWidget` that paints hairlines/rules/segmented bars
   eliminates entire texture families and their blank-white-rectangle failure mode (BP70 D2).
   *Scar: `UBRHairlineBorder` replaced ~4,500 would-be border widgets across the front end.*
5. **Producer / ViewModel / widget is a firewall, not a convention.** ViewModels include zero
   game headers. Widgets include only ViewModels. Exactly one director per surface includes
   both sides. *Scar: the HUD shipped consumer-perfect with 19 of 21 feeds dead, because no
   file's existence was owed for the producer role. The missing file class of bug is the worst
   class — nothing that compiles detects it.*
6. **Config over subclassing, soft refs over hard.** `UDeveloperSettings` + `DefaultGame.ini`
   wire every asset; R26 BP children only for defaults. *Scar: one missing ini section made the
   entire stack unreachable while everything compiled.*
7. **Lifecycle symmetry is a review item.** Whatever `NativeDestruct` tears down,
   `NativeConstruct` rebuilds; `NativeOnInitialized` is once-per-object only. No gameplay Tick,
   ever — timers and delegates. *Scar: three nav-bar bugs shared this one root cause.*

---

## 3. The minimal file map — 28 seams (≈54 files), 71 → ~66 classes

```
Source/Breachpoint/UI/
│
│  ── foundation (4 seams) ──────────────────────────────────────────────
├── BRUITypes.h                  header-only · enums + view structs crossing seams
├── BRTokens.h                   header-only · GENERATED from figma_tokens.json by a
│                                committed script — colors, type scale, geometry. The .h is
│                                an artifact; the JSON is the source (principle 3)
├── BRStyles.h/.cpp              all CommonUI style CDOs (button + text, 9 classes today)
├── BRUISettings.h/.cpp          UDeveloperSettings — every soft class ref in the module
│
│  ── framework (3 seams) ───────────────────────────────────────────────
├── BRActivatableWidget.h/.cpp   THE screen base: input config, focus target, back action
├── BRUIManager.h/.cpp           UBRUIManagerSubsystem + UBRRootLayout — the two classes
│                                that own the layer stack are one seam
├── BRHUDLayout.h/.cpp           the Game layer widget + UBRKillfeedEntryWidget (as today)
│
│  ── data (3 seams) ────────────────────────────────────────────────────
├── BRViewModels.h/.cpp          ALL ViewModels (Combat, Match, Player, FrontEnd) — pure,
│                                zero game includes, FieldNotify push only
├── BRHUDDirector.h/.cpp         producer: game events → HUD VMs (exists, b2eaaf8)
├── BRFrontEndDirector.h/.cpp    producer: session/party/save → menu VMs — DOES NOT EXIST
│                                TODAY and its absence is the menu's 19-dead-feeds bug
│                                waiting to repeat (principle 5; BP24's real shape)
│
│  ── primitives (1 seam) ───────────────────────────────────────────────
├── Components/BRPrimitives.h/.cpp   HairlineBorder + Rule + ProgressBar — every SLeafWidget
│                                    that paints instead of shipping textures
│
│  ── components (5 seams) ──────────────────────────────────────────────
├── Components/BRMenuRow.h/.cpp      UBRMenuRow + UBRSettingsRow (the subclass belongs with
│                                    its contract) — ten button assets, one seam
├── Components/BRNavBar.h/.cpp       NavTab + NavBar + ButtonPrompt — the chrome strip is
│                                    one data contract (bumpers host prompts)
├── Components/BRRoster.h/.cpp       Header + Row + Panel (already one header today)
├── Components/BRPanels.h/.cpp       Panel + Scrim + LeftRail + FeatureCard — the surface
│                                    family; FeatureCard still serves two assets
├── Components/BRLists.h/.cpp        ItemTile + ItemGrid + TableRow + SmallHeader +
│                                    ScrollBar — the pooled-list family (IUserObjectListEntry)
│
│  ── HUD (4 seams) ─────────────────────────────────────────────────────
├── HUD/BRVitals.h/.cpp              vitals (consumes the ProgressBar primitive)
├── HUD/BRReticle.h/.cpp             reticle + hit markers
├── HUD/BRTray.h/.cpp                EquipmentTray + AmmoBlock — one seam EITHER WAY the
│                                    founder rules on the tray split; the ruling changes the
│                                    widget tree, not the file
├── HUD/BRFeeds.h/.cpp               Killfeed + MatchBand — the two GameState-fed strips
│
│  ── screens (5 seams) ─────────────────────────────────────────────────
├── Screens/BRScreen_FrontEnd.h/.cpp
├── Screens/BRScreen_Settings.h/.cpp     settings + key-remap as a MODE of one screen, not
│                                        a second screen (they share the registry, the
│                                        stack slot, and the back-action semantics)
├── Screens/BRScreen_Scoreboard.h/.cpp
├── Screens/BRScreen_DeathRespawn.h/.cpp
├── Screens/BRModals.h/.cpp              Options + Warning + Toast — one layer, small
│                                        classes, one seam
│
│  ── domains (3 seams) ─────────────────────────────────────────────────
├── Settings/BRSettingsRegistry.h/.cpp   registry + data objects + value objects (the
│                                        whole settings model is one writer)
├── Settings/BRGameUserSettings.h/.cpp   the engine-facing half + key remap storage
└── Loading/BRLoadingScreen.h/.cpp       subsystem + its settings class
```

**28 seams · ≈54 files · ~66 classes · est. 17–18k lines.**

### What each cut actually buys

| Merge | From → to | Why it is safe |
|---|---|---|
| Tokens | 2 headers + JSON → 1 generated header + JSON | one source of truth; E3-class drift becomes impossible to hide |
| Styles | 2 headers, 9 classes → 1 header | styles are CDOs consuming tokens; nobody edits one family alone |
| Manager + RootLayout | 2 → 1 | neither is ever touched without the other; the stack is one concept |
| NavBar + ButtonPrompt | 2 → 1 | the bar hosts the prompts; one BindWidget contract chain |
| Panels family | 4 → 1 | Panel/Scrim/LeftRail/FeatureCard share the ground/hairline/token idiom |
| Lists family | 3 → 1 | all pooled `IUserObjectListEntry` consumers; one pooling discipline |
| HUD 6 → 4 | Tray pair, Feeds pair | each pair shares its producer feed and its founder DECIDE |
| Settings 5 → 2 | registry model vs engine half | two writers exist, so two files — not one, not five |
| KeyRemap screen | folded into Settings screen | one stack destination; a mode switch, not a screen |
| VMs 2 headers + folder → 1 | ViewModels are the firewall layer; one file IS the firewall audit |

### What is deliberately NOT merged

- **Screens stay 1:1 with stack destinations** (except modals). Different lifecycles, different
  tickets, different activation semantics — merging them saves nothing and breaks one-writer.
- **The four HUD seams stay four.** Six-into-one would be a 2,400-line header with four
  independent producers feeding it; the reviewability cost exceeds the file-count win. This is
  the "without sacrificing quality" clause doing real work.
- **`BRHUDDirector` and `BRFrontEndDirector` stay separate.** One per surface is the rule that
  makes a missing producer *visible as a missing file* — merge them and the menu's producer can
  go missing inside an existing file again.

### The one file the minimal architecture ADDS

`BRFrontEndDirector.h/.cpp`. Fewer files is not the goal — *the necessary files* is. The menu
today has wired ViewModels and **no producer** (LANES-AND-ROADMAP §4: "no producer"), which is
byte-for-byte the HUD bug we just spent four packets fixing. The minimal set includes the file
whose absence was the largest defect this project has found.

---

## 4. The native-practice spine (what "advanced" means here, concretely)

| Concern | The one pattern | Not this |
|---|---|---|
| Global UI state | `UGameInstanceSubsystem` (manager) / `ULocalPlayerSubsystem` (directors) | manager actors, singletons, statics |
| Wiring | `UDeveloperSettings` + ini, `TSoftClassPtr` resolved on demand | `ConstructorHelpers`, hard refs |
| Data flow | FieldNotify MVVM, push on event | polling, Tick, per-frame GetHealth() |
| Update cadence | timers, delegates, GameplayCues | any gameplay Tick (law 4) |
| Chrome | `SLeafWidget` primitives painting from tokens | texture-per-border, per-state art |
| Lists/feeds | `FUserWidgetPool`, `IUserObjectListEntry` | hand-placed rows, CreateWidget per entry |
| Input | CommonUI activatable stack, `GetDesiredInputConfig`, `CommonActionWidget` glyphs | per-platform key art, manual focus juggling |
| Styling | C++ `UCommonButtonStyle`/text-style CDOs reading `BRTokens.h` | BP style assets (R18), per-widget hex |
| Asset trees | generated by `mcp-ui/` from the plan; header = validated BindWidget contract | hand-authored WBPs the generator later reverts |
| Lifecycle | `NativeOnInitialized` once / construct-destruct symmetric; `TWeakObjectPtr` for every cross-object bind | rebind-in-initialize (the nav-bar triple bug) |
| Include hygiene | forward declares in headers; a seam includes tokens + its VMs, nothing sideways | widget→widget includes (today's graph has them) |

---

## 5. The honest accounting

| Metric | Today | Minimal | Δ |
|---|---|---|---|
| Headers (seams) | 54 | **28** | −48% |
| Files | ~104 | ~54 | −48% |
| Classes | 71 | ~66 | −7% |
| Lines | 20,646 | ~17–18k | −10–15% |

Read the deltas honestly: **files halve; code barely shrinks.** The line savings are boilerplate
(54 copyright/include preambles → 28, duplicate token tables, the KeyRemap fold) — not logic.
Anyone promising a 50% *code* reduction from re-architecting this layer would be selling the
class-level cut that already happened. What the minimal shape actually buys:

1. a shallow include graph (the top include today is a primitive, which is correct; the
   sideways widget→widget edges go away),
2. one token source (E3-class drift becomes a generator diff, not an archaeology dig),
3. seams that match tickets (one header per owner path claim — law 5 stops fighting the tree),
4. **and one new file that prevents the project's worst bug class from recurring.**

---

## 6. If we adopt it (not ordered, not a demolition)

The migration is four independent moves, each a normal packet, none blocking the others:

1. **Tokens:** generate `BRTokens.h` from `figma_tokens.json`; delete the two hand headers.
   (Terminal lane; the generator script is ~100 lines next to `mcp-ui/gen_ui/`.)
2. **`BRFrontEndDirector`:** write the missing producer. This is BP24's real shape and is worth
   doing *first* — it is the only move that fixes a defect rather than a shape.
3. **Folds:** styles→1, manager+root→1, HUD pairs, panels/lists families, KeyRemap→mode.
   Mechanical, but every fold moves `#include` lines in consumers and re-slices `wbp_plan.py`
   header references — so it rides with BP71's compile gate, never ahead of it.
4. **Include sweep:** forward-declare pass + kill sideways widget includes. Free-standing.

Rule for all four: **the generator's `header:` fields move in the same commit as the file.**
A plan that names a dead header fails validation loudly — that is the safety net working; do
not pre-widen it.
