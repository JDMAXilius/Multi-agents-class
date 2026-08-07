#!/usr/bin/env python3
# =====================================================================================
# BREACHPOINT - build_abilitysets.py :: execute the ability-set plan into /Game/Abilities
#
# Serves:   TICKET BP20 steps B1, B2 (populate the four sets), B3 (clear GM_BR's class
#           overrides) and B4/B5 (audit only - reported, never written).
# Law:      CLAUDE.md law 7 - the committed, reviewable artifact is a script or a receipt,
#           never the .uasset alone. One owner per binary; lock before editing.
# Doctrine: .claude/skills/ue-editor - "the profile is the source of truth, the editor state
#           is its projection. Regenerate the projection; never let the projection drift
#           ahead of the source."
#
# This half is a DUMB EXECUTOR plus the asserts that only a LIVE asset can answer. Every
# decision - which sets exist, which abilities, which tags, which levels, which GM_BR
# properties get cleared - is made in abilityset_plan.py, which is plain CPython and
# reviewable with no editor. What lives here and cannot live there:
#
#   * reading what is ALREADY on disk (the drift audit);
#   * gameplay-tag registration and native-class resolution, asserted against the real engine;
#   * the real UBRAbilitySet::IsDataValid;
#   * writing .uasset bytes.
#
# Headless (the crew's default):
#   UnrealEditor-Cmd.exe <repo>\Breachpoint.uproject ^
#       -run=pythonscript -script="<repo>\Tools\gen_abilitysets\build_abilitysets.py" ^
#       -stdout -unattended -nosplash -nop4 -NoLiveCoding
# The PowerShell wrapper Tools/gen_abilitysets/build-abilitysets.ps1 does exactly this, plus
# engine resolution, the R21 "is an editor already running" guard, the plugin check and logging.
#
# BINDING HONESTY: this file was authored with NO editor available (BP20 was cut from a PIE
# run; the MCP bridge was down). Every Python binding name it depends on - the data-asset
# factory, FBRAbilitySetEntry's struct import, TSoftClassPtr conversion, the Blueprint CDO
# path - is reached through a defensive helper that tries the documented spelling, falls back
# through the plausible alternatives, and FAILS LOUDLY with what it actually saw. A wrong
# guess must stop the run, not half-write an asset and report a pass.
#
# Exit codes: 0 built/in-sync · 1 refused or contract violation · 3 blocked (bad input/env).
# =====================================================================================
"""Execute the validated ability-set plan: DA_AbilitySet_* rows, GM_BR override clearing."""

import argparse
import datetime
import json
import os
import shlex
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import abilityset_plan  # noqa: E402  (same directory; see sys.path line above)

try:
    import unreal
except ImportError:  # pragma: no cover - the whole point of the message
    sys.stderr.write(
        "BLOCKED: build_abilitysets.py must run inside Unreal (it imports `unreal`).\n"
        "  Headless: UnrealEditor-Cmd Breachpoint.uproject -run=pythonscript "
        "-script=\"Tools/gen_abilitysets/build_abilitysets.py\" -stdout -unattended -nosplash\n"
        "  Or use the wrapper: Tools\\gen_abilitysets\\build-abilitysets.ps1\n"
        "  To check the PROFILE without an editor, run abilityset_plan.py instead - it is plain\n"
        "  CPython and needs nothing installed.\n")
    sys.exit(3)


def log(msg):
    unreal.log("[BR-genability] %s" % msg)


def warn(msg):
    unreal.log_warning("[BR-genability] %s" % msg)


def err(msg):
    unreal.log_error("[BR-genability] %s" % msg)


class Refused(Exception):
    """A condition that must stop the run. The message is printed verbatim, and every one of
    them is written to be read by someone who was not here when it fired."""


# =====================================================================================
# argument plumbing
# =====================================================================================

def parse_args():
    """Args reach a -run=pythonscript script inconsistently across launch shapes, so we
    accept them from sys.argv AND from BR_GENABILITYSETS_ARGS (what the PS wrapper sets)."""
    ap = argparse.ArgumentParser(prog="build_abilitysets.py")
    ap.add_argument("--profile", default=None)
    ap.add_argument("--dry-run", action="store_true",
                    help="audit the live assets and report; write nothing")
    ap.add_argument("--allow-drift", action="store_true",
                    help="repair a hand-edited asset without failing the run (say why in the "
                         "ticket Log)")
    ap.add_argument("--no-lock-check", action="store_true",
                    help="skip the git-lfs lock check on pre-existing .uassets (recorded)")
    ap.add_argument("--skip-game-mode", action="store_true",
                    help="do B1/B2 only; leave GM_BR alone (B3)")
    ap.add_argument("--report-dir", default=None)

    argv = [a for a in sys.argv[1:] if not a.startswith("-run=")]
    env_args = os.environ.get("BR_GENABILITYSETS_ARGS", "").strip()
    if env_args:
        argv.extend(shlex.split(env_args))
    args, unknown = ap.parse_known_args(argv)
    if unknown:
        log("ignoring unrecognised args (engine flags): %s" % " ".join(unknown))
    return args


def repo_root():
    return os.path.normpath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))


def content_dir():
    return unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())


def uasset_disk_path(package):
    rel = package[len("/Game/"):] if package.startswith("/Game/") else package.lstrip("/")
    return os.path.normpath(os.path.join(content_dir(), rel + ".uasset"))


