#!/usr/bin/env python3
# =====================================================================================
# BREACHPOINT - selftest_no_editor.py :: exercise build_input.py with NO editor
#
# Serves:   TICKET BP01 step 3b. Runs in plain CPython:  python Tools/gen_input/selftest_no_editor.py
#
# WHAT THIS IS, AND WHAT IT IS NOT (honesty ladder, stated up front).
# It stands in a FAKE `unreal` module and drives build_input.py against an in-memory
# pretend editor. It therefore proves the generator's CONTROL FLOW and DECISIONS:
#   1. a first run creates the eight missing actions, the config and the mappings;
#   2. a second run writes NOTHING (idempotency) - the property the packet asks for;
#   3. a hand-edited Pressed trigger on an ability action is DETECTED, named, repaired,
#      and fails the run (the trigger contract asserted, not merely applied);
#   4. a key already mapped to a foreign action is refused, not silently double-bound;
#   5. an unregistered gameplay tag stops the run instead of writing tagless rows.
#
# It proves NOTHING about the real engine: not one Python binding name, not one .uasset
# byte, not FKey validity, not whether UInputAction's factory behaves as documented. Those
# are editor-rung facts and this packet was forbidden from launching an editor (a human
# holds the project lock). The fake is written to the engine headers this script was
# authored against (UE 5.8: EnhancedInput/InputAction.h, InputMappingContext.h,
# InputEditorModule.h, GameplayTagContainer.h) - if the real bindings differ, build_input.py
# is written to FAIL LOUDLY on the difference rather than half-write an asset, and that is
# the behaviour this test cannot check for you.
#
# Exit codes: 0 all cases pass · 1 a case failed.
# =====================================================================================
"""No-editor self-test for the input generator's logic."""

import copy
import os
import shutil
import sys
import tempfile
import types

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))

REGISTERED_TAGS = {
    "InputTag.Move", "InputTag.Look", "InputTag.Jump", "InputTag.Crouch",
    "InputTag.Sprint", "InputTag.Fire", "InputTag.Reload", "InputTag.Swap",
    "InputTag.Grenade", "InputTag.Melee", "InputTag.Grapple",
}

VALID_KEYS = {
    "LeftControl", "C", "LeftShift", "LeftMouseButton", "R", "Q", "G", "V", "E",
    "MiddleMouseButton", "MouseScrollUp", "MouseScrollDown", "SpaceBar", "W", "A", "S", "D",
}


# =====================================================================================
# the fake editor
# =====================================================================================

class FakeClass(object):
    def __init__(self, name):
        self._name = name

    def get_name(self):
        return self._name


class FakeObject(object):
    """Anything with get/set_editor_property. Property names are the python-cased ones."""

    def __init__(self, class_name, path=""):
        self._class = FakeClass(class_name)
        self._path = path
        self._props = {}

    def get_class(self):
        return self._class

    def get_name(self):
        return self._path.rsplit("/", 1)[-1].split(".")[0]

    def get_path_name(self):
        return self._path

    def get_editor_property(self, name):
        if name not in self._props:
            raise Exception("no property '%s' on %s" % (name, self._class.get_name()))
        return self._props[name]

    def set_editor_property(self, name, value):
        self._props[name] = value

    def modify(self):
        return True


class FakeStruct(FakeObject):
    def export_text(self):
        return str(self._props)


class FakeTag(FakeStruct):
    def __init__(self):
        FakeStruct.__init__(self, "GameplayTag")
        self._props["tag_name"] = "None"

    def import_text(self, text):
        name = text.strip().strip('"')
        if name.startswith("("):  # (TagName="x")
            name = name.split('"')[1]
        if name in REGISTERED_TAGS:
            self._props["tag_name"] = name
            return True
        self._props["tag_name"] = "None"
        return False


class FakeRow(FakeStruct):
    def __init__(self):
        FakeStruct.__init__(self, "BRInputAction")
        self._props["input_action"] = None
        self._props["input_tag"] = FakeTag()

    def import_text(self, text):
        # (InputAction="/Game/...IA_Fire",InputTag=(TagName="InputTag.Fire"))
        try:
            action = text.split('InputAction="', 1)[1].split('"', 1)[0]
            tag_name = text.split('TagName="', 1)[1].split('"', 1)[0]
        except IndexError:
            return False
        self._props["input_action"] = action
        t = FakeTag()
        if not t.import_text(tag_name):
            return False
        self._props["input_tag"] = t
        return True


