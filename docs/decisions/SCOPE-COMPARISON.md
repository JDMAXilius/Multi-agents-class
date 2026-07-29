# Concept A vs. Concept B — 6-Week Scope Comparison
## Which capstone is actually buildable in C++, with an agent crew, and shippable on Steam?

**Prepared:** 29 July 2026 · **Author:** Juan Diego Lugo
**Window:** 6 weeks · **Team:** 1 principal engineer + the 11-agent crew
**Hard requirement:** deploy on Steam
**Documents compared:** the Arena GDD (A — see `archive/slashroller/`) ·
`breachpoint/BREACHPOINT-GDD-FULL-CONCEPT.md` (B)

---

## 0. The two concepts in one line each

- **A — Slash Roller: Arena.** Third-person melee deathmatch (souls-weight
  combat, stamina, FFA/TDM), built on the studio's **already-shipped** GAS
  combat stack, Steam listen server.
- **B — Breachpoint.** Halo-inspired 4v4 arena **FPS** (shields over
  health, two-weapon carry, grenades, Grappleshot, bots), built **100%
  from scratch in C++** on the UE 5.8 First Person template, dedicated
  server on GameLift.

**Shared:** UE 5.8, pure native C++, GAS, MetaSounds, CommonUI, the same
11-agent crew, the same contracts, sourced art. **No vehicles, no Lyra
project** (art/animation may be sourced).

---

## 1. Method

Both concepts were decomposed into their **build inventory** — every
system that must exist at ship — and each item marked *reused*, *ported*,
*sourced*, or *authored*. Only **authored** and **ported** work consumes
the 6 weeks. Estimates are engineer-weeks for one principal with crew
assistance, based on the systems as specified in each GDD.

**Reading the numbers honestly:** these are *build inventory* estimates,
not a promise. What matters is not the absolute figures but the **ratio
between the two columns**, which is large enough that estimation error
doesn't change the conclusion.

---

## 2. Build inventory — Concept A (Arena)

| System | Origin | Weeks |
|---|---|---|
| GAS core (PlayerState ASC, input buffer, prediction) | **Reused — shipped** | 0 |
| Melee combat, `SoftLockTarget`, target assist, motion warping | **Reused — shipped** | 0 |
| Animation: AL Framework, Motion Matching, AnimGraph nodes | **Reused — shipped** | 0 |
| `OSSessionsSubsystem` (Steam sessions) | **Reused — shipped** | 0 |
| Validation-ladder bootstrap (specs + Gauntlet) | Authored | 0.5 |
| Match/round flow, scoring, respawn | Authored | 0.5 |
| Stamina + winded (GAS attributes/effects) | Authored | 0.5 |
| Bots (melee stance machine) | Authored | 1.0 |
| CommonUI HUD + front end | Authored | 1.0 |
| MetaSounds combat cues | Authored | 0.5 |
| Arena map (crew blockout + sourced art) | Authored/sourced | 0.5 |
| Caster agent + telemetry | Authored | 0.5 |
| Balance, polish, Steam depot, demo | — | 1.0 |
| **TOTAL NEW WORK** | | **6.0** |

## 3. Build inventory — Concept B (Breachpoint)

| System | Origin | Weeks |
|---|---|---|
| FPS template setup, project skeleton, module layout | Given/authored | 0.2 |
| GAS core **ported from OnSight** (own code, not Lyra) | **Ported** | 0.5 |
| Validation-ladder bootstrap (specs + Gauntlet) | Authored | 0.5 |
| Shields + health two-layer damage model | Authored | 0.5 |
| Weapon sandbox: 4 weapons, hitscan + projectile, two-weapon carry, ammo/reload/swap, damage-type table | Authored | 1.5 |
| Grenades (physics throw, cook, AoE effect) | Authored | 0.5 |
| Melee incl. rear-kill detection | Authored | 0.2 |
| **Grappleshot** (predicted movement ability, 3 modes) | Authored | 1.0 |
| Motion tracker (server-computed contacts + replication design) | Authored | 0.5 |
| Bots: StateTree + EQS + perception + 3 tiers | Authored | 1.5 |
| Team Slayer: teams, scoring, spawn scoring, power-weapon timers | Authored | 0.7 |
| CommonUI HUD (shields/health/ammo/radar/medals) + front end | Authored | 1.0 |
| Map: 3-level vertical arena, grapple points, spawn/power nodes | Authored/sourced | 1.0 |
| Dedicated server target + GameLift SDK 5 + containers + backend + Steam auth validation | Authored | 1.5 |
| MetaSounds (weapons, shields, medals) | Authored | 0.5 |
| Spotter agent + telemetry | Authored | 0.3 |
| Balance, polish, Steam depot, demo | — | 1.0 |
| **TOTAL NEW WORK** | | **12.9** |

