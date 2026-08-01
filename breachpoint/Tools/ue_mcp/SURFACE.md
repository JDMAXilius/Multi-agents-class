# UE 5.8 MCP — the verified surface (BP16 step 1)

**Enumerated against a LIVE editor, 1 Aug 2026.** This file supersedes `RESEARCH.md` wherever
the two disagree — `RESEARCH.md` is desk research from a container with no engine, and it says
so itself. Where *this* file and a running editor disagree, the editor is right and this file
is the bug.

## Provenance (so the next session can judge how much to trust this)

| | |
|---|---|
| Editor | `D:\Program Files\UE_5.8_Source\Engine\Binaries\Win64\UnrealEditor.exe`, PID 44352, started 11:19 local |
| Project | `Breachpoint.uproject` (`ModelContextProtocol` + `MCPClientToolset` enabled, lines 75/79) |
| Endpoint | `http://127.0.0.1:8000/mcp` — **confirmed listening**, loopback only |
| Protocol | MCP `2025-06-18`, streamable HTTP, `Mcp-Session-Id` header. `serverInfo` name/title/version are all **empty strings** |
| Capabilities | `tools` (`listChanged: true`), `resources` (empty object — no resources exposed) |
| How probed | raw JSON-RPC over HTTP from PowerShell: `initialize` → `notifications/initialized` → `tools/list` → `describe_toolset` × 19 |

**Honesty note, stated at the top because it changes what this file proves.** No `.mcp.json`
existed when this was written, so the tools were **never registered in a Claude Code session** —
they were reached over raw HTTP. Per this ticket's own Kickoff (*"a tool that does not appear in
the session's tool list does not exist"*), **the transport and the schemas are verified; the
client integration is not.** Ticking that Kickoff line requires a session whose tool list
actually contains `unreal-mcp` tools.

**Only read-only calls were made.** `list_toolsets`, `describe_toolset`, and
`ProgrammaticToolset.get_execution_environment`. **Nothing was mutated, nothing was saved, no
asset was touched.** The read-only/mutating marks below are derived from each tool's own
declared description and schema, not from firing it.

---

## 1. Shape: it is a gateway, not a flat tool list

Three top-level tools. That is the entire `tools/list` response.

| Tool | Args | Kind |
|---|---|---|
| `list_toolsets` | — | R |
| `describe_toolset` | `toolset_name` | R |
| `call_tool` | `tool_name` (req), `toolset_name` (opt), `arguments` (opt) | dispatcher |

Everything else is reached as `call_tool{toolset_name, tool_name, arguments}`. **19 toolsets,
255 tools.** This is the first correction to `RESEARCH.md`, which listed nine documented
*categories* ("spawning actors, configuring lighting, creating material instances, inspecting
Slate widgets, running automation tests, navigating Blueprints, manipulating assets, building
levels, working with meshes"). Of those nine, **"inspecting Slate widgets" and "running
automation tests" have no toolset at all** — there is no Slate/UMG toolset and no automation
toolset. Do not plan against them.

**Legend used throughout:**

- **R** — read-only. Observational; changes nothing.
- **E** — editor-state. Mutates the *editor session* (selection, camera, PIE, verbosity) but no
  project data on disk. Cheap and safe; still not free — see R29 on PIE.
- **M** — mutating. Changes an asset, an actor, a level, or a file.

---

## 2. The toolsets

### `editor_toolset.toolsets.blueprint.BlueprintTools` — 53 tools ⚠️ **largest law exposure**

Creating and editing Blueprint graphs is this toolset's entire purpose. Against R18 (zero
Blueprint classes) and R26 (BP children as **default-value containers, empty graphs, no new
members**), this is the surface that can produce a forbidden artifact fastest, and produce it as
a binary the critic cannot diff. See §4.

