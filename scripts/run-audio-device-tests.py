#!/usr/bin/env python3
"""Explicit hardware acceptance. Exit 77 is blocked, never a physical-audio pass."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import time

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    environment = os.environ.copy()
    environment.pop('RELAXER_SKIP_AUDIO_DEVICE_TESTS', None)
    environment['QT_MEDIA_BACKEND'] = 'ffmpeg'
    environment['QT_QPA_PLATFORM'] = 'windows' if os.name == 'nt' else 'xcb'
    rows = []
    for executable, method in [('relaxer_tests', 'realPlaybackAndSeek'),
                               ('relaxer_tests', 'realResumeAndLoop'),
                               ('relaxer_gui_tests', 'playerBarStableAndRealSeek')]:
        report = args.output / (method + '.txt')
        log = args.output / (method + '.process.log')
        if report.exists() or log.exists():
            raise FileExistsError('Use a fresh output directory: ' + str(args.output))
        program = args.build / (executable + ('.exe' if os.name == 'nt' else ''))
        command = [str(program.resolve()), method, '-v1', '-o', str(report.resolve()) + ',txt']
        row = dict(method=method, command=command)
        start = time.monotonic()
        with log.open('xb') as stream:
            process = subprocess.Popen(command, env=environment, stdout=stream, stderr=subprocess.STDOUT)
            try:
                code = process.wait(timeout=60)
                text = report.read_text(encoding='utf-8', errors='replace') if report.exists() else ''
                # A QtTest SKIP exits zero. Explicitly prevent that becoming PASS.
                status = 'ENVIRONMENT BLOCKED' if code == 0 and 'SKIP ' in text else (
                    'PASS' if code == 0 and '0 failed, 0 skipped' in text else 'FAIL')
                row.update(exitCode=code, status=status)
            except subprocess.TimeoutExpired:
                process.kill(); process.wait()
                row.update(status='TIMEOUT / UNRESOLVED', exitCode=None)
        row['seconds'] = time.monotonic() - start
        rows.append(row)
        print(json.dumps(row), flush=True)
    status = 'FAIL' if any(r['status'] in ('FAIL', 'TIMEOUT / UNRESOLVED') for r in rows) else (
        'ENVIRONMENT BLOCKED' if any(r['status'] != 'PASS' for r in rows) else 'PASS')
    (args.output / 'audio-results.json').write_text(json.dumps(dict(
        status=status, skipVariableRemoved=True, cases=rows,
        limitation='Programmatic device/output checks do not constitute human listening acceptance'), indent=2) + '\n', encoding='utf-8')
    return 0 if status == 'PASS' else (77 if status == 'ENVIRONMENT BLOCKED' else 1)

if __name__ == '__main__':
    raise SystemExit(main())