---

## 4. The headline number

> **Concept A ≈ 6.0 engineer-weeks. Concept B ≈ 12.9 engineer-weeks.**
> **B is ~2.15× the build inventory of A, for the same 6-week window.**

Applying realistic crew compression (~20–30% on parallelizable content
work — map, tuning, QA soaks — but *not* on core systems, because one
principal reviews every packet and is the bottleneck):

| | Raw | After crew compression | Fits 6 weeks? |
|---|---|---|---|
| **A** | 6.0 w | ≈ 4.8–5.0 w | **Yes — with ~1 week buffer** |
| **B** | 12.9 w | ≈ 9.7–10.3 w | **No — ~4 weeks over** |

**With B's full pre-declared cut list applied** (GameLift → listen server
−1.0 · drop 4th weapon −0.35 · drop motion tracker −0.5 · single bot tier
−0.5 = −2.35 w):

> B ≈ 10.6 w raw → **≈ 8 w compressed. Still ~2 weeks over budget.**

**The critical diagnostic:** B's overrun is **not** caused by
infrastructure. Removing GameLift entirely still leaves ≈ 11.7 w. The
overrun is caused by **sandbox breadth** — weapons + grenades + grapple +
radar + bots + a vertical map — and breadth is precisely what makes it
feel like Halo. **You cannot cut B into budget without cutting the thing
that makes it B.**

---

## 5. Axis-by-axis comparison

| Axis | A — Arena | B — Breachpoint | Edge |
|---|---|---|---|
| Existing code reused | **~70%** (shipped GAS combat + anim + sessions) | ~5% (own GAS core ported only) | **A** |
| Systems to author | 8 | 16 | **A** |
| Net build inventory | 6.0 w | 12.9 w | **A** |
| Difficulty *per system* | Higher (melee warping under latency) — **but already solved** | Lower (hitscan is the most-solved problem in UE) | **B** |
| Total netcode surface | Small (reused, proven) | Large (weapons, grapple prediction, radar hidden-info) | **A** |
| AI depth | Melee stance machine | **StateTree + EQS, cover, squad rotation, power-weapon timing** | **B** |
| Art/content load | 1 small arena | 1 vertical map + 4 weapons + FPS anim sets | **A** |
| Infrastructure | Listen server, 1 build target | Dedicated server, 2 targets, containers, fleet, backend, Steam auth | **A** |
| Steam deploy complexity | Low | High | **A** |
| Market appeal on Steam | Niche | Broad | **B** |
| Capstone showcase value | Moderate | **Strong** | **B** |
| 6-week ship confidence | **High** | Low–Medium | **A** |

---

## 6. The C++ / Claude-buildability question

You asked specifically which is **quicker to build from C++ with Claude**.
Rating each system by how reliably an agent crew authors it from scratch:

| Domain | Buildability | Why |
|---|---|---|
| FPS weapons, hitscan, ammo, reload | **Excellent** | Enormous training signal; canonical patterns |
| GAS attributes, effects, cues | **Excellent** | Well-documented (GASDocumentation, Epic docs) |
| Game mode, scoring, respawn | **Excellent** | Standard UE framework work |
| CommonUI + ViewModels | **Good** | Documented Lyra-derived patterns |
| StateTree + EQS bots | **Good** | Newer API — less signal than Behavior Trees, but documented |
| GameLift / containers / backend | **Medium** | Well-documented but environment-specific; plugin lags 5.8 |
| **Predicted movement (Grappleshot)** | **Hard** | Prediction/reconciliation is where LLM-authored code most often breaks *subtly* — the silent-and-confident class |
| **Melee soft-lock + warping (A)** | **Hard** | …but **already written, shipped, and debugged** in your codebase |

**The finding that resolves your instinct:** you are **right** that B is
more programming-forward, and right that most of its code is the kind
Claude writes exceptionally well. But *"easier to write"* does not beat
*"already written."*

> **B has more code that Claude writes well. A has less code to write.
> Volume dominates ease.**

There is one genuine counter-point in B's favor worth recording: B's
hardest netcode problem (grapple prediction) is *one system*, whereas A's
hardest problem (melee warping under latency) would be *unsolvable in six
weeks if it weren't already done*. A's advantage is entirely borrowed from
past work — which is legitimate, but worth naming honestly.

---

## 7. Steam deployment comparison

| Step | A | B |
|---|---|---|
| Build targets | Client only | Client **+ Server** |
| Hosting | Listen server (host + invite) | Dedicated fleet (GameLift Managed Containers) |
| Backend service | None | Session placement service required |
| Identity | Steam sessions (`OSSessionsSubsystem`, shipped) | Steam auth ticket **validated server-side** |
| Container/image pipeline | None | Required |
| Cert/infra surface | Minimal | Substantial |
| Cost at idle | $0 | Fleet hosting costs |
| Est. infra work | ≈ 0.2 w | ≈ 1.5 w |