| Tool | Kind | Required args | Notes / refusals |
|---|---|---|---|
| `create` | M | `folder_path, asset_name, asset_type` | creates a Blueprint asset |
| `compile_blueprint` | M | `blueprint` | call after graph edits are complete |
| `get_graph_dsl_docs` | R | — | full DSL syntax reference; call before first `write_graph_dsl` |
| `read_graph_dsl` | R | `graph` | returns an S-expression DSL round-trippable into `write_graph_dsl` |
| `write_graph_dsl` | M | `graph, code` | **populates a graph from a DSL script AND compiles.** The single highest-leverage mutating call on the whole server |
| `create_node` | M | `graph, type_id, pos` | |
| `delete_node` | M | `node` | |
| `connect_pins` / `break_pins` | M | `output_pin, input_pin` | |
| `set_pin_value` | M | `pin, value` | |
| `get_pin_value` | R | `pin` | |
| `add_node_pin` / `remove_node_pin` | M | `node` / `node, pin` | dynamic-pin nodes only (Switch, Sequence, Add/Multiply, Make Array) |
| `retarget_node_class` | M | `node, old_class, new_class` | in-place class swap + node reconstruction |
| `set_node_position` | M | `node, pos` | |
| `arrange_nodes` | M | `nodes` | layout only |
| `find_nodes` | R | `graph, title` | filters ANDed; use before `get_connected_subgraph` on large graphs |
| `get_connected_subgraph` | R | `node` | read one event chain without reading all of EventGraph |
| `get_node_infos` | R | `nodes` | |
| `find_node_types` | R | `graph, type_id_filter, context_pins` | **refuses to be broad**: "thousands of valid node types", filter must be specific |
| `find_node_categories` | R | `graph, category_filter, context_pins` | |
| `get_node_type_pins` | R | `graph, type_id` | |
| `add_variable` | M | `blueprint, name, type_name` | **restricted type list**: bool/int/float/byte/string/name/text + Vector/Rotator/Transform/Vector2D/LinearColor |
| `add_struct_variable` | M | `blueprint, name, struct_type` | any UStruct — the escape hatch from `add_variable`'s list (HitResult, GameplayTag) |
| `add_object_variable` | M | `blueprint, name, object_class` | |
| `remove_variable` | M | `blueprint, name` | |
| `list_variables` | R | `blueprint` | |
| `set_variable_replication` | M | `blueprint, variable_name, replication` | **NONE / REPLICATED / REP_NOTIFY. RepNotify auto-creates an `OnRep_` function.** See §4 — this is a replicated-surface change with no netcode packet and no REFUTER |
| `get_variable_replication` | R | `blueprint, variable_name` | |
| `set_variable_instance_editable` | M | `blueprint, variable_name, instance_editable` | |
| `set_variable_category` / `get_variable_category` | M / R | `blueprint, variable_name[, category]` | cosmetic (My Blueprint panel grouping) |
| `set_parent` | M | `blueprint, parent_class` | reparent; must recompile after |
| `get_parent` | R | `blueprint` | |
| `add_function_graph` | M | `blueprint, graph_name` | idempotent; overrides inherited function if name matches |
| `remove_function_graph` | M | `blueprint, graph_name` | **closes the Blueprint editor window if open** |
| `list_functions` / `list_graphs` / `get_graph` | R | `blueprint[, graph_name]` | `list_functions` includes inherited |
| `add_event` | M | `blueprint, event_name` | idempotent; overrides inherited event if name matches, else custom event |
| `list_events` | R | `blueprint` | local + inheritable + interface |
| `add_function_param` | M | `graph, param_name, param_type, input_param` | same restricted primitive/struct list as `add_variable`; **output params not supported on event dispatchers** |
| `add_struct_function_param` | M | `graph, param_name, struct_type, input_param` | any UStruct; same dispatcher restriction |
| `add_object_function_param` | M | `graph, param_name, object_class, input_param` | same dispatcher restriction |
| `remove_function_param` | M | `graph, param_name, input_param` | same dispatcher restriction |
| `add_event_dispatcher` | M | `blueprint, name` | |
| `list_event_dispatchers` | R | `blueprint` | |
| `add_component_bound_event` | M | `component, event_name, graph` | returns pre-existing node if present |
| `list_component_events` | R | `component` | |
| `list_compatible_event_functions` | R | `node` | Create Event nodes |
| `set_create_event_function` / `get_create_event_function` | M / R | `node[, function_name]` | |
| `get_default_object` | R | `blueprint` | returns the CDO; ObjectTools resolves the CDO automatically, so this is for handing the CDO to ActorTools |

