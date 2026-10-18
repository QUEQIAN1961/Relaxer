#!/usr/bin/env python3
"""Native Windows C1 startup/memory/idle CPU/WM_CLOSE and descendant measurements.

Uses only an isolated, identical empty database copy per sample. No toolchain
PATH/QML paths reach the application. An unprimed first launch is recorded
separately; repeated launches are warm-cache observations, never OS cold boot.
"""
import argparse
import ctypes as c
from ctypes import wintypes as w
import json
import os
from pathlib import Path
import platform
import shutil
import statistics
import subprocess
import time
import winreg

k = c.WinDLL("kernel32", use_last_error=True)
u = c.WinDLL("user32", use_last_error=True)
ps = c.WinDLL("psapi", use_last_error=True)


class ProcessEntry(c.Structure):
    _fields_ = [("dwSize", w.DWORD), ("usage", w.DWORD), ("pid", w.DWORD), ("heap", c.c_size_t),
                ("module", w.DWORD), ("threads", w.DWORD), ("parent", w.DWORD), ("priority", w.LONG),
                ("flags", w.DWORD), ("name", w.WCHAR * 260)]


class Memory(c.Structure):
    _fields_ = [("cb", w.DWORD), ("faults", w.DWORD), ("peakWorkingSet", c.c_size_t),
                ("workingSet", c.c_size_t), ("peakPaged", c.c_size_t), ("paged", c.c_size_t),
                ("peakNonPaged", c.c_size_t), ("nonPaged", c.c_size_t), ("pagefile", c.c_size_t),
                ("peakPagefile", c.c_size_t), ("private", c.c_size_t)]


k.CreateToolhelp32Snapshot.restype = w.HANDLE
k.Process32FirstW.argtypes = [w.HANDLE, c.POINTER(ProcessEntry)]
k.Process32NextW.argtypes = [w.HANDLE, c.POINTER(ProcessEntry)]
k.CloseHandle.argtypes = [w.HANDLE]
k.GetProcessTimes.argtypes = [w.HANDLE, *([c.POINTER(w.FILETIME)] * 4)]
ps.GetProcessMemoryInfo.argtypes = [w.HANDLE, c.POINTER(Memory), w.DWORD]
u.GetWindowThreadProcessId.argtypes = [w.HWND, c.POINTER(w.DWORD)]
u.IsWindowVisible.argtypes = [w.HWND]
u.PostMessageW.argtypes = [w.HWND, w.UINT, w.WPARAM, w.LPARAM]
callback_type = c.WINFUNCTYPE(w.BOOL, w.HWND, w.LPARAM)
u.EnumWindows.argtypes = [callback_type, w.LPARAM]


def processes():
    result = {}
    snapshot = k.CreateToolhelp32Snapshot(2, 0)
    entry = ProcessEntry(); entry.dwSize = c.sizeof(entry)
    if k.Process32FirstW(snapshot, c.byref(entry)):
        while True:
            result[entry.pid] = (entry.parent, entry.name)
            if not k.Process32NextW(snapshot, c.byref(entry)): break
    k.CloseHandle(snapshot)
    return result


def windows(pid):
    result = []
    @callback_type
    def callback(hwnd, _):
        owner = w.DWORD(); u.GetWindowThreadProcessId(hwnd, c.byref(owner))
        if owner.value == pid and u.IsWindowVisible(hwnd): result.append(hwnd)
        return True
    u.EnumWindows(callback, 0)
    return result


def cpu_seconds(handle):
    values = [w.FILETIME() for _ in range(4)]
    assert k.GetProcessTimes(handle, *(c.byref(v) for v in values))
    return sum((v.dwHighDateTime << 32) + v.dwLowDateTime for v in values[2:]) / 10000000


