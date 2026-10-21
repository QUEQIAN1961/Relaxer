# C6-C RC1 source preparation — 2026-10-21

Feature scope is frozen after the C6-A and C6-B developer gates. Product version
metadata is prepared as `0.2.1-rc1` / `V0.2.1-RC1`. This source-preparation commit
alone does not certify an RC1 delivery: the final clean build, full test suite,
portable runtime, isolation matrix and archive checks must all pass on it.
No formal tag or remote push is part of this work.

Latest dev preflight source `b2f0620f131f86652634780bd93eb4d8bd6d2fb5`:

- Incremental all-target build plus navigation/performance probes passed.
- PlaybackFacade 32/32 and foundation 20/20 passed.
- Separate freshly deployed Chinese/spaced portable runtime: 33 QtTest suites,
  587 passed / 0 failed / 1 skip; PCM, both UIs at 1x/2x, navigation passed.
- Actual launcher child close: exit 0, about 70 ms, no remaining process or lock.
- Eight optional-QML/lock/path cases passed, including same-copy DLL restoration
  and successful Widgets/QML after restoration.

Evidence root: `C6-C-preflight-native/Run-20261021T085938Z-163fd77b`.
Its original driver SUMMARY contains an obsolete `compiledSourcesTestedAt`
annotation. The actual source identity is `b2f0620`, as recorded by deployment,
command results and the additive `SOURCE-IDENTITY-CORRECTION.json`; do not reuse
the obsolete annotation as build evidence. Earlier full 75/75 run was at
`d724017`, not the later preflight and not the final RC1 source.

Reproduction uses explicit selected Qt Base/Declarative prefixes:

1. `scripts/verify-phase-c6.py --source SOURCE --build NEW_BUILD --qt-prefix BASE
   --quick-prefix QUICK --evidence EVIDENCE --temporary-root SHORT_TEMP
   --fresh-build --require-clean --target all relaxer_ui_navigation_probe
   relaxer_c4c5_performance_probe`.
2. `scripts/summarize-phase-c6.py` with the immutable run, build, new output,
   `--expect-tests 75` and original 74-test `--baseline-tests-file`.
3. `scripts/deploy-phase-c.py` into a new candidate, then
   `validate-phase-c-native.py` and `validate-c1-fix1-isolation.py` with new outputs.
   The child must exit inside its unchanged close deadline; never discard an
   earlier timeout or package the executed QA runtime.
4. Run `relaxer_c4c5_performance_probe --data NEW_DATA --count N --prepare`,
   then a separate process with the same data/count plus `--output JSON
   --screenshots NEW_DIRECTORY`, for N=0,1000,10000. Use native Windows QPA,
   software Quick rendering and the documented isolated physical-audio skip.
5. Re-deploy a fresh shipping stage with `--verification-proof` from the separate
   matching native candidate. Write the six evidence-driven reports with
   `scripts/write-phase-c6-reports.py`. Assemble with
   `scripts/assemble-phase-c6-release.py`, passing explicit source/build/staging,
   reports, evidence-list/root, full-summary, native-proof, isolation and new
   publish/verification paths. Both scripts describe all required CLI inputs.

Assembly verifies the final extracted ZIP copy, the entire archived Git Tree
and recomputed source fingerprint, and a verified history bundle through an
independent clone/fsck. DELIVERY and component/full SHA256SUMS are written only
after these checks. Historical startup ready-marker protocol remains `1.5.0`;
it is independent of the display/product version.

Physical audio output, physical global-key input, real browser login/CAPTCHA,
long-duration stability and independent R16 remain explicitly separate from the
developer gate. The observed Windows audio-service probe stall remains disclosed
in `c6-c-exit-observation.md`; headless isolation is not a Windows RPC repair.
C6-D01 remains record-only. Schema, storage, queue and player authority remain
unchanged.
