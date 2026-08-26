# AIBOT — the protocol book (aib-verifier's instrument list)

> Proofs 3 and 4 of `docs/AIBOT-ROADMAP.md`, concretized: every protocol names its expected
> `LogAIBot` lines and its sample counts BEFORE the run (the 19-of-20-FORWARD method — a
> behaviour claim is a count, never an impression). Rung named on every claim. A protocol
> that cannot finish is BLOCKED, never half-reported.

## The instruments (what the module already logs)

| line | emitted when | what it proves |
|---|---|---|
| `AIBot: <bot> possessed <pawn>, avatar door open.` | possession, adapter found | the seam works |
| `AIBot: ... found NO avatar adapter component.` | possession, adapter missing | the designed miss answer (F7) |
| `AIBot: <bot> acquired <target> after N.NNNs reaction.` | first think after a sighting matures | the F1 sample: N.NNN ≥ 0.200 ALWAYS |
| `AIBot: <bot> ambition -> <tag> (S.SS) over <runner> (R.RR)[ [interrupt]]` | winner changes | arbitration live; hysteresis visible (S vs R); cause class |
| `AIBot: ... dropped a blast warning (no world/pawn/authority).` | blast note refused | F7 on the warning path |
| `AIBot: ... could not perceive a blast warning — dropped (F2).` (Verbose) | perceivability gate fires | the wall-grenade ban working |

## P-1 · First possession (Phase 3, first PIE with BotSystem=AIB)

Rung 3. Solo PIE, BR_Arena01, `BotSystem=AIB`, one bot (`TargetPlayers=2`).
- EXPECT exactly one `possessed ... avatar door open` per bot; ZERO `found NO avatar adapter`.
- EXPECT one `ambition -> AIBot.Ambition.Roam` within 1s of possession (the fresh-spawn floor).
- FAIL any `did not resolve` on `AIBBotControllerClass`.
- Counts: `grep -c "avatar door open" log` == bots; `grep -c "NO avatar adapter" log` == 0.

## P-2 · The fairness latency sample (every AIB PIE session, standing)

Rung 3. From any session's log:
- `grep "acquired .* after" log` — EVERY value ≥ 0.200. One sub-floor line = `high` for
  aib-critic, not a tuning note.
- Distribution check when ≥10 samples: values spread inside [tier min, tier max + one think
  interval + hitch]; identical values across DIFFERENT bots on the same stimulus = the
  lockstep-seed regression (P2 F-3.7), reopened.

## P-3 · Ambition arbitration on the walked scenarios (Phase 3)

Rung 3. Solo PIE, stand where a bot can acquire you.
1. Spawn-deafness: bot must log `-> Engage` within ~1s of your first `acquired` line —
   never after a 6s Roam hold (the H-1 regression).
2. Corpse-chase: kill the bot's target (or die and respawn far); `-> Search` or `-> Roam`
   must follow within ~2 thinks of the target vanishing — never a 5s Engage hold (H-2).
3. Hysteresis visibility: in any switch line, winner score vs runner-up — squeakers
   (< 15% apart) should be RARE; frequent squeaker-switches = hysteresis regressed.
- Counts: paste 10 consecutive switch lines with wall-clock stamps.

## P-4 · The blast (Phase 3+, needs grenades flying)

Rung 3. Throw AT a bot in the open, and FROM behind cover it has never seen around.
- Open throw: bot may react no earlier than 0.20s after the warning (pair the throw time
  with its first movement change); an Evade-shaped ambition line carries `[interrupt]`.
- Unseen throw: the Verbose `could not perceive` line fires and the bot does NOT dodge —
  eating that grenade is the PASS (F2 working).
- Counts per session: ≥5 open throws, ≥3 unseen throws; report reacted/ate per class.

## P-5 · The A/B acceptance match (Phase 9, the roadmap's proof 2)

Rung 4 (listen + client). Mixed match in BR_Arena01: BN bots vs AIB bots, `TargetPlayers=8`,
one full match, observed from a CLIENT screen.
- Telemetry per side: eliminations, damage dealt/taken, avg reaction sample, objective time
  (when modes land).
- Humanness observations, NAMED not felt: strafe-while-firing ratio (the 19-of-20 method),
  corner-juke behaviour (does the bot aim at the vanish point or track through the wall),
  grenade-eaten-from-behind count (should be > 0 — omniscient dodges are the failure).
- DONE = AIB wins the scoreboard while its humanness counts are no worse than BN's.

## Cadence rules (unchanged, restated)

Rung 2 (`Tools/run-specs.sh AIBot`) with NO editor holding the project; zero-ran =
INCONCLUSIVE. Rung 2 and editor-dependent protocols never share a W-VERIFY wave. Every
protocol's Log entry: rung · command/scenario · counts · verdict · pasted lines.
