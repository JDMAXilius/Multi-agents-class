# DECISIONS OWED — the founder's register

**Written 1 Aug 2026 by the lead.** Every decision on this board that is waiting on the founder,
in one place, ordered by **what it unblocks**. Swept from all seventeen ticket Logs,
`docs/WORK-ROUTING.md`, `docs/tickets/HANDOFF.md`, `Tools/ue_mcp/{SURFACE,RESEARCH,STEP1-PLAN}.md`,
`docs/bus/`, `docs/BUILD-STATE.md` and `docs/DESIGN-RULINGS.md`.

**This file decides nothing.** Every "recommend" line below is a recommendation, not a ruling.
A decision becomes real when it lands in `docs/DESIGN-RULINGS.md` (if it is a law-8 ruling) or in
the named ticket's Log (if it is an ordinary call). Per CLAUDE.md's closing line: *a decision that
lives only in chat is lost.*

**Law-8 marking.** `RULING` = it must get an R-number in `docs/DESIGN-RULINGS.md`, because reviews
will judge against it. `call` = an ordinary decision; it goes in the ticket Log. Rulings are
**closed once made** — nothing below re-opens an existing R-number. Two items the founder's own
list carried are already closed and are recorded in §Closed at the bottom rather than re-litigated.

---

## ⏰ EXTERNAL DEADLINE — D1–D3 are due before **6 August 2026, 11:59 ET**

`TICKET_BP15_ARCHITECT.md` Notes: *"Assignment #5's slide states two dates — 13 August in the
header, 'Before S09 (6 August 11:59 ET)' in the table. **Plan against 6 August** until the
instructor confirms."* Five days from today. The deliverable is written and lives at
`docs/ASSIGNMENT-5.md`; D1–D3 are the three questions it currently answers with *"not fixed — this
is a design decision for the project owner."* They are answerable in one sitting, from a desk, with
no engine. **A fourth, free action: confirm the real date with the instructor** so the register can
stop planning against the earlier of two.

---

## Summary

