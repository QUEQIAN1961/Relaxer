#!/usr/bin/env python3
"""Exercise file-only QtTest output and failure propagation without Qt or Windows."""
import os
from pathlib import Path
import subprocess
import tempfile


def main():
    runner = Path(__file__).resolve().parents[1] / "scripts/run-qt-test.cmake"
    with tempfile.TemporaryDirectory(prefix="relaxer-qt-report-") as temporary:
        root = Path(temporary) / "path with spaces"
        root.mkdir()
        executable = root / "quiet test"
        executable.write_text('''#!/usr/bin/env python3
import os, pathlib, sys
assert sys.argv[1:3] == ["-v1", "-o"]
assert sys.argv[3].endswith(",txt")
if os.environ.get("MOCK_REPORT"):
    pathlib.Path(sys.argv[3][:-4]).write_text(os.environ["MOCK_REPORT"])
if os.environ.get("MOCK_STDERR"):
    print(os.environ["MOCK_STDERR"], file=sys.stderr)
sys.exit(int(os.environ.get("MOCK_EXIT", "0")))
''')
        executable.chmod(0o755)
        report = root / "reports" / "result with spaces.txt"
        passed = 0

        def run(*, text="", code="0", stderr="", program=executable):
            return subprocess.run(
                ["cmake", "-DRELAXER_TEST_EXECUTABLE=" + str(program),
                 "-DRELAXER_TEST_LOG=" + str(report), "-P", str(runner)],
                env=dict(os.environ, MOCK_REPORT=text, MOCK_EXIT=code, MOCK_STDERR=stderr),
                capture_output=True, text=True, timeout=10)

        def check(condition, name, result):
            nonlocal passed
            assert condition, name + "\n" + result.stdout + result.stderr
            passed += 1
            print("PASS: " + name)

        result = run(text="PASS: file-only report\n")
        check(result.returncode == 0 and "PASS: file-only report" in result.stderr
              and report.read_text() == "PASS: file-only report\n",
              "file-only successful output is forwarded; paths with spaces work", result)
        failure = "FAIL!  : settingsRow() mismatch\nActual: 10\nExpected: 20\nLoc: [test.cpp(42)]\n"
        result = run(text=failure, code="3")
        check(result.returncode != 0 and failure in result.stderr
              and "original process result=3" in result.stderr and report.read_text() == failure,
              "file-only failures, values and locations are forwarded with original exit", result)
        result = run(text="PASS: warning case\n", stderr="backend warning")
        check(result.returncode == 0 and "backend warning" in result.stderr,
              "stderr remains visible without changing successful test status", result)
        result = run(code="2")
        check(result.returncode != 0 and not report.exists()
              and "original process result=2" in result.stderr,
              "startup failure is preserved and stale report is removed", result)
        report.write_text("stale passing report")
        result = run(program=root / "missing executable")
        check(result.returncode != 0 and not report.exists()
              and "did not create its report" in result.stderr,
              "missing executable fails without displaying an older report", result)
        result = run()
        check(result.returncode != 0 and "success without the required report" in result.stderr,
              "success without diagnostics is rejected", result)
        print(f"Totals: {passed} passed, 0 failed")


if __name__ == "__main__":
    main()
