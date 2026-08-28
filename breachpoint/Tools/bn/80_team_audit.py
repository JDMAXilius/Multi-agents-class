"""TEAM SURFACES, audited against the LIVE EDITOR.

    python Tools/bn/80_team_audit.py          # read-only; prints a table and exits 0/1

THE QUESTION THIS ANSWERS. "Are the team colours actually applied?" has three different
answers per surface and only one of them is "yes":

  APPLIED      the class is configured AND the thing it points at can honour it
  SILENT       the class is configured and the asset CANNOT honour it -- no warning, no
               log, no colour. This is the dangerous state, and the reason this file
               exists: a Niagara USER parameter the system does not declare is dropped
               without a word, so the code looks correct forever.
  UNSET        nothing configured. Honest, and visible.

WHY THE CDO AND NOT THE INI. Reading DefaultGame.ini would prove what I typed, not what
the engine loaded. Every value below is read back off the class default object in the
running editor, so a section header typo, a renamed property or a stale config all show
up as UNSET here rather than as a mystery in PIE.

THE ONE THING MCP CANNOT ANSWER, and how it is answered instead. A NiagaraSystem's
ExposedParameters is an FNiagaraUserRedirectionParameterStore, not a UPROPERTY -- it is
not in list_properties and get_properties refuses it. So the declaration test reads the
PACKAGE: an FName that exists in the system appears in the .uasset's name table. Coarse
but decisive, and it agrees with the measurement recorded in BNGameplayCues.h.

Stdlib only; drives the running editor over the MCP server's HTTP JSON-RPC endpoint.
"""
from __future__ import annotations

import json
import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "mcp-ui" / "gen_ui"))
from mcp import MCP, OBJ, ASSET  # noqa: E402

REPO = Path(__file__).resolve().parents[2]

# Every cue class that inherits the tint. Kept as a list rather than discovered, so a NEW
# cue class that nobody added here is a visible omission in review rather than a silent
# gap in the audit.
CUES = [
    "BNGameplayCue_MuzzleFlash",
    "BNGameplayCue_Impact",
    "BNGameplayCue_Tracer",
    "BNGameplayCue_GrappleFire",
    "BNGameplayCue_GrappleRope",
    "BNGameplayCue_GrappleHit",
    "BNGameplayCue_Death",
    "BNGameplayCue_Explosion",
    "BNGameplayCue_ShieldRegen",
    "BNGameplayCue_HealthRegen",
]

APPLIED, SILENT, UNSET, BROKEN = "APPLIED", "SILENT", "UNSET", "BROKEN"
ICON = {APPLIED: "OK  ", SILENT: "!!  ", UNSET: "--  ", BROKEN: "FAIL"}


def content_path(object_path: str) -> Path | None:
    """/Game/A/B.B -> <repo>/Content/A/B.uasset. None for anything not under /Game."""
    if not object_path.startswith("/Game/"):
        return None
    package = object_path.split(".")[0]
    return REPO / "Content" / (package[len("/Game/"):] + ".uasset")


def user_params(object_path: str) -> list[str]:
    """Every User.* parameter this system declares. Used to say WHY a tint is silent — a
    finding that names the parameters an asset does have is actionable; "it did not work"
    is not."""
    path = content_path(object_path)
    if not path or not path.is_file():
        return []
    try:
        out = subprocess.run(["strings", str(path)], capture_output=True, text=True, timeout=60).stdout
    except Exception:
        return []
    return sorted({l.strip() for l in out.splitlines() if l.startswith("User.")})


def declares(object_path: str, name: str) -> bool | None:
    """Does this asset's package contain the FName? None when the file is not on disk."""
    path = content_path(object_path)
    if not path or not path.is_file():
        return None
    try:
        out = subprocess.run(["strings", str(path)], capture_output=True, text=True, timeout=60).stdout
    except Exception:
        return None
    # The stored FName is the bare name; "User." is the redirection prefix the code writes.
    bare = name.split(".")[-1]
    return bare in out


