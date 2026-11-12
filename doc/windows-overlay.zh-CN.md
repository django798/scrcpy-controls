<!-- Copyright (C) 2026 moasm -->
<!-- SPDX-License-Identifier: Apache-2.0 -->
# Windows 客户端覆盖包

[English](windows-overlay.md) | **简体中文**

作者：moasm <moasm@sina.com>。
项目：https://github.com/django798/scrcpy-controls

1. 从 [上游官方 v5.0.1](https://github.com/Genymobile/scrcpy/releases/tag/v5.0.1)
   下载并解压 `scrcpy-win64-v5.0.1.zip`。
2. 从 [本项目版本页](https://github.com/django798/scrcpy-controls/releases)
   下载 `scrcpy-controls-v5.0.1-controls.2-windows-x64-client.zip`，
   解压到同一目录，覆盖 `scrcpy.exe`。可先备份原客户端。
3. 启用设备 USB 调试并授权电脑，运行 `run-controls.bat`；所有在线且已授权的
   设备会分别打开窗口。单台选择：`run-controls.bat -s DEVICE_SERIAL`。

本覆盖包只提供客户端和文档；ADB、运行库 DLL 和 Android 服务端须来自匹配的
官方 **v5.0.1 Windows x64** 完整包。请保留许可证和通知文件。
恢复上游客户端时，重新解压官方包覆盖即可。

源码、功能说明及 Linux/macOS 构建方式见项目仓库。仓库及覆盖包均提供两个
独立的 README：`README.md` 为英文，`README.zh-CN.md` 为简体中文。
