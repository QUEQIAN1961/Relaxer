#!/usr/bin/env python3
"""Run legacy, C1/Fix1, C2+C3 and C4+C5 suites on a disposable candidate.

Temporary test EXEs/DLL are removed afterwards. Does not use real user data,
credentials or paid APIs. The package must be a fresh staging directory.
"""
import argparse
import ctypes
from ctypes import wintypes
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

    def run(exe, arguments, name, timeout=120, cwd=None):
        start = time.perf_counter()
        visible_startup = subprocess.STARTUPINFO()
        visible_startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        visible_startup.wShowWindow = 5
        native_gui = Path(exe).name.lower() == "relaxer.exe" or name.endswith("_tests")
        process_startup = visible_startup if name.endswith("-navigation") or native_gui else startup
        with (out / (name + ".stdout")).open("wb") as stdout, (out / (name + ".stderr")).open("wb") as stderr:
            process = subprocess.run([str(exe), *map(str, arguments)], cwd=cwd or out, env=env,
                                     stdout=stdout, stderr=stderr, timeout=timeout, startupinfo=process_startup)
        results.append({"name": name, "exit": process.returncode, "seconds": time.perf_counter() - start})
        (out / "program-results.json").write_text(json.dumps(results, indent=2), encoding="utf-8")
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
             "relaxer_phase03_tests", "relaxer_phase031_tests", "relaxer_qml_tests", "relaxer_qml_loading_tests",
             "relaxer_library_model_tests", "relaxer_c2c3_tests", "relaxer_qml_widgets_parity_tests", "relaxer_queue_projection_tests", "relaxer_qml_lyrics_model_tests",
             "relaxer_c4c5_bridge_tests", "relaxer_c4c5_ui_tests", "relaxer_qml_search_bridge_tests", "relaxer_qml_upcoming_parity_tests", "relaxer_qml_ai_parity_tests", "relaxer_qml_settings_parity_tests", "relaxer_qml_ui_parity_library_tests", "relaxer_qml_lyrics_workspace_parity_tests", "relaxer_qml_v020_ui_parity_tests", "relaxer_qml_ui2_refinement_tests"]
    names += ["relaxer_qml_c6_evidence_tests", "relaxer_qml_c6_parity_tests"]
    env["RELAXER_C6_EVIDENCE"] = str(out / "native-evidence")
    # Theme suites record evidence before the C6 suite initializes this folder.
    (out / "native-evidence").mkdir()
    deployment = json.loads((package / "deployment.json").read_text(encoding="utf-8"))
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
            run(runtime / (name + ".exe"), ["-v1", "-o", report.name + ",txt"], name, 240)
            passed, failed, skipped = map(int, re.search(rb"Totals: (\d+) passed, (\d+) failed, (\d+) skipped", report.read_bytes()).groups())
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
    # version is the retained legacy startup protocol, not the product version.
    marker.write_text(json.dumps({"version": "1.5.0", "displayVersion": deployment["displayVersion"], "windows_only_path": True,
        "audio_decode": True, "qml_window": True, "audio_device_required": False, "media_clock": False}), encoding="utf-8")
    assert not (package / "data").exists(), "Do not run this against an existing user installation"
    with (out / "launcher.stdout").open("wb") as stdout, (out / "launcher.stderr").open("wb") as stderr:
        launcher_env = env.copy(); launcher_env.pop("RELAXER_DATA", None)
        process = subprocess.Popen([str(package / "RelaxerStart.exe")], cwd=out, env=launcher_env,
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
        # The launcher deliberately detaches after its 2.5-second loader check.
        # Its exit cannot establish that its GUI child has finished closing.
        kernel = support["k"]
        kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
        kernel.OpenProcess.restype = wintypes.HANDLE
        kernel.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
        kernel.GetExitCodeProcess.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD)]
        child = kernel.OpenProcess(0x100000 | 0x1000, False, player)
        assert child, "Cannot monitor the launched GUI child"
        closing = time.perf_counter()
        try:
            for hwnd in support["windows"](player): assert support["u"].PostMessageW(hwnd, 0x10, 0, 0)
            launcher_code = process.wait(timeout=20)
            child_wait = kernel.WaitForSingleObject(child, max(0, int((20 - (time.perf_counter() - closing)) * 1000)))
            child_code = wintypes.DWORD()
            assert kernel.GetExitCodeProcess(child, ctypes.byref(child_code))
            remaining = sorted(owned & support["processes"]().keys())
            launcher_result = {"launcherExit": launcher_code, "childWait": child_wait,
                "childExit": child_code.value, "closeMs": (time.perf_counter() - closing) * 1000,
                "remainingOwned": remaining}
            (out / "launcher-result.json").write_text(json.dumps(launcher_result, indent=2), encoding="utf-8")
            assert launcher_code == 0 and child_wait == 0 and child_code.value == 0, launcher_result
            assert not remaining, "Owned process survived normal close"
        finally:
            kernel.CloseHandle(child)
    created_data = (package / "data").resolve()
    assert created_data.parent == package and created_data.name == "data"
    assert not (created_data / "relaxer.lock").exists(), "Launcher child did not release its data lock"
    assert (runtime / "startup.log").is_file(), "First launcher run must log inside runtime"
    assert not (out / "startup.log").exists() and not (package / "startup.log").exists()
    proof = json.loads((package / "deployment.json").read_text(encoding="utf-8"))
    # The executed candidate remains dirty and unpackageable. This proof is used
    # only to reassemble a new shipping directory from the same build binaries.
    proof["verified"] = True; proof["note"] = "Developer native C6 candidate QA; awaiting independent Harness R16"
    proof["verification"] = "System-only PATH, native QPA, PCM/Widgets/QML/launcher; actual suite list and counts in result.json; C6 lyrics, fullscreen, motion, fonts, tray/native-message shortcuts, real import and Mock-only consent/preview workflows"
    (out / "verification-proof.json").write_text(json.dumps(proof, ensure_ascii=False, indent=2), encoding="utf-8")
    (out / "result.json").write_text(json.dumps({"results": results, "suites": suites,
        "qtTestTotals": [sum(s[key] for s in suites) for key in ("passed", "failed", "skipped")],
        "launcherOwnedRemaining": [], "physicalListening": False, "paidRequests": 0}, ensure_ascii=False, indent=2), encoding="utf-8")


if __name__ == "__main__": main()
