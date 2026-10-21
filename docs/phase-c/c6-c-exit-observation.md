# C6-C native exit investigation — 2026-10-21

At source `1ff7a7649dc867c53a80289bdbf3f62bf380556f`, all 33 deployed
QtTest suites passed, but the final launcher check failed. Earlier portable
foundation child checks also timed out. These failures remain in the evidence.

An eight-launch ASCII-path forensic run reproduced four slow exits (18.8 s and
about 60 s) and four ordinary exits (1.8–2.0 s). Native debugger stacks show
the main thread waiting inside Qt Multimedia application-static cleanup, with
`WasapiProbeThreadpool` blocked in AudioSes/RPC. Diagnostics recorded successful
save and event-loop return before the process finished. This is evidence of
an audio-device probe stall on this execution desktop, not a database-save
failure. Debugger attachment affects timings; these are forensic observations,
not performance benchmarks.

The existing `RELAXER_SKIP_AUDIO_DEVICE_TESTS` contract was incomplete:
`QAudioOutput` was a value member, whose constructor requests the default
physical device even though playback attachment was suppressed. It is now
created only for physical-output paths; headless and decode-only paths keep
their volume/settings without constructing it. Ordinary application runs still
use the normal Qt audio device. A regression checks both isolation modes and
volume persistence. This does not claim to repair Windows audio-service RPC
or prove physical audio output on this desktop.

The launcher separately detaches after its documented 2.5 s loader check.
Native QA now holds and waits for the actual GUI child process handle, checks
its exit code, verifies the same 20 s close deadline and checks data-lock release.
Waiting for the launcher alone was not a valid child-exit check. Per-program
results and launcher timing are written before assertions so failed runs remain
reviewable.

Evidence roots under `phase-c-c6-work-20261021/Evidence`:

- `C6-C-preflight-native/Run-20261021T080309Z-5efe654d`: original foundation failures.
- `C6-C-preflight-native/Run-20261021T083707Z-af787db2`: all deployed suites passed;
  original launcher assertion failed.
- `C6-C-launcher-close/Run-20261021T084627Z-b305f270`: child survived a real 20 s wait;
  this proves the observation was not merely an immediate snapshot race.
- `C6-C-exit-stacks/Run-20261021T084851Z-2ed7f6fe`: all eight rows and native stacks.

R16 must independently test physical playback and normal shutdown in an
interactive Windows session. Neither the host-wait test change nor headless
isolation may be described as a permanent fix for the underlying device stall.
