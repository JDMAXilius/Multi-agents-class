#!/usr/bin/env python3
"""BP15 F1 gate -- does every landed unit have a blackboard that authorised it?

    python3 check_authorisation.py             audit the real repo (exit 1 on violation)
    python3 check_authorisation.py --selftest  red-then-green over synthetic git fixtures
    python3 check_authorisation.py --json      machine-readable result on stdout

THE FINDING THIS FILE CLOSES (F1, BP15 step 6, executed and proven by the critic).
The blackboard authorises unit X; a builder lands unit Y instead; NOTHING in the repo notices.
`architect.py --all` exits 0, `build_state.py` exits 0, and docs/BUILD-STATE.md lists Y under
BUILT as though it were sanctioned. `build_state.py` reads blackboard/ only for a date string.
Worse, the next `--scan` erases the trace: Y is now BUILT, scores -1000, and sinks out of sight.

BP15's stated law is "nothing reaches the codebase unlogged" and its step 3 says the blackboard
is written BEFORE anything else happens. Until this file existed that was an intention with no
mechanism. This file is the mechanism. It is a GATE: it exits nonzero on any violation.

WHAT IT COMPARES
  authorisation record : Tools/architect/blackboard/*.md   (+ blackboards recovered from git
                         history, because the live directory is overwritten -- see below)
  unit states          : Tools/architect/state/perception.json
  what actually landed : `git log --diff-filter=AR --name-status`, `git diff --diff-filter=A
                         HEAD`, and `git ls-files --others` over Source/Breachpoint/

TWO VIOLATION CLASSES
  F1  UNAUTHORISED         -- a unit is BUILT, or its source files were added, and no blackboard
                              anywhere (disk or git history) ever named it.
  F2  BLACKBOARD-AFTER-SRC -- the blackboard that supposedly authorised a unit is LATER than the
                              source it authorised. Authorising after the fact is not
                              authorising. See EVIDENCE STRENGTH below -- this one is graded,
                              because the naive signal for it is worthless.

EVIDENCE STRENGTH (read this before believing an F2 result).
The blackboard filename is DAY-GRANULAR (`<YYYY-MM-DD>-<unit>.md`, written by
`architect.py:blackboard()` from `datetime.now(timezone.utc).strftime("%Y-%m-%d")`) and the
file is OVERWRITTEN in place on every re-run of `--blackboard` on the same day. So filesystem
mtime says only "the architect ran at some point", never "the authorisation happened then":
every blackboard on disk in this repo has an mtime later than every source file in it, and an
mtime-only F2 check would report the entire codebase as a violation. It is worthless as a gate.

So F2 is graded and only the strong grades fail the gate:
  STRONG  (fails the gate)  both the blackboard and the source have a git commit that first
                            introduced them, and the blackboard's commit is strictly later.
                            Git timestamps are the strongest evidence available here: they are
                            second-granular, they survive the file being overwritten or
                            deleted, and they are what "the blackboard was written BEFORE
                            generation" actually means in a repo.
  MEDIUM  (fails the gate)  no commit for one side, but the blackboard's FILENAME DATE is a
                            strictly later DAY than the source's commit day. Day granularity
                            cannot order events inside one day, so same-day is never MEDIUM.
  WEAK    (WARNS only)      filesystem mtime is the only signal left. Reported, never fatal,
                            for the reason above. A WEAK finding is a prompt to commit the
                            blackboard, which is what would turn it into STRONG evidence.

THE BASELINE PROBLEM, HANDLED OUT LOUD.
Almost every unit here was built before the architect existed. A naive check reports ~31
violations, which is the same as reporting none. So there is an explicit CUTOFF: the first
commit that introduced Tools/architect/ -- the instant the authorisation mechanism became
mechanically possible. Everything whose source landed before it is reported as
`PRE-ARCHITECT (not a violation)` and counted SEPARATELY. It is PRINTED, with its SHA, and
every pre-architect unit is listed by name. Per game-lead, a silent cap reads as "covered
everything", so nothing here is silently excluded -- the cutoff is an argument you can check,
not a filter you cannot see.

BLACKBOARDS ARE RECOVERED FROM GIT, NOT JUST READ OFF DISK.
`--blackboard` writes one file per run and the directory is not append-only, so yesterday's
authorisation can be gone tomorrow. In this repo the blackboard that authorised BRGA_WeaponFire
(commit 395a312) no longer exists on disk -- git records the current file as a RENAME of it
(R057, commit 3773f30). Reading only the live directory would have lost that authorisation and
manufactured a false F1. So the authorisation record is (disk) UNION (every path that ever
existed under blackboard/ in git history), and a record present in history but missing from
disk is reported as an erased trace.

Deterministic. Zero network imports -- no module below opens a socket, and no model is
consulted. `sys.stdout.reconfigure(encoding="utf-8", errors="replace")` runs at import because
cp1252 has bitten this repo three times today and the blackboard format is full of em dashes.

Exit codes: 0 clean (gate passes) - 1 violations found (gate fails) - 2 usage/IO/no-git.
"""

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from datetime import datetime
from pathlib import Path

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
if hasattr(sys.stderr, "reconfigure"):
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")

HERE = Path(__file__).resolve().parent
REPO = HERE.parent.parent                       # Tools/architect/ -> repo root

EXIT_OK, EXIT_VIOLATION, EXIT_ERROR = 0, 1, 2

