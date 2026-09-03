"""BREACHPOINT NEXT - input assets + read-back audit (Roadmap 1, Goal 7.3/4.1/4.2).

Run inside the UE 5.8 editor (Python Editor Script Plugin):

    py "Tools/bn/10_input_assets.py"

Packet run order: 10_input_assets.py -> 20_bp_character.py -> 30_reparent_abp.py
-> 40_unarmed_layer.py. 10 touches only input and is independent of the other three.

Creates the two assets ABNPlayerController soft-references out of DefaultGame.ini:

  /Game/BN/Input/IMC_BNNext   the mapping context — move/look/combat/lean/swap
  /Game/BN/Input/DA_BNInput   the UBNInputConfig: InputAction <-> Input.* tag pairs

InputActions are REUSED from /Game/FPSTemplate/Input/Actions (Roadmap 1's reuse
verdict) when the asset exists AND its value type is the one the handler reads;
otherwise a BN-owned IA is created next to the IMC. Which route each action took
is an audit row, so a silent substitution cannot happen.

Then it reads everything back from the live editor - including the
ABNPlayerController CDO, whose Config properties come from DefaultGame.ini - and
prints an intent-vs-actual diff table. The CDO rows are the ones that matter: they
prove the shipped controller resolves these assets, not just that the assets exist.
The audit table is the proof, not "the script ran". Idempotent: a second run finds
everything set and re-audits.

Exits non-zero if the audit diffs, so a caller can gate on it.
"""
import sys

import unreal

BN_INPUT_PATH = "/Game/BN/Input"
IMC_NAME = "IMC_BNNext"
IMC_PATH = BN_INPUT_PATH + "/" + IMC_NAME
CONFIG_NAME = "DA_BNInput"
CONFIG_PATH = BN_INPUT_PATH + "/" + CONFIG_NAME
CONFIG_CLASS_PATH = "/Script/BreachpointNext.BNInputConfig"
PC_CLASS_PATH = "/Script/BreachpointNext.BNPlayerController"
REUSE_ROOT = "/Game/FPSTemplate/Input/Actions"

# Modifier shorthand used in KEYS below: (python class name, {property: value}).
SWIZZLE = ("InputModifierSwizzleAxis", {})               # default order YXZ: X<->Y
NEGATE_ALL = ("InputModifierNegate", {})
NEGATE_Y = ("InputModifierNegate", {"x": False, "y": True, "z": False})

# GAMEPAD (founder, 3 Sep 2026). A stick rests at a small non-zero magnitude, so an
# undeadzoned Left2D walks the player and an undeadzoned Right2D drifts the camera --
# and a drifting camera on the MAIN MENU is how a controller "presses" buttons nobody
# touched. 0.25 matches the legacy AxisConfig dead zones in DefaultInput.ini (which
# Enhanced Input ignores, so it has to be restated here as a modifier).
DEADZONE = ("InputModifierDeadZone", {"lower_threshold": 0.25, "upper_threshold": 1.0})
# Stick look is a RATE, mouse look is a DELTA, so the stick needs its own gain. 1.75 is a
# starting value, not a measured one -- it is the one number here that wants a feel pass.
LOOK_SCALAR = ("InputModifierScalar", {"scalar": unreal.Vector(1.75, 1.75, 1.0)})

