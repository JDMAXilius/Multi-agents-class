#!/usr/bin/env python3
# =====================================================================================
# BREACHPOINT - selftest_no_editor.py :: exercise build_abilitysets.py with NO editor
#
# Serves:   TICKET BP20.  python Tools/gen_abilitysets/selftest_no_editor.py
#
# WHAT THIS IS, AND WHAT IT IS NOT (honesty ladder, stated up front).
# It stands in a FAKE `unreal` module and drives build_abilitysets.py against an in-memory
# pretend editor. It therefore proves the generator's CONTROL FLOW and DECISIONS:
#   1. a first run populates all four empty sets and clears GM_BR's four class overrides;
#   2. a second run writes NOTHING (idempotency) - the property BP20 needs, because this
#      generator will be re-run every time somebody wants to prove the assets are right;
#   3. a hand-edited set is DETECTED, named, repaired, and FAILS the run (drift asserted,
#      not merely applied);
#   4. an unregistered gameplay tag stops the run instead of writing tagless rows - which
#      is BP20's own bug, and the one failure this generator must never reintroduce;
#   5. an ability class the editor cannot resolve stops the run;
#   6. GM_BR is refused if it does not actually derive from ABRGameMode, because clearing
#      its overrides would then blank them rather than hand control to the C++ constructor;
#   7. a profile that puts InputTag.Jump in a set is refused with no editor at all.
#
# It proves NOTHING about the real engine: not one Python binding name, not one .uasset
# byte, not whether FBRAbilitySetEntry's import_text accepts the string this generator
# builds. Those are editor-rung facts, and BP20 was cut from a PIE run with the MCP bridge
# down. The fake is written to the headers this script was authored against (UE 5.8;
# Source/Breachpoint/AbilitySystem/BRAbilitySet.h as of 2 Aug 2026) - if the real bindings
# differ, build_abilitysets.py is written to FAIL LOUDLY on the difference rather than
# half-write an asset, and that is the behaviour this test cannot check for you.
#
# Exit codes: 0 all cases pass · 1 a case failed.
# =====================================================================================
"""No-editor self-test for the ability-set generator's logic."""

import copy
import json
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

RESOLVABLE_CLASSES = {
    "/Script/Breachpoint.BRGA_Sprint", "/Script/Breachpoint.BRGA_Melee",
    "/Script/Breachpoint.BRGA_Grenade", "/Script/Breachpoint.BRGA_Grapple",
    "/Script/Breachpoint.BRGA_WeaponFire", "/Script/Breachpoint.BRGA_Reload",
    "/Script/Breachpoint.BRGA_WeaponSwap", "/Script/Breachpoint.BRGA_Jump",
}


# =====================================================================================
# the fake editor
# =====================================================================================

class FakeClass(object):
    def __init__(self, name, path="", parent=None):
        self._name = name
        self._path = path or ("/Script/Breachpoint." + name)
        self._parent = parent
        self.cdo = None

    def get_name(self):
        return self._name

    def get_path_name(self):
        return self._path

    def get_super_class(self):
        return self._parent

    def is_child_of(self, other):
        node = self
        while node is not None:
            if node is other:
                return True
            node = node._parent
        return False


class FakeObject(object):
    """Anything with get/set_editor_property. Property names are the python-cased ones."""

    def __init__(self, class_name, path="", props=None):
        self._class = FakeClass(class_name)
        self._path = path
        self._props = dict(props or {})

    def get_class(self):
        return self._class

    def get_name(self):
        return self._path.rsplit("/", 1)[-1].split(".")[0]

    def get_path_name(self):
        return self._path

    def get_editor_property(self, name):
        if name not in self._props:
            raise Exception("no property '%s' on %s" % (name, self._path))
        return self._props[name]

    def set_editor_property(self, name, value):
        self._props[name] = value


