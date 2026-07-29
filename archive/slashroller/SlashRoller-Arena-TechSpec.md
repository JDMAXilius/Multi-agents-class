> ⚠️ **ARCHIVED — the capstone build pivoted to BREACHPOINT** (see
> `../../docs/decisions/SCOPE-COMPARISON.md` and `../../breachpoint/`).
> *Slash Roller: Arena* remains the game of record for the course
> assignments (`../../assignments/`), and this spec remains the reference
> for the studio's OnSight-era GAS patterns — but nothing here is current
> build scope.

# Slash Roller: Arena — Technical Specification
## GAS-First Engineering Spec for the Capstone Cut

**Companion to:** `assignments/02-gdd-final-draft/GDD.md`
**Engine:** UE 5.8, pure native C++
**Standards:** inherits all Non-Negotiable Engineering Rules from the master
context doc (no-Tick, server-authoritative, GAS purity, PlayerState-owned
ASC, SOLID subsystems, no dangling state on cancel, abstraction at
migration seams). One deviation from the older context doc, by direction:
**audio is MetaSounds, not Wwise** (see §6). Every system below is declared
the way the house rules require: **owner → replication → cancel/interrupt →
simulated proxy**, before implementation.

---

## 0. What Already Exists (do not rebuild)

| Shipped system | Reused as-is |
|---|---|
| GAS core (PlayerState ASC, input buffer, prediction keys) | All combat |
| Soft-lock melee (`SoftLockTarget` replicated source of truth) | All strikes |
| Target assist (score-based) + motion warping | Strike alignment, target switching |
| `OSSessionsSubsystem` (Steam OSS, host/find/join) | Online FFA/TDM |
| AL Framework + custom AnimGraph nodes, Motion Matching | Locomotion/combat anim |

New engineering surface is **six systems**, specced below.

---

## 1. Match Flow, Kill Scoring, and Respawn

- **Owner:** `AOSArenaGameMode` (server-only logic) + `AOSArenaGameState`
  (replicated match state). Kill/death counts live on `AOSPlayerState`
  (replicated per-fighter), team totals derived on GameState.
- **Phase machine (no Tick):** `EOSMatchPhase : uint8 { Lobby, MatchActive,
  SuddenDeath, MatchEnd }`. Transitions are event-driven: one
  `FTimerManager` timer for the 8-minute clock, gameplay events for kills.
- **Kill detection through GAS, not around it:** Health reaching 0 is
  detected in the AttributeSet's `PostGameplayEffectExecute`, which raises
  `GameplayEvent.Arena.Death` on the victim and broadcasts
  `FOS_KillMessage {killer, victim, method_tags}` on the Gameplay Message
  Subsystem. GameMode listens: increments `AOSPlayerState::Kills/Deaths`,
  starts the victim's respawn timer. **Attribution:** last-instigator
  tracking via the damage GE's context; environment deaths with no
  instigator inside 5 s count as −1 self.
- **Tiebreak:** at timer expiry GameMode compares kills → fewest deaths →
  enters `SuddenDeath` (no respawns, first kill ends it).
- **Respawn:** 5-second timer per dead fighter; spawn selection is a
  server-side score over `arena_manifest.json` spawn points (distance from
  living fighters, time since last use). Pawn is destroyed and respawned;
  **the ASC lives on PlayerState (house rule 5), so attributes, granted
  abilities, and cooldowns-in-flight survive death by construction.**
  Stamina and Health are reset to max by an explicit respawn GE — reset is
  a gameplay decision, not a side effect.
- **Replication:** `EOSMatchPhase`, `float MatchEndServerTime`, and
  per-PlayerState `Kills/Deaths` (RepNotify → Gameplay Message → UI).
  Clients render the countdown locally from `MatchEndServerTime` against
  `GetServerWorldTimeSeconds` — no per-tick replication.
- **Cancel/interrupt:** `MatchEnd` entry force-cancels all live abilities
  (`ASC->CancelAbilities()`); existing cancel guarantees clean montages,
  cues, and audio. Death itself cancels the victim's active abilities
  through the same path before ragdoll/death montage.
- **Simulated proxy:** proxies learn of kills and phase changes only via
  replication; a mid-rollback death resolves server-side — clients never
  locally decide a kill happened.

## 2. Loadouts and Swap-While-Dead

- **Owner:** `UOSLoadoutSubsystem` (GameInstanceSubsystem, data) +
  server-side grant in `AOSArenaGameMode`.
