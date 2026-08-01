---
name: ue-editor
description: UE 5.8 editor automation for BREACHPOINT — Python Editor Script Plugin, commandlets, Remote Control, and the rules for any Unreal MCP server. Load for ANY task that must touch the editor from outside it - executing arena_manifest.json into a blockout, reimporting DataTables, taking review screenshots, or evaluating/driving a UE MCP. Doctrine: generated scripts over live editing; the manifest is the source of truth, the editor state is its projection.
---

# UE Editor Automation — the bridge, done safely

**The doctrine (ruling-grade, from the binary-asset law):** prefer a **generated,
committed Python script** over live interactive editing — by hand or by MCP. A script is
reviewable text, re-runnable by the verifier, and version-controlled; a stream of live
editor mutations to a binary `.umap` is none of those. The manifest/CSV is the source of
truth; the editor state is its **projection**. Regenerate the projection; never let the
projection drift ahead of the source.

## 1. The surface (all first-party, UE 5.8)

- **Python Editor Script Plugin** (+ Editor Scripting Utilities) — enable once in the
  project; unchanged API family across 5.x.
- Headless execution (the crew's default — no human at an editor):
  `UnrealEditor-Cmd Breachpoint.uproject -run=pythonscript -script="Tools/py/<script>.py" -stdout -unattended -nosplash`
- In-editor: `py <script>` in the console, or Output Log's Python REPL, for spikes only.
- **Remote Control API** (WebControl plugin, HTTP :30010) — for poking a RUNNING editor
  (set a property, call a function). Useful for iteration; never the landing mechanism.
- Commandlets you already own: `Tools/reimport-tables.ps1` (DataTable CSV reimport),
  `Tools/run-*.ps1` (the ladder). Automation prefers these wrappers over raw flags.

## 2. Core Python patterns (5.x subsystem style — not the deprecated globals)

```python
import unreal
actor_ss  = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
level_ss  = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
asset_ss  = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)

cube = unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/Cube")
a = actor_ss.spawn_actor_from_object(cube, unreal.Vector(x*100, y*100, z*100))  # m → uu
a.set_actor_scale3d(unreal.Vector(sx, sy, sz))
a.set_actor_label("Cover_TheBar_01")          # names ARE callouts — label everything
level_ss.save_current_level()                  # explicit save, never rely on prompts
```
- **Units:** the manifest speaks metres; the editor speaks centimetres. Convert at the
  boundary, once, in one function — a mixed-units map is the classic silent defect.
- Deterministic scripts: iterate the manifest in stable order, derive labels from ids,
  and make the script **idempotent** (delete its own previously-tagged actors first) so
  a re-run after a manifest change is a clean rebuild, not a duplicate pile.

## 3. `Tools/py/build_arena.py` — the BP07 pattern (manifest → blockout)

1. Read `Content/Data/arena_manifest.json` (schema per `data-and-assets.md`).
2. Open/create `BR_Arena01` — **the .umap this ticket owns, lfs-locked** (law 7).
3. Floors/walls from `bounds` per z-level → scaled cube blockout, all actors tagged
   `BlockoutGenerated` (the idempotency handle).
4. `cover[]` → boxes by `height_class` · `landmarks[]` → labeled markers/geometry
   (labels = callout names; bots' EQS and humans' comms both read them).
5. `spawn_points[]` → `PlayerStart` per entry, rotation from `facing`, tag with
   `scoring_hints` values for the respawn scorer.
6. `NavMeshBoundsVolume` covering bounds (+ margin) — bots need nav from day one;
   rocket pad marker at its landmark.
7. Save; then **screenshots for the evidence loop**:
   `unreal.AutomationLibrary.take_high_res_screenshot(1920, 1080, "arena_top.png")`
   (top-down + one per landmark) — the arena-architect iterates from screenshots,
   and the walkthrough rung confirms the doubts[] geometry claims.

## 4. Unreal MCP servers (the "MCP for UE" question, answered with rules)

Community `unreal-mcp` servers exist and wrap this same Python/Remote-Control surface —
there is no first-party UE MCP. If one is evaluated:

- **Spike rules apply** (CREW-OPERATIONS restraint list): one throwaway session,
  findings to the ticket Log, nothing lands from the spike itself.
- The MCP session must obey the SAME laws as any agent: never touch a binary another
  ticket owns, never land geometry directly — its output is a *generated script* or a
  manifest diff that goes through the normal pipeline (curator → critic → builder).
- Live MCP driving is legitimate for: reading editor state, screenshots, spike-grade
  "does this layout read?" probes. It is NOT the landing mechanism, ever — that is the
  generated-script doctrine, and it is what keeps the verifier able to reproduce a map.
- Treat any third-party MCP as untrusted code: pin the version, read what it executes,
  and give it a project copy first, not your working tree.

## 5. DataTable reimport (the BP13-step-6 shape)

`Tools/reimport-tables.ps1` wraps the commandlet path; the Python equivalent when a
script needs it inline:
```python
ok = unreal.DataTableFunctionLibrary.fill_data_table_from_csv_file(
        unreal.EditorAssetLibrary.load_asset("/Game/Data/DT_Weapons"),
        unreal.Paths.project_content_dir() + "Data/DT_Weapons.csv")
assert ok, "reimport failed - row struct/schema mismatch (check BRDataRows.h)"
```
Reimport claims are rung-honest: "schema declared ≠ schema live" — a table change
exists when the reimport RAN and the pinned suites passed against the new values.

## 6. Self-check before handoff

Script committed under `Tools/py/` (not pasted into a chat) · idempotent re-run proven
· units converted once · every generated actor labeled + tagged · .umap locked before,
saved after, one owner · screenshots attached to the ticket Log · claims name their
rung (a generated map "works" at the walkthrough/functional rung, not on "the script
exited 0").