class FakeGameplayTag(FakeObject):
    def __init__(self, name=""):
        FakeObject.__init__(self, "GameplayTag", "", {"tag_name": name})

    def import_text(self, text):
        # The real FGameplayTag resolves against the tags manager; an unregistered tag comes
        # back empty. That is the behaviour the generator must refuse on.
        if text in REGISTERED_TAGS:
            self._props["tag_name"] = text
            return True
        self._props["tag_name"] = ""
        return False


class FakeSetEntry(FakeObject):
    def __init__(self):
        FakeObject.__init__(self, "BRAbilitySetEntry", "",
                            {"ability": "", "ability_level": 1, "input_tag": FakeGameplayTag()})

    def import_text(self, text):
        # Mirrors the engine's struct text import for
        # (Ability="...",AbilityLevel=N,InputTag=(TagName="..."))
        try:
            body = text.strip()[1:-1]
            ability = body.split('Ability="', 1)[1].split('"', 1)[0]
            level = int(body.split("AbilityLevel=", 1)[1].split(",", 1)[0])
            tag_name = body.split('TagName="', 1)[1].split('"', 1)[0]
        except (IndexError, ValueError):
            return False
        tag = FakeGameplayTag()
        if not tag.import_text(tag_name):
            return False
        self._props["ability"] = ability
        self._props["ability_level"] = level
        self._props["input_tag"] = tag
        return True

    def export_text(self):
        return '(Ability="%s",AbilityLevel=%s,InputTag=(TagName="%s"))' % (
            self._props["ability"], self._props["ability_level"],
            self._props["input_tag"].get_editor_property("tag_name"))


class FakeEditor(object):
    """The whole pretend project: which assets exist, what they hold, what reached disk."""

    def __init__(self, content_dir):
        self.content_dir = content_dir
        self.assets = {}
        self.saved = []
        self.created = []

    # ---- asset table -------------------------------------------------------------
    def add_set(self, package, rows=None):
        asset = FakeObject("BRAbilitySet", package + "." + package.rsplit("/", 1)[-1],
                           {"abilities": list(rows or []), "effects": []})
        self.assets[package] = asset
        self._touch(package)
        return asset

    def add_blueprint(self, package, parent_class, props):
        generated = FakeClass(package.rsplit("/", 1)[-1] + "_C",
                              package + "." + package.rsplit("/", 1)[-1] + "_C", parent_class)
        generated.cdo = FakeObject(generated.get_name(), generated.get_path_name(), props)
        asset = FakeObject("Blueprint", package + "." + package.rsplit("/", 1)[-1],
                           {"generated_class": generated})
        asset.generated_class = lambda g=generated: g
        self.assets[package] = asset
        self._touch(package)
        return asset

    def _disk_path(self, package):
        rel = package[len("/Game/"):] if package.startswith("/Game/") else package.lstrip("/")
        return os.path.normpath(os.path.join(self.content_dir, rel + ".uasset"))

    def _touch(self, package):
        path = self._disk_path(package)
        folder = os.path.dirname(path)
        if not os.path.isdir(folder):
            os.makedirs(folder)
        with open(path, "w", encoding="utf-8") as fh:
            fh.write("fake uasset for %s\n" % package)

    def save(self, package):
        self.saved.append(package)
        self._touch(package)
        return True