# =====================================================================================
# law 7: one owner per binary - lock before you touch it
# =====================================================================================

class LockGate(object):
    """Law 7, applied to the assets this run ACTUALLY rewrites - and only those.

    All four ability sets and GM_BR already exist (BP20 measured them), so in practice every
    write here is lock-gated. A set that does not exist yet has no second writer to collide
    with and is not gated; neither is an asset the run leaves alone, because demanding a lock
    on a file nobody is going to touch makes the honest path (re-run and confirm) the
    annoying one.
    """

    def __init__(self, root, enabled):
        self.root = root
        self.enabled = enabled
        self.locked = {}
        self.query_error = None
        self.gated = []
        # git lfs reports lock paths relative to the GIT root, which is not the UE project
        # root whenever the .uproject sits in a subdirectory of the repo (it does here:
        # <repo>/breachpoint/Breachpoint.uproject). Comparing a project-relative path against
        # a repo-relative one never matches, so every lock check failed as "NOT lfs-locked"
        # no matter how many locks were actually held. Both sides are normalised to the git
        # root; if git cannot answer, the project root is the old behaviour unchanged.
        self.lock_root = root
        if not enabled:
            return
        try:
            top = subprocess.check_output(["git", "rev-parse", "--show-toplevel"], cwd=root,
                                          stderr=subprocess.STDOUT)
            self.lock_root = top.decode("utf-8").strip() or root
        except (OSError, subprocess.CalledProcessError) as exc:
            self.query_error = str(exc)
            return
        try:
            out = subprocess.check_output(["git", "lfs", "locks", "--json"], cwd=root,
                                          stderr=subprocess.STDOUT)
            for lk in (json.loads(out.decode("utf-8") or "[]") or []):
                self.locked[str(lk.get("path", "")).replace("\\", "/")] = \
                    (lk.get("owner") or {}).get("name", "?")
        except (OSError, subprocess.CalledProcessError, ValueError) as exc:
            self.query_error = str(exc)

    def rel(self, package):
        return os.path.relpath(uasset_disk_path(package), self.lock_root).replace("\\", "/")

    def require(self, package, existed_before):
        """Raise Refused unless `package` may be written now."""
        if not self.enabled or not existed_before:
            return
        rel = self.rel(package)
        if self.query_error is not None:
            raise Refused(
                "about to rewrite the pre-existing binary %s, but git lfs locks could not be "
                "queried (%s). Install git-lfs, or re-run with --no-lock-check and say why in "
                "the ticket Log." % (rel, self.query_error))
        if rel not in self.locked:
            raise Refused(
                "about to rewrite the pre-existing binary %s, which is NOT lfs-locked. One owner "
                "per binary per ticket (law 7). Run:  git lfs lock %s" % (rel, rel))
        self.gated.append("%s locked by %s" % (rel, self.locked[rel]))

    def summary(self):
        if not self.enabled:
            return "skipped (--no-lock-check or --dry-run)"
        if not self.gated:
            return "no pre-existing binary was rewritten by this run"
        return "; ".join(self.gated)


# =====================================================================================
# reflection helpers - every one of these is defensive on purpose (see BINDING HONESTY)
# =====================================================================================

def _first_attr(names, what):
    for n in names:
        obj = getattr(unreal, n, None)
        if obj is not None:
            return obj, n
    raise Refused("this engine's Python bindings expose none of %s (%s). The binding name "
                  "changed or the module is not loaded." % (", ".join(names), what))


def make_tag(tag_string):
    """FGameplayTag from a string, ASSERTED.

    FGameplayTag::TagName is VisibleAnywhere (CPF_EditConst) and Python's set_editor_property
    refuses EditConst properties, so the tag is built through the engine's own text import,
    which resolves the name against the tags manager. A tag that is NOT registered comes back
    empty, and that failure is exactly the signal we want: it means this editor build does not
    carry Core/BRGameplayTags.h's native tags, and every row we were about to write would have
    been silently tagless - which is BP20's bug, re-created by BP20's own fix.
    """
    tag = unreal.GameplayTag()
    imported = False
    try:
        imported = bool(tag.import_text(tag_string))
    except Exception as exc:
        raise Refused("GameplayTag.import_text('%s') raised: %s" % (tag_string, exc))

    try:
        got = str(tag.get_editor_property("tag_name"))
    except Exception:
        got = ""

    if got != tag_string:
        raise Refused(
            "gameplay tag '%s' did not resolve (import returned %s, tag_name read back as '%s'). "
            "Either the tag is not declared in Source/Breachpoint/Core/BRGameplayTags.h, or this "
            "editor is running a module build that predates it. Writing rows with an empty tag "
            "would produce a set that grants abilities no key can ever reach - the exact failure "
            "BP20 exists to fix - so this run stops here." % (tag_string, imported, got))
    return tag


def resolve_ability_class(class_path):
    """/Script/Breachpoint.BRGA_X -> the live UClass, ASSERTED.

    A soft class pointer to a class this editor does not have serialises perfectly happily and
    fails at grant time with an ensure and a skipped verb. Resolving it HERE turns a runtime
    mystery into a refusal with a name in it.
    """
    cls = unreal.load_class(None, class_path)
    if cls is None:
        cls = unreal.load_object(None, class_path)
    if cls is None:
        raise Refused(
            "'%s' does not resolve to a class in this editor. Either the Breachpoint module is "
            "not compiled into this build, or it predates that ability. Build BreachpointEditor "
            "(rung 1) first - this script cannot reference a class the editor does not have."
            % class_path)
    return cls


