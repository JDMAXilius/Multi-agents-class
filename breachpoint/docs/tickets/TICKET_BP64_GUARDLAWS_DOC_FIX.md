# TICKET — BP64: The front-end board's folder split rests on a wrong claim about `guard_laws.py`

> STATUS: open — `contract_gap` G5, filed by BP26's cut, 2 Aug 2026. Doc correction only, zero
> code. Small, and it decides the shape of ~34 packets, so it is not cosmetic.

Founder directive: `docs/ui/ue-frontend/TICKETS.md` §2(a) (lines 63–65) states *"`guard_laws.py`
enforces `owner_path` at **folder** granularity, so packets sharing `Source/Breachpoint/UI/` pass
the hook and then collide on files"*, and derives an 18-folder split of `Source/Breachpoint/UI/`
and `Content/UI/` from it. **The premise is false.** `.claude/hooks/guard_laws.py:73` reads:

```python
if not any(rel.startswith(o.rstrip("/") + "/") or rel == o for o in owners):
```

`rel == o` is an **exact-file** grant. The repo already buys concurrency that way — R23
(`DESIGN-RULINGS.md:229-231`) and R25 (`:270-273`) both chose exact files *over* a folder split,
`TICKETS.md` itself uses exact paths for BP38–BP42, and BP62 is being cut with a two-file grant on
`BRCore.h`/`.cpp`. The correction matters because the false premise forces two-deep source paths,
which `architect.py:139` cannot parse at all (BP60).

**Ordering law:** none. Independent of every other ticket, including the ones it corrects.

## Kickoff (machine-checkable — the tickets skill verifies these BEFORE a claim)

- requires: files-only
- `.claude/hooks/guard_laws.py:73` still contains `or rel == o` (if it does not, this ticket is
  moot — verify before writing a word).
- `docs/ui/ue-frontend/TICKETS.md:63` still contains the phrase *"at *folder* granularity"*.
- owner_path: `docs/ui/ue-frontend/TICKETS.md`

## Steps (in order)

1. Correct §2(a) to state what the hook does: prefix matching is **recursive** (granting `UI/`
   already grants everything under it, so a sub-folder *narrows*, it never *enables*), **and**
   exact-file entries are accepted. Quote `guard_laws.py:73`. Owner: **builder**.
2. State the consequence in the same paragraph, because the paragraph exists to justify a
   structure: **file-level collisions are prevented by exact-file `owner_path` grants, not by
   deeper folders.** Cross-reference R23, R25 and R31 (`:476-482`, which concedes a multi-ticket
   union weakens confinement to zero anyway). Owner: **builder**.
3. Mark, do not silently rewrite, every downstream block whose owner_path shape depends on the
   corrected premise — the 18-folder tree at `:67-72` above all. **This ticket does not re-cut the
   board**; it makes the board's premise true so someone can. Owner: **builder**.

## Done when

- [ ] `TICKETS.md` §2(a) describes `guard_laws.py`'s actual behaviour, with the line reference, and
      no longer claims folder-only granularity
- [ ] The 18-folder derivation is flagged as resting on the corrected premise — not deleted, not
      quietly kept as if nothing changed
- [ ] **Zero changes under `.claude/`, `Tools/` or `Source/`** — this is a doc fix, and the hook is
      correct as written
- [ ] Findings + decisions written to this ticket's Log

## Notes

- Crew: builder. No verifier rung applies — nothing executable changes.
- Binary files this ticket OWNS (lock before editing): none
- Out of scope: changing `guard_laws.py` (it is right) · re-cutting BP27–BP59's owner paths · the
  two-deep-path question, which is BP60 step 2 · editing `DESIGN-RULINGS.md`
- Verified 2 Aug 2026: `guard_laws.py:73` matches the quote above character for character, and
  `TICKETS.md:63-65` makes the claim as described. Note this ticket **is** the one edit to
  `TICKETS.md` anyone is authorised to make — BP26 explicitly does not touch it.

## Log

(append findings here, dated, newest last — this is what the next session reads)