def build_fake_unreal(editor):
    """A module object that satisfies exactly the surface build_abilitysets.py touches."""
    mod = types.ModuleType("unreal")

    mod.log = lambda m: None
    mod.log_warning = lambda m: None
    mod.log_error = lambda m: None

    mod.GameplayTag = FakeGameplayTag
    mod.BRAbilitySetEntry = FakeSetEntry
    mod.BRAbilitySet = FakeClass("BRAbilitySet")
    mod.SoftClassPath = lambda p: p
    mod.DataAssetFactory = lambda: FakeObject("DataAssetFactory", "/Engine/DataAssetFactory")

    game_mode = FakeClass("BRGameMode")
    game_mode.cdo = FakeObject("BRGameMode", "/Script/Breachpoint.Default__BRGameMode", {
        "player_controller_class": FakeClass("BRPlayerController"),
        "default_pawn_class": FakeClass("BRCharacter"),
        "player_state_class": FakeClass("BRPlayerState"),
        "game_state_class": FakeClass("BRGameState"),
    })
    mod.BRGameMode = game_mode

    def get_default_object(cls):
        return getattr(cls, "cdo", None)
    mod.get_default_object = get_default_object

    def load_class(_outer, path):
        if path in RESOLVABLE_CLASSES:
            return FakeClass(path.rsplit(".", 1)[-1], path)
        return None
    mod.load_class = load_class
    mod.load_object = lambda _outer, path: load_class(_outer, path)

    class Paths(object):
        @staticmethod
        def convert_relative_path_to_full(p):
            return p

        @staticmethod
        def project_dir():
            return os.path.dirname(editor.content_dir)

        @staticmethod
        def project_content_dir():
            return editor.content_dir
    mod.Paths = Paths

    class EditorAssetLibrary(object):
        @staticmethod
        def does_asset_exist(package):
            return package in editor.assets

        @staticmethod
        def load_asset(package):
            return editor.assets.get(package)

        @staticmethod
        def save_loaded_asset(asset, _only_if_dirty=False):
            for package, held in editor.assets.items():
                if held is asset:
                    return editor.save(package)
            return False
    mod.EditorAssetLibrary = EditorAssetLibrary

    class AssetTools(object):
        @staticmethod
        def create_asset(name, path, _cls, _factory):
            package = path + "/" + name
            editor.created.append(package)
            return editor.add_set(package)

    mod.AssetToolsHelpers = types.SimpleNamespace(get_asset_tools=lambda: AssetTools())
    # DataValidation plugin absent: the generator must WARN and carry on, not crash.
    mod.EditorValidatorSubsystem = None
    mod.BlueprintEditorLibrary = None
    return mod


# =====================================================================================
# harness
# =====================================================================================

class Case(object):
    def __init__(self, name):
        self.name = name
        self.failures = []

    def check(self, condition, message):
        if not condition:
            self.failures.append(message)

    def report(self):
        if self.failures:
            sys.stdout.write("FAIL  %s\n" % self.name)
            for f in self.failures:
                sys.stdout.write("        %s\n" % f)
            return False
        sys.stdout.write("ok    %s\n" % self.name)
        return True


def make_project(with_game_mode_parent=True):
    """FakeEditor + the fake unreal module, wired so GM_BR's parent is the same FakeClass
    object the generator will compare against."""
    content = tempfile.mkdtemp(prefix="br-abilityset-content-")
    editor = FakeEditor(os.path.join(content, "Content"))
    mod = build_fake_unreal(editor)

    sprint = FakeSetEntry()
    sprint.import_text('(Ability="/Script/Breachpoint.BRGA_Sprint",AbilityLevel=1,'
                       'InputTag=(TagName="InputTag.Sprint"))')
    editor.add_set("/Game/Abilities/DA_AbilitySet_Core", [sprint])
    for name in ("AR", "Magnum", "Rocket"):
        editor.add_set("/Game/Abilities/DA_AbilitySet_" + name, [])

    parent = mod.BRGameMode if with_game_mode_parent else FakeClass("ShooterGameMode")
    editor.add_blueprint("/Game/Core/GM_BR", parent, {
        # BP20's measured evidence: the template's controller is what GM_BR actually held.
        "player_controller_class": FakeClass("BP_ShooterPlayerController_C"),
        "default_pawn_class": FakeClass("BP_BRcharacter_C"),
        "player_state_class": FakeClass("BRPlayerState"),
        "game_state_class": FakeClass("BRGameState"),
    })
    editor.add_blueprint("/Game/Core/PC_BR", mod.BRGameMode, {
        "jump_action": "", "fire_action": "", "reload_action": "", "swap_action": "",
        "grenade_action": "", "melee_action": "", "grapple_action": "", "sprint_action": "",
    })
    editor.add_blueprint("/Game/Characters/BP_BRcharacter", mod.BRGameMode, {
        "move_action": FakeClass("IA_Move", "/Game/Input/Actions/IA_Move.IA_Move"),
        "look_action": FakeClass("IA_Look", "/Game/Input/Actions/IA_Look.IA_Look"),
        "mouse_look_action": FakeClass("IA_MouseLook", "/Game/Input/Actions/IA_MouseLook.IA_MouseLook"),
    })
    return editor, mod, content


