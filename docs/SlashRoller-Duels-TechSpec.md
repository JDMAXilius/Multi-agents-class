# Slash Roller: Duels — Technical Specification
## GAS-First Engineering Spec for the Capstone Cut

**Companion to:** `assignments/02-gdd-final-draft/GDD.md`
**Standards:** inherits all Non-Negotiable Engineering Rules from the master
context doc (pure native C++, no-Tick, server-authoritative, GAS purity,
PlayerState-owned ASC, SOLID subsystems, no dangling state on cancel,
abstraction at migration seams). Every system below is declared the way the
house rules require: **owner → replication → cancel/interrupt → simulated
proxy**, before implementation.

---

## 0. What Already Exists (do not rebuild)

| Shipped system | Reused as-is |
|---|---|
| GAS core (PlayerState ASC, input buffer, prediction keys) | All combat |
| Soft-lock melee (`SoftLockTarget` replicated source of truth) | All strikes |
| `OSSessionsSubsystem` (Steam OSS, host/find/join) | Online 1v1 |
| AL Framework + 7 custom AnimGraph nodes, Motion Matching | Locomotion/combat anim |
| Wwise-over-GAS pipeline (rollback-safe audio) | All feedback |

New engineering surface is **five systems**, specced below.

---

## 1. Round & Match Flow

- **Owner:** `AOSDuelGameMode` (server-only logic) + `AOSDuelGameState`
  (replicated match state). New `UOSRoundComponent` on GameState holds the
  round state machine so GameState stays thin.
- **State machine (no Tick):** `EOSRoundPhase : uint8 { Lobby, RoundIntro,
  RoundActive, RoundBreak, MatchEnd }`. Transitions are event-driven:
  timers (`FTimerManager`) for the 60 s round clock and 5 s break countdown,
  gameplay events for knockouts. No polling anywhere.
- **Knockout detection through GAS, not around it:** the Health attribute
  hitting 0 is detected in the AttributeSet's `PostGameplayEffectExecute`,
  which raises `GameplayEvent.Duel.Knockout` to the dying actor and
  broadcasts `FOS_RoundEndedMessage` on the Gameplay Message Subsystem.
  `AOSDuelGameMode` listens and advances the phase. **No system
  hard-references another** — round flow knows nothing about abilities.
- **Timer expiry:** GameMode compares Health attributes via each
  PlayerState's ASC (`GetNumericAttribute`), awards the round to the leader.
- **Replication:** `EOSRoundPhase CurrentPhase`, `int8 RoundWins[2]`, and
  `float PhaseEndServerTime` replicate on GameState with RepNotify. Clients
  render countdowns from `PhaseEndServerTime` against the synchronized
  server clock (`GetServerWorldTimeSeconds`) — **no per-tick countdown
  replication**.
- **Cancel/interrupt:** phase transitions force-cancel all active abilities
  via `ASC->CancelAbilities()` on both fighters at `RoundBreak` entry —
  existing cancel guarantees (audio/VFX/montage cleanup) do the rest.
- **Simulated proxy:** proxies see phase changes via RepNotify → Gameplay
  Message → UI. A mid-rollback knockout resolves server-side only; clients
  never locally decide a round ended.

## 2. Classes, Loadouts, and the Magic-Slot Swap

- **Owner:** `UOSLoadoutSubsystem` (GameInstanceSubsystem, data) +
  server-side grant in `AOSDuelGameMode::HandleStartingNewPlayer`.
- **Data:** `DT_ClassLoadouts` DataTable — per class: ability set (granted
  ability classes + levels), attribute init GE, magic-slot options. Pure
  data; Blueprints only as asset containers per house rule 1.
- **Grant model:** ability sets granted to the PlayerState ASC once at
  match join (ASC persists across pawn respawn per house rule 5). The magic
  slot is **one granted ability handle** swapped by class:
  `ASC->ClearAbility(Handle)` + `GiveAbility(NewSpec)`.
- **Swap timing = the safe point:** swaps are only legal during
  `RoundBreak` (server validates phase). No prediction interaction: no
  ability is active, no montage playing, no keys in flight. A swap request
  outside `RoundBreak` is dropped server-side (never trust the client).
- **Replication:** granted handles replicate through existing GAS spec
  replication; the HUD reads the swap via `AbilitySpecDirtied` delegates.

## 3. Bots (deterministic, GAS-native)

- **Owner:** `AOSBotController : AAIController` + `UOSBotBrainComponent`.
  Tuning rows in `DT_BotTuning` (agent-produced, human-reviewed).
- **No Tick, no BT ticking:** the brain is an event-driven C++ state
  machine (`EOSBotStance { Neutral, Pressure, Punish, Retreat }`). It acts
  on: gameplay messages (opponent ability started/ended — the "reaction"
  trigger), quantized decision timers (`ReactionMs` from tuning, jittered
  once at round start from the match seed — deterministic thereafter), and
  its own ability-ended callbacks.
