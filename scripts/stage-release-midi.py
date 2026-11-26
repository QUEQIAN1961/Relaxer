"""Stage the RC MIDI redistributables from signed MSYS2 packages."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path, PurePosixPath


PACKAGES = {
    'libsndfile': ('1.2.2-1', 'libsndfile-1.dll', 'sndfile.dll'),
    'flac': ('1.5.0-3', 'libFLAC.dll', 'libFLAC.dll'),
    'mpg123': ('1.33.7-1', 'libmpg123-0.dll', 'libmpg123-0.dll'),
}
REPOSITORY = 'https://repo.msys2.org/mingw/'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def msys_path(path):
    return '/'+path.drive[0].lower()+'/'+path.relative_to(path.anchor).as_posix()


def stage(build, packages, destination, msys2):
    windows_tar = Path('C:/Windows/System32/tar.exe')
    gpg = msys2 / 'usr/bin/gpg.exe'
    gpgv = msys2 / 'usr/bin/gpgv.exe'
    destination.mkdir(parents=True, exist_ok=True)
    keyring = destination / 'msys2-public.gpg'
    if not keyring.exists():
        subprocess.run([str(gpg), '--batch', '--yes', '--no-options', '--dearmor',
                        '--output', msys_path(keyring),
                        '/usr/share/pacman/keyrings/msys2.gpg'], check=True)
    manifest = json.loads((build / 'runtime-midi.json').read_text(encoding='utf-8'))
    manifest['buildSdk'] = manifest['sndfile']
    manifest['msys2Packages'] = {}
    manifest['distribution'] = (
        'FluidSynth and libsndfile LGPL-2.1-or-later; libsndfile redistributable from MSYS2, '
        'dynamically linked with the packaged codecs; GeneralUser GS separate soundfont license')

    def write(relative, data):
        target = destination / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
        manifest['files'][relative] = sha(data)

    for name, digest in list(manifest['files'].items()):
        if name == 'sndfile.dll':
            continue
        data = (build / name).read_bytes()
        if sha(data) != digest:
            raise ValueError('MIDI build file differs from its manifest: ' + name)
        write(name, data)

    for name, (version, dll, target) in PACKAGES.items():
        binary = f'mingw-w64-ucrt-x86_64-{name}-{version}-any.pkg.tar.zst'
        source = f'mingw-w64-{name}-{version}.src.tar.zst'
        records = {}
        for kind, filename, folder in [('binary', binary, 'ucrt64'), ('source', source, 'sources')]:
            archive = packages / filename
            signature = packages / (filename + '.sig')
            subprocess.run([str(gpgv), '--keyring', msys_path(keyring), msys_path(signature), msys_path(archive)], check=True)
            records[kind] = dict(archive=filename, sha256=sha(archive.read_bytes()),
                                 signatureSHA256=sha(signature.read_bytes()), url=REPOSITORY+folder+'/'+filename)
        archive = packages / binary
        members = subprocess.check_output([str(windows_tar), '-tf', str(archive)], text=True).splitlines()
        if 'ucrt64/bin/'+dll not in members:
            raise ValueError('Expected DLL absent: ' + dll)
        write(target, subprocess.check_output([str(windows_tar), '-xOf', str(archive), 'ucrt64/bin/'+dll]))
        notices = [member for member in members if member.startswith('ucrt64/share/licenses/') and not member.endswith('/')]
        if not notices:
            raise ValueError('No licence notices in ' + binary)
        for member in notices:
            relative = PurePosixPath(member).relative_to('ucrt64/share/licenses')
            if '..' in relative.parts:
                raise ValueError('Unexpected licence path')
            write('licenses/midi/msys2/'+relative.as_posix(), subprocess.check_output([str(windows_tar), '-xOf', str(archive), member]))
        manifest['msys2Packages'][name] = dict(version=version, runtime=target, **records)
    manifest['sndfile'] = dict(version='1.2.2-1', **manifest['msys2Packages']['libsndfile']['source'])
    (destination / 'runtime-midi.json').write_text(json.dumps(manifest, indent=2)+'\n', encoding='utf-8')


if __name__ == '__main__':
    cli = argparse.ArgumentParser(description=__doc__)
    for name in ('build', 'packages', 'destination', 'msys2'):
        cli.add_argument('--'+name, type=Path, required=True)
    args = cli.parse_args()
    stage(args.build.resolve(), args.packages.resolve(), args.destination.resolve(), args.msys2.resolve())
