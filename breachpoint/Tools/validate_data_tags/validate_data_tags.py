#!/usr/bin/env python3
"""
validate_data_tags.py -- cross-artifact gate: do the gameplay tags named in
Content/Data/*.csv actually resolve against the native tag registry in
Source/Breachpoint/Core/BRGameplayTags.cpp?

WHY THIS EXISTS
---------------
DT_Weapons.csv carried FireCueTag = GameplayCue.Weapon.{AR,Magnum,Rocket}.Fire from
29 Jul 2026, landing with a verifier PASS, while no C++ declared any of those three
symbols. The data crew's verifier checked the CSV's *schema* -- column present, cell
non-empty, type plausible -- and a schema check cannot see the other side of a
cross-artifact reference. A dangling reference passed every gate for three days.

This tool checks the OTHER side: the string in the cell must name a tag that the
native registry actually declares.

HOW A COLUMN IS DECIDED TO BE TAG-VALUED (this is the whole design)
-------------------------------------------------------------------
The naive rule -- "a cell that looks like a dotted PascalCase tag is a tag" -- is
WRONG in this repo, and loudly so. Measured against the tree of 1 Aug 2026, shape
alone flags 85 cells (45 distinct file/value pairs) as dangling on a CLEAN repo:

  * DT_Medals.TriggerId and DT_SpotterLines.TriggerId hold `Kill.Multi.Double`,
    `Match.Phase.SuddenDeath` and 31 more. They are tag-SHAPED and they are NOT tags.
    BRDataRows.h:363 declares them `FName, not FGameplayTag, by packet instruction`
    and writes down the cost. Reporting them as dangling would be reporting a
    deliberate design decision as a defect.
  * CT_Combat.csv's first column holds `Damage.Kinetic.Multiplier` and friends. Those
    are CURVE ROW KEYS in a CurveTable that has no row struct at all
    (Tools/reimport-tables.ps1:77). A row key is an FName, never a tag reference.

A gate that is red on a clean repo gets switched off within a week, and then it
protects nothing. So shape alone never decides. Shape only nominates a CANDIDATE;
the C++ row-struct field type DECIDES. A column is treated as tag-valued when BOTH:

  1. SHAPE  -- at least one non-empty cell matches the gameplay-tag shape, and
  2. TYPE   -- the column binds to a row-struct member declared FGameplayTag or
               FGameplayTagContainer.

Every candidate that fails (2) is printed with the reason it was set aside. That is
deliberate: a silent cap reads as "covered everything", and the whole lesson of this
defect is that an unstated limit is indistinguishable from a passing check.

THE HONEST GAP IN THIS TOOL
---------------------------
Rule (2) means a tag column in a CSV with NO row-struct binding is invisible to the
resolution check -- exactly the blind spot that let the original defect land, moved
one level up. It is not hidden: such columns print as `UNBOUND` in the discovery
section, and `--strict-unbound` promotes them to gate failures. Default is off
because CT_Combat's curve keys would trip it every run, which is the switched-off-
gate failure mode again.

RESOLUTION SEMANTICS
--------------------
UE's tag tree implicitly registers every ancestor of a declared tag, so declaring
`Damage.Kinetic` also makes `Damage` a real, usable tag. This tool mirrors that:

  EXACT   -- the string is declared verbatim by UE_DEFINE_GAMEPLAY_TAG[_COMMENT]
  PARENT  -- the string is an implicit ancestor of a declared tag (resolves in UE)
  DANGLING-- neither; nothing in C++ answers to this string

EXACT and PARENT both count as RESOLVES. DANGLING fails the gate.

Empty cells, `-`, and whitespace-only cells are NOT tag references. They are the
authored way to say "this row has no cue", and are never dangles.

EXIT CODES
----------
  0  every tag reference resolved (gate PASS)
  1  at least one DANGLING reference (gate FAIL)
  2  the validator could not run: missing input, unreadable file, empty registry

Deterministic output, stdlib only, no network imports.
"""

from __future__ import annotations

import argparse
import csv
import difflib
import io
import os
import re
import sys
from dataclasses import dataclass, field
from typing import Dict, Iterable, List, Optional, Sequence, Set, Tuple