### `editor_toolset.toolsets.material.MaterialTools` — 22

| Tool | Kind | Required args | Notes / refusals |
|---|---|---|---|
| `create_material` | M | `folder_path, asset_name` | **warns**: each new Material increases shader compile time; prefer a MaterialInstance |
| `create_function` | M | `folder_path, asset_name` | empty MaterialFunction |
| `create_parameter_collection` | M | `folder_path, asset_name` | MPC — runtime params without shader recompile |
| `recompile` | M | `material_or_function` | **raises if the shader fails to compile.** Call once after a batch of graph edits |
| `add_expression` | M | `material_or_function, expression_class` | |
| `delete_expression` | M | `material_or_function, expression` | |
| `list_expression_classes` | R | `material_or_function, search` | context-valid subclasses only |
| `get_expressions` | R | `material_or_function` | |
| `connect_expressions` | M | `from_expression, from_output_name, to_expression, to_input_name` | |
| `disconnect_expressions` | M | `to_expression, to_input_name` | |
| `connect_to_output` | M | `expression, output_name, material_property` | |
| `disconnect_from_output` | M | `material, material_property` | |
| `get_property_input` | R | `material, material_property` | `expression` is None when disconnected |
| `get_expression_inputs` | R | `material_or_function, expression` | declaration order |
| `get_expression_input_names` / `get_expression_output_names` | R | `expression` | empty string = default unnamed output |
| `delete_unused_expressions` | M | `material` | **triggers no recompile** — call `recompile` after |
| `layout_expressions` | M | `material_or_function` | |
| `list_parameter_groups` | R | `material_or_function` | |
| `rename_parameter_group` | M | `material_or_function, old_name, new_name` | merges if target exists; **internal recompile, no separate call needed** |
| `delete_parameter_group` | M | `material_or_function, group_name` | ungroups only, does not delete parameters; internal recompile |
| `get_referencing_materials` | R | `material_function` | |

### `editor_toolset.toolsets.skeletal_mesh.SkeletalMeshTools` — 22

| Tool | Kind | Required args | Notes / refusals |
|---|---|---|---|
| `import_file` | M | `folder_path, asset_name, source_file` | **source must contain skeleton hierarchy + skinned mesh data**; may also produce a new Skeleton asset |
| `add_socket` | M | `mesh, socket_name, bone_name` | |
| `remove_socket` / `rename_socket` | M | `mesh, socket_name[, new_name]` | |
| `set_socket_transform` / `get_socket_transform` | M / R | `mesh, socket_name[, transform]` | local, relative to parent bone |
| `get_socket_bone` / `get_socket_names` | R | `mesh[, socket_name]` | |
| `assign_physics_asset` | M | `mesh, physics_asset` | **must be compatible with the mesh's skeleton** |
| `get_physics_asset` | R | `mesh` | |
| `set_material` / `get_material` / `get_material_slots` | M / R / R | `mesh[, slot_name[, material]]` | `set_material` affects all non-overriding instances |
| `get_bone_names` / `get_bone_parent` / `get_bone_children` | R | `mesh[, bone_name]` | hierarchy order, root first; `''` for root's parent |
| `get_skeleton` | R | `mesh` | |
| `get_morph_target_names` | R | `mesh` | |
| `get_bounds` | R | `mesh` | **reference pose only — does not account for animation** |
| `get_vertex_count` / `get_lod_count` / `get_section_count` | R | `mesh` | sections ≥ material slots |

### `EditorToolset.EditorAppToolset` — 21

