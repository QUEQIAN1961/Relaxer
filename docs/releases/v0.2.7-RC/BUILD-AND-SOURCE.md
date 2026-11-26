# Build and source

The RC release tag includes the Round39 sources, including files that were untracked when the independent regression was run. The source content fingerprint `fbdf77c04234369b5e1dc355495189b0f2c12f65f2add961789c78c67a528fd7` covers 487 files under the policy in `scripts/source-fingerprint.cmake`. Core binaries embed that fingerprint. The distribution records the source commit and binary hashes in `BUILD-IDENTITY.json`.

Windows x64, Ninja Release, GCC 16.2.0 UCRT64, Qt 6.11.2 and FluidSynth 2.6.2. QML and BUILD_TESTING are enabled. Harness independently built and ran the Round39 regression. The release uses those application binaries. The portable runtime uses the MSYS2 libsndfile 1.2.2 build, with dynamically linked codecs and corresponding source packages, in place of the vendor SDK binary used in the regression build. The packaged MIDI runtime is checked separately.

The runtime uses FluidSynth with SF3 support, GeneralUser GS, patched FFmpeg, RelaxerAudioDsp.dll and the matching Qt FFmpeg multimedia plugin. Runtime manifests record their identities. Base dependency source materials remain available with V0.2.2-GA. The Qt multimedia source attachment includes the matching MSYS2 source package, recipes and Relaxer patches; the replacement plugin's source and build recipe are also in the application archive.

`scripts/stage-release-midi.py` verifies the MSYS2 packages and stages the distribution MIDI runtime. Use the package versions and SHA-256 values in `runtime/runtime-midi.json`. Then run `scripts/package-rc.py` with `--midi-runtime` pointing to that staged directory and `scripts/package-additional-sources.py` with `--midi-runtime` and `--midi-package-dir`.
