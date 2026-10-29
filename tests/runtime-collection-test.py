#!/usr/bin/env python3
"""Linux maintainer regression: real PE imports, simulated Qt deployment.

Requires Bash, CMake and x86_64-w64-mingw32-gcc. This is not a Windows
execution test. It does not add a Python dependency to Relaxer's startup.
Pass --original DIR with the old two deployment scripts to reproduce the bug.
"""

import argparse
import hashlib
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--original", type=Path)
    args = parser.parse_args()
    project_source = Path(__file__).resolve().parents[1]
    cc = shutil.which("x86_64-w64-mingw32-gcc")
    cmake = shutil.which("cmake")
    objdump = shutil.which("x86_64-w64-mingw32-objdump")
    if not all((cc, cmake, objdump)):
        parser.error("CMake and MinGW-w64 compiler/binutils are required")
    checks = 0

    def passed(label):
        nonlocal checks
        checks += 1
        print("PASS: " + label, flush=True)

    with tempfile.TemporaryDirectory(prefix="relaxer-pe-deploy-") as temporary:
        base = Path(temporary)
        project = base / "project with spaces"
        prefix = base / "ucrt64"
        plugins = prefix / "share/qt6/plugins"
        tool_bin = base / "tools"
        objects = base / "objects"
        build = base / "retained build"
        for folder in (project / "scripts", project / "resources", project / "licenses",
                       prefix / "bin", tool_bin, objects, build):
            folder.mkdir(parents=True, exist_ok=True)
        imports = {}

        def make_pe(path, name, dependencies=()):
            path.parent.mkdir(parents=True, exist_ok=True)
            symbol = re.sub(r"[^a-z0-9]", "_", name.lower()) + "_fn"
            source = objects / (name + ".c")
            declarations = "\n".join(
                "__declspec(dllimport) int " + imports[dep][1] + "(void);"
                for dep in dependencies)
            expression = " + ".join(imports[dep][1] + "()" for dep in dependencies) or "1"
            exported = "__declspec(dllexport) " if path.suffix == ".dll" else ""
            function = symbol if path.suffix == ".dll" else "entry"
            source.write_text(declarations + "\n" + exported + "int " + function +
                              "(void) { return " + expression + "; }\n", encoding="utf-8")
            command = [cc, "-nostdlib", "-O0", str(source), "-o", str(path)]
            if path.suffix == ".dll":
                library = objects / (name + ".a")
                command += ["-shared", "-Wl,--entry,0", "-Wl,--out-implib," + str(library)]
                imports[name] = (library, symbol)
            else:
                command += ["-Wl,--entry,entry", "-Wl,--subsystem,windows"]
            command += [str(imports[dep][0]) for dep in dependencies]
            subprocess.run(command, check=True, capture_output=True, text=True)

        for name in ("gcc-fixture.dll", "sqlite3.dll", "codec-aux.dll", "jpeg.dll",
                     "helper-only.dll"):
            make_pe(prefix / "bin" / name, name)
        make_pe(prefix / "bin/qt6core.dll", "qt6core.dll", ("gcc-fixture.dll",))
        make_pe(prefix / "bin/avcodec-fixture.dll", "avcodec-fixture.dll", ("codec-aux.dll",))
        for driver, client in (("qsqlibase.dll", "fbclient.dll"),
                               ("qsqlmysql.dll", "libmariadb.dll"),
                               ("qsqlpsql.dll", "libpq.dll")):
            # Clients are built only to produce import libraries. They are absent
            # from the runtime search paths, just as optional MSYS2 clients are.
            make_pe(base / "optional-clients" / client, client)
            make_pe(plugins / "sqldrivers" / driver, driver, (client,))
        make_pe(plugins / "sqldrivers/qsqlite.dll", "qsqlite.dll", ("sqlite3.dll",))
        make_pe(plugins / "sqldrivers/qsqlodbc.dll", "qsqlodbc.dll")
        make_pe(plugins / "multimedia/ffmpegmediaplugin.dll", "ffmpegmediaplugin.dll",
                ("avcodec-fixture.dll",))
        make_pe(plugins / "multimedia/windowsmediaplugin.dll", "windowsmediaplugin.dll")
        make_pe(plugins / "platforms/qwindows.dll", "qwindows.dll", ("qt6core.dll",))
        make_pe(plugins / "imageformats/qjpeg.dll", "qjpeg.dll", ("jpeg.dll",))
        make_pe(build / "Relaxer.exe", "Relaxer.exe", ("qt6core.dll",))
        shutil.copyfile(build / "Relaxer.exe", build / "RelaxerStart.exe")
        make_pe(prefix / "bin/ffmpeg.exe", "ffmpeg.exe",
                ("avcodec-fixture.dll", "helper-only.dll"))
        make_pe(prefix / "bin/ffprobe.exe", "ffprobe.exe", ("avcodec-fixture.dll",))
        shutil.copyfile(build / "Relaxer.exe", project / "RelaxerStart.exe")
        (project / "THIRD_PARTY_NOTICES.md").write_text("fixture", encoding="utf-8")
        (project / "resources/icon.ico").write_bytes(b"fixture")
        music = project / "data/Music/keep-original.wav"
        music.parent.mkdir(parents=True)
        music.write_bytes(b"user data remains untouched")
        retained_hash = hashlib.sha256((build / "Relaxer.exe").read_bytes()).digest()

        def tool(name, text):
            path = tool_bin / name
            path.write_text(text, encoding="utf-8")
            path.chmod(0o755)

        tool("cygpath", '#!/bin/sh\nfor value; do :; done\nprintf "%s\\n" "$value"\n')
        # Only the external Qt deployment tool is simulated. It deliberately
        # deploys every SQL driver; Bash selection and CMake PE scanning are real.
        tool("windeployqt", "#!/usr/bin/env python3\nimport os,sys,shutil\n"
             "from pathlib import Path\n"
             "stage=Path(sys.argv[-1]).parent\n"
             "prefix=Path(os.environ['FIXTURE_PREFIX'])\n"
             "shutil.copy2(prefix/'bin/qt6core.dll',stage/'qt6core.dll')\n"
             "for group in (prefix/'share/qt6/plugins').iterdir():\n"
             "    shutil.copytree(group,stage/group.name,dirs_exist_ok=True)\n")
        # The script requires a native Windows host in production. The fixture
        # changes this guard alone so CMake's PE backend can be exercised on Linux.
        tool("cmake", "#!/usr/bin/env python3\nimport sys,subprocess\nfrom pathlib import Path\n"
             "arguments=sys.argv[1:]\n"
             "index=arguments.index('-P')+1\n"
             "original=Path(arguments[index])\n"
             "wrapper=original.parent/'fixture-host.cmake'\n"
             "wrapper.write_text('set(CMAKE_HOST_WIN32 TRUE)\\ninclude([=['+str(original)+']=])\\n')\n"
             "arguments[index]=str(wrapper)\n"
             "sys.exit(subprocess.call([" + repr(cmake) + "]+arguments))\n")
        (tool_bin / "objdump").symlink_to(objdump)
        environment = os.environ.copy()
        environment.update(MSYSTEM="UCRT64", RELAXER_BUILD_DIR=str(build),
                           RELAXER_SOURCE_ROOT=str(project), FIXTURE_PREFIX=str(prefix),
                           PATH=str(tool_bin) + os.pathsep + environment["PATH"])

        def deploy(sources):
            script = (sources / "collect-runtime.sh").read_text(encoding="utf-8")
            # Adapt only the MSYS2 installation prefix to the isolated fixture.
            script = script.replace("/ucrt64", str(prefix))
            (project / "scripts/collect-runtime.sh").write_text(script, encoding="utf-8")
            shutil.copyfile(sources / "runtime-dependencies.cmake",
                            project / "scripts/runtime-dependencies.cmake")
            return subprocess.run(["bash", str(project / "scripts/collect-runtime.sh")],
                                  env=environment, capture_output=True, text=True, timeout=60)

        if args.original:
            result = deploy(args.original.resolve())
            output = result.stdout + result.stderr
            assert result.returncode != 0, output
            assert all(name in output for name in ("fbclient.dll", "libmariadb.dll", "libpq.dll")), output
            passed("original deployment reproduces all three missing database clients")

        current = project_source / "scripts"
        result = deploy(current)
        assert result.returncode == 0, result.stdout + result.stderr
        stage = project / "runtime.pending"
        assert sorted(p.name for p in (stage / "sqldrivers").iterdir()) == ["qsqlite.dll"]
        assert sorted(p.name for p in (stage / "multimedia").iterdir()) == ["ffmpegmediaplugin.dll"]
        for name in ("qt6core.dll", "gcc-fixture.dll", "sqlite3.dll", "avcodec-fixture.dll",
                     "codec-aux.dll", "jpeg.dll"):
            assert (stage / name).is_file(), name
        for name in ("helper-only.dll", "avcodec-fixture.dll", "codec-aux.dll"):
            assert (stage / "bin" / name).is_file(), name
        assert not (stage / "helper-only.dll").exists()
        passed("fixed deployment keeps SQLite, FFmpeg, image plugins and recursive DLL closures")
        assert (plugins / "sqldrivers/qsqlmysql.dll").exists()
        assert (plugins / "multimedia/windowsmediaplugin.dll").exists()
        assert hashlib.sha256((build / "Relaxer.exe").read_bytes()).digest() == retained_hash
        assert music.read_bytes() == b"user data remains untouched"
        passed("installed Qt plugins, retained build and existing music remain intact")

        launcher = build / "RelaxerStart.exe"
        launcher.rename(build / "RelaxerStart.saved")
        result = deploy(current)
        assert result.returncode != 0 and "RelaxerStart.exe is missing" in result.stderr, result.stderr
        assert (project / "RelaxerStart.exe").is_file()
        (build / "RelaxerStart.saved").rename(launcher)
        passed("missing current launcher cannot fall back to source bootstrap")

        shutil.copyfile(plugins / "sqldrivers/qsqlmysql.dll", stage / "sqldrivers/qsqlmysql.dll")
        result = deploy(current)
        assert result.returncode == 0, result.stdout + result.stderr
        assert not (stage / "sqldrivers/qsqlmysql.dll").exists()
        passed("retry replaces owned staging and reuses the retained executable")

        for name, label in (("sqlite3.dll", "required SQLite DLL"),
                            ("helper-only.dll", "helper-only runtime DLL")):
            required = prefix / "bin" / name
            backup = base / (name + ".saved")
            required.rename(backup)
            result = deploy(current)
            assert result.returncode != 0, name
            assert "Unresolved runtime dependencies" in result.stderr and name in result.stderr, result.stderr
            backup.rename(required)
            passed("missing " + label + " still stops deployment")

        required = plugins / "multimedia/ffmpegmediaplugin.dll"
        required.rename(base / "ffmpeg-plugin.saved")
        result = deploy(current)
        assert result.returncode != 0 and "ffmpegmediaplugin.dll" in result.stderr, result.stderr
        (base / "ffmpeg-plugin.saved").rename(required)
        passed("missing playback plugin still stops deployment")

        (stage / ".relaxer-staging-owner").unlink()
        sentinel = stage / "keep-unowned.txt"
        sentinel.write_text("unowned", encoding="utf-8")
        result = deploy(current)
        assert result.returncode != 0 and "ownership mismatch" in result.stderr, result.stderr
        assert sentinel.read_text() == "unowned"
        passed("unowned staging is preserved")

        source = (current / "runtime-dependencies.cmake").read_text(encoding="utf-8")
        expression = re.search(r"POST_EXCLUDE_REGEXES ([^\n]+)", source).group(1).removesuffix(")")
        regex_test = base / "system-filters.cmake"
        regex_test.write_text("set(filters " + expression + ")\n" + r'''
foreach(path IN ITEMS
    [=[C:\WINDOWS\system32/secur32.dll]=]
    [=[c:/windows/SYSTEM32/kernel32.dll]=]
    [=[C:\Windows\SysWOW64\user32.dll]=])
  set(excluded FALSE)
  foreach(pattern IN LISTS filters)
    if(path MATCHES "${pattern}")
      set(excluded TRUE)
    endif()
  endforeach()
  if(NOT excluded)
    message(FATAL_ERROR "System DLL was not excluded: ${path}")
  endif()
endforeach()
foreach(path IN ITEMS [=[C:/msys64/ucrt64/bin/qt6core.dll]=]
    [=[D:\Music\System32-copy\custom.dll]=])
  foreach(pattern IN LISTS filters)
    if(path MATCHES "${pattern}")
      message(FATAL_ERROR "Application DLL was incorrectly excluded: ${path}")
    endif()
  endforeach()
endforeach()
''', encoding="utf-8")
        subprocess.run([cmake, "-P", str(regex_test)], check=True)
        passed("system DLL filters handle mixed separators without excluding application libraries")
    print("Runtime collection regression: " + str(checks) + " checks passed.", flush=True)


if __name__ == "__main__":
    main()
