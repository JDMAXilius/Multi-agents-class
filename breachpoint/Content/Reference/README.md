# Content/Reference — internal development and testing assets

**Use whatever you need in here, with no restrictions on how you use it.** Original Halo
Infinite assets, extracted game content, competitor art, packs you have not cleared for ship.

**Nothing gates development.** PIE, editor work, automation, and **packaged internal playtest
builds** may all use these assets. There is no `DirectoriesToNeverCook` entry and
`Tools/verify_notices.py` ignores this directory entirely on a normal run — its reference checks
only execute under `--ship`.

## The one protection, and why it is this one

Everything here is **gitignored** (except this README).

This costs development nothing — assets live on the machine that runs the editor, and git was
never how you move tens of gigabytes of extracted content. What it prevents is the failure mode
that actually bites: a `git add -A` sweeping copyrighted assets into a commit and pushing them to
GitHub. Local use is private; a push is **distribution**, and git history makes it close to
permanent.

This project has already had assets swept in by an over-broad `add` — commit `bc5cf8f` pulled in
two character assets under a message about grenade tuning. The same reflex with reference content
is the scenario worth a mechanical guard.

If you ever genuinely need one committed, `git add -f` still works. That is a deliberate act
rather than an accident, which is the whole distinction.

## At ship time

`python3 Tools/verify_notices.py --ship` reports anything under `Content/` outside this directory
that **hard-references** `/Game/Reference/` — a hard reference pulls the asset into a cook. Soft
paths are fine; they simply fail to resolve in a packaged build.

Replacement art is tracked in `docs/ui/ART-PASS-STAGE-3.md`. That work gates **shipping**, not
development.