def soft_class_path(value):
    """Whatever Python hands back for a TSoftClassPtr -> a comparable path string."""
    if value is None:
        return ""
    if isinstance(value, str):
        return value
    for getter in ("get_path_name",):
        fn = getattr(value, getter, None)
        if callable(fn):
            try:
                return str(fn())
            except Exception:
                pass
    for prop in ("asset_path_name", "asset_path", "path"):
        try:
            return str(value.get_editor_property(prop))
        except Exception:
            continue
    return str(value)


def same_class(a, b):
    """Path comparison that survives the several shapes UE prints class paths in, including
    the trailing _C on a Blueprint-generated class."""
    def norm(s):
        s = str(s).strip().strip("'\"")
        if "'" in s and s.count("'") >= 2:      # Class'/Script/Foo.Bar'
            s = s.split("'")[-2]
        if " " in s:                            # Class /Script/Foo.Bar
            s = s.split(" ")[-1]
        s = s.rstrip("'").lower()
        if s.endswith("_c"):
            s = s[:-2]
        # /Script/Breachpoint.BRGA_Melee and .../BRGA_Melee.BRGA_Melee are the same class.
        head, _, tail = s.rpartition(".")
        if head and "." in head and head.endswith(tail):
            s = head
        return s
    na, nb = norm(a), norm(b)
    return bool(na) and na == nb


def object_path_of(obj):
    if obj is None:
        return ""
    try:
        return str(obj.get_path_name())
    except Exception:
        return str(obj)


def default_object_of(cls):
    """The CDO, through whichever binding this engine exposes."""
    for attempt in (lambda: unreal.get_default_object(cls),
                    lambda: cls.get_default_object()):
        try:
            cdo = attempt()
            if cdo is not None:
                return cdo
        except Exception:
            continue
    return None


# =====================================================================================
# asset creation / loading
# =====================================================================================

def split_package(package):
    head, _, name = package.rpartition("/")
    return head, name


def ability_set_class():
    cls = getattr(unreal, "BRAbilitySet", None)
    if cls is None:
        raise Refused(
            "unreal.BRAbilitySet is not present in this editor. The Breachpoint module either is "
            "not compiled into this build or predates "
            "Source/Breachpoint/AbilitySystem/BRAbilitySet.h. Build BreachpointEditor (rung 1) "
            "first - this script cannot author an instance of a class the editor does not have.")
    return cls


def load_or_create_set(package, report):
    """Load the set if it exists, else create it. NEVER recreates an existing asset: an
    existing DA_AbilitySet_ keeps every property this generator does not manage (its Effects
    list, its asset metadata)."""
    if unreal.EditorAssetLibrary.does_asset_exist(package):
        asset = unreal.EditorAssetLibrary.load_asset(package)
        if asset is None:
            raise Refused("%s exists but failed to load." % package)
        return asset, False

    set_class = ability_set_class()
    factory_cls, _n = _first_attr(["DataAssetFactory"], "the UDataAsset factory")
    factory = factory_cls()
    try:
        factory.set_editor_property("data_asset_class", set_class)
    except Exception:
        pass  # FactoryCreateNew falls back to the class passed to create_asset

    path, name = split_package(package)
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, path, set_class, factory)
    if asset is None:
        raise Refused("could not create %s (factory %s)." % (package, factory.get_name()))
    report.setdefault("created", []).append(package)
    return asset, True


def save(asset, package, existed_before, lock, report):
    lock.require(package, existed_before)
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, False):
        raise Refused("save_loaded_asset failed for %s" % package)
    report.setdefault("saved", []).append(package)


# =====================================================================================
# B1 + B2 - populate the four sets
# =====================================================================================

def make_row(row):
    """One FBRAbilitySetEntry, authored through the engine's own struct text import.

    That is the one path which sets a TSoftClassPtr AND an EditConst FGameplayTag without
    guessing at Python type conversions. The property fallback exists because import_text on a
    USTRUCT is the binding this script could least afford to be wrong about.
    """
    struct_type = getattr(unreal, "BRAbilitySetEntry", None)
    if struct_type is None:
        raise Refused(
            "unreal.BRAbilitySetEntry is not present in this editor. FBRAbilitySetEntry is a "
            "USTRUCT(BlueprintType) in BRAbilitySet.h, so its absence means this editor is "
            "running a module build that predates it. Build BreachpointEditor (rung 1) first.")

    tag = make_tag(row["input_tag"])                 # asserts registration
    ability_class = resolve_ability_class(row["ability"])  # asserts the class exists

    entry = struct_type()
    text = '(Ability="%s",AbilityLevel=%d,InputTag=(TagName="%s"))' % (
        row["ability"], row["level"], row["input_tag"])
    ok = False
    try:
        ok = bool(entry.import_text(text))
    except Exception as exc:
        warn("row import_text failed for %s (%s); falling back to property sets."
             % (row["input_tag"], exc))

    if not ok or not _row_is(entry, row):
        set_ok = False
        for value in (ability_class, unreal.SoftClassPath(row["ability"]), row["ability"]):
            if value is None:
                continue
            try:
                entry.set_editor_property("ability", value)
                set_ok = True
                break
            except Exception:
                continue
        try:
            entry.set_editor_property("ability_level", int(row["level"]))
        except Exception as exc:
            raise Refused("cannot set AbilityLevel on a row: %s" % exc)
        try:
            entry.set_editor_property("input_tag", tag)
        except Exception as exc:
            raise Refused("cannot set InputTag on a row: %s" % exc)
        if not set_ok or not _row_is(entry, row):
            raise Refused(
                "could not author the row for %s. import_text('%s') returned %s and the property "
                "fallback did not stick; the row reads back as %s. This is the one conversion this "
                "script could not prove without an editor - report the read-back above in the "
                "ticket Log so the next run can be fixed in one line."
                % (row["input_tag"], text, ok, _row_debug(entry)))
    return entry


