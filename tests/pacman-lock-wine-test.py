#!/usr/bin/env python3
"""Maintainer test: run the real Windows lock checker and Win32 tests in Wine.

Requires Linux, Wine 64 and MinGW-w64 C++. No real MSYS2 packages are modified.
The dependency script uses fake pacman commands; its lock checker is the shipped
Windows binary. This does not add Python or Wine to the player requirements.
"""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def windows(path):
    return "Z:" + str(path.resolve()).replace("/", "\\")


def key(project):
    value = 14695981039346656037
    for byte in project.encode("utf-8"):
        value = ((value ^ byte) * 1099511628211) & ((1 << 64) - 1)
    return f"hb-{value:016x}"


def main():
    source = Path(__file__).resolve().parents[1]
    wine = os.environ.get("RELAXER_TEST_WINE", "/usr/lib/wine/wine64")
    compiler = os.environ.get("RELAXER_TEST_CXX", "x86_64-w64-mingw32-g++")
    with tempfile.TemporaryDirectory(prefix="relaxer-native-lock-") as temporary:
        base = Path(temporary)
        env = dict(os.environ, WINEPREFIX=str(base / "wine"), WINEDEBUG="-all")
        binary = base / "native-tests.exe"
        subprocess.run([compiler, "-std=c++17", "-O2", "-Wall", "-Wextra", "-Wpedantic",
                        "-Werror", "-municode", "-static", "-I", str(source / "src"),
                        str(source / "tests/pacman-lock-windows-tests.cpp"), "-o", str(binary)],
                       check=True, timeout=90)
        subprocess.run([wine, str(binary)], env=env, check=True, timeout=90)

        root = base / "Relaxer 中文路径 &!%() with spaces"
        (root / "scripts").mkdir(parents=True)
        shutil.copyfile(source / "scripts/install-windows-dependencies.sh",
                        root / "scripts/install-windows-dependencies.sh")
        shutil.copyfile(source / "RelaxerLockCheck.exe", root / "NativeLockCheck.exe")
        project = windows(root)
        cache = base / "cache" / key(project)
        msys = cache / "msys64"
        database = msys / "var/lib/pacman"
        database.mkdir(parents=True)
        sentinel = database / "db.lck"
        (cache / ".relaxer-owned-cache").write_bytes(
            ("Relaxer-owned-cache-v1\n" + project + "\n").encode("utf-8"))
        (database / "packages.keep").write_bytes(b"installed database")
        (root / "data").mkdir()
        (root / "data/keep").write_bytes(b"user data")
        tools = base / "tools"
        tools.mkdir()
        calls = base / "calls"

        def tool(path, text):
            path.write_text(text, encoding="utf-8")
            path.chmod(0o755)

        tool(root / "RelaxerLockCheck.exe", """#!/usr/bin/env bash
exec "$RELAXER_TEST_WINE" "$(dirname -- "$0")/NativeLockCheck.exe" "$@"
""")
        tool(tools / "cygpath", """#!/usr/bin/env python3
import sys
print('Z:' + sys.argv[2].replace('/', '\\\\'))
""")
        tool(tools / "pacman-conf", "#!/usr/bin/env bash\nprintf '%s\\n' \"$MOCK_DATABASE\"\n")
        tool(tools / "pacman", """#!/usr/bin/env bash
printf 'pacman %s\\n' "$*" >> "$MOCK_CALLS"
case "$1" in
  -Q) exit 1 ;;
  -Qu|-Syu|-S) exit 0 ;;
esac
exit 3
""")
        env.update(RELAXER_SOURCE_ROOT=project, RELAXER_BUILD_CACHE=windows(cache),
                   RELAXER_MSYS2_DIR=windows(msys), RELAXER_TEST_WINE=str(Path(wine).resolve()),
                   PATH=str(tools) + os.pathsep + env["PATH"], MSYSTEM="UCRT64",
                   MOCK_DATABASE=str(database), MOCK_CALLS=str(calls))
        for phase in ("--update", "--install"):
            sentinel.write_bytes(b"")
            calls.write_bytes(b"")
            subprocess.run(["bash", str(root / "scripts/install-windows-dependencies.sh"), phase],
                           env=env, check=True, timeout=30)
            assert not sentinel.exists()
            assert "pacman -S" in calls.read_text()
            assert (database / "packages.keep").read_bytes() == b"installed database"
            assert (root / "data/keep").read_bytes() == b"user data"
            print("PASS: real Windows checker + Bash " + phase + " integration", flush=True)

        sentinel.write_bytes(b"keep this lock")
        calls.write_bytes(b"")
        bad_env = dict(env, RELAXER_MSYS2_DIR=windows(base / "external-msys"))
        result = subprocess.run(["bash", str(root / "scripts/install-windows-dependencies.sh"),
                                 "--update"], env=bad_env, timeout=30)
        assert result.returncode == 75 and sentinel.read_bytes() == b"keep this lock"
        assert calls.read_bytes() == b""
        print("PASS: real Windows checker rejects external environment before transaction", flush=True)
        result = subprocess.run([wine, str(root / "NativeLockCheck.exe"), "--unknown"],
                                env=env, timeout=30)
        assert result.returncode == 2 and sentinel.exists()
        print("PASS: native command-line guard preserves lock", flush=True)
        print("Wine API and Bash integration passed; not Windows/MSYS2 first-start evidence.", flush=True)


if __name__ == "__main__":
    main()
