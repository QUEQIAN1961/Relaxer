#!/usr/bin/env bash
set -euo pipefail
[[ "${MSYSTEM:-}" == UCRT64 ]] || exit 2
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.."
build="$(cygpath -u "${RELAXER_BUILD_DIR:?}")"
# The first launch uses a prebuilt bootstrap, so read the isolated-build marker
# here too: the already-running bootstrap cannot see newly compiled C++ changes.
if [[ -n "${RELAXER_BUILD_CACHE:-}" ]]; then
  cache_dir="$(cygpath -u "$RELAXER_BUILD_CACHE")"
  marker="$cache_dir/active-build-directory.txt"
  if [[ -f "$marker" && -f "$cache_dir/.relaxer-owned-cache" ]]; then
    IFS= read -r leaf < "$marker" || true
    if [[ "$leaf" =~ ^build-recovery-[0-9]+-[0-9]+$ &&
          ! -L "$cache_dir/$leaf" && -f "$cache_dir/$leaf/Relaxer.exe" ]]; then
      build="$cache_dir/$leaf"
      printf 'Deploying recovered build: %s\n' "$leaf"
    fi
  fi
fi
stage="$(pwd)/runtime.pending"
owner="${RELAXER_SOURCE_ROOT:?}"
[[ -f "$build/Relaxer.exe" ]] || { echo 'Relaxer.exe is missing' >&2; exit 2; }
if [[ -e "$stage" ]]; then
  [[ ! -L "$stage" && ! -L "$stage/.relaxer-staging-owner" && -f "$stage/.relaxer-staging-owner" && "$(cat "$stage/.relaxer-staging-owner")" == "$owner" ]] || { echo 'Staging folder ownership mismatch' >&2; exit 2; }
  rm -rf -- "$stage"
fi
mkdir -p "$stage/bin" "$stage/resources"
printf '%s\n' "$owner" > "$stage/.relaxer-staging-owner"
cp "$build/Relaxer.exe" "$stage/"
cp RelaxerStart.exe THIRD_PARTY_NOTICES.md "$stage/"
# Prefer the newly compiled launcher when available; the first-start bootstrap
# in the source root remains intact while Windows is executing it.
if [[ -f "$build/RelaxerStart.exe" ]]; then cp "$build/RelaxerStart.exe" "$stage/"; fi
cp resources/icon.ico "$stage/resources/"
cp -R licenses "$stage/"
deploy_qt=''
for name in windeployqt6 windeployqt-qt6 windeployqt; do
  if command -v "$name" >/dev/null; then deploy_qt="$name"; break; fi
done
[[ -n "$deploy_qt" ]] || { echo 'windeployqt missing' >&2; exit 2; }
qml_args=()
if grep -q '^RELAXER_ENABLE_QML:BOOL=ON$' "$build/CMakeCache.txt"; then
  qml_args=(--qmldir "$(cygpath -m "$(pwd)/src/qml")")
fi
"$deploy_qt" --release --no-translations "${qml_args[@]}" "$(cygpath -m "$stage/Relaxer.exe")"
plugins=/ucrt64/share/qt6/plugins
[[ -d "$plugins" ]] || plugins=/ucrt64/lib/qt6/plugins
# windeployqt can copy every installed SQL driver, including drivers whose
# optional database clients are absent. Rebuild these plugin groups from the
# application's supported backends before scanning dependencies. The staging
# tree was created above with an ownership marker; the installed Qt tree stays
# untouched.
rm -rf -- "$stage/sqldrivers" "$stage/multimedia"
for kind in sqldrivers multimedia imageformats; do
  [[ -d "$plugins/$kind" ]] || continue
  mkdir -p "$stage/$kind"
  case "$kind" in
    sqldrivers) cp "$plugins/$kind/qsqlite.dll" "$stage/$kind/" ;;
    multimedia) cp "$plugins/$kind/ffmpegmediaplugin.dll" "$stage/$kind/" ;;
    imageformats) find "$plugins/$kind" -maxdepth 1 -iname '*.dll' -exec cp '{}' "$stage/$kind/" \; ;;
  esac
done
[[ -f "$stage/platforms/qwindows.dll" && -f "$stage/sqldrivers/qsqlite.dll" ]] || { echo 'Qt plugins missing' >&2; exit 2; }
[[ -f "$stage/multimedia/ffmpegmediaplugin.dll" ]] || { echo 'Qt FFmpeg playback plugin missing' >&2; exit 2; }
printf '%s\n' '[Paths]' 'Prefix=.' 'Plugins=.' 'QmlImports=qml' > "$stage/qt.conf"
if [[ -x runtime-ffmpeg/ffmpeg.exe && -x runtime-ffmpeg/ffprobe.exe ]]; then
  cp runtime-ffmpeg/ffmpeg.exe runtime-ffmpeg/ffprobe.exe "$stage/bin/"
else
  cp /ucrt64/bin/ffmpeg.exe /ucrt64/bin/ffprobe.exe "$stage/bin/"
fi
if [[ -d /ucrt64/share/licenses ]]; then cp -R /ucrt64/share/licenses "$stage/third-party-licenses"; fi
cmake "-DDEST=$(cygpath -m "$stage")" "-DUCRT_BIN=$(cygpath -m /ucrt64/bin)" \
  "-DOBJDUMP=$(cygpath -m "$(command -v objdump)")" -P "$(cygpath -m scripts/runtime-dependencies.cmake)"
echo 'Runtime collected. Windows-only validation follows.'
