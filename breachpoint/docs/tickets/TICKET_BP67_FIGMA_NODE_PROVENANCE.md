# TICKET — Resolve every recorded Figma node id against its file key

> STATUS: open — cut by builder (front-end provenance pass) 2 Aug 2026. Needs a session with the
> **Figma MCP available** (`get_metadata`, read-only). The three docs' *headers* are corrected;
> the **per-id sweep is not done** and is this ticket's job.

The repo records ~83 Figma node ids across the front-end docs and, until today, attributed all of
them to Breachpoint's own working file. They are not in it. `1:2` — the `Play` frame every grid
number in `COMPONENT-SPECS.md` §6 is measured from — returns *"the provided node ID was not
found"* against `yznvnVdOFDADaugZSeomfP` and resolves cleanly in the reference community file
`Kn87U5sy2VD0lP8K7h4LcQ`. **The measurements are sound; the file attribution was wrong.** An id
without its file key is not a citation, it is a coincidence — that is what this ticket closes for
good. Founder law that binds: nothing here invents a mapping. An id that resolves in **neither**
file is recorded as unresolved and re-measured, never guessed.

Boundary, unchanged: **geometry and node ids only.** No art, no image fills, no Halo strings
(`ui-presentation` §8, `UI-REFERENCE.md`).

**Ordering law:** Step 1 (the sweep) gates Steps 2–4. Nothing may be re-attributed, deleted or
re-measured before the sweep says which bucket each id is in.

## Kickoff (machine-checkable — the tickets skill verifies these BEFORE a claim)

- requires: files-only — **plus the Figma MCP `get_metadata` tool must be callable in the
  session.** This is the gate that failed on 2 Aug: the thread that cut this ticket had the two
  file keys and the doc set but no Figma tool, so it could correct the headers and could not run
  the sweep. Verify with one throwaway call before claiming.
- `docs/ui/REFERENCE-EXTRACTION.md`, `docs/ui/COMPONENT-SPECS.md` and
  `docs/ui/ue-frontend/SCREEN-MANIFEST.md` §4 each carry a "Node-id provenance" header naming
  `Kn87U5sy2VD0lP8K7h4LcQ` (landed 2 Aug 2026 — grep `Node-id provenance`, expect 3 hits).
- owner_path: `docs/ui/`, `docs/tickets/`

## Steps (in order)

1. **The sweep — builder.** For **every** node id in `REFERENCE-EXTRACTION.md` §4 + §5 and
   `SCREEN-MANIFEST.md` §4 (~83 distinct ids), call `get_metadata` against **both** file keys and
   bucket the result: `REF` (reference file only) · `BP` (working file only — a genuinely
   Breachpoint-authored frame) · `BOTH` · `NEITHER`. Record id, node name, bucket, and the
   resolved x/y/w/h in a table appended to this ticket's Log. Start with the 22 below (≥1 per
   wave, the sample this ticket was cut from) so a partial run is still evidence:
   - Wave 1 — `1:2` `266:1762` `572:10452` `98:763`
   - Wave 2 — `2058:28286` `933:8346` `927:43283` `1414:15140`
   - Wave 3 — `208:1603` `317:2434` `2095:28981`
   - Wave 4 — `934:9674` `3606:39204` `1862:25791`
   - Wave 5 — `276:2013` `2371:97304` `581:4459` `1548:19329`
   - Wave 6 — `1031:13111`
   - Wave 0 / overlays — `2387:32475` `707:5770`
   - Dropped — `36:0`
2. **Re-attribute per table, not per document — builder.** Where a table is uniformly `REF`, its
   existing one-line header is already correct and nothing else changes. Where any id is `BP`,
   annotate **that row** with its file key. **Do not renumber, do not delete a measurement, do not
   restructure a document.**
3. **The `NEITHER` bucket is the real finding — builder.** Every id in it is a lost citation: the
   geometry beside it was measured off *something*, and nobody can now say what. Mark each row
   `NODE LOST — re-measure`, and re-measure by **name** (`get_metadata` on the page, match the
   frame name) rather than by id. A number whose node cannot be recovered is downgraded to
   UNMEASURED and joins `SCREEN-MANIFEST.md` §9 — it is **not** deleted and **not** trusted.
   Suspect first: `2371:97304` (`Map or Playlist Feature`). Its siblings in the same authoring
   run are `2377:28958` / `2377:29265`; a 5-digit `97304` next to those is far more likely a
   transcription slip than a real id. Hypothesis, not a claim — the sweep decides.
4. **Fix `SCREEN-BUILD-SPEC.md` §1 — builder** (`docs/ui/` owner path; the thread that cut this
   ticket did not own that file). Line 18 reads:
   ```
   Nav Bar              44,45  666x30     root level
   ```
   The root nav bar is **`33,45  666x30`** — live node `124:1179` in `Kn87U5sy2VD0lP8K7h4LcQ`,
   confirming `COMPONENT-SPECS.md` §6. `SCREEN-MANIFEST.md` §9 question #1 is already marked
   RESOLVED on this evidence and **18 screens inherit the number**, so this correction is the
   last place `44` survives for the root bar. Line 19's sub-level `44,75 / 44,110  516x30` is a
   **different** component and is *not* corrected by this — verify it separately in Step 1 and
   leave it alone if it holds. Worth recording why the error happened: `SCREEN-BUILD-SPEC.md`'s
   header says it was produced by a pass over the *pasted* pages in `yznvnVdOFDADaugZSeomfP`,
   whereas `COMPONENT-SPECS.md` read the reference file — two files, one number, no file key
   written down. That is the whole bug class in one line.
