# TICKET — Package a Windows build and publish the playable link

> STATUS: open, CRITICAL PATH — cut by the cloud lead 1 Sep 2026 for assignment #10, whose
> deadline is 1 Sep 11:59 PM ET. Needs a **Windows PC with UE 5.8**. Without this link the
> assignment is capped at 50% no matter how good everything else is.

Founder directive: assignment #10 gates on a playable link — *"a stranger must be able to
open this link and play your game within 2 minutes without setup instructions."* macOS
cannot cross-compile a Windows build, and UE5 has no browser target, so this is a Windows
packaging job. Ship a downloadable build on itch.io and put its URL in the submission.

**Ordering law:** step 1 (pull) gates everything — the config changes that make the build
bootable landed on main today and a stale clone packages the wrong map. Step 4 (smoke test)
gates the upload; an unplayable link scores worse than an honest absence.

## Kickoff

- requires: **a Windows machine with UE 5.8 installed and ~60 GB free**
- Content is committed (5,526 `.uasset`, 10 `.umap`), so a fresh clone is sufficient — no
  LFS, no external asset fetch
- owner_path: `Config/` `docs/tickets/` — do not change gameplay code to fix a package error

## Why this can work at all (the facts, so you don't rediscover them at 2 AM)

1. **The Game target compiles.** BN24 proved `Editor+Game` clean on 28 Aug. Only
   `BreachpointServer` is blocked (BN38), and packaging a client build does not need it.
2. **The boot map was wrong until today.** `GameDefaultMap` pointed at `BR_Arena01`, which
   has no WorldSettings GameMode override and therefore fell through to the OLD module's
   `ABPGameMode`. A build made yesterday would have booted into something that is not the
   game. It now points at **`BR_Spillway`**, where assignment #9's probe ran a full match
   with bots live. **This is the single most important reason to pull before building.**
