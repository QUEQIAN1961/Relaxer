"""Append-free evidence runner: raw streams plus an execution receipt.

Empty stdout is valid evidence only alongside the command/identity/timestamps/
exit code. Failures and timed-out original process handles are never PASS.
"""
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import subprocess
import time


def execute(command, directory, name, *, cwd=None, env=None, identity=None, timeout=None, expected=0, startupinfo=None):
    directory = Path(directory); directory.mkdir(parents=True, exist_ok=True)
    receipt = directory / (name + '.json')
    if receipt.exists():
        raise FileExistsError(receipt)
    record = {'command': list(map(str, command)), 'cwd': str(cwd or Path.cwd()),
              'inputIdentity': identity, 'expectedExit': expected,
              'startedUTC': datetime.now(timezone.utc).isoformat(), 'status': 'ERROR'}
    started = time.monotonic()
    output = directory / (name + '.stdout.log'); error = directory / (name + '.stderr.log')
    try:
        with output.open('xb') as stdout, error.open('xb') as stderr:
            result = subprocess.run(record['command'], cwd=cwd, env=env, stdout=stdout, stderr=stderr, timeout=timeout, startupinfo=startupinfo)
        record.update(exitCode=result.returncode, status='PASS' if result.returncode == expected else 'FAIL')
    except Exception as exception:
        record['error'] = repr(exception)
    finally:
        record.update(finishedUTC=datetime.now(timezone.utc).isoformat(), seconds=round(time.monotonic()-started, 3))
        record['streams'] = {key: {'path': str(path), 'bytes': path.stat().st_size,
            'sha256': hashlib.sha256(path.read_bytes()).hexdigest()} for key, path in [('stdout', output), ('stderr', error)] if path.exists()}
        receipt.write_text(json.dumps(record, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    return record
