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

## Per-project fill-ins — SLASH ROLLER (filled 2026-07-22)

- Replication system: **[x] classic property replication** (Iris not enabled; revisit at UE6).
- Net topology: **[x] listen servers allowed** — host-advantage review is a STANDING item on
  every netcode packet: the host shares a process with a client, so every claim is tested
  separately for host and remote client ("works for the host" is half a claim).
- Movement/ability stack: **[x] GAS (prediction keys) on the PlayerState-owned ASC + CMC for
  movement.** Reconciliation mechanism packets must use: `FPredictionKey` scoped windows;
  predicted GameplayEffects roll back on server rejection; cosmetic prediction via GameplayCues
  (`OnActive` removed on rollback). No custom prediction paths outside GAS/CMC.
- Tick + bandwidth budgets: server tick target **30 Hz** · per-connection budget **15 KB/s**
  (2–4 fighters + listen host; melee intent is small — budget forces `COND_OwnerOnly` stamina
  and dormant scoreboard actors). Track per-class numbers here as classes land.
- Session/matchmaking boundary: **Steam OSS is trusted for identity and session membership
  only** (`OSSessionsSubsystem`). ALL gameplay state is validated in-game; a session token
  never grants gameplay authority; the Caster Agent API key lives host-side only and its
  output enters the game exclusively as replicated strings.
