# C6-B checkpoint — 2026-10-21

Developer parity gate PASS, with the physical-input coverage limits explicitly
listed in `c6-parity.md`. Feature scope is frozen for C6-C. No new provider,
schema, store, player core or shell rewrite was introduced. C6-D01 stays deferred.

Tested code: `d3ae2f2` (full hash in runner command JSON). Source fingerprint:
`19fca0dc4591b632151464ff228b872f92cf4199d1ae44a340c18dcae8c0eee4`.

- Relevant CTest: 4/4 pass (settings, original UI animations, native C6 runtime,
  new native parity), including 6/6 new parity QtTest cases and 16/16 native C6.
- Native actual font properties: all five regions individually enabled and
  checked for independent application, including translated lyrics and song
  metadata. Existing managed Fonts files/settings schema retained.
- Native image pixels: managed shared left/right background remains visible
  through real page transitions. Screenshots inspected; synthetic red/blue
  checker fixture is a test asset, not a shipped appearance preset.
- Real system tray reported available; close-to-tray, restore, playback/previous/
  next and explicit quit actions tested on the same facade/connection.
- Real Windows hotkey conflict/rollback/recording release/destruction release
  and OS thread-message dispatch pass. SendInput/physical keyboard is NOT
  EXECUTED because the OS foreground window is unavailable on this test desktop.
- 0/1000/10000 performance runs all exit 0; actual full sorting/mapping/order,
  filter/frame, queue readiness, 80/5000 lyric rows, import and lifecycle checks.
  Times are response samples, not FPS. No progress-driven model resets.

C6-A's four service implementations are unchanged by B; their existing gate
evidence remains applicable. C6-C will rebuild and run the full suite, including
the new 75th target, on the final source rather than relabeling B's targeted run
as a full regression. This checkpoint adds documentation only after the tested
code; product/test bytes and their fingerprint remain unchanged.

Next: final reproducible deployment/version metadata, clean Release/full tests,
portable-native QA, optional-module/lock/shutdown/PCM, final performance, residue
policy, source/commit/binary binding, bundle/ZIP verification and six handoff
reports. No RC1 has been frozen or delivered by this checkpoint.