| Tool | Kind | Required args | Notes / refusals |
|---|---|---|---|
| `StartPIE` | E | `options` | **completes after `PostPIEStarted` + `Options.WarmupSeconds`** — it waits for BeginPlay, it does not fire-and-forget |
| `StopPIE` | E | — | **raises if no play session is running** |
| `IsPIERunning` | R | — | |
| `CaptureViewport` | R | *(all optional)* `captureTransform, annotations, bShowUI` | annotations overlay a projected world-space grid + name/position labels on visible actors — **this is the arena-architect's evidence loop** |
| `CaptureEditorImage` | R | — | whole editor app as the user sees it |
| `CaptureAssetImage` | R | `assetPath` | thumbnail render (meshes, skeletons, anims, montages, materials, textures) |
| `GetCameraTransform` / `SetCameraTransform` | R / E | `[transform]` | level viewport camera |
| `FocusOnActors` | E | `actors` | **cannot be called while PIE is active** |
| `GetVisibleActors` | R | — | bounds intersecting the viewport frustum |
| `GetSelectedActors` / `SelectActors` | R / E | `[actors]` | |
| `GetSelectedAssets` / `SelectAssets` | R / E | `[assetPaths]` | `SelectAssets` completes once applied |
| `GetOpenAssets` | R | — | assets open in asset editors |
| `OpenEditorForAsset` | E | `assetPath` | |
| `GetContentBrowserPath` / `SetContentBrowserPath` | R / E | `[path]` | |
| `WorldPosToScreenCoords` | R | `position` | normalized screen space |
| `ScreenCoordsToWorld` | R | `coords` | nearest solid object at normalized view coords |
| `SearchCVars` | R | `name` | **finds** CVars by substring. **There is NO tool to SET a CVar and NO tool to execute a console command** — see §3 |

### `editor_toolset.toolsets.asset.AssetTools` — 21

| Tool | Kind | Required args | Notes / refusals |
|---|---|---|---|
| `write_file` | M | `file_path, content` | **CONFINED: only under `/Game/`, an enabled plugin's `Content/`, or project `Saved/`. Plain text only.** Overwrites silently. See §4 |
| `read_file` | R | `file_path` | same confinement, same text-only rule |
| `find_assets` | R | `folder_path, name` | |
| `exists` | R | `path` | |
| `load_asset` | M* | `asset_path` | loads into memory; marked M* because loading is a side effect on editor state, not on disk |
| `save_assets` | M | `asset_paths` | |
| `is_dirty` | R | `asset_path` | |
| `delete` | M | `path` | asset **or folder** |
| `move` | M | `path, new_path` | move **or rename** — this is the R26 rename mechanism, and unlike `git mv` it rewrites the package name and fixes referencers |
| `duplicate` | M | `path, new_path` | |
| `create_folder` / `list_folders` | M / R | `path` / `root_path` | `create_folder` returns True if it already exists |
| `get_dependencies` / `get_referencers` | R | `asset_path` | **exactly what a law-7 binary-owner check needs** |
| `get_asset_class` | R | `asset_path` | returns e.g. `'HeroCharacter_C'` — **a `_C` suffix identifies a Blueprint generated class, so this is the mechanical R18/R26 audit primitive `Tools/audit_blueprints/` never got working** |
| `get_asset_tags` / `get_metadata_tags` / `update_metadata_tags` | R / R / M | `asset_path` | |
| `is_checked_out` / `can_edit_asset` | R | `asset_path` | **`can_edit_asset` is always True when source control is disabled** — it is NOT an lfs-lock check, do not use it for law 7 |
| `get_plugin_content_paths` | R | *(opt)* `include_engine` | |

### `editor_toolset.toolsets.scene.SceneTools` — 20

