---
name: ue5-ui-architecture
description: CommonUI + MVVM implementation patterns for BREACHPOINT (UE 5.8). Load for ANY work touching Source/Breachpoint/UI or Content/UI — the layer stack, activatable widgets, ViewModels, HUD binding, killfeed pooling, front-end flow, gamepad routing, or join-in-progress states. This is BP10's reference layer. There is no UI contract file; the binding law is CLAUDE.md (laws 3, 4, 5) + ARCHITECTURE §3.9 — on any conflict those win and the conflict is a finding against this skill.
---

# UE5 UI Architecture — the lawful CommonUI/MVVM patterns

**Authority note (read first):** unlike `gas-purity`, this skill has no contract file behind it.
Its law comes from `CLAUDE.md` (law 3 soft refs · law 4 no gameplay Tick · law 5 owner paths) and
`BREACHPOINT-ARCHITECTURE.md` §3.9. Where this skill and those disagree, **they win** and the
conflict is a finding against this skill — fix it in the same packet.

**The one sentence:** the UI is a *projection of replicated state*, never a source of it. Every
value on screen arrives by delegate or RepNotify, never by asking.

## 1. The layer stack (`BRUIManagerSubsystem`)

- A `UGameInstanceSubsystem` owning the CommonUI layer stack: **GameHUD / Menu / Modal**.
  Push and pop by layer tag; never parent widgets to each other across layers.
- Layers are a **stack, not a visibility toggle** — a modal pushes, it does not hide the HUD by
  setting `ESlateVisibility`. Popping restores focus to the layer beneath automatically, which is
  what makes gamepad back-navigation work without bespoke code.
- Widget classes are **soft** (`TSoftClassPtr<UBRActivatableWidget>`) resolved at push time
  (law 3). A hard `UPROPERTY` widget-class pointer in C++ is a finding.

## 2. `BRActivatableWidget` — the one base

- Subclass of `UCommonActivatableWidget`. Every screen and panel derives from it; there is no
  second widget base.
- Override `GetDesiredInputConfig()` to declare input mode (`Game`, `GameAndMenu`, `Menu`) and
  mouse capture. **This is how the front end and the HUD hand input back and forth** — never call
  `SetInputMode*` from a controller or widget body.
- `NativeOnActivated` / `NativeOnDeactivated` are the only lifecycle hooks that bind and unbind
  delegates. Bind on activate, **unbind on deactivate** — a leaked ViewModel binding that outlives
  its widget is the crash the death cam finds.

## 3. ViewModels (`BRViewModels` — `UBRVM_Combat` + `UBRVM_Match`)

The MVVM layer is where the no-Tick law is actually enforced.

- Both are `UMVVMViewModelBase` with `UPROPERTY(FieldNotify, Setter, Getter)` fields. The setter
  calls `UE_MVVM_SET_PROPERTY_VALUE` — that macro is what broadcasts; a plain assignment updates
  nothing and looks like a "UI doesn't refresh" bug.
- **Feeds, and only these:**
  | ViewModel | Fed by |
  |---|---|
  | `UBRVM_Combat` | ASC attribute-change delegates (Shields/Health), equipment RepNotify (ammo, active slot), grenade count, `GE_Cooldown` tag events (grapple ring) |
  | `UBRVM_Match` | GameState RepNotify (phase, team scores, `MatchEndServerTime`), killfeed ring-buffer delegate, `ABRPowerWeaponSpawner` countdown RepNotify |
- **The match clock is computed, not replicated.** `MatchEndServerTime` is one replicated float;
  the ViewModel derives remaining time locally from it. A ticking replicated countdown is a
  bandwidth finding *and* it desyncs across the three views.
- Attribute *presentation* smoothing (shield bar lerp) lives here as a visual interpolation only —
  it never feeds back into simulation (ARCHITECTURE §5.3).

## 4. `BRHUDLayout` — binding, not logic

- C++ owns the bindings; **BP subclasses hold layout and art ONLY**. A BP graph with a branch on
  gameplay state is a finding — that decision belongs in the ViewModel.
- Elements per the GDD §2.9: shields-over-health (top-left), weapon + ammo + ghosted second weapon
  (bottom-right), grenade count + grapple cooldown ring (bottom-left), reticle with
  **shield-hit vs flesh-hit markers** (center), team score + timer + rocket countdown (top-center),
  killfeed + medals, death overlay, carnage report.
- **Hit markers are gameplay information, not decoration** — they are driven by the damage cue's
  parameters (shield vs flesh), so they arrive on the same path as every other confirmed effect.

## 5. The killfeed pool (the one place widgets are recycled)

- Fixed-size pool created on activate; entries are **claimed and released**, never
  `CreateWidget`/`RemoveFromParent` per kill — that path allocates during a firefight.
- The ring buffer is authoritative (GameState); the pool renders a window over it. **Pool
  exhaustion drops the oldest visible entry, and the drop is logged** — a silent cap reads as
  "covered everything" when it didn't (game-lead honesty law).
- Each entry reserves an append slot for the Spotter line. The slot renders empty when the string
  is empty — it never collapses layout or waits on the LLM. Offline ⇒ identical HUD minus flavor.

## 6. Gamepad routing (CommonUI does this, you do not)

- Navigation, focus, and back are CommonUI's job: set `bAutoActivate`, declare desired focus
  targets, and route back through the activatable stack. Hand-rolled focus math is a finding.
- Every interactive path must be reachable without a mouse: **menu → match → death → rematch**.
  Verify with the controller only, unplugged mouse — the M6 stranger test allows no coaching.
- Input actions reach widgets through the same `BRInputConfig` tags the ASC uses (ARCHITECTURE
  §3.2). The UI never binds a raw key.

## 7. Join-in-progress and honest empty states

The frames a late joiner sees are the ones nobody tests. All three happen:

- `PlayerState` is **null** for one or more frames after possession.
- The ASC has not run `InitAbilityActorInfo` yet, so attribute delegates have not fired.
- GameState arrives before team assignment.

Rules: ViewModels initialize to an explicit *unknown* state, not to zero; widgets render an honest
placeholder (dashes, dimmed) rather than a confident `0/100`; nothing dereferences `PlayerState`,
`ASC`, or `PlayerController` without a null check on the first frames. **A stale or garbage first
frame is a finding, not a cosmetic nit** — it tells the player something false about a fight.

## 8. What the grep gate and the critic will catch (self-check before handoff)

`NativeTick` in any widget · a UMG **property binding** in a BP graph (the per-frame poll wearing a
different hat) · `GetPlayerState()`/`GetASC()` polled from a widget instead of delegate-fed ·
a widget writing to authoritative state (calling a Server RPC directly rather than through the
controller's UI-intent boundary) · `CreateWidget` inside a per-kill path · a hard widget-class
`UPROPERTY` or `ConstructorHelpers` · a gameplay literal in a widget · `SetInputMode*` outside
`GetDesiredInputConfig()` · a BP subclass containing gameplay branching · an unbound delegate
surviving `NativeOnDeactivated`.

**Rung honesty (always):** the HUD "works" only when it has been read on all three of server,
acting client, and observing client — a scoreboard that is right on the host and wrong on a remote
is the exact bug this layer exists to surface. Join-in-progress claims need a client that joined
*mid-match*, not one that was present at map load.
