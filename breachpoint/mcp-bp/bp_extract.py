#!/usr/bin/env python3
"""Make a Blueprint readable. Editor-live; reads only, writes nothing to the project.

    python3 mcp-bp/bp_extract.py --list
    python3 mcp-bp/bp_extract.py --set character
    python3 mcp-bp/bp_extract.py                      # every set

WHY THIS EXISTS. R18's stated reason for banning Blueprint classes is that **binary assets are
invisible to the critic — no diff, no merge, no grep.** A bought template is exactly that: 63
Blueprints nobody on this project can review, argue with, or port from. This tool turns each one
into a JSON record that git can diff and a human can read, and that record becomes the
acceptance criteria for the C++ that replaces it.

WHAT IT CAN AND CANNOT READ — stated up front so nobody mistakes the output for a translation.

  CAN (via ObjectTools on the generated class's CDO):
    · parent class, so the inheritance chain is knowable
    · every variable: name, type, current default value
    · component defaults where they are UPROPERTYs on the CDO
    · asset references — which mesh, montage, curve or layer class it points at

  CANNOT, and no tool in this project can:
    · the node-by-node logic inside an event graph or function
    · exec-pin ordering, latent nodes, timeline internals
    · what the graph DECIDES

So this is an INVENTORY, not a transpiler. The logic is re-implemented by reading and
understanding it, never by machine translation — which is also the only way it gets BETTER
rather than faithfully preserving whatever the seller wrote.

THE ANIMBLUEPRINT CASE IS THE VALUABLE ONE. An ABP's CDO carries the AnimInstance's variables —
which IS the anim state model. `ABP_Mannequin_Base`'s property list is, almost exactly, the
field list `UBRAnimInstance` needs. That one extraction is most of the port's design work.
"""
from __future__ import annotations

import argparse, json, sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO / "mcp-ui" / "gen_ui"))
from mcp import MCP, ASSET, OBJ, EXIT_PASS, EXIT_BLOCKED           # noqa: E402

# FINDING, filed not fixed: `mcp.py` is generic transport and now serves three lanes — UI
# widgets, materials, and this. It lives in `mcp-ui/gen_ui/` because that is where it was
# extracted from, which is the same "shared thing inside one consumer" shape that extraction
# was meant to end. Moving it is a separate packet; the sys.path hop above is the honest
# interim and is deliberately ugly so nobody mistakes it for the design.

T = "/Game/FPSTemplate"

# The sets are ordered by PORT VALUE, not by folder. `character` and `anim_spine` are the
# whole job; everything below them is per-weapon DATA that stays an asset.
SETS: dict[str, list[str]] = {
    # The two that carry the logic worth porting.
    "character": [
        f"{T}/Blueprints/Characters/BP_FPST_Character",
    ],
    "anim_spine": [
        f"{T}/Demo/Characters/Heroes/Mannequin/Animations/ABP_Mannequin_Base",
        f"{T}/Demo/Characters/Heroes/Mannequin/Animations/LinkedLayers/ABP_ItemAnimLayersBase",
        f"{T}/Demo/Characters/Heroes/Mannequin/Animations/LinkedLayers/ALI_ItemAnimLayers",
        f"{T}/Demo/Characters/Heroes/Mannequin/Animations/ABP_Mannequin_Retarget",
        f"{T}/Demo/Characters/Heroes/Mannequin/Animations/ABP_Mannequin_CopyPose",
    ],
    # Weapon logic: one base plus its children. The children usually differ only in defaults,
    # which is itself the finding — if they do, they are rows, not classes.
    "weapons": [
        f"{T}/Blueprints/Weapons/BP_FPST_BaseWeapon",
        f"{T}/Blueprints/Weapons/BP_FPST_Weapon_Rifle",
        f"{T}/Blueprints/Weapons/BP_FPST_Weapon_Pistol",
        f"{T}/Blueprints/Weapons/BP_FPST_Weapon_Shotgun",
        f"{T}/Blueprints/Weapons/BP_FPST_Weapon_Knife",
    ],
    # CONTENT, not logic. Extracted only to prove they are content: if a layer ABP's CDO
    # carries no variables, it holds poses and nothing else, and it stays an asset untouched.
    "anim_layers": [
        f"{T}/Demo/Characters/Heroes/Mannequin/Animations/Locomotion/Rifle/ABP_RifleAnimLayers",
        f"{T}/Demo/Characters/Heroes/Mannequin/Animations/Locomotion/Pistol/ABP_PistolAnimLayers",
        f"{T}/Demo/Characters/Heroes/Mannequin/Animations/Locomotion/Unarmed/ABP_UnarmedAnimLayers",
    ],
}


def cdo(path: str) -> str:
    """`/Game/X/BP_Y` -> the generated class's default object.

    The `_C` suffix and the `Default__` prefix are both required. Without them ObjectTools
    resolves the Blueprint ASSET, whose properties are editor metadata (parent class, category,
    description) rather than the class's variables — it succeeds and returns the wrong thing,
    which is the failure mode worth naming because it looks like success.
    """
    name = path.rsplit("/", 1)[-1]
    return f"{path}.Default__{name}_C"


def extract_one(m: MCP, path: str) -> dict:
    rec: dict = {"path": path, "found": False, "properties": {}, "notes": []}

    exists, _ = m.call(ASSET, "exists", {"path": path})
    if not exists:
        rec["notes"].append("asset not found at this path")
        return rec
    rec["found"] = True

    instance = {"refPath": cdo(path)}
    names, txt = m.call(OBJ, "list_properties", {"instance": instance})
    if names is None:
        rec["notes"].append(f"list_properties failed: {txt[:200]}")
        return rec

    try:
        keys = names if isinstance(names, list) else json.loads(names)
    except Exception:
        keys = []
        rec["notes"].append(f"could not parse the property list: {str(names)[:200]}")
    rec["property_count"] = len(keys)

    # Values in one call. A per-key loop is 200 round-trips against a live editor for one
    # Blueprint and there is no reason for it.
    if keys:
        got, txt = m.call(OBJ, "get_properties", {"instance": instance, "properties": keys})
        try:
            rec["properties"] = json.loads(got) if isinstance(got, str) else (got or {})
        except Exception:
            rec["notes"].append(f"could not parse values: {str(got)[:200]}")
    return rec


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--set", choices=sorted(SETS), help="extract one set instead of all")
    ap.add_argument("--list", action="store_true", help="print the sets and exit; no editor")
    ap.add_argument("--out", default="mcp-bp/bp_inventory.json")
    args = ap.parse_args()

    todo = {args.set: SETS[args.set]} if args.set else SETS

    if args.list:
        for name, paths in todo.items():
            print(f"  {name:12} {len(paths):2} asset(s)")
            for p in paths:
                print(f"      {p}")
        return EXIT_PASS

    m = MCP()
    try:
        m.init()
    except Exception as e:
        print(f"BLOCKED — no MCP server: {e}")
        print("Nothing was read. This tool requires a LIVE editor with the project open.")
        return EXIT_BLOCKED

    out: dict = {"sets": {}}
    for name, paths in todo.items():
        out["sets"][name] = [extract_one(m, p) for p in paths]
        found = sum(1 for r in out["sets"][name] if r["found"])
        print(f"  {name:12} {found}/{len(paths)} found")

    dest = REPO / args.out
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_text(json.dumps(out, indent=1, sort_keys=True), encoding="utf-8")
    print(f"\nwrote {dest}")
    print("COMMIT IT. A Blueprint that has never been diffable now is; that is the deliverable,")
    print("not the C++ that comes later.")
    return EXIT_PASS


if __name__ == "__main__":
    raise SystemExit(main())
