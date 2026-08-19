# RESEARCH — the R7 HUD: what exists, what's missing, and how little we need

**Cut:** 19 August 2026 by the cloud lead · founder's brief: *"let's discuss UI HUD — deep
research and ultra thinking."* Discussion artifact — nothing here is applied yet.

Sources: full inventory of the OLD module's UI (98 files, 20,709 lines, `Source/Breachpoint/UI/`),
a feed audit of every BN class a HUD would bind, the `ue5-ui-architecture` and `ui-presentation`
skills, and the old module's HUD-CPP-AUDIT rulings.

---

## 1. The one sentence, inherited and kept

**The HUD is a projection of replicated state — every value arrives by delegate or RepNotify,
never by asking.** The old module built its whole UI on this and the audits proved it right.
R4 already built the server half of this doctrine without knowing it: the match clock is an end
stamp computed locally, the winner is a RepNotify, the state change is one delegate on every
machine. R7 is the client half.

## 2. What the deep research found

### 2a. The old module is a pattern library, not a parts bin

~20.7k lines of UI, and the coupling map is one file deep: **everything except `BRHUDDirector`
binds only to ViewModels or engine types.** The ViewModels, the honest-unknown discipline, the
killfeed pool ("the best-engineered thing in the folder — do not touch it"), the match-band
binding shape, the clock phase-lock — all port to BN as transcription, not design work. The
director (game→ViewModel wiring, ~422 lines) is the one real rewrite, because every line of it
names an old-module gameplay type.

**No old WBP asset is reusable** — a widget Blueprint's parent class is baked into the binary,
and all of them parent old-module `BR` classes. The *plan* that generated them is committed text
(`mcp-ui/gen_ui/wbp_plan.py`) with measured Figma geometry, and geometry ports.

**Plugins cost nothing**: CommonUI and ModelViewViewModel are already enabled at project level.
`BreachpointNext.Build.cs` has zero UI deps — and the old module's hard-won warning stands:
missing `SlateCore` fails at LINK with 13 unresolved externals, not at compile.

### 2b. The BN feed audit — per element

| HUD element | Feed today | Gap |
|---|---|---|
| **Vitals** (health/shield) | ✅ complete — attributes replicate `COND_None` with RepNotify; change delegates proven client-side (`BNCharacter.cpp` binds them already) | none |
| **Match band** (phase/clock/limit) | ✅ complete — `OnMatchStateChanged` fires on every machine; clock is the end stamp + local compute; `ScoreLimit` mirrored | none |
| **Post-match** (winner/scoreboard rows) | ✅ complete — `OnRep_Winner` handles the late-GUID join case; rows are `PlayerArray` + replicated Kills/Deaths | live refresh needs the score delegate below |
| **Score readout** (my kills) | ⚠️ state replicates | `OnRep_Kills/Deaths` only log — their own comments say "the scoreboard binds here later." **Later is now**: add one delegate, broadcast from the OnReps AND the authority mutators (a listen host runs no OnReps) |
| **Ammo + weapon** | ⚠️ state replicates (`CurrentAmmo`/`AmmoReserve`/`RowName`/`CurrentIndex`, all `COND_None`) | the OnReps are empty (comments claim "the HUD binds here" — an OnRep is not bindable). Add `OnAmmoChanged` on the weapon and `OnEquippedWeaponChanged` at the tail of `ApplyCurrentWeapon` (already runs on every machine) |
| **Killfeed** | ❌ nothing | **the one hard gap.** `HandlePlayerDeath` is authority-only and its whole output is three log lines; `OnPlayerDeath` is authority-gated; no multicast, no replicated array, no killer-carrying cue. A client cannot learn "X killed Y" by any route. Fix: a replicated fixed-size ring on `ABNGameState` with server-assigned `Sequence`, pushed where the kill line prints — the OnRep+ring is join-in-progress-correct where a multicast RPC is not |
| **Death overlay** | ⚠️ half | "am I dead" — yes (`State.Dead` GE tag, client read already proven in `BNPlayerController::IsDead`). "Respawn in N" — no: `RespawnDelay` is server config and the timer is anonymous. Fix: stamp `RespawnAtServerTime` on `ABNPlayerState` (`COND_OwnerOnly`), computed locally — the match clock's own proven pattern |
| **A HUD to hang it on** | ❌ nothing | BN has no widget, no `HUDClass`, no UI module deps; the screen today is empty |