| Tool | Kind | Required args | Notes / refusals |
|---|---|---|---|
| `load_level` | E | `level_path` | |
| `get_current_level` | R | — | |
| `add_to_scene_from_class` | M | `actor_type, name, xform` | **returns nothing if creation was unsuccessful** — a silent-null failure mode; check the return |
| `add_to_scene_from_asset` | M | `asset_path, name, xform` | same silent-null |
| `remove_from_scene` | M | `actor` | |
| `save_actor` | M | `actor` | per-actor save (One File Per Actor / World Partition) |
| `find_actors` | R | `name, tag, collision_channels` | **tag-based search is the idempotency handle** the blockout doctrine needs (`BlockoutGenerated`) |
| `get_collision_channels` | R | — | |
| `trace_world` | R | `start, end` | returns distance to first hit, or None. **This is the tool that can settle R7's "geometry claims are editor-rung" doubts mechanically** — mutual visibility and LOS-breakage become measurements, not doubts |
| `get_folders` / `get_actors_in_folder` | R | `[folder_path]` | includes intermediate parent paths |
| `set_actor_folder` | M | `actor, folder_path` | creates folder implicitly; `''` = outliner root |
| `rename_folder` / `delete_folder` | M | `old_path, new_path` / `folder_path` | both re-root sub-folders rather than orphaning actors |
| `create_level_instance` | M | `level_path, name, xform` | |
| `edit_level_instance` | M | `level_instance` | **only one level instance in edit mode at a time**; scene tools then operate inside its sub-level |
| `commit_level_instance` | M | `level_instance` | saves **or discards** and exits edit mode |
| `merge_actors` | M | `actors, output_path, name` | StaticMesh actors → one mesh asset |
| `is_checked_out` / `can_edit` | R | `actor` | same source-control caveat as AssetTools |

### `editor_toolset.toolsets.actor.ActorTools` — 17

| Tool | Kind | Required args | Notes |
|---|---|---|---|
| `set_actor_transform` / `get_actor_transform` | M / R | `actor[, xform]` | world space |
| `look_at` | M | `actor, target` | forward vector → world position |
| `get_actor_bounds` | R | `actor` | world-space box |
| `set_label` / `get_label` | M / R | `actor[, label]` | **labels ARE callouts** — the blockout doctrine requires every generated actor labeled |
| `add_tag` / `remove_tag` / `has_tag` / `get_tags` | M / M / R / R | `actor[, tag]` | the idempotency handle |
| `add_component` / `remove_component` / `get_components` | M / M / R | `owner, component_type, name` | **works on an actor instance OR a blueprint** |
| `get_root_component` / `get_parent_component` / `set_parent_component` | R / R / M | `actor` / `component` | on BP actors, parenting a component to the root **promotes it to scene root** |
| `get_component_actor` | R | `component` | |

### `editor_toolset.toolsets.static_mesh.StaticMeshTools` — 16

| Tool | Kind | Required args | Notes / refusals |
|---|---|---|---|
| `import_file` | M | `folder_path, asset_name, source_file` | may also produce Materials/Textures |
| `set_nanite_enabled` / `is_nanite_enabled` | M / R | `mesh[, enabled]` | **enabling triggers a mesh rebuild** |
| `generate_lods` | M | `mesh, triangle_percents` | one LOD per entry; fractions in (0.0, 1.0] |
| `remove_lods` | M | `mesh` | keeps LOD 0 |
| `set_lod_thresholds` / `get_lod_thresholds` | M / R | `mesh[, thresholds]` | screen-size ratios; values > 1.0 are valid |
| `generate_convex_collisions` | M | `mesh` | **replaces existing collision** |
| `remove_collisions` | M | `mesh` | |
| `set_material` / `get_material` / `get_material_slots` | M / R / R | `mesh[, slot_name[, material]]` | |
| `get_bounds` / `get_vertex_count` / `get_triangle_count` / `get_lod_count` | R | `mesh` | |

### `editor_toolset.toolsets.material_instance.MaterialInstanceTools` — 13

| Tool | Kind | Required args | Notes / refusals |
|---|---|---|---|
| `create` | M | `folder_path, asset_name, parent` | MIC from a parent material — **no shader recompile on param change** (the cheap path) |
| `set_parent` | M | `instance, parent` | |
| `set_scalar_parameter` / `get_scalar_parameter` | M / R | `instance, name[, value]` | getters return the **effective** value, inheriting from parent |
| `set_vector_parameter` / `get_vector_parameter` | M / R | `instance, name[, value]` | LinearColor RGBA |
| `set_texture_parameter` / `get_texture_parameter` | M / R | `instance, name[, value]` | getter returns None if not overridden |
| `set_static_switch_parameter` / `get_static_switch_parameter` | M / R | `instance, name[, value]` | **overriding a static switch triggers a shader recompile** |
| `set_parameter_override` | M | `instance, name, override` | disabling **discards** the prior value for non-static types |
| `clear_parameters` | M | `instance` | reverts all to parent |
| `list_parameters` | R | `material` | works on material or instance |