- **Data:** `DT_ClassLoadouts` — per archetype (blade, spellblade):
  granted ability set, attribute-init GE, magic-slot options. Pure data;
  Blueprints only as asset containers.
- **Grant model:** ability sets granted once to the PlayerState ASC at
  match join. The magic slot is one granted handle:
  `ClearAbility(Handle)` + `GiveAbility(NewSpec)`.
- **Swap timing = the safe point:** swaps are legal **only while dead**
  (server validates `IsDead` on the PlayerState). No live abilities, no
  montage, no prediction keys in flight. Requests from living fighters are
  dropped server-side — never trust the client.

## 3. Stamina and the Winded State

- **Owner:** new `Stamina/MaxStamina` attributes in the existing attribute
  set; regen and winded logic in GAS, no manager class.
- **Costs:** every attack/dodge/block-hold ability carries a stamina cost
  GE (`ExecutionCalculation` for scaling costs). Costs are checked in
  `CanActivateAbility` — an unaffordable ability never activates, which
  keeps prediction clean (no activate-then-refund).
- **Regen without Tick:** a single infinite periodic GE (period 0.1 s)
  applies regen, gated by a `State.Combat.Recovering` tag applied for 1 s
  after any stamina spend (`RemoveGameplayEffectsWithTags` + re-apply
  pattern). Periodic GEs are engine-driven — this is not a Tick violation.