class FakeKey(FakeStruct):
    def __init__(self, name=""):
        FakeStruct.__init__(self, "Key")
        self._props["key_name"] = str(name)


class FakeMapping(FakeStruct):
    def __init__(self, action, key):
        FakeStruct.__init__(self, "EnhancedActionKeyMapping")
        self._props["action"] = action
        self._props["key"] = key


class FakeMappingData(FakeStruct):
    def __init__(self):
        FakeStruct.__init__(self, "InputMappingContextMappingData")
        self._props["mappings"] = []


class FakeIMC(FakeObject):
    def __init__(self, path):
        FakeObject.__init__(self, "InputMappingContext", path)
        self._props["default_key_mappings"] = FakeMappingData()

    def _list(self):
        return self._props["default_key_mappings"].get_editor_property("mappings")

    def map_key(self, action, key):
        m = FakeMapping(action, key)
        self._list().append(m)
        return m

    def unmap_all_keys_from_action(self, action):
        keep = [m for m in self._list()
                if m.get_editor_property("action") is not action]
        self._props["default_key_mappings"].set_editor_property("mappings", keep)


class FakeEditor(object):
    """The package registry, a write ledger so the test can assert 'wrote nothing', and a
    throwaway content directory.

    The content directory is real because build_input's post-write verification demands an
    on-disk .uasset newer than the run that claims it - this project's answer to a verifier
    that once reported PASS on a stale binary. A fake that never touched the filesystem
    would have made that check silently untestable, which is the opposite of the point.
    """

    def __init__(self):
        self.packages = {}
        self.saves = []
        self.creates = []
        self.content_dir = tempfile.mkdtemp(prefix="br_geninput_selftest_")

    def add(self, package, obj):
        self.packages[package] = obj
        return obj

    def write_file(self, package):
        rel = package[len("/Game/"):] if package.startswith("/Game/") else package.lstrip("/")
        path = os.path.join(self.content_dir, rel + ".uasset")
        d = os.path.dirname(path)
        if not os.path.isdir(d):
            os.makedirs(d)
        with open(path, "w", encoding="utf-8") as fh:
            fh.write("fake uasset for %s\n" % package)
        return path


TEMP_DIRS = []
EDITOR = FakeEditor()
TEMP_DIRS.append(EDITOR.content_dir)


