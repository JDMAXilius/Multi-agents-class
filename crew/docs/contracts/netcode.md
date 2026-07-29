# Contract — Netcode (server authority is law)

Status: v1 template · Owner: netcode-builder · Every replicated surface is bound by this file.
The trust model in one line: **clients send intent, the server simulates truth, clients render
results.** There is no second line of defense.

## Laws

1. **Authority gate on every mutation.** Any function that changes gameplay state begins with
   an authority check (`HasAuthority()` / `ROLE_Authority`). No exceptions for "it's only
   called on the server anyway" — the check IS the documentation and the guard.
2. **Server RPCs validate or don't ship.** `UFUNCTION(Server, Reliable, WithValidation)` with
   a real `_Validate`: range checks, rate limits, possession/ownership checks. An empty
   `return true;` fails review. Unreliable for high-frequency intent (movement-adjacent),
   Reliable for discrete actions — never Reliable spam.
3. **Replicate results, not authority.** Clients never own gameplay-authoritative properties.
   `OnRep_` and NetMulticast are for COSMETIC reaction only (VFX, sounds, UI refresh) — if
   removing every OnRep body changed a gameplay outcome, the design is wrong.
4. **Minimum replication.** Tightest `DOREPLIFETIME_CONDITION` that works; `COND_OwnerOnly`
   for private state (loadout internals, cooldown timers others don't need); dormancy for
   rarely-changing actors; relevancy/NetCullDistance tuned per class. Track the per-class
   bandwidth budget in this file as classes are added.
5. **Hidden state stays hidden.** Never replicate to a client what its player must not know
   (enemy positions through walls, other players' hands/inventory, upcoming spawns) — wall-hack
   prevention happens at replication, not at rendering.
6. **Prediction reconciles.** Client prediction (movement, ability activation feels) is UX
   over server truth; every predicted path has a correction path, and a mispredict may never
   fork gameplay state.
7. **Join/travel honesty.** Every replicated consumer handles late-arriving state: PlayerState
   may be null on first frame, arrays arrive incrementally, seamless travel re-creates actors.
   "Worked from map start" is not a claim about join-in-progress.
8. **The attack ships with the feature.** Each new replicated surface adds its cheat-attempt
   test (forged RPC, out-of-range value, spam) whose REJECTION is the acceptance criterion.
   The critic re-attacks independently at V2.

## Per-project fill-ins — BREACHPOINT (refilled 2026-07-29; supersedes the Slash Roller fill)

- Replication system: **[x] classic property replication** (Iris not enabled; revisit at UE6).
- Net topology: **[x] listen servers allowed** (slice ships listen; dedicated behind
  `IBRServerLifecycle` is the Phase-2 swap) — host-advantage review is a STANDING item on
  every netcode packet: every claim is tested separately for host and remote client.
- Movement/ability stack: **[x] GAS (prediction keys) on the PlayerState-owned ASC
  (`ReplicationMode::Mixed`, `ServerAbilityRPCBatch` on fire) + CMC for movement — the
  Grappleshot is a root-motion source THROUGH the CMC** so it predicts/reconciles via saved
  moves. Reconciliation: `FScopedPredictionWindow` for TargetData; predicted GEs roll back on
  rejection; cosmetic prediction via cues (`OnActive` removed on rollback). No custom
  prediction paths outside GAS/CMC.
- Hitscan trust model: **client-traced TargetData, server-validated** (rate ≤ RPM+tolerance,
  ammo > 0, cone-from-server-muzzle, range ≤ table max) — Lyra parity; server rewind is a
  named Phase-2 packet. Radar/wallhack class: hidden information is culled at replication
  (relevancy/conditions), never at render.
- Tick + bandwidth budgets: server tick target **30 Hz** · per-connection budget **20 KB/s**
  (8 fighters + projectiles; ammo is `COND_OwnerOnly`, killfeed is a ring buffer, clock is
  one replicated float). Track per-class numbers here as classes land.
- Session/matchmaking boundary: **Steam OSS is trusted for identity and session membership
  only** (`BRSessionsSubsystem`). ALL gameplay state is validated in-game; a session token
  never grants gameplay authority; the Spotter API key lives host-side only and its output
  enters the game exclusively as replicated strings.
