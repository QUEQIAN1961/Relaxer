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
    (dest / "UI-REPAIR-READ-ME-FIRST.md").write_text('# Relaxer V0.2.1-dev-c4c5-ui2-fix1\n\n基于 C4+C5 的 v0.2.0 界面与交互恢复版，等待独立验收。\n\n运行 RelaxerQml.cmd 打开本次修复的 QML 界面。RelaxerStart.exe 沿用原 Widgets 入口。两种界面共享资料路径与锁，请先关闭另一种界面。首次验证建议使用独立资料目录：\nruntime/Relaxer.exe --ui=qml --data <测试目录>\n\n原侧栏恢复本地音乐、歌单、个人、设置、我喜欢与我的歌单。左下角封面或标题展开播放页，保留底部播放栏。恢复切页过渡、封面飞行动画、切歌动画及下一首浮层。\n查找歌词位于歌曲右键菜单、批量操作栏及“查找缺失歌词”；设置仅保留 AI 服务配置。选中歌曲查歌词和预览翻译不改变当前播放。\n修复加载模型后无法选择、刷新丢失配置等问题；模型选择立即保存。顶部操作控件统一为 36px，补齐按钮边框；正文字号和字重适度提高；清理重复悬停提示，列表与菜单统一圆角。背景展开与收起使用一致缓动；下一首先向下离开窗口，再从另一侧底边垂直升起。AI 联网验证使用回环 Mock，未访问用户 Key，未进行真实付费调用。\n字体、外观编辑、快捷键、托盘、导入、个人资料及歌单卡片已接回 QML。保留文件的删除和从歌单移除沿用原核心。\n本次为开发修复包，不是正式版本。数据和数据库格式未改变；旧交付包保留。\n', encoding="utf-8")
    if verified:
        (runtime / "relaxer-ready.json").write_text(json.dumps({"version": "1.5.0", "displayVersion": "V0.2.1-dev-c4c5-ui2-fix1",
            "windows_only_path": True, "audio_decode": True, "qml_window": True,
            "audio_device_required": False, "media_clock": False}), encoding="utf-8")
    proof = {"version": "0.2.1-dev-c4c5-ui2-fix1", "qmlImports": imports, "copiedDependencies": sorted(copied),
             "binarySHA256": binary_hashes, "stagingPolicy": "fresh-build-inputs-only",
             "verified": verified, "note": "Native proof from separate candidate" if verified else "Candidate collection only; do not package before native QA"}
    (dest / "deployment.json").write_text(json.dumps(proof, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps({"destination": str(dest), "qmlImports": len(imports), "dependencies": len(copied)}))


if __name__ == "__main__": main()
