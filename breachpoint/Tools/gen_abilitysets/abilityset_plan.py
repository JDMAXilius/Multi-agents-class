#!/usr/bin/env python3
# =====================================================================================
# BREACHPOINT - abilityset_plan.py :: abilityset_profile.json -> validated ASSET PLAN
#
# Serves:   TICKET BP20 steps B1, B2, B3 (populate the four DA_AbilitySet_* assets and
#           clear GM_BR's four class overrides).
# Law:      CLAUDE.md law 7 - the committed, reviewable artifact is a script or a receipt,
#           never the .uasset alone. The profile is the source of truth; Content/Abilities
#           is its projection.
# Contract: docs/contracts/data-and-assets.md (soft asset refs; one owner per binary).
# Acceptance: UBRAbilitySet::IsDataValid (BRAbilitySet.cpp), which this file mirrors
#           statically, plus the BP20 rules that IsDataValid cannot see.
#
# THIS FILE IMPORTS NOTHING FROM UNREAL. Plain CPython 3.8+, stdlib only, so the whole plan
# can be validated and reviewed WITHOUT an editor - which is the point, because BP20 is an
# editor-live ticket and the editor is held by one person at a time:
#
#     python Tools/gen_abilitysets/abilityset_plan.py --verbose
#
# build_abilitysets.py (which does import unreal) imports this module and executes the plan
# it returns. Same split as Tools/gen_input and Tools/blockout: every DECISION is made here,
# in reviewable text; the editor half is a dumb executor plus the asserts only a live asset
# can answer.
#
# What makes this more than a schema check: it cross-reads the REPO. The tags are checked
# against Source/Breachpoint/Core/BRGameplayTags.cpp, the ability classes against
# Source/Breachpoint/AbilitySystem/Abilities/*.h, the weapon sets against
# Content/Data/DT_Weapons.csv, and the startup set against Config/DefaultGame.ini. A typo in
# a class path is caught here, with no editor, instead of as a silently-skipped verb in a
# match.
#
# Exit codes match the ladder convention (Tools/_BRLadderCommon.ps1):
#   0 PASS  plan is valid    1 FAIL  validation errors    3 BLOCKED  cannot read input
# =====================================================================================
"""Validate abilityset_profile.json and derive the deterministic ability-set plan."""

import argparse
import hashlib
import json
import os
import re
import sys

# -------------------------------------------------------------------------------------
# THE CONTRACT. Acceptance law from BP20's measured evidence, BRAbilitySet.cpp and
# BRGameplayTags.cpp. These are constants HERE and are deliberately NOT readable from
# abilityset_profile.json: the profile chooses among legal values, it cannot legalise an
# illegal one. (Same rule as input_plan.CONTRACT_* and arena_plan.KICKOFF_*.)
# -------------------------------------------------------------------------------------

# The seven tags UBRInputConfig routes to the ASC. Only these may appear on a set row.
CONTRACT_ABILITY_TAGS = (
    "InputTag.Sprint",
    "InputTag.Fire",
    "InputTag.Reload",
    "InputTag.Swap",
    "InputTag.Grenade",
    "InputTag.Melee",
    "InputTag.Grapple",
)

# Tags an ability set must NEVER carry, and why. Jump is the load-bearing one: BP20's whole
# reason for existing is that jump WORKS while the rest are dead, precisely because
# ABRPlayerState::GiveNativeAbility grants it from C++. A row here is a SECOND spec on the
# same class - two activations per press, and the C++ grant is not the one you would find.
CONTRACT_BANNED_TAGS = {
    "InputTag.Jump": ("ABRPlayerState::GiveNativeAbility already grants UBRGA_Jump from C++; "
                      "a row here would give it a second spec"),
    "InputTag.Move": "native action, consumed by the CMC - it never reaches the ASC",
    "InputTag.Look": "native action, consumed by the controller - it never reaches the ASC",
    "InputTag.Crouch": "native action, consumed by the CMC - it never reaches the ASC",
}

CONTRACT_TAG_ROOT = "InputTag"
CONTRACT_MIN_LEVEL = 1