# Full combat set. Move reads Axis.Y as forward, so W/S are swizzled onto Y;
# HandleLook feeds AddPitchInput, so Look's Y is negated. Melee is F (founder).
ACTIONS = [
    {
        "id": "Move",
        "tag": "Input.Move",
        "value_type": "AXIS2D",
        "reuse": REUSE_ROOT + "/IA_FPST_Move",
        "keys": [("W", [SWIZZLE]), ("S", [SWIZZLE, NEGATE_ALL]),
                 ("D", []), ("A", [NEGATE_ALL]),
                 # No SWIZZLE: Gamepad_Left2D already reports X=right, Y=forward.
                 ("Gamepad_Left2D", [DEADZONE])],
    },
    {
        "id": "Look",
        "tag": "Input.Look",
        "value_type": "AXIS2D",
        "reuse": REUSE_ROOT + "/IA_FPST_Look",
        # NEGATE_Y for the same reason the mouse needs it: HandleLook feeds AddPitchInput,
        # where positive pitches DOWN, and a stick pushed up must look UP.
        "keys": [("Mouse2D", [NEGATE_Y]),
                 ("Gamepad_Right2D", [DEADZONE, LOOK_SCALAR, NEGATE_Y])],
    },
    {
        "id": "Jump",
        "tag": "Input.Jump",
        "value_type": "BOOLEAN",
        "reuse": REUSE_ROOT + "/IA_FPST_Jump",
        "keys": [("SpaceBar", []), ("Gamepad_FaceButton_Bottom", [])],  # A / cross
    },
    {
        "id": "Crouch",
        "tag": "Input.Crouch",
        "value_type": "BOOLEAN",
        "reuse": REUSE_ROOT + "/IA_FPST_Crouch",
        "keys": [("LeftControl", []), ("Gamepad_RightThumbstick", [])],  # R3
    },
    {
        "id": "Sprint",
        "tag": "Input.Sprint",
        "value_type": "BOOLEAN",
        "reuse": REUSE_ROOT + "/IA_FPST_Sprint",
        "keys": [("LeftShift", []), ("Gamepad_LeftThumbstick", [])],  # L3
    },
    {
        "id": "Fire",
        "tag": "Input.Weapon.Fire",
        "value_type": "BOOLEAN",
        "reuse": REUSE_ROOT + "/IA_FPST_Weapon_Fire",
        "keys": [("LeftMouseButton", []), ("Gamepad_RightTrigger", [])],  # R2
    },
    {
        "id": "Reload",
        "tag": "Input.Weapon.Reload",
        "value_type": "BOOLEAN",
        "reuse": REUSE_ROOT + "/IA_FPST_Weapon_Reload",
        "keys": [("R", []), ("Gamepad_FaceButton_Left", [])],  # X / square
    },
    {
        "id": "ADS",
        "tag": "Input.Weapon.ADS",
        "value_type": "BOOLEAN",
        "reuse": REUSE_ROOT + "/IA_FPST_Aim",
        "keys": [("RightMouseButton", []), ("Gamepad_LeftTrigger", [])],  # L2
    },
    {
        "id": "LeanLeft",
        "tag": "Input.Lean.Left",
        "value_type": "BOOLEAN",
        "reuse": BN_INPUT_PATH + "/IA_BN_LeanLeft",
        "keys": [("Q", []), ("Gamepad_DPad_Left", [])],  # dpad left
    },
    {
        "id": "LeanRight",
        "tag": "Input.Lean.Right",
        "value_type": "BOOLEAN",
        "reuse": BN_INPUT_PATH + "/IA_BN_LeanRight",
        "keys": [("E", []), ("Gamepad_DPad_Right", [])],  # dpad right
    },
    {
        "id": "WeaponNext",
        "tag": "Input.Weapon.Next",
        "value_type": "BOOLEAN",
        "reuse": BN_INPUT_PATH + "/IA_BNWeaponNext",
        # MouseScrollUp, not MouseWheelUp. UE's key names are MouseScrollUp/MouseScrollDown
        # (InputCoreTypes.cpp); "MouseWheelUp" is not a key at all, so the IMC entry looked
        # perfectly correct in the editor and could never fire. Bots were unaffected because
        # they call AbilityInputTagPressed directly and never touch Enhanced Input — which is
        # exactly why this survived: the logs showed 308 successful weapon swaps while the
        # founder's wheel did nothing.
        "keys": [("MouseScrollUp", []), ("Gamepad_FaceButton_Top", [])],  # Y / triangle
    },
    {
        "id": "WeaponPrevious",
        "tag": "Input.Weapon.Previous",
        "value_type": "BOOLEAN",
        "reuse": BN_INPUT_PATH + "/IA_BNWeaponPrevious",
        "keys": [("MouseScrollDown", []), ("Gamepad_DPad_Down", [])],  # dpad down
    },
    {
        "id": "Melee",
        "tag": "Input.Melee",
        "value_type": "BOOLEAN",
        "reuse": BN_INPUT_PATH + "/IA_BN_Melee",
        "keys": [("F", []), ("Gamepad_FaceButton_Right", [])],  # B / circle
    },
    {
        "id": "Grenade",
        "tag": "Input.Grenade",
        "value_type": "BOOLEAN",
        "reuse": BN_INPUT_PATH + "/IA_BN_Grenade",
        "keys": [("G", []), ("Gamepad_DPad_Up", [])],  # dpad up
    },
    {
        # SCOREBOARD AND MENU — RESTORED, and their absence was a self-inflicted wound.
        # This file's docstring calls itself "the full source of truth" for IMC_BNNext, and
        # it means it: every run REBUILDS the context from this table. Scoreboard and Menu
        # had IA assets on disk and live C++ handlers (BNPlayerController.cpp:97-99) but no
        # row here, so the first regeneration for an unrelated reason silently dropped both
        # bindings and Tab stopped opening the scoreboard. Nothing errored — the table is
        # the contract, and anything missing from it does not exist.
        #
        # Scoreboard is a HOLD: the controller binds Started AND Completed, so the board is
        # up only while the key is down.
        "id": "Scoreboard",
        "tag": "Input.Scoreboard",
        "value_type": "BOOLEAN",
        "reuse": BN_INPUT_PATH + "/IA_BNScoreboard",
        "keys": [("Tab", []), ("Gamepad_Special_Left", [])],  # Select / Share
    },
    {
        "id": "Menu",
        "tag": "Input.Menu",
        "value_type": "BOOLEAN",
        "reuse": BN_INPUT_PATH + "/IA_BNMenu",
        "keys": [("Escape", []), ("Gamepad_Special_Right", [])],  # Start / Options
    },
    {
        # THE GRAPPLE (founder, 28 Aug). UBNGA_Grapple and the Input.Grapple tag already
        # existed, and the ability is already granted in BNPlayerState — the ONLY missing
        # link was this row. With no IA asset, no key ever reached the tag, so the player
        # could not grapple while AIB19's bots could.
        #
        # "One" is UE's key name for the 1 key. Founder's call after G and Q both turned
        # out taken: G is Grenade, Q/E are LeanLeft/LeanRight. Double-binding any of them
        # would fire two abilities on a single press.
        "id": "Grapple",
        "tag": "Input.Grapple",
        "value_type": "BOOLEAN",
        "reuse": BN_INPUT_PATH + "/IA_BN_Grapple",
        "keys": [("One", []), ("Gamepad_LeftShoulder", [])],  # L1 - ability 1
    },
    {
        # THE DASH (founder, 29 Aug). "Two" is UE's key name for the 2 key, sitting beside
        # the grapple's One — the two mobility verbs on adjacent keys, which is how a player
        # learns them as a pair rather than as two unrelated bindings.
        #
        # Checked against every other row in this table before choosing: 2 is unbound.
        # Double-binding is the failure this whole generated table exists to prevent, since
        # a duplicate key fires two abilities on one press and neither looks broken alone.
        "id": "Dash",
        "tag": "Input.Dash",
        "value_type": "BOOLEAN",
        "reuse": BN_INPUT_PATH + "/IA_BN_Dash",
        "keys": [("Two", []), ("Gamepad_RightShoulder", [])],  # R1 - ability 2
    },
]