def main() -> int:
    m = MCP()
    m.init()
    rows: list[tuple[str, str, str]] = []
    findings: list[str] = []

    def get(instance: str, props: list[str]) -> dict:
        """Read what this object HAS. get_properties fails the WHOLE call if one requested
        name is absent, and these classes differ on purpose — Tracer has no Sound, Death has
        no Effect. Asking for the union would report every such class as unreadable, which is
        how the first run of this audit produced six false failures."""
        rv, _ = m.call(OBJ, "list_properties", {"instance": {"refPath": instance}})
        try:
            have = set(json.loads(rv).keys())
        except Exception:
            return {}
        # CASE-INSENSITIVE, and resolved to the name list_properties actually reports:
        # the lister answers camelCase (skeletalMeshAsset) while get_properties also accepts
        # PascalCase, so a literal match silently found nothing and the caller read that as
        # "unreadable object" rather than "I asked with the wrong casing".
        lower = {h.lower(): h for h in have}
        want = [lower[p.lower()] for p in props if p.lower() in lower]
        if not want:
            return {}
        rv, txt = m.call(OBJ, "get_properties", {"instance": {"refPath": instance}, "properties": want})
        if not rv:
            return {}
        try:
            got = json.loads(rv)
        except Exception:
            return {}
        # Key by BOTH casings so callers can ask however reads best at the call site.
        out = dict(got)
        for k, v in got.items():
            out[k[0].lower() + k[1:]] = v
            out[k[0].upper() + k[1:]] = v
        return out

    def ref(value) -> str:
        if isinstance(value, dict):
            return value.get("refPath", "")
        return "" if value in (None, "None") else str(value)

    # ---------------------------------------------------------------- 1. cue tints
    print("\n=== GAMEPLAY CUES — does the tint reach an FX asset that can honour it? ===")
    for cue in CUES:
        cdo = f"/Script/BreachpointNext.Default__{cue}"
        d = get(cdo, ["effect", "sound", "tintParameter"])
        if not d:
            rows.append((BROKEN, cue, "CDO unreadable — class missing or renamed"))
            continue
        if "effect" not in d:
            rows.append((UNSET, cue, "this cue has no Effect property at all — nothing to tint"))
            continue
        effect, tint = ref(d.get("effect")), d.get("tintParameter") or ""
        if not effect:
            rows.append((UNSET, cue, f"no Effect configured (tint '{tint}' has nothing to write to)"))
            continue
        decl = declares(effect, tint)
        asset = effect.split("/")[-1].split(".")[0]
        if decl is None:
            rows.append((BROKEN, cue, f"{asset}: package not found on disk"))
        elif decl:
            rows.append((APPLIED, cue, f"{asset} declares {tint}"))
        else:
            colourish = [p for p in user_params(effect) if re.search(r"colou?r|tint|team", p, re.I)]
            why = f"has {', '.join(colourish)}" if colourish else "declares NO colour parameter at all"
            rows.append((SILENT, cue, f"{asset} does NOT declare {tint} ({why})"))

    for state, name, detail in rows:
        print(f"  {ICON[state]} {name:28} {detail}")
    silent = [r for r in rows if r[0] == SILENT]
    if silent:
        findings.append(f"{len(silent)} cue(s) write a tint no FX asset declares — colour is silently dropped")

    # ------------------------------------------------------- 2. character materials
    print("\n=== CHARACTER BODY — team materials on the 3P mannequin ===")
    ch = "/Script/BreachpointNext.Default__BNCharacter"
    keys = ["allyTorsoMaterial", "allyHeadLegsMaterial", "threatTorsoMaterial",
            "threatHeadLegsMaterial", "torsoSlotName", "headLegsSlotName"]
    d = get(ch, keys)
    if not d:
        print("  FAIL  BNCharacter CDO unreadable")
        findings.append("BNCharacter CDO unreadable")
    else:
        for k in keys[:4]:
            path = ref(d.get(k))
            if not path:
                print(f"  --   {k:24} UNSET — that slot keeps the mesh's shipped material")
                findings.append(f"{k} is unset")
                continue
            rv, txt = m.call(ASSET, "load_asset", {"asset_path": path})
            ok = bool(rv) and "FAILED" not in str(txt)
            print(f"  {'OK  ' if ok else 'FAIL'} {k:24} {path.split('/')[-1]}")
            if not ok:
                findings.append(f"{k} points at an asset that will not load: {path}")

        # The slot names must exist on the mesh the BP child actually assigns, and the
        # material families must not be crossed -- SKM_Manny's torso carries MI_Manny_02
        # and its head/legs MI_Manny_01, so an index-order pairing swaps them silently.
        mesh = ref(get("/Game/BN/Characters/BP_BNCharacter.Default__BP_BNCharacter_C:CharacterMesh0",
                       ["SkeletalMeshAsset"]).get("SkeletalMeshAsset"))
        if not mesh:
            # A SILENT SKIP here would be the exact bug this file exists to catch: the four
            # materials would still print OK and the slot pairing — the part that can be
            # invisibly swapped — would simply never be checked.
            print("  FAIL BP_BNCharacter's mesh component is unreadable — SLOT PAIRING NOT CHECKED")
            findings.append("could not read BP_BNCharacter's mesh: the slot pairing went unverified")
        if mesh:
            mats = get(mesh, ["Materials"]).get("Materials", [])
            slots = {s["materialSlotName"]: ref(s.get("materialInterface")).split("/")[-1].split(".")[0]
                     for s in mats}
            print(f"  ..   mesh {mesh.split('/')[-1]} slots: {', '.join(slots) or '<none>'}")
            for slot_key, mat_key in [("torsoSlotName", "allyTorsoMaterial"),
                                      ("headLegsSlotName", "allyHeadLegsMaterial")]:
                slot = d.get(slot_key) or ""
                if slot not in slots:
                    print(f"  FAIL slot '{slot}' is not on the mesh — that slot keeps its shipped material")
                    findings.append(f"configured slot '{slot}' does not exist on {mesh.split('/')[-1]}")
                    continue
                family = slots[slot].rsplit("_", 1)[0]          # MI_Manny_02
                mine = ref(d.get(mat_key)).split("/")[-1].split(".")[0]
                if mine and not mine.startswith(family):
                    print(f"  FAIL slot '{slot}' ships {family} but config gives {mine} — SWAPPED")
                    findings.append(f"slot '{slot}' is paired with the wrong material family ({mine} vs {family})")
                else:
                    print(f"  OK   slot '{slot}' ships {family}, config gives {mine}")

    # ------------------------------------------------------- 3. the projectile trail
    print("\n=== GRENADE TRAIL — the one team surface that is not a cue ===")
    d = get("/Script/BreachpointNext.Default__BNProjectile", ["trailEffect", "trailTintParameter"])
    trail, ttint = ref(d.get("trailEffect")), d.get("trailTintParameter") or ""
    if not trail:
        print("  --   no TrailEffect configured")
    else:
        decl = declares(trail, ttint)
        asset = trail.split("/")[-1].split(".")[0]
        if decl:
            print(f"  OK   {asset} declares {ttint} — the grenade in flight reads its side")
        else:
            print(f"  !!   {asset} does NOT declare {ttint}")
            findings.append(f"grenade trail tint '{ttint}' is not declared by {asset}")

    # ---------------------------------------------------------------- 4. team audio
    print("\n=== AUDIO — is anything team-relative? ===")
    print("  --   No cue exposes a team-varying Sound: every Sound key above is one asset for")
    print("       both sides. Team audio does not exist yet; this line is here so its ABSENCE")
    print("       is a recorded answer rather than an unasked question.")

    print("\n" + "=" * 78)
    if findings:
        print(f"{len(findings)} FINDING(S):")
        for f in findings:
            print(f"  - {f}")
        return 1
    print("No findings — every configured team surface is applied.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
