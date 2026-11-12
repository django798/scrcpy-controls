<!-- SPDX-License-Identifier: Apache-2.0 -->
<!-- Copyright (C) 2026 moasm -->
# 右侧按键面板

作者：moasm <moasm@sina.com>

基于上游 scrcpy v5.0.1，在投屏窗口右侧增加可点击的 Android 按键和完整
PC 键盘。保留上游 Apache-2.0 许可证和版权声明。

## Windows 本机运行

使用公开 Release 时，先按 [客户端覆盖包说明](windows-overlay.md)将覆盖包解压到
上游官方 v5.0.1 完整包的目录，再运行 `run-controls.bat`。

从源码编译完成后，双击仓库根目录的 `run-controls.bat`，或运行便携目录中的同名
文件：`dist/scrcpy-controls-windows-x64/run-controls.bat`。本机构建的便携目录
包含 ADB、运行库和匹配的 Android 服务端，不依赖开发环境；公开下载只提供
客户端覆盖包，第三方依赖由用户从上游获取。见 [许可与分发说明](../THIRD_PARTY_NOTICES.md)。

```powershell
.\run-controls.bat
.\run-controls.bat -s DEVICE_SERIAL --no-audio
```

设备须已启用 USB 调试并授权当前 PC。连接多个设备时，直接启动会为启动时
所有在线且已授权的设备分别打开一个窗口，每个窗口都有右侧键盘。
自动生成的窗口标题包含型号和 ADB 序列号；关闭或断开其中一台不影响其他窗口。
离线、未授权设备会跳过并在控制台提示。只投屏某一台时，仍可用 `-s` 选择；
显式 `-d`、`-e`、`--tcpip=地址` 和 `ANDROID_SERIAL` 保留原来的设备选择语义。
`--help`、`--version`、设备信息查询和 OTG 模式不触发自动多开。
多设备下录制到同一个文件、V4L2 输出或 `--kill-adb-on-close` 会冲突，
这些操作须指定 `-s`。

## 操作

- 顶部：Power、Back、Home、多任务、音量、Menu、Search、Wake、Sleep。
- 中部：上下左右、OK、Home/End、Page Up/Down、Insert/Delete、数字小键盘。
- 下部：Esc、F1–F12、Print Screen、Scroll Lock、Pause、字母、数字、符号、
  Tab、Backspace、Enter、Space，以及左右 Shift/Ctrl/Alt/Meta。
- 底部第一排：`Info` 刷新电量、`/data` 存储和内存的已用/总量、当前熄屏超时；
  窗口启动时自动读取一次，`G` 为 GiB。设备公开满充/设计容量时同时显示电池 mAh，
  未公开时不使用剩余电荷量估算额定容量。缺失数据显示 `--`。
- 底部第二排：`Sleep` 的 `1m / 3m / 5m / 30m / Always`。设置读回一致后才高亮，
  拒绝写入、被系统改回或读取超时会显示失败状态，可点 `Info` 重读。
  两排替换原底部提示文字，不增加面板高度；读取和设置在后台执行，键盘仍可用。
- 普通按键在按下鼠标时发送 `ACTION_DOWN`，松开时发送 `ACTION_UP`。按住普通
  按键 500 ms 后每 50 ms 重复；Power/Home/Back 等系统键只保持按下，不连续触发。
  `--no-key-repeat` 同样关闭面板的重复输入。
- 点击 Shift/Ctrl/Alt/Meta 锁定，再次点击释放，可以用鼠标输入 Ctrl+A 等组合。
  Caps/Num/Scroll Lock 分别切换锁定状态；Shift 切换符号键的显示。
- 松开鼠标时即使指针已离开面板，也会释放该按键。窗口失焦、隐藏或最小化时释放
  按住的键和锁定的修饰键；退出时最多等待 200 ms 将控制队列写入连接。
  该等待仅确认传输写入，不代表 Android 已处理输入。

面板默认显示，`--no-control-panel` 可隐藏，`--control-panel` 可显式开启。
无控制权限、无视频播放、摄像头镜像和 OTG 模式不显示面板。
`--window-width/height` 表示整个窗口尺寸；面板随窗口缩放。面板显示期间窗口
不锁定整体宽高比，视频仍按原来的显示模式渲染，触摸坐标只对应左侧画面。
使用 UHID/AOA 相对鼠标模式时，先用上游的鼠标捕获快捷键释放指针才能点击面板。