# The blackboard header is written with EM DASHES (U+2014) by architect.py. Accept en dash and
# hyphen too so a hand-written blackboard is not rejected on a typography detail.
DASH = "[—–-]"
BB_FILENAME_RE = re.compile(r"^(\d{4}-\d{2}-\d{2})-(BR[A-Za-z0-9_]+)\.md$")
BB_HEADER_RE = re.compile(r"^#\s*Blackboard\s*" + DASH + r"\s*(BR[A-Za-z0-9_]+)\s*"
                          + DASH + r"\s*(\d{4}-\d{2}-\d{2})", re.M)
BB_SELECTED_RE = re.compile(r"\*\*Selected:\s*`(BR[A-Za-z0-9_]+)`\*\*")
BB_OWNER_RE = re.compile(r"\|\s*owner_path\s*\|\s*`([^`]+)`")


def log(msg=""):
    print(msg, flush=True)


def rel(p, root=None):
    try:
        return Path(p).resolve().relative_to(root or REPO).as_posix()
    except (ValueError, OSError):
        return str(p)


def iso(dt):
    return "-" if dt is None else dt.astimezone().strftime("%Y-%m-%d %H:%M:%S%z")


# ======================================================================================
# git -- the only source of "when did this actually land"
# ======================================================================================

def git(git_root, args):
    """Run git; return stdout, or None if git is unusable. Never raises."""
    if git_root is None:
        return None
    try:
        r = subprocess.run(["git", "-c", "core.quotepath=false", *args],
                           cwd=str(git_root), capture_output=True, text=True,
                           encoding="utf-8", errors="replace",
                           env={**os.environ, "GIT_OPTIONAL_LOCKS": "0"})
    except (OSError, ValueError):
        return None
    return r.stdout if r.returncode == 0 else None


def git_toplevel(start):
    try:
        r = subprocess.run(["git", "rev-parse", "--show-toplevel"], cwd=str(start),
                           capture_output=True, text=True, encoding="utf-8", errors="replace")
    except OSError:
        return None
    if r.returncode != 0 or not r.stdout.strip():
        return None
    return Path(r.stdout.strip()).resolve()


def gitrel(git_root, p):
    """Path relative to the GIT ROOT, posix. Note the git root is NOT the repo root here:
    breachpoint/ is a subdirectory of the enclosing git repo, so every pathspec below has to
    be rebased or git silently resolves it against the wrong directory (it did, once, while
    this file was being written -- `git ls-files -- breachpoint/Source` from inside
    breachpoint/ warns about breachpoint/breachpoint/ and returns nothing, which reads exactly
    like "no untracked files")."""
    try:
        return Path(p).resolve().relative_to(git_root).as_posix()
    except (ValueError, OSError):
        return None


def first_appearance_map(git_root, rel_dirs):
    """{path -> record} for the first commit that introduced each path under rel_dirs.

    `--diff-filter=A` ALONE IS NOT ENOUGH and that is not hypothetical: this repo's live
    blackboard is recorded as R057 (a rename of the previous day's file), so an A-only scan
    finds no origin for it at all and would report the unit it authorises as unauthorised.
    Renames are therefore collected too, and resolved back to the originating add.
    """
    out = {}
    if not rel_dirs:
        return out
    txt = git(git_root, ["log", "--diff-filter=AR", "--name-status", "--date-order",
                         "--format=%x01%H%x1f%aI%x1f%cI%x1f%s", "--", *rel_dirs])
    if txt is None:
        return out
    sha = adate = cdate = subject = None
    for line in txt.splitlines():
        if line.startswith("\x01"):
            sha, adate, cdate, subject = (line[1:].split("\x1f") + ["", "", "", ""])[:4]
            continue
        if not line.strip() or sha is None:
            continue
        parts = line.split("\t")
        status = parts[0]
        origin = None
        if status.startswith("R") and len(parts) >= 3:
            origin, path = parts[1], parts[2]
        elif len(parts) >= 2:
            path = parts[1]
        else:
            continue
        # git log walks newest-first, so the LAST record written for a path is its earliest.
        out[path] = {"sha": sha, "adate": _dt(adate), "cdate": _dt(cdate),
                     "subject": subject, "status": status[0], "origin": origin, "path": path}
    return out


def _dt(s):
    try:
        return datetime.fromisoformat(s.strip())
    except (ValueError, AttributeError):
        return None


def resolve_origin(fam, path, depth=0):
    """Follow a rename chain back to the commit that really introduced the content.

    A file moved after the cutoff is not a new landing, and charging its mover with an
    unauthorised build would be a false positive of exactly the kind that makes gates get
    switched off. Returns (record, chain_of_paths).
    """
    rec = fam.get(path)
    chain = [path]
    while rec is not None and rec.get("status") == "R" and rec.get("origin") and depth < 20:
        nxt = fam.get(rec["origin"])
        chain.append(rec["origin"])
        if nxt is None:
            break
        rec, depth = nxt, depth + 1
    return rec, chain