- **GAS purity:** the bot presses "virtual inputs" — it activates abilities
  **through the same input-buffer path players use** (`ASC` on its
  PlayerState, same ability sets from `DT_ClassLoadouts`). No side-channel
  damage, no cheating attributes; a bot is a player the AI happens to
  drive. This also means Combat QA's bot-vs-bot matches exercise the real
  combat path.
- **Determinism guarantee (GDD §4 promise):** within a round, bot behavior
  is a pure function of (tuning row, match seed, observed events). The
  Ringside Agent may only swap the tuning row pointer **between** rounds.
- **Replication/proxy:** none needed beyond normal pawn/GAS replication —
  bots run server-side only; clients see a replicated fighter like any
  other.

## 4. Telemetry & the Ringside Agent

- **Telemetry owner:** `UOSTelemetrySubsystem` (WorldSubsystem,
  server-only). Listens to Gameplay Messages already emitted by combat
  (damage dealt, blocks, parries, dashes, whiffs) and folds them into a
  plain `FOS_RoundTelemetry` struct per fighter per round. Zero new hooks
  inside abilities — it only consumes existing messages (house rule 6).
- **Ringside owner:** `UOSRingsideSubsystem` (GameInstanceSubsystem,
  **authority-only**; early-outs on clients). On `RoundBreak` entry it
  fires one async `FHttpModule` POST (Claude API, Haiku) with the telemetry
  summary; 3 s timeout.
- **Fallback is the contract:** `DT_RingsideCannedLines` ships in the
  build. The 5 s break countdown starts immediately and never waits;
  whichever of {model reply, timeout, error, cap} resolves first decides
  the content. Cap: 10 calls/match, counted server-side.
- **Replication:** result lands in `FOS_RingsideLine { FText Announcer;
  FText CoachTipFor[2]; }` on `AOSDuelGameState` with RepNotify → Gameplay
  Message → UI. Strings only; clients never talk to the API and the API
  key never leaves the host.
- **Bot adjustment path:** `bot_adjust` from the reply is applied as a
  tuning-row swap on `UOSBotBrainComponent` **only during RoundBreak**
  (server-side phase check), preserving §3's determinism guarantee.
- **Cancel:** an in-flight HTTP request is abandoned (lambda holds weak
  refs) if the match ends first — no dangling callbacks into dead worlds.

## 5. Front End & HUD (CommonUI, GAS-driven, event-driven only)

- **Owner:** `UOSUIManagerSubsystem : ULocalPlayerSubsystem` (per-player UI
  state, per house rule 6) managing a Lyra-style activatable layer stack:
  `GameHUD (ECommonInputMode::Game) → Menu (Menu) → Modal (Menu)`. All new
  UI is CommonUI-first (`UCommonActivatableWidget`); the GameHUD layer is
  never deactivated — it is the fallback input config.
- **Widgets owned by PlayerController**, added with `AddToPlayerScreen`;
  all references `UPROPERTY() TObjectPtr`; every delegate bound in
  `NativeConstruct` is unbound in `NativeDestruct`.
- **Zero polling, zero property bindings (anti-pattern ban):**
  - **Health bars:** `ASC->GetGameplayAttributeValueChangeDelegate(Health)`.
  - **Magic-slot cooldown:** cooldown-tag events via
    `ASC->RegisterGameplayTagEvent(Cooldown.Magic, NewOrRemoved)` for the
    sweep start/end; duration read once from the cooldown GE spec — no
    per-frame remaining-time queries.
  - **Round pips / phase / countdown:** GameState RepNotify → Gameplay
    Message → widget handler; countdown rendered locally from
    `PhaseEndServerTime` (one timer, not replication).
  - **Ringside lines:** the same message raised by `FOS_RingsideLine`'s
    RepNotify.
- **Screens at ship (6):** MainMenu, HostLobby, JoinLobby, ClassSelect,
  GameHUD, RoundBreak/MatchEnd overlay — each an activatable widget on the
  stack; Escape/B handled by CommonUI back-action, no bespoke input code.

## 6. Strike Alignment (existing systems, one integration note)

Melee hit confirmation stays server-side: montage notify windows raise
gameplay events that execute the damage GE — clients predict activation,
montage, and audio (rollback-safe), never damage. Motion warping targets
`SoftLockTarget` (already the replicated source of truth), so warp
divergence between server and proxy stays bounded by an already-replicated
value — no new prediction surface.

---

## 7. Build Order (maps to GDD week plan)

| Week | Systems from this spec |
|---|---|
| W1 | §1 round flow, §2 loadouts (1 class), §3 bot stance machine (tier 1) |
| W2 | §2 class 2 + swap flow, online path (existing sessions), §3 tiers 1–2 |
| W3 | §4 telemetry + Ringside (fallback-first: canned lines before API), §3 tier 3 |
| W4 | §5 CommonUI stack + HUD bindings, balance pass via QA telemetry |
| W5 | Ship: Steam depot, soak tests (bot-vs-bot overnight), capstone demo |

**Definition of done, every system:** states its cancel path, passes a
150 ms simulated-latency PIE session (Network Emulation profile), and
leaves zero state after `RoundBreak` force-cancel — checked by the Combat
QA agent's nightly bot-vs-bot run.
