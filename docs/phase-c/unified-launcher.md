# Unified Launcher / V0.2.1-Final-RC

See unified-launcher-audit.md for the pre-change architecture and frozen UI1 identity.

- Double-click RelaxerStart.exe: QML. --qml explicitly selects QML; --widgets selects the compatible Widgets UI. Runtime --ui=qml, --ui=widgets and --ui VALUE are also accepted. Repeating the same selector is allowed; conflicting/invalid selectors return 64.
- Other arguments are forwarded as Windows argv values. Relative paths are resolved against the caller's working directory. The launcher has no Qt dependency and creates the GUI child without a console.
- RelaxerQml.cmd remains byte-compatible and calls runtime/Relaxer.exe --ui=qml. Direct runtime retains its Widgets default for existing diagnostics. Use --widgets for audio/database diagnostics through the launcher.
- All entries use the existing package/data by default or explicit --data. The runtime owns the single dataRoot lock; code 3 is preserved. The launcher releases only its preparation guard before launching, waits for the runtime lifetime and forwards its exact status, including delayed lock dialogs (D-R16-02).
- Missing QML DLL/dependencies: runtime code 11 before dataRoot creation. Ordinary launcher users see a short recovery message and --widgets guidance; --diagnostics invocations remain noninteractive. No silent fallback. Detailed QLibrary errors remain in runtime/startup.log.
- One hidden native launcher remains while its runtime runs, with no extra business core; both leave on normal shutdown. External forced termination of the launcher is not a normal shutdown guarantee.
- Native tests: existing relaxer_launcher_env now covers real child 0/11/delayed 3 and Unicode argv. scripts/validate-unified-launcher.py adds native default/alias/CMD, shared-root, cross-UI lock, friendly dialog, missing dependency, PE and process checks on disposable copies. Existing isolation and full regressions remain required.
- Candidate identity is 0.2.1-final-rc / V0.2.1-Final-RC in phase-c-version.h and both phase-c .rc files. FILEVERSION stays 0,2,1,0 with prerelease flag. Ready protocol 1.5.0 is unchanged. Final 0.2.1 identity would require a separate rebuild of these three resources and renewed binary verification; no tag or release is made here.
- Historical C1 documentation describes earlier builds. The generated READ-ME-FIRST.md is the current portable entry guide. No in-repository .lnk or installer configuration depends on the compatibility CMD.
