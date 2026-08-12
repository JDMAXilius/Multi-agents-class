"""rag.py — the retrieval half of the BREACHPOINT content pipeline.

Knowledge base = the capstone's own design documents and its shipped data
tables. Nothing here is written for the assignment: `BREACHPOINT-GDD-VERTICAL-
SLICE.md` is the document the game is being built from, and `Content/Data/*.csv`
are the tables the running game imports.

Stdlib only, on purpose. The pipeline has to run on a grader's machine with no
`pip install`, so BM25 is implemented here rather than pulled from a package.

Two ideas carry most of the quality:

1. **Chunks are headed sections, not fixed windows.** A GDD is already
   structured; `### 2.7 Information Without Radar` is a semantically complete
   unit and splitting it at 512 characters would cut the "the slice cuts it"
   sentence away from the heading that makes it findable. Every chunk keeps its
   heading trail, its source file, and its line span, so a retrieval can be
   opened in an editor and checked.

2. **Chunks are tagged `slice` or `phase2`.** BREACHPOINT ships a vertical
   slice; a second GDD describes the Phase-2 game with systems the slice
   explicitly cuts (motion tracker, more weapons, more modes). Both are true
   documents about the same game, which is exactly what makes an untagged
   knowledge base dangerous: retrieval happily returns a cut system as canon.
   The tag lets the pipeline scope retrieval and lets the critic tell a lore
   break from a Phase-2 reference. See README §"The retrieval tweak".
"""

from __future__ import annotations

import csv
import io
import math
import re
from collections import Counter
from dataclasses import dataclass, field
from pathlib import Path

# --------------------------------------------------------------------------
# Where the knowledge base lives. Paths are relative to the repo root so the
# KB is the real project, not a copy that can drift away from it.
# --------------------------------------------------------------------------

KB_SOURCES = [
    # (relative path, canon scope, weight)
    ("breachpoint/BREACHPOINT-GDD-VERTICAL-SLICE.md", "slice", 1.0),
    ("breachpoint/Content/Data/DT_SpotterLines.csv", "slice", 1.0),
    ("breachpoint/Content/Data/DT_Medals.csv", "slice", 1.0),
    ("breachpoint/Content/Data/DT_Weapons.csv", "slice", 1.0),
    ("breachpoint/Content/Data/DT_BotTuning.csv", "slice", 1.0),
    ("breachpoint/Content/Data/DT_MatchRules.csv", "slice", 1.0),
    ("breachpoint/Content/Data/arena_manifest.json", "slice", 1.0),
    # The SHIPPED telemetry schema. Added after the first run shipped coach
    # lines keyed to nine stats the game does not record: GDD Appendix C
    # describes the telemetry we WANT, and the header is the telemetry we HAVE.
    # A knowledge base of design documents alone cannot tell those apart, which
    # is the sharpest retrieval lesson this pipeline produced. See README §6.
    ("breachpoint/Source/Breachpoint/Telemetry/BRTelemetrySubsystem.h", "slice", 1.0),
    ("breachpoint/BREACHPOINT-GDD-FULL-CONCEPT.md", "phase2", 1.0),
]

# Words that carry no retrieval signal in a design document. Deliberately short:
# an aggressive stoplist in a small corpus removes more signal than noise.
STOPWORDS = {
    "the", "a", "an", "and", "or", "of", "to", "in", "is", "are", "it", "its",
    "for", "on", "at", "by", "with", "as", "that", "this", "be", "from", "not",
    "but", "if", "so", "than", "then", "into", "over", "per", "one", "two",
}

TOKEN_RE = re.compile(r"[a-z0-9][a-z0-9_.]*")

MAX_CHUNK_CHARS = 1800     # a long GDD section is split at paragraph joins
CSV_ROWS_PER_CHUNK = 24


def tokenize(text: str) -> list[str]:
    return [t for t in TOKEN_RE.findall(text.lower()) if t not in STOPWORDS]


@dataclass
class Chunk:
    id: str
    source: str          # repo-relative path — a citation you can open
    heading: str         # heading trail, e.g. "2. Game Mechanics > 2.7 Information Without Radar"
    canon: str           # "slice" | "phase2"
    line_start: int
    line_end: int
    text: str
    tokens: list[str] = field(default_factory=list, repr=False)

    @property
    def citation(self) -> str:
        return f"{self.source}:{self.line_start}-{self.line_end}"

    def to_dict(self) -> dict:
        return {"id": self.id, "source": self.source, "heading": self.heading,
                "canon": self.canon, "citation": self.citation, "text": self.text}


# --------------------------------------------------------------------------
# Chunking
# --------------------------------------------------------------------------

HEADING_RE = re.compile(r"^(#{1,4})\s+(.*?)\s*$")