# Windows has bitten this repo twice with cp1252 encode errors on tool output
# (see BP14's Log). Do this at import, before anything can print.
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
if hasattr(sys.stderr, "reconfigure"):
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")


# --------------------------------------------------------------------------------
# Shapes
# --------------------------------------------------------------------------------

# A gameplay tag: two or more dot-separated segments, each starting uppercase.
#   GameplayCue.Weapon.AR.Fire   yes
#   Damage.Kinetic               yes
#   1.0                          no  (segment must start with a letter)
#   /Game/Weapons/AR/SM_AR.SM_AR no  (slashes, and segments start lowercase/underscore)
#   Assault Rifle                no  (space, no dot)
#   PowerWeaponAvailable         no  (no dot -- single-segment tags exist in UE but are
#                                     indistinguishable from enum names in a CSV cell, so
#                                     this tool requires a dot to nominate a candidate)
TAG_SHAPE = re.compile(r"^[A-Z][A-Za-z0-9]*(?:\.[A-Z][A-Za-z0-9]*)+$")

# UE_DEFINE_GAMEPLAY_TAG_COMMENT(Symbol, "Tag.String", "dev comment")
# UE_DEFINE_GAMEPLAY_TAG(Symbol, "Tag.String")
TAG_DEFINE = re.compile(
    r"""UE_DEFINE_GAMEPLAY_TAG(?:_COMMENT|_STATIC)?\s*\(\s*
        (?P<symbol>[A-Za-z_][A-Za-z0-9_]*)\s*,\s*
        (?:TEXT\s*\(\s*)?"(?P<tag>[^"]+)"
    """,
    re.VERBOSE,
)

# Cells that mean "no reference here".
NULL_CELLS = {"", "-", "--", "none", "None", "NONE", "null", "NULL"}

# Column names that are the DataTable row key rather than a struct field.
ROW_KEY_NAMES = {"name", "rowname", "row_name", "---"}

TAG_CPP_TYPES = {"FGameplayTag", "FGameplayTagContainer"}


# --------------------------------------------------------------------------------
# The native tag registry
# --------------------------------------------------------------------------------


@dataclass
class TagRegistry:
    """Tags declared in C++, plus the ancestors UE implicitly registers with them."""

    declared: Dict[str, str] = field(default_factory=dict)  # tag string -> C++ symbol
    source: str = ""

    @property
    def implicit_parents(self) -> Set[str]:
        parents: Set[str] = set()
        for tag in self.declared:
            segments = tag.split(".")
            for i in range(1, len(segments)):
                parents.add(".".join(segments[:i]))
        return parents - set(self.declared)

    def resolve(self, value: str) -> str:
        """Return 'EXACT', 'PARENT', or 'DANGLING'."""
        if value in self.declared:
            return "EXACT"
        if value in self.implicit_parents:
            return "PARENT"
        return "DANGLING"

    def nearest(self, value: str, n: int = 1) -> List[str]:
        """Closest declared tags, for diagnosing a typo. Deterministic."""
        pool = sorted(self.declared)
        return difflib.get_close_matches(value, pool, n=n, cutoff=0.6)


def parse_tag_registry(path: str) -> TagRegistry:
    """Read UE_DEFINE_GAMEPLAY_TAG* entries. The tag STRINGS are the source of truth."""
    try:
        with io.open(path, "r", encoding="utf-8", errors="replace") as handle:
            text = handle.read()
    except OSError as exc:
        raise ValidatorError("cannot read tag registry %s: %s" % (path, exc))

    text = strip_comments(text)
    registry = TagRegistry(source=path)
    for match in TAG_DEFINE.finditer(text):
        registry.declared[match.group("tag")] = match.group("symbol")
    return registry


def strip_comments(text: str) -> str:
    """Drop // and /* */ comments so a tag string quoted in prose is not read as declared."""
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    text = re.sub(r"//[^\n]*", "", text)
    return text


# --------------------------------------------------------------------------------
# Row structs -- the type side of the decision
# --------------------------------------------------------------------------------


