# Phase B8 frozen search contracts

Baseline: 9ea170656fbe58dc0b5ae99b5565c8198ecde0b0; tree ca94eea1002543223c4df7faf3d13c17192e6db9; parent 4506e9e4f4a4cadc5ccf0bf605abc90b0ca31f48. R5.1 4ede1c3456d4edfbb054c06d2d1664be7ff30c63 is an ancestor. Fingerprint ce032f3811f4de017d5817b85dcca55f021b740582e7d1407dcdbf280700a8af. B7 independently passed R6; B8 awaits R7.

## Baseline call chain and events

LyricsLookup construction loads local rows/cache and never dispatches searches. start freezes selected visible rows, or asks before freezing all currently visible rows. pumpTracks admits at most 10 songs, lookup checks sibling LRC/TXT, same-song reuse and bounded ffprobe (5 seconds), then beginOnline creates attempts. manualSearch bypasses local resolution and never automatically accepts a candidate.

beginOnline starts with normalized title/artist search, exact get, alias/lead artist, title-only, base title, keyword, album and compact CJK variants; adds original metadata, manual fragment, original-credit queries and existing domestic sources. Independent sources interleave by round, backup Lyrics.ovh follows. No new provider or shorter query depth is permitted. Original and match contexts are distinct.

Per-song slots start at 3 and rise to 5 after eight fast responses; song slots rise to 10. Errors lower both to 3. Host cap is 6. Total online budget is 30000 ms, per-request timeout at most 8000 ms, body at most 4 MiB. One 5xx retry waits 300 ms; transient failures may trigger one serial recovery pass, normally after 500 ms. 429 cooldown lasts 30–600 seconds. A cooling/saturated host must not block independent sources. Domestic details are inserted only into the unsent suffix, preserving dispatched attempt indexes.

Each response first releases its slot, verifies job/generation/hash, parses provider-specific records and deduplicates candidates. Collection/cache epochs invalidate rank snapshots. One rank task per song captures original/match fingerprints, generation, epoch, full payload and timed requirement. B3 policy computes scores once, stable sort compares cached scores. Ranking from stale snapshots must be rejected and recomputed.

Early completion requires confident original synchronized credible lyrics, no conflicting vocals and no instrumental ambiguity, after all currently active replies drain. Auto acceptance additionally protects existing lyrics, checks original/cover contexts and ambiguity, validates raw HTML/parsed lyric length/timeline, and writes via Library::saveLyricsChecked with the frozen SHA snapshot. Network success, match confidence, automatic acceptance and successful save remain distinct. Manual selection retains its confirmation, bilingual replacement preservation and save-failure retry behavior.

Cancellation invalidates identity before aborting replies/probes. Closing disconnects late callbacks. Explicit search/navigation retains B7 preview-disposal semantics; candidate refresh/ranking/filter/cancellation must never dispatch AI or implicitly discard paid preview. B7 TranslationCoordinator and AiRequestCoordinator remain the only translation and paid HTTP authorities. R5.1 audit revision and SHA cache remain independent from search/rank revisions.

## Baseline evidence and reusable oracles

Independent Windows Release build: 226/226. Complete baseline CTest: 47/47, 168.50 seconds; raw output retained under D:/Git/Relaxer/reviews/phase-b8-lyrics-search-coordinator-20261017/logs/baseline-*.log and build-b8-baseline/Testing/QtResults. Existing Phase03/031 suites freeze comparator counts, response-order-independent query parity, bounded recovery, independent sources, cancellation, cover handling and explicit manual reselect. B3 fixed corpus retains 104 matching examples and six rankings. These oracles must all remain unchanged.

Before extraction, the UI owns job/query/network/candidate/rank/recovery state. B8 must replace that authority with a standalone QObject service and explicit value inputs, with no row indices or window controls in the service. Frontend owns only rendering, visible selection, confirmations and manual checked saves.

## Upstream disposition

R6 PASS: 47/47 CTest, 803/0/4 QtTest, translation-only 17/17, import-only 11/11, paid-safety 35/35. D-R6-01 (translation duplicate prepare defense), D-R6-02 (browser line id 0), D-R6-03 (wrong-thread shutdown) are inherited P3 and remain outside B8. Eleven historic P3 and five disclosed limitations persist. L-1 window-scoped unsaved preview/close behavior is a product decision; it remains unchanged without separate authorization. R6 source/evidence is read-only.