def chunk_markdown(path: Path, rel: str, canon: str) -> list[Chunk]:
    """Split on markdown headings, keeping the heading trail with each chunk."""
    lines = path.read_text(encoding="utf-8").splitlines()
    trail: dict[int, str] = {}
    sections: list[tuple[str, int, list[str]]] = []
    cur_heading: str | None = None
    cur_start: int = 1
    cur: list[str] = []

    def flush(end_line: int):
        if cur and any(s.strip() for s in cur):
            sections.append((cur_heading or path.stem, cur_start, list(cur)))

    for i, line in enumerate(lines, start=1):
        m = HEADING_RE.match(line)
        if m:
            flush(i - 1)
            level, title = len(m.group(1)), m.group(2)
            trail[level] = title
            for deeper in [k for k in trail if k > level]:
                trail.pop(deeper)
            cur_heading = " > ".join(trail[k] for k in sorted(trail))
            cur_start, cur = i, []
        else:
            cur.append(line)
    flush(len(lines))

    chunks: list[Chunk] = []
    for heading, start, body in sections:
        text = "\n".join(body).strip()
        if not text:
            continue
        for part_i, (ptext, poffset) in enumerate(_split_long(body)):
            if not ptext.strip():
                continue
            nlines = ptext.count("\n") + 1
            chunks.append(Chunk(
                id=f"{rel}#{start}.{part_i}", source=rel, heading=heading,
                canon=canon, line_start=start + poffset,
                line_end=start + poffset + nlines,
                text=f"{heading}\n\n{ptext}".strip()))
    return chunks


def _split_long(body: list[str]) -> list[tuple[str, int]]:
    """Split an over-long section at blank lines. Returns (text, line offset)."""
    joined = "\n".join(body)
    if len(joined) <= MAX_CHUNK_CHARS:
        return [(joined.strip(), 0)]
    parts, buf, buf_start, offset = [], [], 0, 0
    for idx, line in enumerate(body):
        buf.append(line)
        offset += 1
        if len("\n".join(buf)) >= MAX_CHUNK_CHARS and not line.strip():
            parts.append(("\n".join(buf).strip(), buf_start))
            buf, buf_start = [], idx + 1
    if buf:
        parts.append(("\n".join(buf).strip(), buf_start))
    return [p for p in parts if p[0]]


def chunk_csv(path: Path, rel: str, canon: str) -> list[Chunk]:
    """A data table chunks as header + row groups.

    These chunks are what make the generated content sound like the game rather
    than like a shooter: `DT_SpotterLines.csv` IS the authored announcer voice,
    so retrieving it hands the generator real exemplars instead of an adjective.
    """
    raw = path.read_text(encoding="utf-8")
    rows = list(csv.reader(io.StringIO(raw)))
    if not rows:
        return []
    header, body = rows[0], rows[1:]
    header_line = ",".join(header)
    chunks = []
    for i in range(0, len(body), CSV_ROWS_PER_CHUNK):
        group = body[i:i + CSV_ROWS_PER_CHUNK]
        text = "\n".join([f"{path.name} (existing shipped table)", header_line]
                         + [",".join(r) for r in group])
        chunks.append(Chunk(
            id=f"{rel}#rows{i}", source=rel, heading=path.name, canon=canon,
            line_start=i + 2, line_end=i + 1 + len(group), text=text))
    return chunks


def chunk_json(path: Path, rel: str, canon: str) -> list[Chunk]:
    """Top-level keys of a manifest become chunks; arrays of named objects split."""
    import json
    data = json.loads(path.read_text(encoding="utf-8"))
    chunks = []
    for key, value in data.items():
        text = f"{path.name} :: {key}\n{json.dumps(value, indent=1)}"
        for part_i in range(0, max(1, len(text)), MAX_CHUNK_CHARS):
            piece = text[part_i:part_i + MAX_CHUNK_CHARS]
            if not piece.strip():
                continue
            chunks.append(Chunk(
                id=f"{rel}#{key}.{part_i}", source=rel, heading=f"{path.name} :: {key}",
                canon=canon, line_start=1, line_end=1, text=piece))
    return chunks


# --------------------------------------------------------------------------
# BM25
# --------------------------------------------------------------------------