### `editor_toolset.toolsets.data_table.DataTableTools` — 10 ⭐ **the BP13-step-6 path**

| Tool | Kind | Required args | Notes / refusals |
|---|---|---|---|
| `import_file` | M | `folder_path, asset_name, source_file, schema` | **the CSV reimport.** "The file's columns must match the property names in schema" — a schema mismatch is the documented failure |
| `search_row_structs` | R | *(opt)* `struct_name` | **structs derived from `TableRowBase`** — this is how you discover `FBRWeaponRow` etc. from `BRDataRows.h` without guessing |
| `get_schema` | R | `data_table` | JSON: column → type info. **"schema declared ≠ schema live" becomes checkable** |
| `create` | M | `folder_path, asset_name, schema` | |
| `list_rows` / `get_rows` | R | `data_table[, row_names]` | `get_rows` returns JSON |
| `add_rows` | M | `data_table, row_names` | default values |
| `set_rows` | M | `data_table, values` | |
| `rename_rows` / `remove_rows` | M | `data_table, renames` / `row_names` | |

### `editor_toolset.toolsets.curve_table.CurveTableTools` — 9

| Tool | Kind | Required args | Notes |
|---|---|---|---|
| `import_file` | M | `folder_path, asset_name, source_file, interp_mode` | **first column = row name; subsequent columns are sample times and values** — this is the `CT_Combat.csv` shape |
| `create` | M | `folder_path, asset_name` | |
| `list_rows` / `add_row` / `remove_row` / `rename_row` | R / M / M / M | `curve_table[, row_name]` | |
| `get_keys` / `set_keys` / `add_key` | R / M / M | `curve_table, row_name[, keys]` | `set_keys` **replaces all** keys in the row |

### `editor_toolset.toolsets.string_table.StringTableTools` — 8

| Tool | Kind | Required args | Notes / refusals |
|---|---|---|---|
| `import_file` | M | `folder_path, asset_name, source_file` | **requires header columns `Key` and `SourceString`**; extra meta columns import, but **namespace does NOT** — it is derived from the asset path |
| `create` | M | `folder_path, asset_name` | |
| `set_entry` / `get_entry` / `remove_entry` / `list_keys` | M / R / M / R | `string_table[, key[, value]]` | `set_entry` overwrites existing keys |
| `get_namespace` / `get_table_id` | R | `string_table` | table ID derives from package path |

### `editor_toolset.toolsets.object.ObjectTools` — 6

| Tool | Kind | Required args | Notes |
|---|---|---|---|
| `list_properties` / `get_properties` | R | `instance[, properties]` | `get_properties` returns JSON |
| `set_properties` | M | `instance, values` | **the generic property-write path — reaches any UObject, including a Blueprint CDO.** This is how R26's "default values only" would actually be authored |
| `reset_properties` | M | `instance, properties` | removes per-instance overrides |
| `get_class` | R | `instance` | |
| `search_subclasses` | R | `base_class, class_name` | **class discovery** — e.g. every `UBRGameplayAbility` subclass |

### `EditorToolset.LogsToolset` — 4

| Tool | Kind | Required args | Notes |
|---|---|---|---|
| `GetLogEntries` | R | `pattern` | **from the current session's log file** — the `BRTEST\|AUTH` / `HOSTLOCAL` / `REMOTE` three-viewpoint assertion pattern is readable from here during PIE |
| `GetLogCategories` | R | `filter` | sorted; R24 gave us per-discipline channels, so this enumerates them |
| `GetVerbosity` / `SetVerbosity` | R / E | `[category]` / `verbosity` | |

