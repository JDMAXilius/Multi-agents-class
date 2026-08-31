"""BR_Spillway - the six material variants the arena is built from.

    python3 Tools/blockout/make_spillway_meshes.py     # editor must be open

WHY THIS EXISTS
    The obvious way to colour a blockout is a per-actor material override
    (StaticMeshComponent.OverrideMaterials). Through the editor's MCP server that does not
    work: ObjectTools.set_properties returns false and changes nothing on the components of
    World Partition external actors - verified against `overrideMaterials`, `overlayMaterial`,
    `minLOD` and `bOverrideMinLOD` alike, so it is the writer, not the property name.

    So the material rides on the MESH instead. Each variant is a duplicate of one of
    Lvl_Shooter's own prototyping meshes with one slot repointed, living in our folder -
    the template's assets are never modified, and the arena still reads as the same kit.

    Geometry is identical to the source in every case; only the material differs.
"""

import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from land_spillway import ASSET, call, connect  # noqa: E402

STATIC_MESH = "editor_toolset.toolsets.static_mesh.StaticMeshTools"
FOLDER = "/Game/Spillway/Meshes"

MAT_DECK = "/Game/LevelPrototyping/Materials/MI_PrototypeGrid_TopDark.MI_PrototypeGrid_TopDark"
MAT_WALL = "/Game/LevelPrototyping/Materials/MI_PrototypeGrid_Gray.MI_PrototypeGrid_Gray"
MAT_TEAM = "/Game/FirstPerson/MI_FirstPersonColorway.MI_FirstPersonColorway"

# name              source mesh                                    slot           material
VARIANTS = [
    ("SM_SPW_Deck", "/Game/LevelPrototyping/Meshes/SM_Cube", "lambert1", MAT_DECK),
    ("SM_SPW_Wall", "/Game/LevelPrototyping/Meshes/SM_Cube", "lambert1", MAT_WALL),
    ("SM_SPW_Accent", "/Game/LevelPrototyping/Meshes/SM_Cube", "lambert1", MAT_TEAM),
    ("SM_SPW_Ramp", "/Game/LevelPrototyping/Meshes/SM_Ramp", "lambert1", MAT_TEAM),
    ("SM_SPW_Pillar", "/Game/LevelPrototyping/Meshes/SM_Cylinder", "lambert1", MAT_WALL),
    ("SM_SPW_Crate", "/Game/LevelPrototyping/Meshes/SM_ChamferCube", "CubeMaterial", MAT_TEAM),
]


def main():
    connect()
    call(ASSET, "create_folder", {"path": FOLDER})
    made = []
    for name, src, slot, mat in VARIANTS:
        dst = "%s/%s" % (FOLDER, name)
        ref = "%s.%s" % (dst, name)
        if not call(ASSET, "exists", {"path": dst})["returnValue"]:
            call(ASSET, "duplicate", {"path": src, "new_path": dst})
        slots = call(STATIC_MESH, "get_material_slots", {"mesh": {"refPath": ref}})["returnValue"]
        if slot not in slots:
            raise SystemExit("%s has slots %s, not %r" % (name, slots, slot))
        ok = call(STATIC_MESH, "set_material",
                  {"mesh": {"refPath": ref}, "slot_name": slot,
                   "material": {"refPath": mat}})["returnValue"]
        got = call(STATIC_MESH, "get_material",
                   {"mesh": {"refPath": ref}, "slot_name": slot})["returnValue"]
        assert got["refPath"] == mat, "%s reads back %s" % (name, got)
        made.append(name)
        print("  %-16s <- %-46s %s  %s" % (name, src.split("/")[-1], slot,
                                           "set" if ok else "already"))
    print(call(ASSET, "save_assets",
               {"asset_paths": ["%s/%s" % (FOLDER, n) for n in made]}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