def run_with(editor, mod, argv, profile_path=None):
    sys.modules["unreal"] = mod
    sys.modules.pop("build_abilitysets", None)
    if HERE not in sys.path:
        sys.path.insert(0, HERE)
    import build_abilitysets  # noqa: E402

    report_dir = tempfile.mkdtemp(prefix="br-abilityset-report-")
    args = list(argv) + ["--no-lock-check", "--report-dir", report_dir]
    if profile_path:
        args += ["--profile", profile_path]
    # parse_args() runs the env var through shlex.split in POSIX mode, where a backslash is
    # an escape character - so a Windows path MUST be quoted or it arrives with its
    # separators eaten. The PowerShell wrapper quotes for the same reason.
    os.environ["BR_GENABILITYSETS_ARGS"] = " ".join(
        ('"%s"' % a) if ("\\" in a or " " in a) else a for a in args)
    try:
        code = build_abilitysets.main()
    finally:
        os.environ.pop("BR_GENABILITYSETS_ARGS", None)

    names = sorted(os.listdir(report_dir))
    report = {}
    if names:
        with open(os.path.join(report_dir, names[-1]), "r", encoding="utf-8") as fh:
            report = json.load(fh)
    shutil.rmtree(report_dir, ignore_errors=True)
    return code, report


def codes(report):
    return set(f["code"] for f in report.get("findings", []))


def rows_of(editor, package):
    out = []
    for entry in editor.assets[package].get_editor_property("abilities"):
        out.append((entry.get_editor_property("ability"),
                    entry.get_editor_property("input_tag").get_editor_property("tag_name"),
                    entry.get_editor_property("ability_level")))
    return out


def write_profile(mutate):
    """A copy of the real profile with `mutate` applied, in a temp file."""
    with open(os.path.join(HERE, "abilityset_profile.json"), "r", encoding="utf-8") as fh:
        profile = json.load(fh)
    mutate(profile)
    handle, path = tempfile.mkstemp(suffix=".json", prefix="br-abilityset-profile-")
    with os.fdopen(handle, "w", encoding="utf-8") as fh:
        json.dump(profile, fh, indent=2)
    return path


# =====================================================================================
# the cases
# =====================================================================================

def case_first_run_populates():
    case = Case("a first run populates all four sets and clears GM_BR's overrides")
    editor, mod, content = make_project()
    try:
        code, report = run_with(editor, mod, [])
        case.check(code == 0, "expected exit 0, got %d (verdict %s)" % (code, report.get("verdict")))

        core = rows_of(editor, "/Game/Abilities/DA_AbilitySet_Core")
        case.check(len(core) == 4, "Core should hold 4 rows, holds %d: %s" % (len(core), core))
        case.check(("/Script/Breachpoint.BRGA_Melee", "InputTag.Melee", 1) in core,
                   "melee is not in Core: %s" % (core,))
        case.check(all(tag for _a, tag, _l in core), "a Core row has no InputTag: %s" % (core,))
        case.check(not any(tag == "InputTag.Jump" for _a, tag, _l in core),
                   "Jump was granted from a set - it is granted in C++: %s" % (core,))

        for name in ("AR", "Magnum", "Rocket"):
            rows = rows_of(editor, "/Game/Abilities/DA_AbilitySet_" + name)
            tags = sorted(t for _a, t, _l in rows)
            case.check(tags == ["InputTag.Fire", "InputTag.Reload", "InputTag.Swap"],
                       "%s should grant fire/reload/swap, granted %s" % (name, tags))

        gm_cdo = editor.assets["/Game/Core/GM_BR"].generated_class().cdo
        case.check(gm_cdo.get_editor_property("player_controller_class")
                   is mod.BRGameMode.cdo.get_editor_property("player_controller_class"),
                   "GM_BR's PlayerControllerClass was not cleared to the C++ default")
        case.check("/Game/Core/GM_BR" in editor.saved, "GM_BR was not saved")
        case.check("EMPTY_ON_DISK" in codes(report),
                   "the empty weapon sets should be reported as EMPTY_ON_DISK, not drift")
        case.check("OVERRIDE_FOUND" in codes(report),
                   "GM_BR's overrides should be named in the receipt")
        case.check(report.get("verdict") == "built", "verdict was %s" % report.get("verdict"))
    finally:
        shutil.rmtree(content, ignore_errors=True)
    return case