### `editor_toolset.toolsets.primitive.PrimitiveTools` — 4

| Tool | Kind | Required args | Notes |
|---|---|---|---|
| `add_cube` / `add_sphere` / `add_cylinder` / `add_cone` | M | `actor, name` | adds a shaped **StaticMeshComponent to an existing actor**. NOT a standalone spawn — the blockout pattern is `SceneTools.add_to_scene_from_class` first, then these |

### `ToolsetRegistry.AgentSkillToolset` — 4 ⚠️

| Tool | Kind | Required args | Notes |
|---|---|---|---|
| `ListSkills` | R | — | AgentSkills **in the project** |
| `GetSkills` | R | `skillPaths` | |
| `CreateSkill` | M | `folderPath, assetName, description, details` | *"should ONLY be called after getting explicit direction or permission from the user"* |
| `UpdateSkill` | M | `skillPath, description, details` | same caveat |

**The editor can write agent instructions.** The only guard is a sentence in the tool
description asking politely. See §4.

### `editor_toolset.toolsets.programmatic.ProgrammaticToolset` — 2 ⭐ **the law-7 answer**

| Tool | Kind | Required args | Notes |
|---|---|---|---|
| `get_execution_environment` | R | — | returns the sandbox contract |
| `execute_tool_script` | M* | `script` | Python; **must define `run() -> Dict[str, Any]`**. Batches calls to the other 253 tools |

**The sandbox, quoted from the live server** (this matters more than it looks):

- Language: Python. Entry point: `run()` returning a dict. Non-dict return → `TypeError`.
- Helper available: `execute_tool(tool_name, json_input)` — full dotted tool name, JSON string
  in, dict-like out, **raises `RuntimeError` on failure so no error checking is needed**.
- **Importable modules — the complete frozenset: `{time, datetime, math, json, re, copy}`.**
- Refusals: `SyntaxError` on bad syntax · `ValueError` on a disallowed import **or a missing
  `run()`** · `TypeError` on a non-dict return.

**Read the import list again, because it decides the retrofit design.** There is **no `unreal`
module, no `os`, no `open()`, no filesystem access.** A script cannot read
`Content/Data/arena_manifest.json` directly — it must go through
`AssetTools.read_file("/Game/Data/arena_manifest.json")`, which the confinement rule permits.
Any generator ported to this path reads its manifest through the MCP or not at all.

### `editor_toolset.toolsets.texture.TextureTools` — 2

| Tool | Kind | Required args |
|---|---|---|
| `import_file` | M | `folder_path, asset_name, source_file` |
| `get_size` | R | `texture` |

### `editor_toolset.toolsets.data_asset.DataAssetTools` — 1

| Tool | Kind | Required args | Notes |
|---|---|---|---|
| `create` | M | `folder_path, asset_name, asset_type` | **the `DA_InputConfig` path** — `Tools/gen_input/`'s asset half is this one call plus `ObjectTools.set_properties` |

---

## 3. What is NOT here (plan against this list, not against hope)

Absences verified by enumeration, not assumed:

- **No console-command execution.** `SearchCVars` finds CVars; nothing sets one and nothing runs
  an arbitrary console command. **Consequence: `ModelContextProtocol.GenerateClientConfig
  ClaudeCode` cannot be invoked through the MCP** — the config must be typed into the editor
  console by a human or hand-written (see the companion `.mcp.json`).
- **No Slate/UMG/widget toolset.** `RESEARCH.md` listed "inspecting Slate widgets" from
  secondary coverage. It does not exist. **BP10's WBP layout assets stay Tier-4 human work.**
- **No automation/test toolset.** `RESEARCH.md` listed "running automation tests". It does not
  exist. **The ladder stays headless and stays `Tools/run-*.ps1`** — which is what BP16's
  out-of-scope list already required, now confirmed rather than asserted.
