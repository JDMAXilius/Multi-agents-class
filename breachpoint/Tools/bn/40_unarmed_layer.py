"""BREACHPOINT NEXT - unarmed anim-layer discovery + DefaultGame.ini wiring (Roadmap 1, Goal 7.2/6.4).

Run inside the UE 5.8 editor (Python Editor Script Plugin):

    py "Tools/bn/40_unarmed_layer.py"

Packet run order: 20_bp_character.py -> 30_reparent_abp.py -> 40_unarmed_layer.py.

Finds every anim-layer candidate under /Game (AnimBlueprints whose name matches
Layer/Unarmed/ALI patterns, with a best-effort check of which unarmed candidates
implement an ALI interface) and prints the list. If EXACTLY ONE plausible unarmed
layer exists, writes its _C class path into Config/DefaultGame.ini under
[/Script/BreachpointNext.BNCharacter] UnarmedAnimLayer= - replacing only the
empty value, preserving every other byte. Zero or multiple candidates: the
scaffold line is ensured but left empty for the founder's one-word choice, and
an existing non-empty value is never overwritten (the founder's word wins).
Reads the line back from disk and prints an intent-vs-actual audit table; the
audit table is the proof, not "the script ran". Idempotent: a second run
converges to the same file and re-audits.

Exits non-zero if the audit diffs, so a caller can gate on it.
"""
import os
import sys

import unreal

SECTION = "[/Script/BreachpointNext.BNCharacter]"
KEY = "UnarmedAnimLayer"
NAME_PATTERNS = ("layer", "unarmed", "ali_")

SCAFFOLD = (
    "\n"
    "; Goal 6.4 - the anim-layer class BNCharacter links when no weapon is held\n"
    "; (LinkAnimClassLayers at anim init). Soft class path, _C suffix required -\n"
    "; a class ref, not an asset ref; without it the resolve silently nulls.\n"
    "; Written by Tools/bn/40_unarmed_layer.py when exactly one candidate exists;\n"
    "; otherwise that script prints the candidates and the founder fills in the\n"
    "; one word below.\n"
    + SECTION + "\n"
    + KEY + "=\n")

AUDIT_FMT = "%-32s | %-72s | %-72s | %s"


def _find_abps():
    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    try:
        filt = unreal.ARFilter(
            class_paths=[unreal.TopLevelAssetPath("/Script/Engine", "AnimBlueprint")],
            package_paths=["/Game"], recursive_paths=True)
        assets = ar.get_assets(filt)
    except Exception:
        # class_names is the pre-5.1 spelling; kept as the fallback only.
        filt = unreal.ARFilter(class_names=["AnimBlueprint"],
                               package_paths=["/Game"], recursive_paths=True)
        assets = ar.get_assets(filt)
    return sorted(str(a.package_name) for a in assets)


def _name_of(path):
    return path.rsplit("/", 1)[-1].lower()


def _ali_interface_classes(candidates):
    out = []
    for path in candidates:
        if not _name_of(path).startswith("ali_"):
            continue
        try:
            asset = unreal.EditorAssetLibrary.load_asset(path)
            cls = asset.get_editor_property("generated_class")
            if cls is not None:
                out.append(cls)
        except Exception:
            continue
    return out


def _implements_any(path, interfaces):
    if not interfaces:
        return "?"
    try:
        asset = unreal.EditorAssetLibrary.load_asset(path)
        cdo = unreal.get_default_object(asset.get_editor_property("generated_class"))
        for ic in interfaces:
            if unreal.SystemLibrary.does_implement_interface(cdo, ic):
                return "Y"
        return "N"
    except Exception:
        return "?"


# --- idempotent ini edit ---------------------------------------------------------

def _ini_path():
    return os.path.join(
        unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_config_dir()),
        "DefaultGame.ini")


def _read_lines(path):
    with open(path, "r", encoding="utf-8", newline="") as f:
        return f.read().splitlines(keepends=True)


