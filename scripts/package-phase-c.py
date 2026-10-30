#!/usr/bin/env python3
"""Package a freshly reassembled, native-verified stage. Never package a QA runtime.

Logs, databases, user media, credentials and caches are rejected before creating
the ZIP. Regenerate a fresh stage from build inputs after QA; no runtime deletion
or in-place log cleanup is used. Entries and timestamps are deterministic.
"""
import argparse
import hashlib
import json
from pathlib import Path
import zipfile


def inventory(stage, test_proof=None):
    stage = stage.resolve()
    proof = json.loads((stage / "deployment.json").read_text(encoding="utf-8"))
    if proof.get("stagingPolicy") != "fresh-build-inputs-only":
        raise ValueError("Package only fresh build inputs")
    if test_proof is None and proof.get("verified") is not True:
        raise ValueError("Package only a fresh stage assembled with the matching separate candidate QA proof")
    if test_proof is not None:
        if test_proof.get('purpose') != 'private-test-unverified' or test_proof.get('publicDistributionApproved') is not False:
            raise ValueError('Explicit private test proof required')
        for name in ('sourceCommit','sourceTree','sourceFingerprint','coreBinarySHA256'):
            if not proof.get(name) or proof[name] != test_proof.get(name):
                raise ValueError(f'Test proof identity mismatch: {name}')
        for name in ('dependencyClosure','qmlStartup','widgetsStartup','sqliteIntegrity'):
            if test_proof.get('checks',{}).get(name) != 'PASS':
                raise ValueError(f'Incomplete private test validation: {name}')
        if (stage/'runtime/relaxer-ready.json').exists():
            raise ValueError('Private test package must not carry a launcher ready marker')
    entries = {}
    media = {".mp3", ".flac", ".wav", ".ogg", ".opus", ".mid", ".midi", ".m4a", ".lrc"}
    for file in sorted(stage.rglob("*")):
        if file.is_symlink(): raise ValueError(f"Symlink is not a release input: {file}")
        if not file.is_file(): continue
        relative = file.relative_to(stage).as_posix()
        lower = relative.lower(); parts = lower.split("/"); name = parts[-1]
        forbidden = (file.suffix.lower() in media | {".log", ".tmp", ".dmp", ".pyc", ".qmlc", ".pem", ".pfx", ".key"}
            or ".sqlite" in name or name.endswith((".db", ".db-wal", ".db-shm"))
            or name == ".env" or name.startswith(".env.") or name.endswith("_tests.exe") or name == "qt6test.dll"
            or any(p in {"data", "__pycache__", ".git", ".cache", "cmakefiles", "browser-translation"} for p in parts))
        if forbidden: raise ValueError(f"Runtime residue or private input is forbidden: {relative}")
        entries[relative] = hashlib.sha256(file.read_bytes()).hexdigest()
    required = ["RelaxerStart.exe", "RelaxerQml.cmd", "runtime/Relaxer.exe", "runtime/RelaxerQmlUi.dll",
                "runtime/Qt6Qml.dll", "runtime/Qt6Quick.dll", "runtime/Qt6QuickControls2.dll", "Relaxer.exe", "PORTABLE-SHA256SUMS.txt"]
    required += (["TEST-ONLY-README.txt", "BUILD-IDENTITY.json", "RelaxerWidgets.cmd", "RelaxerLockCheck.exe", "runtime/qt.conf"]
                 if test_proof is not None else ["runtime/relaxer-ready.json"])
    for name in required:
        if name not in entries: raise ValueError(f"Required release input missing: {name}")
    actual = {name: entries[("" if name == "RelaxerStart.exe" else "runtime/") + name]
              for name in ("Relaxer.exe", "RelaxerStart.exe", "RelaxerQmlUi.dll")}
    if actual != proof.get("binarySHA256"):
        raise ValueError("Stage binaries differ from the native candidate QA proof")
    if test_proof is not None:
        actual['RelaxerLockCheck.exe']=entries['RelaxerLockCheck.exe']
        actual['RelaxerPortable.exe']=entries['Relaxer.exe']
        if actual != test_proof['coreBinarySHA256']:
            raise ValueError('Private test core binaries differ from validated candidate')
    return entries


def package(stage, output, prefix, test_proof=None):
    entries = inventory(stage, test_proof)
    if test_proof is not None and 'TEST-UNVERIFIED' not in output.name:
        raise ValueError('Private test ZIP filename must contain TEST-UNVERIFIED')
    if output.exists(): raise ValueError(f"Existing artifact preserved: {output}")
    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, "w") as archive:
        for name in [*entries, "PACKAGE-MANIFEST.json"]:
            info = zipfile.ZipInfo(prefix.rstrip("/") + "/" + name, (1980, 1, 1, 0, 0, 0))
            info.external_attr = 0o100644 << 16
            data = (stage / name).read_bytes() if name in entries else json.dumps(entries, ensure_ascii=False, indent=2).encode("utf-8")
            archive.writestr(info, data, compress_type=zipfile.ZIP_DEFLATED, compresslevel=6)
    return entries


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--staging", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--prefix", help="Defaults to the frozen deployment display version")
    parser.add_argument("--test-proof", type=Path, help="Separate standalone private-test QA proof; never grants launcher/release readiness")
    args = parser.parse_args()
    proof = json.loads((args.staging / "deployment.json").read_text(encoding="utf-8"))
    test_proof = json.loads(args.test_proof.read_text(encoding='utf-8')) if args.test_proof else None
    entries = package(args.staging.resolve(), args.output.resolve(), args.prefix or "Relaxer-" + proof["displayVersion"], test_proof)
    print(json.dumps({"files": len(entries), "residue": False, "output": str(args.output)}))