def _row_is(entry, want):
    try:
        tag = str(entry.get_editor_property("input_tag").get_editor_property("tag_name"))
    except Exception:
        tag = ""
    try:
        level = int(entry.get_editor_property("ability_level"))
    except Exception:
        level = -1
    ability = soft_class_path(entry.get_editor_property("ability"))
    return (tag == want["input_tag"] and level == int(want["level"])
            and same_class(ability, want["ability"]))


def _row_debug(entry):
    try:
        return entry.export_text()
    except Exception:
        return "<row export_text unavailable>"


def _rows_summary(rows):
    out = []
    for entry in (rows or []):
        try:
            tag = str(entry.get_editor_property("input_tag").get_editor_property("tag_name"))
        except Exception:
            tag = ""
        try:
            level = int(entry.get_editor_property("ability_level"))
        except Exception:
            level = -1
        out.append((soft_class_path(entry.get_editor_property("ability")), tag, level))
    return out


def _same_rows(have, want):
    if len(have) != len(want):
        return False
    for (ha, ht, hl), w in zip(have, want):
        if ht != w["input_tag"] or hl != int(w["level"]) or not same_class(ha, w["ability"]):
            return False
    return True


def _classify(have, want):
    """What the rows on disk MEAN, which is not the same question as whether they match.

      'in_sync'     - already correct.
      'empty'       - no rows at all. BP20's measured starting state for the three weapon sets.
      'incomplete'  - every row present is one the profile also wants, and the profile simply
                      wants MORE. That is DA_AbilitySet_Core on 2 Aug 2026: sprint was there,
                      melee/grenade/grapple were the delta. Filling it in is the ticket, not a
                      repair.
      'drift'       - a row on disk CONTRADICTS the profile (different tag, level, ability, or
                      ordering). Somebody hand-edited a generated asset; law 7 says that must
                      be seen, so this is the only class that fails the run.

    Conflating 'incomplete' with 'drift' would make the very first BP20 run fail on the asset
    it was written to fix, and would train the next reader to pass --allow-drift by reflex.
    """
    if _same_rows(have, want):
        return "in_sync"
    if not have:
        return "empty"
    wanted = [(r["ability"], r["input_tag"], int(r["level"])) for r in want]
    remaining = list(wanted)
    for ability, tag, level in have:
        match = None
        for candidate in remaining:
            if candidate[1] == tag and candidate[2] == level and same_class(ability, candidate[0]):
                match = candidate
                break
        if match is None:
            return "drift"
        remaining.remove(match)
    # Every existing row is one the profile wants, and (since _same_rows already failed) the
    # profile wants at least one more. Ordering still changes, but no authoring is lost.
    return "incomplete"


def build_sets(plan, args, lock, findings, report):
    """Author the four sets' Abilities arrays. Returns {package: asset}.

    TWO PHASES, and the split is load-bearing. Phase 1 reads every set, classifies it, and
    MATERIALISES every row - which is where the assertions that can refuse live: gameplay-tag
    registration and native-class resolution. Phase 2 writes. Nothing reaches disk until all
    four sets have been proven authorable, so a bad tag in the last set cannot leave the first
    one written and the run reporting a refusal. A partially-populated Content/Abilities is
    worse than an empty one: it looks done.
    """
    assets = {}
    pending = []

    # ---- phase 1: read, classify, materialise. No writes. ---------------------------
    for entry in plan["sets"]:
        package = entry["package"]
        want = entry["abilities"]

        asset, created = load_or_create_set(package, report)
        assets[package] = asset

        if created:
            log("created %s" % package)
            have, verdict = [], "empty"
        else:
            have = _rows_summary(asset.get_editor_property("abilities"))
            verdict = _classify(have, want)

        if verdict == "in_sync":
            report.setdefault("in_sync", []).append(package)
            continue
        if verdict == "empty":
            findings.append(("info", "EMPTY_ON_DISK",
                             "%s carried no ability rows - BP20's measured starting state, not "
                             "drift. Populating with %d row(s)." % (entry["name"], len(want))))
        elif verdict == "incomplete":
            findings.append(("info", "INCOMPLETE_ON_DISK",
                             "%s carried %d of the %d rows the profile wants, and none of them "
                             "contradicts it. On disk: %s. This is the BP20 delta, not a hand-edit."
                             % (entry["name"], len(have), len(want), have)))
        else:
            # ERROR, not warn, on purpose: main() only treats a DRIFT_ finding as the
            # "repaired, but failing so the edit is SEEN" case if it is an error. A warn here
            # would repair the asset and report a clean pass, which is exactly the silence
            # law 7 is written against.
            findings.append(("error", "DRIFT_SET_ROWS",
                             "%s's rows CONTRADICT the profile. On disk: %s. Somebody edited a "
                             "generated asset. Rewriting the Abilities array from the profile; "
                             "the Effects array is left untouched." % (entry["name"], have)))

        pending.append((entry, asset, not created, [make_row(row) for row in want]))

    # ---- phase 2: apply and save ----------------------------------------------------
    for entry, asset, existed_before, rows in pending:
        asset.set_editor_property("abilities", rows)

        # Post-set read-back: prove the property took, in this process, before saving.
        got = _rows_summary(asset.get_editor_property("abilities"))
        if not _same_rows(got, entry["abilities"]):
            raise Refused(
                "%s did not accept its rows: reads back as %s, expected %s."
                % (entry["name"], got,
                   [(r["ability"], r["input_tag"], r["level"]) for r in entry["abilities"]]))

        if args.dry_run:
            log("--dry-run: would write %d row(s) to %s" % (len(rows), entry["package"]))
        else:
            save(asset, entry["package"], existed_before, lock, report)
    return assets