def build_fake_unreal():
    u = types.ModuleType("unreal")

    u.log = lambda m: sys.stdout.write("      LOG   %s\n" % m)
    u.log_warning = lambda m: sys.stdout.write("      WARN  %s\n" % m)
    u.log_error = lambda m: sys.stdout.write("      ERROR %s\n" % m)

    class Paths(object):
        @staticmethod
        def convert_relative_path_to_full(p):
            return p

        @staticmethod
        def project_dir():
            return REPO

        @staticmethod
        def project_content_dir():
            return EDITOR.content_dir
    u.Paths = Paths

    class InputActionValueType(object):
        BOOLEAN = "Boolean"
        AXIS1_D = "Axis1D"
        AXIS2_D = "Axis2D"
        AXIS3_D = "Axis3D"
    u.InputActionValueType = InputActionValueType

    u.InputAction = FakeClass("InputAction")
    u.InputAction_Factory = lambda: FakeObject("InputAction_Factory")
    u.DataAssetFactory = lambda: FakeObject("DataAssetFactory")
    u.BRInputConfig = FakeClass("BRInputConfig")
    u.BRInputAction = FakeRow
    u.GameplayTag = FakeTag
    u.Key = FakeKey
    u.SoftObjectPath = lambda s: s
    u.Name = lambda s: str(s)

    for t in ("Down", "Pressed", "Released", "Hold", "Tap", "Pulse"):
        setattr(u, "InputTrigger" + t, FakeClass("InputTrigger" + t))

    def new_object(cls, outer=None, **kwargs):
        return FakeObject(cls.get_name())
    u.new_object = new_object

    class KismetInputLibrary(object):
        @staticmethod
        def key_is_valid(key):
            return key.get_editor_property("key_name") in VALID_KEYS
    u.KismetInputLibrary = KismetInputLibrary

    class EditorAssetLibrary(object):
        @staticmethod
        def does_asset_exist(p):
            return p in EDITOR.packages

        @staticmethod
        def load_asset(p):
            return EDITOR.packages.get(p)

        @staticmethod
        def save_loaded_asset(asset, only_if_dirty=True):
            EDITOR.saves.append(asset.get_path_name())
            EDITOR.write_file(asset.get_path_name())
            return True
    u.EditorAssetLibrary = EditorAssetLibrary

    class AssetTools(object):
        @staticmethod
        def create_asset(name, path, asset_class, factory):
            package = "%s/%s" % (path, name)
            cls_name = asset_class.get_name()
            obj = FakeIMC(package) if cls_name == "InputMappingContext" \
                else FakeObject(cls_name, package)
            if cls_name == "InputAction":
                obj.set_editor_property("value_type", InputActionValueType.BOOLEAN)
                obj.set_editor_property("triggers", [])
            if cls_name == "BRInputConfig":
                obj.set_editor_property("native_input_actions", [])
                obj.set_editor_property("ability_input_actions", [])
            EDITOR.creates.append(package)
            return EDITOR.add(package, obj)

    class AssetToolsHelpers(object):
        @staticmethod
        def get_asset_tools():
            return AssetTools()
    u.AssetToolsHelpers = AssetToolsHelpers

    u.EditorValidatorSubsystem = None  # exercised as 'plugin absent' - reported, not hidden
    u.get_editor_subsystem = lambda cls: None
    return u


# =====================================================================================
# scenario plumbing
# =====================================================================================

def reset_world(with_template=True, imc_extra=None):
    """A fresh pretend project: the four template assets, and IMC_Default with its
    template mappings. Mirrors what is actually on disk in Content/Input today."""
    global EDITOR
    EDITOR = FakeEditor()
    TEMP_DIRS.append(EDITOR.content_dir)
    if not with_template:
        return
    for name, vt in (("IA_Move", "Axis2D"), ("IA_Look", "Axis2D"), ("IA_Jump", "Boolean"),
                     ("IA_MouseLook", "Axis2D")):
        p = "/Game/Input/Actions/%s" % name
        o = FakeObject("InputAction", p)
        o.set_editor_property("value_type", vt)
        o.set_editor_property("triggers", [])
        EDITOR.add(p, o)
    imc = FakeIMC("/Game/Input/IMC_Default")
    for action_name, key in (("IA_Move", "W"), ("IA_Move", "A"), ("IA_Move", "S"),
                             ("IA_Move", "D"), ("IA_Jump", "SpaceBar")):
        imc.map_key(EDITOR.packages["/Game/Input/Actions/%s" % action_name], FakeKey(key))
    for action_name, key in (imc_extra or []):
        imc.map_key(EDITOR.packages["/Game/Input/Actions/%s" % action_name], FakeKey(key))
    EDITOR.add("/Game/Input/IMC_Default", imc)


class Args(object):
    def __init__(self, **kw):
        self.profile = None
        self.dry_run = False
        self.allow_drift = False
        self.no_lock_check = True
        self.report_dir = None
        self.__dict__.update(kw)


def run(bi, plan, args=None):
    """Drive the three phases + verify, exactly as build_input.main does."""
    args = args or Args()
    findings, report = [], {}
    lock = bi.LockGate(REPO, enabled=False)
    try:
        assets = bi.build_actions(plan, args, lock, findings, report)
        cfg = bi.build_config(plan, assets, args, lock, findings, report)
        imc = bi.build_mapping_context(plan, assets, args, lock, findings, report)
        ok = bi.verify(plan, assets, cfg, imc, 0, findings, report,
                       check_mappings=not args.dry_run)
    except bi.Refused as exc:
        return {"refused": str(exc), "findings": findings, "report": report}
    return {"refused": None, "ok": ok, "findings": findings, "report": report,
            "assets": assets, "imc": imc}


