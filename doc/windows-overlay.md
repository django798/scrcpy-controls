<!-- Copyright (C) 2026 moasm -->
<!-- SPDX-License-Identifier: Apache-2.0 -->
# Windows 客户端覆盖包 / Client overlay

作者 / Maintainer: moasm <moasm@sina.com>.
Project: https://github.com/django798/scrcpy-controls

1. 从 [Genymobile/scrcpy v5.0.1](https://github.com/Genymobile/scrcpy/releases/tag/v5.0.1)
   下载 `scrcpy-win64-v5.0.1.zip` 并解压。
2. 从 [scrcpy-controls Releases](https://github.com/django798/scrcpy-controls/releases)
   下载 `scrcpy-controls-v5.0.1-controls.1-windows-x64-client.zip`，解压到上一步
   的目录，覆盖 `scrcpy.exe`。可先备份原来的客户端。
3. 启用设备 USB 调试并授权 PC，双击 `run-controls.bat`。多台在线设备会同时打开。
   单台选择：`run-controls.bat -s DEVICE_SERIAL`。

本覆盖包仅提供客户端；ADB、DLL 和 Android 服务端必须来自上述匹配的官方包。
请保留许可证和通知文件。恢复上游客户端时，解压官方包覆盖即可。
源码、功能说明及 Linux/macOS 构建方式见项目仓库。

Download and extract the official **v5.0.1 Windows x64** package first, then
extract this overlay into the same directory, replacing `scrcpy.exe`. Keep the
original runtime DLLs, ADB and Android server. Run `run-controls.bat`; each online
authorized device opens in its own window. Use `-s SERIAL` for one device.
Extract the official package again to restore the original client.