class GitFacts:
    """Everything the audit knows about what git says landed, gathered in four calls."""

    def __init__(self, git_root, src_root, bb_dir, architect_dir):
        self.root = git_root
        self.ok = git_root is not None
        self.src_rel = gitrel(git_root, src_root) if self.ok else None
        self.bb_rel = gitrel(git_root, bb_dir) if self.ok else None
        self.arch_rel = gitrel(git_root, architect_dir) if self.ok else None
        dirs = [d for d in (self.src_rel, self.bb_rel) if d]
        self.first = first_appearance_map(git_root, dirs) if self.ok else {}

        self.untracked, self.staged_adds, self.tracked = set(), set(), set()
        if self.ok and dirs:
            u = git(git_root, ["ls-files", "--others", "--exclude-standard", "--", *dirs])
            self.untracked = set(filter(None, (u or "").splitlines()))
            # `git diff` against HEAD: files added in the index/worktree since the last commit.
            d = git(git_root, ["diff", "--name-status", "--diff-filter=A", "HEAD", "--", *dirs])
            for line in (d or "").splitlines():
                p = line.split("\t")
                if len(p) >= 2:
                    self.staged_adds.add(p[-1])
            t = git(git_root, ["ls-files", "--", *dirs])
            self.tracked = set(filter(None, (t or "").splitlines()))

    def landing(self, abspath):
        """When did this file land, and how sure are we? -> dict."""
        p = gitrel(self.root, abspath) if self.ok else None
        if p is None:
            return {"when": _mtime(abspath), "grade": "WEAK", "how": "mtime (outside git)",
                    "sha": None}
        if p in self.untracked or (p not in self.tracked and p not in self.staged_adds
                                   and p not in self.first):
            return {"when": _mtime(abspath), "grade": "UNCOMMITTED",
                    "how": "on disk, NEVER COMMITTED", "sha": None}
        if p in self.staged_adds:
            return {"when": _mtime(abspath), "grade": "UNCOMMITTED",
                    "how": "added vs HEAD, not yet committed", "sha": None}
        rec, chain = resolve_origin(self.first, p)
        if rec is None:
            return {"when": _mtime(abspath), "grade": "WEAK", "how": "tracked, no add found",
                    "sha": None}
        how = "git add-commit"
        if len(chain) > 1:
            how = "git add-commit via rename <- " + " <- ".join(chain[1:])
        return {"when": rec["adate"], "grade": "STRONG", "how": how, "sha": rec["sha"][:7],
                "subject": rec.get("subject", "")}


def _mtime(p):
    try:
        return datetime.fromtimestamp(Path(p).stat().st_mtime).astimezone()
    except OSError:
        return None


# ======================================================================================
# The authorisation record
# ======================================================================================

def parse_blackboard(name, text, source):
    """One blackboard -> its authorisation record, with the filename cross-checked against the
    file's own contents.

    The unit in the FILENAME and the unit the body SELECTS must agree. If they do not, the
    record is not trustworthy in either direction and is a violation in its own right: an
    authorisation you cannot read unambiguously authorises nothing. Note the body also contains
    a top-12 TABLE naming eleven other units -- those are candidates, not authorisations, and
    are deliberately never parsed as such.
    """
    rec = {"file": name, "source": source, "unit": None, "date": None, "owner": None,
           "problems": [], "body_unit": None, "header_date": None}
    m = BB_FILENAME_RE.match(name)
    if not m:
        rec["problems"].append(f"filename {name!r} is not <YYYY-MM-DD>-<BRUnit>.md")
        return rec
    rec["date"], rec["unit"] = m.group(1), m.group(2)
    if text is None:
        rec["problems"].append("contents unavailable -- filename taken on trust")
        return rec

    h = BB_HEADER_RE.search(text)
    s = BB_SELECTED_RE.search(text)
    o = BB_OWNER_RE.search(text)
    if o:
        rec["owner"] = o.group(1)
    if h:
        rec["body_unit"], rec["header_date"] = h.group(1), h.group(2)
    else:
        rec["problems"].append("no `# Blackboard - <unit> - <date>` header")
    if s:
        rec["selected"] = s.group(1)
    else:
        rec["problems"].append("no `**Selected: `<unit>`**` line")

    for label, got in (("header", rec.get("body_unit")), ("Selected", rec.get("selected"))):
        if got and got != rec["unit"]:
            rec["problems"].append(
                f"filename says {rec['unit']}, {label} says {got} -- the record contradicts "
                f"itself, so it authorises neither")
    if rec.get("header_date") and rec["header_date"] != rec["date"]:
        rec["problems"].append(
            f"filename dated {rec['date']}, header dated {rec['header_date']}")
    return rec


def collect_blackboards(bb_dir, gf):
    """Disk UNION git history. See the module docstring: the live directory is overwritten."""
    records = []
    seen_paths = set()

    for p in sorted(bb_dir.glob("*.md")) if bb_dir.is_dir() else []:
        try:
            text = p.read_text(encoding="utf-8", errors="replace")
        except OSError:
            text = None
        r = parse_blackboard(p.name, text, "disk")
        r["path"] = p
        r["landing"] = gf.landing(p)
        gr = gitrel(gf.root, p) if gf.ok else None
        if gr:
            seen_paths.add(gr)
        records.append(r)

    # Anything that ever existed under blackboard/ in history but is not on disk now.
    if gf.ok and gf.bb_rel:
        for path, rec in sorted(gf.first.items()):
            if not path.startswith(gf.bb_rel + "/") or path in seen_paths:
                continue
            name = path.rsplit("/", 1)[-1]
            blob = git(gf.root, ["show", f"{rec['sha']}:{path}"])
            r = parse_blackboard(name, blob, "git history (ERASED FROM DISK)")
            r["path"] = None
            origin, _chain = resolve_origin(gf.first, path)
            when = (origin or rec)["adate"]
            r["landing"] = {"when": when, "grade": "STRONG", "how": "git add-commit",
                            "sha": (origin or rec)["sha"][:7],
                            "subject": (origin or rec).get("subject", "")}
            r["erased"] = True
            records.append(r)

    records.sort(key=lambda r: (r.get("date") or "", r["file"]))
    return records


# ======================================================================================
# The audit
# ======================================================================================

def load_perception(path):
    try:
        data = json.loads(Path(path).read_text(encoding="utf-8"))
    except (OSError, ValueError) as e:
        return None, f"cannot read {path}: {e}"
    units = data.get("units")
    if not isinstance(units, list):
        return None, f"{path} has no 'units' list -- run `architect.py --scan` first"
    return units, None


