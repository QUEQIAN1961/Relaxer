#!/usr/bin/env python3
"""Build real CMake/Ninja fixtures with backdated source/header/resource changes."""
import os
from pathlib import Path
import subprocess
import tempfile
import time


def main():
    module = Path(__file__).resolve().parents[1] / "scripts/source-fingerprint.cmake"
    passed = 0
    with tempfile.TemporaryDirectory(prefix="relaxer-source-cache-") as temporary:
        root = Path(temporary) / "project with spaces"
        build = Path(temporary) / "retained build"
        (root / "src").mkdir(parents=True)
        (root / "resources").mkdir()
        source = root / "src/main.cpp"
        header = root / "src/value.h"
        resource = root / "resources/data.txt"
        (root / "CMakeLists.txt").write_text('''cmake_minimum_required(VERSION 3.22)
project(CacheFixture LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_SUPPRESS_REGENERATION ON)
if(TRACK_CONTENT)
  include("''' + module.as_posix() + '''")
  relaxer_track_source_content("${CMAKE_CURRENT_SOURCE_DIR}")
endif()
add_executable(fixture src/main.cpp)
''')

        def code(prefix):
            return '''#include "value.h"
#include <iostream>
int main() {
    std::cout << "''' + prefix + '''" << value << ":";
#ifdef RELAXER_BUILD_SOURCE_SHA256
    std::cout << RELAXER_BUILD_SOURCE_SHA256;
#else
    std::cout << "legacy";
#endif
}
'''

        source.write_text(code("initial:"))
        header.write_text("inline constexpr int value = 1;\n")
        resource.write_text("original resource\n")
        old_time = time.time() - 86400

        def backdate(path):
            os.utime(path, (old_time, old_time))

        def run(args, expected=0):
            result = subprocess.run(args, capture_output=True, text=True, timeout=30)
            assert (result.returncode == 0) == (expected == 0), result.stdout + result.stderr
            return result.stdout + result.stderr

        def configure(tracked):
            return run(["cmake", "-S", str(root), "-B", str(build), "-G", "Ninja",
                        "-DTRACK_CONTENT=" + ("ON" if tracked else "OFF")])

        def compile(expected=0):
            return run(["cmake", "--build", str(build)], expected)

        def output():
            return run([str(build / ("fixture.exe" if os.name == "nt" else "fixture"))]).strip()

        def identity():
            return (build / "relaxer-source-fingerprint.txt").read_text().strip()

        def ok(name):
            nonlocal passed
            passed += 1
            print("PASS: " + name, flush=True)

        configure(False)
        compile()
        assert output() == "initial:1:legacy"
        source.write_text(code("changed:"))
        header.write_text("inline constexpr int value = 2;\n")
        backdate(source)
        backdate(header)
        configure(False)
        assert "no work to do" in compile() and output() == "initial:1:legacy"
        ok("legacy Ninja reuses stale executable after backdated source and header overwrite")

        configure(True)
        assert "Building CXX object" in compile() and output() == "changed:2:" + identity()
        ok("first fingerprint migration recompiles retained objects and runs current source")
        previous = identity()
        configure(True)
        assert identity() == previous and "no work to do" in compile()
        ok("unchanged content keeps compiled results without cleaning the cache")

        source.write_text(code("source-only:"))
        backdate(source)
        configure(True)
        assert identity() != previous and "Building CXX object" in compile()
        assert output() == "source-only:2:" + identity()
        previous = identity()
        ok("backdated C++ source change triggers a real recompile")

        header.write_text("inline constexpr int value = 4;\n")
        backdate(header)
        configure(True)
        assert identity() != previous and "Building CXX object" in compile()
        assert output() == "source-only:4:" + identity()
        previous = identity()
        ok("backdated included header change triggers a real recompile")

        resource.write_text("modified resource\n")
        backdate(resource)
        configure(True)
        assert identity() != previous and "Building CXX object" in compile()
        assert output() == "source-only:4:" + identity()
        previous = identity()
        ok("backdated resource change refreshes the compiled source identity")

        (root / "README.md").write_text("uncompiled notes\n")
        configure(True)
        assert identity() == previous and "no work to do" in compile()
        ok("uncompiled notes do not invalidate object files")

        source.write_text("deliberate compilation error\n")
        backdate(source)
        configure(True)
        assert "FAILED" in compile(1)
        configure(True)
        assert "FAILED" in compile(1)
        ok("failed compilation remains failed on retry instead of reusing the old executable")

        source.write_text(code("repaired:"))
        backdate(source)
        configure(True)
        compile()
        assert output() == "repaired:4:" + identity()
        ok("repaired source rebuilds successfully in the same retained cache")
        print(f"Totals: {passed} passed, 0 failed", flush=True)


if __name__ == "__main__":
    main()