def _locate(lines):
    """Returns (section_line_idx or None, key_line_idx or None, value or None)."""
    section_idx, in_section = None, False
    for i, line in enumerate(lines):
        s = line.strip()
        if s.startswith("["):
            in_section = (s == SECTION)
            if in_section:
                section_idx = i
        elif in_section and "=" in s and s.split("=", 1)[0].strip() == KEY:
            return section_idx, i, s.split("=", 1)[1].strip()
    return section_idx, None, None


def _eol(lines):
    for line in lines:
        if line.endswith("\r\n"):
            return "\r\n"
        if line.endswith("\n"):
            return "\n"
    return "\n"


def ensure_ini(path, value_to_write):
    """Ensures section+key exist; writes value_to_write only over an EMPTY value
    (None leaves an empty value empty). Everything else is preserved verbatim."""
    lines = _read_lines(path)
    original = "".join(lines)
    eol = _eol(lines)
    section_idx, key_idx, current = _locate(lines)

    if section_idx is None:
        if lines and not lines[-1].endswith(("\n", "\r\n")):
            lines[-1] += eol
        lines.append(eol)
        lines += [l + eol for l in SCAFFOLD.strip("\n").split("\n")]
        section_idx, key_idx, current = _locate(lines)
    elif key_idx is None:
        lines.insert(section_idx + 1, "%s=%s" % (KEY, eol))
        section_idx, key_idx, current = _locate(lines)

    if current == "" and value_to_write:
        lines[key_idx] = "%s=%s%s" % (KEY, value_to_write, eol)

    new_text = "".join(lines)
    if new_text != original:
        with open(path, "w", encoding="utf-8", newline="") as f:
            f.write(new_text)


def main():
    ini = _ini_path()
    if not os.path.isfile(ini):
        unreal.log_error("INI NOT FOUND: %s. Nothing was modified." % ini)
        return 1

    abps = _find_abps()
    candidates = [p for p in abps
                  if any(t in _name_of(p) for t in NAME_PATTERNS)]
    interfaces = _ali_interface_classes(candidates)
    # Interface assets themselves are not layers; unarmed = the founder's target set.
    unarmed = [p for p in candidates
               if "unarmed" in _name_of(p) and not _name_of(p).startswith("ali_")]

    unreal.log("== ANIM-LAYER CANDIDATES under /Game ==")
    for p in candidates:
        tag = "unarmed, implements ALI=%s" % _implements_any(p, interfaces) \
            if p in unarmed else "-"
        unreal.log("%-96s | %s" % (p, tag))
    if not candidates:
        unreal.log("(none)")

    _section_idx, _key_idx, existing = _locate(_read_lines(ini))
    if existing:
        decided, why = existing, "existing value preserved (founder's word wins)"
    elif len(unarmed) == 1:
        decided = "%s.%s_C" % (unarmed[0], unarmed[0].rsplit("/", 1)[-1])
        why = "exactly one plausible unarmed layer"
    else:
        decided = ""
        why = ("%d unarmed candidates - founder's one-word choice needed, ini "
               "value left empty" % len(unarmed))

    ensure_ini(ini, decided if not existing else None)

    # --- read-back audit (from disk, not from memory) ---
    _section_idx, key_idx, actual = _locate(_read_lines(ini))
    rows = [
        ("ini section + key present", "True", str(key_idx is not None)),
        ("%s (%s)" % (KEY, why), decided,
         actual if actual is not None else "(key missing)"),
    ]

    unreal.log("== AUDIT: %s %s ==" % (ini, SECTION))
    unreal.log(AUDIT_FMT % ("CHECK", "INTENT", "ACTUAL", "STATUS"))
    diffs = 0
    for name, intent, actual_v in rows:
        ok = intent == actual_v
        diffs += 0 if ok else 1
        unreal.log(AUDIT_FMT % (name, intent, actual_v, "OK" if ok else "DIFF"))
    unreal.log("== AUDIT %s: %d/%d checks =="
               % ("PASSED" if diffs == 0 else "FAILED", len(rows) - diffs, len(rows)))
    return 0 if diffs == 0 else 1


sys.exit(main())