def unit_files(src_root, folder, unit):
    root = Path(src_root) / folder
    if not root.is_dir():
        return []
    found = list(root.rglob(f"{unit}.h")) + list(root.rglob(f"{unit}.cpp"))
    # rglob is case-insensitive on Windows and case-sensitive on Linux; pin the stem so the
    # same run on the cloud and on the workstation agrees (same defect class as the cp1252 one).
    found = [p for p in found if p.stem == unit]
    return sorted(set(found))


def find_cutoff(gf, blackboards):
    """The instant the authorisation mechanism became possible. PRINTED, never silent."""
    arch = None
    if gf.ok and gf.arch_rel:
        txt = git(gf.root, ["log", "--reverse", "--format=%H%x1f%aI%x1f%s", "--",
                            gf.arch_rel])
        for line in (txt or "").splitlines():
            sha, adate, subject = (line.split("\x1f") + ["", "", ""])[:3]
            arch = {"sha": sha, "when": _dt(adate), "subject": subject}
            break
    dates = sorted(r["date"] for r in blackboards if r.get("date"))
    bb = None
    if dates:
        bb = {"date": dates[0],
              "when": datetime.fromisoformat(dates[0] + "T00:00:00").astimezone()}
    if arch and arch["when"]:
        return arch["when"], f"first commit under {gf.arch_rel or 'Tools/architect/'} " \
                             f"({arch['sha'][:7]} {iso(arch['when'])} \"{arch['subject']}\")", \
               arch, bb
    if bb:
        return bb["when"], f"earliest blackboard date {bb['date']} (00:00 local) -- " \
                           f"git had no commit for the architect directory", arch, bb
    return None, "NONE -- no architect commit and no blackboard; every unit is in scope", arch, bb


def audit(*, repo_root, src_root, bb_dir, perception_path, architect_dir, git_start=None):
    """Pure-ish: gathers facts and returns a result dict. Printing is main()'s job."""
    res = {"violations": [], "warnings": [], "notes": [], "units": [], "blackboards": [],
           "counts": {}, "cutoff": None, "cutoff_why": None, "error": None}

    units, err = load_perception(perception_path)
    if err:
        res["error"] = err
        return res

    git_root = git_toplevel(git_start or repo_root)
    gf = GitFacts(git_root, Path(src_root), Path(bb_dir), Path(architect_dir))
    res["git_root"] = str(git_root) if git_root else None
    if not gf.ok:
        res["warnings"].append(
            "NO GIT REPOSITORY -- every landing time falls back to filesystem mtime (WEAK). "
            "This gate is materially weaker without git; F2 cannot reach STRONG.")

    bbs = collect_blackboards(Path(bb_dir), gf)
    res["blackboards"] = bbs

    # Index authorisations by unit. A malformed record authorises nothing.
    auth = {}
    for r in bbs:
        if r["problems"] and any("contradicts itself" in p or "not <YYYY" in p
                                 for p in r["problems"]):
            res["violations"].append({
                "kind": "MALFORMED-BLACKBOARD", "unit": r.get("unit") or r["file"],
                "detail": f"{r['file']} ({r['source']}): " + "; ".join(r["problems"])})
            continue
        for p in r["problems"]:
            res["warnings"].append(f"blackboard {r['file']}: {p}")
        if r.get("unit"):
            auth.setdefault(r["unit"], []).append(r)
        if r.get("erased"):
            res["notes"].append(
                f"AUTHORISATION RECORD ERASED FROM DISK: {r['file']} authorised "
                f"{r.get('unit')} and is gone from Tools/architect/blackboard/. Recovered from "
                f"git ({r['landing'].get('sha')}). `--blackboard` overwrites the directory, so "
                f"git is the only durable copy -- commit the blackboard on the run that writes "
                f"it or the authorisation trail evaporates.")

    cutoff, why, arch_anchor, bb_anchor = find_cutoff(gf, bbs)
    res["cutoff"], res["cutoff_why"] = cutoff, why
    res["cutoff_anchors"] = {"architect": arch_anchor, "blackboard": bb_anchor}

    counts = {"PRE-ARCHITECT": 0, "AUTHORISED": 0, "AUTHORISED-PENDING": 0,
              "UNBUILT": 0, "VIOLATION": 0, "WARN": 0}

    for u in units:
        name, folder = u.get("unit"), u.get("folder", "")
        state = u.get("state", "?")
        files = unit_files(src_root, folder, name)
        landings = [(f, gf.landing(f)) for f in files]
        earliest = None
        for _f, l in landings:
            if l["when"] and (earliest is None or l["when"] < earliest[1]["when"]):
                earliest = (_f, l)
        row = {"unit": name, "folder": folder, "state": state,
               "files": [rel(f, repo_root) for f in files],
               "landed": earliest[1] if earliest else None,
               "authorisations": [], "verdict": None, "detail": ""}

        records = sorted(auth.get(name, []), key=lambda r: (r.get("date") or "", r["file"]))
        row["authorisations"] = [{"file": r["file"], "source": r["source"],
                                  "when": r["landing"].get("when"),
                                  "grade": r["landing"].get("grade"),
                                  "sha": r["landing"].get("sha")} for r in records]

        landed = bool(files) or state == "BUILT"

        # -- perception vs disk disagreement is not F1, but it is never silently swallowed.
        if state == "BUILT" and not files:
            res["warnings"].append(
                f"{name}: perception says BUILT but no {name}.h/.cpp under "
                f"{rel(Path(src_root) / folder, repo_root)}/ -- perception.json is stale or the "
                f"unit moved. Not counted as a landing.")
            counts["WARN"] += 1
        if state == "MISSING" and files:
            res["warnings"].append(
                f"{name}: perception says MISSING but {len(files)} file(s) exist on disk. "
                f"Treated as LANDED for this gate -- the disk wins over a stale scan.")
            counts["WARN"] += 1

        if not landed:
            row["verdict"] = "AUTHORISED-PENDING" if records else "UNBUILT"
            row["detail"] = ("authorised, not yet landed" if records
                             else "not built, nothing to authorise")
            counts[row["verdict"]] += 1
            res["units"].append(row)
            continue

        # -- the baseline. Explicit, printed, counted apart. Never a silent exclusion.
        if earliest and cutoff and earliest[1]["when"] and earliest[1]["when"] < cutoff:
            row["verdict"] = "PRE-ARCHITECT"
            row["detail"] = (f"first landed {iso(earliest[1]['when'])} "
                             f"({earliest[1]['how']}), before the cutoff -- the blackboard "
                             f"mechanism did not exist yet")
            counts["PRE-ARCHITECT"] += 1
            late = [(f, l) for f, l in landings
                    if l["when"] and l["when"] >= cutoff]
            if late:
                res["notes"].append(
                    f"{name} is PRE-ARCHITECT but {len(late)} of its file(s) were added after "
                    f"the cutoff: " + ", ".join(rel(f, repo_root) for f, _ in late) +
                    " -- informational, the unit itself predates the mechanism.")
            res["units"].append(row)
            continue

        if not records:
            row["verdict"] = "VIOLATION:UNAUTHORISED"
            when = iso(earliest[1]["when"]) if earliest else "unknown"
            how = earliest[1]["how"] if earliest else "no file evidence"
            row["detail"] = (f"landed {when} ({how}) with NO blackboard, on disk or in git "
                             f"history, ever naming it")
            counts["VIOLATION"] += 1
            res["violations"].append({
                "kind": "F1:UNAUTHORISED", "unit": name,
                "detail": f"{name} ({state}) {row['detail']}. Files: "
                          + (", ".join(row["files"]) or "<none; perception says BUILT>")})
            res["units"].append(row)
            continue

        # -- F2: was the authorisation written after the thing it authorised?
        best = _f2_verdict(records, earliest, cutoff)
        row["verdict"], row["detail"] = best["verdict"], best["detail"]
        if best["verdict"].startswith("VIOLATION"):
            counts["VIOLATION"] += 1
            res["violations"].append({"kind": "F2:BLACKBOARD-AFTER-SOURCE", "unit": name,
                                      "detail": f"{name}: {best['detail']}"})
        elif best["verdict"].startswith("WARN"):
            counts["WARN"] += 1
            res["warnings"].append(f"{name}: {best['detail']}")
            counts["AUTHORISED"] += 1
        else:
            counts["AUTHORISED"] += 1
        res["units"].append(row)

    res["counts"] = counts
    return res


