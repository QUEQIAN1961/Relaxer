# Qt FFmpeg PCM equalizer hook

The Windows build requires the recorded MSYS2 UCRT64 Qt 6.11.2 private ABI. `resources/peq-runtime.json` records the installed static implementation archive and renderer header identities plus upstream reference hashes. The installed Qt libraries are never edited. The application builds its own `multimedia/ffmpegmediaplugin.dll`, linking the installed implementation archive with the replacement AudioRenderer object. The replacement has the same class layout and includes the matching generated moc; other renderer/time-controller/decoder code stays in the recorded archive.

Upstream: https://github.com/qt/qtmultimedia/tree/v6.11.2/src/plugins/multimedia/ffmpeg

The retained Qt source files keep their original copyright and SPDX notices, including GPL-3.0-only as an available licence. Relaxer remains GPL-3.0-only. Exact corresponding Qt/toolchain source materials and patches must be retained/reviewed when distributing; this development build does not publish a release.

The hook processes each converted PCM buffer exactly once before the original QAudioSink write. Partial writes retain the processed buffer/offset. Seeking resets filter history; output clocks, position accounting, media sources and Qt audio/video scheduling stay with Qt. QAudioBufferOutput remains the original pre-EQ decoding tap, so it must not be used as proof of audible post-EQ output.

The common RelaxerAudioDsp.dll holds the live configuration and creates a separate filter state for every Qt renderer and FluidSynth stream. Disabled settled processing is byte-transparent. Controls publish bounded atomic snapshots; audio processing has no settings/file I/O, waits, heap allocations or writer-mutex acquisition. Changes crossfade filter chains for 20 ms. Automatic headroom uses a sampled combined frequency response plus band-center checks; it reduces overload risk but is not a true-peak limiter.

Only this build's plugin is placed in the application's multimedia directory and preferred before installed plugins. `runtime-peq.json` hashes both processing binaries and records the source fingerprint. The packaging recipe carries these files after collecting the existing runtime baseline, but no package is produced by development.
