#pragma once
// PHASE 2 — not yet implemented. Contract:
// One scoring input: a fact selector + a response curve + a weight. Worldless C++ struct;
// curves are hand-authorable assets mirrored from C++ defaults. Evaluate(Facts) -> 0..1.
// aib-critic attacks saturation here: a consideration pinned at 0 or 1 across the real
// fact range is a dead weight, and that is a finding.
