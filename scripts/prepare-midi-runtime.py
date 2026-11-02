"""Stage pinned BASS/BASSMIDI and GeneralUser GS for local Windows MIDI playback."""
import argparse
import hashlib
import json
import urllib.request
import zipfile
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
    for component in ('bass', 'bassmidi', 'soundfont'):
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
            if component == 'soundfont':
                prefix = 'GeneralUser-GS-' + info['revision'] + '/'
                save('midi/GeneralUser-GS.sf2', package.read(prefix + 'GeneralUser-GS.sf2'))
                save('licenses/midi/GeneralUser-GS-License.txt', package.read(prefix + 'documentation/LICENSE.txt'))
                save('licenses/midi/GeneralUser-GS-README.md', package.read(prefix + 'README.md'))
            else:
                save(component + '.dll', package.read('x64/' + component + '.dll'))
                save('licenses/midi/' + component.upper() + '.txt', package.read(component + '.txt'))
    (destination / 'runtime-midi.json').write_text(json.dumps(dict(metadata, files=files), ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    return {'destination': str(destination), 'files': len(files), 'soundfont': metadata['soundfont']['version']}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    root = Path(__file__).resolve().parent.parent / '.dependencies'
    parser.add_argument('--destination', type=Path, default=root / 'midi')
    parser.add_argument('--cache', type=Path, default=root / 'downloads')
    args = parser.parse_args()
    print(json.dumps(prepare(args.destination.resolve(), args.cache.resolve()), ensure_ascii=False))
