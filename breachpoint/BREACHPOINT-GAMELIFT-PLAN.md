# BREACHPOINT — Dedicated Servers Plan: AWS GameLift, Phase 2

**Decision in one line:** the slice ships on Steam listen server; GameLift arrives later
**behind `IBRServerLifecycle`** — and this plan makes the swap real by fixing the seam's
shape NOW (three interface gaps, zero AWS code) and phasing the cloud work behind a
telemetry trigger, not a date. Binding rulings: **R15–R16** in `docs/DESIGN-RULINGS.md`.

Researched against the full managed-GameLift stack (account/IAM → UE5 server SDK →
Anywhere fleets → managed fleets → Lambda + API Gateway → game/player sessions → identity
and access tokens → lobbies → admission → DynamoDB → leaderboards).

---

## 1. Why this is a small Phase 2 (the part most teams get wrong)

The expensive half of "dedicated servers" is making the game *run* as one — a headless
server target, no local player assumed, authority clean. **Breachpoint pays that cost in
the slice already**: `BreachpointServer` builds from week 1 (BP00 rung 1), the Gauntlet
smoke runs dedicated server + 2 clients (rung 4), and the netcode contract bans local-player
assumptions outside marked listen-only code. Phase 2 is therefore only **hosting, identity,
admission, and matchmaking** — the cloud around the game, not the game itself.

*Environment note this implies (already true, worth saying loudly): server targets need a
**source-built UE 5.8**, not the launcher install. That's a slice-time setup cost in BP00,
not a Phase-2 surprise.*

## 2. Course-stack → Breachpoint mapping

| Stack piece | Breachpoint home | Phase |
|---|---|---|
| AWS account / IAM | env/secret store; keys NEVER in repo or agent context (crew law) | GL-1 |
| GameLift Server SDK in UE5 (`InitSDK`→`ProcessReady`) | `BRGameLiftLifecycle` — implementation #2 of `IBRServerLifecycle` | GL-1 |
| **Anywhere fleets** (your PC as a fleet) | the crew's **verification rung** for D4 — full control plane, ~$0 | GL-1 |
| Managed EC2 fleets | real money; gated by the R16 trigger | GL-3 |
| Lambda + API Gateway | the meta-backend behind `FindAndJoinBestSession` | GL-2 |
| Game sessions / player sessions | placement + per-player admission tickets | GL-3 |
| Cognito + access tokens | **adapted — see §3**: Steam-ticket validation, not account signup | GL-2 |
| Lobbies + timers | `BRLobbySubsystem` (already contract-flagged Phase 2) | GL-5 |
| Accepting player sessions | the trust boundary: server-side admission (`ValidateJoin`) | GL-3 |
| DynamoDB | where `FBRMatchTelemetry` rows land | GL-5 |
| Leaderboards | read path over **server-written** results only | GL-5 |

## 3. Identity: the one deliberate deviation (ruling R15)

Generic GameLift tutorials build email/password signup (Cognito user pools). **A Steam game
must never ask players to create an account.** Breachpoint's flow:

```
client (Steam session ticket) ──► API Gateway ──► Lambda:
    validate ticket via Steam Web API (AuthenticateUserTicket)
    ──► issue OUR short-lived token (Lambda-signed JWT; Cognito only ever as
        token machinery in custom-auth mode — never as a login UI)
client presents token ──► /sessions/* endpoints ──► GameLift placement
server: AcceptPlayerSession(playerSessionId) admits or rejects (S14)
```

This is the existing services law — *platform trust is validated, not assumed; a Steam ID
proves identity, nothing more* — extended one hop into the cloud. Token handling
(expiry, refresh, replay rejection) applies verbatim; only the issuer differs.

## 4. The three seam gaps fixed in the slice (contract v1.1)

Cheap now, expensive later — all three are interface shape only, no AWS code:

1. **Per-player admission.** GameLift requires `AcceptPlayerSession` per join.
   `IBRServerLifecycle` gains `ValidateJoin(player, join_credential) → accept|reject`;
   listen implementation accepts always (one line). Retrofitting an admission gate after
   PlayerController flows harden is real pain.
2. **Outbound lifecycle events.** The seam needs server→game signals (`OnHostingEnding` —
   GameLift's `ProcessEnding`/scale-down, health-check pulse) not just game→server
   (`MatchComplete`). Listen impl: host-quit maps onto the SAME `OnHostingEnding` path —
   which the host-quit law needed anyway.
3. **Join target is a variant, not a handle.** Steam joins by session handle; GameLift by
   `ip:port` + player-session ID. `FindAndJoinBestSession`'s result carries both shapes
   from day one, or the swap leaks into every caller.

## 5. The phased plan (each rung gated, costed, crewed)

| Rung | What lands | Cost | Owner(s) |
|---|---|---|---|
| **GL-0** *(in slice, BP11)* | seam v1.1 (the 3 gaps) · server target proven by Gauntlet | $0 | services-builder, netcode-builder |
| **GL-1** | Server SDK behind `BRGameLiftLifecycle` · **Anywhere fleet on dev PC** · CreateGameSession/PlayerSession via CLI · the same Gauntlet smoke green with GameLift as lifecycle owner | ~$0 | services-builder; verifier gains the Anywhere rung |
| **GL-2** | API Gateway + Lambdas (`/auth/steam`, `/sessions/find-or-create`) · Steam ticket validation · short-lived tokens | pennies (free tiers) | services-builder; critic REFUTER on the token path |
| **GL-3** | ONE managed fleet (small instance, Spot where tolerable) · build-upload pipeline · admission live end-to-end · **billing alarms in the Done-when** | first real $ — **R16 trigger required** | services-builder + netcode-builder (trust boundary) |
| **GL-4** | queues, latency-based placement, backfill signal into `BRBotManagerComponent` | fleet-scale $ | services-builder, ai-builder consult |
| **GL-5** | DynamoDB (`FBRMatchTelemetry` rows, server-written only) · leaderboards read path · `BRLobbySubsystem` if the player base justifies it | low | services-builder, ui-builder |

**The R16 trigger for GL-3 (real money):** demo telemetry, not enthusiasm — host-quit
match-abandonment rate, NAT/join failure rate, host-advantage complaints. If those read
low, the game keeps shipping listen + GL-2 (auth/stats) and the fleet money stays unspent.
Tickets for GL-1+ are cut when Phase 2 opens (kickoff discipline) — not before.

## 6. The critic's Phase-2 attack surface (written down now, attacked then)

Forged/replayed player-session IDs · expired-token joins · a Lambda authorizer trusting the
client's *claimed* Steam ID instead of the validated ticket · client-reported leaderboard
writes (leaderboards read server-written results ONLY) · secrets in server logs/crash dumps
· and the one nobody models: **cost runaway** — a scaling misconfig is a denial-of-wallet
exploit; billing alarms + fleet caps are acceptance criteria, not ops afterthoughts.