def case_idempotent():
    case = Case("a second run writes nothing")
    editor, mod, content = make_project()
    try:
        run_with(editor, mod, [])
        editor.saved = []
        code, report = run_with(editor, mod, [])
        case.check(code == 0, "expected exit 0 on the second run, got %d" % code)
        case.check(editor.saved == [], "the second run saved %s" % (editor.saved,))
        case.check(len(report.get("in_sync", [])) >= 4,
                   "expected every set to be reported in_sync, got %s" % report.get("in_sync"))
    finally:
        shutil.rmtree(content, ignore_errors=True)
    return case


def case_drift_is_detected_and_fails():
    case = Case("a hand-edited set is detected, repaired, and fails the run")
    editor, mod, content = make_project()
    try:
        run_with(editor, mod, [])
        # Somebody opens the editor and drops the InputTag off melee - the exact hand-edit
        # that produces a granted, unreachable ability.
        rows = editor.assets["/Game/Abilities/DA_AbilitySet_Core"].get_editor_property("abilities")
        rows[1].set_editor_property("input_tag", FakeGameplayTag(""))
        editor.saved = []

        code, report = run_with(editor, mod, [])
        case.check(code == 1, "drift must fail the run; got exit %d" % code)
        case.check("DRIFT_SET_ROWS" in codes(report),
                   "drift was not named: %s" % sorted(codes(report)))
        case.check(report.get("verdict") == "drift", "verdict was %s" % report.get("verdict"))
        repaired = rows_of(editor, "/Game/Abilities/DA_AbilitySet_Core")
        case.check(all(tag for _a, tag, _l in repaired),
                   "the run failed WITHOUT repairing the row: %s" % (repaired,))
        case.check("/Game/Abilities/DA_AbilitySet_Core" in editor.saved,
                   "the repair was not saved")

        # ...and --allow-drift turns the same state into a clean pass.
        rows[1].set_editor_property("input_tag", FakeGameplayTag(""))
        code, _report = run_with(editor, mod, ["--allow-drift"])
        case.check(code == 0, "--allow-drift should pass after repairing; got exit %d" % code)
    finally:
        shutil.rmtree(content, ignore_errors=True)
    return case


def case_unregistered_tag_refuses():
    case = Case("an unregistered gameplay tag stops the run")
    editor, mod, content = make_project()
    path = write_profile(lambda p: p["sets"][0]["abilities"][1].__setitem__(
        "input_tag", "InputTag.Melee"))
    try:
        # Take the tag out of the editor's registry: the module compiled without it.
        REGISTERED_TAGS.discard("InputTag.Melee")
        code, report = run_with(editor, mod, [], profile_path=path)
        case.check(code == 1, "expected exit 1, got %d" % code)
        case.check(report.get("verdict") == "refused",
                   "verdict was %s, expected 'refused'" % report.get("verdict"))
        case.check(editor.saved == [],
                   "the run wrote %s before refusing - a tagless row must never reach disk"
                   % (editor.saved,))
    finally:
        REGISTERED_TAGS.add("InputTag.Melee")
        os.unlink(path)
        shutil.rmtree(content, ignore_errors=True)
    return case


