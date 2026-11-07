"""Assemble an RC or GA from published dependencies and a matching build."""
import argparse
import hashlib
import json
import re
import shutil
import subprocess
import zipfile
from pathlib import Path, PurePosixPath

from build_inputs import binary_identity
from source_identity import identity

BASELINE_SHA256 = '00a6cbac8725d4b1568c4a06329c627cb8dc59a1a94f052f638bb3b1e88b6c9e'


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def write_json(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')


def assemble(source, build, baseline, destination):
    if destination.exists():
        raise ValueError('Use a new destination; existing archives are preserved')
    if subprocess.check_output(['git', 'status', '--porcelain'], cwd=source).strip():
        raise ValueError('Commit the source before packaging')
    version = re.search(r'Version\[\]\s*=\s*"([^"]+)"', (source / 'src/phase-c-version.h').read_text())[1]
    if not re.fullmatch(r'\d+\.\d+\.\d+-(?:RC\d*|GA)', version):
        raise ValueError('Expected an RC or GA version')
    channel = 'GA' if version.endswith('-GA') else 'RC'
    frozen = identity(source)
    fingerprint = (build / 'relaxer-source-fingerprint.txt').read_text().strip()
    if fingerprint != frozen['sourceFingerprint']:
        raise ValueError('Configure and rebuild from the current committed source')
    core = binary_identity(build, fingerprint)
    if digest(baseline) != BASELINE_SHA256:
        raise ValueError('Baseline is not the published V0.2.2-GA Windows archive')

    display = 'V' + version
    stage = destination / 'Desktop-Win64'
    stage.mkdir(parents=True)
    with zipfile.ZipFile(baseline) as archive:
        prefix = archive.namelist()[0].split('/')[0] + '/'
        manifest = json.loads(archive.read(prefix + 'PACKAGE-MANIFEST.json'))
        components = json.loads(archive.read(prefix + 'COMPONENTS.json'))
        for name, expected in manifest.items():
            relative = PurePosixPath(name)
            if relative.is_absolute() or '..' in relative.parts or '\\' in name or ':' in name:
                raise ValueError('Unsafe baseline path')
            data = archive.read(prefix + name)
            if hashlib.sha256(data).hexdigest() != expected:
                raise ValueError('Baseline manifest mismatch: ' + name)
            if name.startswith(('runtime/', 'licenses/', 'third-party-licenses/')):
                path = stage / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(data)

    for binary, target in [('RelaxerPortable.exe', 'Relaxer.exe'), ('Relaxer.exe', 'runtime/Relaxer.exe'),
                           ('RelaxerQmlUi.dll', 'runtime/RelaxerQmlUi.dll')]:
        shutil.copy2(build / binary, stage / target)
    midi_runtime = json.loads((build / 'runtime-midi.json').read_text(encoding='utf-8'))
    for name, expected in midi_runtime['files'].items():
        relative = PurePosixPath(name)
        if relative.is_absolute() or '..' in relative.parts or '\\' in name or ':' in name:
            raise ValueError('Unsafe MIDI runtime path')
        path = build / name
        if digest(path) != expected:
            raise ValueError('MIDI runtime differs from its staged manifest: ' + name)
        target = stage / 'runtime' / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, target)
    shutil.copy2(build / 'runtime-midi.json', stage / 'runtime/runtime-midi.json')
    shutil.copy2(source / 'docs/MIDI-BASS.md', stage / 'MIDI-BASS.md')
    for name in ('LICENSE', 'LICENSE-NOTICE.txt', 'THIRD_PARTY_NOTICES.md'):
        shutil.copy2(source / name, stage / name)
    (stage / 'licenses/ncmdump').mkdir(parents=True, exist_ok=True)
    shutil.copy2(source / 'licenses/ncmdump-MIT.txt', stage / 'licenses/ncmdump/LICENSE.txt')
    markers = stage / 'licenses/playlist-marks'
    markers.mkdir(parents=True, exist_ok=True)
    for name in ('LICENSE.txt', 'README.md'):
        shutil.copy2(source / 'resources/playlist-marks' / name, markers / name)
    notes = source / 'docs/releases' / ('v' + version)
    shutil.copy2(notes / 'RELEASE-NOTES.md', stage / 'RELEASE-NOTES.md')
    (stage / 'READ-ME-FIRST.md').write_text(
        '# Relaxer ' + display + '\n\n完整解压后双击根目录 Relaxer.exe。兼容界面使用 Relaxer.exe --widgets。\n'
        '本包自带运行依赖，不需要 Qt、编译环境或源码准备工具；保留完整 runtime 和配套清单。\n'
        '默认资料位于同级 data/，也可用 --data 指定独立目录。升级前退出旧程序并备份资料，先用副本验证。\n'
        + ('正式版更新与验证范围见 RELEASE-NOTES.md。\n' if channel == 'GA' else '这是候选预发行版。更新与验证范围见 RELEASE-NOTES.md。\n') +
        'MIDI 使用开源 FluidSynth 与 GeneralUser GS；运行库、音色许可及对应源码资料见 MIDI-BASS.md。\n'
        '应用源码附件：Relaxer-' + display + '-Source.zip；基础第三方源码沿用 V0.2.2-GA 附件；新增 MIDI 源码见同版 MIDI-Source.zip。\n', encoding='utf-8')

    metadata = dict(sourceCommit=frozen['commit'], sourceTree=frozen['tree'], sourceFingerprint=fingerprint,
                    displayVersion=display, fileVersion=version.split('-')[0] + '.0', entry='Relaxer.exe',
                    published=False, publicReleaseApproved=True, acceptanceStatus='rc-targeted-checks-complete',
                    channel=channel, dependencyBaseline=baseline.name,
                    dependencyBaselineSHA256=BASELINE_SHA256)
    metadata['midiDependencies'] = midi_runtime
    metadata['coreBinarySHA256'] = {'Relaxer.exe': core['RelaxerPortable.exe'],
                                   'runtime/Relaxer.exe': core['Relaxer.exe'],
                                   'runtime/RelaxerQmlUi.dll': core['RelaxerQmlUi.dll']}
    write_json(stage / 'BUILD-IDENTITY.json', metadata)
    write_json(stage / 'deployment.json', dict(metadata, runtime='runtime/', userDataDefault='data/',
               userDataOverride='--data or RELAXER_DATA', sourcePreparationRequired=False, readyMarkerRequired=False,
               integrity=dict(manifest='PORTABLE-SHA256SUMS.txt', mode='full-sha256-every-launch', workers=4, cache=False)))
    components['displayVersion'] = display
    # A GA version label is not evidence of a completed Harness regression.
    components['publicReleaseApproved'] = True
    components['acceptanceStatus'] = 'rc-targeted-checks-complete'
    components['packages'].append(dict(name='Lucide playlist markers', version=['0.468.0'],
                                      licenses=['ISC', 'MIT'], upstream=['https://github.com/lucide-icons/lucide/tree/0.468.0/icons']))
    components['midiLicenseCompatibility'] = 'FluidSynth LGPL-2.1-or-later with GPL-3.0-only; separate soundfont license; see MIDI-BASS.md'
    components['sourceMaterials'] = 'Relaxer-' + display + '-Source.zip; Relaxer-V0.2.2-GA-Third-Party-Source.zip'
    components['sourceMaterials'] += '; Relaxer-' + display + '-MIDI-Source.zip'
    components['midiDistributionScope'] = midi_runtime['distribution']
    for key, name, license_name in [('fluidsynth', 'FluidSynth', 'LGPL-2.1-or-later'),
                                     ('soundfont', 'GeneralUser GS', 'LicenseRef-GeneralUser-GS')]:
        components['packages'].append(dict(name=name, version=[midi_runtime[key]['version']],
                                          licenses=[license_name], upstream=[midi_runtime[key]['url']]))
    for name, component in [('libfluidsynth-3.dll', 'FluidSynth')]:
        components['binaries'].append(dict(path='runtime/' + name, component=component))
    for package in components['packages']:
        if package['name'] == 'Relaxer':
            package['version'] = [display]
    for binary in components['binaries']:
        path = stage / binary['path']
        binary.update(sha256=digest(path), bytes=path.stat().st_size)
    write_json(stage / 'COMPONENTS.json', components)
    runtime_hashes = {path.relative_to(stage).as_posix(): digest(path) for path in sorted((stage / 'runtime').rglob('*')) if path.is_file()}
    (stage / 'PORTABLE-SHA256SUMS.txt').write_text(''.join(value + '  ' + name + '\n' for name, value in runtime_hashes.items()), encoding='utf-8')
    write_json(stage / 'PACKAGE-MANIFEST.json', {path.relative_to(stage).as_posix(): digest(path) for path in sorted(stage.rglob('*')) if path.is_file()})

    assets = destination / 'Assets'
    assets.mkdir()
    shutil.copy2(notes / 'RELEASE-NOTES.md', assets / 'RELEASE-NOTES.md')
    portable_zip = assets / ('Relaxer-' + display + '-Win64.zip')
    with zipfile.ZipFile(portable_zip, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
        for path in sorted(stage.rglob('*')):
            if path.is_file():
                archive.write(path, portable_zip.stem + '/' + path.relative_to(stage).as_posix())
    source_zip = assets / ('Relaxer-' + display + '-Source.zip')
    subprocess.run(['git', 'archive', '--format=zip', '--prefix=' + source_zip.stem + '/', '-o', str(source_zip), 'HEAD'], cwd=source, check=True)
    with zipfile.ZipFile(source_zip) as archive:
        archive.extractall(destination / 'Source')
    return dict(version=display, commit=frozen['commit'], portable=str(portable_zip), source=str(source_zip), files=len(runtime_hashes))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source', 'build', 'baseline', 'destination'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(assemble(*(getattr(args, name).resolve() for name in ('source', 'build', 'baseline', 'destination'))), ensure_ascii=False))
