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


def inventory(stage):
    stage = stage.resolve()
    proof = json.loads((stage / "deployment.json").read_text(encoding="utf-8"))
    if proof.get("stagingPolicy") != "fresh-build-inputs-only" or proof.get("verified") is not True:
        raise ValueError("Package only a fresh stage assembled with the matching separate candidate QA proof")
    entries = {}
    media = {".mp3", ".flac", ".wav", ".ogg", ".opus", ".mid", ".midi", ".m4a", ".lrc"}
    for file in sorted(stage.rglob("*")):
        if file.is_symlink(): raise ValueError(f"Symlink is not a release input: {file}")
        if not file.is_file(): continue
        relative = file.relative_to(stage).as_posix()
        lower = relative.lower(); parts = lower.split("/"); name = parts[-1]
        forbidden = (file.suffix.lower() in media | {".log", ".tmp", ".dmp", ".pyc", ".qmlc", ".pem", ".pfx", ".key"}
            or ".sqlite" in name or name.endswith((".db", ".db-wal", ".db-shm"))
            or name == ".env" or name.startswith(".env.")
            or any(p in {"data", "__pycache__", ".git", ".cache", "cmakefiles", "browser-translation"} for p in parts))
        if forbidden: raise ValueError(f"Runtime residue or private input is forbidden: {relative}")
        entries[relative] = hashlib.sha256(file.read_bytes()).hexdigest()
    for name in ["RelaxerStart.exe", "RelaxerQml.cmd", "runtime/Relaxer.exe", "runtime/RelaxerQmlUi.dll",
                 "runtime/relaxer-ready.json", "runtime/Qt6Qml.dll", "runtime/Qt6Quick.dll", "runtime/Qt6QuickControls2.dll"]:
        if name not in entries: raise ValueError(f"Required release input missing: {name}")
    actual = {name: entries[("" if name == "RelaxerStart.exe" else "runtime/") + name]
              for name in ("Relaxer.exe", "RelaxerStart.exe", "RelaxerQmlUi.dll")}
    if actual != proof.get("binarySHA256"):
        raise ValueError("Stage binaries differ from the native candidate QA proof")
    return entries


def package(stage, output, prefix):
    entries = inventory(stage)
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
    parser.add_argument("--prefix", default="Relaxer-V0.2.1-dev-C4C5-UI2")
    args = parser.parse_args()
    entries = package(args.staging.resolve(), args.output.resolve(), args.prefix)
    print(json.dumps({"files": len(entries), "residue": False, "output": str(args.output)}))