@dataclass
class StructField:
    csv_column: str
    member: str
    cpp_type: str

    @property
    def is_tag_typed(self) -> bool:
        return self.cpp_type in TAG_CPP_TYPES


@dataclass
class RowStruct:
    name: str
    header: str
    fields: Dict[str, StructField] = field(default_factory=dict)  # csv column -> field

    def column_names(self) -> Set[str]:
        return set(self.fields)


# A member declaration inside a USTRUCT body. Deliberately narrow: no parentheses
# (that would be a function), no static/constexpr/using/typedef.
MEMBER_DECL = re.compile(
    r"""^\s*
        (?P<type>(?:TSoftObjectPtr|TSoftClassPtr|TArray|TMap|TSet)\s*<[^;]*?>|[A-Za-z_][A-Za-z0-9_:]*)
        \s+
        (?P<name>[A-Za-z_][A-Za-z0-9_]*)
        \s*(?:=[^;]*)?;\s*$
    """,
    re.VERBOSE,
)

STRUCT_OPEN = re.compile(
    r"\bstruct\s+(?:[A-Z_][A-Z0-9_]*_API\s+)?(?P<name>F[A-Za-z0-9_]+)\b[^;{]*\{"
)

CSV_ANNOTATION = re.compile(r"CSV:\s*`?(?P<col>[A-Za-z_][A-Za-z0-9_]*)`?")

SKIP_DECL_TOKENS = ("static", "constexpr", "using", "typedef", "friend", "return", "GENERATED_BODY")


def parse_row_structs(header_paths: Sequence[str]) -> List[RowStruct]:
    """
    Extract USTRUCT row types and their CSV column bindings.

    A member's CSV column comes from the `CSV: <Name>` annotation in its doc comment
    (the convention BRDataRows.h uses throughout), falling back to the member name --
    which is what DT_Weapons.FireCueTag relies on, since member and column agree there.
    """
    structs: List[RowStruct] = []
    for path in sorted(header_paths):
        try:
            with io.open(path, "r", encoding="utf-8", errors="replace") as handle:
                text = handle.read()
        except OSError:
            continue
        for match in STRUCT_OPEN.finditer(text):
            body = extract_braced_body(text, match.end() - 1)
            if body is None:
                continue
            struct = RowStruct(name=match.group("name"), header=path)
            harvest_fields(body, struct)
            if struct.fields:
                structs.append(struct)
    structs.sort(key=lambda s: (s.name, s.header))
    return structs


def extract_braced_body(text: str, open_index: int) -> Optional[str]:
    """Return the text between the brace at open_index and its match."""
    depth = 0
    for i in range(open_index, len(text)):
        char = text[i]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return text[open_index + 1 : i]
    return None


def harvest_fields(body: str, struct: RowStruct) -> None:
    """Walk a struct body, pairing each member declaration with its CSV annotation."""
    pending_comment: List[str] = []
    in_block_comment = False

    for raw_line in body.splitlines():
        line = raw_line.rstrip()
        stripped = line.strip()

        if in_block_comment:
            pending_comment.append(stripped)
            if "*/" in stripped:
                in_block_comment = False
            continue
        if stripped.startswith("/*"):
            pending_comment.append(stripped)
            if "*/" not in stripped:
                in_block_comment = True
            continue
        if stripped.startswith("//"):
            pending_comment.append(stripped)
            continue
        if not stripped:
            continue
        if stripped.startswith("UPROPERTY") or stripped.startswith("UFUNCTION"):
            continue  # keeps the comment buffer alive across the macro line
        if any(token in stripped for token in SKIP_DECL_TOKENS):
            pending_comment = []
            continue

        decl = MEMBER_DECL.match(line)
        if decl:
            member = decl.group("name")
            cpp_type = re.sub(r"\s+", "", decl.group("type"))
            column = member
            for comment_line in reversed(pending_comment):
                found = CSV_ANNOTATION.search(comment_line)
                if found:
                    column = found.group("col")
                    break
            struct.fields[column] = StructField(
                csv_column=column, member=member, cpp_type=cpp_type
            )
        pending_comment = []


