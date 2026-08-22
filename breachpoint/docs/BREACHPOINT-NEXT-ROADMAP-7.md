# ROADMAP 7 — THE HUD: the match, finally visible

**Cut:** 20 August 2026 by the cloud lead · founder's brief: *"advanced programming, multiplayer,
GAS purity, CommonUI native, best practices — less is more, modular and scalable."*
**Research:** [`RESEARCH-UI-HUD`](BREACHPOINT-NEXT-RESEARCH-UI-HUD.md) (the old module's 20.7k-line
UI inventoried; BN's feeds audited per element).

## The one sentence

The HUD is a **projection of replicated state** — every value arrives by delegate or RepNotify,
never by asking — rendered through **CommonUI's native layer stack** so every later screen
(pause, settings, front end) slots into a machine that already exists.

## Ruling changes since the research doc

- **R-UI-3 is SUPERSEDED by the founder, 20 Aug:** CommonUI native from day one. The research
  recommended deferring it; the founder ruled modular-and-scalable wins — the activatable stack,
  the one widget base, and the layer tags land in R7, and they are exactly what makes the next
  ten screens cheap. R-UI-1 (port patterns, never classes) and R-UI-2 (MVVM ViewModels without
  the MVVM machinery) stand.
- **Modularity ruling:** per-surface widget classes (the old module's shape), not the research
  doc's single monolith — a surface is its own class the moment it has its own lifecycle or can
  be reused in a second screen, and the match band already appears in two (HUD + scoreboard).
  Every class stays small; the module total is ~16 C++ units against the old module's 98 files.

## What R7 ships (and the player finally sees)

Health and shield bars · ammo + weapon name · match band (my score / limit / clock / phase) ·
killfeed · a static center reticle · death overlay ("eliminated by X — respawning in N") ·
hold-Tab scoreboard · post-match winner + standings. Warmup and post-match announce themselves.
Nothing else — every deferred surface is in the ledger at the bottom.

## The laws, applied to UI

1. **GAS purity, projected:** the UI READS the ASC (attribute delegates, tag events) and never
   writes it. No widget calls a Server RPC, mutates an attribute, or activates an ability. The
   one UI input verb (scoreboard) is the CONTROLLER's, handled before the input tag would reach
   the ASC — a UI verb is not an ability and must not log "NO granted ability carries it".
2. **Multiplayer three-views:** every feed is RepNotify- or delegate-driven and fires on listen
   host AND remote client (authority broadcasts by hand where OnReps don't run — the R4
   GameState discipline, now applied to PlayerState/Weapon/Equipment too). HUD claims name their
   rung; the killfeed and scoreboard claims come in threes.
3. **No Tick, anywhere:** FieldNotify subscriptions, tag events, and ONE re-armed clock timer
   phase-locked to the server second. `DisableNativeTick` meta on every widget.
4. **C++ first:** C++ owns every binding and every decision; WBPs hold layout, anchors and
   animation curves ONLY (zero graph nodes — a `BlueprintImplementableEvent` is therefore
   unimplementable; widget animations are `BindWidgetAnimOptional`, played FROM C++). Widget
   classes resolve from ini soft paths; no hard widget-class pointer anywhere.
5. **Honest unknown:** ViewModels initialize to explicit Unknown and render dashes — `Live`
   requires the denominators (MaxHealth known, teams resolved), never "some value arrived".
   Every subscriber reads state once at bind time; join-in-progress is a first-class path.

## Goals

- **G1 — the feeds:** every HUD value has a client-side event source. Killfeed ring and respawn
  stamp are NEW replication; the rest is delegates on state that already replicates.
- **G2 — the spine:** CommonUI layer stack + the two ViewModels + the ONE producer
  (`BNHUDDirector`) that wires game → ViewModel on every machine identically.
- **G3 — the surfaces:** the six in-match elements, each a small class bound to a ViewModel.
- **G4 — the screens:** death overlay and scoreboard as activatables on their layers.
- **G5 — the assets:** one terminal ticket builds every WBP from written trees, plus the
  scoreboard input assets.

## The waves, atomic

### Wave 0 — foundations (no behavior change)
| # | Task | Where |
|---|---|---|
| 0.1 | Build.cs: `+UMG +SlateCore +CommonUI +CommonInput +ModelViewViewModel`, public. NO `FieldNotification` (arrives transitively — the compiled reference omits it too) and NO `Slate` until a ListView appears (R7 ships none; both are LINK-time lessons the old module paid for, critic-reconciled) | `BreachpointNext.Build.cs` |
| 0.2 | `Input.Scoreboard` in `BNTags` (its existing `UE_DEFINE_GAMEPLAY_TAG` form). The LAYER tags do NOT go here: CommonUI layers key on `FUITag`, which those macros cannot produce — see 0.3 | `Core/BNGameplayTags.*` |
| 0.3 | `BNUITypes.{h,cpp}`: `FBNUITags : FGameplayTagNativeAdder` registering `UI.Layer.Game / GameMenu / Menu / Modal` via `FUITag::AddNativeTag` (the old module's exact shape — and NOT the `Layer.*` strings it registers, since both modules load) · `EBNUIDataState {Unknown, Live, Stale}` · `FBNKillfeedViewEntry` · the color tokens as C++ constants (cyan=you, health yellow never green, amber=a clock runs, red=threat only, white=you-in-a-list) | `UI/BNUITypes.*` |

### Wave 1 — the feeds (GAMEPLAY lane: replication — critic REFUTER pass mandatory)
| # | Task | Where |
|---|---|---|
| 1.1 | **Killfeed ring**: `FBNKillfeedEntry {Victim, Killer (TObjectPtr<ABNPlayerState>), Sequence, ServerTime}` fixed-size replicated array on `ABNGameState`, `ReplicatedUsing=OnRep_Killfeed` + `OnKillfeedChanged` delegate (OnRep AND authority push). Pushed from `HandlePlayerDeath` where the kill line prints. Ring, not multicast RPC: join-in-progress and GUID re-resolve come free | `Match/BNGameState.*`, `BNGameMode.cpp` |
| 1.2 | **Score delegates**: `OnScoreChanged` on `ABNPlayerState`, broadcast from `OnRep_Kills`/`OnRep_Deaths` AND from `AddKill`/`AddDeath`/`ResetScore` (a listen host runs no OnReps) | `Match/BNPlayerState.*` |
| 1.3 | **Respawn stamp**: `RespawnAtServerTime` (`COND_OwnerOnly`, `ReplicatedUsing` + delegate) on `ABNPlayerState`; stamped in `RequestRespawn`, cleared in `RespawnPlayer`. Client computes remaining locally — the match clock's own proven pattern | `Match/BNPlayerState.*`, `BNGameMode.cpp` |
| 1.4 | **Ammo delegate**: `OnAmmoChanged` on `ABNWeapon`, broadcast from BOTH empty OnReps and the authority mutation points (`ConsumeAmmo`, reload transfer) | `Weapons/BNWeapon.*` |
| 1.5 | **Equipped delegate**: `OnEquippedWeaponChanged` at the tail of `ApplyCurrentWeapon` (already runs on every machine) | `Weapons/BNEquipmentComponent.*` |

### Wave 2 — the spine (UI lane — critic pass)
| # | Task | Where |
|---|---|---|
| 2.1 | **ViewModels**: `UBNVM_Combat` (vitals, ammo, weapon, dead/respawn) + `UBNVM_Match` (phase, clock, scores, winner, killfeed view ring). Transcribed from the old module's compiled code: the injected-attribute-struct decoupling, the `bAnyFound && bDenominatorsKnown` Live gate, `ClearToUnknown()`, the `Delay += 1.0` clock phase-lock, expiry ON the entry. NO global MVVM collection, NO per-widget MVVM view push — C++ field subscription is the one channel | `UI/BNViewModels.*` |
| 2.2 | **`UBNActivatableWidget`** — the ONE widget base (CommonUI): input-mode enum → `GetDesiredInputConfig()`, VM accessors, bind-on-activate/unbind-on-deactivate contract. Transcribed from the compiled reference (§API below) | `UI/BNActivatableWidget.*` |
| 2.3 | **`UBNRootLayout`**: four `BindWidget` activatable stacks registered by `UI.Layer.*` tag | `UI/BNRootLayout.*` |
| 2.4 | **`UBNUIManager`** (`UGameInstanceSubsystem`, `Config=Game`): per-LocalPlayer root layout + the two VMs, soft widget classes from ini, `PushToLayer/PopFromLayer`, mid-match preload of death/scoreboard classes (a soft class must never sync-load at the death moment) | `UI/BNUIManager.*` |
| 2.5 | **`UBNHUDDirector`** (`ULocalPlayerSubsystem`) — THE producer, the only file that knows gameplay types. Wires: GameStateSet + travel reset → rebind; possession → ASC attribute delegates (via `InitAbilityActorInfo`-complete path) + equipment/ammo delegates + `State.Dead` tag event + respawn stamp; GameState → match delegate, score delegates, killfeed ring. Pushes the HUD on first possession; pushes/pops death overlay on the dead tag; auto-shows scoreboard on `WaitingPostMatch`. Symmetric on every machine | `UI/BNHUDDirector.*` |
| 2.6 | ini: `[/Script/BreachpointNext.BNUIManager]` soft classes at final `/Game/BN/UI/…` paths (assets not built yet — designed miss answer, loud) | `Config/DefaultGame.ini` |

### Wave 3 — the surfaces (UI lane — critic pass)
| # | Task | Class (all bind a VM, nothing else) |
|---|---|---|
| 3.1 | **HUD layout**: Game-layer activatable; `HitTestInvisible` root-to-leaf; never takes focus; no SafeZone wrapper on the canvas; hosts the surfaces + the static center reticle (an Image — per-weapon reticles are deferred) | `UI/BNHUDLayout.*` |
| 3.2 | **Match band**: my kills / limit · clock (VM-owned, phase-locked). The warmup/post-match BANNER is NOT here — it landed on `BNHUDLayout` at the design's own upper-centre anchor, which is where the Figma puts it | `UI/BNMatchBand.*` |
| 3.3 | **Vitals**: shield over health; health hidden until damaged; honest dashes at Unknown | `UI/BNVitalsWidget.*` |
| 3.4 | **Ammo block**: mag / reserve / weapon name; dashes at Unknown, never `0/100` | `UI/BNAmmoBlock.*` |
| 3.5 | **Killfeed** + **entry** (own header — the old module's two-classes-one-header wart, fixed): fixed pool claimed/released, never per-kill CreateWidget; exhaustion drops oldest AND logs; whole-row tint (white=you, red never spent here) | `UI/BNKillfeed.*`, `UI/BNKillfeedEntry.*` |

### Wave 4 — the screens (UI lane — critic pass) + the test doc
| # | Task | Where |
|---|---|---|
| 4.1 | **Death overlay**: GameMenu-layer activatable; "eliminated by X" from the killfeed ring (my own entry), respawn countdown from the stamp; pushed/popped by the director on the `State.Dead` tag event — the widget decides nothing | `UI/BNScreen_Death.*` |
| 4.2 | **Scoreboard**: Game-layer activatable; rows over `PlayerArray` + Kills/Deaths (pooled rows, live refresh via score delegates); hold-Tab = controller-owned UI verb (`Input.Scoreboard` handled in `ABNPlayerController` BEFORE the ASC forward); auto-shown with the winner banner during `WaitingPostMatch` | `UI/BNScreen_Scoreboard.*`, `Match/BNPlayerController.*` |
| 4.3 | **TEST-HUD.md**: the protocol — three views, join-mid-match, death/respawn cycle, post-match, restart | `docs/` |

### G5 — the terminal ticket (after the founder's build)
**`TASK-R7-WBP-HUD`**: all nine WBPs from written trees (layout/anchors only, zero graphs,
parented to the BN classes), the scoreboard input assets (`IA_BNScoreboard`, IMC mapping,
`DA_BNInput` row), read-backs. The bot-asset lane (`Tools/bn/6x` + C++ authoring where scripting
has no surface) is the proven pattern; WidgetBlueprints DO have a Python factory surface, so the
old `wbp_plan.py` shape applies.

## The file structure

```
Source/BreachpointNext/
├── BreachpointNext.Build.cs            W0.1  +7 UI modules
├── Core/BNGameplayTags.{h,cpp}         W0.2  +UI.Layer.* +Input.Scoreboard
├── Match/BNGameState.{h,cpp}           W1.1  +killfeed ring +OnKillfeedChanged
├── Match/BNGameMode.cpp                W1.1  push the ring · W1.3 stamp the respawn time
├── Match/BNPlayerState.{h,cpp}         W1.2  +OnScoreChanged · W1.3 +RespawnAtServerTime
├── Match/BNPlayerController.{h,cpp}    W4.2  +Input.Scoreboard (UI verb, never reaches the ASC)
├── Weapons/BNWeapon.{h,cpp}            W1.4  +OnAmmoChanged
├── Weapons/BNEquipmentComponent.{h,cpp} W1.5 +OnEquippedWeaponChanged
└── UI/                                       (all NEW — BN's UI folder is empty today)
    ├── BNUITypes.h                     W0.3  data-state · killfeed view entry · color tokens
    ├── BNViewModels.{h,cpp}            W2.1  UBNVM_Combat · UBNVM_Match
    ├── BNActivatableWidget.{h,cpp}     W2.2  the ONE base (CommonUI)
    ├── BNRootLayout.{h,cpp}            W2.3  four layer stacks
    ├── BNUIManager.{h,cpp}             W2.4  layouts · soft classes · push/pop
    ├── BNHUDDirector.{h,cpp}           W2.5  THE producer (the only gameplay-aware UI file)
    ├── BNHUDLayout.{h,cpp}             W3.1  Game layer · static reticle
    ├── BNMatchBand.{h,cpp}             W3.2
    ├── BNVitalsWidget.{h,cpp}          W3.3
    ├── BNAmmoBlock.{h,cpp}             W3.4
    ├── BNKillfeed.{h,cpp}              W3.5  pooled
    ├── BNKillfeedEntry.{h,cpp}         W3.5  own header
    ├── BNScreen_Death.{h,cpp}          W4.1  GameMenu layer
    └── BNScreen_Scoreboard.{h,cpp}     W4.2  Game layer · hold-Tab · post-match

Config/DefaultGame.ini                  W2.6  [BNUIManager] soft classes — the only asset naming
Content/BN/UI/    (terminal, G5)        WBP_BNRootLayout · WBP_BNHUD · WBP_BNMatchBand ·
                                        WBP_BNVitals · WBP_BNAmmoBlock · WBP_BNKillfeed ·
                                        WBP_BNKillfeedEntry · WBP_BNScreen_Death ·
                                        WBP_BNScreen_Scoreboard
Content/BN/Input/ (terminal, G5)        IA_BNScoreboard (+IMC row, +DA_BNInput row)
docs/                                   ROADMAP-7 (this) · TEST-HUD · TASK-R7-WBP-HUD
```

## §API — the pinned CommonUI/MVVM surface (transcription contract)

Every signature below is extracted from old-module code that COMPILED against this project's
engine. Waves 2–4 transcribe these; deviating from this section is a finding.

**Activatable base** (`BRActivatableWidget` shape): `UCLASS(Abstract, meta=(DisableNativeTick))
: public UCommonActivatableWidget`. Override `TOptional<FUIInputConfig> GetDesiredInputConfig()
const override` — 3-arg ctor `FUIInputConfig(ECommonInputMode, EMouseCaptureMode, bool)`;
Game = `(Game, CapturePermanently_IncludingInitialMouseDown, bHide)`, Menu = `(Menu, NoCapture,
false)`, Inherit = return an UNSET `TOptional`, never a default-constructed config. Bind/unbind
hooks ride `NativeOnActivated`/`NativeOnDeactivated` (Super FIRST on activate, LAST on
deactivate). Includes: `CommonActivatableWidget.h`, `CommonInputModeTypes.h`.

**Root layout**: `UCommonUserWidget` (NOT activatable) holding four
`UPROPERTY(meta=(BindWidget)) TObjectPtr<UCommonActivatableWidgetStack>` stacks — declared in
`Widgets/CommonActivatableWidgetContainer.h` — registered into a
`TMap<FGameplayTag, TObjectPtr<UCommonActivatableWidgetStack>>` in `NativeOnInitialized`.
Stack API: push = `Stack->AddWidget<T>(WidgetClass)` (templated, takes `TSubclassOf`), remove =
`Stack->RemoveWidget(*Widget)` (REFERENCE), clear = `Stack->ClearWidgets()`. There is no
separate pop call — removal + CommonUI's own back-action are the pop.

**Manager** (`UGameInstanceSubsystem`): per-LocalPlayer lifetime via
`GameInstance->OnLocalPlayerAddedEvent/RemovedEvent.AddUObject` + seeding from
`GetLocalPlayers()`. Layout creation chain: `LocalPlayer->GetPlayerController(World)` →
`SoftClass.LoadSynchronous()` → `CreateWidget<T>(OwningPC, Class)` → `AddToPlayerScreen(0)`;
teardown `RemoveFromParent()`. Preload: `TArray<FSoftObjectPath>` from `ToSoftObjectPath()` →
`UAssetManager::GetStreamableManager().RequestAsyncLoad(...)` held in a
`TSharedPtr<FStreamableHandle>`, released with `ReleaseHandle()`. (The MVVM global-collection
calls exist in the reference and are deliberately NOT ported — R-UI-2.)

**ViewModels**: `: public UMVVMViewModelBase` (`MVVMViewModelBase.h`). Field form:
`UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetHealth", Category = "…",
meta = (AllowPrivateAccess))` — `FieldNotify` is a BARE specifier (not meta), `Getter` names a
public const getter declared above it. Setter: `UE_MVVM_SET_PROPERTY_VALUE(Member, Value)` —
the macro is the broadcast; a plain assignment updates nothing. Works on float/int32/bool/FText/
enums/TArray alike; event pulses are `FMVVMEventField` (`Types/MVVMEventField.h`).

**Widget-side field binding** (the one channel): field id =
`UBNVM_Match::FFieldNotificationClassDescriptor::FieldName` (an `UE::FieldNotification::FFieldId`
— `FieldNotificationId.h` in the header, `INotifyFieldValueChanged.h` in the cpp). Subscribe =
`VM->AddFieldValueChangedDelegate(FieldId,
INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateUObject(this, &ThisClass::Handler))`
with handler `(UObject* Source, UE::FieldNotification::FFieldId FieldId)`. Store
`TArray<TPair<FFieldId, FDelegateHandle>> BoundFields` + `TWeakObjectPtr<VM> BoundViewModel`,
and unbind against THE STORED OBJECT with `RemoveFieldValueChangedDelegate(Id, Handle)` —
never a fresh subsystem lookup (H9). HUD surfaces are `UCommonUserWidget` (never activatable —
activation scope is construct scope, and the activatable base costs a focus hazard).

**Clock**: VM owns `FTimerHandle`; world access via the time source
(`TWeakObjectPtr<AGameStateBase>` → `GetWorld()`); re-arm
`Delay = 1.0 - frac(GetServerWorldTimeSeconds()); if (Delay < 0.01) Delay += 1.0;` — ADD, never
replace — one-shot `SetTimer`, self re-arming.

**Animations**: `UPROPERTY(Transient, meta=(BindWidgetAnimOptional))
TObjectPtr<UWidgetAnimation>` + `PlayAnimationForward/Reverse` only. Never a BIE.

**Build.cs**: Public `UMG, SlateCore, CommonUI, CommonInput, ModelViewViewModel`
(+`FieldNotification`); Private `Slate` the moment any ListView/TileView appears (`STableViewBase`
is Slate — LINK-time failure, both directions proven in the old module's history).

**Layer tags**: CommonUI keys stacks by `FUITag` (`UITag.h`), registered via
`FUITag::AddNativeTag(TEXT("Layer.Game"))` inside a `FGameplayTagNativeAdder` singleton — the
body is ROOT-RELATIVE, `AddNativeTag` prepends its own `"UI."` (critic-corrected: an earlier
draft passed the full string and would have registered `UI.UI.Layer.Game`). The registered tags
are therefore `UI.Layer.*`, the SAME strings the old module lands on — safe: same-string native
registration dedupes without complaint. BN's `UE_DEFINE_GAMEPLAY_TAG` macros produce plain
`FGameplayTag` and cannot declare these. Two registrars, two jobs: `BNTags` for gameplay,
`FBNUITags` for layers.

## Status — 21 Aug 2026

| Wave | State |
|---|---|
| 0+1 (feeds) | **LANDED**, critic-passed — 1 blocking fixed (FUITag root-prepend), ring cleared on restart, respawn stamp InProgress-gated |
| 2 (spine) | **LANDED**, critic verdict SHIP — 0 blocking, 6 of 9 notes fixed (GameStateSet handle, standing-dead screen push, name-fallback killed-by, post-match pops the death screen, killfeed as 4th acquisition edge, no manager on dedicated) |
| 3 (surfaces) + 4 (screens) | **LANDED**, critic verdict HOLD→cleared — 1 blocking fixed (the roster had no join/leave hook while its comment claimed one; opening the board now recomputes), + explicit Game input config on both screens, dimmed Unknown dash, silent empty roster |
| — (geometry) | **MEASURED, not invented** — the ticket's numbers come from the project's own Figma (node `6:47`): vitals x503.33 y66 273.33×20 as an ARC, band x474.67 y622 302×22 with its midpoint deliberately 14.33px left of centre, feed x60 y455 340×76, tray as ONE 280×110 unit |
| G5 | `TASK-R7-WBP-HUD` cut — ten WBPs (the scoreboard row joined), Tab input assets |

**Not compiled** — the founder's next build is the first real test; `TEST-HUD.md` is the protocol.

## R7.1 — what the reference render exposed

Rendering the design at fidelity showed exactly which weapon-section elements have no feed.
Landing the cheap one now; the other two are written down rather than guessed at.

| # | Item | State |
|---|---|---|
| 1.1 | **Weapon icon** — `FBNWeaponRow.Icon` (soft `UTexture2D`) → VM → the ammo block's silhouette slot | **CODE LANDED; the SLOT is open, the gap is not closed.** Nothing fills the column yet, so every row reads null and the Image stays hidden. `TASK-R7-WBP-HUD` Step 5 authors it — art already exists under `Content/UI/HUD/HUD_Weapon_*` |
| 1.2 | **The killing weapon** (the feed's measured `Weapon Glyph` 22×8, and the death screen's weapon line) | **DEFERRED, sized:** the damage door takes a row, not a row NAME, and carrying an `FName` from the door to `FBNLastDamage` needs a custom `FGameplayEffectContext` (with `NetSerialize` + `TStructOpsTypeTraits` + an allocation override) or a server-side pre-stash. That is a gameplay packet with its own critic pass, not a UI rider |
| 1.3 | **The stowed slot** | **FOUNDER DECISION.** The design carries one stowed weapon; BN carries FIVE (Unarmed/Pistol/Rifle/Shotgun/Knife), so "stowed" has no single meaning here. The useful reading is *what one swap press gives you* — the NEXT weapon in the cycle. Say the word and it is a getter on the equipment component plus two VM fields |

## R7.2 — the pause menu (the first interactive screen)

The CommonUI stack existed from Wave 2 and nothing had used it as a *menu* yet. This is that
screen, and it is deliberately two rows: RESUME and LEAVE MATCH. The design's chassis also has
Settings, Controls and File Share — none of those screens exist, and a button that opens
nothing is worse than a button that is absent.

- `UBNScreen_Pause` — `Menu` input, `bIsBackHandler` (Esc/gamepad B closes it), focus lands on
  RESUME so a controller never opens the menu sitting on the row that leaves the match.
- **It does not pause the match, and says so on its own face** — the match is server-
  authoritative and keeps running, so a player who opens this can still be killed. That is the
  design's own warning text, rendered from C++ so it cannot drift in an asset.
- **Opening is the controller's (`Input.Menu`), closing is the WIDGET's.** Once Menu input mode
  is desired, a game input action is not a dependable way back — so Resume and the back action
  own the exit, and re-pressing Esc while it is up is a no-op rather than a second copy.
- `LeaveMatch` lives on the controller (a widget never travels the player) and is a **designed
  miss**: `LeaveMatchMapPath` is commented out in the ini because no front-end map exists, so
  today it logs one loud warning and does not travel. A guessed path strands the player
  somewhere worse than the match.
- Plain UMG `Button`s, not `CommonButtonBase`: the Common variant needs a style asset per
  button and R7 ships none. Named cost — gamepad navigation rides Slate focus rather than
  CommonUI's richer routing, which is fine for two rows and is the trigger to revisit.

Also recorded from the same pass: **grenade and equipment counts do not exist as state anywhere**
(the pips in the design have nothing to bind), and the FFA reading of the design's two team bars
is *me vs the leader* — which needs no rework when teams land.

## Deferred, with named slots

Per-weapon reticle + spread + hit markers (wants the damage cue's confirm param) · grenade pip
(wants grenade-count state) · nameplates (attributes already replicate to observers) · pause
menu & settings & front end (**the stack is now ready for them — that was the founder's point**) ·
medals, damage direction, motion tracker · spectator HUD · MVVM editor bindings (the day a
designer joins).

## Honesty

Written-not-compiled applies to every wave until the founder's build. The named compile risks:
the FieldNotify macro surface and CommonUI's input-config types — both pinned by §API to code
that compiled against this exact engine. The three-views rung applies to every "the HUD works"
claim; join-in-progress claims need a client that joined mid-match.
