"""Read-only audit of the local Phase A source repository (Python stdlib only)."""
import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
parser.add_argument("--phase-a", action="store_true", help="also require unchanged accepted product code and no remotes")
args = parser.parse_args()
root = args.root.resolve()

def git(*arguments, check=True):
    result = subprocess.run(["git", "-C", str(root), *arguments], capture_output=True)
    if check and result.returncode:
        raise RuntimeError(result.stderr.decode("utf-8", errors="replace"))
    return result

tracked = git("ls-files", "-z").stdout.decode("utf-8").split("\0")
tracked = [name for name in tracked if name]
for name in tracked:
    path = Path(name)
    assert path.parts[0] not in {"data", "runtime", "runtime.pending", "test", "测试记录", "预览"}, name
    assert not any(part in {"browser-translation", "Profile", "Music", "Artwork", "Lyrics"} for part in path.parts), name
    assert path.suffix.lower() not in {".sqlite", ".db", ".log", ".mp3", ".flac", ".wav", ".lrc", ".zip", ".bundle"}, name
    assert not re.search(r"\.(?:sqlite3?|db)(?:-.+)?$", name, re.IGNORECASE), name
    if path.suffix.lower() == ".exe":
        assert name in {"RelaxerStart.exe", "RelaxerLockCheck.exe"}, name

ignored = ["data/Music/private.mp3", "data/relaxer.sqlite", "data/settings.json",
           "runtime/Relaxer.exe", "runtime.pending/Qt6Core.dll", "build-phase-b/CMakeCache.txt",
           "browser-translation/profile/Default/Cookies", "scratch/browser-translation/TEMP/cache",
           "scratch/relaxer.sqlite-wal", "test/run.log", "startup.log", "build-cache-path.txt", "msys2-path.txt"]
for name in ignored:
    assert git("check-ignore", "--no-index", "-q", "--", name, check=False).returncode == 0, name
for name in ["RelaxerStart.exe", "RelaxerLockCheck.exe", "CMakeLists.txt", "src/main.cpp",
             "resources/icon.png", "third_party/ffmpeg-7.1.tar.xz", "tests/tests.cpp"]:
    assert git("check-ignore", "--no-index", "-q", "--", name, check=False).returncode == 1, name
    assert name in tracked, name

manifest_count = 0
for line in (root / "SHA256SUMS.txt").read_text(encoding="utf-8").splitlines():
    expected, name = line.split("  ", 1)
    path = (root / name).resolve()
    assert path.is_relative_to(root) and path.is_file(), name
    with path.open("rb") as stream:
        assert hashlib.file_digest(stream, "sha256").hexdigest() == expected, name
    manifest_count += 1

if args.phase_a:
    protected = ["src", "resources", "CMakeLists.txt", "更新日志.txt", "RelaxerStart.exe", "RelaxerLockCheck.exe",
                 "scripts/build-windows-ucrt64.sh", "scripts/verify-runtime.ps1"]
    assert not git("diff", "baseline/windows-accepted-20261015", "--", *protected).stdout, "Product changes outside Phase A"
    assert not git("remote").stdout.strip(), "Phase A requires a local-only repository"

print(json.dumps({"tracked_files": len(tracked), "manifest_entries_verified": manifest_count,
                  "ignore_probes": len(ignored), "phase_a_product_unchanged": args.phase_a,
                  "head": git("rev-parse", "HEAD").stdout.decode().strip(),
                  "git_dir": git("rev-parse", "--absolute-git-dir").stdout.decode().strip()}, ensure_ascii=False, indent=2))
