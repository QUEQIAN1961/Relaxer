# Relaxer V0.2.5-GA

Stable release: [V0.2.5-GA](https://github.com/QUEQIAN1961/Relaxer/releases/tag/v0.2.5-GA).

A local music player for Windows, built with C++17, Qt 6 and QML. Includes a music library, playlists, queue, synced lyrics and custom themes, plus Widgets compatibility mode.

V0.2.5-GA adds NetEase Cloud Music and QQ Music discovery, sign-in, search, recommendations and downloads, cached sidebar playlists, per-playlist sorting and 48 icons. It improves AI configuration recovery, lyrics and interactions, and uses open-source FluidSynth for MIDI.

Extract the complete Windows x64 portable ZIP to a new writable folder and launch the root `Relaxer.exe`. Use `Relaxer.exe --widgets` for compatibility mode. Back up the entire `data/` folder before upgrading, test with a copy, and never run two versions against the same data. An explicit `--data` path is supported. Do not mix runtime files from different packages.

Local playback works offline. Online lyrics and optional translation use user-selected services; credentials and paid requests require the user's decision.

See the [Chinese guide](README.md), [release notes](docs/releases/v0.2.5-GA/RELEASE-NOTES.md), and [build/source instructions](docs/releases/v0.2.5-GA/BUILD-AND-SOURCE.md). Historical releases and changelogs are preserved.

Relaxer-authored code is GPL-3.0-only. Third-party code retains its own licenses, copyrights and applicable exceptions; retain all notices and accompanying corresponding source. This grant does not cover external music, artwork, lyrics or service content.

V0.2.5-GA uses open-source FluidSynth with GeneralUser GS. Run `python scripts/prepare-midi-runtime.py` to build the pinned minimal engine; see [MIDI notes](docs/MIDI-BASS.md) for licensing and corresponding source.
