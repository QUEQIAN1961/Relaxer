#!/usr/bin/env python3
"""Dependency-script regression with isolated pacman/checker command fixtures.

No real packages are modified. Native file/process checks are tested separately
by pacman-lock-windows-tests.cpp. Pass --original SCRIPT to reproduce exit 75
for an orphan with the previous script. Python is only a maintainer dependency.
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--original", type=Path)
    args = parser.parse_args()
    source = Path(__file__).resolve().parents[1] / "scripts/install-windows-dependencies.sh"
    passed = 0

    def ok(label):
        nonlocal passed
        passed += 1
        print("PASS: " + label, flush=True)

    with tempfile.TemporaryDirectory(prefix="relaxer-lock-script-") as temporary:
        base = Path(temporary)
        root = base / "project with spaces &!%()"
        (root / "scripts").mkdir(parents=True)
        tools = base / "tools"
        tools.mkdir()
        database = base / "database"
        database.mkdir()
        sentinel = database / "db.lck"
        calls = base / "calls"
        script = root / "scripts/install-windows-dependencies.sh"
        shutil.copyfile(source, script)
        helper = root / "RelaxerLockCheck.exe"

        def executable(path, text):
            path.write_text(text, encoding="utf-8")
            path.chmod(0o755)

        executable(tools / "pacman", """#!/usr/bin/env bash
printf 'pacman %s\\n' "$*" >> "$MOCK_CALLS"
case "$1" in
  -Q) exit "${MOCK_QUERY:-0}" ;;
  -Qu) [[ "${MOCK_PENDING:-0}" == 0 ]] || echo 'pending-package 1 -> 2'; exit 0 ;;
  -Syu)
    [[ "${MOCK_REQUIRE_NATIVE_LOG:-0}" != 1 || "${RELAXER_PREP_CAPTURE:-0}" == 1 ]] || exit 92
    if [[ "${MOCK_REQUIRE_LOCALE:-0}" == 1 ]]; then
      [[ "${LC_ALL:-}" == C && "$*" == *"--color never"* ]] || exit 91
    fi
    exit "${MOCK_TRANSACTION:-0}" ;;
  -S) exit "${MOCK_TRANSACTION:-0}" ;;
esac
exit 3
""")
        executable(tools / "pacman-conf", "#!/usr/bin/env bash\nprintf '%s\\n' \"$MOCK_DATABASE\"\n")
        executable(tools / "cygpath", "#!/usr/bin/env bash\nprintf '%s\\n' \"$2\"\n")
        helper_text = """#!/usr/bin/env bash
printf 'checker %s\\n' "$*" >> "$MOCK_CALLS"
[[ "$1" == --recover-selected-pacman-lock && "$2" == "$MOCK_DATABASE/db.lck" ]] || exit 3
case "${MOCK_CHECKER:-recover}" in
  recover) rm -- "$2"; exit 0 ;;
  busy|unowned) exit 75 ;;
  error) exit 19 ;;
  lying) exit 0 ;;
esac
exit 3
"""
        executable(helper, helper_text)
        env = dict(os.environ, PATH=str(tools) + os.pathsep + os.environ["PATH"],
                   MSYSTEM="UCRT64", MOCK_DATABASE=str(database), MOCK_CALLS=str(calls))

        def run(mode, *, locked=True, **overrides):
            calls.write_text("", encoding="utf-8")
            if locked:
                sentinel.write_bytes(b"previous transaction sentinel\n")
            else:
                sentinel.unlink(missing_ok=True)
            result = subprocess.run(["bash", str(script), mode], env=dict(env, **overrides),
                                    capture_output=True, text=True, timeout=20)
            # Wait for the shell's tee process substitution too (capture_output).
            return result, calls.read_text(encoding="utf-8")

        result, log = run("--check")
        assert result.returncode == 0 and sentinel.exists()
        assert log.startswith("pacman -Q ") and "checker" not in log and "pacman -S" not in log
        assert not (root / "install-dependencies.log").exists()
        ok("--check remains read-only with a lock, no checker or transaction/log writes")
        result, _ = run("--check", MOCK_QUERY="1")
        assert result.returncode == 1 and sentinel.exists()
        ok("missing dependency still returns 1 without changing the lock")
        for mode in ("--update", "--install"):
            result, log = run(mode)
            assert result.returncode == 0 and not sentinel.exists(), result.stdout + result.stderr
            assert log.index("checker ") < log.index("pacman -S")
            assert "Database lock check passed." in result.stdout
            ok(mode + " continues after confirmed orphan recovery")
            for reason in ("busy", "unowned", "error", "lying"):
                result, log = run(mode, MOCK_CHECKER=reason)
                assert result.returncode == 75 and "pacman " not in log
                assert sentinel.read_bytes() == b"previous transaction sentinel\n"
                ok(mode + " blocks and preserves lock when checker is " + reason)
        helper.unlink()
        result, log = run("--update")
        assert result.returncode == 75 and sentinel.exists() and not log
        assert "缺少数据库锁检查组件" in result.stdout
        ok("missing checker fails safely with complete-package instruction")
        executable(helper, helper_text)
        result, log = run("--update", locked=False)
        assert result.returncode == 0 and "checker " not in log and "pacman -Syu" in log
        ok("normal unlocked update does not run checker")
        result, log = run("--update", locked=False, MOCK_REQUIRE_LOCALE="1")
        assert result.returncode == 0, result.stdout + result.stderr
        ok("update uses deterministic English output and disables color for restart detection")
        result, log = run("--install", locked=False, MOCK_PENDING="1")
        assert result.returncode == 75 and "pacman -Qu" in log and "pacman -S " not in log
        ok("pending full upgrades still block package installation")
        result, log = run("--update", locked=False, MOCK_TRANSACTION="37")
        assert result.returncode == 37
        ok("package update error code is retained")
        (root / "install-dependencies.log").unlink(missing_ok=True)
        result, log = run("--update", locked=False, RELAXER_PREP_CAPTURE="1", MOCK_REQUIRE_NATIVE_LOG="1")
        assert result.returncode == 0 and "pacman -Syu" in log
        assert not (root / "install-dependencies.log").exists()
        ok("native capture runs update without tee/log file; stdout is owned by launcher")
        result, log = run("--update", locked=False, RELAXER_PREP_CAPTURE="1", MOCK_TRANSACTION="37")
        assert result.returncode == 37 and not (root / "install-dependencies.log").exists()
        ok("exec update preserves a genuine failure for native controller")
        if args.original:
            shutil.copyfile(args.original, script)
            result, log = run("--update")
            assert result.returncode == 75 and sentinel.exists() and "checker " not in log
            ok("previous script reproduces exit 75 without attempting orphan recovery")
        print(f"{passed} isolated dependency-script scenarios passed.")


if __name__ == "__main__":
    main()