| # | The question, in one line | What it blocks | Law 8 |
|---|---|---|---|
| **D1** | Is the `depth` term **added or subtracted** in the architect's score? | The course deliverable's answer to "why did it pick that one" · the architect's next pick | **RULING** |
| **D2** | Add a fifth **readiness** term (are the unit's inputs actually on disk)? | Same deliverable · whether BP15 step 4 can re-run against a startable unit | call |
| **D3** | Demote the **`#include` graph** from a score term to a printed cross-check? | Same deliverable · F3 (one non-compiling line moves the pick) | call |
| **D4** | Accept **(a) executor-only** as the law-7 MCP boundary, and number it? | Every mutating MCP call · BP16 steps 3–5 · the OPEN-mode path for the R26 rename, input generator and CSV reimport | **RULING** |
| **D5** | Who owns the two one-line fixes that stop the module compiling? | **Rung 1 for the entire project** | call |
| **D6** | Amend ARCHITECTURE §3 to give the **projectile a home**, and assign it | BP05 step 1 · **all of BP09** | **RULING** |
| **D7** | Does **BP03** land the pickup's replicated-motion seam before it closes? | BP06 step 2 — and the **window closes when BP03 closes** | call |
| **D8** | Where does the **headshot multiplier** compose — row, curve, or both? | BP03's fire-path damage composition | **RULING** |
| **D9** | Buy the **second machine**; supply a **Steam App ID + two test accounts** | All of BP11/M5 · BP12 behind it. Long procurement lead time | call |
| **D10** | Widen **R29.3** from *"a build"* to *"anything that takes the project lock"* | **Nothing today.** Cheap; prevents a future misread | **RULING** |
| **D11** | Do the four **UNDECLARED** units become numbered units, or get deleted? | Nothing mechanically. §3's 44-unit budget is wrong until answered | **RULING** |
| **D12** | Install the **`Stop` hook** for the session bus? Needs explicit consent | The bus's "never stop working" property. `bus.py` already claims this hook exists | call |
| **D13** | **Standing rule** for Windows/`Tools/` platform defects, or keep filing Log entries? | Nothing. Prevents a fifth recurrence | RULING *if adopted* |
| **D14** | `.mcp.json` is **already committed** — endorse it, and does anything guard it? | Nothing today. It is the key a future MCP gate would match on | call |
| **D15** | `docs/bus/` — commit or gitignore? | Nothing. Decide before the history fills | call |
| **D16–D19** | Four smaller calls — see the tail table | Little to nothing | mixed |

---

## D1 — The sign of the `depth` term · **RULING** · deadline

**Question.** `score = depth + blockers + tier + state`. `depth` is *distance from the roots of the
ticket DAG* — how many tickets must finish first. It is **added**. Should it be subtracted, dropped,
or left as is?

**Filed.** `docs/tickets/TICKET_BP15_ARCHITECT.md` Log — step 6 finding **F6b** (~L753–776), and
again in the 1 Aug fix entry (~L861–868): *"**Flagged, not fixed: the sign of the depth term is a
founder call**, on the same precedent as the readiness term."* The same entry says explicitly that
it *"belongs in `DESIGN-RULINGS.md`, not in a builder's diff."* Restated in
`docs/ASSIGNMENT-5.md` §"What is still wrong".

**Blocked until answered.**
- The course deliverable's central answer. `ASSIGNMENT-5.md` Q2 has to report that the corrected
  score picks **`BRSpotterSubsystem` (BP11, total 104)** — gated behind BP08, itself gated behind
  BP02+BP04 — and that it wins **on depth 4 alone**. That is the least startable unit on the board.
- BP15 step 4's re-run. The blackboard currently authorises `BRSpotterSubsystem`; dispatching a
  builder at it repeats step 4's outcome (stop at a `contract_gap`) for a different reason.

**Correction to the founder's framing, verified.** The list said the corrected pick is *"gated two
tickets deep"* — it is **three** by BP11's own chain (BP11 ← BP08 ← BP02+BP04). And the illustration
*"startable test specs rank 7–9"* was true at the time of writing and is **being overtaken right
now**: T2 has two sim-builders writing `BRCombatSpec.cpp` and `BRShieldSpec.cpp` this session (bus,
1 Aug, BP02). Two of the three depth-0 units the defect suppressed are landing anyway, by hand. That
strengthens the case for the fix rather than weakening it — the human board routed around the
scorer.

**Options.**
- **(a) Subtract it** (`-depth`). Rewards being close to a root, i.e. startable. Smallest edit, one
  sign. Risk: it now *penalises* deep units permanently, so a genuinely critical late unit never
  surfaces — but `blockers` is the term that is supposed to carry criticality anyway.
- **(b) Drop it** and let `blockers` carry criticality alone. Honest about what the term was for;
  loses the only signal distinguishing two units with equal blocker counts. Reduces the score to
  three terms, which is a visible change to BP15 step 2's spec.
- **(c) Keep it added**, and rule that "what to build next" means "what unblocks the most eventually",
  not "what can start today". Defensible only if D2 (readiness) lands, because readiness is then the
  term that stops it dispatching at un-startable work. **(a)+(c) are mutually exclusive; (c)+D2 is a
  coherent pair.**

**Recommendation — (a), subtract.** Reason: the ticket's own name for the layer is *"score what to
build next"*, and the evidence is in the generated file. `docs/BUILD-STATE.md` reports rung 2
**BLOCKED because `Tests/` is empty**, while ranking the three units whose absence causes that block
at 7–9 — below `BRGA_Grapple`, *"THE netcode packet"*, which outranks them **as a reward for having
three more prerequisites.** A scorer that demotes the thing blocking the whole ladder is not
measuring what its ticket says it measures. (a) is one character and is provable in one re-run.
If (c) is preferred instead, it should not be taken without D2.

---

## D2 — A fifth "readiness" term · call · deadline

**Question.** Add a fifth score term measuring whether a unit's **inputs actually exist** — declared
gameplay tags, row-struct fields, CSV columns — so the score can distinguish *most valuable* from
*startable*.

**Filed.** `TICKET_BP15_ARCHITECT.md` Log, step-4 entry (~L446–453): *"the score ranks by dependency
depth, blocker count, tier and state — **it has no term for whether a unit's inputs exist** … Filed
as a proposal, not implemented: step 2's four terms are specified by this ticket, and adding a fifth
is a founder call, not a builder's."*

**Blocked until answered.**
- Nothing mechanically — the architect runs and exits 0 today.
- But it is the direct cause of the step-4 result the deliverable reports: `BRGA_WeaponFire` was
  selected, authorised on the blackboard, and a builder dispatched **before** anyone knew its two
  data inputs did not exist. Those two gaps are still open at HEAD (verified today: `FBRWeaponRow`
  has no range and no spread; `DT_Weapons.csv` has no `AbilitySet` column — 16 columns, none of
  them). Without this term, the next dispatch can repeat that.

**Options.**
- **(a) Add it as a fifth additive term** with a modest weight. Cheap; surfaces un-startability in
  the printed table. Risk: another term computed from disk, and F3 already shows that disk-derived
  terms are in a feedback loop with model output.
- **(b) Add it as a gate**, like `state` and `tier` (a large negative for "inputs absent"), so an
  un-startable unit can never be selected. Stronger; risks a unit becoming permanently invisible
  because a single CSV column is missing.
- **(c) Do not add it; print readiness as a non-scoring column** next to the ranked table. Zero
  ranking risk, keeps step 2's four-term spec intact, and still stops a builder being dispatched
  blind — a human reads the column before authorising.

**Recommendation — (c) now, (b) later.** Reason: this is a deadline week, and (c) is the version
that survives F3's objection completely — a printed column cannot be gamed by a line of source
because it does not feed the total. It delivers the whole practical benefit (nobody dispatches at a
unit whose inputs are absent) at zero risk to the property the assignment is actually about
(*an LLM never chooses the unit*). Promote to (b) once F3 is settled and readiness has been observed
to be correct over a few runs. **Answer D1 and D2 in the same sitting** — together they decide
whether the score means "most valuable" or "most valuable among startable".

---

## D3 — Demote the `#include` graph from a score term? · call · deadline

**Question.** `depth` and `blockers` are computed partly by parsing `#include` edges out of
`Source/` — which is what the model writes. Should the include graph stop feeding the total and
become a printed cross-check that flags disagreement instead?

**Filed.** `TICKET_BP15_ARCHITECT.md` Log, step 6 **F3** (~L570–611), and the fix entry's closing
line (~L825): *"**F6b's sign and F3's demotion are founder calls**, per this ticket's own precedent
for the readiness term."* Still listed as unfixed in the 1 Aug entry's *"Not fixed, still open"*.
Not on the founder's list — surfaced here because it is the third item in the same sentence as D1.

**Blocked until answered.** Nothing operationally. But it is a live correctness hole in the thing
the assignment is being graded on: adding **one `#include` line naming a header that does not
exist** moved a unit **+27 points and to #1**. `INCLUDE_RE` checks only that the captured name is a
declared unit; it never checks the header exists, so a line that **cannot compile** still moves the
ranking. A stale include or a builder tidying headers is enough.

**Options.**
- **(a) Demote fully.** Compute `depth`/`blockers` from the ticket DAG and the manifest's declared
  relationships only; print the include graph as a cross-check that flags disagreement. Closest to
  the ticket's own wording (*"declared include edges"* — an include a model just wrote is not
  declared). Loses real signal about coupling the manifest does not record.
- **(b) Keep it as a term, but require the included header to exist on disk.** Two lines. Closes the
  proven exploit; does **not** close the structural objection that the scorer reads its own output.
- **(c) Leave it.** Only defensible if the run log prints the include edges by name so a reviewer
  can see what moved.

**Recommendation — (b) this week, (a) after the deadline.** Reason: (b) is two lines and kills the
only *proven* wrong outcome before the deliverable is submitted; (a) is the correct long answer but
changes what two of four terms mean, which is not a change to make five days out with no rung-2
coverage of `rank()` (F7: `classify`, `rank` and `blackboard` still have zero test cases). Whichever
is chosen, **F7's missing cases should land with it** — F4 and F5 were both found in exactly that gap.

---

## D4 — The law-7 MCP boundary: accept (a) executor-only, and number it · **RULING**

**Question.** When an agent creates an asset through the UE MCP, what is the committed, reviewable
artifact? **(a)** MCP is an executor and the committed script stays the artifact · **(b)** MCP calls
are recorded to a committed, replayable transcript · **(c)** MCP is exploratory only, never lands a
shipping asset.

