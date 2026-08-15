"""Remove the dead Input.Debug.DamageSelf chain from the input assets.

    py "Tools/bn/56_remove_debug_damage_key.py"

The K-key take-damage binding and its BNDamageSelf exec were removed from the
source on 14 Aug 2026 (production rebuild); the tag has had zero code consumers
since. This strips the vestige from the asset side to match: the DA_BNInput row,
the IMC_BNNext K mapping, and the IA_BN_DebugDamageSelf action itself.
10_input_assets.py's DebugDamageSelf entry is removed in the same commit, so a
regenerate does not resurrect it. Restoring the key means restoring the code
first; the assets then return through 10_input_assets.py.
"""
import unreal

DA_PATH = "/Game/BN/Input/DA_BNInput"
IMC_PATH = "/Game/BN/Input/IMC_BNNext"
IA_PATH = "/Game/BN/Input/IA_BN_DebugDamageSelf"
IA_OBJECT = IA_PATH + ".IA_BN_DebugDamageSelf"
DEAD_TAG = "Input.Debug.DamageSelf"


def _path(value):
    if value is None:
        return "None"
    if hasattr(value, "get_path_name"):
        return value.get_path_name()
    return str(value)


def strip_da_row():
    da = unreal.EditorAssetLibrary.load_asset(DA_PATH)
    if da is None:
        unreal.log_error("MISSING: %s" % DA_PATH)
        return False
    bindings = list(da.get_editor_property("bindings"))
    kept = [b for b in bindings
            if str(b.get_editor_property("input_tag").get_editor_property("tag_name")) != DEAD_TAG]
    removed = len(bindings) - len(kept)
    if removed:
        da.set_editor_property("bindings", kept)
        unreal.EditorAssetLibrary.save_asset(DA_PATH)
    unreal.log("DA_BNInput: %d binding(s) removed (%d -> %d)" % (removed, len(bindings), len(kept)))
    return True


def strip_imc_mapping():
    imc = unreal.EditorAssetLibrary.load_asset(IMC_PATH)
    if imc is None:
        unreal.log_error("MISSING: %s" % IMC_PATH)
        return False
    # Same access shape as 55_founder_inputs.py: the mappings live behind
    # default_key_mappings on this engine build.
    data = imc.get_editor_property("default_key_mappings")
    mappings = list(data.get_editor_property("mappings")) if data else []
    kept = [m for m in mappings if _path(m.get_editor_property("action")) != IA_OBJECT]
    removed = len(mappings) - len(kept)
    if removed:
        data.set_editor_property("mappings", kept)
        unreal.EditorAssetLibrary.save_asset(IMC_PATH)
    unreal.log("IMC_BNNext: %d mapping(s) removed (%d -> %d)" % (removed, len(mappings), len(kept)))
    return True


def delete_ia():
    if not unreal.EditorAssetLibrary.does_asset_exist(IA_PATH):
        unreal.log("IA already gone: %s" % IA_PATH)
        return True
    refs = unreal.EditorAssetLibrary.find_package_referencers_for_asset(IA_PATH)
    if refs:
        unreal.log_error("IA still referenced, NOT deleting: %s" % list(refs))
        return False
    ok = unreal.EditorAssetLibrary.delete_asset(IA_PATH)
    unreal.log("IA_BN_DebugDamageSelf deleted: %s" % ok)
    return bool(ok)


def read_back():
    da = unreal.EditorAssetLibrary.load_asset(DA_PATH)
    tags = [str(b.get_editor_property("input_tag").get_editor_property("tag_name"))
            for b in da.get_editor_property("bindings")]
    unreal.log("DA_BNInput tags after: %s" % tags)
    imc = unreal.EditorAssetLibrary.load_asset(IMC_PATH)
    data = imc.get_editor_property("default_key_mappings")
    mappings = list(data.get_editor_property("mappings")) if data else []
    dead = [m for m in mappings if "DebugDamageSelf" in _path(m.get_editor_property("action"))]
    unreal.log("IMC mappings after: %d, dead refs: %d" % (len(mappings), len(dead)))
    return DEAD_TAG not in tags and not dead


ok = strip_da_row() and strip_imc_mapping() and delete_ia() and read_back()
unreal.log("56_remove_debug_damage_key: %s" % ("CLEAN" if ok else "FAILED"))
