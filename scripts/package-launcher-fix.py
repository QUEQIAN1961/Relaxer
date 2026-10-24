#!/usr/bin/env python3
"""Promote an unchanged portable payload plus a rebuilt launcher after scoped QA.

The input is a pristine extracted candidate, never a runtime used by QA. Native
QA uses a disposable copy with a test-only ready handshake. Its proof binds all
original payload bytes after replacing the launcher. This command creates the
shipping handshake only after that proof, then uses the strict Phase C packager.
Runtime provenance remains the baseline build; this is not a full rebuild or an
independent Harness attestation.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess

from ready_marker import marker


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def payload(candidate, launcher):
    entries = {}
    for file in sorted(candidate.rglob("*")):
        if file.is_symlink():
            raise ValueError(f"Symlink is not a release input: {file}")
        if file.is_file():
            name = file.relative_to(candidate).as_posix()
            entries[name] = digest(launcher if name == "RelaxerStart.exe" else file)
    return entries


def payload_digest(entries):
    return hashlib.sha256(json.dumps(entries, sort_keys=True, separators=(",", ":")).encode()).hexdigest()


def build_identity(source, launcher):
    def git(*args):
        return subprocess.check_output(["git", *args], cwd=source).decode().strip()
    # MSYS2 Git can corrupt the ^{tree} argument from native Python. Use the
    # equivalent show query, and retain the fingerprint emitted by CMake.
    return {"commit": git("rev-parse", "HEAD"), "tree": git("show", "-s", "--format=%T", "HEAD"),
            "sourceFingerprint": launcher.with_name("relaxer-source-fingerprint.txt").read_text().strip()}


def promote(source, candidate, launcher, proof_path, stage, output):
    def git(*args):
        return subprocess.check_output(["git", *args], cwd=source).decode().strip()

    if stage.exists() or output.exists():
        raise ValueError("Use new staging and output paths; existing artifacts are preserved")
    if git("status", "--porcelain"):
        raise ValueError("Package only clean committed source")
    frozen = build_identity(source, launcher)
    baseline = json.loads((candidate / "deployment.json").read_text(encoding="utf-8"))
    # Only launcher/deployment patches descending from this baseline may inherit
    # its runtime binaries, including a follow-up packaging-only correction.
    subprocess.check_call(["git", "merge-base", "--is-ancestor", baseline["sourceCommit"], "HEAD"], cwd=source)
    allowed = {"src/start-windows.cpp", "scripts/package-launcher-fix.py",
               "docs/phase-c/launcher-deployment-fix.md"}
    if set(git("diff", "--name-only", baseline["sourceCommit"], "HEAD").splitlines()) - allowed:
        raise ValueError("Runtime source changed; perform a new runtime build instead")
    for name in ("Relaxer.exe", "RelaxerQmlUi.dll", "RelaxerStart.exe"):
        file = candidate / (name if name == "RelaxerStart.exe" else "runtime/" + name)
        if digest(file) != baseline["binarySHA256"][name]:
            raise ValueError(f"Baseline binary differs from its deployment manifest: {name}")
    entries = payload(candidate, launcher)
    proof = json.loads(proof_path.read_text(encoding="utf-8"))
    hashes = {name: entries[("" if name == "RelaxerStart.exe" else "runtime/") + name]
              for name in ("Relaxer.exe", "RelaxerStart.exe", "RelaxerQmlUi.dll")}
    required = {"launcher-default-qml": 0, "compatibility-cmd": 0, "missing-qml-module": 11}
    cases = proof.get("cases", [])
    if (proof.get("verified") is not True or proof.get("verificationScope") != "launcher-deployment"
            or proof.get("identity") != frozen or proof.get("binarySHA256") != hashes
            or proof.get("payloadSHA256") != payload_digest(entries)
            or len(cases) != len(required) or {case.get("name") for case in cases} != set(required)
            or any(case.get("status") != "PASS" or case.get("exitCode") != required.get(case.get("name")) for case in cases)
            or proof.get("windowsOnlyPath") is not True or proof.get("chineseSpacePath") is not True):
        raise ValueError("Scoped native proof does not match this source, payload and required cases")
    spec = importlib.util.spec_from_file_location("phase_c_packager", Path(__file__).with_name("package-phase-c.py"))
    packager = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(packager)
    # Assemble from immutable inputs; never clean logs/data out of a QA runtime.
    shutil.copytree(candidate, stage)
    shutil.copy2(launcher, stage / "RelaxerStart.exe")
    proof_hash = digest(proof_path)
    (stage / "runtime/relaxer-ready.json").write_text(json.dumps(marker(baseline["version"],
        displayVersion=baseline["displayVersion"], verificationScope="launcher-deployment",
        windows_only_path=True, qml_window=True, nativeQaProofSHA256=proof_hash), indent=2) + "\n", encoding="utf-8")
    baseline_identity = {key: baseline[key] for key in ("sourceCommit", "sourceTree", "sourceFingerprint")}
    deployment = {**baseline, "sourceCommit": frozen["commit"], "sourceTree": frozen["tree"],
        "sourceFingerprint": frozen["sourceFingerprint"], "binarySHA256": hashes,
        "binaryProvenance": {name: frozen if name == "RelaxerStart.exe" else baseline_identity for name in hashes},
        "baselineDeploymentSHA256": digest(candidate / "deployment.json"),
        "verified": True, "verificationScope": "launcher-deployment", "nativeQaProofSHA256": proof_hash,
        "independentHarness": "PENDING", "executionReceipts": proof_path.name,
        "note": "Scoped developer launcher QA; runtime bytes inherited unchanged from baseline; Harness pending"}
    (stage / "deployment.json").write_text(json.dumps(deployment, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    shutil.copy2(proof_path, stage / "LAUNCHER-QA-PROOF.json")
    guide = source / "docs/phase-c/launcher-deployment-fix.md"
    for name in ("READ-ME-FIRST.md", "HANDOFF.md"):
        shutil.copy2(guide, stage / name)
    # These checks reject unverified/missing readiness, changed binaries and QA
    # residue before a ZIP is created. Never substitute an unrestricted archive().
    packed = packager.package(stage, output, "Relaxer-V0.2.1-LauncherFix-Candidate")
    return {"files": len(packed), "output": str(output), "sha256": digest(output), "identity": frozen}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("source", "candidate", "launcher", "verification-proof", "staging", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    paths = (args.source, args.candidate, args.launcher, args.verification_proof, args.staging, args.output)
    print(json.dumps(promote(*(path.resolve() for path in paths))))
