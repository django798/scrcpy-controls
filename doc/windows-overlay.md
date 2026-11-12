<!-- Copyright (C) 2026 moasm -->
<!-- SPDX-License-Identifier: Apache-2.0 -->
# Windows client overlay

**English** | [简体中文](windows-overlay.zh-CN.md)

Maintainer: moasm <moasm@sina.com>.
Project: https://github.com/django798/scrcpy-controls

1. Download and extract `scrcpy-win64-v5.0.1.zip` from the
   [official v5.0.1 release](https://github.com/Genymobile/scrcpy/releases/tag/v5.0.1).
2. Download `scrcpy-controls-v5.0.1-controls.2-windows-x64-client.zip` from
   [this fork's releases](https://github.com/django798/scrcpy-controls/releases)
   and extract it into the same directory, replacing `scrcpy.exe`.
3. Enable USB debugging and authorize the computer on Android. Run
   `run-controls.bat`; each online authorized device opens in its own window.
   Use `run-controls.bat -s DEVICE_SERIAL` to select one device.

This overlay contains the client and documents. Keep the ADB, runtime DLLs and
Android server from the matching official **v5.0.1 Windows x64** package.
Keep the licenses and notice files. Extract the official package again to
restore the original client.

The project repository provides the source, feature guide and Linux/macOS build
instructions. Its English README is `README.md`; the Chinese README is
`README.zh-CN.md`. The overlay includes these installation instructions under
the same two README filenames.