**Filed.** `TICKET_BP16_UE_MCP_BRIDGE.md` step 2 (the three options) and its Log entry *"1 Aug 2026
— step 2 PROPOSAL — not a ruling"* (~L267–418), which recommends **(a)** and states plainly:
*"**Per law 8 the R-number and the acceptance are the founder's**."* Also `Tools/ue_mcp/STEP1-PLAN.md`
L51–52 (*"Draft step 2's ruling … **as a proposal for the founder**"*) and the bus thread
`docs/bus/20260801T161722Z…` / `…164846Z…`.

**Blocked until answered.** This is the largest unblocking item on the board.
- **Every mutating MCP call.** T2 has confirmed the tools exist by name and schema —
  `DataTableTools.import_file`, `CurveTableTools.import_file`, `DataAssetTools.create`,
  `AssetTools.move` — and has fired **none** of them, correctly, because there is no R-number.
- **BP16 steps 3, 4 and 5**, and 4 of its 6 Done-when boxes.
- **`WORK-ROUTING.md` §4a stays a hypothesis.** If (a) lands and BP16 step 4 measures the retrofit,
  the R26 rename / input generator / CSV reimport may move out of CLOSED mode — which is the board's
  whole critical path. Until then §4 stands as written and all three wait for the editor to close.

**Options and the real trade-off.**
- **(a) Executor-only.** No new machinery: `ProgrammaticToolset.execute_tool_script` already runs a
  committed Python script whose sandbox imports only `{time, datetime, math, json, re, copy}` — no
  `unreal`, no `os`, no `open()`. The reviewable act lands via `Write`, which is **the one
  jurisdiction `guard_laws.py` actually has**. That is the mechanical argument and it is bigger than
  the convenience one. Cost: does not close the jurisdiction hole; nothing available today does.
- **(b) Recorded transcript.** Rejected on four independent grounds in the proposal — there is no MCP
  call journal (so the transcript is a self-report by the calling agent), no undo/transaction/
  snapshot/source-control toolset (so replay cannot be a controlled experiment), documented
  silent-nulls (`add_to_scene_from_*` *"return nothing if creation was unsuccessful"* — a replay
  reproduces the calls and can silently not reproduce the actors), and no transaction boundary
  (a half-completed batch leaves a state no committed artifact describes). Law 7 also wants the
  reviewable thing to exist *before* the asset; a transcript exists only after.
- **(c) Exploratory only.** Strictly dominated: (a) permits everything (c) permits, plus the one
  thing that pays for this ticket. And (c) buys **no** enforcement — the server has no read-only
  mode, no auth, and no per-tool permission; all 255 tools front the single name `call_tool`. (c)
  would also kill BP16 step 4, the only measurement that settles §4a.

**One material fact that post-dates the proposal and belongs in the ruling.** T2 fired a rejecting
`AssetTools.write_file` case (`SURFACE.md` §5): the confinement **held**, but its refusal enumerated
**~80 allowed roots and only two are ours** (`<project>\Content`, `<project>\Saved`). The rest are
**engine-install** plugin `Content/` directories under `UE_5.8_Source`. **An MCP call can write
inside the engine install — outside the repo, invisible to git.** That is a wider blast radius than
"the project's Content folder" and it is the one direction version control cannot cover.

**Recommendation — accept (a), with the proposal's five sub-rules, one addition, and two conditions.**
Reason: (a) is the only option that relocates the reviewable act back into the jurisdiction the hook
already has, and it needs no machinery that does not exist. The addition: rule 4 should say *writes
into `Content/Data/` **and the engine install** are checked by git and by nothing else* — and the
engine install is not checked by git at all, so it is a prohibition, not a caveat. The two
conditions, both from the proposal's own honesty flags: **(i)** it owes a critic REFUTER pass before
it is closed, and **(ii)** every `R`/`E`/`M` mark it reasons from is description-derived — only four
tools have been fired at — *"if a tool marked R turns out to mutate, this proposal is wrong in a way
no amount of doctrine catches."* Number it, then have step 5 attack that first. Step 3 (the
`data-and-assets.md` fill-in) lands in the same sitting — the Done-when needs **both**, not either.

---

## D5 — Two unowned one-line fixes are stopping the module compiling · call

**Question.** Who owns (i) renaming one of `FBRKillfeedEntry` / `FBRKillFeedEntry`, and (ii) adding
`"SlateCore"` to `Breachpoint.Build.cs`?

**Filed.** `TICKET_BP02_GAS_CORE.md` Log, `contract_gaps` (~L233–240). Both were applied **locally**
to get a clean UHT + link, and **neither was applied to the repo**: *"Both are filed below; neither
was applied to the repo."*

**Blocked until answered.** **Rung 1, for the entire project.** BP02's own words: *"UHT compares
engine names case-insensitively. **Nothing in the module compiles until one is renamed.**"* And the
link fails with 8 unresolved Slate symbols without `SlateCore`. Every other rung-1 claim on the
board — BP15 step 4, BP03 step 2, the four new gameplay tags, the two spec files T2 is writing right
now — is downstream of this. It is the cheapest high-value item in this register.

**Options.**
- **The rename.** (i) BP04 renames `FBRKillFeedEntry` in `Match/BRGameState.h`; (ii) BP10 renames
  `FBRKillfeedEntry` in `UI/BRUITypes.h`; (iii) neither renames — one struct is deleted and both
  packets share one. BP02 flags the real issue: *"two structs describing the same concept in two
  packets is itself the finding."*
- **`Build.cs`.** It is in **no** packet's `owner_path`. Either grant it by exact file to whichever
  packet next needs a module (the same device R23 uses for `BRGameplayTags.h/.cpp` and BP01 used for
  its three `.Target.cs` entries), or declare a standing owner for build files.

**Recommendation — (iii) for the struct, exact-file grant for `Build.cs`.** Reason: two structs for
one concept is a defect that a rename preserves. The killfeed entry is UI-facing data produced by the
match layer; BP10 should own the one struct and BP04 should consume it, which also removes the
`Build.cs` question's twin later. For `Build.cs`, the exact-file grant is the device this board has
already ruled on twice — it needs no new ruling, only an application. **If the founder wants the
fastest unblock instead of the cleanest: rename BP04's to match BP10's and grant `Build.cs` to the
next CLOSED-window claim.** Either way it is one line each and it should not survive the week.