3. **The bot names in the build are pipeline-generated** (assignment #10's whole thesis).
   They live in `Config/DefaultGame.ini`, baked in at cook time — so the cook must happen
   after the pull, or the scoreboard shows the old hand-typed names and the submission's
   central claim is false on screen.

## Steps (in order)

1. **Pull.** `git clone https://github.com/JDMAXilius/Multi-agents-class.git` (or `git pull`)
   — must include today's commits.
2. **Verify the two config facts before spending 40 minutes on a cook:**
   ```
   findstr GameDefaultMap breachpoint\Config\DefaultEngine.ini      :: expect BR_Spillway
   python assignments\10-ai-dev-pipeline\land_in_engine.py --check  :: expect OK, 15 names
   ```
3. **Generate + build.** Right-click `breachpoint/Breachpoint.uproject` → *Generate Visual
   Studio project files*, then open the editor once so it compiles the Game target.
4. **Package.** Editor → *Platforms → Windows → Package Project* (Shipping if it cooks
   clean, Development if Shipping fights you — say which in the Log). Equivalent CLI:
   ```
   "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun ^
     -project="%CD%\breachpoint\Breachpoint.uproject" -noP4 -platform=Win64 ^
     -clientconfig=Development -cook -allmaps -build -stage -pak -archive ^
     -archivedirectory="%CD%\Export\Win64"
   ```
5. **SMOKE TEST THE PACKAGED BUILD — not the editor.** Run the `.exe` from the archive
   folder on a machine that has never opened the project. Confirm, in this order:
   - it boots into an arena and you can move and shoot within ~30 seconds of launch
   - **bots are present and their names are the pipeline callsigns** — `Dulledge`,
     `Softaim`, `Deadeye`, etc. on the scoreboard. Screenshot this; it is the evidence for
     the Pipeline-to-Game Connection criterion and for the run video.
   - if the names read `Marcus`/`Vale`/`Ossian`, the cook predates the pull — go to step 1.
6. **Publish.** itch.io → new project → kind **Downloadable**, upload the zipped archive,
   set it **Public** (not draft — a draft link 404s for the grader, which reads as a broken
   link and triggers the 50% cap). Confirm the page loads in a private browser window.
7. **Record the link and the caveats** in `assignments/10-ai-dev-pipeline/README.md`
   (Deliverable 1) and write what happened into this ticket's Log.

## Done when

- [ ] Packaged Win64 build exists and runs on a machine that has never opened the project
- [ ] Bots in the packaged build carry the pipeline callsigns (screenshot captured)
- [ ] itch.io page is public and downloads successfully in a private window
- [ ] Link recorded in the assignment README; caveats stated honestly
- [ ] Findings + decisions written to this ticket's Log

## If it goes wrong

| Symptom | Likely cause / move |
|---|---|
| Cook fails on a missing asset | `Content/Reference/*` is gitignored and set `DirectoriesToNeverCook`; if something else references it, exclude that map rather than committing reference art. |
| Build boots to a black screen or empty level | `GameDefaultMap` did not take — check step 2. |
| Bots absent | `TargetPlayers=8` and `BotSystem=AIB` in `DefaultGame.ini`; if the AIB path misbehaves in a cooked build, flip `BotSystem=BN` and note it in the Log. |
| Shipping config fails, Development works | Ship Development. Say so in the audit. It is a real build either way. |
| No time to finish | STOP and say so — submit without the link and take the cap. A broken link scores the same as no link and costs credibility on top. |

## Notes

- Out of scope: fixing gameplay bugs the smoke test finds (file them; the deadline owns
  tonight), the dedicated-server target (BN38), and the spotter lines (AUDIT.md §1b says
  why they are deliberately not landed).
- Honesty: the link is a **packaged Windows client**, not a browser build and not a
  multiplayer claim. Say "Windows download, single player vs bots" in the submission — the
  rubric's 2-minute test is about friction, and an honest description of what the download
  is beats an implication it cannot support.

## Log

### 3 Sep (cloud) — the BUILD is done; the SUBMISSION is not. Two fields and an upload.

Commit `8f442386` closes every technical step this ticket asked for, verified on the
packaged `.exe` rather than the editor: boots to `FE_MainMenu` on `BNFrontEndGameMode`,
PLAY reaches `BR_Spillway`, the match runs and returns to the menu, and the bots carry the
pipeline callsigns (Dulledge, Evenkeel, Midpace, Shakygrip, Slowdraw, Softaim, Wideshot)
rather than the old hand-typed names — which is the Pipeline-to-Game Connection criterion
demonstrated, on the artifact that gets graded.

Steps 1–5 of this ticket are therefore DONE. **Steps 6 and 7 are not, and they are the two
that the grade actually reads:**

- [ ] **6. Publish.** itch.io → new project → kind **Downloadable** → upload the zipped
      archive → set it **Public**, not draft (a draft link 404s for the grader, which reads
      as a broken link and triggers the same cap as no link at all). Confirm it downloads
      in a private browser window.
- [ ] **7. Record it.** `assignments/10-ai-dev-pipeline/README.md` still reads `PENDING` at
      **line 19** (Deliverable 1, the playable link) and **line 39** (the pipeline run
      video). Both are graded fields.

Neither needs an engine or a build — they need a browser and a screen recording, and they
are the whole distance between a working game and a submitted one.

**Two things worth carrying into the run video**, both now true and both cheap to show:
the scoreboard with the generated callsigns on it (that single frame IS the pipeline
claim), and the menu → match → menu loop (the thing a stranger does in the first two
minutes). The honest caption for the link stays what the README already says: a packaged
**Windows client, single player against bots** — not a browser build and not a multiplayer
claim.

**One caveat to state in the submission, because it is true and cheap to say:** the gamepad
layout is generated and audited (77/77) but was never held — the smoke runs registered
KBM only. Keyboard and mouse are proven; the pad is not.