- **No Niagara, MetaSound, AnimGraph, StateTree, or EQS toolset.** Every Tier-4 asset class in
  the authoring matrix that isn't a material or a mesh is untouched. **`ST_Bot` and the EQS
  queries (BP08's `editor-live` justification) get no help from the MCP.**
- **No source-control toolset.** `can_edit_asset` returns True whenever source control is
  disabled, so **nothing here checks an lfs lock.** Law 7's one-owner-per-binary remains a
  human/hook obligation.
- **No `resources` and no `prompts`.** The `initialize` response advertises an empty `resources`
  capability and no prompts.

---

## 4. Findings that belong to steps 2 and 5, recorded here because step 1 produced them

Step 1 is enumeration and this section is not the ruling. These are the inputs the ruling needs.

**(a) The jurisdiction hole is real, and narrower than `RESEARCH.md` feared.**
`AssetTools.write_file` writes arbitrary text with no `file_path` visible to `guard_laws.py` —
confirmed. **But the server confines it to `/Game/`, plugin `Content/`, and `Saved/`.** So an
MCP call **cannot** reach `Source/`, `docs/`, `Config/`, or `Tools/`. It **can** silently
overwrite `Content/Data/DT_Weapons.csv` — BP03's and BP13's owner path — with law 5 never
firing. The hole is bounded by the server, not by us, and the bound is exactly the tuning data
the whole data-crew pipeline exists to protect.

**(b) `BlueprintTools` can violate R18/R26 in one call.** `write_graph_dsl` populates a graph
**and compiles it**. R26 permits a BP child with *empty graphs, no new members, no gameplay
numbers*. A single `write_graph_dsl` produces the exact artifact R26 forbids, as a binary
`.uasset` no critic can diff. Whatever step 2 rules, it must name this toolset explicitly.

**(c) `set_variable_replication` is a netcode change with no packet.** It sets
NONE/REPLICATED/REP_NOTIFY on a Blueprint variable and auto-generates `OnRep_`. Law 1 says a new
replicated property = a netcode-builder packet + a critic REFUTER. This tool creates one in a
binary asset, invisible to the diff that would trigger that review.

**(d) `AgentSkillToolset` lets the editor rewrite agent instructions.** `CreateSkill` /
`UpdateSkill` are guarded only by a sentence in their own descriptions asking for user
permission. An MCP-driven session can modify the instructions it operates under.

**(e) The step-2 mechanism already exists, and it is option (a).** `ProgrammaticToolset` was
built for exactly the generated-script doctrine: **the committed, reviewable artifact is the
Python script**, and the MCP is the executor that runs it. The doctrine does not need to bend to
accommodate the MCP; the MCP shipped with the doctrine's shape in it. The sandbox's refusal to
import anything but `{time, datetime, math, json, re, copy}` is what makes the script reviewable
— it can only orchestrate declared tools, never do arbitrary I/O.

**(f) Three tools would close open project problems, and none is a new mechanism:**
`AssetTools.get_asset_class` returns `_C`-suffixed generated-class names — the R18/R26 audit
primitive `Tools/audit_blueprints/` never got working. `SceneTools.trace_world` turns R7's
editor-rung geometry doubts into measurements. `DataTableTools.get_schema` +
`search_row_structs` make "schema declared ≠ schema live" mechanically checkable against
`BRDataRows.h`.

**(g) R29 has a second edge nobody wrote down.** `StartPIE`/`StopPIE` mean an MCP session can
start a play session inside the editor that holds the project lock. One editor, one driver now
also means: **an MCP session running PIE and a human playing in that same editor are the same
conflict as an MCP session and a build.**

---

## 5. Still owed before the Kickoff line is honestly ticked

- [ ] Register `.mcp.json` and confirm the tools **resolve by name in a session's tool list**.
      Until then this file documents a transport, not a capability.
- [ ] Fire one **rejecting** case at the confinement rule (`write_file` to a path outside
      `/Game/`) and record the refusal verbatim. *Every defect the last three sessions found was
      a rule that read as enforced and was not — this file asserts a confinement it has not
      tested, which is precisely that shape.*
- [ ] Steps 2–5 of the ticket. Nothing in this file is a ruling.