def bind_struct(header: Sequence[str], structs: Sequence[RowStruct]) -> Tuple[Optional[RowStruct], float]:
    """
    Pick the row struct whose CSV column set best covers this CSV's header.

    Evidence-based rather than a hardcoded filename map, and the score is printed so a
    weak binding is visible instead of assumed.
    """
    candidates = [c for c in header if c.strip().lower() not in ROW_KEY_NAMES and c.strip()]
    if not candidates:
        return None, 0.0
    best: Optional[RowStruct] = None
    best_score = 0.0
    for struct in structs:  # already sorted -> deterministic ties
        overlap = len(set(candidates) & struct.column_names())
        score = overlap / float(len(candidates))
        if score > best_score:
            best, best_score = struct, score
    if best_score < 0.5:
        return None, best_score
    return best, best_score


# --------------------------------------------------------------------------------
# Column classification
# --------------------------------------------------------------------------------

# Why a shape-nominated column was or was not resolved.
TAG_VALUED = "TAG-VALUED"        # shape + FGameplayTag type -> resolved
EXEMPT_TYPE = "NOT-A-TAG"        # shape, but C++ declares a non-tag type
EXEMPT_ROWKEY = "ROW-KEY"        # shape, but it is the DataTable row name column
UNBOUND = "UNBOUND"              # shape, but no row struct binds this CSV -- blind spot


@dataclass
class ColumnVerdict:
    csv: str
    column: str
    index: int
    classification: str
    reason: str
    shaped_cells: int


@dataclass
class Reference:
    csv: str
    column: str
    row: str
    line: int
    value: str
    status: str          # EXACT | PARENT | DANGLING
    suggestion: str = ""

    @property
    def resolves(self) -> bool:
        return self.status in ("EXACT", "PARENT")


class ValidatorError(Exception):
    pass


def is_null_cell(value: str) -> bool:
    return value.strip() in NULL_CELLS


def read_csv_rows(path: str) -> Tuple[List[str], List[List[str]]]:
    try:
        with io.open(path, "r", encoding="utf-8-sig", errors="replace", newline="") as handle:
            rows = list(csv.reader(handle))
    except OSError as exc:
        raise ValidatorError("cannot read CSV %s: %s" % (path, exc))
    if not rows:
        return [], []
    return [c.strip() for c in rows[0]], rows[1:]


def classify_columns(
    csv_name: str,
    header: Sequence[str],
    rows: Sequence[Sequence[str]],
    struct: Optional[RowStruct],
    struct_score: float,
) -> List[ColumnVerdict]:
    """Decide, per column, whether it is tag-valued -- and record WHY either way."""
    verdicts: List[ColumnVerdict] = []
    for index, column in enumerate(header):
        shaped = 0
        for row in rows:
            if index < len(row):
                cell = row[index].strip()
                if not is_null_cell(cell) and TAG_SHAPE.match(cell):
                    shaped += 1
        if shaped == 0:
            continue  # no cell in this column ever looks like a tag: not a candidate

        if index == 0 or column.strip().lower() in ROW_KEY_NAMES:
            verdicts.append(
                ColumnVerdict(
                    csv_name, column, index, EXEMPT_ROWKEY,
                    "column 0 is the DataTable/CurveTable row key (an FName row name), "
                    "not a struct field, so its dotted values are not tag references",
                    shaped,
                )
            )
            continue

        if struct is None:
            verdicts.append(
                ColumnVerdict(
                    csv_name, column, index, UNBOUND,
                    "no row struct binds this CSV (best header overlap %.0f%%, need 50%%), "
                    "so no C++ type can confirm or deny that this column is tag-valued"
                    % (struct_score * 100.0),
                    shaped,
                )
            )
            continue

        bound = struct.fields.get(column)
        if bound is None:
            verdicts.append(
                ColumnVerdict(
                    csv_name, column, index, UNBOUND,
                    "%s binds this CSV but declares no member for this column" % struct.name,
                    shaped,
                )
            )
            continue

        if bound.is_tag_typed:
            verdicts.append(
                ColumnVerdict(
                    csv_name, column, index, TAG_VALUED,
                    "%s::%s is declared %s" % (struct.name, bound.member, bound.cpp_type),
                    shaped,
                )
            )
        else:
            verdicts.append(
                ColumnVerdict(
                    csv_name, column, index, EXEMPT_TYPE,
                    "%s::%s is declared %s, not a gameplay tag -- tag-shaped by convention only"
                    % (struct.name, bound.member, bound.cpp_type),
                    shaped,
                )
            )
    return verdicts


