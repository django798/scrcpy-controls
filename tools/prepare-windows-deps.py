#!/usr/bin/env python3
# Copyright (C) 2026 moasm
# SPDX-License-Identifier: Apache-2.0
"""Prepare import libraries against the verified upstream Windows runtime."""
import re
import shutil
import subprocess
import sys
from pathlib import Path


def prepare(root: Path) -> None:
    work = root / "build-tools"
    prefix = work / "deps"
    include = prefix / "include"
    lib = prefix / "lib"
    pc = lib / "pkgconfig"
    for directory in (include, lib, pc):
        directory.mkdir(parents=True, exist_ok=True)

    sdl = work / "SDL3-3.4.18" / "x86_64-w64-mingw32"
    shutil.copytree(sdl / "include", include, dirs_exist_ok=True)
    shutil.copytree(sdl / "lib", lib, dirs_exist_ok=True)
    (include / "libusb-1.0").mkdir(exist_ok=True)
    shutil.copy(work / "libusb" / "include" / "libusb.h", include / "libusb-1.0")

    ffmpeg = work / "ffmpeg-9.0.2"
    for name in ("libavcodec", "libavformat", "libavutil", "libswresample"):
        target = include / name
        target.mkdir(exist_ok=True)
        for header in (ffmpeg / name).glob("*.h"):
            shutil.copy(header, target)
    (include / "libavutil" / "avconfig.h").write_text(
        "#ifndef AVUTIL_AVCONFIG_H\n#define AVUTIL_AVCONFIG_H\n"
        "#define AV_HAVE_BIGENDIAN 0\n#define AV_HAVE_FAST_UNALIGNED 1\n#endif\n",
        encoding="ascii",
    )

    runtime = work / "scrcpy-win64-v5.0.1"
    toolbin = work / "w64devkit" / "bin"
    modules = {"libavcodec": "avcodec-63", "libavformat": "avformat-63",
               "libavutil": "avutil-61", "libswresample": "swresample-7",
               "libusb-1.0": "libusb-1.0"}
    for name, dll in modules.items():
        subprocess.run([str(toolbin / "gendef.exe"), str(runtime / f"{dll}.dll")],
                       cwd=lib, check=True)
        link_name = name.removeprefix("lib")
        subprocess.run([str(toolbin / "dlltool.exe"), "-d", f"{dll}.def",
                        "-l", f"lib{link_name}.dll.a"], cwd=lib, check=True)
        if name == "libusb-1.0":
            version = "1.0.30"
        else:
            version_text = (ffmpeg / name / "version.h").read_text()
            major_header = ffmpeg / name / "version_major.h"
            if major_header.exists():
                version_text += major_header.read_text()
            macro = name.upper() + "_VERSION_"
            version = ".".join(re.search(r"#define\s+" + macro + field
                                        + r"\s+(\d+)", version_text)[1]
                               for field in ("MAJOR", "MINOR", "MICRO"))
        (pc / f"{name}.pc").write_text(
            "prefix=${pcfiledir}/../..\nlibdir=${prefix}/lib\n"
            "includedir=${prefix}/include\n\n"
            f"Name: {name}\nDescription: scrcpy Windows shared runtime\n"
            f"Version: {version}\nLibs: -L${{libdir}} -l{link_name}\n"
            "Cflags: -I${includedir}\n", encoding="ascii")


if __name__ == "__main__":
    prepare(Path(sys.argv[1]).resolve())