这些按键通过 scrcpy 现有控制连接注入 Android `KeyEvent`，不逐次启动
`adb shell input`，无需修改 Android 服务端。Meta 是 Android Meta 键，
Print Screen、Pause、F1–F12 等键的作用由设备系统和当前应用决定。
屏幕已关但系统仍运行时可用 Power/Wake 唤醒；设备完全关机、ADB 不在线时
不能代替硬件电源键。中文输入继续使用上游的 PC 输入法/剪贴板能力，
面板本身提供标准 PC 键位。

快捷设置只操作当前窗口的 ADB 序列号，修改 Android 系统设置并保留到退出后。
`Always` 设置最大超时 `2147483647 ms`（约 24.9 天），并启用供电时保持唤醒，
USB 持续供电时可常亮；选回分钟档会关闭供电保持唤醒。手动熄屏及设备管理、
厂商电源策略仍可生效。若启动时另外指定了上游 `--stay-awake` 或
`--screen-off-timeout`，上游的退出恢复机制仍会执行。

## Windows 从源码构建

需要 Python 3、PowerShell 和 Windows 自带的 `curl.exe`、`tar.exe`：

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\build-windows.ps1
powershell -ExecutionPolicy Bypass -File .\tools\build-windows.ps1 -Configuration release
```

脚本将工具和依赖放在被 Git 忽略的 `build-tools/`，不修改系统安装的 scrcpy。
下载的编译器、SDL 开发包、FFmpeg 源码、libusb 开发包和上游便携包均校验固定
SHA-256；Meson/Ninja 固定版本。默认 debug 构建运行全部测试后生成便携目录。
FFmpeg/libusb 导入库直接从已校验的上游运行库导出，保证与服务端及运行库版本匹配。

## Linux / macOS

面板使用 SDL3 渲染、事件和定时器，没有 Windows 专用 UI 代码。按上游
[构建说明](build.md)安装 FFmpeg、SDL3、libusb、Meson/Ninja，然后编译：

```sh
meson setup build-controls --buildtype=debug -Dcompile_server=false
meson compile -C build-controls
meson test -C build-controls --print-errorlogs
```

运行前下载上游 v5.0.1 的匹配服务端，校验 SHA-256 为
`764eb6f79811d5211fe9df341120882ba9994c7a61b897d7bf3fb662e53bc536`：

```sh
curl -fL https://github.com/Genymobile/scrcpy/releases/download/v5.0.1/scrcpy-server-v5.0.1 -o scrcpy-server
SCRCPY_SERVER_PATH="$PWD/scrcpy-server" SCRCPY_ICON_DIR="$PWD/app/data" \
    ./build-controls/app/scrcpy --control-panel
```

新增 `control-panel.yml` 在 Linux、macOS Intel 和 Apple Silicon 构建并运行
同一套测试。[查看 CI 结果](https://github.com/django798/scrcpy-controls/actions/workflows/control-panel.yml)。
设备侧交互仍需各平台实机确认。

## 本机验收

1. 左侧投屏可正常点击、滑动；点击右侧不会触发左侧触摸或双击缩放。
2. Back、Home、多任务、音量、方向键和 OK 在目标设备上响应。
3. 在可输入文本的界面测试字母、数字、符号、Shift/Caps、Ctrl+A、退格和回车。
4. 按住方向键或退格；跨出面板松开、切换到其他窗口、关闭窗口后不持续按键。
5. 改变窗口大小、最大化、全屏、旋转视频后，面板与触摸坐标仍正确。
6. 连接多台已授权设备，直接启动应同时打开多个窗口；关闭其中一个后其他窗口
   继续可用。加 `-s DEVICE_SERIAL` 时只打开指定设备。
7. 点 `Info` 查看设备数据；逐档设置超时并验证高亮和 `Off` 读回值，切回分钟档
   应取消 `Always`；其他设备的设置不应改变。

Windows 已完成双设备交互验证；Linux/macOS 的设备交互尚待实机验收。
