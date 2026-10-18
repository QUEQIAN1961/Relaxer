#!/usr/bin/env python3
"""Native system-PATH optional-QML matrix on new, disposable portable copies.

Preserves every executed copy as evidence. Never touches user installations.
"""
import argparse
import ctypes
import hashlib
import json
from pathlib import Path
import runpy
import shutil
import sqlite3
import subprocess
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    package, out = args.package.resolve(), args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    support = runpy.run_path(str(Path(__file__).with_name("measure-phase-c.py")))
    env = support["clean_environment"](out)
    env.pop("RELAXER_DATA", None)
    support["k"].SetErrorMode(0x8003)
    startup = subprocess.STARTUPINFO(); startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW; startup.wShowWindow = 5
    cmd = Path(env.get("SYSTEMROOT", env.get("SystemRoot", r"C:\Windows"))) / "System32/cmd.exe"
    results = []
    digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()

    def hashes(root):
        return {p.relative_to(root).as_posix(): digest(p) for p in root.rglob("*") if p.is_file()}

    def run(command, tag, expected=0):
        result = subprocess.run(command if isinstance(command, str) else list(map(str, command)), cwd=out, env=env, capture_output=True,
                                timeout=40, startupinfo=startup)
        (out / (tag + ".stdout")).write_bytes(result.stdout)
        (out / (tag + ".stderr")).write_bytes(result.stderr)
        assert result.returncode == expected, (tag, result.returncode, result.stderr)
        return result

    def visible_close(exe, data, tag):
        report = out / (tag + ".json")
        with (out / (tag + ".stdout")).open("wb") as stdout, (out / (tag + ".stderr")).open("wb") as stderr:
            process = subprocess.Popen([str(exe), "--data", str(data), "--diagnostics", str(report)],
                                       cwd=out, env=env, stdout=stdout, stderr=stderr, startupinfo=startup)
            owned = {process.pid}; deadline = time.monotonic() + 25
            while time.monotonic() < deadline:
                assert process.poll() is None, (tag, process.returncode)
                table = support["processes"]()
                for pid, (parent, _) in table.items():
                    if parent in owned: owned.add(pid)
                hwnds = support["windows"](process.pid)
                if hwnds and report.exists(): break
                time.sleep(.025)
            else:
                process.kill(); process.wait(); raise AssertionError("No visible Widgets window: " + tag)
            # A second UI using the same root must respect the host's existing lock.
            locked = run([exe, "--ui=widgets", "--data", data, "--diagnostics", out / (tag + "-locked.json")], tag + "-lock", 3)
            for hwnd in hwnds: assert support["u"].PostMessageW(hwnd, 0x10, 0, 0)
            assert process.wait(timeout=20) == 0
            assert not owned & support["processes"]().keys(), "Owned process survived close"
        proof = json.loads(report.read_text(encoding="utf-8"))
        assert proof["databaseConnections"] == 1 and proof["playbackInstances"] == 1 and proof["saveSucceeded"]
        assert not (data / "relaxer.lock").exists()
        return proof

    def snapshot(database):
        with sqlite3.connect(database) as db:
            return {"schema": db.execute("SELECT type,name,sql FROM sqlite_master WHERE sql IS NOT NULL ORDER BY type,name").fetchall(),
                    "tracks": db.execute("SELECT * FROM tracks ORDER BY id").fetchall(),
                    "sentinel": db.execute("SELECT value FROM settings WHERE key='fix1.sentinel'").fetchone()}

    quick = ["Qt6Qml.dll", "Qt6Quick.dll", "Qt6QuickControls2.dll"]
    cases = [("English", []), ("path with spaces", []), ("中文 路径", [])]
    cases += [("missing-" + name.removesuffix(".dll"), [name]) for name in quick]
    cases += [("中文 缺少全部 Quick", quick), ("missing-module", ["RelaxerQmlUi.dll"])]
    for index, (name, missing) in enumerate(cases):
        copy = out / name
        shutil.copytree(package, copy)
        runtime = copy / "runtime"; exe = runtime / "Relaxer.exe"
        for filename in missing: (runtime / filename).unlink()
        tag = f"case-{index}"
        data = out / (name + " data")
        proof = visible_close(exe, data, tag)
        for page in ("local", "settings"):
            image = out / (tag + "-" + page + ".png")
            run([exe, "--data", data, "--screenshot", image, "--page", page], tag + "-" + page)
            assert image.stat().st_size > 1024
        database = data / "relaxer.sqlite"
        with sqlite3.connect(database) as db:
            db.execute("INSERT INTO tracks(hash,file,title,artist,duration,cover_origin) VALUES(?,?,?,?,?,?)",
                       ("fix1-fixture", "Music/isolated-fixture.wav", "Fix1 identity fixture", "Synthetic", 1000, "none"))
            db.execute("INSERT INTO settings(key,value) VALUES('fix1.sentinel','[\"preserved\"]')")
        run([exe, "--data", data, "--screenshot", out / (tag + "-seeded.png")], tag + "-seeded")
        before = snapshot(database)
        before_bytes = hashes(data)
        if missing:
            response = run([exe, "--ui=qml", "--data", data], tag + "-qml-missing", 11)
            assert b"Optional QML module" in response.stderr
            assert hashes(data) == before_bytes, "Failed optional load modified existing user files"
            absent = out / (tag + "-must-not-create")
            run([exe, "--ui=qml", "--data", absent], tag + "-absent", 11)
            assert not absent.exists()
            # cmd must forward both arguments and stable nonzero failure status.
            command = f'"{copy / "RelaxerQml.cmd"}" --data "{absent}"'
            run(f'{cmd} /d /s /c "{command}"', tag + "-cmd", 11)
            assert not absent.exists()
        else:
            image = out / (tag + "-qml.png")
            report = out / (tag + "-qml.json")
            run([exe, "--ui=qml", "--data", data, "--screenshot", image, "--diagnostics", report], tag + "-qml")
            qml = json.loads(report.read_text(encoding="utf-8"))
            assert qml["databaseConnections"] == 1 and qml["playbackInstances"] == 1 and qml["saveSucceeded"]
            assert Path(qml["dataRoot"]) == data and image.stat().st_size > 1024
            # Also exercise wrapper forwarding of Unicode/spaced output and exit 0.
            cmd_image = out / (tag + "-cmd qml.png")
            command = f'"{copy / "RelaxerQml.cmd"}" --data "{data}" --screenshot "{cmd_image}"'
            run(f'{cmd} /d /s /c "{command}"', tag + "-cmd")
            assert cmd_image.stat().st_size > 1024
            assert snapshot(database) == before
        assert not (data / "relaxer.lock").exists()
        assert list(data.rglob("*.sqlite")) == [database]
        row = {"path": str(copy), "missing": missing, "widgetsVisible": True, "localAndSettings": True,
               "lockExit": 3, "qmlExit": 11 if missing else 0, "cmdExit": 11 if missing else 0,
               "existingFilesUnchangedOnLoadFailure": bool(missing), "dataIdentityPreserved": True,
               "remainingOwned": [], "widgets": proof}
        results.append(row)
        (out / "results.json").write_text(json.dumps(results, ensure_ascii=False, indent=2), encoding="utf-8")
        print(json.dumps({"case": index, "missing": missing, "passed": True}), flush=True)


if __name__ == "__main__": main()