# UBRAbilitySet::GiveToAbilitySystem ensureAlwaysMsgf()s on a row with no InputTag, so an
# untagged row is loud now instead of silent. The profile may not author one at all.
CONTRACT_TAG_REQUIRED = True

# The set every player is granted at startup. Config/DefaultGame.ini's
# [/Script/Breachpoint.BRPlayerState] StartupAbilitySet must name exactly this package.
CONTRACT_STARTUP_SET = "/Game/Abilities/DA_AbilitySet_Core"

SET_PACKAGE_RE = re.compile(r"^/Game/Abilities/DA_AbilitySet_[A-Za-z][A-Za-z0-9_]*$")
SET_NAME_RE = re.compile(r"^DA_AbilitySet_[A-Za-z][A-Za-z0-9_]*$")
# A native UClass path drops the U/A prefix, exactly as Config/DefaultGame.ini spells
# /Script/Breachpoint.BRPlayerState. A path with the prefix ("...UBRGA_Melee") resolves to
# nothing and the row is skipped at grant time with an ensure - so it is an error here.
ABILITY_CLASS_RE = re.compile(r"^/Script/Breachpoint\.BRGA_[A-Za-z][A-Za-z0-9_]*$")
PACKAGE_RE = re.compile(r"^/Game/[A-Za-z0-9_/]+$")

ERROR = "error"
WARN = "warn"
INFO = "info"


class ProfileError(Exception):
    """The profile could not be read or parsed at all. Callers exit 3 (BLOCKED), not 1:
    'I could not look' is a different claim from 'I looked and it is wrong'."""


class Finding(object):
    __slots__ = ("severity", "code", "message")

    def __init__(self, severity, code, message):
        self.severity = severity
        self.code = code
        self.message = message

    def as_dict(self):
        return {"severity": self.severity, "code": self.code, "message": self.message}

    def __repr__(self):
        return "%s %s: %s" % (self.severity.upper(), self.code, self.message)


# =====================================================================================
# repo cross-reads - the part that makes this a real check instead of a schema lint
#
# Each returns (value, problem). A problem is reported as a WARN and the corresponding
# assertion is SKIPPED, never quietly passed: "I could not read BRGameplayTags.cpp" must
# not look the same as "every tag is declared".
# =====================================================================================

def repo_root():
    return os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))


def _read_text(path):
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as fh:
            return fh.read(), None
    except OSError as exc:
        return None, str(exc)


TAG_DEFINE_RE = re.compile(r'UE_DEFINE_GAMEPLAY_TAG(?:_COMMENT|_STATIC)?\s*\(\s*[A-Za-z0-9_]+\s*,\s*"([^"]+)"')


def declared_tags(root):
    """Every gameplay tag string natively declared in BRGameplayTags.cpp."""
    path = os.path.join(root, "Source", "Breachpoint", "Core", "BRGameplayTags.cpp")
    text, problem = _read_text(path)
    if text is None:
        return None, "cannot read %s (%s)" % (path, problem)
    return frozenset(TAG_DEFINE_RE.findall(text)), None


ABILITY_CLASS_DECL_RE = re.compile(r"class\s+BREACHPOINT_API\s+(U?BRGA_[A-Za-z0-9_]+)\s*:")


def declared_ability_classes(root):
    """Every UBRGA_* class declared under AbilitySystem/Abilities, keyed WITHOUT the U
    prefix - which is the spelling a /Script path uses."""
    folder = os.path.join(root, "Source", "Breachpoint", "AbilitySystem", "Abilities")
    if not os.path.isdir(folder):
        return None, "cannot read %s" % folder
    found = {}
    for name in sorted(os.listdir(folder)):
        if not name.endswith(".h"):
            continue
        text, _problem = _read_text(os.path.join(folder, name))
        if text is None:
            continue
        for decl in ABILITY_CLASS_DECL_RE.findall(text):
            found[decl[1:] if decl.startswith("U") else decl] = name
    if not found:
        return None, "no UBRGA_* class declarations found under %s" % folder
    return found, None


