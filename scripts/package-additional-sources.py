"""Bundle the exact MIDI and modified FFmpeg source materials for this release."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import zipfile


def checked(path, expected):
    data = path.read_bytes()
    if hashlib.sha256(data).hexdigest() != expected:
        raise ValueError('Source archive differs from pinned identity: ' + str(path))
    return data


def assemble(source, build, assets, release_revision=None, midi_runtime_dir=None, midi_package_dir=None):
    version = re.search(r'Version\[\]\s*=\s*"([^"]+)"', (source / 'src/phase-c-version.h').read_text())[1]
    if release_revision is not None:
        if not re.fullmatch(r'r[1-9]\d*', release_revision):
            raise ValueError('Expected a release revision such as r2')
        version += '-' + release_revision
    midi_root = midi_runtime_dir or build
    midi = json.loads((midi_root / 'runtime-midi.json').read_text(encoding='utf-8'))
    ffmpeg = json.loads((build / 'runtime-ffmpeg.json').read_text(encoding='utf-8'))
    for label in ('MIDI', 'FFmpeg'):
        destination = assets / f'Relaxer-V{version}-{label}-Source.zip'
        with zipfile.ZipFile(destination, 'x', zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
            prefix = destination.stem + '/'
            if label == 'MIDI':
                keys = ['fluidsynth', 'gcem', 'soundfont', 'sndfileSource']
                msys2 = midi.get('msys2Packages', {})
                if not msys2:
                    keys.append('sndfile')
                for key in keys:
                    entry = midi[key]
                    archive.writestr(prefix + 'archives/' + entry['archive'], checked(source / '.dependencies/downloads' / entry['archive'], entry['sha256']))
                if msys2:
                    if midi_package_dir is None:
                        raise ValueError('Supply --midi-package-dir for corresponding MSYS2 source packages')
                    for package in msys2.values():
                        entry = package['source']
                        name = entry['archive']
                        archive.writestr(prefix + 'archives/' + name, checked(midi_package_dir / name, entry['sha256']))
                        archive.writestr(prefix + 'archives/' + name + '.sig', checked(midi_package_dir / (name + '.sig'), entry['signatureSHA256']))
                    archive.write(source / 'scripts/stage-release-midi.py', prefix + 'scripts/stage-release-midi.py')
                for name in ('scripts/prepare-midi-runtime.py', 'resources/midi-dependencies.json', 'docs/MIDI-BASS.md'):
                    archive.write(source / name, prefix + name)
                archive.write(midi_root / 'runtime-midi.json', prefix + 'runtime-midi.json')
                archive.writestr(prefix + 'BUILD.md', 'Use the matching application Source.zip. Put archives under .dependencies/downloads, then run python scripts/prepare-midi-runtime.py. Original component licenses are inside their source archives. If runtime-midi.json lists msys2Packages, stage the release with scripts/stage-release-midi.py using the corresponding archives and signatures. The MSYS2 source packages include the source, patches and PKGBUILD recipes; base codec sources are available with V0.2.2-GA.\n')
            else:
                spec = json.loads((source / 'resources/ffmpeg-dash-runtime.json').read_text(encoding='utf-8'))
                name = 'ffmpeg-' + spec['version'] + '.tar.xz'
                archive.writestr(prefix + 'archives/' + name, checked(source / '.dependencies/downloads' / name, spec['sha256']))
                for name in ('scripts/prepare-dash-ffmpeg.py', 'resources/ffmpeg-dash-runtime.json', 'docs/third-party/ffmpeg/dash-cancel.patch', 'docs/third-party/ffmpeg/README.md'):
                    archive.write(source / name, prefix + name)
                for name in ('config.h', 'config.mak', 'runtime-ffmpeg.json'):
                    archive.write(source / '.dependencies/ffmpeg' / name, prefix + name)
                archive.write(source / 'LICENSE', prefix + 'LICENSE')
                archive.writestr(prefix + 'README.md', 'FFmpeg ' + spec['version'] + '\n\nUse the matching Relaxer Source.zip and the MSYS2 FFmpeg ABI described by runtime-ffmpeg.json. Put the verified archive under .dependencies/downloads and run python scripts/prepare-dash-ffmpeg.py --msys2 C:/Software/MSYS2. The script applies the included patch and builds only avformat against the matching installed avcodec/avutil. config.h and config.mak record the actual release configuration. See docs/third-party/ffmpeg/README.md and the upstream archive for original copyrights and licenses. Unmodified base dependency sources are available with Relaxer V0.2.2-GA.\n')
                if checked(build / 'avformat-63.dll', ffmpeg['binarySHA256']) != (source / '.dependencies/ffmpeg/avformat-63.dll').read_bytes():
                    raise ValueError('Modified FFmpeg build provenance mismatch')
        print(destination.name)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source', 'build', 'assets'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--release-revision')
    parser.add_argument('--midi-runtime', type=Path)
    parser.add_argument('--midi-package-dir', type=Path)
    args = parser.parse_args()
    assemble(args.source.resolve(), args.build.resolve(), args.assets.resolve(), args.release_revision, args.midi_runtime, args.midi_package_dir)
