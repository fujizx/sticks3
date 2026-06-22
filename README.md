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

## 当前 Demo：三项菜单

当前 `src/main.cpp` 已经有一个可操作菜单：

- 菜单第 1 项：时钟
- 菜单第 2 项：番茄钟
- 菜单第 3 项：骰子

按键规则：

- 在菜单里，按 BtnB 向下选择
- 在菜单里，按 BtnA 确认进入
- 在菜单里，摇动设备回到第 1 项
- 在应用里，按 BtnB 返回菜单
- 在骰子应用里，按 BtnA 或摇动设备重新掷骰子

番茄钟说明：

- 进入后先看到番茄钟菜单：`Start / Records`
- `Start` 进入时长选择，支持 `15 / 25 / 50` 分钟，默认 `25` 分钟
- `Records` 查看本地最近 10 条番茄钟记录
- 在时长选择页，按 BtnB 切换时长，按 BtnA 开始
- 开始后屏幕切到竖屏沙漏
- 蓝色液体按所选总时长计算流速，逐步从上半部分流到下半部分
- 沙漏运行时只局部重绘液体和时间，减少屏幕闪烁
- 进入竖屏沙漏后重新采集 IMU 重力基准，用于后续反转检测
- 摇动或倾斜设备时，蓝色液体会按相对重力方向产生倾斜、偏移和滴落变化
- 设备倒置 180 度时，会显示 `Reset / Exit` 菜单
- 倒置菜单里按 BtnB 上下选择，按 BtnA 确认
- 时间到后会轻声滴滴两下，并记录为完成
- 运行中退出或重置，会记录为未完成
- 记录先保存在本地 Flash/NVS，后续可以接 API 上传

时钟说明：

- Wi-Fi 和 NTP 正常时，显示网络校准后的真实时间
- 未联网或 NTP 未同步时，回退显示从设备启动开始累计的运行时间
- 当前时钟使用翻页钟样式，顶部保留日期，下方是尽量放大的 3 个数字卡片
- 时间变化时，对变化的卡片播放一个短翻页过渡
- 时钟每秒重画一次，主循环按约 50Hz 轮询按键、Wi-Fi、WebSocket 和电池状态

### 显示亮度和刷新策略

当前亮度在 `src/main.cpp` 里配置：

```cpp
constexpr uint8_t kDisplayBrightness = 160;
M5.Display.setBrightness(kDisplayBrightness);
```

亮度范围是 `0-255`。运行时也可以读取：

```cpp
M5.Display.getBrightness();
```

启动日志会输出当前显示配置：

```text
display brightness=160 ui_loop=50Hz clock_redraw=1Hz
```

这里的 `ui_loop=50Hz` 是程序主循环/交互轮询频率，`clock_redraw=1Hz` 是时钟页面重画频率。ST7789 面板自身的硬件扫描刷新率没有在当前 M5Unified 抽象里作为通用运行时数值暴露出来，所以项目里按“应用重画频率”来控制功耗和观感。

## 基础框架能力

项目已经拆出一组基础模块，放在 `src/core/`：

- `AppLog`：串口日志输出，支持 DEBUG / INFO / WARN / ERROR
- `AppConfig`：使用 ESP32 Preferences/NVS 保存应用配置到 Flash
- `WifiPortal`：首次启动自动进入手机配网，后续自动连接已保存 Wi-Fi
- `TimeSync`：联网后通过 NTP 自动校时
- `NetClient`：HTTP GET / POST JSON 和 WebSocket 客户端封装
- `BatteryMeter`：读取并显示电池百分比
- `PomodoroHistory`：番茄钟完成/未完成记录，本地保存最近 10 条

### Wi-Fi 首次手机配网

首次没有保存 Wi-Fi 时，设备会开启一个临时热点：

```text
StickS3-Setup
```

手机连接这个热点后，按页面提示选择家里的 Wi-Fi 并输入密码。配置成功后，ESP32 会把 Wi-Fi 信息保存到 Flash/NVS，后续开机自动连接。

如果 180 秒内没有完成配网，设备会继续以离线模式启动，骰子和菜单仍可用。

### 自动重连

联网后如果 Wi-Fi 断开，`WifiPortal::loop()` 会定期调用重连逻辑。主循环里已经接入：

```cpp
wifiPortal.loop();
```

### Flash 保存配置

应用配置通过 `AppConfig` 保存，当前默认项包括：

- 设备名：`StickS3`
- 时区：`CST-8`
- NTP：`pool.ntp.org`、`ntp.aliyun.com`
- HTTP base URL
- WebSocket host / port / path

Wi-Fi SSID 和密码由 ESP32 Wi-Fi/WiFiManager 机制保存到 NVS。

### NTP 自动校时

联网后 `TimeSync` 会自动等待 NTP 同步。默认时区是中国时间：

```text
CST-8
```

时钟页面优先显示 NTP 时间；如果还未同步，则显示离线运行时间。

### HTTP / WebSocket

HTTP 封装在 `HttpClient`：

```cpp
String response;
httpClient.get("/api/ping", response);
httpClient.postJson("/api/data", "{\"hello\":\"stick\"}", response);
```

WebSocket 封装在 `WsClient`：

```cpp
wsClient.onText([](const String &text) {
  LOGI("ws", "text=%s", text.c_str());
});
wsClient.sendText("hello");
```

默认没有配置远端地址，所以 WebSocket 不会主动连接。后面需要接服务端时，改 `AppConfig` 的默认配置或做一个设置页面即可。

### 电池显示

菜单、骰子、时钟页面顶部都会显示：

- Wi-Fi 状态：`WiFi` / `Offline`
- 电池百分比：`BAT xx%`

### 日志输出

串口波特率为 `115200`。打开串口监视器：

```powershell
pio device monitor
```

日志格式类似：

```text
[      1234] INFO  wifi       connected ssid=xxx ip=192.168.1.10
```

## 先用 M5Burner 验证连接

M5Burner 适合先烧录官方/现成固件，用来确认 Windows 驱动、USB 线、COM 口和设备 USB 模式都正常。

当前电脑已经识别到设备：

- 当前串口：`COM4`
- 波特率：`115200`

操作步骤：

1. 用 USB-C 连接 Stick S3，并保持在 USB 模式。
2. 启动 `tools\M5Burner-v3-beta-win-x64\M5Burner.exe`。
3. 选择对应的 Stick S3 / M5Stick 设备分类。
4. 选择串口 `COM4`。如果设备重新插拔后端口变化，以 Windows 设备管理器或 PlatformIO 显示的端口为准。
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

- `upload_port = COM4`
- `monitor_port = COM4`
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