def weapon_table_sets(root):
    """{row name: ability set package} from DT_Weapons.csv's AbilitySet column.

    The .csv is the source the .uasset is reimported from (law 3), so it is the honest
    thing to read with no editor.
    """
    path = os.path.join(root, "Content", "Data", "DT_Weapons.csv")
    text, problem = _read_text(path)
    if text is None:
        return None, "cannot read %s (%s)" % (path, problem)
    lines = [ln for ln in text.splitlines() if ln.strip()]
    if len(lines) < 2:
        return None, "%s has no rows" % path
    header = [c.strip() for c in lines[0].split(",")]
    if "AbilitySet" not in header:
        return None, "%s has no AbilitySet column" % path
    col = header.index("AbilitySet")
    out = {}
    for line in lines[1:]:
        cells = line.split(",")
        if len(cells) <= col:
            continue
        # "/Game/X/DA_Y.DA_Y" -> "/Game/X/DA_Y"; an object path is package.objectname.
        ref = cells[col].strip().strip('"')
        out[cells[0].strip()] = ref.split(".")[0] if ref else ""
    return out, None


INI_STARTUP_RE = re.compile(r"^\s*StartupAbilitySet\s*=\s*(\S+)\s*$", re.MULTILINE)


def ini_startup_set(root):
    path = os.path.join(root, "Config", "DefaultGame.ini")
    text, problem = _read_text(path)
    if text is None:
        return None, "cannot read %s (%s)" % (path, problem)
    match = INI_STARTUP_RE.search(text)
    if match is None:
        return None, ("Config/DefaultGame.ini has no [/Script/Breachpoint.BRPlayerState] "
                      "StartupAbilitySet line")
    return match.group(1).split(".")[0], None


# =====================================================================================
# validation
# =====================================================================================

