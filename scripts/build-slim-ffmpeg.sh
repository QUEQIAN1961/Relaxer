#!/usr/bin/env bash
# Builds a small local audio decoder into runtime-ffmpeg/. The binaries are not shipped.
# The source tarball is bundled under third_party/ so first-start does not download it.
# A failure is non-fatal: the caller keeps launching Relaxer.
set -u
root="$(cd "$(dirname -- "$0")/.." && pwd)"
dest="$root/runtime-ffmpeg"
mkdir -p -- "$dest"
if [[ -x "$dest/ffmpeg.exe" && -x "$dest/ffprobe.exe" ]]; then
  echo 'Slim FFmpeg already built.'
  exit 0
fi
if [[ "${MSYSTEM:-}" != UCRT64 ]]; then
  echo 'Skip slim FFmpeg: this step runs in MSYS2 UCRT64.' >&2
  exit 1
fi
jobs="${RELAXER_BUILD_JOBS:-}"
if ! [[ "$jobs" =~ ^[1-9][0-9]*$ ]]; then
  jobs="$(nproc 2>/dev/null || echo 2)"
fi
if ! [[ "$jobs" =~ ^[1-9][0-9]*$ ]]; then jobs=2; fi
work="$(mktemp -d)"
cleanup() { rm -rf -- "$work"; }
trap cleanup EXIT
version="${RELAXER_FFMPEG_VERSION:-7.1}"
expected_sha="40973d44970dbc83ef302b0609f2e74982be2d85916dd2ee7472d30678a7abe6"
archive="$work/ffmpeg.tar.xz"
bundled="$root/third_party/ffmpeg-${version}.tar.xz"
got_archive=0
if [[ -f "$bundled" ]]; then
  printf '%s\n' 'RELAXER_PHASE:ffmpeg-local'
  actual="$(sha256sum "$bundled" | awk '{print $1}')"
  if [[ "$actual" == "$expected_sha" ]]; then
    cp -f -- "$bundled" "$archive"
    echo "Using bundled FFmpeg source: third_party/ffmpeg-${version}.tar.xz"
    got_archive=1
  else
    echo 'Bundled FFmpeg source failed checksum; trying the network.' >&2
  fi
fi
if [[ "$got_archive" != 1 ]]; then
  printf '%s\n' 'RELAXER_PHASE:ffmpeg-download'
  url="https://ffmpeg.org/releases/ffmpeg-${version}.tar.xz"
  if ! curl -L --fail --retry 2 --output "$archive" "$url"; then
    echo 'Could not download the FFmpeg source tarball.' >&2
    exit 1
  fi
fi
tar -xJf "$archive" -C "$work"
source_dir="$(echo "$work"/ffmpeg-*)"
cd -- "$source_dir"
printf '%s\n' 'RELAXER_PHASE:ffmpeg-configure'
./configure \
  --prefix="$dest/prefix" \
  --target-os=mingw32 \
  --arch=x86_64 \
  --enable-small \
  --disable-everything \
  --disable-autodetect \
  --disable-doc \
  --disable-debug \
  --disable-avdevice \
  --disable-swscale \
  --disable-postproc \
  --disable-network \
  --enable-ffmpeg \
  --enable-ffprobe \
  --enable-protocol=file \
  --enable-demuxer=ogg,matroska,ape,wv,tta,mpc,ac3,amr,caf,mov \
  --enable-decoder=vorbis,opus,flac,ape,wavpack,tta,mpc7,mpc8,ac3,eac3,amrnb,amrwb \
  --enable-parser=vorbis,opus,flac,ac3,mpegaudio \
  --enable-muxer=flac \
  --enable-encoder=flac \
  --extra-cflags="-Os" \
  --extra-ldflags="-s"
printf '%s\n' 'RELAXER_PHASE:ffmpeg-compile'
make -j"$jobs"
make install
cp "$dest/prefix/bin/ffmpeg.exe" "$dest/prefix/bin/ffprobe.exe" "$dest/"
rm -rf -- "$dest/prefix"
echo 'Slim FFmpeg is ready in runtime-ffmpeg/.'