**Cross-cutting fact:** every BN delegate is a native `DECLARE_MULTICAST_DELEGATE` — no WBP can
bind any of them. A C++ producer layer is mandatory regardless of any other choice, which is the
doctrine anyway.

## 3. The three rulings this research recommends

### R-UI-1 — Port patterns, never reuse old classes. Fresh `BN` classes in `BreachpointNext/UI/`.
The old UI can't boot in BN anyway (it gates on `ABRGameState`, which never spawns), its WBPs
can't be reparented without binary surgery, and one module owning its whole runtime is the BN
covenant. What ports is knowledge: the honest-unknown gate (`Live` requires non-zero
denominators, not "some attribute arrived"), the clock phase-lock (`Delay += 1.0`, re-armed on
the whole server second so every readout shows the same digit), the pool-and-project killfeed,
unbind-against-the-stored-object (H9), `BindWidgetAnimOptional` played from C++ — **never a
BlueprintImplementableEvent** (empty-graph law makes BIEs unimplementable; the old module wrote
thirteen before noticing).

### R-UI-2 — MVVM ViewModels yes, MVVM machinery no.
Keep `UMVVMViewModelBase` + FieldNotify for the two ViewModels (`UBNVM_Combat`, `UBNVM_Match`) —
that code ports near-verbatim from proven, compiled prior art, and field-level notification is
exactly the granularity a HUD wants. **Drop** the global ViewModel collection and the per-widget
MVVM-view push: the old module itself noted those are a second channel for the same data, used
only by editor-authored bindings — and BN's WBPs will have none (C++ binds everything).
One channel: widgets subscribe `AddFieldValueChangedDelegate` in C++.

### R-UI-3 — No CommonUI in R7. The HUD needs no input, no focus, no stack.
Everything R7 ships is non-interactive: the HUD is `HitTestInvisible` root-to-leaf, the death
overlay is informational (respawn is automatic), the scoreboard is hold-to-view, the post-match
banner takes no clicks. Zero input-mode changes means the activatable stack solves a problem R7
does not have. **Named trigger to reopen:** the first interactive screen — pause menu, settings,
or front-end — brings in CommonUI + the layer stack, and the old module's `RootLayout` /
`ActivatableWidget` / layer-tag trio is the ~800-line port waiting for that day. (Watch on that
day: the old module natively registers `Layer.*` gameplay tags — a duplicate registration
asserts if both modules load.)

## 4. The shape — radically smaller than the prior art

**Six UI files, one WBP asset, five small gameplay additions.**