def resolve_column(
    csv_name: str,
    header: Sequence[str],
    rows: Sequence[Sequence[str]],
    verdict: ColumnVerdict,
    registry: TagRegistry,
) -> List[Reference]:
    """Resolve every non-null cell of a tag-valued column against the registry."""
    references: List[Reference] = []
    for offset, row in enumerate(rows):
        line = offset + 2  # 1-based, header is line 1
        if verdict.index >= len(row):
            continue
        value = row[verdict.index].strip()
        if is_null_cell(value):
            continue  # authored "no reference" -- never a dangle
        row_name = row[0].strip() if row else ""
        status = registry.resolve(value)
        suggestion = ""
        if status == "DANGLING":
            near = registry.nearest(value)
            if near:
                suggestion = near[0]
        references.append(
            Reference(csv_name, verdict.column, row_name, line, value, status, suggestion)
        )
    return references


# --------------------------------------------------------------------------------
# Driver
# --------------------------------------------------------------------------------


@dataclass
class Report:
    registry: TagRegistry
    verdicts: List[ColumnVerdict] = field(default_factory=list)
    references: List[Reference] = field(default_factory=list)
    csv_bindings: List[Tuple[str, str, float]] = field(default_factory=list)

    @property
    def dangling(self) -> List[Reference]:
        return [r for r in self.references if not r.resolves]

    @property
    def unbound(self) -> List[ColumnVerdict]:
        return [v for v in self.verdicts if v.classification == UNBOUND]


def run(data_dir: str, tags_file: str, source_dirs: Sequence[str]) -> Report:
    registry = parse_tag_registry(tags_file)
    if not registry.declared:
        raise ValidatorError(
            "tag registry %s declared 0 tags -- refusing to run, because an empty "
            "registry would mark every reference DANGLING and read as a real failure"
            % tags_file
        )

    headers: List[str] = []
    for source_dir in source_dirs:
        for root, _dirs, files in os.walk(source_dir):
            for name in files:
                if name.endswith(".h"):
                    headers.append(os.path.join(root, name))
    structs = parse_row_structs(headers)

    if not os.path.isdir(data_dir):
        raise ValidatorError("data directory not found: %s" % data_dir)

    report = Report(registry=registry)
    csv_paths = sorted(
        os.path.join(data_dir, n) for n in os.listdir(data_dir) if n.lower().endswith(".csv")
    )
    for path in csv_paths:
        name = os.path.basename(path)
        header, rows = read_csv_rows(path)
        if not header:
            continue
        struct, score = bind_struct(header, structs)
        report.csv_bindings.append((name, struct.name if struct else "(none)", score))
        verdicts = classify_columns(name, header, rows, struct, score)
        report.verdicts.extend(verdicts)
        for verdict in verdicts:
            if verdict.classification == TAG_VALUED:
                report.references.extend(
                    resolve_column(name, header, rows, verdict, registry)
                )
    return report


