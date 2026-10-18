#!/usr/bin/env python3
"""Run the legacy deployment subset plus C1 against a collected native runtime.

Temporary test EXEs/DLL are removed afterwards. Does not use real user data,
credentials or paid APIs. The package must be a fresh staging directory.
"""
import argparse
import json
from pathlib import Path
import re
import runpy
import shutil
import subprocess
import time
import wave


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("package", "build", "qt-prefix", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    package, build, qt, out = [p.resolve() for p in (args.package, args.build, args.qt_prefix, args.output)]
    out.mkdir(parents=True, exist_ok=False)
    support = runpy.run_path(str(Path(__file__).with_name("measure-phase-c.py")))
    env = support["clean_environment"](out)
    env["RELAXER_TEST_APP"] = str(package / "runtime/Relaxer.exe")
    env["RELAXER_DATA"] = str(out / "test-default-data")
    support["k"].SetErrorMode(0x8003)
    runtime = package / "runtime"
    startup = subprocess.STARTUPINFO(); startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW; startup.wShowWindow = 0
    results = []

    def run(exe, arguments, name, timeout=120):
        start = time.perf_counter()
        visible_startup = subprocess.STARTUPINFO()
        visible_startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        visible_startup.wShowWindow = 5
        process_startup = visible_startup if name.endswith("-navigation") else startup
        with (out / (name + ".stdout")).open("wb") as stdout, (out / (name + ".stderr")).open("wb") as stderr:
            process = subprocess.run([str(exe), *map(str, arguments)], cwd=runtime, env=env,
                                     stdout=stdout, stderr=stderr, timeout=timeout, startupinfo=process_startup)
        results.append({"name": name, "exit": process.returncode, "seconds": time.perf_counter() - start})
        assert process.returncode == 0, results[-1]
        print(json.dumps(results[-1]), flush=True)

    run(runtime / "bin/ffmpeg.exe", ["-version"], "ffmpeg")
    run(runtime / "bin/ffprobe.exe", ["-version"], "ffprobe")
    run(runtime / "Relaxer.exe", ["--data", out / "self-test-data", "--self-test"], "selftest")
    audio = out / "synthetic.wav"
    with wave.open(str(audio), "wb") as file:
        file.setparams((1, 2, 8000, 0, "NONE", "not compressed")); file.writeframes(b"\0" * 16000)
    run(runtime / "Relaxer.exe", ["--media-smoke", audio], "pcm-decode")
    for mode in ("widgets", "qml"):
        for scale in ("1", "2"):
            env["QT_SCALE_FACTOR"] = scale
            tag = f"{mode}-dpi{scale}"
            image = out / (tag + ".png")
            run(runtime / "Relaxer.exe", ["--ui=" + mode, "--data", out / (tag + "-data"), "--screenshot", image], tag)
            assert image.stat().st_size > 1024
    env.pop("QT_SCALE_FACTOR")
    names = ["relaxer_midi_tests", "relaxer_translation_classification_storage_tests", "relaxer_translation_workflow_tests",
             "relaxer_phase_b_integration_tests", "relaxer_phase_b_storage_tests", "relaxer_playback_facade_tests",
             "relaxer_queue_playback_tests", "relaxer_gui_tests", "relaxer_lyrics_search_coordinator_tests",
             "relaxer_translation_coordinator_tests", "relaxer_ai_lyrics_translation_tests", "relaxer_lyrics_persistence_tests",
             "relaxer_phase03_tests", "relaxer_phase031_tests", "relaxer_qml_tests"]
    copied = []
    suites = []
    try:
        for file in [qt / "bin/Qt6Test.dll", *[build / (name + ".exe") for name in names], build / "relaxer_ui_navigation_probe.exe"]:
            target = runtime / file.name; assert not target.exists()
            shutil.copy2(file, target); copied.append(target)
        for name in names:
            env["QT_QUICK_BACKEND"] = "software"
            if name == "relaxer_gui_tests": env["QT_SCALE_FACTOR"] = "2"
            else: env.pop("QT_SCALE_FACTOR", None)
            report = out / (name + ".txt")
            run(runtime / (name + ".exe"), ["-v1", "-o", str(report) + ",txt"], name, 240)
            passed, failed, skipped = map(int, re.search(r"Totals: (\d+) passed, (\d+) failed, (\d+) skipped", report.read_text(encoding="utf-8")).groups())
            suites.append({"name": name, "passed": passed, "failed": failed, "skipped": skipped})
        env.pop("QT_QUICK_BACKEND", None); env.pop("QT_SCALE_FACTOR", None)
        for mode in ("widgets", "qml"):
            run(runtime / "relaxer_ui_navigation_probe.exe", ["--ui", mode, "--data", out / (mode + "-navigation-data"),
                "--output", out / (mode + "-navigation.json")], mode + "-navigation")
    finally:
        for file in copied: file.unlink()
        (out / "suites.json").write_text(json.dumps(suites, indent=2), encoding="utf-8")
    # The collection becomes launcher-ready only after actual native checks.
    marker = runtime / "relaxer-ready.json"
    marker.write_text(json.dumps({"version": "1.5.0", "displayVersion": "V0.2.1-dev-c1", "windows_only_path": True,
        "audio_decode": True, "qml_window": True, "audio_device_required": False, "media_clock": False}), encoding="utf-8")
    assert not (package / "data").exists(), "Do not run this against an existing user installation"
    with (out / "launcher.stdout").open("wb") as stdout, (out / "launcher.stderr").open("wb") as stderr:
        process = subprocess.Popen([str(package / "RelaxerStart.exe")], cwd=package, env=env,
                                   stdout=stdout, stderr=stderr, startupinfo=startup)
        owned = {process.pid}; player = None; deadline = time.perf_counter() + 30
        while time.perf_counter() < deadline:
            table = support["processes"]()
            for pid, (parent, name) in table.items():
                if parent in owned: owned.add(pid)
                if pid in owned and name.lower() == "relaxer.exe" and support["windows"](pid): player = pid
            if player: break
            assert process.poll() is None, process.returncode
            time.sleep(.05)
        assert player, "Launcher did not show Widgets"
        for hwnd in support["windows"](player): assert support["u"].PostMessageW(hwnd, 0x10, 0, 0)
        assert process.wait(timeout=20) == 0
        assert not (owned & support["processes"]().keys()), "Owned process survived normal close"
    created_data = (package / "data").resolve()
    assert created_data.parent == package and created_data.name == "data"
    shutil.rmtree(created_data)
    if (package / "startup.log").exists(): shutil.move(package / "startup.log", out / "launcher-startup.log")
    proof = json.loads((package / "deployment.json").read_text(encoding="utf-8"))
    proof["verified"] = True; proof["note"] = "Native validation completed; see validation evidence"
    proof["verification"] = "System-only PATH, native QPA, PCM/Widgets/QML/launcher, legacy 14-suite subset and C1 suite"
    (package / "deployment.json").write_text(json.dumps(proof, ensure_ascii=False, indent=2), encoding="utf-8")
    (out / "result.json").write_text(json.dumps({"results": results, "suites": suites,
        "qtTestTotals": [sum(s[key] for s in suites) for key in ("passed", "failed", "skipped")],
        "launcherOwnedRemaining": [], "physicalListening": False, "paidRequests": 0}, ensure_ascii=False, indent=2), encoding="utf-8")


if __name__ == "__main__": main()