# =====================================================================================
# B3 - clear GM_BR's four class overrides
# =====================================================================================

def clear_game_mode_overrides(plan, args, lock, findings, report):
    """Clear the class overrides GM_BR serialised before ABRGameMode set them in C++.

    HOW CLEARING WORKS, stated because it is the non-obvious part: a Blueprint CDO is
    delta-serialised against its parent CDO, so a property whose value EQUALS the parent's is
    not written to the .uasset at all. Setting the BP CDO's property to the C++ CDO's value is
    therefore exactly 'clear the override' - the same bytes as hitting the yellow reset arrow.
    Repointing at ABRPlayerController would leave an override that merely happens to agree
    today; clearing means the C++ default wins forever, which is why the C++ defaults were
    added in the first place.
    """
    gm = plan.get("game_mode") or {}
    package = gm.get("package") or ""
    props = list(gm.get("clear_properties") or [])
    if not package or not props:
        return None
    if args.skip_game_mode:
        log("--skip-game-mode: %s left alone (BP20 B3 NOT done by this run)." % package)
        report["game_mode"] = "skipped"
        return None

    if not unreal.EditorAssetLibrary.does_asset_exist(package):
        raise Refused("%s does not exist, so BP20 B3 cannot be done. Restore the asset, or make "
                      "its absence an explicit ticket decision." % package)
    blueprint = unreal.EditorAssetLibrary.load_asset(package)
    if blueprint is None:
        raise Refused("%s exists but failed to load." % package)

    generated = None
    for attempt in (lambda: blueprint.generated_class(),
                    lambda: blueprint.get_editor_property("generated_class")):
        try:
            generated = attempt()
            if generated is not None:
                break
        except Exception:
            continue
    if generated is None:
        raise Refused("could not reach %s's generated class, so its defaults cannot be read. "
                      "Is it actually a Blueprint?" % package)

    bp_cdo = default_object_of(generated)
    if bp_cdo is None:
        raise Refused("could not reach %s's class default object." % package)

    parent = getattr(unreal, "BRGameMode", None)
    if parent is None:
        raise Refused(
            "unreal.BRGameMode is not present in this editor, so there is no C++ default to clear "
            "TOWARDS. Build BreachpointEditor (rung 1) first.")
    parent_cdo = default_object_of(parent)
    if parent_cdo is None:
        raise Refused("could not reach ABRGameMode's class default object.")

    if not generated.is_child_of(parent):
        raise Refused(
            "%s does not derive from ABRGameMode (its parent is %s). Clearing its class overrides "
            "would not hand control to ABRGameMode's constructor - it would just blank them. This "
            "is a reparenting decision, not a generator's call: report it in the ticket Log."
            % (package, object_path_of(generated.get_super_class()
                                       if hasattr(generated, "get_super_class") else None)))

    detail = {}
    dirty = False
    for prop in props:
        try:
            have = bp_cdo.get_editor_property(prop)
        except Exception as exc:
            raise Refused("cannot read %s.%s: %s. Check the snake_case spelling in "
                          "abilityset_profile.json." % (package, prop, exc))
        try:
            want = parent_cdo.get_editor_property(prop)
        except Exception as exc:
            raise Refused("cannot read ABRGameMode's default for %s: %s" % (prop, exc))

        have_path, want_path = object_path_of(have), object_path_of(want)
        detail[prop] = {"was": have_path, "cpp_default": want_path}

        if same_class(have_path, want_path):
            log("%s.%s already agrees with the C++ default (%s)" % (package, prop, want_path))
            continue

        findings.append(("info", "OVERRIDE_FOUND",
                         "%s.%s was %s; ABRGameMode's C++ default is %s. Clearing."
                         % (package, prop, have_path or "(none)", want_path or "(none)")))
        if args.dry_run:
            dirty = True
            continue
        try:
            bp_cdo.set_editor_property(prop, want)
        except Exception as exc:
            raise Refused("cannot set %s.%s: %s" % (package, prop, exc))
        got = object_path_of(bp_cdo.get_editor_property(prop))
        if not same_class(got, want_path):
            raise Refused("%s.%s did not accept the C++ default: reads back as %s, expected %s."
                          % (package, prop, got, want_path))
        detail[prop]["now"] = got
        dirty = True

    report["game_mode"] = {"package": package, "properties": detail,
                           "changed": bool(dirty), "dry_run": bool(args.dry_run)}

    if not dirty:
        report.setdefault("in_sync", []).append(package)
        log("%s already carries no overriding class defaults; nothing written." % package)
        return blueprint

    if args.dry_run:
        log("--dry-run: would clear %d override(s) on %s" % (len(detail), package))
        return blueprint

    compiler = getattr(unreal, "BlueprintEditorLibrary", None)
    if compiler is not None and hasattr(compiler, "compile_blueprint"):
        try:
            compiler.compile_blueprint(blueprint)
        except Exception as exc:
            warn("could not recompile %s after clearing its defaults (%s); the saved asset is "
                 "still correct but the editor's in-memory class may be stale until reload."
                 % (package, exc))
    save(blueprint, package, True, lock, report)
    return blueprint


