#!/usr/bin/env bash
set -euo pipefail
if [[ "${MSYSTEM:-}" != UCRT64 ]]; then
  echo '请在 MSYS2 UCRT64 终端运行。' >&2
  exit 2
fi
cd -- "$(dirname -- "$0")/.."
mode="${1:-}"
run_tests() {
  local total
  total="$(ctest --test-dir "$build_dir" -N | sed -n 's/^Total Tests: *\([0-9][0-9]*\).*$/\1/p')"
  if [[ "$total" =~ ^[1-9][0-9]*$ ]]; then printf 'RELAXER_TEST_TOTAL:%s\n' "$total"; fi
  ctest --test-dir "$build_dir" --output-on-failure
}
build_dir="${RELAXER_BUILD_DIR:-build-windows}"
if [[ -n "${RELAXER_BUILD_DIR:-}" ]]; then build_dir="$(cygpath -u "$RELAXER_BUILD_DIR")"; fi
if [[ -n "$mode" && "$mode" != --launch ]]; then
  echo 'Usage: build-windows-ucrt64.sh [--launch]' >&2
  exit 2
fi
if [[ "$mode" == --launch ]]; then
  if [[ "${RELAXER_PREP_CAPTURE:-0}" != 1 ]]; then
    exec > >(tee -a prepare-startup.log) 2>&1
  fi
  trap 'status=$?; echo "Build stopped (exit $status). See prepare-startup.log." >&2; exit "$status"' ERR
fi
printf '%s\n' 'RELAXER_PHASE:configure'
# This entry explicitly configures before every build. Disable Ninja's
# redundant regeneration target so a ZIP extracted with future timestamps
# cannot repeatedly regenerate build.ninja instead of compiling.
jobs="${RELAXER_BUILD_JOBS:-}"
if ! [[ "$jobs" =~ ^[1-9][0-9]*$ ]]; then
  jobs="$(nproc 2>/dev/null || echo 2)"
fi
if ! [[ "$jobs" =~ ^[1-9][0-9]*$ ]]; then jobs=2; fi
printf 'RELAXER_JOBS:%s\n' "$jobs"
configure_log="$(mktemp)"
if cmake -S . -B "$build_dir" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_SUPPRESS_REGENERATION=ON 2>&1 | tee "$configure_log"; then
  rm -f "$configure_log"
else
  # Windows can retain handles on a timed-out test/build tree. Only recover
  # from access-denied errors, and only inside this project's owned cache.
  if [[ "$mode" != --launch || -z "${RELAXER_BUILD_CACHE:-}" ]] ||
     ! grep -Eqi 'Permission denied|Access is denied|Permission error' "$configure_log"; then
    rm -f "$configure_log"
    exit 1
  fi
  rm -f "$configure_log"
  cache_dir="$(cygpath -u "$RELAXER_BUILD_CACHE")"
  if [[ "$build_dir" != "$cache_dir"/build* || ! -f "$cache_dir/.relaxer-owned-cache" ]]; then
    echo 'Refusing build recovery outside the owned cache.' >&2
    exit 1
  fi
  leaf="build-recovery-$(date +%s)-$$"
  new_build_dir="$cache_dir/$leaf"
  echo "Existing CMake tree was inaccessible; retry in owned cache: $leaf" >&2
  cmake -S . -B "$new_build_dir" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_SUPPRESS_REGENERATION=ON
  printf '%s\n' "$leaf" > "$cache_dir/active-build-directory.txt"
  build_dir="$new_build_dir"
fi
if [[ "$mode" == --launch ]]; then
  printf '%s\n' 'RELAXER_PHASE:compile'
  cmake --build "$build_dir" --parallel "$jobs"
  printf '%s\n' 'RELAXER_PHASE:test'
  run_tests
  echo 'Relaxer compiled. Runtime collection and standalone verification are next.'
  printf '%s\n' 'RELAXER_PHASE:ffmpeg'
  if ! bash scripts/build-slim-ffmpeg.sh; then
    echo '精简 FFmpeg 未能编译。常见格式和 MIDI 仍可播放；稀有格式会在精简 FFmpeg 准备好后播放。' >&2
  fi
  exit 0
fi
cmake --build "$build_dir" --parallel "$jobs"
run_tests
printf '%s\n' '编译与回归完成。启动方式见 README.md；便携部署仍须验证。'
