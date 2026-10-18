#!/usr/bin/env bash
# Each phase uses a fresh shell, from the native launcher or dependency CMD.
set -euo pipefail
if [[ "${MSYSTEM:-}" != UCRT64 ]]; then
  echo 'Please use RelaxerStart.exe or the MSYS2 UCRT64 terminal.' >&2
  exit 2
fi
case "${1:-}" in
  --update|--install|--check) mode="$1" ;;
  *) echo 'Usage: install-windows-dependencies.sh --update|--install|--check' >&2; exit 2 ;;
esac
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.."
command -v pacman >/dev/null || { echo 'MSYS2 pacman was not found.' >&2; exit 2; }
packages=(
  mingw-w64-ucrt-x86_64-gcc
  mingw-w64-ucrt-x86_64-cmake
  mingw-w64-ucrt-x86_64-ninja
  mingw-w64-ucrt-x86_64-qt6-base
  mingw-w64-ucrt-x86_64-qt6-declarative
  mingw-w64-ucrt-x86_64-qt6-multimedia
  mingw-w64-ucrt-x86_64-qt6-multimedia-ffmpeg
  mingw-w64-ucrt-x86_64-qt6-svg
  mingw-w64-ucrt-x86_64-qt6-imageformats
  mingw-w64-ucrt-x86_64-ffmpeg
)
if [[ "$mode" == --check ]]; then
  # Read-only first-start probe. Never synchronize or upgrade from this branch.
  pacman -Q "${packages[@]}" >/dev/null 2>&1 && exit 0
  exit 1
fi
# The native launcher owns logging. Do not spawn tee under an MSYS2 runtime
# which pacman is about to replace/terminate; keep tee for manual terminal use.
if [[ "${RELAXER_PREP_CAPTURE:-0}" != 1 ]]; then
  exec > >(tee -a install-dependencies.log) 2>&1
fi
trap 'status=$?; echo "Dependency setup stopped (exit $status). See install-dependencies.log." >&2; exit "$status"' ERR
# Queries are read-only; only update/install transactions need this lock.
# Recover an orphan only in the exact MSYS2 environment selected by the
# launcher. Active transactions, custom databases and uncertain paths stay locked.
database=/var/lib/pacman
if command -v pacman-conf >/dev/null; then
  database="$(pacman-conf DBPath)"
fi
lock="${database%/}/db.lck"
if [[ -e "$lock" ]]; then
  echo "锁文件：$lock" >&2
  recovered=0
  if [[ -f ./RelaxerLockCheck.exe ]] && command -v cygpath >/dev/null; then
    if lock_windows="$(cygpath -w "$lock")" &&
       ./RelaxerLockCheck.exe --recover-selected-pacman-lock "$lock_windows"; then
      recovered=1
    fi
  else
    echo '缺少数据库锁检查组件，请完整覆盖解压修复包后重试。' >&2
  fi
  if [[ "$recovered" == 1 && ! -e "$lock" ]]; then
    echo 'Database lock check passed.'
  else
    echo 'MSYS2 软件包数据库仍被锁定，本次更新/安装已停止。' >&2
    echo '请等待已有安装或更新任务完成，再重新运行；不要同时打开多个部署入口。' >&2
    echo '若仍失败，请提供 prepare-startup.log（手动运行另附 install-dependencies.log）。' >&2
    echo '未被选中的 MSYS2、自定义数据库及活动锁不会被自动清理。' >&2
    exit 75
  fi
fi
if [[ "$mode" == --update ]]; then
  echo 'Updating the complete MSYS2 installation...'
  # Deterministic output lets the native launcher distinguish MSYS2's expected
  # core-runtime shell shutdown from real update failures. This phase never
  # runs install/build in a shell that loaded the old runtime.
  # Replace the phase shell instead of keeping an old Bash runtime waiting
  # beneath pacman. The Windows parent captures the original exit and output.
  export LC_ALL=C
  exec pacman -Syu --noconfirm --color never
fi
# A core update can finish before the remaining packages are upgraded.
# Refuse to install new libraries into an installation with pending upgrades.
pending="$(mktemp)"
query_errors="$pending.stderr"
trap 'rm -f -- "$pending" "$query_errors"' EXIT
query_status=0
pacman -Qu > "$pending" 2> "$query_errors" || query_status=$?
if [[ -s "$query_errors" ]]; then cat "$query_errors" >&2; fi
if [[ "$query_status" -gt 1 || ( "$query_status" -ne 0 && -s "$query_errors" ) ]]; then
  echo 'Unable to check pending MSYS2 upgrades.' >&2
  exit "$query_status"
fi
if [[ -s "$pending" ]]; then
  cat "$pending"
  echo 'MSYS2 still has pending upgrades. Close other MSYS2 terminals and run the CMD again.' >&2
  exit 75
fi
echo 'Installing Relaxer build and runtime dependencies; existing versions are skipped...'
pacman -S --needed --noconfirm "${packages[@]}"
echo 'Verifying installed packages...'
pacman -Q "${packages[@]}"
echo 'All required Relaxer packages are installed.'
