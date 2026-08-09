---
name: unreal-mcp
description: Drive a running Unreal Engine 5.8 editor through the built-in Unreal MCP server (unreal-mcp) — spawn/inspect actors, edit Blueprints, materials, meshes, data tables, run PIE, capture viewports, read logs. Use whenever the task touches a live UE editor session, a .uproject, a level/map, an Actor, a Blueprint, a Material, or the user says "in the editor", "in Unreal", "spawn", "place in the level", "compile the blueprint", "start PIE".
---

# Unreal MCP

The `unreal-mcp` server is Epic's first-party MCP server shipped in UE 5.8
(`Engine/Plugins/Experimental/ModelContextProtocol`). It runs **inside the editor
process**, HTTP at `http://127.0.0.1:8000/mcp`.

## Preflight

The server only exists while the editor is open. Before anything else:

```
call list_toolsets
```

- Fails / no tools → the editor isn't running, or the MCP plugin is off for that project.
- Toolsets come from whichever plugins that project enables, so **the set changes per
  project**. Never work from a remembered tool list — re-run `list_toolsets`.

Autostart is per-project, in `<Project>/Config/DefaultEditorPerProjectUserSettings.ini`:

```ini
[/Script/ModelContextProtocolEngine.ModelContextProtocolSettings]
bAutoStartServer=True
ServerPortNumber=8000
ServerUrlPath=/mcp
```

Or launch the editor with `-ModelContextProtocolStartServer`.

## How calling works

Only three tools are exposed at the MCP layer (tool-search mode, `bEnableToolSearch`).
Everything else is discovered on demand:

1. `list_toolsets` → toolset names
2. `describe_toolset(toolset_name)` → every tool in it, with input and output schemas
3. `call_tool(toolset_name, tool_name, arguments)` → run one

`tool_name` is the **bare** name (`find_actors`), not the dotted path.
Always `describe_toolset` before first use of a tool — do not guess argument names.

## Four rules that cause most failures

**1. Every property in the schema must be present.** `"default": null` does *not* mean
optional. Omitting one is a hard error. Pass `null` for unset objects, `""` for unset
strings.

```jsonc
// FAILS: 'Function "find_actors", input param "tag" is required ... but is missing'
{"name": "Light"}

// WORKS
{"root": null, "name": "Light", "actor_type": null,
 "tag": "", "bounds": null, "collision_channels": null}
```

**2. Object and class references are `{"refPath": "<soft path>"}`** — never a bare string.

```jsonc
{"actor_type": {"refPath": "/Script/Engine.PointLight"}}
{"root":       {"refPath": "/Game/Maps/BR_Arena01.BR_Arena01:PersistentLevel.SkyLight_..."}}
```

Actors returned by `find_actors` come back as `refPath` — feed them straight back in.

**3. Results are JSON text wrapped in `returnValue`.** Parse the content text, then read
`.returnValue`:

```json
{"returnValue": [{"refPath": "/Game/Maps/BR_Arena01.BR_Arena01:PersistentLevel.SkyLight_..."}]}
```

**4. `outputSchema` is present but not universal** — 242 of 309 tools here carry one;
void-returning tools omit it. When a tool has no `outputSchema`, make one real call and
look at the result before writing logic that parses it.

## Transforms

`ToolsetTransform` = optional `location` / `rotation` / `scale`, each `{x,y,z}`.
Unset means **identity when creating**, **leave unchanged when modifying**. On
`add_to_scene_from_class`, `xform` is parent-local if `parent` is set, world-space otherwise.

## Batching

More than ~3 dependent calls → use `ProgrammaticToolset.execute_tool_script` instead of
round-tripping each one. The script defines `run()` returning a dict and calls
`execute_tool(full_dotted_tool_name, json_input_string)` — note it takes the **full**
name there, unlike `call_tool`. Wrap each call in a short helper at the top.
Call `get_execution_environment` for the exact contract before writing a script.

It's tool orchestration, not general Python — no arbitrary engine scripting.

## Toolsets (Breachpoint, UE 5.8 — 23 toolsets / 309 tools; verified 2026-08-04)