AUDIT_FMT = "%-38s | %-56s | %-56s | %s"


# --- reflection helpers: every UE-surface guess is tried, never assumed ----------

def _value_type(name):
    """EInputActionValueType - UE's python name mangling for Axis2D is not obvious."""
    enum = unreal.InputActionValueType
    for candidate in (name, name.replace("_", ""), name.title()):
        value = getattr(enum, candidate, None)
        if value is not None:
            return value
    raise RuntimeError("EInputActionValueType has no member like %r" % name)


def _make_key(key_name):
    # FKey's python constructor takes no arguments; key_name is Read-Write.
    key = unreal.Key()
    key.set_editor_property("key_name", key_name)
    return key


def _key_name_of(key):
    try:
        return str(key.get_editor_property("key_name"))
    except Exception:
        pass
    return str(getattr(key, "key_name", key))


def _make_tag(tag_name):
    # FGameplayTag.TagName is Read-Only and python has no RequestGameplayTag; the struct's
    # own text importer is the one route in. Round-tripped by _tag_name_of in the audit.
    tag = unreal.GameplayTag()
    tag.import_text('(TagName="%s")' % tag_name)
    return tag


def _tag_name_of(tag):
    try:
        return str(tag.get_editor_property("tag_name"))
    except Exception:
        return str(tag)


