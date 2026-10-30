# RC5 historical diagnostic context

Original diagnostic snapshot direct baseline: ce32ba1308a41c7297927d28384958d45e1caa2a (RC4).
The original snapshot changed tests, test helpers and this note. Product business
source, QML, storage, playback, existing RC3 stop fixes and the RC4 animation test
remain unchanged. The current private candidate additionally updates version
metadata and test-runner initialization, as described in this release README.

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
The original diagnostic snapshot retained RC4 metadata. The user subsequently
authorized a private RC5 test candidate after fresh validation, with ART-01 kept
as UNRESOLVED — NOT REPRODUCED. Promotion does not establish its cause or a fix.
The seven GA gates remain open; there is no release-ready claim.