def codes(result, severity=None):
    return [c for s, c, _m in result["findings"] if severity is None or s == severity]


# =====================================================================================
# the cases
# =====================================================================================

CASES = []


def case(fn):
    CASES.append(fn)
    return fn


@case
def case_first_run_creates_the_eight(bi, plan):
    """A fresh project gains exactly the eight missing actions + config; nothing else."""
    reset_world()
    r = run(bi, plan)
    assert r["refused"] is None, r["refused"]
    created = r["report"].get("created", [])
    names = sorted(c.rsplit("/", 1)[-1] for c in created)
    expected = sorted(["IA_Crouch", "IA_Sprint", "IA_Fire", "IA_Reload", "IA_Swap",
                       "IA_Grenade", "IA_Melee", "IA_Grapple", "DA_InputConfig"])
    assert names == expected, "created %s" % names
    assert "/Game/Input/Actions/IA_MouseLook" not in created
    mapped = [(m.get_editor_property("action").get_name(),
               m.get_editor_property("key").get_editor_property("key_name"))
              for m in r["imc"]._list()]
    assert ("IA_Move", "W") in mapped, "template mappings must survive: %s" % mapped
    assert ("IA_Jump", "SpaceBar") in mapped
    assert ("IA_Fire", "LeftMouseButton") in mapped
    assert len([m for m in mapped if m[0] == "IA_Grapple"]) == 2
    assert r["ok"] is True
    return "created %d asset(s), %d mapping(s) live, template mappings intact" \
           % (len(created), len(mapped))


@case
def case_second_run_writes_nothing(bi, plan):
    """IDEMPOTENCY: run twice on one world; the second run must save NOTHING."""
    reset_world()
    run(bi, plan)
    first_saves = list(EDITOR.saves)
    EDITOR.saves = []
    EDITOR.creates = []
    r = run(bi, plan)
    assert r["refused"] is None, r["refused"]
    assert EDITOR.creates == [], "re-run created %s" % EDITOR.creates
    assert EDITOR.saves == [], "re-run saved %s" % EDITOR.saves
    assert not codes(r, "error"), codes(r, "error")
    mapped = [(m.get_editor_property("action").get_name(),
               m.get_editor_property("key").get_editor_property("key_name"))
              for m in r["imc"]._list()]
    assert len(mapped) == len(set(mapped)), "duplicate mappings after re-run: %s" % mapped
    assert len([m for m in mapped if m[0] == "IA_Fire"]) == 1
    return "run 1 saved %d, run 2 saved 0, no duplicate mappings" % len(first_saves)


@case
def case_hand_edited_pressed_trigger_is_caught(bi, plan):
    """THE TRIGGER CONTRACT, asserted: a Pressed trigger added by hand after a clean run is
    detected by name, repaired, and fails the run so the edit cannot pass silently."""
    reset_world()
    run(bi, plan)
    EDITOR.saves = []
    sprint = EDITOR.packages["/Game/Input/Actions/IA_Sprint"]
    sprint.set_editor_property("triggers", [FakeObject("InputTriggerPressed")])
    r = run(bi, plan)
    assert r["refused"] is None, r["refused"]
    assert "CONTRACT_VIOLATION_ON_DISK" in codes(r, "error"), codes(r)
    msg = [m for s, c, m in r["findings"] if c == "CONTRACT_VIOLATION_ON_DISK"][0]
    assert "IA_Sprint" in msg and "Pressed" in msg
    assert [bi.trigger_short_name(t) for t in sprint.get_editor_property("triggers")] == []
    assert "/Game/Input/Actions/IA_Sprint" in EDITOR.saves, "repair was not saved"
    assert r["ok"] is True, "post-write verification should pass once repaired"
    return "detected, named, repaired and reported as an error (run exits 1 without --allow-drift)"


