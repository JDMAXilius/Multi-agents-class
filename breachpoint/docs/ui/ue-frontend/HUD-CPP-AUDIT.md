# HUD audit — classes, ViewModels, and the built assets, judged the way the menu was

**Status:** v1, 3 Aug 2026. Three parallel read-only audits over the HUD lane: the nine widget
classes, the two ViewModels (~900 lines) **with their producer map**, and — new to this pass —
the seven built WBP assets against their plan, receipts and BP70. Twin of `CPP-AUDIT.md`; every
finding carries a file:line.

**The headline is different from the menu's.** The menu's code was unfinished in its wiring; the
HUD's code is *complete and consumer-perfect* — and **the producer layer does not exist. 19 of 21
ViewModel feeds have zero callers, and nothing ever calls `ShowHUD` or
`CreateLayoutForLocalPlayer`.** The HUD is a fully-wired consumer half bolted to a socket with no
wire in it. Everything else in this file is secondary to that one fact.

---

## 1. Verdicts

| Unit | Verdict | One line |
|---|---|---|
| `UBRVM_Combat` | **FIX** | Sound architecture; zero producers; can declare `Live` on a partial attribute set (renders a 0% health bar on a living client) |
| `UBRVM_Match` | **FIX** | Timer discipline genuinely good — and the phase-lock re-arm has a self-perpetuating bug; killfeed keeps a hand-synced parallel expiry array with a `0.0`-time poison path |
| `UBRVitalsWidget` | **FIX** | Correct data path; state latch survives destruct; D3's real story is below |
| `UBRProgressBar` | **FIX** | The hidden-until-damaged rule hides only `Fill`+`Track` — `Frame`/root keep drawing; `Treatment`/`Channel` are `EditAnywhere`, the exact fork its own comments forbid |
| `UBRAmmoBlock` | **FIX** | Two of four readout states unreachable by construction, taking a switcher, a text block and a colour branch with them |
| `UBRReticleWidget` | **FIX** | Best-behaved class in the folder — carrying the menu's exact lifecycle bug: preload cancelled in destruct, never re-issued in construct |
| `UBRMatchBand` | **FIX** | Re-base: uses nothing from the activatable base but two getters |
| `UBREquipmentTray` | **FIX** | Clean timers; stale BOTTOM-LEFT doc (it's bottom-right); local-time cooldown vs the band's server-time doctrine |
| `UBRKillfeed` | **FIX** | Pooling is the best-engineered thing in the folder — do not touch it; pool-failure path retries (and sync-loads) forever |
| `UBRKillfeedEntryWidget` | **FIX** | Still zero `BindWidget` members + a BIE-only update path — the R18 violation stands; the plan pre-commits the exact 4-name contract that fixes it |
| `UBRHUDLayout` | **FIX** | The one legitimate activatable; carries 4 unimplementable BP hooks that duplicate the reticle's own subscription |
| `FBRCombatAttributeBindings` | **FIX (move)** | 2 consumers, 17 files paying `AttributeSet.h` for it; `BRViewModels.h` is header-included by nothing → zero cascade |
| `EBRUIDataState` · `EBRHitMarkerKind` · `FBRUITags` · the killfeed pool · `UBRActivatableWidget` | **KEEP** | Stated so nobody re-litigates |
| 7 `FMVVMEventField`s + `FBROnKillfeedEntryAdded` | **CUT** | Broadcast into the void — widgets consume the native delegates for the same three signals; two channels, one consumer |

**Counts: KEEP 5 · FIX 12 · CUT 8** (member-level: ~25 dead members, 13 unimplementable
`BlueprintImplementableEvent`s, 7 stale comments that actively mislead about geometry).

---

## 2. The defect ledger — ranked

| # | Sev | Where | Defect |
|---|---|---|---|
| H1 | **P0** | the whole module | **No producer layer.** 19/21 feeds dead; `ShowHUD` and `CreateLayoutForLocalPlayer` have zero callers — the HUD is never created and never fed. The good news: `ABRGameState` already publishes symmetric host/client delegates for all four match signals (each `Server*` mutator calls its own `OnRep_` locally), so a producer written against them fires identically on host and client |
| H2 | **P1** | `BRViewModels.cpp:153,180` | **`Live` on a partial attribute set.** `bAnyFound` on ANY of four attributes → `Live`; a client where `Health` replicated before `MaxHealth` renders an empty health bar on a living player. Host correct, client wrong — different digits, live today. Also: the initial publish sets `bShieldsBroken` without firing the paired events |
| H3 | **P1** | `BRViewModels.cpp:457-461` | **Clock phase-lock is self-perpetuating.** A fire at `frac≈0.995` computes `Delay=0.005`, gets forced to `1.0`, and lands at `frac≈0.995` again — one hitch and the clock ticks a second out of phase for the rest of the match. Fix: `Delay += 1.0`, not `= 1.0` |
| H4 | **P1** | no travel hook anywhere | **Seamless travel strands both VMs:** timers die with the old world, `MatchState` stays `Live` with frozen scores, carried killfeed entries expire on the old world's clock (old kills persist through the new match). Needs `PostLoadMapWithWorld` → `ClearToUnknown` × 2 + rebind |
| H5 | **P1** | `BRViewModels.cpp:274,484,513` | **Spotter line can attach to the wrong row per machine.** The per-VM fallback sequence id differs host vs client; `AppendSpotterLine` matches on it. Producer must always use the server-assigned `FBRKillFeedEntry::Sequence`; delete the fallback |
| H6 | **P1** | `BRProgressBar.cpp:99-117` + `BRVitalsWidget.cpp:134-138` | **D3, the real story (see §3).** The rule exists but (a) gates on health percent, never shield state, (b) hides only `Fill`+`Track` while `Frame`/`ValueText`/root keep drawing — the "gold bar" in the founder render is most plausibly the frame of a bar whose fill IS hidden — and (c) uses `Hidden` where the ticket demands `Collapsed` |
| H7 | **P2** | 5 of 6 HUD roots + ~12 leaves | **`HitTestInvisible` is asserted by nobody.** One class sets it (`BRReticleWidget.cpp:69`); the generator can't author visibility; the rest default to hit-testable. Masked today by `InputMode=Game` mouse capture; unmasked the moment a `GameAndMenu` screen sits above |
| H8 | **P2** | `BRReticleWidget.cpp:93/112` | The menu's lifecycle bug again: `PreloadArt()` in `NativeOnInitialized`, handle cancelled in `NativeDestruct`, never re-issued — after one viewport re-add every weapon swap sync-loads on the swap path |
| H9 | **P2** | `BRReticleWidget.cpp:152`, `BRMatchBand.cpp:92`, `BRHUDLayout.cpp:61,72` | **Unbind by re-lookup, not stored pointer** — if the subsystem swapped the VM between construct and destruct, `RemoveAll(this)` runs on the wrong object and the binding outlives the widget. Three classes; the other three already store `TWeakObjectPtr` correctly |
| H10 | **P2** | `BRVitalsWidget.h:118`, `BRAmmoBlock.h:146` | State latches never reset in destruct — a second construct skips the state push and the WBP re-enters the tree stale |
| H11 | **P2** | `BRKillfeed.cpp:86-98` | Pool failure retries forever: null entry class re-warns and re-`LoadSynchronous`es on every activation. Needs a build-attempted latch |
| H12 | **P2** | `BRViewModels.cpp:496-500` | Killfeed push with a null world stamps expiry on a `0.0` clock and arms no timer — entries never expire or all burst at once. Bail on push instead |
| H13 | **P3** | `BRMatchBand.cpp:79-83` vs `:145-146` | The band gates scores on `LocalTeamId` but doesn't subscribe to it — a late team-id lands after the last score push and the band shows dashes for the match. Also scores default to a confident `0`, not unknown |

---

## 3. BP70 — the three defects, re-judged against evidence

**D1 (duplicate ammo readout).** The receipts **cannot** exonerate the asset: `build_wbp.py`
deletes before creating, so its `tree matches plan` check only ever sees what it just built —
**the gate exists (`build_wbp.py:243-249`, computes `extra`, returns False) but is unreachable
for the defect it names.** The ticket's "proven against a deliberately stale asset" cannot be
satisfied by the code as written; what's missing is a `--verify` mode (~20 lines) that skips the
delete. Also: **no committed receipt matches the current plan digest** (`67f7732f…` vs the last
build's `1e0f3c4c…`) — a rebuild at the current digest closes D1 by construction and is the
highest value-per-effort item in the whole lane. The latent C++ twin is already filed: **two
killfeed implementations read the same array** (`wbp_plan.py:769-777`) — authoring both draws
the feed twice; one must be deleted.

**D2 (blank rectangle).** Not produced by the current plan at all — neither tray widget has a
single `UImage` child, so the rectangle is a stale widget (D1's class) or an unrebuilt asset.
The prescribed placeholder **`T_UI_Weapon_Unknown` does not exist** in `Content/UI`; D2's
default-brush fix is blocked on one texture. Two plan-side blockers noted for when it lands:
`_BRUSH_CLASSES` accepts only `UMG.Image` (no `CommonLazyImage`), and on this checkout **every
`.uasset` under `Content/UI` is an LFS pointer stub (169 files)** — a checkout artifact, but it
means no brush write can be verified from this tree.

**D3 (health bar at full shields) — the audits disagreed, and the deeper read wins.** The rule
exists (`bHiddenUntilDamaged` in `BRProgressBar.cpp:99-105`) and the ticket's own prescription
("bind to `GetHealthPercent() < 1.0`") is *what the code already does* — so the ticket's fix
would change nothing. Three real gaps:
1. The gate reads **health percent, never shield state**. If the design is Halo-model
   (shields regen, health persists), a health-damaged player at full shields shows the bar
   forever. `bShieldsBroken` is already FieldNotify and already in the widget's observed list.
2. **Only `Fill` and `Track` hide.** `Frame`, `ValueText`, `LabelText` and the widget's own
   root never do — the most likely literal cause of the render.
3. `Hidden` vs the ticket's `Collapsed`: in a canvas slot they are identical (no reflow), so
   the ticket's prescription is a no-op *and* the conflict must be ruled, not patched.

**Fix shape (one place):** `UBRProgressBar::SetHiddenUntilDamagedGate(bool)` fed from
`AreShieldsBroken()`, visibility set on `this` rather than two named children.
**FOUNDER DECIDE:** is the gate health-damage (current code, per UI-DESIGN-SYSTEM §1's "health
appears only after damage") or shield-state (the widget auditor's reading of the render)?

---

## 4. The activatable question — ruled

**Re-base `UBRMatchBand` and `UBRKillfeed` onto `UCommonUserWidget`. The earlier verdict
("bAutoActivate=true, verified handled") checked that binding fires — not whether activation was
needed to make it fire. It isn't:** neither is ever pushed to a layer (the only four
`PushWidgetToLayer` call sites are the subsystem's `Show*`), so activation fires from
`NativeConstruct` anyway — activation scope *is* construct scope wearing a CommonUI hat. The
nested proof: `WBP_DeathRespawn` hosts a `MatchBand` that auto-activates on its own construct
regardless of whether its (deliberately non-auto-activating) parent screen is active.

What the base *costs* them: `bSupportsActivationFocus` defaults **true** (the one live focus
hazard on the HUD — `BRPanel_Toast` had to turn it off explicitly), and CommonUI's
`EditAnywhere` visibility-on-activation knobs hand a C++-owned surface to a details panel.
The re-base is ~15 lines each: bind/unbind move to `NativeConstruct`/`NativeDestruct` (fixing
H9's stored-pointer issue in the same motion), `EnsurePool` moves to construct where its
`GetOwningPlayer()` dependency belongs. After it, **"activatable = screens only" is literally
true and grep-checkable**, with `UBRHUDLayout` the single documented exception because it
genuinely declares an input config.

---

## 5. The BlueprintImplementableEvent problem

**Thirteen BIEs across the HUD, every one unimplementable under R18/R26** (a BIE needs a graph
node; WBPs have empty graphs). The Components folder already retired this pattern —
`BindWidgetAnimOptional` played from C++, five classes, reason documented. The HUD folder is the
last holdout. Four of the thirteen are also **duplicate subscriptions**: `UBRHUDLayout`
subscribes to the same hit-marker multicast the reticle consumes, then fires four hooks nobody
can implement. The two state-latch bugs (H10) exist only to gate these hooks — retiring the
hooks retires the latches.

**The highest-value single instance is `UBRKillfeedEntryWidget`** (BP66): zero binds, BIE-only
update, five dead accessors. The plan pre-commits the exact contract
(`RootSizeBox`/`Row`/`KillerNameText`/`WeaponIcon`/`VictimNameText`/`SpotterLineText`,
`wbp_plan.py:477-498`) — landing those names makes the already-planned asset correct with zero
re-authoring and turns `validate()` on automatically. Near-free bonus: move the class to its own
header, deleting the two-classes-one-header workaround that both tools carry (`class_body()`).

---

## 6. Doctrine vs plan vs Figma — the doc corrects, not the plan

Spot-checked geometry: the plan's canvas arithmetic is **exact** in all three checks (vitals
centre lands on 640.0 to the decimal; tray and killfeed offsets reconcile). Every mismatch found
is **doctrine vs Figma**, and the plan sides with Figma, which `figma_hud_layout.json` declares
authoritative for position:

| LAYOUT-DOCTRINE §7 says | Measured/plan truth |
|---|---|
| Vitals offset (0, **24**) | y = **66** |
| WeaponTray (−52.67, −144) 196×104, one widget | two widgets (EquipmentTray 140×34 @−200,−106 · AmmoBlock 218×60 @−62,−36); Figma says the frame is ONE 280×110 unit — **the split is an unrecorded design decision** (FOUNDER DECIDE with the InvalidationBox question, which needs a common parent) |
| Killfeed **TOP-RIGHT** (−43, 60) | plan already built **bottom-left** (60, −189) — the ticket's open founder call, and the plan pre-empted it (FOUNDER DECIDE) |

**Missing surfaces — no class, no art, no plan node:** MotionTracker, DamageDirection,
MedalPopup; HitMarkers has a class but no imported art; `interaction_prompt` and
`objective_waypoint` exist only as Figma rects. These are new packets, not BP70 work — recorded
so nobody reads the six-child layout as complete.

---

## 7. What was clean

Killfeed pooling (rows created once, claimed/released, exhaustion logged) — the best-engineered
thing in the folder. Zero Tick, zero accumulators (the tray's 20 Hz ring interpolates between
absolute instants). Both timers cleared on destruct. Attribute bind/unbind mechanics correct
(matched tag events, right teardown order) — only the invocation is missing. FieldNotify
discipline complete; derived getters broadcast with their inputs. VM-formats-clock /
widget-formats-integers split is disciplined. Zero function-local static brushes. `ABRGameState`
is symmetric host/client — the single biggest reason H1 is cheap to fix correctly.

---

## 8. The packets

### HUD-A — "The producer" *(terminal lane · H1, H4, H5 · the one that makes everything real)*
One class (`UBRHUDDirector` or equivalent) owned by `ABRPlayerController`: subscribe to the four
`ABRGameState` delegates + `UBRWeaponInstance::OnAmmoChanged` +
`UBREquipmentComponent::OnEquippedWeaponChanged` + the PlayerState ASC; call
`CreateLayoutForLocalPlayer` + `ShowHUD` on possession; `PostLoadMapWithWorld` →
`ClearToUnknown` ×2 + rebind; always populate `SequenceId` from the server-assigned sequence.
File contract gaps for the six signals with no game-side event (team id, grapple, rocket,
hit-marker confirm, spotter, respawn countdown). ~200 lines; touches gameplay code, so it is a
netcode-adjacent packet with a critic pass, not a UI-lane free action.

### HUD-B — "VM correctness" *(terminal lane · H2, H3, H12, H13 + the move + the cuts)*
Gate `Live` on non-zero denominators; broadcast the shield events from the initial publish;
`Delay += 1.0`; bail killfeed push on null world; fold expiry into `FBRKillfeedViewEntry` and
delete the parallel array + fallback id; score defaults to unknown (or the band subscribes to
`LocalTeamId`); move `FBRCombatAttributeBindings` to `BRViewModels.h`; cut the 7 void event
fields + the zero-subscriber delegate.

### HUD-C — "Widget lifecycle + laws" *(terminal lane · H7-H11 + re-base + BIE retirement + dead surface)*
Re-base band + feed onto `UCommonUserWidget` (§4); stored-pointer unbind in the three lookup
classes; `PreloadArt` to construct; pool latch; `BRUI::MakeHudPassive` + root `HitTestInvisible`
on all six surfaces; retire the 13 BIEs (BP66's four binds first — the plan contract is
pre-written); `NativePreConstruct` on tray + progress bar and demote `Treatment`/`Channel` from
`EditAnywhere`; the ~25 dead members; the 7 stale comments.

### HUD-D — "The asset gate + rebuild" *(editor lane · D1, D2)*
`--verify` mode in `build_wbp.py` (~20 lines, comparison already exists) proven against a
deliberately stale asset; rebuild everything at the current plan digest and commit the receipt
(closes D1 by construction, builds the never-built `WBP_MenuRow`); `INVALIDATION` class
constant; the three doctrine coordinate corrections. D2 waits on `T_UI_Weapon_Unknown` (art).

### FOUNDER DECIDEs — block their items, nothing else
1. **D3's gate:** health-damage (current code + UI-DESIGN-SYSTEM §1) or shield-state? And
   Hidden vs Collapsed (a no-op in canvas — rule it so the ticket box can be ticked).
2. **Killfeed corner:** doctrine says top-right, the built plan says bottom-left, the render
   showed mid-left. One of the three wins.
3. **The tray split:** one 280×110 widget (Figma + doctrine) or the shipped two? Decides the
   InvalidationBox shape.