def _f2_verdict(records, earliest, _cutoff):
    """Pick the strongest ordering evidence across every blackboard naming this unit.

    A unit is fine if ANY authorisation predates the landing -- re-authorising a unit later is
    noise, not a violation. So the verdict is the BEST case across records, and F2 fires only
    when every record that names the unit is later than the source.
    """
    if earliest is None or earliest[1]["when"] is None:
        return {"verdict": "AUTHORISED",
                "detail": "authorised; landing time unknown, ordering not checked"}
    src_when, src_grade = earliest[1]["when"], earliest[1]["grade"]
    src_day = src_when.date().isoformat()

    best, best_rank = None, -1
    RANK = {"VIOLATION-STRONG": 0, "VIOLATION-MEDIUM": 1, "WARN-WEAK": 2,
            "INCONCLUSIVE": 3, "OK": 4}
    for r in records:
        when, grade = r["landing"].get("when"), r["landing"].get("grade")
        if grade == "STRONG" and src_grade == "STRONG":
            if when <= src_when:
                v = ("OK", f"authorised by {r['file']} at {iso(when)} "
                           f"({r['landing'].get('sha')}), before the source landed "
                           f"{iso(src_when)} ({earliest[1].get('sha')}) -- STRONG (git)")
            else:
                v = ("VIOLATION-STRONG",
                     f"blackboard {r['file']} was committed {iso(when)} "
                     f"({r['landing'].get('sha')}) but the source landed EARLIER at "
                     f"{iso(src_when)} ({earliest[1].get('sha')}). Authorising after the fact "
                     f"is not authorising. Evidence: STRONG (both sides have git commits)")
        elif r.get("date") and src_grade in ("STRONG", "UNCOMMITTED", "WEAK"):
            if r["date"] > src_day:
                v = ("VIOLATION-MEDIUM",
                     f"blackboard {r['file']} is dated {r['date']}, a later DAY than the "
                     f"source's landing {src_day}. Evidence: MEDIUM (filename date is "
                     f"day-granular; a stronger check needs the blackboard committed before "
                     f"the source commit)")
            elif r["date"] < src_day:
                v = ("OK", f"authorised by {r['file']} (dated {r['date']}), a day before the "
                           f"source landed {src_day} -- MEDIUM (day-granular filename)")
            else:
                v = ("INCONCLUSIVE",
                     f"blackboard {r['file']} and the source both land on {src_day}. The "
                     f"filename is day-granular, so the order INSIDE the day is unknowable "
                     f"from it. Commit the blackboard in its own commit before the source "
                     f"commit and this becomes STRONG evidence either way")
        else:
            later = when and when > src_when
            v = (("WARN-WEAK" if later else "OK"),
                 f"only filesystem mtime is available for {r['file']} "
                 f"({iso(when)} vs source {iso(src_when)}). mtime is WEAK: `--blackboard` "
                 f"overwrites the day's file in place on every re-run, so mtime dates the last "
                 f"architect RUN, not the authorisation. "
                 + ("Reported, not fatal." if later else "No ordering problem visible."))
        if RANK[v[0]] > best_rank:
            best, best_rank = v, RANK[v[0]]

    kind, detail = best
    verdict = {"VIOLATION-STRONG": "VIOLATION:BLACKBOARD-AFTER-SOURCE",
               "VIOLATION-MEDIUM": "VIOLATION:BLACKBOARD-AFTER-SOURCE",
               "WARN-WEAK": "WARN:BLACKBOARD-MTIME-AFTER-SOURCE",
               "INCONCLUSIVE": "AUTHORISED",
               "OK": "AUTHORISED"}[kind]
    return {"verdict": verdict, "detail": detail}


