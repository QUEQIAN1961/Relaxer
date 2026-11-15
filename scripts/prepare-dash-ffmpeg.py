"""Build the pinned DASH cancellation fix against the existing MSYS2 FFmpeg ABI.

Only avformat-63.dll is replaced; installed Qt/FFmpeg files remain untouched.
Retain the verified upstream archive, patch and generated configuration for source delivery.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import urllib.request


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def prepare(msys, jobs):
    root = Path(__file__).resolve().parent.parent
    spec = json.loads((root / 'resources/ffmpeg-dash-runtime.json').read_text(encoding='utf-8'))
    env = dict(os.environ, MSYSTEM='UCRT64', CHERE_INVOKING='1')
    env['PATH'] = str(msys / 'ucrt64/bin') + os.pathsep + env['PATH']
    subprocess.run([str(msys / 'usr/bin/bash.exe'), '-lc', 'command -v make cmp'], env=env, check=True)
    version = subprocess.check_output([str(msys / 'ucrt64/bin/ffmpeg.exe'), '-version'], env=env, text=True)
    if not version.startswith('ffmpeg version ' + spec['version'] + ' '):
        raise RuntimeError('This narrow avformat patch requires the pinned MSYS2 FFmpeg 9.0.2 ABI')
    archive = root / '.dependencies/downloads/ffmpeg-9.0.2.tar.xz'
    archive.parent.mkdir(parents=True, exist_ok=True)
    if not archive.exists() or digest(archive) != spec['sha256']:
        with urllib.request.urlopen(spec['url'], timeout=120) as response:
            payload = response.read()
        if hashlib.sha256(payload).hexdigest() != spec['sha256']:
            raise RuntimeError('FFmpeg source checksum mismatch')
        archive.write_bytes(payload)
    work = root / '.dependencies/ffmpeg-build'
    source = work / 'ffmpeg-9.0.2'
    if not source.exists():
        work.mkdir(parents=True, exist_ok=True)
        with tarfile.open(archive) as package:
            package.extractall(work, filter='data')
    patch = root / 'docs/third-party/ffmpeg/dash-cancel.patch'
    dash = source / 'libavformat/dashdec.c'
    if b'return ret ? ret : AVERROR_EXIT;' not in dash.read_bytes():
        subprocess.run([str(msys / 'usr/bin/patch.exe'), '-p1', '-i', str(patch)], cwd=source, check=True)
    # Reuse matching installed avcodec/avutil import libraries. Their DLLs and
    # optimized codec implementations are not rebuilt or replaced.
    for library in ('avcodec', 'avutil'):
        shutil.copy2(msys / ('ucrt64/lib/lib' + library + '.dll.a'), source / ('lib' + library))
    command = ['bash', '-lc', 'cd "$(cygpath -u "$1")"; shift; exec ./configure "$@"', 'configure', str(source), *spec['configure']]
    command[0] = str(msys / 'usr/bin/bash.exe')
    subprocess.run(command, env=env, check=True)
    subprocess.run([str(msys / 'usr/bin/bash.exe'), '-lc', 'cd "$(cygpath -u "$1")"; exec make -j"$2" -o libavcodec/avcodec-63.dll -o libavcodec/avcodec.dll -o libavutil/avutil-61.dll -o libavutil/avutil.dll libavformat/avformat-63.dll', 'build', str(source), str(jobs)], env=env, check=True)
    destination = root / '.dependencies/ffmpeg'
    destination.mkdir(parents=True, exist_ok=True)
    binary = destination / 'avformat-63.dll'
    shutil.copy2(source / 'libavformat/avformat-63.dll', binary)
    licenses = destination / 'licenses/ffmpeg'
    licenses.mkdir(parents=True, exist_ok=True)
    for name in ('COPYING.GPLv3', 'LICENSE.md'):
        shutil.copy2(source / name, licenses)
    shutil.copy2(source / 'config.h', destination / 'config.h')
    shutil.copy2(source / 'ffbuild/config.mak', destination / 'config.mak')
    metadata = dict(version=spec['version'], basePackage=spec['basePackage'], sourceSHA256=spec['sha256'], patchSHA256=digest(patch), binarySHA256=digest(binary), configure=spec['configure'])
    (destination / 'runtime-ffmpeg.json').write_text(json.dumps(metadata, indent=2) + '\n', encoding='utf-8')
    print('Prepared patched FFmpeg avformat runtime:', destination)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--msys2', type=Path, default=Path('C:/Software/MSYS2'))
    parser.add_argument('--jobs', type=int, default=6)
    args = parser.parse_args()
    prepare(args.msys2.resolve(), max(1, args.jobs))
