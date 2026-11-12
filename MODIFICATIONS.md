<!-- Copyright (C) 2026 moasm -->
<!-- SPDX-License-Identifier: Apache-2.0 -->
# Changes from upstream

Maintainer: moasm <moasm@sina.com>.
Base: upstream scrcpy v5.0.1, commit `a60891ae`.
The Android server and its protocol version remain unchanged.

## 5.0.1-controls.2

- Provide separate English and Simplified Chinese READMEs, with runtime
  screenshots and matching client-overlay installation guides.
- Keep a persistent `Used/Total GiB` legend for both storage and RAM, including
  while reading settings or reporting errors. Label storage as `Data(/data)`
  and document its distinction from the device's whole flash capacity.
- Preserve the existing used/total calculations: RAM used is
  `MemTotal - MemAvailable`, and Data uses the filesystem statistics for `/data`.

## 5.0.1-controls.1

- Add an integrated SDL3 right-side full PC keyboard and Android navigation,
  Power, volume and other common keys. Handle key holds, repeats, modifiers,
  focus changes, resizing and separation from the device touch area.
- Enable the panel by default; add `--control-panel` and `--no-control-panel`.
- Launch one process/window per online authorized device when no explicit
  device selection is provided; preserve single-device and query modes.
- Use exclusive Windows listening sockets and non-rebinding ADB forwards to
  prevent multi-device video/audio/control connections from colliding.
- Read current-device battery, optional battery capacity, storage, RAM and
  screen-timeout information asynchronously over ADB. Add timeout presets of
  1, 3, 5 and 30 minutes plus Always, with readback verification.
- Flush pending panel key releases for a bounded interval during shutdown;
  handle early timed-condition wakeups without treating them as the deadline.
- Add focused regression tests, Windows build/package helpers and native
  Linux/macOS CI. Identify the fork in the CLI banner and documentation.

The bottom controls occupy two rows within the existing panel height. Always
uses Android's maximum timeout and stay-awake while externally powered; vendor
power policies may still apply. See [the user guide](doc/control-panel.md).

Windows builds, automated tests and interaction with two real Android devices
were checked during development. Linux/macOS CI checks builds and automated
tests; those checks do not establish physical-device interaction on those OSes.