# ======================================================================================
# Reporting
# ======================================================================================

def report(res):
    log("=" * 86)
    log("BP15 F1 GATE -- AUTHORISATION CHECK   (blackboard  ->  what actually landed)")
    log("=" * 86)
    log("Law under test (BP15 step 3): the blackboard is written BEFORE anything else happens,")
    log("and nothing reaches the codebase unlogged. This file is the mechanism for that law.")
    log("Deterministic - zero API calls - no network import.")

    if res["error"]:
        log(f"\nFATAL: {res['error']}")
        return EXIT_ERROR

    log("\n-- Cutoff (PRINTED, never silently applied) ------------------------------------")
    log(f"  cutoff = {iso(res['cutoff'])}")
    log(f"  from   : {res['cutoff_why']}")
    a, b = res["cutoff_anchors"]["architect"], res["cutoff_anchors"]["blackboard"]
    if a and a.get("when"):
        log(f"  anchor A (used if present): architect's first commit {a['sha'][:7]} "
            f"{iso(a['when'])}")
    if b:
        log(f"  anchor B (fallback)       : earliest blackboard date {b['date']}")
    log("  WHY: almost every unit here was built before the architect existed. Without this")
    log("  line the check reports ~31 violations, which is the same as reporting none. Units")
    log("  landing before the cutoff are counted SEPARATELY as PRE-ARCHITECT and listed by")
    log("  name below -- they are argued away, not filtered away.")
    if res.get("git_root"):
        log(f"  git root: {res['git_root']}")

    log("\n-- Authorisation record ---------------------------------------------------------")
    if not res["blackboards"]:
        log("  (none) -- nothing has ever been authorised.")
    for r in res["blackboards"]:
        mark = "!" if r["problems"] else " "
        log(f" {mark} {r['file']:<44} unit={r.get('unit') or '?':<22} "
            f"[{r['source']}] {r['landing'].get('grade','')} {iso(r['landing'].get('when'))}")
        for p in r["problems"]:
            log(f"      ! {p}")

    log("\n-- Unit ledger ------------------------------------------------------------------")
    order = {"VIOLATION:UNAUTHORISED": 0, "VIOLATION:BLACKBOARD-AFTER-SOURCE": 1,
             "WARN:BLACKBOARD-MTIME-AFTER-SOURCE": 2, "AUTHORISED": 3,
             "AUTHORISED-PENDING": 4, "UNBUILT": 5, "PRE-ARCHITECT": 6}
    for row in sorted(res["units"], key=lambda r: (order.get(r["verdict"], 9), r["unit"])):
        log(f"  {row['verdict']:<36} {row['unit']:<26} {row['folder']}/  [{row['state']}]")
        if row["verdict"] != "PRE-ARCHITECT" and row["detail"]:
            log(f"      {row['detail']}")
    log("\n  (PRE-ARCHITECT rows above are the baseline: source that predates the cutoff.)")

    for label, items in (("NOTES", res["notes"]), ("WARNINGS", res["warnings"])):
        if items:
            log(f"\n-- {label} " + "-" * (78 - len(label)))
            for i in items:
                log(f"  * {i}")

    c = res["counts"]
    log("\n" + "=" * 86)
    log("SUMMARY")
    log("=" * 86)
    log(f"  PRE-ARCHITECT (not a violation) . {c.get('PRE-ARCHITECT', 0):>4}   "
        f"built before the mechanism existed; counted separately, listed by name")
    log(f"  AUTHORISED ...................... {c.get('AUTHORISED', 0):>4}   "
        f"a blackboard named the unit before it landed")
    log(f"  AUTHORISED-PENDING .............. {c.get('AUTHORISED-PENDING', 0):>4}   "
        f"authorised, not yet landed")
    log(f"  UNBUILT ......................... {c.get('UNBUILT', 0):>4}   "
        f"not built and not authorised -- nothing owed")
    log(f"  WARNINGS ........................ {len(res['warnings']):>4}   "
        f"reported, not fatal (WEAK evidence or stale perception)")
    log(f"  VIOLATIONS ...................... {c.get('VIOLATION', 0) + sum(1 for v in res['violations'] if v['kind'] == 'MALFORMED-BLACKBOARD'):>4}")

    if res["violations"]:
        log("\n-- VIOLATIONS -------------------------------------------------------------------")
        for v in res["violations"]:
            log(f"  [{v['kind']}] {v['detail']}")
        log("\nGATE FAILS. Either the landing was never authorised, or the authorisation was")
        log("written after the fact. Both mean the blackboard is not a record of what happened.")
        return EXIT_VIOLATION

    log("\nGATE PASSES: every unit that landed after the cutoff was named by a blackboard")
    log("written before it. Nothing reached the codebase unlogged.")
    return EXIT_OK