---

## D6 — The projectile has no home in ARCHITECTURE §3 · **RULING**

**Question.** Which §3 folder owns the server-authoritative projectile class, and which discipline
authors it?

**Filed.** `TICKET_BP05_TRIANGLE.md` Log (~L90–107): *"**ARCHITECTURE GAP — the grenade projectile
has no home, and neither does the rocket's.** … §3.5 `Weapons/` enumerates exactly three units …
**none of them is a projectile.** No other §3 folder claims one either. … *Escalated to the lead;
deliberately not decided inline.*"* Not on the founder's list.

**Blocked until answered.**
- **BP05 step 1** — the grenade needs a projectile with bounce physics and a fuse.
- **All of BP09.** Its ticket says the rocket *"is a ROW plus a spawner, not a system: projectile +
  radial damage already exist (BP05)"* — so **BP09 is explicitly relying on a class §3 never
  allocates.** BP09 is the M-milestone's first cut if W5 slips, but it cannot be started at all
  today.
- It is **not** fixable at claim time: *"you cannot grant a path to a file the architecture never
  named."* This is the one shape of blocker that `owner_path` amendment cannot route around.

**Options.**
- **(a) `Weapons/` gains a fourth unit** (`BRProjectile`). Natural reading — projectiles are weapon
  output. Takes §3.5 from 3 to 4 and the in-slice budget from 43 to 44 (and the total to 45), so the
  composition table in §4 and BP15's self-check move with it. That is mechanical: `architect.py`
  parses §3 and asserts, so the number is checked, not trusted.
- **(b) `AbilitySystem/Abilities/`** — keeps the grenade's whole feature in one owner's folder.
  Wrong on the merits: a rocket projectile is not an ability, and BP09 would then reach into BP05's
  ability folder, which is the same law-7 collision as D7.
- **(c) A new `Projectiles/` folder.** Cleanest conceptually, most expensive: a new §3 folder, a new
  `LogBR*` channel under R24, a new owner-path row in §9.

**Recommendation — (a), `Weapons/BRProjectile`, owned by sim-builder for ballistics with
netcode-builder on spawn/replication/dormancy.** Reason: it is the reading BP05 itself proposes, it
matches how step 1 already splits its owners, and it is the only option that costs one table row.
Note this is a §3 amendment and therefore a ruling — a packet that invents a home for a class the
architecture does not name is precisely the improvisation law 5 exists to prevent.

---

## D7 — The pickup attract seam · call · **TIME-BOXED: decide before BP03 closes**

**Question.** Does **BP03** land the pickup's replicated-motion seam (an interface or a server-called
`AttractTo`) as part of its own packet, so BP06 only calls it?

**Filed.** `TICKET_BP06_GRAPPLE.md` Log (~L74–88): grapple-a-weapon means motion code on
`ABRWeaponPickup`, *"a class **BP03 authors and owns**. That is not merely an owner_path amendment —
it is two packets writing the same file across a milestone boundary, which **law 7's one-owner rule
exists to forbid.** … but that is a design call for the lead, and it has to be made **before BP03
closes**, or BP06 arrives to find the seam absent and its owner archived."*

**Blocked until answered.** BP06 step 2's weapon-attract mode is BLOCKED today either way — BP06 is
gated behind BP05, gated behind BP02. What is *urgent* is not the block, it is the **window**:
`WORK-ROUTING.md` §4 puts **BP03 step 2 fifth in the next CLOSED batch**. If BP03 closes without the
seam, the only remaining resolutions are a law-7 violation or reopening an archived ticket.

**Options.**
- **(a) BP03 lands the seam.** One interface/virtual on `ABRWeaponPickup`, authored by its owner;
  BP06 calls it. Preserves one-owner-per-file cleanly. Cost: BP03 writes a few lines it does not
  itself consume, which reads as speculative work — and it must be specified now, sight-unseen of
  the grapple's actual needs.
- **(b) Defer, and amend `owner_path` when BP06 claims.** Two packets write one file across a
  milestone boundary. Law 7 forbids exactly this; it would need its own ruling to permit.
- **(c) Move `ABRWeaponPickup`'s motion to a component BP06 owns.** Avoids the collision by
  splitting the artifact. More design than the problem deserves, and it fragments the pickup.

**Recommendation — (a), and make it a Done-when line on BP03 step 2 today**, before that packet is
picked up in the next CLOSED window. Reason: (a) is the only option that does not require a new
ruling, and the cost of specifying it slightly wrong is a few lines BP06 revises inside its own
folder — versus the cost of (b), which is a law-7 exception that would apply forever. **This is the
only item in the register with a window rather than a deadline**, and the window is short.

---

## D8 — Two sources of truth for the headshot multiplier · **RULING**

**Question.** `FBRWeaponRow::HeadshotMult` is per-weapon (AR 1.0, Magnum 2.0 — R1/R2/R4) *and* the
exec calc composes a global `Damage.Headshot.Multiplier` curve. Shipping both live multiplies them.
Which axis is authoritative?

**Filed.** `TICKET_BP02_GAS_CORE.md` Log (~L395–401), ending in exactly two words: *"**Needs a
ruling.**"*

