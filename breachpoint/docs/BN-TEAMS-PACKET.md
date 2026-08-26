# BN TEAMS — the native team layer (design packet, 26 Aug 2026)

> Two deep-research passes feed this: a reverse-engineering of the founder's on-site
> project (`references projects/OnSight/OnSight/` — 398 files, GAS-on-PlayerState, a
> close architectural cousin) and a full BN integration audit. OnSight's claim is
> VERIFIED: it runs Unreal's native team machinery — `FGenericTeamId` +
> `IGenericTeamAgentInterface` — with AIModule as a dependency solely for that header.
> This packet is the merged design. STATUS: awaiting founder go; nothing built.

## The design in one paragraph

A team is ONE replicated byte: `FGenericTeamId TeamId` on `ABNPlayerState`
(`ReplicatedUsing`, default `NoTeam`=255, authority-gated setter that fires the OnRep
manually, `OnTeamChanged` native delegate, carried through `CopyProperties` because
BN's between-rounds path really does seamless-travel). The pawn and the BN bot
controller answer `IGenericTeamAgentInterface` by DELEGATING to the PlayerState. Every
"friend or foe?" in game code goes through ONE static helper with the NoTeam guard —
OnSight's crown jewel — so FFA and team modes share every line of combat code: when
either side is NoTeam the answer is hostile-or-neutral, never friendly, which is what
keeps today's FFA byte-for-byte identical with `bTeamsEnabled=false`. Friendly fire is
refused at the top of `BNDamage::ApplyDamage` — the single damage door gas-purity
already mandates — with self-damage still allowed (kill credit already words that
case). Bots inherit teams through the doors that were built for exactly this day:
`UBNAIBWorldQuery::AreEnemies` gains the team compare (which makes the claims board's
alliance scope real, for free), `CountNearbyAllies` becomes a real count, and ONE
pre-sanctioned module edit makes the AIB controller's perception attitude consult the
registered world query (its own comment says "a team system... replaces this constant").

## What OnSight proved, adopted verbatim

1. **PlayerState is the single source of team truth; pawns delegate.** One byte
   replicates; respawn costs zero team code (the pawn only reads).
2. **The one static choke point** (`AreActorsFriendly`: null/self/no-interface ladder,
   then the NoTeam guard, then `GetAttitude`). OnSight routes ELEVEN systems through
   it. The guard exists because the engine's default solver answers
   `GetAttitude(NoTeam, NoTeam) == Friendly` — unguarded, FFA blocks all damage.
3. **Filter friendlies UPSTREAM of the GE, keep a downstream net.** OnSight's stated
   reason is a real GAS footgun: cues and dummy modifiers fire even when an ExecCalc
   early-returns — ally hit-flashes and hit-sounds survive a damage=0 filter. BN's
   shape is better still: one `ApplyDamage` door before any spec exists.
4. **Assignment in the one init seam, idempotent, lowest-population.** BN's
   `GenericPlayerInitialization` is even cleaner than OnSight's `InitNewPlayer`: the
   engine calls it for humans, the bot fill calls it for bots, and it runs before each
   entity's first spawn choice.
5. **Relative UI color (friendly/enemy), never absolute team colors**, bound to the
   team-changed delegate, with deferred subscription for the PlayerState-before-TeamId
   replication race. (UI is a follow-up ticket; BR-module prior art exists to
   transcribe.)

## What OnSight got wrong, fixed here

- **Sentinel chaos**: OnSight uses 255, -1, and INDEX_NONE for "no team" in different
  systems, and its own accessor's doc lies about which. BN exposes `FGenericTeamId`
  only; the ONLY sentinel is `FGenericTeamId::NoTeam`; no int accessor ships.
- **A raw-`GetAttitude` bypass existed** (a vestigial ExecCalc skipped the guard — a
  latent FFA-breaking bug). BN prevents the class: the helper is the only caller of
  `GetAttitude` (enforced by grep, the module-law way). The OnSight report suggests
  `FGenericTeamId::SetAttitudeSolver` instead; the BN audit marks that API
  NEEDS-HEADER-PROBE (nothing in any repo exercises it) — so per the transcription
  law: helper now, solver registration only if the terminal proves the header.
- **Two coexisting spawn systems** — BN builds exactly one (below).
- **`static constexpr` team count** — BN's is Config.

## The BN build list — 8 edits, 0 new classes, 1 sanctioned plugin edit

| File | Purpose (one line) |
|---|---|
| `Match/BNPlayerState.h/.cpp` | The byte: replicated `TeamId` + interface + authority setter + `OnTeamChanged` + `CopyProperties` (the existing Kills/ObjectivePoints idiom, third application). |
| `Core/BNGameplayTags.h` area (or `Match/BNTeams.h` if a header is warranted) | `BNTeams::GetAttitude / AreActorsFriendly` — the choke point with the NoTeam guard (~15 lines, static). |
| `Match/BNGameMode.h/.cpp` | Assign in `GenericPlayerInitialization` (idempotent, fewest-members); team-dedupe stanza in `HillTick` (same-team occupants do not contest each other); team win path beside `FinishMatch`; `ChoosePlayerStart` filtering `PlayerStartTag` Team0/Team1 with Super fallback; Config `bTeamsEnabled=false`, `bFriendlyFire=false`. |
| `Match/BNGameState.h/.cpp` | Two replicated team scores + `WinningTeamId` with OnReps — the honest record (client-side PlayerArray sums lose a leaver's points). |
| `AbilitySystem/Effects/BNDamage.cpp` | THE friendly-fire gate at the top of `ApplyDamage`: same non-NoTeam team and not self → refused. GAS purity untouched — the check runs before any spec exists. |
| `AIBotAdapter/BNAIBWorldQuery.cpp` | `AreEnemies` = alive AND different teams (NoTeam = enemy to all — FFA-safe); `CountNearbyAllies` = bounded same-team living count. The bots' ENTIRE team feed; the claims board turns real through it with zero further edits. |
| `AI/BNBotController.cpp` | BN bot's `GetGenericTeamId` reads the PlayerState; attitude compares ids, FFA fallback at NoTeam — BN bots stop hunting teammates. |
| `Plugins/AIBot/.../Core/AIBBotController.cpp` | THE one module edit, pre-sanctioned by the module's own comments (its FFA override names itself the fallback; `AIBWorldQuery.h`: "the game answers friend-or-foe"): attitude consults the registered `IAIBWorldQuery::AreEnemies`, keeps the FFA constant when no query exists. Engine+module types only; boundary grep stays clean. |

Plus: a committed `Tools/bn/` script tags the arena's 8 PlayerStarts (law 7 — never
hand-placed), and two ini lines.

## Security (netcode.md is law; the audit checked each rule)

- Assignment is SERVER-ONLY in a server-only class; **no new RPC of any kind** (BN
  currently ships zero Server RPCs — the team layer keeps it that way; nothing for a
  client to forge).
- `TeamId` OnRep bodies are cosmetic only — deleting them changes no gameplay outcome
  (rendering-only replication, law 3).
- Team membership is HUD-grade (the scoreboard shows it to everyone) → `COND_None` is
  lawful; nothing positional rides the team channel.
- New replicated property = netcode packet + critic REFUTER, and the cheat test is the
  degenerate case: assert a client-side TeamId write never replicates up.
- Late joiners honestly read NoTeam for a frame; every reader treats NoTeam as
  unknown-hostile, never friendly.

## Deliberately NOT built (recorded, not forgotten)

Team switching, spectator policy, leave-rebalance, absolute team colors, scored spawn
safety (BR prior art at `BRGameMode.cpp:769-889` when wanted), team UI (follow-up
ticket; BR prior art transcribes), a teams DataAsset/subsystem (a byte and a helper do
the whole job at this scale — Lyra's subsystem earns its keep at Lyra's scale, not 4v4).

## Proof plan (the ticket's spine)

Rung 1 all targets → `bTeamsEnabled=False` regression (FFA byte-identical: same kill
counts possible, zero attitude changes, damage spec unchanged) → `True`: (1) 4v4 lands
2v2v-balanced-by-population, log line per assignment; (2) friendly fire refused at the
door (log count), self-grenade still damages; (3) BN bots never target teammates
(acquisition lines never name a same-team pawn); (4) AIB bots: perception Note-boundary
refuses teammates, claims become binding between teammates (first `claim GRANTED` +
`DENIED` lines ever seen in a live match — AIB12's FFA-inert result must hold when
teams are OFF); (5) team hill: two same-team bodies on the hill score (not contested),
cross-team contests; (6) team win announced from team score. The AIBot roadmap's
DEFERRED row-7 measurement (two allied bots, one claimable slot, contested count 0)
becomes runnable for the first time.

## Open for founder (with recommendations)

1. **Scope now vs later**: build all 8 files in one packet (recommended — they are one
   feature; the FFA-off regression is the safety net) vs. identity+damage first, hill
   and spawns second.
2. **Friendly fire default**: recommended `false` (arena standard); the Config bool
   ships either way.
3. **Team count**: recommended hard 2 for the packet (Config-widened later); the
   balancing code is count-agnostic already.
4. **The AIBot module edit**: recommended to take (it is the module's own documented
   design; without it AIB bots hunt teammates through perception even with the world
   query fixed). Alternative — leave AIB FFA until a dedicated module pass — makes
   `BotSystem=AIB` unusable in team modes.
