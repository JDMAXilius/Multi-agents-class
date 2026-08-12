# BREACHPOINT NEXT — the reference research

**Written:** 12 August 2026 · **Question:** what does a multiplayer-native, Halo-style arena FPS
on UE 5.8 actually need, as evidenced by how comparable projects are built — GAS, CommonUI,
EOS + Steam, GameLift?
**Answers:** `BREACHPOINT-NEXT-STRUCTURE.md` v3 — every unit there now traces to a finding here
or to a project law, **not** to the old `Source/Breachpoint/` tree.

---

## 1. The corpus

Six projects read on disk (`references projects/`), plus Epic and AWS documentation for the
services layer.

| Project | Files | What it is | The one lesson |
|---|---|---|---|
| **Lyra** | 482 | Epic's own multiplayer FPS sample — GAS, CommonUI, EOS, teams, bots | The canonical answer sheet — *and* a warning about generality: ~40% of it exists to make game modes pluggable, which Breachpoint doesn't need |
| **OnSight** | 395 | shipped-scale GAS shooter | `Managers/` + `Subsystems/` folders = grouping by base class; things with one home each land in two |
| **ZoransResistance** | 250 | GAS shooter | **Five** damage entry points, each building specs; a copy-pasted `FMath::Rand()` bug lives in two of them. This is what "one damage door" prevents |
| **NewMoons** | 87 | small GAS multiplayer game | A *single-file* sessions subsystem over the OSS interface is enough — the abstraction does the work |
| **ShooterCore** | 42 | course-scale FPS | Tick-driven weapon state: five bools + two modulo counters kept consistent by 180 lines of switch |
| **UE5_Multiplayer_FPS** | 38 | minimal net FPS | The floor: what a shooter is with nothing else |

The spread matters: Breachpoint NEXT at ~121 files sits deliberately between NewMoons (87) and
ZoransResistance (250) — and far under Lyra (482), because Lyra is a *platform for games*
and Breachpoint is *a game*.

---

## 2. Lyra findings — adopt / reject, each with the reason

### Adopted (v2 already had these; now they carry evidence, not opinion)

| Pattern | Lyra's version | Ours |
|---|---|---|
| ASC on PlayerState; sets granted/revoked as a bundle | `LyraAbilitySet` | `BRAbilitySet` |
| Input → InputTag → ability activation, config as DataAsset | `LyraInputConfig` + `LyraInputComponent` (14 files) | `BRInputConfig` + `BRInputComponent` (3) |
| **Firing lives in the ability**, weapon holds state only | `LyraGameplayAbility_RangedWeapon` computes spread/trace; `LyraWeaponInstance` holds heat/spread *state* | `BRGA_Fire` + `BRWeapon` — the split survives contact with the best reference |
| One damage execution behind one door | `LyraDamageExecution`, specs built in one place | `BRDamage` + `BRDamageExecution` (and Zorans shows the failure mode) |
| Screen base owns input mode/back handling | `LyraActivatableWidget` | `BRActivatableWidget` |
| One button base under CommonUI | `LyraButtonBase` | `BRButton` |
| UI pushed by exactly one subsystem | `LyraUIManagerSubsystem` | `BRUISubsystem` |
| Team identity via an interface, not a class check | `ILyraTeamAgentInterface` | `IBRTeamAgent` in `Interfaces/` |
| Cheat manager as the testing lever | `LyraTeamCheats` etc. | `BRCheatManager` |

### Rejected — with what each rejection saves

| Lyra system | Files | Why Breachpoint doesn't build it |
|---|---|---|
| **Experience system** (`ExperienceDefinition`, `ExperienceManager`, action sets…) | ~20 | Exists so one build can host arbitrary pluggable game modes. Breachpoint has one mode family; `DT_MatchRules` rows carry the variation. |
| **Inventory fragments** (`InventoryItemDefinition` + 5 fragment types + instances) | ~16 | Generalized item system for a game where "item" means weapon-or-grenade. Halo model: fixed loadout + map pickups. `BREquipmentComponent` + weapon rows. |
| **PawnData / HeroComponent / PawnExtensionComponent** | ~6 | Modular pawn assembly exists to serve GameFeature plugins. One pawn class initializes itself. ⚠ The *problem* it solves is real — on clients the PlayerState ASC can arrive after the pawn — but that is an init-ordering discipline in `BRCharacter`, not three classes. |
| **Teams/ as a subsystem + info actors + display assets** | ~22 | Two fixed teams. `ETeam` on PlayerState + the attitude solver in `BRCore` + `IBRTeamAgent`. |
| **Camera mode stack** | ~12 | Blending camera *modes* serves third-person + vehicles + emotes. A first-person arena shooter has one camera + recoil offsets: `BRCameraComponent`. |
| **Settings screens machinery** | ~33 | The single biggest post-slice UI cost in the corpus. Deferred entirely — no stub folders. |
| **ReplicationGraph** | ~4 | Pays off at high actor×player counts. 8 players in one arena is the case the default replication handles fine. |
| **Cosmetics, Hotfix, Replays, Performance** | ~25 | Phase-2-or-never at slice scope. |

