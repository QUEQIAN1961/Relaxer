#!/usr/bin/env python3
"""Deploy the Phase C development runtime using matching MSYS2 Qt prefixes.

Does not install packages or open user data. Destination must be new. Scan QML
imports, then recursively resolve real PE imports for every copied executable
and plugin, including FFmpeg tools in their own DLL search directory.
"""
import argparse
from collections import deque
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import hashlib
from datetime import datetime, timezone
from ready_marker import marker
from evidence_run import execute
from source_identity import identity
from build_inputs import binary_identity, qml_inputs


def validate_current_build(source, build, git_executable):
    """Reject a retained build configured for another checkout or source tree."""
    frozen = identity(source, git_executable=git_executable)
    cache = (build / 'CMakeCache.txt').read_text(encoding='utf-8')
    home = re.search(r'^CMAKE_HOME_DIRECTORY:INTERNAL=(.+)$', cache, re.MULTILINE)
    if not home or Path(home[1].strip()).resolve() != source.resolve():
        raise ValueError('Build was configured for another source directory')
    built = (build / 'relaxer-source-fingerprint.txt').read_text(encoding='utf-8').strip()
    if built != frozen['sourceFingerprint']:
        raise ValueError('Build fingerprint differs from current committed source')
    return frozen


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("source", "build", "qt-prefix", "quick-prefix", "destination"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--ffmpeg-bin", type=Path)
    parser.add_argument("--git", default=shutil.which("git"), help="Git executable; pass explicitly in a sanitized tool PATH")
    parser.add_argument("--verification-proof", type=Path,
                        help="Native QA proof from a separate candidate; binds the three built binaries")
    args = parser.parse_args()
    # A hard interruption leaves RUNNING, never a successful deployment claim.
    logs = args.destination.resolve().parent / (args.destination.name + '.deployment-evidence')
    logs.mkdir(parents=True, exist_ok=False)
    receipt = {'status': 'RUNNING', 'startedUTC': datetime.now(timezone.utc).isoformat(),
               'destination': str(args.destination.resolve())}
    attempt = logs / 'deployment-attempt.json'
    attempt.write_text(json.dumps(receipt, indent=2)+'\n', encoding='utf-8')
    try:
        deploy(args, logs)
        receipt['status'] = 'COLLECTED'
    except BaseException as error:
        receipt.update(status='ERROR', error=repr(error))
        raise
    finally:
        receipt['finishedUTC'] = datetime.now(timezone.utc).isoformat()
        attempt.write_text(json.dumps(receipt, indent=2)+'\n', encoding='utf-8')


def deploy(args, logs):
    source, build, qt, quick, dest = [p.resolve() for p in
        (args.source, args.build, args.qt_prefix, args.quick_prefix, args.destination)]
    assert not dest.exists(), "Use a fresh destination; existing runtime is preserved"
    assert (qt / "bin/Qt6Core.dll").is_file() and (quick / "bin/Qt6Quick.dll").is_file()
    header = (source / "src/phase-c-version.h").read_text(encoding="utf-8")
    version = re.search(r'Version\[\]\s*=\s*"([^"]+)"', header)[1]
    display = re.search(r'Display\[\]\s*=\s*"([^"]+)"', header)[1]
    fingerprint = (build / "relaxer-source-fingerprint.txt").read_text(encoding="utf-8").strip()
    if not args.git: parser.error("Git is unavailable; provide --git with its executable path")
    commit = subprocess.check_output([args.git, "rev-parse", "HEAD"], cwd=source, text=True).strip()
    tree = subprocess.check_output([args.git, "show", "-s", "--format=%T", "HEAD"], cwd=source, text=True).strip()
    assert not subprocess.check_output([args.git, "status", "--porcelain"], cwd=source, text=True).strip(), "Deploy from clean committed source"
    frozen = validate_current_build(source, build, str(args.git))
    assert (frozen['commit'], frozen['tree']) == (commit, tree), 'Source changed during input validation'
    binaries = ["Relaxer.exe", "RelaxerStart.exe", "RelaxerQmlUi.dll"]
    core_hashes = binary_identity(build, fingerprint)
    binary_hashes = {name: core_hashes[name] for name in binaries}
    verified = False
    native_proof = {}
    if args.verification_proof:
        native_proof = json.loads(args.verification_proof.read_text(encoding="utf-8"))
        assert native_proof.get("verified") is True and native_proof.get("binarySHA256") == binary_hashes, "Native proof does not match build inputs"
        assert native_proof.get("sourceFingerprint") == fingerprint and native_proof.get("sourceCommit") == commit, "Native proof does not match frozen source"
        assert native_proof.get("sourceTree", tree) == tree, "Native proof does not match source tree"
        verified = True
    env = os.environ.copy()
    env["PATH"] = str(quick / "bin") + os.pathsep + str(qt / "bin") + os.pathsep + env["PATH"]
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0

    run_index = 0
    def run(command, cwd=None):
        nonlocal run_index
        run_index += 1
        result = execute(command, logs, f'{run_index:04d}', env=env, cwd=cwd,
            identity={'commit':commit,'tree':tree,'sourceFingerprint':fingerprint}, timeout=120, startupinfo=startup)
        assert result['status'] == 'PASS', result
        return Path(result['streams']['stdout']['path']).read_text(encoding='utf-8', errors='replace')

    imports = json.loads(run([quick / "share/qt6/bin/qmlimportscanner.exe", "-rootPath", source / "src/qml",
                              "-importPath", quick / "share/qt6/qml", "-importPath", qt / "share/qt6/qml"]))
    qml_inputs(imports, (quick, qt))
    dest.mkdir(parents=True)
    runtime = dest / "runtime"
    runtime.mkdir()
    shutil.copy2(build / "Relaxer.exe", runtime)
    shutil.copy2(build / "RelaxerQmlUi.dll", runtime)
    shutil.copy2(build / "RelaxerStart.exe", dest)
    shutil.copy2(build / "RelaxerLockCheck.exe", dest)
    shutil.copy2(source / "THIRD_PARTY_NOTICES.md", dest)
    for name in ('LICENSE', 'LICENSE-NOTICE.txt'):
        shutil.copy2(source / name, dest)
    shutil.copytree(source / "licenses", dest / "licenses")
    (runtime / "resources").mkdir()
    shutil.copy2(source / "resources/icon.ico", runtime / "resources")
    for folder, names in {"platforms": ["qwindows.dll", "qoffscreen.dll"],
                          "sqldrivers": ["qsqlite.dll"], "multimedia": ["ffmpegmediaplugin.dll"]}.items():
        (runtime / folder).mkdir()
        for name in names:
            shutil.copy2(qt / "share/qt6/plugins" / folder / name, runtime / folder)
    for folder in ("imageformats", "iconengines", "tls"):
        plugin = qt / "share/qt6/plugins" / folder
        if plugin.exists(): shutil.copytree(plugin, runtime / folder)
    for entry in imports:
        if entry.get("type") != "module" or not entry.get("path"): continue
        original = Path(entry["path"]).resolve()
        for prefix in (quick, qt):
            module_root = prefix / "share/qt6/qml"
            if original.is_relative_to(module_root):
                target = runtime / "qml" / original.relative_to(module_root)
                shutil.copytree(original, target, dirs_exist_ok=True)
                break
    (runtime / "qt.conf").write_text("[Paths]\nPrefix=.\nPlugins=.\nQmlImports=qml\n", encoding="utf-8")
    (runtime / "bin").mkdir()
    ffmpeg = (args.ffmpeg_bin or qt / "bin").resolve()
    for name in ("ffmpeg.exe", "ffprobe.exe"): shutil.copy2(ffmpeg / name, runtime / "bin")
    copied = set(); seen_dependencies = set()
    system = Path(os.environ.get("SystemRoot", r"C:\Windows")) / "System32"
    objdump = qt / "bin/objdump.exe"

    def closure(output, files, prefixes):
        pending = deque(files)
        seen = set()
        while pending:
            file = pending.popleft()
            if file in seen: continue
            seen.add(file)
            seen_dependencies.add(file)
            # Binutils' narrow filename conversion cannot represent every
            # Unicode absolute path. CreateProcessW can set the real Unicode
            # working directory; pass the unchanged file's ASCII basename.
            for name in re.findall(r"DLL Name:\s*(\S+)", run([objdump, "-p", file.name], cwd=file.parent)):
                if name.lower().startswith(("api-ms-", "ext-ms-")): continue
                target = output / name
                if target.exists():
                    pending.append(target)
                    continue
                found = next((p / name for p in prefixes if (p / name).is_file()), None)
                if found:
                    shutil.copy2(found, target); copied.add(str(target.relative_to(dest))); pending.append(target)
                else:
                    assert (system / name).is_file(), f"Unresolved import {name} from {file}"
    # The optional QML DLL is copied explicitly above. No retained build DLL may
    # silently satisfy a third-party dependency.
    closure(runtime, [runtime / "Relaxer.exe", *runtime.rglob("*.dll")], [quick / "bin", qt / "bin"])
    closure(dest, [dest / "RelaxerStart.exe", dest / "RelaxerLockCheck.exe"], [quick / "bin", qt / "bin"])
    closure(runtime / "bin", list((runtime / "bin").glob("*.exe")), [ffmpeg, qt / "bin"])
    for prefix in (qt, quick):
        license_root = prefix / "share/licenses"
        if license_root.exists(): shutil.copytree(license_root, dest / "third-party-licenses", dirs_exist_ok=True)
    (dest / "RelaxerQml.cmd").write_text('@echo off\r\nsetlocal\r\n"%~dp0runtime\\Relaxer.exe" --ui=qml %*\r\nexit /b %errorlevel%\r\n', encoding="ascii")
    (dest / "RelaxerWidgets.cmd").write_text('@echo off\r\nsetlocal\r\n"%~dp0runtime\\Relaxer.exe" --ui=widgets %*\r\nexit /b %errorlevel%\r\n', encoding="ascii")
    (dest / "READ-ME-FIRST.md").write_text(
        f"# Relaxer {display}\n\n"
        "测试入口：双击 RelaxerQml.cmd；兼容界面：RelaxerWidgets.cmd。\n"
        "RelaxerStart.exe 已包含，但首次启动器正向流程仍待完整验收。\n"
        "两种界面使用同一资料目录和锁，请先关闭另一入口。\n"
        "首次独立验证可使用 runtime/Relaxer.exe --ui=qml --data <测试目录>。\n"
        "歌曲、歌词和自定义资源保存在资料目录；发行包不包含个人资料或 AI 凭据。\n"
        "本包为开发方验证的候选构建，正式验收状态见随包交付报告。\n",
        encoding="utf-8")
    if verified:
        scope = native_proof.get("verificationScope")
        fields = {"windows_only_path": True, "qml_window": True}
        if scope:
            assert native_proof.get("windowsOnlyPath") is True and native_proof.get("qmlWindow") is True, "Scoped native proof lacks window/PATH verification"
            fields.update(verificationScope=scope, nativeQaProofSHA256=hashlib.sha256(args.verification_proof.read_bytes()).hexdigest())
        else:
            fields.update(audio_decode=True, audio_device_required=False, media_clock=False)
        (runtime / "relaxer-ready.json").write_text(json.dumps(marker(version, displayVersion=display, **fields)), encoding="utf-8")
    proof = {"version": version, "displayVersion": display, "sourceFingerprint": fingerprint, "sourceCommit": commit, "sourceTree": tree,
             "qmlImports": imports, "copiedDependencies": sorted(copied), "resolvedDependencyInputs": sorted(str(p) for p in seen_dependencies),
             "binarySHA256": binary_hashes, "coreBinarySHA256": core_hashes, "stagingPolicy": "fresh-build-inputs-only",
             "verified": verified, "verificationScope": native_proof.get("verificationScope", "legacy-native" if verified else "candidate-collection"),
             "missingDependencies": [], "executionReceipts": str(logs),
             "note": "Native proof from separate candidate" if verified else "Candidate collection only; do not package before native QA"}
    (dest / "deployment.json").write_text(json.dumps(proof, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps({"destination": str(dest), "qmlImports": len(imports), "dependencies": len(copied)}))


if __name__ == "__main__": main()
