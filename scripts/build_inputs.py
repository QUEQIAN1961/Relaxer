"""Validate compiled identity and required QML modules before runtime collection."""
import hashlib
from pathlib import Path

CORE_BINARIES = ('Relaxer.exe', 'RelaxerStart.exe', 'RelaxerQmlUi.dll', 'RelaxerLockCheck.exe')


def binary_identity(build, fingerprint):
    expected = ('RelaxerBuildSourceSHA256=' + fingerprint).encode('ascii') + b'\0'
    hashes = {}
    for name in CORE_BINARIES:
        path = Path(build) / name
        data = path.read_bytes()
        if expected not in data:
            raise ValueError(f'Compiled source identity mismatch: {name}')
        hashes[name] = hashlib.sha256(data).hexdigest()
    return hashes


def qml_inputs(imports, prefixes):
    modules = [entry for entry in imports if entry.get('type') == 'module']
    names = {entry.get('name') for entry in modules}
    required = {'QtQuick', 'QtQuick.Controls', 'QtQuick.Layouts', 'QtQml'}
    if not required.issubset(names):
        raise ValueError(f'Required QML imports absent: {sorted(required - names)}')
    roots = [Path(prefix).resolve() / 'share/qt6/qml' for prefix in prefixes]
    for entry in modules:
        original = Path(entry.get('path') or '__missing_qml_module__').resolve()
        if not any(original.is_relative_to(root) for root in roots):
            raise ValueError(f'Unresolved or foreign QML module: {entry.get("name")}')
        if not (original / 'qmldir').is_file():
            raise ValueError(f'Missing QML module descriptor: {original}')
        plugin = entry.get('plugin')
        if plugin and not (original / (plugin + '.dll')).is_file():
            raise ValueError(f'Missing QML plugin: {original / (plugin + ".dll")}')
    return modules