```
Source/BreachpointNext/
├── BreachpointNext.Build.cs          +UMG, Slate, SlateCore, ModelViewViewModel, FieldNotification
│
│   Wave 1 — THE FEEDS (gameplay lane, critic pass — this is netcode)
├── Match/BNGameState.{h,cpp}         +FBNKillfeedEntry ring (fixed size, server Sequence,
│                                      ReplicatedUsing) + OnKillfeedChanged delegate
├── Match/BNGameMode.cpp              push the ring where the kill line prints;
│                                      stamp RespawnAtServerTime in RequestRespawn
├── Match/BNPlayerState.{h,cpp}       +OnScoreChanged (broadcast from OnReps AND AddKill/AddDeath);
│                                      +RespawnAtServerTime (COND_OwnerOnly, OnRep + delegate)
├── Weapons/BNWeapon.{h,cpp}          +OnAmmoChanged (broadcast from both OnReps AND
│                                      ConsumeAmmo/Reload — a listen host runs no OnReps)
├── Weapons/BNEquipmentComponent.{h,cpp}  +OnEquippedWeaponChanged (tail of ApplyCurrentWeapon)
│
│   Wave 2 — THE PROJECTION (UI lane)
└── UI/
    ├── BNViewModels.{h,cpp}          UBNVM_Combat + UBNVM_Match (+EBNUIDataState, the killfeed
    │                                  view ring). Honest-unknown, clock phase-lock, ports in.
    ├── BNHUDDirector.{h,cpp}         ULocalPlayerSubsystem. THE producer: binds GameState
    │                                  arrival, possession, ASC attributes, equipment, score,
    │                                  killfeed → pushes ViewModels. Config soft class for the
    │                                  layout (no separate settings object — the subsystem IS
    │                                  the config owner). Creates + AddToPlayerScreen.
    └── BNHUDLayout.{h,cpp}           ONE widget class, all panels: vitals bars, ammo block,
                                       match band, killfeed lines (fixed pool in the tree),
                                       death panel, warmup banner, post-match panel with fixed
                                       scoreboard rows, static center reticle. HitTestInvisible.
                                       BindWidget primitives; panels shown/collapsed by C++.

Config/DefaultGame.ini                [/Script/BreachpointNext.BNHUDDirector]
                                      HUDLayoutClass=/Game/BN/UI/WBP_BNHUD.WBP_BNHUD_C
Content/BN/UI/WBP_BNHUD               the ONE asset — terminal ticket TASK-R7-WBP-HUD:
                                      layout + anchors only, zero graph nodes, parented to
                                      UBNHUDLayout. Geometry from the measured 1280×720 grid.
```

**Why one layout class instead of the old module's six surface widgets:** the surfaces were
separate classes because they were reused across screens (the match band lives inside the death
screen too). In R7 every panel lives in the same single layout, so the split buys nothing and
costs five WBPs and five classes. **Named trigger to split:** the first surface needed in a
second layout. The killfeed keeps the pool doctrine — fixed rows in the WBP tree, claimed and
released, never created per kill; exhaustion drops the oldest and logs it.

**What the terminal does: one asset.** The whole fence is one WBP with a written tree, zero
graphs, and a read-back. No MVVM bindings to author, no style assets yet, no input assets.

## 5. Design system, inherited semantics

The VISR-derived tokens carry over as **meanings**, C++-owned constants (a WBP never types a
hex): cyan `#35D0F2` = you/yours · health `#F5C542` yellow, **never green** · amber `#FFA333` =
a clock runs (respawn countdown, match clock low) · red `#FF4A3D` = threat only — never spent on
low ammo or UI errors · white = you in a list (your killfeed lines, your scoreboard row).
Grid: base 1280×720, the measured margins/gutters from the old plan where they apply. Death
overlay and post-match get the flat treatment; the in-match elements get the full semantics.

## 6. Join-in-progress honesty (the frames nobody tests)

Ported rule, verbatim: ViewModels initialize to explicit **Unknown**, widgets render dashes —
never a confident `0/100`; `Live` requires the denominators, not merely an arrival; subscribers
read state once at bind time (the `BNGameState.h` contract already documents this); everything
re-binds through the possession delegate because the PlayerState is null for the first frames.
Two-window PIE with a mid-match join is the rung that counts.

## 7. Deferred, with named slots

Grenade count pip (no grenade-count state exists — deferred with the grenade-ammo decision) ·
per-weapon reticle + spread + hit markers (needs a confirmed-hit client signal — the damage cue
carries one when we want it) · nameplates over heads (attributes already replicate to observers;
free feed, later wave) · damage direction, medals, motion tracker (old module never built them
either) · pause menu / settings / front end (**the CommonUI trigger**) · MVVM editor bindings
(the day a designer joins) · spectator HUD.

## 8. Honesty

Everything above is a plan against read code. The old-module patterns were compiled and audited
in THEIR module; their transcription into BN is written-not-compiled like everything since R4.
The FieldNotify macro surface is the named compile risk (the skill flags it; the old module's
working ViewModels are the reference to transcribe exactly).
