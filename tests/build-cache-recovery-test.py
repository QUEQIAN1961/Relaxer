#!/usr/bin/env python3
"""Isolated launcher-build retry test; no Qt, MSYS2 or Windows required."""
import os
from pathlib import Path
import subprocess
import tempfile

SOURCE = Path(__file__).resolve().parents[1] / 'scripts' / 'build-windows-ucrt64.sh'

with tempfile.TemporaryDirectory(prefix='relaxer-build-recovery-') as temporary:
    root = Path(temporary)
    project = root / 'project'
    (project / 'scripts').mkdir(parents=True)
    script = project / 'scripts' / 'build-windows-ucrt64.sh'
    script.write_bytes(SOURCE.read_bytes())
    cache = root / 'owned-cache'
    cache.mkdir()
    (cache / '.relaxer-owned-cache').write_text('fixture\n')
    old = cache / 'build'
    old.mkdir()
    mock = root / 'mock'
    mock.mkdir()
    (mock / 'cygpath').write_text('#!/bin/sh\n[ "$1" = -u ] && { printf "%s\\n" "$2"; exit 0; }\nexit 1\n')
    (mock / 'cmake').write_text('''#!/usr/bin/env bash
if [[ "$1" == -S ]]; then
  build=''
  while (($#)); do
    if [[ "$1" == -B ]]; then build="$2"; break; fi
    shift
  done
  if [[ "$MOCK_MODE" == permission && "$build" == "$OLD_BUILD" ]]; then
    echo 'CMake Error: Permission denied' >&2
    exit 1
  fi
  if [[ "$MOCK_MODE" == other_error && "$build" == "$OLD_BUILD" ]]; then
    echo 'CMake Error: some other configuration error' >&2
    exit 1
  fi
  mkdir -p "$build"
  echo "configured: $build"
  exit 0
fi
[[ "$1" == --build ]] || exit 2
[[ -d "$2" ]] || exit 3
echo "build: $2"
''')
    (mock / 'ctest').write_text('#!/bin/sh\necho ctest-mock-pass\n')
    for name in ('cygpath', 'cmake', 'ctest'):
        (mock / name).chmod(0o755)
    def run(mode):
        env = dict(os.environ, MSYSTEM='UCRT64', RELAXER_BUILD_DIR=str(old),
                   RELAXER_BUILD_CACHE=str(cache), OLD_BUILD=str(old), MOCK_MODE=mode,
                   PATH=str(mock) + os.pathsep + os.environ['PATH'])
        return subprocess.run(['bash', str(script), '--launch'], cwd=project, env=env,
                              capture_output=True, text=True, timeout=10)
    success = run('success')
    assert success.returncode == 0 and 'build: ' + str(old) in success.stdout, success.stderr
    print('PASS: normal configure keeps original build directory')
    denied = run('permission')
    marker = cache / 'active-build-directory.txt'
    assert denied.returncode == 0, denied.stdout + denied.stderr
    assert marker.is_file(), denied.stdout + denied.stderr
    name = marker.read_text().strip()
    assert name.startswith('build-recovery-')
    assert 'build: ' + str(cache / name) in denied.stdout, denied.stdout + denied.stderr
    print('PASS: access denied retries in owned isolated cache and records deployment marker')
    marker.unlink()
    error = run('other_error')
    assert error.returncode != 0 and not marker.exists(), error.stdout + error.stderr
    print('PASS: non-permission errors are not hidden by a recovery build')
print('Totals: 3 passed, 0 failed')
