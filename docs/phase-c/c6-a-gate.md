# C6-A developer checkpoint — 2026-10-21

C6-A developer gate PASS. This is an internal stage checkpoint, not an RC1
freeze or independent Harness closure. C6-B may now start.

Tested commit: `b4dd4b6be4bc98cb03afe92deef887e34688445d`  
Tested tree: `e39245b918c33749478c394df5043aba2d4312b2`  
Source fingerprint: `20ec7472869ffd0d601b1638a6ecb9f9382a63e9765a40748738d195165e80cd`

This checkpoint adds only this record and the executed evidence summarizer;
product and test sources are unchanged from the tested commit.

| Gate | Actual evidence |
|---|---|
| Fresh Release | New `build-c6a-final`, configure/build exit 0; actual selected Base/Declarative prefixes in command JSON |
| Clean external CTest | 74/74 passed; original 74 registered test names retained; no external QML import/plugin variables or Declarative PATH injection into CTest |
| Full QtTest | 1178 passed, 0 failed, 4 skipped, 0 blacklisted; 60 logs |
| PlaybackFacade | 31 passed, 0 failed/skipped |
| LyricsSearchCoordinator | 69 passed, 0 failed/skipped |
| TranslationCoordinator | 47 passed, 0 failed/skipped |
| ImportCoordinator | 22 passed, 0 failed/skipped |
| Native C6 runtime suite | 16 passed, 0 failed/skipped; actual Windows QPA and software renderer |
| Runtime scope | Lyric sync/1.2 scale/bilingual/±5000ms offset/manual return/fixed timestamp, fullscreen Esc and button restore, next-panel/backdrop frames, local AI consent/preview/discard/save/reopen, single core and connection, folder import |
| C6-D05 | Light/dark × normal/reduced motion; page interior sampling, minimize/restore; original fade/movement assertions retained; earlier user black-flash retest PASS retained without enlarging its scope |
| R15-01 | Existing declared runtime environment applied to four targets; all pass |
| R15-02 | Actual mock request identity replaces assumed HTTP arrival order; original checks retained; previous 10×47 and 10×4 stability evidence retained |
| R15-03 | Harness literal backslash-u path creation failure reproduced; valid Unicode/space paths and user folder import PASS; developer diagnosis is HARNESS INVOCATION ERROR CONFIRMED, not a product import fix |

The unchanged skips are `Tests::realPlaybackAndSeek`,
`Tests::realResumeAndLoop`, `GuiTests::playerBarStableAndRealSeek` (audio device
unavailable) and `V16Tests::liveSongComparison` (opt-in live network). No paid
AI or user credentials are used. Native Qt event tests are not represented as
physical user operations; only folder import and the Review2 black flash have
explicit individual user acceptance messages.

The original `4ac6ffe` clean full run was 73/74, with one test-tool failure:
`actualAnimationFrames` reversed after a wall-clock 100ms before observing a
single animation update. Its first screenshot took 109ms, and the recorded
progress/backdrop remained zero. The test now reverses an observed intermediate
frame; all displacement, continuity, frame-count and final-state assertions
remain. Original failure logs and measurements are preserved, not retried away.

Local evidence root:
`D:/Git/Relaxer/phase-c-c6-work-20261021/Evidence/`.

- Successful gate: `C6-A-final/Run-20261021T063829Z-a7c0273f/GATE.json`, with exact commands, source identity, native measurements and copied Qt logs.
- Original failed head regression: `C6-A-head-ctest/Run-20261021-142655-53b4c5/AUDIT.json` and `RuntimeMeasurements`.
- R15 request-order stability: `C6-A2-repeat-suite` and `C6-A2-repeat-case`.
- Prior Review1/Review2 build identities and user confirmation remain in the work directory.

Reproduce using `scripts/verify-phase-c6.py` with actual `--qt-prefix`,
`--quick-prefix`, a new `--build`, `--fresh-build --require-clean`, an append-only
`--evidence`, and a short `--temporary-root`; then run
`scripts/summarize-phase-c6.py`. No personal overlay path is embedded in either
script. Do not modify source while a verification pipeline is running.

Outstanding: C6-B full parity/native integration/performance audit; C6-C final
clean integration/deployment/release evidence and packaging. C6-D01 remains
record-only. Historical P3 and R15 independent closures remain for Harness R16.
