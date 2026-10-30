# V0.2.2-RC4 private regression stabilization

Direct baseline: d4062b5d5c97c74863b1d45d6802e99b822f9f75 (RC3).
Only two test implementations, a necessary thread-local background-window test helper, version metadata/resources, and these notes change.
Product C++ (except version headers), QML, playback, storage, and the RC3 stop fix remain unchanged.

The shortcut test classifies the actual Win32 foreground as owned, absent, or unowned.
It never requests/raises/activates its input window. SendInput rechecks ownership at
its call boundary; absent/unowned state executes the controlled native handler and
thread-message checks instead. Registration, rollback, recording lifecycle, final
volume and zero-network assertions remain. Unexecuted/partial OS injection is a
reasoned QtTest environment SKIP after those assertions, with separate JSON coverage.
Three classifier cases are unit checks only, never evidence of actual OS foreground
coverage. Real states and actual event counts must be read from the receipts.

Animation sampling buffers property notifications and geometry in memory. Five
scenarios retain displacement/height/reversal/terminal checks and also require
real intermediate progress and a stable requested final state. Files and terminal
screenshots are written afterwards. Loop counts are diagnostic, not a pass threshold.

The default full suite still registers 91 CTests. Preserve every full run and all
internal skips. The physical audio runner from RC3 is unchanged; run it separately
with skip variables cleared, and never infer actual sound from PCM decode.
The seven GA gates, public distribution and launcher positive deployment remain
independent. No ready marker or verified state may be fabricated.

A scoped CBT hook applies only to the current test GUI thread in the two target
methods and the tray restore fixture. It rejects programmatic activation of that thread's own windows and is
removed by RAII; it does not hook other processes, change desktop settings, or
activate a window. User mouse activation is not blocked. If Qt local-key active
state is unavailable, that coverage is explicitly unexecuted; setup volume 45
initializes the separate global-dispatch contract and is not a local-key PASS.

Full-suite verification exposed a shared prepare() assumption: pause() can be a
no-op before asynchronous media autoplay is ready, so waiting 100ms afterwards
can leave the tray fixture playing. Both existing prepare helpers now wait for
a valid loaded/buffering/buffered status, seekable and playing before pausing, then verify paused state.
The tray case explicitly asserts that starting precondition; its original playing
and pause assertions remain. No playback/bridge/product implementation changes.
