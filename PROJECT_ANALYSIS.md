# xiaozhi-esp32 项目分析

## 项目定位

这是一个 ESP-IDF C++ 固件工程，目标是把 ESP32 系列硬件做成“小智 AI 语音聊天设备”。设备端负责音频采集与播放、唤醒词、屏幕、灯效、按键、网络连接和设备控制；云端通常负责 ASR、LLM、TTS 等 AI 能力。

当前工程版本是 `2.2.6`，主入口工程名为 `xiaozhi`。项目使用 ESP-IDF Component Manager 管理依赖，`main/idf_component.yml` 中要求 ESP-IDF `>=5.5.2`。

## 核心架构

启动入口在 `main/main.cc`。启动流程很薄：初始化 NVS 后，进入 `Application::Initialize()` 和 `Application::Run()`。

`Application` 是核心单例，负责串联以下模块：

- 显示屏 UI 初始化和状态显示
- 音频服务初始化、启动和队列处理
- 网络事件处理
- 协议初始化与音频通道管理
- 唤醒词、VAD、播放中断、监听状态切换
- OTA 检查与升级
- MCP 工具注册和消息转发

主循环基于 FreeRTOS `EventGroup`，统一处理网络连接、网络断开、唤醒词、VAD、音频发送、状态变化、定时 tick 等事件。

## 状态机

设备状态定义在 `main/device_state.h`，主要状态包括：

- `starting`
- `wifi_configuring`
- `activating`
- `idle`
- `connecting`
- `listening`
- `speaking`
- `upgrading`
- `audio_testing`
- `fatal_error`

状态合法流转集中在 `main/device_state_machine.cc`。这对语音设备很重要，可以避免播放、监听、联网、升级等异步行为互相打架。

## 主要模块

源码集中在 `main` 目录：

- `audio/`：音频 codec、Opus 编解码、音频处理器、AFE、唤醒词、调试。
- `protocols/`：WebSocket 和 MQTT+UDP 两套通信协议，统一继承 `Protocol`。
- `boards/`：板级适配目录，包含大量不同 ESP32 系列开发板。
- `display/`：OLED、LCD、LVGL、表情显示。
- `led/`：单灯、灯带、GPIO 灯。
- `assets/`：语言、音效、字体、表情资源。
- `mcp_server.*`：设备端 MCP 工具系统。
- `ota.*`：固件升级。
- `settings.*`：配置持久化。
- `system_info.*`：设备信息和运行状态。

## 音频链路

`AudioService` 里有两条核心数据流：

1. 麦克风输入 -> 音频处理器 -> Opus 编码 -> 发送队列 -> 服务端
2. 服务端音频 -> 解码队列 -> Opus 解码 -> 播放队列 -> 喇叭输出

项目使用一个任务处理麦克风、喇叭和音频处理器，另一个任务处理 Opus 编码和解码。主循环只在发送队列可用时把 Opus 包交给协议层。

## 通信协议

协议层通过 `Protocol` 抽象统一接口，当前有两种实现：

- `WebsocketProtocol`
- `MqttProtocol`

音频包统一使用 `AudioStreamPacket`。协议层还负责发送开始监听、停止监听、打断播放、唤醒词、MCP 消息等控制消息。

## MCP 能力

`McpServer` 把设备能力封装成工具，云端可以通过 MCP/JSON-RPC 风格消息发现和调用这些工具。

适合通过 MCP 暴露的能力包括：

- 查询设备状态
- 设置音量
- 控制灯光
- 控制 GPIO
- 控制电机或舵机
- 获取摄像头图片
- 触发设备动作

## 板级适配

板级抽象在 `main/boards/common/board.h`。每块板需要实现或提供：

- `GetBoardType()`
- `GetAudioCodec()`
- `GetDisplay()`
- `GetLed()`
- `GetNetwork()`
- `StartNetwork()`
- `SetPowerSaveLevel()`
- `GetBoardJson()`
- `GetDeviceStatusJson()`

新增开发板通常需要：

1. 在 `main/boards/<board-name>` 新建目录。
2. 添加 `config.h`，定义 I2S、I2C、SPI、按键、屏幕、功放等引脚。
3. 添加 `<board>_board.cc`，继承 `WifiBoard`、`Ml307Board` 或其他现有基类。
4. 使用 `DECLARE_BOARD()` 注册板卡。
5. 在 `main/Kconfig.projbuild` 添加 `BOARD_TYPE` 配置。
6. 在 `main/CMakeLists.txt` 添加 `CONFIG_BOARD_TYPE_*` 到 `BOARD_TYPE` 的映射。
7. 如需批量构建，添加或维护板卡目录里的 `config.json`。

## 构建特点

项目根目录的 `CMakeLists.txt` 设置了：

- `MINIMAL_BUILD ON`
- `PROJECT_VER "2.2.6"`
- `project(xiaozhi)`

`main/CMakeLists.txt` 是构建复杂度最高的文件，负责：

- 收集公共源码
- 按 `CONFIG_BOARD_TYPE_*` 选择板卡源码
- 按芯片目标裁剪源码
- 按语言生成 `assets/lang_config.h`
- 嵌入语言音效资源
- 生成或烧录 assets 分区
- 查找 `esp-sr`、`xiaozhi-fonts` 等组件路径

默认配置在 `sdkconfig.defaults`，默认使用 v2 分区表 `partitions/v2/16m.csv`。

## 分区和 Flash 注意事项

