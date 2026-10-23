# P3 infrastructure and release follow-up

Baseline: `9180d872fca6516c595144466b76ca27e4146b4b`, product version 0.2.1.
The preceding twelve product fixes have independent Harness acceptance. This
round changes verification/deployment and tests; it does not re-close those IDs.
No tag, push, or release is performed by any gate here.

## Process ownership and launcher evidence

`verify-runtime.ps1` uses `owned-process.ps1`, Windows PowerShell 5.1 and its
built-in C# compiler; Python is not a new first-start requirement. CreateProcessW
returns the process handle at creation; it is retained until cleanup completes.
Recorded PID/time/image/command/parent/task/root are audit metadata, never a way
to reopen a cleanup target. Cleanup first requests WM_CLOSE, then waits, then
terminates through that original handle with a bounded wait. No descendants are
admitted by this helper, including during finally. It is deliberately root-only.

The native launcher matrix separately retains original root handles and verified
child handles. Child admission requires birth order and a canonical path boundary
inside the fresh run. Cleanup never expands that set. Module assertions use the
retained handle, require two identical nonempty live snapshots, and retain every
API error/retry. EXITED/UNKNOWN/ERROR cannot pass required dependency isolation.
Observation is bounded to 20 samples and a 2-second wall-clock budget, with 50ms
between samples. A preserved first matrix failure showed four complete but
changing startup GPU/OS module sets over the original 75ms sampling window; this
justifies the longer observation budget without weakening isolation assertions.
The original GetModuleFileNameExW failure's exact cause remains unconfirmed.

CMD testing uses native backslash paths, explicit /d /v:off /s /c boundaries,
quoted literal tokens and child-local MSYS conversion controls. Native Windows
and MSYS2 UCRT Python are separate tested distributions. A POSIX MSYS Python is
not installed in this environment. Generic batch percent/quote/newline expansion
is rejected by the verification helper; product CMD entry semantics are retained.

## Test coverage and commands

Use a fresh directory for every output. All Python scripts use the standard
library. Supply a tool PATH for Qt, MinGW, CMake, Ninja and Git as in your normal
build; plugin fault fixtures never alter installed DLLs.

```powershell
python scripts/tests/test_launcher_observation.py
python scripts/tests/test_deployment_contracts.py
powershell.exe -NoProfile -File scripts/tests/test-owned-process.ps1 -Output C:\fresh\safety
python scripts/tests/test_cmd_argv.py --output C:\fresh\argv --echo-python C:\Python\python.exe
python scripts/test-infrastructure-deployment.py --source C:\source --build C:\build --qt C:\qt --quick C:\quick --output C:\fresh\deployment
python scripts/test-ranking-mutants.py --source C:\source --build C:\build --output C:\fresh\mutants --ninja C:\tools\ninja.exe --ar C:\tools\ar.exe
python scripts/test-sync-repetitions.py --build C:\build --output C:\fresh\sync --iterations 10
python scripts/test-sync-repetitions.py --build C:\build --output C:\fresh\activation --iterations 10 --interactive
python scripts/test-line-policy.py --source C:\source --output C:\fresh\lines --bash C:\msys\usr\bin\bash.exe
```

The GUI destruction test covers real Window lifetime and its latency bound.
`destructionCancelsWithoutBlockingAndReleasesWorkerAndThread` supplies a positive
SQLite trigger/control, an observer alive through controlled worker cancellation,
no late writes, worker/thread deletion and consumer lifetime. The removed spy on
an already destroyed sender supplied no independent evidence.

Calling-thread rejection and actual SQL ownership have distinct tests. The latter
observes a SQLite trigger in Library's legal thread while a real separate QThread
produces results. Targeted mutants never dispatch real SQLite work across threads.
Stale ranking tests hold workers at a barrier, deliver captured results in a
controlled queued order, and inspect publication/revision/ready/terminal/save
boundaries before a fresh result can mask them. The only production header change
is a friend diagnostic declaration, with no runtime storage or dispatch hook.

Audit waiting includes debounce, in-flight and pending work plus applied revision.
Existing row/SQL/read/digest thresholds remain. Controlled reentry and close are
covered separately. OS activation requires owned HWND foreground and 1->0->1;
synthetic activation remains unit coverage only. Foreground lock or user input
produces a failing BLOCKED record, never a synthetic pass or permanent QSKIP.
`RELAXER_INTERACTIVE_TESTS=ON` registers the separate interactive CTest label.
Default CTest coverage does not claim that optional desktop test.

GUI test main diagnoses platform/plugin paths before QApplication. The platform
plugin must preload; failures return 78 without a modal fatal dialog. Successful
preflight still requires actual QApplication integration initialization. CTest
offscreen settings and existing native c6 hover settings are preserved.

## Deployment and policy

BUILD_TESTING=OFF defines no test targets and requires neither Qt Test nor SQLite
C headers/library. Production SQLite remains the Qt Sql/plugin dependency. CMake
3.22 remains the minimum. CMake >=4.3 uses SQLite3::SQLite3; earlier FindSQLite3
modules get the documented alias from SQLite::SQLite3. Independent service-only
configurations retain their early returns and no-Widgets boundaries.

Ready marker `version=1.5.0` remains the legacy schema. New generators also emit
`readySchemaVersion=1.5.0` and `productVersion=0.2.1`. Native launchers retain their
existence-only handshake. Python metadata tooling prefers the explicit schema,
accepts old markers/unknown fields, and rejects conflicts, missing schema or bad
JSON. Product version is never substituted for schema version.

`online-msys2.ps1` retains its original LF bytes. Its scoped gitattributes rule
documents the accepted format; two clean checkouts/archive comparisons and local
reuse branches test native PowerShell and MSYS Bash invocation without HTTP.
Historical empty logs and historical early tags remain historical facts.
`evidence_run.py` records command, input identity, UTC start/end, exit, raw stream
paths/digests and verified outcome. Deployment also records each scanner/objdump
invocation, resolved inputs and actual dependency lists. No historical log is
backfilled. Controlled failures must retain nonzero/FAIL receipts.

## Read-only release gate

`python scripts/release_gate.py --source <clean clone> --delivery <delivery>
--attestation <Harness-attestation.json>` never mutates source or artifacts.
Absent acceptance returns PENDING/nonzero. Identity is recomputed from Git blobs
using the unchanged fingerprint algorithm; delivery hashes and Harness-bound
report hashes/identity must match. An attestation schema 1 has `issuer.kind` set
to `independent-harness`, `identity` (commit/tree/sourceFingerprint), an exact
`artifactHashes` map matching SHA256SUMS, `report` (relative path/sha256), `verdict`
(productRegression/artifactIntegrity/processSafety/freezeReadiness), and an explicit
`conditions` array of objects with status SATISFIED or an unresolved state.
Report text must contain all three identity values. Self-test issuers are rejected.
Synthetic fixtures are never release approval. Pending conditions remain PENDING.

The gate checks local bytes, identity and issuer claims; it cannot establish human
authorship or authentic independent provenance from editable local JSON. Review
the referenced Harness origin separately. A transcription of an older report is
not a new Harness verdict. This round stays PENDING until independently accepted.
R19 historical menu trigger remains NOT REPRODUCED / ROOT CAUSE UNCONFIRMED;
the existing c6/v020 synchronization preconditions and tolerances are unchanged.