def case_unresolvable_class_refuses():
    case = Case("an ability class the editor cannot resolve stops the run")
    editor, mod, content = make_project()

    def mutate(profile):
        profile["sets"][0]["abilities"][1]["ability"] = "/Script/Breachpoint.BRGA_Melee"
    path = write_profile(mutate)
    try:
        RESOLVABLE_CLASSES.discard("/Script/Breachpoint.BRGA_Melee")
        code, report = run_with(editor, mod, [], profile_path=path)
        case.check(code == 1, "expected exit 1, got %d" % code)
        case.check(editor.saved == [], "the run wrote %s before refusing" % (editor.saved,))
        case.check(report.get("verdict") == "refused", "verdict was %s" % report.get("verdict"))
    finally:
        RESOLVABLE_CLASSES.add("/Script/Breachpoint.BRGA_Melee")
        os.unlink(path)
        shutil.rmtree(content, ignore_errors=True)
    return case


def case_wrong_game_mode_parent_refuses():
    case = Case("GM_BR is refused if it does not derive from ABRGameMode")
    editor, mod, content = make_project(with_game_mode_parent=False)
    try:
        code, report = run_with(editor, mod, [])
        case.check(code == 1, "expected exit 1, got %d" % code)
        case.check("/Game/Core/GM_BR" not in editor.saved,
                   "GM_BR was written despite the wrong parent")
        case.check(report.get("verdict") == "refused", "verdict was %s" % report.get("verdict"))
    finally:
        shutil.rmtree(content, ignore_errors=True)
    return case


def case_jump_in_a_set_is_refused_statically():
    case = Case("a profile granting InputTag.Jump is refused with no editor at all")
    sys.path.insert(0, HERE)
    import abilityset_plan

    def mutate(profile):
        profile["sets"][0]["abilities"].append({
            "ability": "/Script/Breachpoint.BRGA_Jump",
            "input_tag": "InputTag.Jump",
            "level": 1,
        })
    path = write_profile(mutate)
    try:
        plan, _profile = abilityset_plan.load_all(path)
        codes_seen = set(f["code"] for f in plan["findings"])
        case.check(plan["errors"] >= 1, "the plan reported no error")
        case.check("ROW_BANNED_TAG" in codes_seen,
                   "expected ROW_BANNED_TAG, got %s" % sorted(codes_seen))
    finally:
        os.unlink(path)
    return case


def case_real_profile_is_valid():
    case = Case("the committed profile validates against the live repo")
    sys.path.insert(0, HERE)
    import abilityset_plan
    plan, _profile = abilityset_plan.load_all()
    case.check(plan["errors"] == 0,
               "the committed profile has %d error(s): %s"
               % (plan["errors"], [f for f in plan["findings"] if f["severity"] == "error"]))
    case.check(plan["counts"]["rows"] == 13,
               "expected 13 rows (4 core + 3x3 weapon), got %d" % plan["counts"]["rows"])
    return case


CASES = (
    case_real_profile_is_valid,
    case_jump_in_a_set_is_refused_statically,
    case_first_run_populates,
    case_idempotent,
    case_drift_is_detected_and_fails,
    case_unregistered_tag_refuses,
    case_unresolvable_class_refuses,
    case_wrong_game_mode_parent_refuses,
)


def main():
    sys.stdout.write("BREACHPOINT ability-set generator - no-editor self-test (BP20)\n")
    sys.stdout.write("=" * 78 + "\n")
    ok = True
    for factory in CASES:
        try:
            case = factory()
        except Exception as exc:  # noqa: BLE001 - a crashing case is a failing case
            import traceback
            sys.stdout.write("FAIL  %s raised\n%s\n" % (factory.__name__, traceback.format_exc()))
            ok = False
            continue
        ok = case.report() and ok
    sys.stdout.write("=" * 78 + "\n")
    if ok:
        sys.stdout.write(
            "PASS - the generator's decisions and idempotency hold against a FAKE editor.\n"
            "This is not an engine claim: no Unreal code ran, no .uasset byte was written,\n"
            "and not one Python binding name was verified.\n")
        return 0
    sys.stdout.write("FAIL - see the case(s) above.\n")
    return 1


if __name__ == "__main__":
    sys.exit(main())
