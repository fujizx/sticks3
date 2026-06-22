# Stick S3 小玩意

这个仓库用来开发 M5Stack Stick S3 / ESP32-S3 小项目。

当前开发板包装信息：

- SoC: ESP32-S3-PICO-1-N8R8
- Flash: 8MB
- PSRAM: 8MB
- Wi-Fi: 2.4GHz
- Display: ST7789P3，135x240
- USB: Type-C，5V
- Audio Codec: ES8311
- MIC: MEMS microphone
- Speaker: AW8737 功放 + 8 欧 1W 2011 喇叭
- Battery: 250mAh

## 第一个 Demo：摇摇骰子

当前 `src/main.cpp` 是一个最小 demo：

- 屏幕显示骰子点数
- 按 BtnA 重新掷骰子
- 摇动设备也会重新掷骰子

## 先用 M5Burner 验证连接

M5Burner 适合先烧录官方/现成固件，用来确认 Windows 驱动、USB 线、COM 口和设备 USB 模式都正常。

当前电脑已经识别到设备：

- 串口：`COM3`
- 波特率：`115200`

操作步骤：

1. 用 USB-C 连接 Stick S3，并保持在 USB 模式。
2. 启动 `tools\M5Burner-v3-beta-win-x64\M5Burner.exe`。
3. 选择对应的 Stick S3 / M5Stick 设备分类。
4. 选择串口 `COM3`。
5. 波特率使用 `115200`。
6. 先烧录一个官方 demo，确认设备能正常刷机和启动。

说明：`tools/` 目录只放本机工具和下载包，已经加入 `.gitignore`，不会提交到 Git。

## 用 Trae CN 安装 PlatformIO

M5Burner 只能方便地烧录现成固件。要编译并烧录本仓库里的源码，需要 PlatformIO。

Trae CN 和 VS Code 类似，优先使用插件方式安装：

1. 用 Trae CN 打开项目目录：`D:\sticks3`。
2. 打开扩展/插件面板。
3. 搜索 `PlatformIO IDE`。
4. 安装扩展 `platformio.platformio-ide`。
5. 重启 Trae CN。
6. 打开 PlatformIO Home，等待它自动安装 PlatformIO Core 和依赖工具。

安装完成后，在 Trae CN 的新终端里检查：

```powershell
pio --version
```

如果在普通 PowerShell 里提示 `pio` 无法识别，不一定代表安装失败。先用 Trae CN 内置终端确认，因为插件可能只把 PlatformIO 放在 Trae/VS Code 的扩展环境里。

如果 Trae CN 插件市场找不到 PlatformIO，或者插件安装后仍然没有 `pio`，再用 Python 安装 PlatformIO Core：

```powershell
python -m pip install -U platformio
```

## 编译和烧录

装好 PlatformIO 后，在项目根目录 `D:\sticks3` 执行：

```powershell
pio run
```

烧录到设备：

```powershell
pio run -t upload
```

打开串口监视器：

```powershell
pio device monitor
```

当前 `platformio.ini` 已经固定：

- `upload_port = COM3`
- `monitor_port = COM3`
- `monitor_speed = 115200`
- Flash: 8MB
- PSRAM: OPI 8MB

## Git

本地仓库远程地址：

```powershell
https://github.com/fujizx/sticks3.git
```

不会提交的内容：

- `tools/`
- `.pio/`
- 常见编辑器缓存和临时文件

## 硬件配置备注

当前 `platformio.ini` 使用 `esp32-s3-devkitc-1` 作为保守的 ESP32-S3 PlatformIO board 配置，并通过 `M5Unified` 访问屏幕、按钮、扬声器和 IMU。

如果后续确认这个 Stick S3 有更准确的 PlatformIO board ID，可以把 `platformio.ini` 里的 `board` 改成对应型号。