# =====================================================================================
# B4 + B5 - audit only. This function NEVER writes.
# =====================================================================================

def audit_only(plan, findings, report):
    """B4: confirm nothing on PC_BR shadows the new ini pins. B5: confirm BP_BRcharacter kept
    exactly Move/Look/MouseLook and that the removed JumpAction left no dangling override.

    Writing here would BE the bug B4 warns about: a value serialised on the Blueprint beats
    Config/DefaultGame.ini permanently and silently, because LoadConfig runs before the
    Blueprint's own serialisation. So this reads, reports, and stops.
    """
    audit = plan.get("audit_only") or {}
    out = {}

    pc = audit.get("player_controller") or {}
    package = pc.get("package")
    if package:
        cdo, problem = _cdo_for(package)
        if problem:
            findings.append(("warn", "B4_UNREADABLE", "%s - B4 was NOT confirmed." % problem))
        else:
            shadowed = {}
            for prop in pc.get("ini_pinned_properties", []):
                try:
                    value = cdo.get_editor_property(prop)
                except Exception as exc:
                    findings.append(("warn", "B4_PROP_UNREADABLE",
                                     "cannot read %s.%s (%s)." % (package, prop, exc)))
                    continue
                shadowed[prop] = object_path_of(value)
            out["player_controller"] = {"package": package, "values": shadowed}
            findings.append(("info", "B4_READ",
                             "%s's Input|Abilities values read back as: %s. These come from "
                             "Config/DefaultGame.ini UNLESS someone opened PC_BR and touched one - "
                             "in which case the Blueprint's copy now wins forever. Compare against "
                             "the ini before signing B4 off." % (package, shadowed)))

    ch = audit.get("character") or {}
    package = ch.get("package")
    if package:
        cdo, problem = _cdo_for(package)
        if problem:
            findings.append(("warn", "B5_UNREADABLE", "%s - B5 was NOT confirmed." % problem))
        else:
            kept, missing, dangling = {}, [], {}
            for prop in ch.get("expected_properties", []):
                try:
                    value = cdo.get_editor_property(prop)
                except Exception:
                    missing.append(prop)
                    continue
                path = object_path_of(value)
                kept[prop] = path
                if not path:
                    findings.append(("error", "B5_UNASSIGNED",
                                     "%s.%s is unassigned. The pawn owns Move, Look and MouseLook "
                                     "and nothing else does - an empty slot is a dead axis."
                                     % (package, prop)))
            for prop in ch.get("removed_properties", []):
                try:
                    value = cdo.get_editor_property(prop)
                except Exception:
                    continue  # the property is gone from C++, which is the expected outcome
                path = object_path_of(value)
                if path:
                    dangling[prop] = path
                    findings.append(("warn", "B5_DANGLING",
                                     "%s still carries %s = %s. Jump moved to the controller; this "
                                     "slot should no longer exist. If the property is gone from "
                                     "C++ this is a stale serialised value that will vanish on the "
                                     "next resave - say so in the ticket Log."
                                     % (package, prop, path)))
            if missing:
                findings.append(("info", "B5_ABSENT",
                                 "%s has no %s propert(y/ies) - consistent with them having been "
                                 "removed from C++." % (package, ", ".join(missing))))
            out["character"] = {"package": package, "kept": kept, "dangling": dangling}

    report["audit_only"] = out


def _cdo_for(package):
    """(cdo, problem) for a Blueprint asset package."""
    if not unreal.EditorAssetLibrary.does_asset_exist(package):
        return None, "%s does not exist" % package
    asset = unreal.EditorAssetLibrary.load_asset(package)
    if asset is None:
        return None, "%s exists but failed to load" % package
    generated = None
    for attempt in (lambda: asset.generated_class(),
                    lambda: asset.get_editor_property("generated_class")):
        try:
            generated = attempt()
            if generated is not None:
                break
        except Exception:
            continue
    if generated is None:
        return None, "could not reach %s's generated class" % package
    cdo = default_object_of(generated)
    if cdo is None:
        return None, "could not reach %s's class default object" % package
    return cdo, None


# =====================================================================================
# post-write verification - the pass that decides the exit code
# =====================================================================================