# ======================================================================================
# Selftest -- red THEN green, over synthetic git fixtures in a temp dir.
#
# The standing lesson in this repo is that an enforcement mechanism proves nothing until it is
# run against a case it is supposed to REJECT. A gate that has only ever seen passing input is
# indistinguishable from `exit 0`. So four of the five cases below are engineered to FAIL, and
# the selftest fails if any of them passes.
# ======================================================================================

T_LEGACY = "2026-01-01T09:00:00-05:00"      # source landed long before the architect existed
T_ARCH = "2026-02-01T09:00:00-05:00"        # the architect arrives -> this is the cutoff
T_BB = "2026-03-01T09:00:00-05:00"          # blackboard committed
T_SRC = "2026-03-02T09:00:00-05:00"         # source committed the day after
T_LATE = "2026-03-03T09:00:00-05:00"        # ... or the blackboard committed the day after

HEADER = "// fixture\n#include \"x.h\"\nvoid f() { int a = 1; int b = 2; }\n"

BB_TEMPLATE = """# Blackboard — {unit} — {date}

Written by `Tools/architect/architect.py --blackboard` BEFORE any generation. If this file is
absent, nothing was authorised. Nothing reaches the codebase unlogged.

| # | unit | ticket | state | depth | blockers | tier | state | TOTAL |
|---|---|---|---|---|---|---|---|---|
| 1 | {unit} | BP01 | MISSING | 0 | 0 | 0 | 100 | **100** |
| 2 | BRDecoyCandidate | BP01 | MISSING | 0 | 0 | 0 | 100 | **99** |

**Selected: `{selected}`** — BP01, MISSING, total 100.

## What it will generate

| | |
|---|---|
| target | `Source/Breachpoint/{folder}/{unit}.h` / `.cpp` |
| owner_path | `Source/Breachpoint/{folder}/` |
| ticket | BP01 |
"""