Both ship a Steam demo depot. **A's deploy is a build upload; B's deploy
is a build upload plus a live service.**

---

## 8. What each concept proves (capstone value)

| | A | B |
|---|---|---|
| "A crew can ship a game fast" | ✅ Proven | ✅ Proven — more impressively, if it ships |
| "Agents produce reviewable game content" | ✅ Map, bots, balance | ✅ Same, plus weapons sandbox |
| **"We can build real game AI"** | Partial (melee brain) | ✅✅ **StateTree + EQS + cover + squad + tiers** |
| "We build production multiplayer" | ✅ Listen server | ✅✅ **Dedicated servers + fleet + auth** |
| "We wrote it ourselves" | Partial — leans on prior work | ✅✅ **100% authored this term** |
| Demo-day impact | Moderate | **High** — shooting, explosions, verticality |

B is the stronger *portfolio* artifact on every axis except the one that
matters most for a course deadline: **finishing.**

---

## 9. Verdict

**For the stated constraints — 6 weeks, one principal, must deploy on
Steam — Concept A is the correct choice.** It fits with buffer; B is ~2×
over and does not fit even after its own pre-declared cuts.

**But the honest framing is a goal conflict, not a quality judgment:**

| If the real goal is… | Then choose |
|---|---|
| **Ship a finished game on Steam in 6 weeks** | **A — Arena** |
| **Best possible AI/engineering showcase, ship date negotiable** | **B — Breachpoint** |
| **Both, with an honest redefinition of "ship"** | **Option C, below** |

### Option C — B as a 6-week vertical slice, Steam-deferred

If B is the game you actually want to make, the way to make it real is to
change the *deliverable*, not the *scope*:

- **Ship target:** a polished **vertical slice** — 1 map, 3 weapons
  (AR + Magnum + Rocket), grenades, melee, **Grappleshot**, 1 scaled bot
  profile, Team Slayer, listen server.
- **Corrected estimate:** ≈ **8.4 w raw → ≈ 6.3 w compressed.** *(A first
  pass here read ≈5.5–6 w; re-deriving line-by-line gives 6.3. **The
  authoritative figure and its schedule live in
  `breachpoint/BREACHPOINT-GDD-VERTICAL-SLICE.md` §7**, which plans around the
  gap with a pre-declared cut order and a Week-4 go/no-go.)* It fits six
  weeks with near-zero slippage, seven comfortably.
- **Steam:** upload the slice as a **playable demo depot on listen
  server** (still satisfies "deployed on Steam"), with `IOSServerLifecycle`
  already in place so the GameLift fleet is a post-course swap.
- **What you lose:** the full sandbox (plasma, rockets, radar, 3 bot
  tiers) and the dedicated-server story — both **post-course phase 2**.
- **What you keep:** the golden triangle, shields-over-health, the
  Grappleshot pillar, real StateTree/EQS AI, 100% authored C++, and a
  Steam page.

Option C is the only version of B that both **fits the window** and
**preserves what makes it Halo** — because it protects the triangle and
the traversal pillar and sacrifices breadth instead of identity.

---

## 10. Recommendation

1. **If the course grade depends on a shipped, finished game:** build **A**.
   It is the only option with schedule buffer, and its risk is borrowed
   from work already proven.
2. **If the course grade depends on the crew method and the engineering
   showcase** (which the syllabus emphasis suggests): build **B as
   Option C** — the vertical slice — and say so explicitly in the GDD's
   scope section. A finished slice beats an unfinished game every time.
3. **Do not attempt B as fully specified in six weeks.** The build
   inventory says ~10 weeks compressed; committing to it means shipping
   nothing, which is the one outcome that fails every rubric.

**Decision gate:** the Week-2 fun-gate in either concept is the honest
checkpoint. If B's golden triangle isn't fun by end of Week 2, the
remaining four weeks cannot save it — and A's foundation is still sitting
there, shipped and waiting.

---

## Appendix — Estimation assumptions

- One principal engineer at full-time capacity; crew accelerates content
  and review, not core-system authorship (the reviewer is the bottleneck).
- Crew compression applied at 20–30%, consistent with the finding in
  `ARCHITECTURE-VALIDATION.md` that multi-agent parallelism pays only on
  genuinely decomposable work.
- "Sourced art" assumes marketplace/free assets requiring integration, not
  authoring — integration time is included in the map/weapon line items.
- Estimates exclude learning time for unfamiliar APIs (StateTree, EQS,
  GameLift), which would push B higher, not lower.