项目同时保留了 `partitions/v1` 和 `partitions/v2`。v1 和 v2 分区表不兼容，不能简单依赖 OTA 从 v1 平滑升级到 v2。

默认配置使用 16MB Flash 分区表：

```text
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions/v2/16m.csv"
```

如果硬件实际只有 4MB 或 8MB Flash，必须切换到对应分区表，例如：

- `partitions/v2/4m.csv`
- `partitions/v2/8m.csv`
- `partitions/v2/16m.csv`

否则可能出现烧录失败、运行异常、资源分区不存在或 OTA 空间不足等问题。

## 主要风险点

- `main/CMakeLists.txt` 很大，板卡、资源、字体、芯片差异都集中在一个文件里，长期维护成本较高。
- 首次构建依赖 ESP-IDF 环境和网络下载组件。
- 默认 16MB 分区不一定适合所有 ESP32 开发板。
- v1/v2 分区表不兼容，量产或 OTA 前必须确认升级路径。
- 音频、网络和 UI 都是异步事件驱动，改动时要注意线程上下文。
- 多数跨线程操作应通过 `Application::Schedule()` 收敛到主任务执行。

## 当前工作区状态

本次分析未修改项目源码，仅新增此分析文档。

## ESP32-D0WD-V3 4MB CH340 适配方案

已通过 `COM6` 查询到当前开发板信息：

```text
Chip type: ESP32-D0WD-V3 revision v3.0
Detected flash size: 4MB
Flash voltage: 3.3V
```

CH340 只是 USB 转串口芯片，不决定 Flash 容量。当前这块板的实际 Flash 是 4MB，因此不能使用项目默认的 16MB 分区配置。

### 推荐目标

这块板建议按“低资源 ESP32 语音终端”方案使用，先做最小可用版本：

- 主控：ESP32-D0WD-V3，4MB Flash
- 串口：CH340，端口 `COM6`
- 网络：Wi-Fi
- 麦克风：I2S 数字麦克风，例如 INMP441
- 喇叭输出：I2S 数字功放，例如 MAX98357A
- 显示：可选 SSD1306 OLED 128x64 或 128x32
- 交互：按键触发监听，先不启用本地唤醒词
- AEC：关闭
- OTA：不建议启用，4MB 空间太紧
- 分区：使用 `partitions/v2/4m.csv`

项目里已有可复用板型：`main/boards/bread-compact-esp32`。该板型正好是 ESP32 + I2S 麦克风 + I2S 喇叭 + OLED 的轻量配置。

### 接线方案

接线以 `main/boards/bread-compact-esp32/config.h` 为准。

I2S 麦克风：

```text
MIC WS   -> GPIO25
MIC SCK  -> GPIO26
MIC SD   -> GPIO32
MIC VCC  -> 3.3V
MIC GND  -> GND
```

I2S 功放 MAX98357A：

```text
BCLK     -> GPIO14
LRCK     -> GPIO27
DIN      -> GPIO33
VIN      -> 5V 或 3.3V，按模块要求
GND      -> GND
SPK+/-   -> 喇叭
```

OLED SSD1306：

```text
SDA      -> GPIO4
SCL      -> GPIO15
VCC      -> 3.3V
GND      -> GND
```

按键：

```text
BOOT           -> GPIO0，板载已有
ASR 按钮       -> GPIO19，对 GND
按住说话按钮   -> GPIO5，对 GND
板载 LED       -> GPIO2
```

注意：`GPIO0`、`GPIO2`、`GPIO15` 都和 ESP32 启动模式相关。外接模块或按键不要在上电时把这些引脚强行拉到错误电平，否则可能导致无法正常启动或进入下载模式。

### 固件配置

关键配置如下：

```text
CONFIG_IDF_TARGET_ESP32=y
CONFIG_BOARD_TYPE_BREAD_COMPACT_ESP32=y
CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions/v2/4m.csv"
CONFIG_OLED_SSD1306_128X64=y
CONFIG_WAKE_WORD_DISABLED=y
CONFIG_USE_DEVICE_AEC=n
CONFIG_USE_SERVER_AEC=n
```

项目已有 `sdkconfig.defaults.esp32`，其中已经包含：

```text
CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions/v2/4m.csv"
```

因此构建 ESP32 目标时，应确保使用 ESP32 专用默认配置，不要沿用项目默认的 16MB 配置。

### 构建和烧录

在 ESP-IDF PowerShell 环境里执行：

```powershell
idf.py set-target esp32
idf.py menuconfig
```

菜单里选择：

```text
Xiaozhi Assistant
  -> Board Type
     -> Bread Compact ESP32 DevKit
```

OLED 选择：

```text
OLED SSD1306 128x64
```

构建、烧录、查看串口：

```powershell
idf.py build
idf.py -p COM6 flash monitor
```

### 功能边界

这块 4MB ESP32 可以运行项目的轻量语音终端形态，但不适合直接开启完整功能：

- 不建议启用本地唤醒词。
- 不建议启用设备端 AEC。
- 不建议使用复杂 LVGL 彩屏 UI。
- 不建议使用大体积表情资源。
- 不建议依赖 OTA，4MB 分区空间有限。
- 更适合“按钮触发语音对话 + OLED 简单显示 + 云端 AI”。

后续实施应优先跑通 `bread-compact-esp32`，确认音频输入、音频输出、Wi-Fi 配网、服务端连接和按钮监听流程正常，再考虑定制引脚或增加显示/灯光等外设。
