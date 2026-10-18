# C1 Fix1 — optional QML runtime and clean delivery

Version: V0.2.1-dev-c1-fix1. Status: **DEVELOPER-VERIFIED — AWAITING HARNESS R13**.
Baseline: `57f95d71503594f973f0e4180d62a09ccc1d21a6`. No C2 work.

## Boundaries

Previously `Relaxer -> relaxer_qml (STATIC) -> Qt6::Qml/Quick/QuickControls2`
propagated import libraries into the main EXE. Windows loaded them before main,
even when the selected UI was Widgets.

The host now links only the Qt Core optional loader and existing Widgets/core
targets. `RelaxerQmlUi.dll` links QmlShell, AppBridge, PlaybackBridge and the same
existing static core targets. It loads only after CLI validation selects QML.
An absolute sibling module filename prevents current-directory module selection.
Loading failure returns 11 before mkdir, QLockFile, Library or Playback creation.

The host keeps QApplication, CLI validation, the original dataRoot algorithm,
QLockFile and diagnostics. Widgets constructs its original Library/Window path.
The mutually exclusive QML module call constructs one Library and one QmlShell
with its existing PlaybackFacade/Queue. No business QObject/vtable ownership
crosses the DLL ABI; the private versioned interface borrows only Qt Core options
and the host diagnostic value. Both sides require this package's matching Qt,
compiler and build. This is not a third-party plugin interface.

QmlShell's established destruction order is engine/root, bridges, facade, then
Library. Callbacks have shell contexts and are canceled during teardown. The
module has `PreventUnloadHint`: its code remains mapped until process exit to
protect Qt's process-global type/metaobject caches. Hard unload is unsupported;
tests verify repeated sessions release all database connections and keep code
mapped, rather than falsely claiming an unload test.

`RelaxerStart.exe` still launches Widgets. `RelaxerQml.cmd` still calls
`runtime/Relaxer.exe --ui=qml`, forwards all arguments, and returns its exit code.
No permanent parent process, new dataRoot or SQLite schema is introduced.

## CTest

`scripts/qml-test-environment.cmake` finds real import roots from the selected
Declarative configs and imported DLLs. Both QtQuick and Controls qmldir files
must exist. CTest receives per-test QML_IMPORT_PATH/PATH modifications, with no
global environment changes or developer path literals. Report filenames are
relative to their report directory to support Unicode report paths. UTF-8
compiled executable paths use QString::fromUtf8.

Full compilation succeeds in the tested build directory containing spaces.
Separate fixtures exercise matching single-prefix Qt, split Qt with a Chinese
overlay, Chinese CTest/report paths, and arbitrary current directories. The
installed MSYS2 Qt moc cannot produce files under a Chinese build directory;
that full-compilation experiment is recorded as a tool limitation, not a pass.

## Delivery

1. Deploy to a new candidate from build inputs.
2. Run native QA with system-only PATH, isolated APPDATA/TEMP/data and no QML
   import/plugin environment. Preserve the executed candidate and evidence.
3. Use its binary-hash-bound proof to deploy to a different, new shipping stage.
4. Package only this unexecuted stage with `package-phase-c.py`. It rejects
   logs, databases, media, data/cache folders and private inputs, validates the
   three binary hashes and required runtime modules, and emits a deterministic
   ZIP with a member hash manifest. It never cleans a user's runtime.
5. Validate extraction and first-launch logging using separate extracted copies.

Service-only builds retain their early return before UI/Quick dependencies.
Protected business sources and all 68 pre-C1 test files remain byte-identical.
The original 20 C1 assertions retain their cases and strength; only version
identity and executable path decoding change. C1-P301 remains unchanged: the
first C2 task is efficient count plus a read-only typed model at 0/1000/10000
tracks. All three R12 fixes remain registered pending independent Harness R13.
