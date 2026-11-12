<!-- Copyright (C) 2026 moasm -->
<!-- SPDX-License-Identifier: Apache-2.0 -->
# Licenses and distribution

scrcpy and the scrcpy-controls source modifications are distributed under
[Apache-2.0](LICENSE). See [NOTICE](NOTICE) for attribution and
[MODIFICATIONS.md](MODIFICATIONS.md) for the changes. Upstream notices in the
source tree are retained. The name scrcpy identifies the upstream project;
this fork is independent and unofficial.

## Windows client overlay

The release ZIP contains only the modified `scrcpy.exe`, `run-controls.bat` and
documentation/license files. It does **not** redistribute ADB, FFmpeg, SDL3,
libusb, their other dependencies, or the Android server. It requires the
[official scrcpy v5.0.1 Windows x64 package](https://github.com/Genymobile/scrcpy/releases/tag/v5.0.1).
Those separately obtained components retain their own terms; Apache-2.0 does
not replace their licenses.

The client dynamically links to the following libraries in that official
package. The DLLs can be replaced with compatible builds, including modified
builds. This project adds no restriction on modification or reverse engineering
for debugging such library modifications.

| Component | License | Source and license |
| --- | --- | --- |
| FFmpeg 9.0.2 (`avcodec`, `avformat`, `avutil`, `swresample`) | LGPL-2.1-or-later, as reported by the supplied DLLs | [Source](https://ffmpeg.org/releases/ffmpeg-9.0.2.tar.xz), [license](licenses/LGPL-2.1.txt), [upstream build recipe](https://github.com/Genymobile/scrcpy/blob/v5.0.1/app/deps/ffmpeg.sh) |
| SDL3 3.4.18 | Zlib | [Source](https://github.com/libsdl-org/SDL/tree/release-3.4.18), [license](licenses/SDL3-Zlib.txt) |
| libusb 1.0.30 | LGPL-2.1-or-later | [Source](https://github.com/libusb/libusb/tree/v1.0.30), [license](licenses/LGPL-2.1.txt) |

Upstream dependency versions, source URLs and checksums are recorded under
[app/deps](https://github.com/Genymobile/scrcpy/tree/v5.0.1/app/deps). The upstream package also provides Android Platform Tools
and other runtime components; consult their respective notices and source
projects. They are not part of this overlay.

## Building locally

The Windows build helper downloads toolchains and an official runtime into
ignored local directories, verifies checksums, then assembles a complete folder
for local use. That folder is not the public release artifact. If distributing
a complete runtime bundle yourself, retain each component's notices and meet
its license and corresponding-source obligations. The client-only packager
uses an explicit file list to keep those downloaded components out of releases.

Linux and macOS source builds use libraries installed separately through the
platform package manager. Their licenses remain applicable independently.
