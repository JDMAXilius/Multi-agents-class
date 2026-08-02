# Content/Reference — internal development and testing assets

**Use whatever you need in here.** Original Halo Infinite assets, extracted game content,
competitor art, licensed packs you have not cleared for ship — this directory exists so
development and testing are never slowed down by ship-time questions.

Three mechanical protections make that safe, and none of them restricts what you put here:

1. **Never committed.** `.gitignore` excludes everything in this directory except this README.
   Local disk is private use; pushing to GitHub is *distribution*, which is a different and much
   larger exposure. This is the protection that matters most.
2. **Never cooked.** `Config/DefaultGame.ini` lists this path in `DirectoriesToNeverCook`, so
   nothing here enters a packaged build even if the directory is populated.
3. **Verified, not assumed.** `Tools/verify_notices.py` fails if the never-cook entry is missing
   or if a shipping asset hard-references this directory — the one route by which a reference
   asset can still be dragged into a cook.

## The single rule

**Nothing under `Content/` outside this directory may hard-reference anything inside it.**

A soft path (`TSoftObjectPtr`, a CSV row) that resolves here at runtime in a dev build is fine —
it simply fails to resolve in a packaged build. A **hard** reference from cooked content pulls
the asset into the package regardless of `DirectoriesToNeverCook`, which is exactly how this
kind of thing ships by accident.

Study it, measure it, prototype against it, use it as a placeholder while the real art is made.
Replace it before ship — `docs/ui/ART-PASS-STAGE-3.md` tracks that work.