def print_report(report: Report, strict_unbound: bool) -> None:
    out = sys.stdout.write
    reg = report.registry

    out("=" * 78 + "\n")
    out("BREACHPOINT -- CSV gameplay tag reference validator\n")
    out("=" * 78 + "\n\n")

    out("TAG REGISTRY (source of truth)\n")
    out("  %s\n" % reg.source)
    out("  %d tags declared, %d implicit ancestors also resolvable\n\n"
        % (len(reg.declared), len(reg.implicit_parents)))

    out("CSV -> ROW STRUCT BINDING (the type side of the decision)\n")
    for name, struct_name, score in report.csv_bindings:
        out("  %-24s %-22s header overlap %3.0f%%\n" % (name, struct_name, score * 100.0))
    out("\n")

    out("COLUMN DISCOVERY -- every column with at least one tag-shaped cell\n")
    out("  A column is resolved only if shape AND C++ type agree. Columns set aside\n")
    out("  are listed with the reason, so the cap on coverage is visible.\n\n")
    if not report.verdicts:
        out("  (no tag-shaped cells found anywhere)\n\n")
    for verdict in sorted(report.verdicts, key=lambda v: (v.csv, v.index)):
        out("  [%s] %s :: %s  (%d tag-shaped cells)\n"
            % (verdict.classification, verdict.csv, verdict.column, verdict.shaped_cells))
        out("      %s\n" % verdict.reason)
    out("\n")

    out("RESOLUTION -- every cell of every tag-valued column\n\n")
    if not report.references:
        out("  (no tag references to resolve)\n\n")
    current = ""
    for ref in sorted(report.references, key=lambda r: (r.csv, r.column, r.line)):
        key = "%s :: %s" % (ref.csv, ref.column)
        if key != current:
            out("  %s\n" % key)
            current = key
        verdict = "RESOLVES" if ref.resolves else "DANGLING"
        detail = ref.status if ref.resolves else "no C++ declares this string"
        out("    row %-14s line %-4d %-34s %-8s (%s)\n"
            % (ref.row or "?", ref.line, ref.value, verdict, detail))
        if not ref.resolves and ref.suggestion:
            out("      -> nearest declared tag: %s\n" % ref.suggestion)
    out("\n")

    resolved = len(report.references) - len(report.dangling)
    out("-" * 78 + "\n")
    out("SUMMARY  %d reference(s) checked: %d resolve, %d DANGLING\n"
        % (len(report.references), resolved, len(report.dangling)))
    if report.unbound:
        out("         %d tag-shaped column(s) UNBOUND and therefore unchecked%s\n"
            % (len(report.unbound), " (strict: FAIL)" if strict_unbound else ""))
    out("-" * 78 + "\n")

    if report.dangling:
        out("VERDICT: FAIL -- %d dangling tag reference(s)\n" % len(report.dangling))
        for ref in sorted(report.dangling, key=lambda r: (r.csv, r.column, r.line)):
            out("  %s :: %s line %d: %s\n" % (ref.csv, ref.column, ref.line, ref.value))
    elif strict_unbound and report.unbound:
        out("VERDICT: FAIL (strict) -- %d unbound tag-shaped column(s)\n" % len(report.unbound))
    else:
        out("VERDICT: PASS -- every tag named in data resolves in C++\n")


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(
        description="Resolve tag-valued CSV columns against the native gameplay tag registry.",
    )
    default_root = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
    parser.add_argument("--repo-root", default=default_root,
                        help="repo root; defaults to two levels above this script")
    parser.add_argument("--data-dir", default=None,
                        help="directory of CSVs (default <repo-root>/Content/Data)")
    parser.add_argument("--tags-file", default=None,
                        help="path to BRGameplayTags.cpp (default under <repo-root>/Source)")
    parser.add_argument("--source-dir", action="append", default=None,
                        help="directory to scan for row-struct headers (repeatable)")
    parser.add_argument("--strict-unbound", action="store_true",
                        help="also fail when a tag-shaped column has no row-struct binding")
    args = parser.parse_args(argv)

    root = os.path.abspath(args.repo_root)
    data_dir = args.data_dir or os.path.join(root, "Content", "Data")
    tags_file = args.tags_file or os.path.join(
        root, "Source", "Breachpoint", "Core", "BRGameplayTags.cpp"
    )
    source_dirs = args.source_dir or [os.path.join(root, "Source")]
    source_dirs = [d for d in source_dirs if os.path.isdir(d)]

    try:
        report = run(data_dir, tags_file, source_dirs)
    except ValidatorError as exc:
        sys.stderr.write("validate_data_tags: ERROR: %s\n" % exc)
        return 2

    print_report(report, args.strict_unbound)

    if report.dangling:
        return 1
    if args.strict_unbound and report.unbound:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
