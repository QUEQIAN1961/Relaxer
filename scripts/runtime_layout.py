"""Share byte-identical Windows runtime DLLs without changing playback binaries."""
import hashlib
import os
from pathlib import Path
import re
import subprocess


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def imports(binary, objdump):
    output = subprocess.check_output(
        [str(objdump), '-p', binary.name], cwd=binary.parent,
        encoding='utf-8', errors='replace')
    return re.findall(r'DLL Name:\s*(\S+)', output)


def verify_runtime_closure(runtime, objdump):
    """Resolve every PE import using the portable launcher's runtime PATH.

    The application and tool keep their own directory first. The root launcher
    supplies runtime/ and runtime/bin/ on PATH, so tools can share identical DLLs
    with the application. Different tool-local versions must remain in bin/.
    """
    system = Path(os.environ.get('SystemRoot', 'C:/Windows')) / 'System32'
    binaries = sorted(path for path in runtime.rglob('*')
                      if path.suffix.lower() in ('.exe', '.dll'))
    for binary in binaries:
        for name in imports(binary, objdump):
            if name.lower().startswith(('api-ms-', 'ext-ms-')):
                continue
            candidates = (binary.parent / name, runtime / name,
                          system / name, runtime / 'bin' / name)
            if not any(path.is_file() for path in candidates):
                raise ValueError('Unresolved packaged import: ' +
                                 binary.relative_to(runtime).as_posix() + ': ' + name)
    return len(binaries)


def share_runtime_libraries(stage, qt_prefix):
    runtime = stage / 'runtime'
    removed = []
    for duplicate in sorted((runtime / 'bin').glob('*.dll')):
        shared = runtime / duplicate.name
        if shared.is_file() and digest(duplicate) == digest(shared):
            removed.append(dict(path=duplicate.relative_to(stage).as_posix(),
                                shared=shared.relative_to(stage).as_posix(),
                                bytes=duplicate.stat().st_size, sha256=digest(shared)))
            duplicate.unlink()
    count = verify_runtime_closure(runtime, qt_prefix / 'bin/objdump.exe')
    return dict(sharedLibraryDirectory='runtime/',
                toolDirectory='runtime/bin/',
                searchPath=['runtime/', 'runtime/bin/'],
                removedIdenticalCopies=removed,
                bytesSaved=sum(row['bytes'] for row in removed),
                inspectedPEFiles=count,
                differingToolLibrariesPreserved=True)