5. **The standing rule — builder, in `docs/ui/` and in the ui-presentation skill's self-check.**
   *Every recorded Figma node id carries its file key.* Format: `` `Kn87U5s…:1:2` `` inline, or a
   one-line "node ids in this table are `<fileKey>`" header per table (the latter is preferred —
   it is the smaller diff and it survives a row being copied out). A bare node id in a doc, a
   ticket or a commit message is a **finding**, not a style note.
6. **Verification — verifier.** Rung 0 (docs): re-run 5 ids picked at random from the sweep table
   and confirm the recorded bucket. No engine rung applies; this ticket ships no code.

## Done when

- [ ] Every node id in the three docs has a bucket (`REF` / `BP` / `BOTH` / `NEITHER`) recorded in
      the Log, with the count per bucket stated
- [ ] `SCREEN-BUILD-SPEC.md` §1 root nav bar reads `33,45  666x30`
- [ ] Every `NEITHER` id is either re-measured by name or downgraded to UNMEASURED in
      `SCREEN-MANIFEST.md` §9 — zero silently-kept numbers
- [ ] The standing "id carries its file key" rule is written where a future session will hit it
- [ ] Findings + decisions written to this ticket's Log

## Notes

- Crew: builder does 1–5 (all inside `docs/`); verifier does 6. No netcode, sim, or UI-widget
  work is in scope — this is a provenance ticket.
- Binary files this ticket OWNS (lock before editing): **none.** Do not open a `.uasset`, do not
  run a commandlet, do not touch Figma in write mode — `get_metadata` only.
- Out of scope: renumbering ids, deleting measurements, restructuring any of the four documents,
  re-litigating geometry that already resolves (`COMPONENT-SPECS.md` §6 is measured and stands),
  and editing the reference file. These are heavily-cited documents; a large diff makes the
  correction unreviewable, which is how the original error survived.

## Log

**2 Aug 2026 — builder (front-end provenance pass). Cut this ticket; corrected the headers only.**

Evidence in hand (3 probes, all `get_metadata`, read-only):

| # | File key | Node | Result |
|---|---|---|---|
| 1 | `yznvnVdOFDADaugZSeomfP` (Breachpoint working) | `1:2` | **NOT FOUND** — *"The provided node ID was not found in the file."* |
| 2 | `Kn87U5sy2VD0lP8K7h4LcQ` (reference) | `1:2` | **FOUND** — `Play`, 1280×720; `Menu Combo` (69,138) 349×510 · `Party List` (862,397) 349×273 · `Navigation Bar` (33,45) 666×30 |
| 3 | `Kn87U5sy2VD0lP8K7h4LcQ` (reference) | `124:1179` | **FOUND** — `Navigation Bar`, **x=33, y=45, 666×30** |

Probe 2 reproduces `COMPONENT-SPECS.md` §6 **exactly**, on all three elements. That is what
establishes the diagnosis: the measurements are sound and only the attribution was wrong. Probes
1–3 were run by the dispatching thread; the thread that wrote this ticket had **no Figma tool**
(`get_metadata` not callable — project `.mcp.json` carries `unreal-mcp` only) and therefore
**could not extend the sample past these three**. Honesty rung: this is a 3-id sample used to
correct a ~83-id attribution. It is enough to prove the *diagnosis* — `1:2`, the id every §6
number hangs off, is not in the working file — and it is **not** enough to prove any *individual*
other id's bucket. Step 1 exists because of that gap. Nothing was re-attributed row-by-row.

Changed today (headers only, no measurement touched, no id renumbered):

- `docs/ui/REFERENCE-EXTRACTION.md` — replaced the single `Target file: … yznvnVdOFDADaugZSeomfP`
  line with a "Node-id provenance" block naming `Kn87U5sy2VD0lP8K7h4LcQ` for §4/§5 ids and
  keeping the working file named as the clone host. The old line contradicted §1 of the same
  document, which already called the community file "**The only measured source.**"
- `docs/ui/COMPONENT-SPECS.md` — same fix. This one was self-contradictory on its face: line 4
  states the numbers were read *"via the Figma Plugin API (read-only against
  `Kn87U5sy2VD0lP8K7h4LcQ`)"* and line 8 then named `yznvnVdOFDADaugZSeomfP` as the target file.
- `docs/ui/ue-frontend/SCREEN-MANIFEST.md` — provenance blockquote at the head of §4 (covers all
  §4.1–§4.9 tables at once); §1 "Grid conflict" block closed; §6.1's `x=33 vs 44 CONFLICT` marker
  replaced with the confirming node; §9 question #1 marked **RESOLVED → x=33**, citing `124:1179`
  and noting 18 screens inherit it.

Open question count in `SCREEN-MANIFEST.md` §9: **13**, not 14 (#1 closed). The section heading
still says 14 — left alone deliberately as a numbering change nobody asked for; fold it into the
next edit of that section.
