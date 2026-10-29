# V0.2.2-RC3 private test candidate

The direct baseline is RC2 `99eb65067dfc2c105a8cd1d7641df70e9b3dd0aa`.
This change does not authorize public distribution or close the remaining GA gates.

The ownership integration test could stop the coordinator after snapshots were
stored but before their queued candidate notifications reached the dialog. Stop
correctly changes the generation, so the old notifications must remain rejected.
The test now waits for the observable view publication before separately checking
coordinator cancellation. A second test holds notifications at that boundary and
uses the actual Stop button. The dialog reconciles completed candidates from the
current snapshot after stopping, without accepting stale callbacks or changing
candidate revisions. Playback, translation ownership and checked saves retain
their existing assertions.

`realPlaybackAndSeek` and `realResumeAndLoop` now attach a real audio output;
their previous `decodeOnly=true` constructors could not test physical playback.
Their listening/session assertions are retained. The player bar contract uses
`bottomSongTitle`, and layout/volume coverage runs separately from hardware seek.

The normal 91-test CTest suite is explicitly deterministic/headless. Its three
hardware cases remain recorded as HARDWARE NOT RUN, rather than claiming that
the device was probed or passed. To run the physical cases from a configured
desktop audio environment with the build's Qt/FFmpeg dependencies on PATH:

```powershell
python scripts/run-audio-device-tests.py --build <Release-build> --output <fresh-audio-evidence-directory>
```

This runner removes `RELAXER_SKIP_AUDIO_DEVICE_TESTS` in its child environment.
Each case first uses a separate Qt-only process to verify output attachment,
play/pause/seek/resume/stop and bounded teardown. Missing or unusable output is
ENVIRONMENT/Qt backend BLOCKED, never a real-audio PASS. A missing probe is a
fixture failure. If the independent control passes, Relaxer assertions run and
their failures remain failures. No PCM-only result substitutes for this gate.
Programmatic output checks also do not prove that a human heard the speakers.

Optional CTest registration is available with
`-DRELAXER_ENABLE_AUDIO_DEVICE_TESTS=ON`, followed by
`ctest --test-dir <build> -L audio-hardware --output-on-failure`.
This adds a separate hardware gate to the normal suite. Exit 77 means blocked;
use a fresh evidence directory when repeating the Python runner.

The private ZIP deliberately has no deployment ready marker. CMD and direct
application startup do not prove first-run launcher acceptance. Never fabricate
`relaxer-ready.json` or change verified state. 100% DPI, native human input,
physical audio, live translation/network, exact Qt/FFmpeg corresponding source,
launcher positive deployment and final distribution acceptance remain separate.