def verify(plan, assets, run_start, findings, report, check_disk=True):
    """Re-read everything and assert it, THEN hand each set to the engine's own validator.

    Honesty note, stated because it is easy to over-claim: these objects are the in-memory ones
    the run just saved, so this proves 'what we saved is what the plan says', not 'the bytes on
    disk parse'. The disk half is covered narrowly and concretely by the mtime check - the same
    'artifact is newer than the run that claims it' rule this project adopted after a stale
    binary was once reported as a pass.

    And none of it proves a KEY DOES ANYTHING. That is the PIE run in BP20's Done-when list.
    """
    ok = True

    for entry in plan["sets"]:
        asset = assets[entry["package"]]
        have = _rows_summary(asset.get_editor_property("abilities"))
        if not _same_rows(have, entry["abilities"]):
            findings.append(("error", "VERIFY_SET_ROWS",
                             "%s reads back as %s, expected %s."
                             % (entry["name"], have,
                                [(r["ability"], r["input_tag"], r["level"])
                                 for r in entry["abilities"]])))
            ok = False
            continue
        # The BP20 rule, asserted one final time and independently of the diff above: this is
        # the check that must never be able to pass by accident.
        for ability, tag, _level in have:
            if not tag:
                findings.append(("error", "VERIFY_ROW_TAGLESS",
                                 "%s ends this run granting %s with NO InputTag. That ability is "
                                 "granted and unreachable by any key - the exact bug BP20 exists "
                                 "to fix." % (entry["name"], ability)))
                ok = False
            elif tag in abilityset_plan.CONTRACT_BANNED_TAGS:
                findings.append(("error", "VERIFY_ROW_BANNED_TAG",
                                 "%s ends this run granting '%s': %s."
                                 % (entry["name"], tag,
                                    abilityset_plan.CONTRACT_BANNED_TAGS[tag])))
                ok = False

    # ---- mtime proof for everything this run claims to have written -----------------
    if check_disk:
        for package in report.get("saved", []):
            path = uasset_disk_path(package)
            if not os.path.isfile(path):
                findings.append(("error", "VERIFY_NO_FILE",
                                 "%s was reported saved but no .uasset exists at %s."
                                 % (package, path)))
                ok = False
                continue
            mtime = os.path.getmtime(path)
            report.setdefault("written_mtimes", {})[package] = \
                datetime.datetime.fromtimestamp(mtime).strftime("%Y-%m-%d %H:%M:%S")
            if mtime < run_start:
                findings.append(("error", "VERIFY_STALE_FILE",
                                 "%s is older than this run started. The save did not reach disk; "
                                 "the file you are looking at is somebody else's." % package))
                ok = False

    # ---- the acceptance gate: UBRAbilitySet::IsDataValid, run for real ---------------
    for entry in plan["sets"]:
        result = run_data_validation(entry["name"], assets[entry["package"]], findings, report)
        if result is False:
            ok = False
    return ok


def run_data_validation(name, asset, findings, report):
    """Run the editor's real validators over one ability set.

    UBRAbilitySet::IsDataValid is the acceptance test BP20 inherits: it rejects a row with no
    class or a level below 1, and warns on a duplicate InputTag. abilityset_plan.py mirrors
    those rules statically; this is the authoritative run.
    Returns True (valid), False (invalid), or None (could not run - reported, not hidden).
    """
    subsystem_cls = getattr(unreal, "EditorValidatorSubsystem", None)
    if subsystem_cls is None:
        findings.append(("warn", "VALIDATION_UNAVAILABLE",
                         "unreal.EditorValidatorSubsystem is absent (the DataValidation plugin is "
                         "not enabled), so UBRAbilitySet::IsDataValid did NOT run on %s. Only "
                         "abilityset_plan.py's static mirror of it did." % name))
        report.setdefault("data_validation", {})[name] = "unavailable"
        return None

    subsystem = unreal.get_editor_subsystem(subsystem_cls)
    usecase = getattr(getattr(unreal, "DataValidationUsecase", None), "SCRIPT", None)
    try:
        out = subsystem.is_object_valid(asset, [], [], usecase)
    except Exception:
        try:
            out = subsystem.is_object_valid(asset, usecase)
        except Exception as exc:
            findings.append(("warn", "VALIDATION_CALL_FAILED",
                             "could not call EditorValidatorSubsystem.is_object_valid on %s (%s). "
                             "UBRAbilitySet::IsDataValid did NOT run." % (name, exc)))
            report.setdefault("data_validation", {})[name] = "call-failed"
            return None

    result, errors, warnings = _unpack_validation(out)
    report.setdefault("data_validation", {})[name] = {
        "result": str(result),
        "errors": [str(e) for e in (errors or [])],
        "warnings": [str(w) for w in (warnings or [])],
    }
    for e in (errors or []):
        findings.append(("error", "ISDATAVALID", "%s: %s" % (name, e)))
    for w in (warnings or []):
        findings.append(("warn", "ISDATAVALID", "%s: %s" % (name, w)))

    invalid = getattr(getattr(unreal, "DataValidationResult", None), "INVALID", None)
    if invalid is not None and result == invalid:
        return False
    if errors:
        return False
    log("UBRAbilitySet::IsDataValid ran on %s and returned %s with 0 errors." % (name, result))
    return True


def _unpack_validation(out):
    if isinstance(out, tuple):
        if len(out) == 3:
            return out[0], out[1], out[2]
        if len(out) == 2:
            return out[0], out[1], []
        if len(out) == 1:
            return out[0], [], []
    return out, [], []


# =====================================================================================
# main
# =====================================================================================

