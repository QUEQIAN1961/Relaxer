"""Stage pinned FluidSynth and GeneralUser GS for local Windows MIDI playback."""
import argparse
import hashlib
import json
import urllib.request
import zipfile
import subprocess
from pathlib import Path


def sha(data):
    return hashlib.sha256(data).hexdigest()


def prepare(destination, cache):
    source = Path(__file__).resolve().parent.parent
    metadata = json.loads((source / 'resources/midi-dependencies.json').read_text(encoding='utf-8'))
    cache.mkdir(parents=True, exist_ok=True)
    destination.mkdir(parents=True, exist_ok=True)
    files = {}
    def save(name, data):
        path = destination / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        files[name] = sha(data)
    for component in ('gcem', 'fluidsynth', 'soundfont'):
        info = metadata[component]
        archive = cache / info['archive']
        if not archive.exists() or sha(archive.read_bytes()) != info['sha256']:
            request = urllib.request.Request(info['url'], headers={'User-Agent': 'Relaxer dependency preparation'})
            with urllib.request.urlopen(request, timeout=120) as response:
                data = response.read()
            if sha(data) != info['sha256']:
                raise ValueError('Upstream archive changed; review the pinned dependency: ' + component)
            archive.write_bytes(data)
        with zipfile.ZipFile(archive) as package:
            if component == 'gcem':
                package.extractall(destination.parent / 'fluidsynth-build')
                save('licenses/midi/GCEM-MIT.txt', package.read('gcem-' + info['version'] + '/LICENSE'))
            elif component == 'soundfont':
                prefix = 'GeneralUser-GS-' + info['revision'] + '/'
                save('midi/GeneralUser-GS.sf2', package.read(prefix + 'GeneralUser-GS.sf2'))
                save('licenses/midi/GeneralUser-GS-License.txt', package.read(prefix + 'documentation/LICENSE.txt'))
                save('licenses/midi/GeneralUser-GS-README.md', package.read(prefix + 'README.md'))
            else:
                work = destination.parent / 'fluidsynth-build'
                source_dir = work / ('fluidsynth-' + info['version'])
                if not source_dir.exists():
                    package.extractall(work)
                build = work / 'build'
                options = ['-Denable-cpp11=ON', '-DCMAKE_BUILD_TYPE=Release', '-DBUILD_TESTING=OFF', '-Denable-signalsmith=OFF',
                           '-DGCEM_INCLUDE_DIR=' + str(work / ('gcem-' + metadata['gcem']['version']) / 'include')]
                for feature in ('libsndfile', 'sdl3', 'readline', 'jack', 'pipewire', 'pulseaudio',
                                'portaudio', 'dbus', 'network', 'ipv6', 'dsound', 'wasapi',
                                'waveout', 'winmidi', 'native-dls', 'aufile'):
                    options.append('-Denable-' + feature + '=OFF')
                subprocess.run(['cmake', '-S', str(source_dir), '-B', str(build), '-G', 'Ninja', *options], check=True)
                subprocess.run(['cmake', '--build', str(build), '--target', 'libfluidsynth', '-j', '6'], check=True)
                save('libfluidsynth-3.dll', (build / 'src/libfluidsynth-3.dll').read_bytes())
                save('licenses/midi/FluidSynth-LGPL-2.1.txt', package.read('fluidsynth-' + info['version'] + '/LICENSE'))
    (destination / 'runtime-midi.json').write_text(json.dumps(dict(metadata, files=files), ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    return {'destination': str(destination), 'files': len(files), 'soundfont': metadata['soundfont']['version']}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    root = Path(__file__).resolve().parent.parent / '.dependencies'
    parser.add_argument('--destination', type=Path, default=root / 'midi')
    parser.add_argument('--cache', type=Path, default=root / 'downloads')
    args = parser.parse_args()
    print(json.dumps(prepare(args.destination.resolve(), args.cache.resolve()), ensure_ascii=False))