def validate(profile, root=None):
    """Every rule BP20 and BRAbilitySet.cpp impose, applied to the profile. Returns
    [Finding]. The caller refuses to build if any is ERROR."""
    findings = []
    root = root or repo_root()

    def error(code, msg):
        findings.append(Finding(ERROR, code, msg))

    def warn(code, msg):
        findings.append(Finding(WARN, code, msg))

    sets = profile.get("sets")
    if not isinstance(sets, list) or not sets:
        error("NO_SETS", "the profile declares no sets; there is nothing to build.")
        return findings

    live_tags, tag_problem = declared_tags(root)
    if tag_problem:
        warn("TAGS_UNREADABLE",
             "%s - so 'is this tag natively declared' was NOT checked statically. The editor "
             "half still asserts it at write time." % tag_problem)
    live_classes, class_problem = declared_ability_classes(root)
    if class_problem:
        warn("CLASSES_UNREADABLE",
             "%s - so 'does this ability class exist' was NOT checked statically. The editor "
             "half still asserts it at write time." % class_problem)

    seen_packages = {}
    tag_owners = {}          # tag -> [set name] , for the cross-set collision check
    core_tags = set()

    for index, entry in enumerate(sets):
        where = entry.get("name") or "set #%d" % index

        name = entry.get("name", "")
        package = entry.get("package", "")

        if not SET_NAME_RE.match(name or ""):
            error("SET_NAME", "%s: name must look like DA_AbilitySet_<Thing>." % where)
        if not SET_PACKAGE_RE.match(package or ""):
            error("SET_PACKAGE",
                  "%s: package '%s' must be /Game/Abilities/DA_AbilitySet_<Thing>." % (where, package))
        elif package.rsplit("/", 1)[-1] != name:
            error("SET_NAME_PACKAGE_MISMATCH",
                  "%s: package '%s' does not end in the declared name '%s'. The editor half loads "
                  "by package, so the name would be a lie in every report." % (where, package, name))

        if package in seen_packages:
            error("SET_DUPLICATE",
                  "%s and %s both claim %s. Two writers on one binary is exactly what law 7 "
                  "forbids." % (seen_packages[package], where, package))
        else:
            seen_packages[package] = where

        rows = entry.get("abilities")
        if not isinstance(rows, list) or not rows:
            error("SET_EMPTY",
                  "%s has no ability rows. An empty set is the BUG BP20 exists to fix - it is "
                  "granted successfully and grants nothing." % where)
            continue

        seen_tags = {}
        for row_index, row in enumerate(rows):
            row_where = "%s row %d" % (where, row_index)
            ability = (row.get("ability") or "").strip()
            tag = (row.get("input_tag") or "").strip()
            level = row.get("level", CONTRACT_MIN_LEVEL)

            # ---- the ability class ------------------------------------------------
            if not ability:
                error("ROW_NO_CLASS",
                      "%s has no ability class. UBRAbilitySet::IsDataValid rejects this row, and "
                      "GiveToAbilitySystem would skip it silently." % row_where)
            elif not ABILITY_CLASS_RE.match(ability):
                hint = ""
                if "/Script/Breachpoint.UBRGA_" in ability:
                    hint = (" A native UClass path drops the U prefix: write "
                            "'/Script/Breachpoint.%s'." % ability.rsplit(".", 1)[-1][1:])
                error("ROW_CLASS_PATH",
                      "%s: ability '%s' is not a native Breachpoint ability class path.%s"
                      % (row_where, ability, hint))
            elif live_classes is not None:
                short = ability.rsplit(".", 1)[-1]
                if short not in live_classes:
                    error("ROW_CLASS_MISSING",
                          "%s names %s, which is not declared anywhere in "
                          "Source/Breachpoint/AbilitySystem/Abilities. Known: %s."
                          % (row_where, short, ", ".join(sorted(live_classes)) or "(none)"))

            # ---- the InputTag: the whole mechanism --------------------------------
            if not tag:
                if CONTRACT_TAG_REQUIRED:
                    error("ROW_NO_TAG",
                          "%s has no InputTag. The ability would be granted and permanently "
                          "unreachable by any key - UBRAbilitySet::GiveToAbilitySystem "
                          "ensureAlways()es on exactly this." % row_where)
            elif tag in CONTRACT_BANNED_TAGS:
                error("ROW_BANNED_TAG",
                      "%s carries '%s', which no ability set may grant: %s."
                      % (row_where, tag, CONTRACT_BANNED_TAGS[tag]))
            elif tag not in CONTRACT_ABILITY_TAGS:
                error("ROW_UNKNOWN_TAG",
                      "%s carries '%s', which is not one of the seven ability input tags (%s). "
                      "Nothing routes it to the ASC, so the row is dead on arrival."
                      % (row_where, tag, ", ".join(CONTRACT_ABILITY_TAGS)))
            elif live_tags is not None and tag not in live_tags:
                error("ROW_TAG_UNDECLARED",
                      "%s carries '%s', which is not declared in "
                      "Source/Breachpoint/Core/BRGameplayTags.cpp. It would serialise as an "
                      "empty tag." % (row_where, tag))

            if tag:
                if tag in seen_tags:
                    error("ROW_TAG_DUPLICATE",
                          "%s repeats '%s' (already on row %d of the same set). Both abilities "
                          "activate on one press - UBRAbilitySet::IsDataValid warns; BP20 treats "
                          "it as an error because it is never what anyone meant."
                          % (row_where, tag, seen_tags[tag]))
                else:
                    seen_tags[tag] = row_index
                tag_owners.setdefault(tag, []).append(name or where)

            # ---- the level ---------------------------------------------------------
            if not isinstance(level, int) or isinstance(level, bool):
                error("ROW_LEVEL_TYPE", "%s: level must be a whole number, got %r." % (row_where, level))
            elif level < CONTRACT_MIN_LEVEL:
                error("ROW_LEVEL_RANGE",
                      "%s: level %d is below %d. UBRAbilitySet::IsDataValid rejects it."
                      % (row_where, level, CONTRACT_MIN_LEVEL))

        if package == CONTRACT_STARTUP_SET:
            core_tags = set(seen_tags)

    # ---- cross-set collision -----------------------------------------------------
    # The startup set is live for the whole match; a weapon set is granted ON TOP of it when
    # a weapon is equipped. A tag in both means two specs answer one press, and which one
    # wins depends on grant order - the kind of bug that only shows up with a weapon out.
    for tag, owners in sorted(tag_owners.items()):
        if len(owners) > 1 and tag in core_tags:
            error("CROSS_SET_TAG_COLLISION",
                  "'%s' is granted by %s. The startup set is never taken away, so once a weapon "
                  "is equipped two specs carry that tag and both activate on one press."
                  % (tag, " and ".join(owners)))

    # ---- the sets the rest of the project actually asks for -----------------------
    declared_packages = set(seen_packages)

    startup, startup_problem = ini_startup_set(root)
    if startup_problem:
        warn("INI_UNREADABLE", "%s - StartupAbilitySet was NOT cross-checked." % startup_problem)
    elif startup != CONTRACT_STARTUP_SET:
        error("INI_STARTUP_MISMATCH",
              "Config/DefaultGame.ini pins StartupAbilitySet=%s, but this generator's startup set "
              "is %s. One of the two is wrong and every player would be granted the other."
              % (startup, CONTRACT_STARTUP_SET))
    elif CONTRACT_STARTUP_SET not in declared_packages:
        error("STARTUP_SET_ABSENT",
              "Config/DefaultGame.ini pins StartupAbilitySet=%s, which this profile does not "
              "build. Nothing would populate the one set every player gets." % CONTRACT_STARTUP_SET)

    weapons, weapon_problem = weapon_table_sets(root)
    if weapon_problem:
        warn("WEAPON_TABLE_UNREADABLE", "%s - DT_Weapons was NOT cross-checked." % weapon_problem)
    else:
        for row_name, ref in sorted(weapons.items()):
            if not ref:
                warn("WEAPON_NO_SET",
                     "DT_Weapons row '%s' has an empty AbilitySet, so equipping it grants no "
                     "fire/reload/swap. Outside this generator's owner path - note it in the "
                     "ticket Log." % row_name)
            elif ref not in declared_packages:
                error("WEAPON_SET_UNBUILT",
                      "DT_Weapons row '%s' points at %s, which this profile does not build. That "
                      "weapon would equip with an empty set - the exact BP20 symptom."
                      % (row_name, ref))

    # ---- GM_BR (B3) ---------------------------------------------------------------
    gm = profile.get("game_mode")
    if gm is not None:
        package = gm.get("package", "")
        props = gm.get("clear_properties")
        if not PACKAGE_RE.match(package or ""):
            error("GM_PACKAGE", "game_mode.package '%s' is not a /Game path." % package)
        if not isinstance(props, list) or not props:
            error("GM_NO_PROPS", "game_mode declares no clear_properties; B3 would be a no-op.")
        else:
            for prop in props:
                if not isinstance(prop, str) or not prop.islower():
                    error("GM_PROP_NAME",
                          "game_mode.clear_properties entry %r must be the snake_case Python name "
                          "(e.g. 'player_controller_class')." % prop)

    return findings