def main():
    args = parse_args()
    run_start = time.time()
    root = repo_root()
    stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")

    log("=" * 78)
    log("BREACHPOINT ability-set generator - BP20 B1/B2/B3 (B4/B5 audited, never written)")
    log("repo: %s" % root)
    log("started: %s (mtime proof baseline)" % datetime.datetime.fromtimestamp(run_start))

    try:
        plan, _profile = abilityset_plan.load_all(args.profile)
    except abilityset_plan.ProfileError as exc:
        err("BLOCKED: %s" % exc)
        return 3

    findings = [(f["severity"], f["code"], f["message"]) for f in plan["findings"]]
    report = {
        "schema": "breachpoint.abilityset.report/1",
        "when": stamp,
        "ticket": "BP20",
        "plan_digest": plan["digest"],
        "profile_path": plan["profile_path"],
        "profile_sha256": plan["profile_sha256"],
        "dry_run": bool(args.dry_run),
        "allow_drift": bool(args.allow_drift),
        "skip_game_mode": bool(args.skip_game_mode),
        "plan": plan,
    }

    log("profile sha256 : %s" % plan["profile_sha256"])
    log("plan digest    : %s   <- re-runs agree iff this agrees" % plan["digest"])
    log("sets           : %d   rows: %d   GM_BR properties to clear: %d"
        % (plan["counts"]["sets"], plan["counts"]["rows"], plan["counts"]["cleared_properties"]))

    if plan["errors"]:
        for f in plan["findings"]:
            (err if f["severity"] == "error" else warn)("%s: %s" % (f["code"], f["message"]))
        err("REFUSING TO BUILD: %d profile validation error(s) above. Fix "
            "Tools/gen_abilitysets/abilityset_profile.json and re-run. Do NOT patch the CONTRACT_ "
            "constants in abilityset_plan.py to get past this." % plan["errors"])
        write_report(report, args, root, stamp, findings, "refused-validation")
        return 1

    # ---- law 7: gate every pre-existing binary at the moment we decide to write it ---
    if args.no_lock_check:
        warn("--no-lock-check: proceeding WITHOUT verifying lfs locks. Two writers on one binary "
             "is an unresolvable merge - say in the ticket Log why this was safe.")
    lock = LockGate(root, enabled=not args.no_lock_check and not args.dry_run)

    try:
        assets = build_sets(plan, args, lock, findings, report)
        clear_game_mode_overrides(plan, args, lock, findings, report)
        audit_only(plan, findings, report)
        verified = verify(plan, assets, run_start, findings, report, check_disk=not args.dry_run)
    except Refused as exc:
        err("REFUSED: %s" % exc)
        report["lock"] = lock.summary()
        write_report(report, args, root, stamp, findings, "refused")
        return 1
    except Exception as exc:  # noqa: BLE001 - a half-written asset must not be reported clean
        err("FAILED: %s" % exc)
        import traceback
        err(traceback.format_exc())
        report["lock"] = lock.summary()
        write_report(report, args, root, stamp, findings, "failed")
        return 1
    report["lock"] = lock.summary()

    for sev, code, msg in findings:
        (err if sev == "error" else warn if sev == "warn" else log)("%s: %s" % (code, msg))

    errors = [f for f in findings if f[0] == "error"]
    drift = [f for f in errors if f[1].startswith("DRIFT_")]
    hard = [f for f in errors if f not in drift]

    log("created : %s" % (", ".join(report.get("created", [])) or "none"))
    log("saved   : %s" % (", ".join(report.get("saved", [])) or "none"))
    log("in sync : %s" % (", ".join(report.get("in_sync", [])) or "none"))

    if args.dry_run:
        log("--dry-run: nothing was written.")
        write_report(report, args, root, stamp, findings, "dry-run")
        return 1 if errors else 0

    if hard or not verified:
        err("FAIL: the generated ability sets did not pass their own verification. Do not commit "
            "them.")
        write_report(report, args, root, stamp, findings, "verify-failed")
        return 1

    if drift and not args.allow_drift:
        err("FAIL (repaired, but failing on purpose): %d asset(s) on disk did not match the "
            "profile and were repaired - see the DRIFT_ lines above. Law 7 says these assets are "
            "generated, never hand-placed, so a difference means somebody edited a generated "
            "asset in the editor. The repair is done; this exit code exists so the edit is SEEN. "
            "Re-run to get a clean pass, or pass --allow-drift and justify it in the ticket Log."
            % len(drift))
        write_report(report, args, root, stamp, findings, "drift")
        return 1

    write_report(report, args, root, stamp, findings, "built")
    log("DONE. Rung honesty: exit 0 means the sets were written and re-read as the profile "
        "describes them and UBRAbilitySet::IsDataValid accepted every one. It does NOT mean a key "
        "does anything. BP20's Done-when still owes a PIE run logging GRANTED: for every ability "
        "with the right tag, and GA ACTIVATED: on melee, grenade and grapple with no ensure - and "
        "fire/reload/swap cannot be exercised at all until something equips a starting weapon "
        "(BP20's 'Blocked' section, C++ lane). PIE is not multiplayer.")
    return 0


def write_report(report, args, root, stamp, findings, verdict):
    """The receipt. Law 7's committed artifact is this file or the script - never the .uasset
    alone - so it is written on EVERY path, including the failures."""
    report["verdict"] = verdict
    report["findings"] = [{"severity": s, "code": c, "message": m} for s, c, m in findings]
    out_dir = args.report_dir or os.path.join(root, "Saved", "AbilitySetGen")
    try:
        if not os.path.isdir(out_dir):
            os.makedirs(out_dir)
        path = os.path.join(out_dir, "abilitysets-%s-%s.json" % (stamp, verdict))
        with open(path, "w", encoding="utf-8") as fh:
            json.dump(report, fh, indent=2, sort_keys=True, default=str)
        log("report: %s" % path)
    except OSError as exc:
        warn("could not write the run report: %s" % exc)


if __name__ == "__main__":
    sys.exit(main())