def _path_of(value):
    if value is None:
        return "None"
    if hasattr(value, "get_path_name"):
        return value.get_path_name()
    if hasattr(value, "to_string"):
        return value.to_string()
    return str(value)


def _asset_path(package_path):
    """/Game/X/Y -> /Game/X/Y.Y, the form DefaultGame.ini and get_path_name use."""
    return "%s.%s" % (package_path, package_path.rsplit("/", 1)[-1])


def _class_label(asset_class):
    """asset_class arrives either as a python type (unreal.InputAction) or as a
    unreal.Class instance (from load_class); only the latter has get_name()."""
    if isinstance(asset_class, type):
        return asset_class.__name__
    try:
        return str(asset_class.get_name())
    except Exception:
        return ""


def _create_asset(name, package_path, asset_class):
    """AssetTools accepts a null factory for plain UObject-derived assets; the
    dedicated factories are tried first only because they set editor metadata."""
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory_class = getattr(unreal, _class_label(asset_class) + "Factory", None)
    if factory_class is not None:
        try:
            return tools.create_asset(name, package_path, asset_class, factory_class())
        except Exception:
            pass
    return tools.create_asset(name, package_path, asset_class, None)


def _make_modifier(spec, outer):
    class_name, props = spec
    modifier = unreal.new_object(getattr(unreal, class_name), outer)
    for prop, value in props.items():
        modifier.set_editor_property(prop, value)
    return modifier


# --- the assets -------------------------------------------------------------------

def resolve_action(spec):
    """Returns (InputAction, route). Reuse only when the type is right - an IA whose
    value type disagrees with the handler is worse than a fresh one."""
    wanted = _value_type(spec["value_type"])
    reuse = spec["reuse"]
    if unreal.EditorAssetLibrary.does_asset_exist(reuse):
        action = unreal.EditorAssetLibrary.load_asset(reuse)
        if action is not None and action.get_editor_property("value_type") == wanted:
            return action, "reused %s" % reuse.rsplit("/", 1)[-1]

    own_name = "IA_BN" + spec["id"]
    own_path = BN_INPUT_PATH + "/" + own_name
    if unreal.EditorAssetLibrary.does_asset_exist(own_path):
        action = unreal.EditorAssetLibrary.load_asset(own_path)
        route = "BN-owned (existing)"
    else:
        action = _create_asset(own_name, BN_INPUT_PATH, unreal.InputAction)
        route = "BN-owned (created; reuse absent or wrong type)"
    if action is not None:
        action.set_editor_property("value_type", wanted)
        unreal.EditorAssetLibrary.save_asset(own_path)
    return action, route