class Index:
    """Okapi BM25 over the knowledge base.

    BM25 rather than embeddings for three reasons that are true of this corpus
    specifically, not of retrieval in general: the corpus is ~200 chunks (recall
    is not the constraint), the queries are full of rare exact tokens that
    lexical search is *better* at than semantic search (`Kill.Rocket.Multi`,
    `Grappleshot`, `DT_SpotterLines`), and it adds no dependency and no network
    call. An embedding index would be the right answer at 10,000 chunks; here it
    would be a worse retriever with more moving parts.
    """

    K1 = 1.5
    B = 0.75

    def __init__(self, chunks: list[Chunk]):
        self.chunks = chunks
        for c in self.chunks:
            c.tokens = tokenize(c.text)
        self.N = len(self.chunks)
        self.avgdl = (sum(len(c.tokens) for c in self.chunks) / self.N) if self.N else 0.0
        self.df: Counter = Counter()
        for c in self.chunks:
            self.df.update(set(c.tokens))
        self.tf = [Counter(c.tokens) for c in self.chunks]

    def _idf(self, term: str) -> float:
        n = self.df.get(term, 0)
        if n == 0:
            return 0.0
        return math.log(1 + (self.N - n + 0.5) / (n + 0.5))

    def search(self, query: str, k: int = 6, *, scope: str | None = None,
               boost: dict[str, float] | None = None) -> list[tuple[Chunk, float]]:
        """Return the top-k (chunk, score).

        `scope="slice"` restricts retrieval to shipped-slice canon — the
        retrieval tweak documented in the README. `boost` multiplies the score
        of chunks whose source path contains a key, which is how a job says
        "the voice exemplars matter more than the prose about them".
        """
        qt = tokenize(query)
        scored = []
        for i, c in enumerate(self.chunks):
            if scope and c.canon != scope:
                continue
            dl = len(c.tokens) or 1
            s = 0.0
            for term in qt:
                f = self.tf[i].get(term, 0)
                if not f:
                    continue
                s += self._idf(term) * (f * (self.K1 + 1)) / (
                    f + self.K1 * (1 - self.B + self.B * dl / self.avgdl))
            if boost:
                for frag, mult in boost.items():
                    if frag in c.source:
                        s *= mult
            if s > 0:
                scored.append((c, s))
        scored.sort(key=lambda p: -p[1])
        return scored[:k]


def resolve_source(repo_root: Path, rel: str) -> Path:
    """Find a KB file in the repo, or in the `kb/` mirror a zip carries.

    The knowledge base is the live project, which is the whole point — but a
    submitted zip has no parent repo, so `make_submission.sh` mirrors the eight
    sources into `kb/` and this falls back to them. Citations keep the
    repo-relative path either way, so a line reference means the same thing in
    both layouts.
    """
    direct = repo_root / rel
    if direct.exists():
        return direct
    mirrored = Path(__file__).resolve().parent / "kb" / Path(rel).name
    if mirrored.exists():
        return mirrored
    raise SystemExit(f"error: knowledge-base source missing: {rel} "
                     f"(looked in {direct} and {mirrored})")


def build_index(repo_root: Path, sources=KB_SOURCES) -> Index:
    chunks: list[Chunk] = []
    for rel, canon, _weight in sources:
        path = resolve_source(repo_root, rel)
        if path.suffix == ".md":
            chunks += chunk_markdown(path, rel, canon)
        elif path.suffix == ".csv":
            chunks += chunk_csv(path, rel, canon)
        elif path.suffix == ".json":
            chunks += chunk_json(path, rel, canon)
        elif path.suffix in (".h", ".cpp"):
            chunks += chunk_cpp(path, rel, canon)
    return Index(chunks)


CPP_DECL_RE = re.compile(r"^(?:USTRUCT|UENUM|UCLASS)\s*\(|^(?:struct|enum class|class)\s+\w+")


def chunk_cpp(path: Path, rel: str, canon: str) -> list[Chunk]:
    """One chunk per struct/enum/class declaration.

    A header is in the knowledge base for one reason: it is the only place that
    says what the game ACTUALLY records, as opposed to what a design document
    says it will. Chunking per declaration keeps a struct's field list intact,
    which is the unit a generator needs to key content to real data.
    """
    lines = path.read_text(encoding="utf-8").splitlines()
    starts = [i for i, l in enumerate(lines) if CPP_DECL_RE.match(l.strip())]
    if not starts:
        return [Chunk(id=f"{rel}#0", source=rel, heading=path.name, canon=canon,
                      line_start=1, line_end=len(lines),
                      text=f"{path.name}\n" + "\n".join(lines))]
    starts.append(len(lines))
    chunks = []
    for i in range(len(starts) - 1):
        a, b = starts[i], starts[i + 1]
        body = "\n".join(lines[a:b]).strip()
        if not body:
            continue
        name = re.search(r"\b(?:struct|enum class|class)\s+(\w+)", body)
        heading = f"{path.name} :: {name.group(1)}" if name else path.name
        chunks.append(Chunk(
            id=f"{rel}#{a}", source=rel, heading=heading, canon=canon,
            line_start=a + 1, line_end=b,
            text=f"{heading}  (the SHIPPED schema — what the game records today)\n{body}"))
    return chunks


def format_context(hits: list[tuple[Chunk, float]]) -> str:
    """The retrieved context block, exactly as the generator receives it."""
    out = []
    for rank, (c, score) in enumerate(hits, 1):
        out.append(f"[{rank}] {c.citation}  (canon: {c.canon}, bm25 {score:.2f})\n{c.text}")
    return "\n\n---\n\n".join(out)


if __name__ == "__main__":   # tiny self-check: `python3 rag.py "rocket"`
    import sys
    root = Path(__file__).resolve().parents[2]
    idx = build_index(root)
    q = " ".join(sys.argv[1:]) or "rocket launcher power weapon respawn timer"
    print(f"{len(idx.chunks)} chunks indexed · query: {q!r}\n")
    for c, s in idx.search(q, k=5):
        print(f"  {s:6.2f}  {c.canon:6}  {c.citation}  — {c.heading[:60]}")
