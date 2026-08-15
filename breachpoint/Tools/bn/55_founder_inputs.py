"""Remap melee to F and print the live IMC + DT_BNWeapons montage table.

    py "Tools/bn/55_founder_inputs.py"

Does not rebuild IMC_BNNext. 10_input_assets.py is the full source of truth;
this is the surgical F remap + a read-back of fire/melee montages per weapon row.
"""
import sys

import unreal

IMC_PATH = "/Game/BN/Input/IMC_BNNext"
MELEE_ACTION = "/Game/BN/Input/IA_BN_Melee.IA_BN_Melee"
DT_PATH = "/Game/BN/Data/DT_BNWeapons"
INTENT_MELEE_KEY = "F"


def _key_name(key):
    try:
        return str(key.get_editor_property("key_name"))
    except Exception:
        return str(getattr(key, "key_name", key))


def _path(value):
    if value is None:
        return "None"
    if hasattr(value, "get_path_name"):
        return value.get_path_name()
    return str(value)


def _soft(value):
    if value is None:
        return "None"
    if hasattr(value, "to_soft_object_path"):
        path = value.to_soft_object_path()
        return path.to_string() if path else "None"
    if hasattr(value, "get_asset_path_string"):
        return value.get_asset_path_string() or "None"
    return str(value)


def remap_melee():
    imc = unreal.EditorAssetLibrary.load_asset(IMC_PATH)
    if imc is None:
        unreal.log_error("MISSING: %s" % IMC_PATH)
        return False

    data = imc.get_editor_property("default_key_mappings")
    mappings = list(data.get_editor_property("mappings")) if data else []
    changed = 0
    for mapping in mappings:
        if _path(mapping.get_editor_property("action")) != MELEE_ACTION:
            continue
        current = _key_name(mapping.get_editor_property("key"))
        if current == INTENT_MELEE_KEY:
            continue
        key = unreal.Key()
        key.set_editor_property("key_name", INTENT_MELEE_KEY)
        mapping.set_editor_property("key", key)
        changed += 1
        unreal.log("MELEE KEY: %s -> %s" % (current, INTENT_MELEE_KEY))

    if changed:
        data.set_editor_property("mappings", mappings)
        imc.set_editor_property("default_key_mappings", data)
        if not unreal.EditorAssetLibrary.save_asset(IMC_PATH):
            unreal.log_error("SAVE FAILED: %s" % IMC_PATH)
            return False

    unreal.log("== IMC mappings ==")
    for mapping in mappings:
        unreal.log("  %s  ->  %s" % (
            _path(mapping.get_editor_property("action")).rsplit("/", 1)[-1],
            _key_name(mapping.get_editor_property("key"))))
    return True


def audit_weapon_rows():
    table = unreal.EditorAssetLibrary.load_asset(DT_PATH)
    if table is None:
        unreal.log_error("MISSING: %s" % DT_PATH)
        return False

    names = list(unreal.DataTableFunctionLibrary.get_data_table_row_names(table))
    unreal.log("== DT_BNWeapons rows (%d) ==" % len(names))
    for name in names:
        row = None
        for getter in (
            lambda: table.find_row(name),
            lambda: unreal.DataTableFunctionLibrary.get_data_table_row(table, name),
        ):
            try:
                row = getter()
                if row is not None:
                    break
            except Exception:
                row = None
        if row is None:
            unreal.log("  %s  (row struct unread — name present)" % name)
            continue
        unreal.log("  %s  fire=%s  reload=%s  melee=%s  layer=%s  ads=%s" % (
            name,
            _soft(row.get_editor_property("fire_montage")),
            _soft(row.get_editor_property("reload_montage")),
            _soft(row.get_editor_property("melee_montage")),
            _soft(row.get_editor_property("anim_layer_class")),
            row.get_editor_property("b_can_ads")))
    return True


def main():
    ok = remap_melee()
    ok = audit_weapon_rows() and ok
    return 0 if ok else 1


sys.exit(main())