| Toolset | # | Use for |
|---|---|---|
| `...blueprint.BlueprintTools` | 53 | graphs, nodes, pins, variables, functions, events, compile |
| `UMGToolSet.UMGToolSet` | 23 | widget blueprints, widget tree, named slots, UI components, event binding |
| `...material.MaterialTools` | 22 | expressions, connections, parameter groups, create material/function |
| `...skeletal_mesh.SkeletalMeshTools` | 22 | sockets, bones, morphs, physics asset, LODs |
| `EditorToolset.EditorAppToolset` | 21 | PIE start/stop, viewport + asset capture, camera, selection, CVars |
| `...asset.AssetTools` | 21 | find/load/save/move/duplicate/delete, deps, metadata, read/write file |
| `...scene.SceneTools` | 20 | load level, spawn/remove actors, folders, world trace, level instances |
| `...actor.ActorTools` | 17 | transforms, labels, tags, components, parenting, bounds |
| `...static_mesh.StaticMeshTools` | 16 | LODs, collision, materials, Nanite |
| `SlateInspectorToolset.SlateInspectorToolset` | 14 | drive the editor's own UI: snapshot, click, type, drag, screenshot |
| `...material_instance.MaterialInstanceTools` | 13 | scalar/vector/texture/switch params, parent |
| `...data_table.DataTableTools` | 10 | rows, schema, import |
| `MVVMToolset.MVVMToolset` | 9 | ViewModels, view bindings, conversion functions |
| `...curve_table.CurveTableTools` | 9 | rows, keys, import |
| `ConfigSettingsToolset.ConfigSettingsToolset` | 8 | list/inspect/edit/save config sections |
| `...string_table.StringTableTools` | 8 | entries, namespace |
| `...object.ObjectTools` | 6 | get/set/list properties, class + subclass search |
| `ToolsetRegistry.AgentSkillToolset` | 4 | list/get/create/update skills |
| `EditorToolset.LogsToolset` | 4 | log entries, categories, verbosity |
| `...primitive.PrimitiveTools` | 4 | add cube/sphere/cylinder/cone |
| `...programmatic.ProgrammaticToolset` | 2 | batch scripting (above) |
| `...texture.TextureTools` | 2 | size, import |
| `...data_asset.DataAssetTools` | 1 | create |

`...` = `editor_toolset.toolsets`. Names in `describe_toolset` must be given in full.

Four of these — `UMGToolSet`, `SlateInspectorToolset`, `MVVMToolset`, `ConfigSettingsToolset`
— are registered by this project via `UToolsetRegistry`, not by stock UE 5.8. They use
PascalCase tool names (`AddWidget`, `Click`), unlike the stock snake_case ones. Toolsets
come from whichever plugins a project enables, so re-run `list_toolsets` per project rather
than trusting this table.

## Task recipes

**Find something in the level** — `SceneTools.find_actors` (filter by name / type / tag /
bounds), or `EditorAppToolset.GetSelectedActors` when the user means "the thing I have
selected". `get_current_level` tells you which map is open.

**Spawn** — `SceneTools.add_to_scene_from_class` (class ref + name + xform), or
`add_to_scene_from_asset` for a placed asset. Bare shapes: `PrimitiveTools.add_*`.
Don't know the class path? `ObjectTools.search_subclasses`.

**Read or change any property on anything** — `ObjectTools.list_properties` first, then
`get_properties` / `set_properties`. This is the general escape hatch when no specialised
tool covers the field.

**Blueprints** — call `BlueprintTools.get_graph_dsl_docs` *before* touching
`read_graph_dsl` / `write_graph_dsl`; the DSL is the efficient path for graph edits versus
node-by-node calls. Always `compile_blueprint` at the end and report the result.

**UMG widgets** — `UMGToolSet` builds the tree (`CreateWidgetBlueprint`, `AddWidget`,
`SetNamedSlotContent`, …), but it sets **no** properties itself. For every widget or slot it
returns: `ObjectTools.list_properties` → `get_properties` → `set_properties`. Property names
vary per widget class and cannot be guessed; skipping `list_properties` makes
`set_properties` silently no-op or write the wrong field. Finish with
`CompileWidgetBlueprint`. Bindings go through `MVVMToolset` (`CreateViewModel`,
`AddViewModelToWidget`, `CreateViewBinding`).

**Drive the editor's own UI** — `SlateInspectorToolset`. A shallow root observer tracks
top-level windows only; `Observe()` the specific window or panel first for deep widget
coverage, then `Unobserve()`. Then `Snapshot` / `Click` / `Type` / `FillForm` / `Screenshot`.

**Verify visually** — `EditorAppToolset.CaptureViewport` (set the camera first with
`SetCameraTransform` or `FocusOnActors`) and read the image back. Use this to confirm a
change instead of asserting it worked.

**Debug** — `LogsToolset.GetLogEntries`. Bump `SetVerbosity` on the relevant category
first if the log is quiet.

## Before writing

Editor state is the user's live work — treat writes as real edits, not experiments.

- Check `AssetTools.can_edit_asset` / `is_checked_out` before modifying an asset.
- Nothing persists until `AssetTools.save_assets` (or `SceneTools.save_actor`). Ask before
  saving unless the user asked for the change to stick.
- `EditorAppToolset.IsPIERunning` — many edits misbehave mid-PIE. `StopPIE` first.
- Deletes (`AssetTools.delete`, `SceneTools.remove_from_scene`) are not undoable from here.
  Confirm first, and name exactly what will be removed.