### The one genuine gap v2 had — adopted into v3

**`Messages/` — Lyra's `FLyraVerbMessage` + `GameplayMessageSubsystem` + `VerbMessageReplication`.**
The question v2 had no answer for: how does *"X killed Y with Z"* travel from the server
GameMode to every client's killfeed ViewModel **without** the UI including `Match/` headers or
polling GameState? Lyra's answer: a small verb-message struct, broadcast locally through the
engine's **GameplayMessageRouter** plugin (a plugin enable, not code we write), replicated
to clients by one tiny component. Decoupled, testable, no Tick.

→ **v3 adds one unit:** `Match/BRMatchMessages.h/.cpp` — the verb-message struct(s) + the
replicated broadcaster. ViewModels subscribe by tag; neither side includes the other.

### Named triggers (adopt *when the trigger fires*, recorded so no one re-litigates)

- **Split the AttributeSet** when meta-attributes need to live on the damage *source*
  (Lyra's `CombatSet` carries `BaseDamage` on the attacker). Until then: one set.
- **Custom TargetData** when server-confirmed hitmarkers need extra payload (Lyra's
  `TargetData_SingleTargetHit` adds timing/confirm data). Until then: the engine struct.
- **Custom `UAssetManager`** when primary-asset scanning outgrows `UDeveloperSettings`
  soft refs. Until then: `BRAssetSettings` + `BRGameData`.

---

## 3. The services layer — EOS · Steam · GameLift

The finding that shaped `Online/`: **coexistence is configuration, not code.**

1. **EOS + Steam together** is the engine's `OnlineSubsystemEOSPlus` layer: EOS provides
   crossplay sessions/presence, the native Steam subsystem stays underneath for platform
   identity/invites/achievements. It is enabled and wired in `DefaultEngine.ini` — there are
   **zero per-service C++ classes** for the game to write. The game's C++ talks only to the
   abstract session/identity interfaces.
2. **Lyra's proof:** its whole online surface is `CommonUserSubsystem` + `CommonSessionSubsystem`
   — thin subsystems over the OSS abstraction, swappable between Null/Steam/EOS by config.
   NewMoons ships the same idea in one file. → `BRSessionSubsystem` stays **one unit**, on purpose.
3. **GameLift** is server-target-only code behind `WITH_GAMELIFT`: `InitSDK()` →
   `ProcessReady()` → callbacks (game session activation, health check, termination), plus
   `AcceptPlayerSession` at the admission boundary. The repo's own `BREACHPOINT-GAMELIFT-PLAN.md`
   (rulings R15–R16) already phases this behind a telemetry trigger with Steam-ticket identity —
   the research confirms its shape. → Slice builds **`IBRServerLifecycle` + the listen-server
   impl only**, but the interface is *GameLift-shaped from day one*: session-activated,
   player-admission (`ValidateJoin`), healthy?, terminate. `BRGameLiftLifecycle` arrives in
   Phase 2 as implementation #2 — a new file, zero edits to callers.

---

## 4. Verdict

The v2 → v3 delta after reading ~1,300 reference files and the services docs:

- **+1 unit** (`Match/BRMatchMessages`) — the messaging gap was real.
- **0 units removed** — every v2 collapse survived: the references either validate it (Lyra
  fires from the ability) or demonstrate its failure mode (Zorans' five damage doors,
  ShooterCore's tick-state, OnSight's `Managers/`).
- **3 named triggers** recorded so future growth is a decision with a citation, not a drift.
- `Online/` stays thin **because the research says so**, not despite it.

**Totals: 63 units · 121 files.** Lyra needs 482 because it is a platform. Breachpoint is a game.

### Sources

- [Common User Plugin (Lyra) — Epic docs](https://dev.epicgames.com/documentation/unreal-engine/common-user-plugin-in-unreal-engine-for-lyra-sample-game?lang=en-US)
- [Using Lyra with Epic Online Services — Epic docs](https://docs.unrealengine.com/5.0/en-US/using-lyra-with-epic-online-services-in-unreal-engine/)
- [Online Subsystem EOS Plugin — Epic docs](https://dev.epicgames.com/documentation/unreal-engine/online-subsystem-eos-plugin-in-unreal-engine)
- [Lyra plugin-structure breakdown — jaydengames.com](https://www.jaydengames.com/posts/ue5-black-magic-plugins-strcture/)
- [GameLift server SDK for Unreal — actions (AWS docs)](https://docs.aws.amazon.com/gamelift/latest/developerguide/integration-server-sdk5-unreal-actions.html)
- [Integrate GameLift Servers into a UE project (AWS docs)](https://docs.aws.amazon.com/gameliftservers/latest/developerguide/integration-engines-setup-unreal.html)
- Local corpus: `references projects/{Lyra, OnSight, ZoransResistance, NewMoons, ShooterCore, UE5_Multiplayer_FPS-main}`
- In-repo: `BREACHPOINT-GAMELIFT-PLAN.md` (R15–R16), `docs/BREACHPOINT-GAMEPLAY-REWORK.md` §0 reference audit