**Blocked until answered.** BP03's fire path — the largest ready code item on the board — has to
know where the multiplier composes before it writes the damage call. BP02 shipped an interim
position (`CT_Combat` ships `Damage.Headshot.Multiplier = 1.0`, identity: *"the modifier axis exists
and is switched off"*) and expects BP03 to fold the weapon's `HeadshotMult` into
`SetByCaller.BaseDamage` **on the server, after it validates the hit bone**. That expectation is not
written anywhere a BP03 builder is required to read.

**Options.**
- **(a) Ratify BP02's interim position.** Per-weapon row is authoritative; the global curve stays at
  identity as a switched-off modifier axis. Zero code change; needs only to be written down where a
  builder meets it.
- **(b) Global curve is authoritative**, rows carry only a per-weapon *delta*. More composable.
  BP02's warning is the objection: *"Raising the global curve would silently buff every weapon
  including the two whose 1.0 is a design position"* — R2 (AR stays 1.0) and R4 (Rocket stays 1.0)
  are **existing closed rulings**, so (b) puts a live tuning knob on top of two settled design
  positions.
- **(c) Delete the global curve row.** Simplest and unambiguous; loses the modifier axis
  permanently, which is real optionality for a future arena-wide modifier.

**Recommendation — (a), ratify.** Reason: it is already the shipped state, it costs nothing, and it
is the only option consistent with R1/R2/R4 without qualification. The whole content of this ruling
should be one sentence plus a warning: *the row is authoritative; the global curve is a switched-off
modifier axis and raising it above 1.0 silently overrides two closed rulings.* Land it in the ledger
**and** in `contracts/gas-purity.md`, because the ledger is not what a fire-path builder reads first.

---

## D9 — The second machine, the Steam App ID, and two test accounts · call

**Question.** Acquire a second machine, and supply a Steam App ID plus two test accounts — or scope
BP11 explicitly to OSS Null and say so.

**Filed.** `docs/WORK-ROUTING.md` §6: *"**A second machine (BP11).** The Steam rung needs two. Not
owned today; acquiring it is a **founder action with lead time**, so it is worth starting before BP11
is otherwise ready."* And `TICKET_BP11_ONLINE_STEAM.md` Kickoff: *"A Steam App ID and two test
accounts are available (or the packet explicitly scopes to OSS Null and says so — an invite claim
needs Steam, per the twice-tested law)."* Same ticket: *"**TD holds upload credentials.**"*

**Blocked until answered.** The whole of **M5/BP11**'s two-box test, and **BP12** behind it. Nothing
in the crew can substitute for it: a one-box Steam test proves nothing about NAT, invites, or
host-quit from a remote client. It is listed here despite being a *purchase* rather than a *ruling*
because it is the only item on the board with a **procurement lead time**, and BP11 is gated behind
BP08, which is close.

**Options.** (a) Buy/borrow a second machine now and register the App ID. (b) Scope BP11 to OSS Null
and defer every Steam claim to a later ticket — honest, and it means the slice ships without ever
having tested its own shipping topology. (c) Do nothing and discover it when BP11 is claimed.

**Recommendation — (a), start now; (b) as the written fallback.** Reason: this is the one item where
delay costs calendar time that no decision later can recover. If (a) is not happening, **say so in
BP11's Kickoff this week** — a Kickoff condition nobody can satisfy reads as blocked work when it is
actually cancelled work, and this board has already catalogued four defects of exactly that shape.

---

## D10 — R29.3 names the wrong operation · **RULING** (amend R29)

**Question.** R29.3 says an editor session and a **build** must not overlap. The real constraint is
the **project lock**, which `UnrealEditor-Cmd -run=pythonscript` also takes without being a build.
Widen the wording?

**Filed.** `TICKET_BP16_UE_MCP_BRIDGE.md` Log (~L250–258): *"**R29.3 names the wrong operation.** …
Every editor-driving script we own is a commandlet, so R29.3's literal wording covers the case we
rarely hit and misses the case we hit every time. … **A ruling whose wording is narrower than the
guards implementing it will eventually be read instead of the guards.** Filed here rather than
amended unilaterally: R29 is a closed ruling (law 8), and widening *build* → *anything that takes the
project lock* is the founder's call, not a lead's edit."*

**Blocked until answered.** **Nothing.** Stated plainly because it is a useful answer: it bites
nobody today, because both editor-driving scripts guard on *"any editor process is live"* — the
correct test — rather than on R29.3's text (`build-input.ps1:180`, `rename-r26.ps1:88`). This is a
pre-emptive fix, not a live blocker.

**Options.** (a) Amend R29.3's wording in place to *"an editor session and **anything that takes the
project lock** must not overlap — builds and `-run=pythonscript` commandlets alike"*, dated as an
amendment (the R17-amended-by-R23 precedent). (b) Cut a new sub-clause R29.4 covering commandlets and
leave .3 alone. (c) Leave it and rely on the guards.

**Recommendation — (a), amend in place with a dated amendment line.** Reason: R29 already carries its
own origin story about a *miscitation* being worse than a missing rule — *"it reads as settled, so
nobody checks"* — and a rule narrower than its own guards is the same failure wearing a different
hat. (a) is three words and it is the amendment shape the ledger already uses. **Note this widens a
closed ruling; it does not re-litigate it** — R29.1 and .2 are untouched and the direction of .3 is
unchanged.

---

## D11 — Four UNDECLARED units · **RULING**

**Question.** Do `BRCombatCurves` (`AbilitySystem/`), `BRRootLayout`, `BRUISettings`, `BRUITypes`
(`UI/`) become numbered units in ARCHITECTURE §3, or are they something else?

**Filed.** `docs/BUILD-STATE.md` (generated, ~L69–75): *"**UNDECLARED — real `BR*` source on disk
that §3 does not declare.** Reported, never adopted as units: adopting them would let the scanner
rewrite the architecture it is checked against."* And `TICKET_BP15_ARCHITECT.md`'s CONTRACT_GAP entry
(~L336–338): *"Whether those become numbered units is a founder call, per this ticket's own
out-of-scope line."*

**Blocked until answered.**
- **Nothing mechanically.** The scanner reports them and exits 0; §3 is *self*-consistent and the
  self-check PASSES at 43 + 1 = 44. This was predicted to be a count mismatch and is not one — it is
  *disk* disagreeing with the doc, which the scanner deliberately reports as a separate output.
- **But §3's unit budget is wrong as documentation** until it is answered, and §3 is the manifest
  `architect.py` parses. It is also the premise `docs/ASSIGNMENT-5.md` opens with ("the full list of
  44"), which is why it is worth clearing this week even though it blocks nothing.

**Options.**
- **(a) Declare all four**, taking the in-slice count 43 → 47. Honest; the scanner's self-check
  verifies the arithmetic in one run so the risk of getting it wrong is near zero.
- **(b) Declare some, exclude others.** `BRUITypes` is a types header and `BRCombatCurves` is a
  curve-access helper — both are arguably *libraries* like the seven `UBRGE_*` classes, which §4
  already excludes by name. `BRRootLayout` and `BRUISettings` look like real units.
- **(c) Declare none**, and instead add them to §4's named-exclusion list with a reason, the same
  device the GE library uses.

**Recommendation — (b), with the split written down.** Reason: §4 already has a working mechanism for
"real C++ that is not a numbered unit" — exclusion **by class name, with the reason stated** — and it
was corrected once already this week precisely because a bare number invites the next reader to
correct it back. `BRRootLayout` and `BRUISettings` are units (a layout and a settings object are
things the architecture should name); `BRUITypes` and `BRCombatCurves` are a types header and an
accessor and belong on the exclusion list. Whichever split is chosen, the scanner asserts it, so a
wrong guess fails loudly on the next run rather than drifting.

---

## D12 — Install the `Stop` hook for the session bus · call · **needs explicit consent**

**Question.** Install a `Stop` hook (`.claude/hooks/bus_notify.py`, registered in
`.claude/settings.json`) that fires when a session tries to finish and hands it the next relevant bus
message.

**Filed.** `Tools/bus/bus.py` docstring L13–14: *"the `Stop` hook in `.claude/hooks/bus_notify.py` is
what turns the mailbox into 'never stop working' — it fires when a session tries to finish and hands
it the next relevant message."* Exit code 2 is documented as *"nothing to do (useful for the Stop
hook)"*.

**Verified today, and it sharpens the question.** **The hook does not exist.**
`.claude/hooks/` contains `guard_laws.py` and `test_guard_laws.py` only, and `.claude/settings.json`
registers a single `PreToolUse` matcher. So `bus.py`'s own docstring **asserts a mechanism that is
not installed** — which is, precisely and for the fifth time this week, *a rule that reads as
enforced and is not*. That is worth fixing whichever way the decision goes.

**Blocked until answered.** The bus works today as a mailbox (`post` / `inbox` / `claim` / `done`),
and the sessions have used it successfully — six messages, four resolved. What is missing is the
*attention* half: a session can stop with high-priority work sitting in its inbox, which is the exact
failure T2's own routing-bug review named (*"the Stop hook lets the session stop with work waiting.
That is the exact failure the bus exists to prevent, and it fails silently on the passing side."*).

**Options.**
- **(a) Install it.** Requires editing `.claude/settings.json`, which changes harness behaviour for
  every session in this repo — hence the consent requirement, and it should be the founder's own
  action, not an agent's.
- **(b) Do not install it, and correct `bus.py`'s docstring** to describe the bus as a mailbox that
  is polled by discipline (`bus.py inbox` at session start and before close), not woken by a hook.
- **(c) Install it, but non-blocking** — advisory output only, never preventing a stop.

**Recommendation — (c) if installing at all, and (b) unconditionally, today.** Reason: (b) costs one
paragraph and removes a false claim from a tool's own documentation, which is this project's most
frequently-recurring defect class and is worth closing regardless. On (a)/(c): a hook that can
prevent a session ending is a hook that can trap a session on a message it cannot action, and the bus
routes by *mode* — a mismatch there (the bug T2 already found once) would strand a terminal. Advisory
first; make it blocking only after the routing has been correct for a few sessions. **This is the one
item in the register that needs the founder's own hands** — an agent should not install a hook that
governs agents.

---

## D13 — A standing rule for Windows/`Tools/` platform defects · RULING *if adopted*

**Question.** Do the recurring Windows platform defects in `Tools/` justify a standing rule, or do
they stay individual Log entries?

**Filed.** Across four files rather than one, which is the argument:
- `Tools/data-crew/run_crew.py:51–58` — cp1252 `UnicodeEncodeError` on the first job banner; the
  whole replay exited 1 on the machine that runs steps 2–5 (BP14 Log, 1 Aug).
- `Tools/architect/build_state.py:326–331` — cp1252 again on `—`/`·`/`∪` via `--stdout`; fixed by
  writing bytes. Its own comment: *"Same platform defect as `Tools/data-crew` (BP14 Log, 1 Aug 2026)."*
- `Tools/architect/architect.py:213–234` — `rglob("BR*.h")` is **case-insensitive on Windows**, so it
  also returns the kept UE template's `breachpointCharacter.h` and *"would be reported as
  architecture drift on Windows and not on Linux. **The same class of platform defect** as the cp1252
  one."*
- Two more of the same family: `Tools/rename_r26/rename_r26.py:68–72` (a case-only rename needs a
  two-step because the filesystem is case-insensitive) and BP15's F5 (**a failed traversal exploit on
  Windows was not evidence the exploit was impossible** — Win32 strips trailing dots; one extra
  character restored it).

**Blocked until answered.** Nothing. But the pattern is already being copied by hand:
`Tools/validate_data_tags/validate_data_tags.py:89–94` pins the encoding pre-emptively with the
comment *"Windows has bitten this repo twice."* It has now bitten more than twice, in more than one
way, and the next author has to rediscover it.

**Correction to the founder's framing.** It is not three defects and it is not one defect class —
it is **one root cause with three surfaces**: `Tools/` is the only place where the cloud box and the
Windows workstation run the *same file* and **nothing compiles it**, so every platform assumption is
inherited rather than pinned. Encoding, path case, and path canonicalisation are three faces of it.

**Options.**
- **(a) One standing rule in the ledger**, three clauses: pin `stdout`/`stderr` to UTF-8 at import in
  every `Tools/**.py`; never rely on glob/`rglob` case sensitivity — test the prefix explicitly; a
  path-safety check that passes on one platform is not evidence for the other. Reviewable, greppable,
  and the critic can judge against it.
- **(b) A shared `Tools/_platform.py` preamble** imported by every tool. Mechanism instead of
  doctrine — but it is a new import every tool must remember, so it has the same adoption problem
  with an extra file.
- **(c) A rung-2 or pre-commit check** that greps `Tools/**.py` for an unpinned stream. Strongest;
  costs a check nobody has written, and rung 2 has no spec files running yet.
- **(d) Keep filing Log entries.** Free, and demonstrably does not stop the fourth occurrence.

**Recommendation — (a) now, (c) when rung 2 is alive.** Reason: this board's own operating lesson is
*"an enforcement mechanism proves nothing until it is tested with a case it should REJECT"* — so (c)
is the real answer and it should be built. But (a) is available today, costs one ledger entry, and is
what makes (c) reviewable when it arrives. Also add BP14's existing informal check as a fourth
clause: *"any `Tools/**.py` box claimed from the cloud gets one re-run on Windows before it is
checked"* — it is currently written in BP14's Log as *"not filed as a new rule because it costs
nothing to just do"*, and it has since been the thing that would have caught two of these.

---

## D14 — `.mcp.json`: it is **already committed** · call

**Correction to the founder's list, verified today.** The question was framed as *commit or
gitignore*. Neither is open — **`.mcp.json` is tracked and committed** (`git ls-files` returns it;
working tree clean; it landed in `51b97b3`, *"BP16 step 1: the MCP surface, enumerated against a live
editor"*). It contains one loopback entry:
`{"mcpServers":{"unreal-mcp":{"type":"http","url":"http://127.0.0.1:8000/mcp"}}}`.

**What is actually still open**, and it is a different and better question:

**Question.** Is committing it endorsed, and does anything guard it?

**Filed.** `TICKET_BP16_UE_MCP_BRIDGE.md` Log — *"`.mcp.json` at the repo root is **NOT** [inside
`owner_path`] — it was written on direct founder instruction with no claim file active (so
`guard_laws.py` was inert and did not fire). Recorded rather than quietly done: if the lead wants
`owner_path` to cover it, that is a one-line ticket edit, not a precedent."* And the step-2 proposal's
Finding 1: *"the matcher's key would live in `.mcp.json` at the repo working root, which this
ticket's own Log records as outside every `owner_path` and which no hook guards. **A gate whose key
is a one-line edit to an unguarded file is not a gate.**"*

**Blocked until answered.** Nothing today. It becomes load-bearing **the moment D4 lands and anyone
builds an argument-parsing MCP gate**, because the server key in this file is what such a gate keys
off.

**Options.** (a) Endorse the commit and add `.mcp.json` to BP16's `owner_path` — one line, and it
stops being writable by a claim that has no business touching it. (b) Endorse and leave unguarded,
accepting that a future gate rests on an unguarded key. (c) Gitignore it and have each machine
generate its own — costs the next machine a step and loses the only committed record of the endpoint.

**Recommendation — (a).** Reason: it points at loopback so committing is harmless and saves the next
machine a step (that part of the original reasoning holds). The addition is free and it removes the
one structural objection Finding 1 raised against the whole mechanism path. Add it to
`docs/contracts/` guardianship in the same edit as D4 step 3.

---

## D15 — `docs/bus/`: commit or gitignore · call

**Question.** Does the session bus's message directory enter git history, or is it ignored?

**Filed.** `docs/bus/20260801T161116Z--T2-to-T1--bus-py-routing-bug…md` (the routing bug itself is
`status: done`; this rider is not): *"**`docs/bus/` has no gitignore decision.** Either it commits
(and every message enters history forever) or it is ignored (and messages vanish on a fresh clone).
Neither is wrong, but per the project's own law — a decision that lives only in chat is lost — the
bus MUST terminate in ticket Logs either way. **Suggest:** gitignore `docs/bus/`, and make
`done --result` the prompt to write the outcome into the ticket Log."*

**Blocked until answered.** Nothing. Six messages exist. Decide before the directory is large enough
that the answer has consequences.

**Options.** (a) Gitignore it; the bus is session-scoped scratch and outcomes land in ticket Logs.
(b) Commit it; the messages are a genuine record of cross-session reasoning — and reading them today,
several contain findings that are *not yet* in any ticket Log (T2's ~80-allowed-roots evidence, the
two concrete reimport risks, the `CT_Combat.csv` single-key observation). (c) Gitignore, but require
`done --result` to write the outcome into a ticket Log before a message may be closed.

**Recommendation — (c).** Reason: (a) as stated would lose real findings, because the discipline it
depends on is not yet in place — the evidence is that today's bus traffic contains at least four
findings with no ticket home. (c) is (a) plus the missing half, and it matches the precedent set by
`Tools/data-crew/output/`, which is gitignored precisely so there is one source of truth rather than
a second drifting copy. **Do not adopt (a) until the `done --result` → Log discipline is real**, or
this is a decision that deletes evidence.

---

## D16–D19 — smaller calls, little or nothing blocked

| # | Question | Filed | Blocks | Options | Recommendation |
|---|---|---|---|---|---|
| **D16** | `UGameplayEffect::StackingType` is `UE_DEPRECATED(5.7)` and its setter is `WITH_EDITOR`-only — **there is no non-deprecated way for a native GE class to declare stacking in a packaged build.** A crack in R18. | BP02 Log ~L383–388: *"Someone should decide before 5.9."* | Nothing today — `GE_RecentDamage` writes it inside a `PRAGMA_DISABLE_DEPRECATION_WARNINGS` scope *so it fails to compile loudly when Epic privatises it* | (a) hold the pragma and revisit at 5.9 · (b) narrow R18 to permit one GE **asset** where stacking is required · (c) implement stacking ourselves in the ExecCalc | **(a).** The pragma is a correctly-armed tripwire; it converts a silent future loss into a compile error. Revisit when 5.9 is real, not before. |
| **D17** | `OnlineSubsystemSteam` is enabled as a **plugin** but not linked as a **module**. §3 reads "OnlineSubsystem(+Steam)". Which was intended? | BP01 Log ~L408–413: *"If a hard link was intended, it is one line — **someone should confirm the intent before BP11.**"* | BP11's Kickoff (and D9 sits next to it) | (a) confirm plugin-enable is correct — Steam is selected at runtime by `DefaultPlatformService` · (b) hard-link the module | **(a), and write it into §3** so the ambiguity does not recur. The builder's reading is the standard UE arrangement; only the doc is ambiguous. |
| **D18** | The **sprint bit is trusted**: a modified client can assert it and move faster without activating `BRGA_Sprint`. | BP02 Log ~L405–414. Names its own decider: *"**netcode-builder rules on this.**"* | Nothing today; it is bounded and is the standard UE arrangement (`bWantsToCrouch` has the same property). Closing it is one function — `IsSprintIntentValid()` exists for it | (a) dispatch netcode-builder to close it in `IsSprintIntentValid()` · (b) accept it and record it in the risk register | **(a), but not this week.** It is delegated, not founder-owned — the founder's only call here is *when*. It should not precede D5, which blocks rung 1 for everything. |
| **D19** | Two BP01 builder's-call items nobody ratified: the **three collision channels** (§3.1 requires aliases *"matching `DefaultEngine.ini`"* but enumerates none), and `SetByCaller.*` **FName-keyed vs tag-keyed** magnitudes. | BP01 Log ~L579–584; BP02 Log ~L402–404 | Possibly BP08 (a fourth channel would need `Core/`, which is closed). Otherwise nothing | Channels: (a) ratify the three · (b) enumerate a fourth pre-emptively for AI. Keying: (a) keep FName · (b) tag-keyed, adding four §3.1 leaves | **Ratify both as-is.** FName-keyed SetByCaller is first-class engine API with no invented tags, which is the better default. On channels: ratify the three **and** record that a fourth needs a `contract_gap`, so BP08 meets a rule rather than a silence. |

---

## Closed — do **not** re-open (checked against the ledger, 1 Aug 2026)

Two items on the founder's starting list are already ruled. Recorded here so the register does not
send them round again, per law 8.

**§7's window claims vs. a lock-mode file — RULED by R31, and it took option (a).**
`WORK-ROUTING.md` §7 still reads *"Two candidate resolutions, **for the founder, not for a lead to
pick**"* — **that text is stale.** `docs/DESIGN-RULINGS.md` **R31** (founder ruling, 1 Aug 2026)
resolved it: *"A claim may name a SET of tickets sharing one window. `owner_path` is their union, and
amendments are ADDITIVE ONLY. … §7 offered two candidates; this takes **(a) window claims**."* It is
live and in use — the current `.claude/active-packet.json` names `["BP15","BP02","BP16"]`.
*What is genuinely owed here is a two-line doc fix, not a decision:* §7 should be marked RESOLVED
BY R31 with a pointer, and §5.6's *"a claim is per session"* is contradicted by R31's own finding
(*"It is not — it is per tree, last-writer-wins"*) and should be corrected. **One live watch item,
not a decision:** R31 names option (b) — a separate lock-holder file — as the standing fallback *"if
that trade later proves wrong"*, because a union deliberately **weakens** confinement. Nothing has
gone wrong yet; if it does, the fallback is already chosen and needs no new ruling.

**`Source/Breachpoint/Tests/` ownership — RULED by R25.** BP03's Log escalates it at length as
*"the single largest founder decision on the board"* and BP02/BP05/BP06 all cross-reference it.
**R25 settled it** and took the equivalent of BP03's option (a): *one spec file per feature packet,
named `BR<Feature>Spec.cpp`, taken by exact path, never the folder* — and R25 explicitly amends
ARCHITECTURE §3.12, which was BP03's stated reason for calling it an architecture change. It is being
executed right now: T2 dispatched two sim-builders this session for `BRCombatSpec.cpp` and
`BRShieldSpec.cpp`, each taking its file by exact path. **The cross-references in BP02/BP03/BP05/BP06
are stale and should point at R25** — a doc fix, not a decision.

---

## What this register deliberately does not contain

For honesty about the sweep's edges, three categories were found and **excluded**:

1. **Unfinished work with a known owner** — unchecked Done-when boxes, BP01's owed clean witnessed
   rung-1 pass, BP13 step 6's reimport, `Tools/audit_blueprints/` being unrun. These are *work*, not
   *decisions*; they are already tracked in their tickets and in `WORK-ROUTING.md` §4/§6.
2. **Environment repair** — the engine's C# libs failing NuGet/SDK resolution, which blocks all of
   rung 4. `WORK-ROUTING.md` §6 calls it *"workstation repair, not a packet"* and it needs an hour,
   not a decision.
3. **Human judgment gates** — BP05's M2 fun gate, BP08's M4 go/no-go, BP10's blindfold footstep test.
   These are decisions the founder owes, but they cannot be answered from a desk; they need a build
   that runs, and nothing on this board has reached PIE.

---

## Verification addendum — 1 Aug 2026, checked against DISK rather than against the Logs

This register was assembled by reading ticket Logs. **Every item was then re-checked against the
working tree, and D5 does not survive that check.** Recorded here rather than silently edited,
because the way it was wrong is the more useful finding.

**D5 is STALE ON BOTH HALVES. It is not a decision the founder owes.**

| D5's claim | On disk, 1 Aug |
|---|---|
| `"SlateCore"` missing from `Breachpoint.Build.cs` | **PRESENT**, `Breachpoint.Build.cs:32`, with a dated comment explaining that UMG's `SObjectWidget` pulls `SWidget`/`EVisibility`/`SNullWidget` from SlateCore, not Slate |
| a case-only UHT collision between `FBRKillfeedEntry` and `FBRKillFeedEntry` | **RESOLVED**, commit `890230c` *"fix: UHT name collision between BP04 and BP10 killfeed structs"*. Disk holds `FBRKillFeedEntry` (Match, 14 refs) and `FBRKillfeedViewEntry` (UI, 15 refs). Lowercased these are `fbrkillfeedentry` and `fbrkillfeedviewentry` — **distinct**, so UHT's case-insensitive comparison does not fire |

**Why this happened, and why it is worth a section.** BP02's Log says the two fixes *"were applied
locally… neither was applied to the repo,"* and that sentence is still true **of BP02**. Another
packet landed both afterwards and BP02's Log was never updated — a Log records what a packet did,
not what the repo now is. **A register built from Logs inherits every Log's expiry date.** This is
the same failure this session hit four other times: the stale `Ability.*` comment that misrouted a
`contract_gap` to BP01, `ARCHITECTURE §4`'s "six GE classes", `WORK-ROUTING §7` being written after
**R31** had already ruled it, and `bus.py`'s docstring asserting a Stop hook that does not exist.
**The document that a reader hits first wins, and it is usually the stale one.**

*Standing rule this suggests, filed as D20 rather than applied:* an item in this register carries
a **disk-verified** marker or it does not belong here. The register's value is that a founder can
clear it in one sitting; an item that was fixed days ago costs exactly the attention the register
was built to save.

**What is NOT resolved, and must not be read as resolved.** Both fixes being present on disk is
**not** evidence that the module compiles. **No rung-1 pass has been run at current HEAD** — a UE
editor is live (the MCP session) and R29.3 forbids the build. The honest statement is: *the two
known blockers are gone from the tree, and whether rung 1 is green is unknown.* First item in the
CLOSED batch when T2 releases the editor (`WORK-ROUTING §4.1`) answers it.

*Unchanged by this addendum:* D1 (depth sign), D2 (readiness term), D3 (F3 include-graph
demotion) — the three the course deliverable currently answers with "this is the project owner's
call" — and D6/D7, which were verified as still open.