def _w(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def _fix_git(root, args, when=None):
    env = {**os.environ,
           "GIT_AUTHOR_NAME": "fixture", "GIT_AUTHOR_EMAIL": "fixture@example.invalid",
           "GIT_COMMITTER_NAME": "fixture", "GIT_COMMITTER_EMAIL": "fixture@example.invalid",
           "GIT_CONFIG_NOSYSTEM": "1", "HOME": str(root), "USERPROFILE": str(root)}
    if when:
        env["GIT_AUTHOR_DATE"] = env["GIT_COMMITTER_DATE"] = when
    r = subprocess.run(["git", "-c", "commit.gpgsign=false", "-c", "core.autocrlf=false",
                        "-c", "init.defaultBranch=main", *args],
                       cwd=str(root), capture_output=True, text=True,
                       encoding="utf-8", errors="replace", env=env)
    if r.returncode != 0:
        raise RuntimeError(f"fixture git {args}: {r.stderr.strip()}")
    return r.stdout


def _commit(root, msg, when):
    _fix_git(root, ["add", "-A"])
    _fix_git(root, ["commit", "-q", "-m", msg], when=when)


def _perception(units):
    return json.dumps({"generated_by": "fixture", "api_calls": 0,
                       "units": [{"unit": u, "folder": f, "state": s, "ticket": "BP01",
                                  "tier": "slice"} for u, f, s in units]}, indent=2)


def build_fixture(root, case):
    """Five fixtures. One is supposed to pass; four are supposed to be rejected."""
    root.mkdir(parents=True, exist_ok=True)
    _fix_git(root, ["init", "-q", "."])

    # T_LEGACY: a unit built long before any architect. Must come out PRE-ARCHITECT, never F1 --
    # this is the baseline case that makes the gate usable at all.
    _w(root / "Source/Breachpoint/Core/BRLegacyUnit.h", "#pragma once\n")
    _w(root / "Source/Breachpoint/Core/BRLegacyUnit.cpp", HEADER)
    _commit(root, "legacy unit, no architect yet", T_LEGACY)

    # T_ARCH: the architect lands. This commit is the cutoff.
    _w(root / "Tools/architect/architect.py", "# fixture architect\n")
    units = [("BRLegacyUnit", "Core", "BUILT")]
    _w(root / "Tools/architect/state/perception.json", _perception(units))
    _commit(root, "the architect arrives", T_ARCH)

    def bb(unit, date, selected=None, folder="Core", when=None, name=None):
        _w(root / "Tools/architect/blackboard" / (name or f"{date}-{unit}.md"),
           BB_TEMPLATE.format(unit=unit, date=date, selected=selected or unit, folder=folder))
        if when:
            _commit(root, f"blackboard: {unit}", when)

    def src(unit, folder="Core", when=None):
        _w(root / f"Source/Breachpoint/{folder}/{unit}.h", "#pragma once\n")
        _w(root / f"Source/Breachpoint/{folder}/{unit}.cpp", HEADER)
        if when:
            _commit(root, f"land {unit}", when)

    if case == "A-authorised":
        bb("BRGoodUnit", "2026-03-01", when=T_BB)
        src("BRGoodUnit", when=T_SRC)
        units.append(("BRGoodUnit", "Core", "BUILT"))
    elif case == "B-unauthorised":
        src("BRRogueUnit", when=T_SRC)
        units.append(("BRRogueUnit", "Core", "BUILT"))
    elif case == "C-blackboard-after-source":
        src("BRLateUnit", when=T_SRC)
        bb("BRLateUnit", "2026-03-03", when=T_LATE)
        units.append(("BRLateUnit", "Core", "BUILT"))
    elif case == "D-uncommitted-rogue":
        # The exact F1 the critic executed: a builder lands a unit that was never authorised
        # and it is not even committed yet. Nothing else in the repo notices.
        src("BRGhostUnit")            # written to disk, never committed
        units.append(("BRGhostUnit", "Core", "BUILT"))
    elif case == "E-self-contradicting":
        # Filename authorises X, body selects Y. The record cannot be trusted either way.
        bb("BRClaimedUnit", "2026-03-01", selected="BROtherUnit", when=T_BB)
        src("BRClaimedUnit", when=T_SRC)
        units.append(("BRClaimedUnit", "Core", "BUILT"))
    else:
        raise ValueError(case)

    _w(root / "Tools/architect/state/perception.json", _perception(units))
    if case != "D-uncommitted-rogue":
        _commit(root, "perception refresh", T_LATE)
    return root


CASES = [
    # case, expect_exit, must_contain_kind, why this case exists
    ("A-authorised", EXIT_OK, None,
     "GREEN control: blackboard committed 03-01, source committed 03-02 -> authorised."),
    ("B-unauthorised", EXIT_VIOLATION, "F1:UNAUTHORISED",
     "RED: a unit lands after the cutoff with no blackboard anywhere. This is F1."),
    ("C-blackboard-after-source", EXIT_VIOLATION, "F2:BLACKBOARD-AFTER-SOURCE",
     "RED: the blackboard is committed a day AFTER the source it 'authorised'. This is F2."),
    ("D-uncommitted-rogue", EXIT_VIOLATION, "F1:UNAUTHORISED",
     "RED: the critic's exact scenario -- unit Y on disk, never authorised, never committed."),
    ("E-self-contradicting", EXIT_VIOLATION, "MALFORMED-BLACKBOARD",
     "RED: filename says BRClaimedUnit, body selects BROtherUnit -> authorises neither."),
]


def selftest(keep=False):
    log("=" * 86)
    log("SELFTEST -- red THEN green over synthetic git fixtures")
    log("=" * 86)
    log("An enforcement mechanism proves nothing until it is run with a case it should REJECT.")
    log("Four of the five fixtures below are engineered to fail; the selftest fails if any of")
    log("them passes, and fails if the one green case does not pass.\n")

    if shutil.which("git") is None:
        log("FATAL: git not on PATH -- the fixtures are real git repositories.")
        return EXIT_ERROR

    tmp = Path(tempfile.mkdtemp(prefix="bp15-authcheck-"))
    failures = []
    try:
        for case, expect, want_kind, why in CASES:
            root = build_fixture(tmp / case, case)
            res = audit(repo_root=root,
                        src_root=root / "Source/Breachpoint",
                        bb_dir=root / "Tools/architect/blackboard",
                        perception_path=root / "Tools/architect/state/perception.json",
                        architect_dir=root / "Tools/architect",
                        git_start=root)
            got = (EXIT_ERROR if res["error"]
                   else EXIT_VIOLATION if res["violations"] else EXIT_OK)
            kinds = sorted({v["kind"] for v in res["violations"]})
            ok = got == expect and (want_kind is None or want_kind in kinds)
            log(f"  [{'PASS' if ok else 'FAIL'}] {case}")
            log(f"         {why}")
            log(f"         expected exit {expect}"
                + (f" with {want_kind}" if want_kind else "")
                + f"; got exit {got} kinds={kinds or '[]'}")
            for v in res["violations"]:
                log(f"           -> {v['kind']}: {v['detail'][:150]}")
            counts = res["counts"]
            log(f"         counts: pre-architect={counts.get('PRE-ARCHITECT', 0)} "
                f"authorised={counts.get('AUTHORISED', 0)} "
                f"pending={counts.get('AUTHORISED-PENDING', 0)} "
                f"violations={counts.get('VIOLATION', 0)}")
            log(f"         cutoff: {iso(res['cutoff'])} <- {res['cutoff_why'][:70]}")
            if counts.get("PRE-ARCHITECT", 0) != 1:
                ok = False
                log("         FAIL: BRLegacyUnit should be the one PRE-ARCHITECT unit -- the "
                    "baseline handling is broken.")
            if not ok:
                failures.append(case)
            log("")
    finally:
        if keep:
            log(f"  fixtures kept at {tmp}")
        else:
            shutil.rmtree(tmp, ignore_errors=True)

    log("-" * 86)
    if failures:
        log(f"SELFTEST FAILED: {len(failures)}/{len(CASES)} -> {', '.join(failures)}")
        return EXIT_VIOLATION
    log(f"SELFTEST PASSED: {len(CASES)}/{len(CASES)} "
        f"(1 green control, {len(CASES) - 1} cases the gate correctly rejected)")
    return EXIT_OK


def main():
    ap = argparse.ArgumentParser(
        description="BP15 F1 gate: every landed unit must have a blackboard that authorised it.")
    ap.add_argument("--selftest", action="store_true",
                    help="run the red-then-green fixtures instead of auditing the repo")
    ap.add_argument("--keep-fixtures", action="store_true", help="do not delete the temp repos")
    ap.add_argument("--json", action="store_true", help="emit the result as JSON")
    a = ap.parse_args()

    if a.selftest:
        return selftest(keep=a.keep_fixtures)

    res = audit(repo_root=REPO,
                src_root=REPO / "Source" / "Breachpoint",
                bb_dir=HERE / "blackboard",
                perception_path=HERE / "state" / "perception.json",
                architect_dir=HERE)
    if a.json:
        def enc(o):
            if isinstance(o, datetime):
                return o.isoformat()
            if isinstance(o, Path):
                return str(o)
            return str(o)
        print(json.dumps(res, indent=2, default=enc, sort_keys=True))
        return (EXIT_ERROR if res["error"] else
                EXIT_VIOLATION if res["violations"] else EXIT_OK)
    return report(res)


if __name__ == "__main__":
    sys.exit(main())