def build_mapping_context(actions_by_id):
    if unreal.EditorAssetLibrary.does_asset_exist(IMC_PATH):
        imc = unreal.EditorAssetLibrary.load_asset(IMC_PATH)
    else:
        imc = _create_asset(IMC_NAME, BN_INPUT_PATH, unreal.InputMappingContext)
    if imc is None:
        unreal.log_error("CREATE FAILED: %s" % IMC_PATH)
        return None

    # Rebuilt from scratch every run: this script owns the whole mapping list, so
    # convergence beats merging (a hand-added mapping here would be unrecorded law).
    mappings = []
    for spec in ACTIONS:
        action = actions_by_id.get(spec["id"])
        if action is None:
            continue
        for key_name, modifier_specs in spec["keys"]:
            mapping = unreal.EnhancedActionKeyMapping()
            mapping.set_editor_property("action", action)
            mapping.set_editor_property("key", _make_key(key_name))
            mapping.set_editor_property(
                "modifiers", [_make_modifier(m, imc) for m in modifier_specs])
            mappings.append(mapping)

    # UE 5.8 deprecated the flat `mappings` array in favour of the DefaultKeyMappings
    # struct. Write the live one and empty the deprecated one, so a PostLoad migration
    # of leftovers cannot double every binding.
    data = unreal.InputMappingContextMappingData()
    data.set_editor_property("mappings", mappings)
    imc.set_editor_property("default_key_mappings", data)
    imc.set_editor_property("mappings", [])

    if not unreal.EditorAssetLibrary.save_asset(IMC_PATH):
        unreal.log_error("SAVE FAILED: %s" % IMC_PATH)
        return None
    return imc


def build_input_config(actions_by_id):
    config_class = unreal.load_class(None, CONFIG_CLASS_PATH)
    if config_class is None:
        unreal.log_error("CLASS LOAD FAILED: %s - the BreachpointNext C++ module is "
                         "not compiled. Nothing was created or saved." % CONFIG_CLASS_PATH)
        return None

    if unreal.EditorAssetLibrary.does_asset_exist(CONFIG_PATH):
        config = unreal.EditorAssetLibrary.load_asset(CONFIG_PATH)
    else:
        config = _create_asset(CONFIG_NAME, BN_INPUT_PATH, config_class)
    if config is None:
        unreal.log_error("CREATE FAILED: %s" % CONFIG_PATH)
        return None

    binding_struct = getattr(unreal, "BNInputBinding", None)
    if binding_struct is None:
        unreal.log_error("FBNInputBinding is not exposed to Python - add BlueprintType "
                         "to the USTRUCT in Source/BreachpointNext/Input/BNInputConfig.h "
                         "and re-run. Nothing was saved.")
        return None

    bindings = []
    for spec in ACTIONS:
        action = actions_by_id.get(spec["id"])
        if action is None:
            continue
        binding = binding_struct()
        binding.set_editor_property("input_action", action)
        binding.set_editor_property("input_tag", _make_tag(spec["tag"]))
        bindings.append(binding)
    config.set_editor_property("bindings", bindings)

    if not unreal.EditorAssetLibrary.save_asset(CONFIG_PATH):
        unreal.log_error("SAVE FAILED: %s" % CONFIG_PATH)
        return None
    return config


# --- read-back audit ---------------------------------------------------------------

