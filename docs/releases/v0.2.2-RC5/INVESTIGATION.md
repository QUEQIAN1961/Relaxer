# RC5 test investigation snapshot

Direct baseline: ce32ba1308a41c7297927d28384958d45e1caa2a (RC4).
Only tests, test helpers and this note change. Product source, QML, storage,
playback, existing RC3 stop fixes, RC4 animation test and version resources remain
unchanged.

Foreground observation records the raw method-entry state before any fixture,
then brackets each stage with a separate read-only witness process. The witness
creates no QApplication/window, installs no hooks and sends no input. Every
snapshot includes HWND, owner PID/thread, process name, class, observer identity,
phase and time. Live injection boundaries are distinct from the original state.
Input requires contemporaneous agreement and an immediate owned-HWND recheck.
An attempted injection or missing product response is still a hard failure.

The foreground case uses Qt's non-activating initial window-show policy instead
of vetoing HCBT_ACTIVATE. That scoped policy is removed before the input window
is created; the other existing visual/tray fixtures retain their previous guard.
No test requests or forces foreground ownership. A legitimate external change
or observer disagreement is reported with current evidence, never substituted
with the original snapshot. Unexecuted OS input remains NOT VERIFIED.

Artwork diagnostics preserve the existing import/cache/insert/readback sequence
and t.id assertion. They buffer SQLite primary/extended codes, errors, connection
and transaction state, SQL/profile events, paths, process/thread IDs, timings and
temporary-directory lifecycle. Failed setup retains its synthetic directory.
Deliberate BUSY and missing-source controls prove these failure stages can be
distinguished; neither establishes the cause of the historical RC4 failure.

RC4-ART-01 is not yet diagnosed. Repeated passes must not be used to classify it
as harmless, prove it is a fixture defect, or rule out a product fault. No busy
timeout, retry, arbitrary delay, database logic or import logic was changed.
This snapshot deliberately retains RC4 version metadata. Promotion to RC5 and
creation of a private RC5 runtime ZIP require an evidenced fix and verification.
The seven GA gates remain open; there is no release-ready claim.
