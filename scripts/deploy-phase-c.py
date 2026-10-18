#!/usr/bin/env python3
"""Deploy the C1 development runtime using matching MSYS2 Qt prefixes.

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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("source", "build", "qt-prefix", "quick-prefix", "destination"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--ffmpeg-bin", type=Path)
    parser.add_argument("--verification-proof", type=Path,
                        help="Native QA proof from a separate candidate; binds the three built binaries")
    args = parser.parse_args()
    source, build, qt, quick, dest = [p.resolve() for p in
        (args.source, args.build, args.qt_prefix, args.quick_prefix, args.destination)]
    assert not dest.exists(), "Use a fresh destination; existing runtime is preserved"
    assert (qt / "bin/Qt6Core.dll").is_file() and (quick / "bin/Qt6Quick.dll").is_file()
    binaries = ["Relaxer.exe", "RelaxerStart.exe", "RelaxerQmlUi.dll"]
    binary_hashes = {name: hashlib.sha256((build / name).read_bytes()).hexdigest() for name in binaries}
    verified = False
    if args.verification_proof:
        native_proof = json.loads(args.verification_proof.read_text(encoding="utf-8"))
        assert native_proof.get("verified") is True and native_proof.get("binarySHA256") == binary_hashes, "Native proof does not match build inputs"
        verified = True
    env = os.environ.copy()
    env["PATH"] = str(quick / "bin") + os.pathsep + str(qt / "bin") + os.pathsep + env["PATH"]
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0

    def run(command):
        result = subprocess.run(list(map(str, command)), env=env, capture_output=True,
                                text=True, encoding="utf-8", errors="replace", timeout=120, startupinfo=startup)
        assert result.returncode == 0, result.stdout + result.stderr
        return result.stdout

    imports = json.loads(run([quick / "share/qt6/bin/qmlimportscanner.exe", "-rootPath", source / "src/qml",
                              "-importPath", quick / "share/qt6/qml", "-importPath", qt / "share/qt6/qml"]))
    dest.mkdir(parents=True)
    runtime = dest / "runtime"
    runtime.mkdir()
    shutil.copy2(build / "Relaxer.exe", runtime)
    shutil.copy2(build / "RelaxerQmlUi.dll", runtime)
    shutil.copy2(build / "RelaxerStart.exe", dest)
    shutil.copy2(source / "THIRD_PARTY_NOTICES.md", dest)
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
    copied = set()
    system = Path(os.environ.get("SystemRoot", r"C:\Windows")) / "System32"
    objdump = qt / "bin/objdump.exe"

    def closure(output, files, prefixes):
        pending = deque(files)
        seen = set()
        while pending:
            file = pending.popleft()
            if file in seen: continue
            seen.add(file)
            for name in re.findall(r"DLL Name:\s*(\S+)", run([objdump, "-p", file])):
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
    closure(runtime, [runtime / "Relaxer.exe", *runtime.rglob("*.dll")], [build, quick / "bin", qt / "bin"])
    closure(runtime / "bin", list((runtime / "bin").glob("*.exe")), [ffmpeg, qt / "bin"])
    for prefix in (qt, quick):
        license_root = prefix / "share/licenses"
        if license_root.exists(): shutil.copytree(license_root, dest / "third-party-licenses", dirs_exist_ok=True)
    (dest / "RelaxerQml.cmd").write_text('@echo off\r\nsetlocal\r\n"%~dp0runtime\\Relaxer.exe" --ui=qml %*\r\nexit /b %errorlevel%\r\n', encoding="ascii")
    (dest / "C1-READ-ME-FIRST.md").write_text(
        "# Relaxer V0.2.1-dev-c1-fix1\n\nC1 Fix1 工程修复开发交付，等待 Harness R13 独立复验。\n\n"
        "默认运行 RelaxerStart.exe 使用完整 Widgets。运行 RelaxerQml.cmd 打开可选 QML Foundation。\n"
        "两种模式使用相同 dataRoot 与资料锁，请先关闭另一种模式。资料路径沿用原算法；"
        "有意使用其他资料库时可直接运行 runtime/Relaxer.exe --data <目录> --ui=qml。\n\n"
        "QML 当前含导航、基础主题、真实曲库数量、播放状态与已恢复歌曲的播放/暂停。"
        "列表、歌单、歌词、完整设置与 AI 尚未迁移，界面有明确标记。"
        "进度与音量只读；完整功能请使用 Widgets。\n", encoding="utf-8")
    if verified:
        (runtime / "relaxer-ready.json").write_text(json.dumps({"version": "1.5.0", "displayVersion": "V0.2.1-dev-c1-fix1",
            "windows_only_path": True, "audio_decode": True, "qml_window": True,
            "audio_device_required": False, "media_clock": False}), encoding="utf-8")
    proof = {"version": "0.2.1-dev-c1-fix1", "qmlImports": imports, "copiedDependencies": sorted(copied),
             "binarySHA256": binary_hashes, "stagingPolicy": "fresh-build-inputs-only",
             "verified": verified, "note": "Native proof from separate candidate" if verified else "Candidate collection only; do not package before native QA"}
    (dest / "deployment.json").write_text(json.dumps(proof, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps({"destination": str(dest), "qmlImports": len(imports), "dependencies": len(copied)}))


if __name__ == "__main__": main()