def clean_environment(out):
    env = os.environ.copy()
    for name in list(env):
        if name.upper() in {"PATH", "QT_PLUGIN_PATH", "QT_QPA_PLATFORM_PLUGIN_PATH", "QML_IMPORT_PATH", "QML2_IMPORT_PATH",
                            "QT_QUICK_BACKEND", "QSG_RHI_BACKEND", "PYTHONHOME", "PYTHONPATH", "RELAXER_DATA"}:
            env.pop(name)
    system = Path(os.environ.get("SystemRoot", r"C:\Windows"))
    env["PATH"] = str(system / "System32") + ";" + str(system)
    env["QT_QPA_PLATFORM"] = "windows"
    env["RELAXER_SKIP_AUDIO_DEVICE_TESTS"] = "1"
    for name in ("TEMP", "TMP", "APPDATA", "LOCALAPPDATA"):
        directory = out / name; directory.mkdir(exist_ok=True)
        env[name] = str(directory)
    return env


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--samples", type=int, default=5)
    args = parser.parse_args()
    out = args.output.resolve(); out.mkdir(parents=True, exist_ok=False)
    runtime = args.package.resolve() / "runtime"
    env = clean_environment(out)
    startup = subprocess.STARTUPINFO(); startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW; startup.wShowWindow = 0
    k.SetErrorMode(0x8003)
    initial = out / "unprimed.json"
    seed = out / "seed"
    # Establish one mature empty fixture, then copy it identically for each UI.
    r = subprocess.run([str(runtime / "Relaxer.exe"), "--data", str(seed), "--diagnostics", str(initial),
                        "--quit-after", "200"], env=env, cwd=runtime, capture_output=True, timeout=30, startupinfo=startup)
    assert r.returncode == 0, (r.returncode, r.stderr)
    results = []
    for sample in range(args.samples):
        for mode in ("widgets", "qml") if sample % 2 == 0 else ("qml", "widgets"):
            directory = out / f"{sample}-{mode}"; directory.mkdir()
            data = directory / "data"; shutil.copytree(seed, data)
            report = directory / "ui.json"
            with (directory / "stdout.txt").open("wb") as stdout, (directory / "stderr.txt").open("wb") as stderr:
                start = time.perf_counter()
                process = subprocess.Popen([str(runtime / "Relaxer.exe"), "--data", str(data), "--ui=" + mode,
                                            "--diagnostics", str(report)], env=env, cwd=runtime,
                                           stdout=stdout, stderr=stderr, startupinfo=startup)
                owned = {process.pid}; visible = None; samples = []; cpu_start = None
                deadline = start + 30
                while time.perf_counter() < deadline:
                    assert process.poll() is None, (mode, process.returncode)
                    table = processes()
                    for pid, (parent, _) in table.items():
                        if parent in owned: owned.add(pid)
                    hwnds = windows(process.pid)
                    now = time.perf_counter()
                    if hwnds and visible is None: visible = now
                    if visible is not None:
                        memory = Memory(); memory.cb = c.sizeof(memory)
                        assert ps.GetProcessMemoryInfo(int(process._handle), c.byref(memory), memory.cb)
                        if now - visible >= 1.5:
                            samples.append([memory.workingSet, memory.private])
                            if cpu_start is None: cpu_start = (now, cpu_seconds(int(process._handle)))
                        if now - visible >= 3.5: break
                    time.sleep(.025)
                assert visible is not None and cpu_start is not None, "Window did not appear"
                idle_cpu = (cpu_seconds(int(process._handle)) - cpu_start[1]) / (time.perf_counter() - cpu_start[0]) * 100
                closing = time.perf_counter()
                for hwnd in windows(process.pid): assert u.PostMessageW(hwnd, 0x10, 0, 0)
                assert process.wait(timeout=20) == 0
                closed = time.perf_counter()
                table = processes()
                remaining = sorted(owned & table.keys())
                assert not remaining, remaining
            ui = json.loads(report.read_text(encoding="utf-8"))
            assert ui["databaseConnections"] == 1 and ui["playbackInstances"] == 1 and ui["saveSucceeded"]
            results.append({"mode": mode, "sample": sample, "nativeVisibleMs": (visible - start) * 1000,
                            "normalCloseMs": (closed - closing) * 1000, "idleCpuOneCorePercent": idle_cpu,
                            "idleWorkingSetMiB": statistics.median(v[0] for v in samples) / 1048576,
                            "idlePrivateMiB": statistics.median(v[1] for v in samples) / 1048576,
                            "remainingOwned": remaining, "ui": ui})
            print(json.dumps(results[-1]), flush=True)
    cpu = "unknown"
    with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, r"HARDWARE\DESCRIPTION\System\CentralProcessor\0") as key:
        cpu = winreg.QueryValueEx(key, "ProcessorNameString")[0]
    summary = {"windows": platform.platform(), "cpu": cpu, "logicalProcessors": os.cpu_count(),
               "method": "Native Windows QPA, default Qt Quick rendering, system-only PATH; WM_CLOSE; identical mature empty SQLite copies; includes Library/Playback initialization; no physical audio output; 25ms OS visibility polling; 2s idle CPU window after 1.5s settling",
               "cacheState": "OS caches not flushed. Unprimed first Widgets launch separate; samples are warm-cache repeated launches with alternating order. uiLoadedMs is process clock; nativeVisibleMs includes CreateProcess/DLL load.",
               "limitations": "Widgets initializes all legacy settings/import/AI UI; QML initializes only C1 Library/Playback and bridges. Different feature completeness: these numbers do not predict final migrated UI performance. Widgets Paint and QML frameSwapped are different first-frame boundaries.",
               "unprimedWidgets": json.loads(initial.read_text(encoding="utf-8")), "results": results}
    (out / "performance.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding="utf-8")


if __name__ == "__main__": main()