# =====================================================================================
# plan
# =====================================================================================

def build_plan(profile, root=None):
    """The deterministic plan the editor half executes. Ordering is fixed so the digest is
    stable across runs and machines - a re-run agrees iff the digest agrees."""
    root = root or repo_root()
    sets = []
    for entry in profile.get("sets", []):
        rows = []
        for row in entry.get("abilities", []):
            rows.append({
                "ability": (row.get("ability") or "").strip(),
                "input_tag": (row.get("input_tag") or "").strip(),
                "level": int(row.get("level", CONTRACT_MIN_LEVEL) or CONTRACT_MIN_LEVEL),
            })
        sets.append({
            "name": entry.get("name", ""),
            "package": entry.get("package", ""),
            "granted_by": entry.get("granted_by", ""),
            "abilities": rows,
        })
    sets.sort(key=lambda s: s["package"])

    gm = profile.get("game_mode") or {}
    audit = profile.get("audit_only") or {}

    plan = {
        "schema": "breachpoint.abilityset.plan/1",
        "sets": sets,
        "game_mode": {
            "package": gm.get("package", ""),
            "clear_properties": list(gm.get("clear_properties", [])),
        },
        "audit_only": audit,
        "counts": {
            "sets": len(sets),
            "rows": sum(len(s["abilities"]) for s in sets),
            "cleared_properties": len(gm.get("clear_properties", [])),
        },
    }
    return plan