@case
def case_unknown_trigger_is_not_assumed_innocent(bi, plan):
    """A Blueprint/unknown trigger class on an ability action is a violation, not a shrug."""
    reset_world()
    run(bi, plan)
    fire = EDITOR.packages["/Game/Input/Actions/IA_Fire"]
    fire.set_editor_property("triggers", [FakeObject("BP_WeirdTrigger_C")])
    r = run(bi, plan)
    assert "CONTRACT_VIOLATION_ON_DISK" in codes(r, "error"), codes(r)
    return "unknown trigger class treated as a contract violation"


@case
def case_foreign_key_collision_is_refused(bi, plan):
    """A key already bound to an action this generator does not own is reported, not
    silently double-bound. (Here: the template maps IA_MouseLook to R.)"""
    reset_world(imc_extra=[("IA_MouseLook", "R")])
    r = run(bi, plan)
    assert "KEY_COLLISION_LIVE" in codes(r, "error"), codes(r)
    msg = [m for s, c, m in r["findings"] if c == "KEY_COLLISION_LIVE"][0]
    assert "IA_Reload" in msg and "IA_MouseLook" in msg
    return "live collision on 'R' found against a mapping the generator does not own"


@case
def case_unregistered_tag_stops_the_run(bi, plan):
    """If the editor does not carry BRGameplayTags' native tags, we refuse rather than
    writing rows whose tag is None - the failure that would otherwise look like a dead key."""
    reset_world()
    REGISTERED_TAGS.discard("InputTag.Grapple")
    try:
        r = run(bi, plan)
    finally:
        REGISTERED_TAGS.add("InputTag.Grapple")
    assert r["refused"] is not None, "run should have been refused"
    assert "InputTag.Grapple" in r["refused"]
    return "refused: %s" % r["refused"].split(".")[0]


@case
def case_template_actions_are_never_remapped(bi, plan):
    """generator_owns_mappings=false means the template's Move/Look/Jump bindings are not
    read for equality and never rewritten - even if they look 'wrong'."""
    reset_world()
    run(bi, plan)
    EDITOR.saves = []
    imc = EDITOR.packages["/Game/Input/IMC_Default"]
    imc.map_key(EDITOR.packages["/Game/Input/Actions/IA_Move"], FakeKey("MouseScrollUp"))
    before = len(imc._list())
    r = run(bi, plan)
    assert r["refused"] is None, r["refused"]
    assert EDITOR.saves == [], "a foreign mapping change must not trigger a rewrite"
    assert len(imc._list()) == before
    return "an unrelated Move mapping neither triggered a write nor was removed"


@case
def case_dry_run_writes_nothing(bi, plan):
    reset_world()
    r = run(bi, plan, Args(dry_run=True))
    assert r["refused"] is None, r["refused"]
    assert EDITOR.saves == [], "dry run saved %s" % EDITOR.saves
    return "no saves in --dry-run"


# =====================================================================================
# main
# =====================================================================================

def main():
    sys.modules["unreal"] = build_fake_unreal()
    sys.path.insert(0, HERE)
    import input_plan
    import build_input as bi

    plan, _profile = input_plan.load_all()
    if plan["errors"]:
        sys.stderr.write("the committed profile does not validate; fix that first.\n")
        return 1

    print("=" * 86)
    print("BREACHPOINT input generator :: NO-EDITOR self-test (fake `unreal`)")
    print("=" * 86)
    print("plan digest : %s" % plan["digest"])
    print("")

    failures = 0
    for fn in CASES:
        name = fn.__name__.replace("case_", "")
        try:
            detail = fn(bi, copy.deepcopy(plan))
            print("PASS  %-38s %s" % (name, detail or ""))
        except AssertionError as exc:
            failures += 1
            print("FAIL  %-38s %s" % (name, exc))
        except Exception as exc:  # noqa: BLE001
            failures += 1
            import traceback
            print("ERROR %-38s %s" % (name, exc))
            traceback.print_exc()

    for d in TEMP_DIRS:
        shutil.rmtree(d, ignore_errors=True)

    print("")
    print("%d/%d cases passed." % (len(CASES) - failures, len(CASES)))
    print("RUNG: this is a logic test against a FAKE editor. It proves the generator's")
    print("decisions, not one line of real Unreal behaviour. The editor rung is owed.")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
