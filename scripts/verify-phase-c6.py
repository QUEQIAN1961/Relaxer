#!/usr/bin/env python3
"""Reproducible C6 configure/build/CTest with isolated, append-only evidence.

Pass the actual Qt Base and Declarative prefixes; no developer-specific path
is embedded. All CTest runtime imports are supplied by project test properties.
"""
import argparse
from datetime import datetime, timezone
import ctypes
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
import uuid


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=Path(__file__).resolve().parents[1])
    for name in ('build', 'qt-prefix', 'evidence', 'temporary-root'):
        parser.add_argument('--'+name, type=Path, required=True)
    parser.add_argument('--quick-prefix', type=Path)
    parser.add_argument('--steps', nargs='+', choices=('configure', 'build', 'ctest'), default=['configure', 'build', 'ctest'])
    parser.add_argument('--target', nargs='+')
    parser.add_argument('--test-regex')
    parser.add_argument('--jobs', type=int, default=8)
    parser.add_argument('--name', default='C6')
    parser.add_argument('--fresh-build', action='store_true')
    parser.add_argument('--require-clean', action='store_true')
    args = parser.parse_args()
    source, build, qt, evidence, temporary = [p.resolve() for p in
        (args.source, args.build, args.qt_prefix, args.evidence, args.temporary_root)]
    quick = (args.quick_prefix or qt).resolve()
    if args.fresh_build and build.exists():
        parser.error('Fresh build requires a new directory; existing files are preserved')
    for prefix, name in ((qt, 'Qt6Core.dll'), (quick, 'Qt6Quick.dll')):
        if not (prefix/'bin'/name).is_file():
            parser.error('Missing selected Qt runtime: '+str(prefix/'bin'/name))
    def git(*command):
        return subprocess.check_output(['git', *command], cwd=source, text=True, encoding='utf-8').strip()
    identity = {'commit':git('rev-parse','HEAD'), 'tree':git('show','-s','--format=%T','HEAD'), 'status':git('status','--porcelain')}
    if args.require_clean and identity['status']:
        parser.error('Source worktree must be clean')
    stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')+'-'+uuid.uuid4().hex[:8]
    run = evidence/args.name/('Run-'+stamp)
    run.mkdir(parents=True, exist_ok=False)
    shutil.copy2(__file__, run/'runner.py')
    if os.name == 'nt':
        ctypes.windll.kernel32.SetErrorMode(0x8003)
    suffix = '.exe' if os.name == 'nt' else ''
    commands = {
        'configure':[qt/('bin/cmake'+suffix), '-S',source,'-B',build,'-G','Ninja','-DCMAKE_BUILD_TYPE=Release',
                     '-DCMAKE_PREFIX_PATH='+qt.as_posix(), '-DQT_ADDITIONAL_PACKAGES_PREFIX_PATH='+quick.as_posix(), '-DRELAXER_ENABLE_QML=ON'],
        'build':[qt/('bin/cmake'+suffix),'--build',build,'--parallel',str(args.jobs)],
        'ctest':[qt/('bin/ctest'+suffix),'--test-dir',build,'--output-on-failure']}
    if args.target:
        commands['build'] += ['--target',*args.target]
    if args.test_regex:
        commands['ctest'] += ['-R',args.test_regex]
    results = []
    print(str(run), flush=True)
    for step in args.steps:
        if git('rev-parse','HEAD') != identity['commit'] or git('status','--porcelain') != identity['status']:
            raise RuntimeError('Source changed during verification')
        folder = run/step
        folder.mkdir()
        env = os.environ.copy()
        for key in list(env):
            if key.upper() in {'PATH','QML_IMPORT_PATH','QML2_IMPORT_PATH','QT_PLUGIN_PATH','RELAXER_DATA','QT_QPA_PLATFORM'}:
                del env[key]
        system = Path(os.environ.get('SystemRoot', 'C:/Windows'))
        paths = [qt/'bin',system/'System32',system]
        if step != 'ctest': paths.insert(0,quick/'bin')
        env['PATH'] = os.pathsep.join(map(str,paths))
        env.update(QT_QUICK_BACKEND='software',QML_DISABLE_DISK_CACHE='1',RELAXER_SKIP_AUDIO_DEVICE_TESTS='1',PYTHONDONTWRITEBYTECODE='1')
        temp = temporary/uuid.uuid4().hex[:8]
        temp.mkdir(parents=True, exist_ok=False)
        env['TMP'] = env['TEMP'] = str(temp)
        for key in ('APPDATA','LOCALAPPDATA'):
            profile = folder/'profile'/key
            profile.mkdir(parents=True)
            env[key] = str(profile)
        env['RELAXER_C6_EVIDENCE'] = str(folder)
        env['RELAXER_UI2_SCREENSHOTS'] = str(folder/'screenshots')
        command = list(map(str,commands[step]))
        record = {**identity,'step':step,'cwd':str(source),'command':command,'startedUtc':datetime.now(timezone.utc).isoformat(),
                  'environment':{key:env.get(key) for key in ('PATH','QML_IMPORT_PATH','QML2_IMPORT_PATH','QT_PLUGIN_PATH','QT_QUICK_BACKEND','TEMP')}}
        (folder/'command.json').write_text(json.dumps(record,indent=2),encoding='utf-8')
        start = time.monotonic()
        with (folder/'output.log').open('xb') as output:
            result = subprocess.run(command,cwd=source,env=env,stdout=output,stderr=subprocess.STDOUT)
        record.update(exitCode=result.returncode,elapsedSeconds=time.monotonic()-start,finishedUtc=datetime.now(timezone.utc).isoformat())
        fingerprint = build/'relaxer-source-fingerprint.txt'
        if fingerprint.exists(): record['sourceFingerprint'] = fingerprint.read_text(encoding='utf-8').strip()
        (folder/'result.json').write_text(json.dumps(record,indent=2),encoding='utf-8')
        if step == 'ctest' and (build/'Testing').exists():
            shutil.copytree(build/'Testing',folder/'Testing')
        results.append(record)
        (run/'results.json').write_text(json.dumps(results,indent=2),encoding='utf-8')
        print(json.dumps({'step':step,'exitCode':result.returncode,'seconds':round(record['elapsedSeconds'],2)}),flush=True)
        if result.returncode: return result.returncode
    return 0


if __name__ == '__main__':
    sys.stdout.reconfigure(encoding='utf-8')
    raise SystemExit(main())