def plan_digest(plan):
    """A hash of the decisions only - counts and comments are excluded, so a re-worded note
    does not read as a changed plan."""
    material = []
    for entry in plan["sets"]:
        material.append(entry["package"])
        for row in entry["abilities"]:
            material.append("%s|%s|%d" % (row["ability"], row["input_tag"], row["level"]))
    material.append(plan["game_mode"]["package"])
    material.extend(sorted(plan["game_mode"]["clear_properties"]))
    return hashlib.sha256("\n".join(material).encode("utf-8")).hexdigest()[:16]


def digest_file(path):
    try:
        with open(path, "rb") as fh:
            return hashlib.sha256(fh.read()).hexdigest()
    except OSError:
        return ""


def default_profile():
    return os.path.join(os.path.dirname(os.path.abspath(__file__)), "abilityset_profile.json")


def load_all(profile_path=None):
    """(plan, profile). Raises ProfileError if the profile cannot be read or parsed."""
    path = profile_path or default_profile()
    if not os.path.isfile(path):
        raise ProfileError("no profile at %s" % path)
    try:
        with open(path, "r", encoding="utf-8") as fh:
            profile = json.load(fh)
    except (OSError, ValueError) as exc:
        raise ProfileError("cannot parse %s: %s" % (path, exc))
    if not isinstance(profile, dict):
        raise ProfileError("%s is not a JSON object" % path)

    root = repo_root()
    findings = validate(profile, root)
    plan = build_plan(profile, root)
    plan["profile_path"] = path
    plan["profile_sha256"] = digest_file(path)
    plan["digest"] = plan_digest(plan)
    plan["findings"] = [f.as_dict() for f in findings]
    plan["errors"] = len([f for f in findings if f.severity == ERROR])
    plan["warnings"] = len([f for f in findings if f.severity == WARN])
    return plan, profile


# =====================================================================================
# reporting / CLI
# =====================================================================================

def print_report(plan, verbose=False):
    out = sys.stdout.write
    out("profile      : %s\n" % plan["profile_path"])
    out("profile sha  : %s\n" % plan["profile_sha256"])
    out("plan digest  : %s   <- re-runs agree iff this agrees\n" % plan["digest"])
    out("sets         : %d   rows: %d   GM_BR properties cleared: %d\n"
        % (plan["counts"]["sets"], plan["counts"]["rows"], plan["counts"]["cleared_properties"]))
    out("\n")

    if verbose:
        for entry in plan["sets"]:
            out("%s  (%s)\n" % (entry["package"], entry["granted_by"] or "granted by: unstated"))
            for row in entry["abilities"]:
                out("    %-40s %-20s level %d\n"
                    % (row["ability"].rsplit(".", 1)[-1], row["input_tag"], row["level"]))
            out("\n")
        gm = plan["game_mode"]
        if gm["package"]:
            out("%s  clear: %s\n\n" % (gm["package"], ", ".join(gm["clear_properties"])))

    for finding in plan["findings"]:
        out("%-5s %-26s %s\n" % (finding["severity"].upper(), finding["code"], finding["message"]))
    if plan["findings"]:
        out("\n")
    out("%d error(s), %d warning(s)\n" % (plan["errors"], plan["warnings"]))


def main(argv=None):
    ap = argparse.ArgumentParser(prog="abilityset_plan.py",
                                 description="Validate abilityset_profile.json (no editor needed).")
    ap.add_argument("--profile", default=None)
    ap.add_argument("--verbose", action="store_true", help="print the whole plan, not just findings")
    ap.add_argument("--json", action="store_true", help="emit the plan as JSON")
    args = ap.parse_args(argv)

    try:
        plan, _profile = load_all(args.profile)
    except ProfileError as exc:
        sys.stderr.write("BLOCKED: %s\n" % exc)
        return 3

    if args.json:
        json.dump(plan, sys.stdout, indent=2, sort_keys=True)
        sys.stdout.write("\n")
    else:
        print_report(plan, verbose=args.verbose)

    if plan["errors"]:
        sys.stdout.write(
            "\nREFUSING: fix Tools/gen_abilitysets/abilityset_profile.json. Do NOT relax the "
            "CONTRACT_* constants in this file to get past it - they are BP20's measured evidence "
            "and BRAbilitySet.cpp's acceptance rules.\n")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