def audit(routes):
    rows = []

    imc = unreal.EditorAssetLibrary.load_asset(IMC_PATH)
    config = unreal.EditorAssetLibrary.load_asset(CONFIG_PATH)
    rows.append(("IMC exists on disk", "True", str(imc is not None)))
    rows.append(("InputConfig exists on disk", "True", str(config is not None)))

    mappings = []
    if imc:
        data = imc.get_editor_property("default_key_mappings")
        mappings = list(data.get_editor_property("mappings")) if data else []
        rows.append(("deprecated `mappings` array empty", "0",
                     str(len(list(imc.get_editor_property("mappings"))))))
    bindings = list(config.get_editor_property("bindings")) if config else []

    for spec in ACTIONS:
        intent_keys = [k for k, _m in spec["keys"]]
        action_path = None
        for binding in bindings:
            if _tag_name_of(binding.get_editor_property("input_tag")) == spec["tag"]:
                action_path = _path_of(binding.get_editor_property("input_action"))
                break

        rows.append(("%s -> tag %s (%s)" % (spec["id"], spec["tag"], routes[spec["id"]]),
                     "bound", "bound" if action_path else "MISSING from InputConfig"))

        mine = [m for m in mappings
                if _path_of(m.get_editor_property("action")) == action_path]
        rows.append(("%s keys" % spec["id"],
                     ",".join(intent_keys),
                     ",".join(_key_name_of(m.get_editor_property("key")) for m in mine)))

        intent_mods = ",".join(
            "%s[%s]" % (k, "+".join(m[0].replace("InputModifier", "") for m in mods) or "-")
            for k, mods in spec["keys"])
        actual_mods = ",".join(
            "%s[%s]" % (_key_name_of(m.get_editor_property("key")),
                        "+".join(type(x).__name__.replace("InputModifier", "")
                                 for x in m.get_editor_property("modifiers")) or "-")
            for m in mine)
        rows.append(("%s modifiers" % spec["id"], intent_mods, actual_mods))

        if action_path:
            action = unreal.EditorAssetLibrary.load_asset(action_path.split(".")[0])
            actual_type = action.get_editor_property("value_type") if action else None
            rows.append(("%s value type" % spec["id"],
                         str(_value_type(spec["value_type"])), str(actual_type)))

    # The rows that matter: the shipped controller's Config properties, resolved from
    # DefaultGame.ini into the CDO. Assets existing proves nothing if these are empty.
    pc_class = unreal.load_class(None, PC_CLASS_PATH)
    if pc_class is None:
        rows.append(("BNPlayerController CDO", "loadable", "CLASS LOAD FAILED"))
    else:
        cdo = unreal.get_default_object(pc_class)
        rows.append(("CDO InputConfig (DefaultGame.ini)", _asset_path(CONFIG_PATH),
                     _path_of(cdo.get_editor_property("input_config"))))
        contexts = list(cdo.get_editor_property("mapping_contexts") or [])
        rows.append(("CDO MappingContexts (DefaultGame.ini)", _asset_path(IMC_PATH),
                     ",".join(_path_of(c) for c in contexts) or "(empty)"))

    unreal.log("== AUDIT: BreachpointNext input assets ==")
    unreal.log(AUDIT_FMT % ("CHECK", "INTENT", "ACTUAL", "STATUS"))
    diffs = 0
    for name, intent, actual in rows:
        ok = intent == actual
        diffs += 0 if ok else 1
        unreal.log(AUDIT_FMT % (name, intent, actual, "OK" if ok else "DIFF"))
    unreal.log("== AUDIT %s: %d/%d checks =="
               % ("PASSED" if diffs == 0 else "FAILED", len(rows) - diffs, len(rows)))
    return diffs == 0


def main():
    if not unreal.EditorAssetLibrary.does_directory_exist(BN_INPUT_PATH):
        unreal.EditorAssetLibrary.make_directory(BN_INPUT_PATH)

    actions_by_id, routes = {}, {}
    for spec in ACTIONS:
        action, route = resolve_action(spec)
        if action is None:
            unreal.log_error("INPUTACTION UNRESOLVED: %s. Nothing else was saved."
                             % spec["id"])
            return 1
        actions_by_id[spec["id"]] = action
        routes[spec["id"]] = route

    if build_mapping_context(actions_by_id) is None:
        return 1
    if build_input_config(actions_by_id) is None:
        return 1

    return 0 if audit(routes) else 1


sys.exit(main())