- **Winded:** Stamina reaching 0 (detected in
  `PostGameplayEffectExecute`) applies `GE_Winded`: grants
  `State.Winded` tag which blocks activation of all attack/dodge abilities
  (activation-blocked tags), removed when Stamina ≥ 30% (attribute-change
  delegate on the GE's own listener). Winded is fully GAS-visible: UI,
  bots, and the parry-punish window all read the same tag.
- **Prediction/rollback:** stamina costs ride the ability's prediction
  key; a rolled-back ability rolls back its cost with it (standard GAS
  cost semantics — no custom refund code).
- **Simulated proxy:** proxies see `State.Winded` via replicated tags for
  the stagger/gasp presentation; stamina values replicate to owner only
  (`COND_OwnerOnly`) — opponents read the *tag*, not the number.

## 4. Bots (deterministic, GAS-native, fill any slot)

- **Owner:** `AOSBotController : AAIController` + `UOSBotBrainComponent`;
  tuning in `DT_BotTuning` (agent-produced, human-reviewed).
- **No Tick, no ticking BT:** event-driven C++ state machine
  (`EOSBotStance { Hunt, Engage, Punish, Disengage }`) acting on gameplay
  messages (nearby ability started/ended, kill events, own `State.Winded`),
  quantized decision timers (`ReactionMs` jittered once from the match
  seed — deterministic thereafter), and ability-ended callbacks. Stamina
  discipline is a tuning parameter: low tiers mash into winded; high tiers
  hold reserves for the parry punish.
- **GAS purity:** bots activate abilities through the same input-buffer
  path players use, on their own PlayerState ASC with the same
  `DT_ClassLoadouts` kits. A bot is a player the AI happens to drive —
  which is exactly why Combat QA's bot-vs-bot soaks exercise the real
  combat path, including stamina and winded.
- **Slot filling:** `AOSArenaGameMode` tops up to the configured fighter
  count at match start and backfills mid-match leavers after a 10 s grace.
- **Determinism guarantee:** within a match, bot behavior is a pure
  function of (tuning row, match seed, observed events). The Caster Agent
  has **no** path into bot tuning mid-match (unlike the earlier duels
  design — deathmatch has no safe between-rounds point, so the hook is
  deleted rather than made unsafe).

## 5. Telemetry and the Caster Agent

- **Telemetry owner:** `UOSTelemetrySubsystem` (WorldSubsystem,
  server-only) folding existing gameplay messages (kills, parries, winded
  events, damage) into `FOS_MatchTelemetry` per fighter. No new hooks
  inside abilities — it only consumes messages.
- **Caster owner:** `UOSCasterSubsystem` (GameInstanceSubsystem,
  **authority-only**, early-outs on clients). Fire-and-forget
  `FHttpModule` POSTs (Claude API, Haiku): notable kill events (streaks ≥
  3, parry kills, sudden-death winners), batched ≥ 10 s apart, ≤ 12
  calls/match; plus one coach call per human player at `MatchEnd`.
- **Never load-bearing:** the factual kill-feed line ("A killed B")
  renders locally and instantly from `FOS_KillMessage`; a Caster line, if
  and when it arrives, *appends* color. Timeout (> 3 s), error, or cap →
  `DT_CasterCannedLines`. The simulation and UI never wait.
- **Replication:** `FOS_CasterLine` entries push through a small
  replicated ring buffer on GameState (RepNotify → Gameplay Message →
  kill-feed widget). Strings only; the API key never leaves the host.
- **Cancel:** in-flight HTTP callbacks hold weak references; match teardown
  abandons them — no dangling callbacks into dead worlds.

## 6. Audio — MetaSounds (replaces the Wwise plan)

- **Owner:** engine-native audio; combat cues fire through **GameplayCues**
  (the GAS-correct trigger path), each cue notify playing a MetaSound
  Source. No middleware, no bank pipeline — one less external dependency
  for a 5-week ship.
- **Rollback safety re-established without Wwise:** the house guarantee
  ("no audio survives a cancelled ability") is preserved by rule:
  **every looping or stateful combat sound must be a GameplayCue
  `WhileActive`/`Removed` pair** — cue removal on ability cancel/rollback
  stops the MetaSound. One-shots (hits, parry clang, winded gasp) fire
  only from `Executed` cues on *server-confirmed* events, so a predicted
  whiff that rolls back never spawned them. Predicted activation sounds
  (swing woosh) are `OnActive` cues, which GAS removes on rollback.
- **MetaSound assets** (swing, hit, parry, winded, kill sting, countdown)
  are data assets — buildable by the crew, reviewed like any other asset.
- **Simulated proxy:** proxies receive cues via existing GAS cue
  replication; no bespoke audio replication.

## 7. Front End & HUD (CommonUI, event-driven only)

- **Owner:** `UOSUIManagerSubsystem : ULocalPlayerSubsystem` managing the
  Lyra-style activatable layer stack: `GameHUD (ECommonInputMode::Game) →
  Menu (Menu) → Modal (Menu)`. GameHUD layer is never deactivated.
  All widgets owned by PlayerController, added with `AddToPlayerScreen`,
  refs in `UPROPERTY() TObjectPtr`, delegates unbound in `NativeDestruct`.
- **Zero polling, zero property bindings:**
  - **Health/Stamina bars:** `GetGameplayAttributeValueChangeDelegate`
    (stamina is owner-only; the enemy widget shows the `State.Winded` tag
    instead).
  - **Magic cooldown sweep:** `RegisterGameplayTagEvent(Cooldown.Magic)`
    start/end + duration read once from the cooldown GE spec.
  - **Timer/phase/scoreboard:** GameState & PlayerState RepNotify →
    Gameplay Message → widget handlers; countdown rendered locally from
    `MatchEndServerTime`.
  - **Kill feed:** `FOS_KillMessage` (instant, factual) + `FOS_CasterLine`
    (async color); feed rows come from a widget pool (bounded, recycled).
- **Screens at ship (7):** MainMenu, HostLobby, JoinLobby, LoadoutSelect,
  GameHUD, DeathOverlay (respawn timer + swap-while-dead), MatchEnd
  scoreboard (K/D, coach line, rematch). Escape/B via CommonUI back-action.

## 8. Strike Alignment (existing systems, one integration note)

Melee hit confirmation stays server-side: montage notify windows raise
gameplay events that execute the damage GE — clients predict activation,
montage, and cue presentation, never damage. Motion warping targets
`SoftLockTarget` (already replicated), so warp divergence between server
and proxy stays bounded by an already-replicated value. Parry is a timed
block ability whose success window is validated server-side against the
attacker's montage position — no client-declared parries.

---

## 9. Build Order (maps to GDD week plan)

| Week | Systems from this spec |
|---|---|
| W1 | §1 match/kill/respawn, §3 stamina + winded, §4 bot stance machine (tier 1), arena blockout via crew |
| W2 | §2 archetype 2 + swap-while-dead, online path (existing sessions), TDM team scoring, §4 tiers 1–2 |
| W3 | §5 telemetry + Caster (canned lines before API), §4 tier 3, §6 MetaSounds combat cues |
| W4 | §7 CommonUI stack + HUD bindings, Balance Analyst pass on nightly QA soaks, spawn tuning |
| W5 | Ship: Steam depot, overnight bot-vs-bot soaks, capstone demo. Buffer. |

**Definition of done, every system:** states its cancel path, passes a
150 ms simulated-latency PIE session (Network Emulation profile), and
leaves zero state after death-cancel and `MatchEnd` force-cancel — checked
by the Combat QA agent's nightly runs.
