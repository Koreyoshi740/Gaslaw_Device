# Gaslaw Device — ESP32 气体定律实验装置

![Platform](https://img.shields.io/badge/platform-ESP32-blue)
![Framework](https://img.shields.io/badge/framework-Arduino%20%7C%20PlatformIO-orange)
![License](https://img.shields.io/badge/license-MIT-green)
![Language](https://img.shields.io/badge/language-C%2B%2B20-lightgrey)

一套基于 ESP32 的嵌入式气体定律实验装置，支持玻意耳定律、查理定律、盖-吕萨克定律三个经典热力学实验。装置通过 PID 闭环控制气体的温度、压力与体积，配合 OLED 实时显示与 phyphox BLE 数据导出，适用于中学/大学物理教学与课题研究。

> **论文配套开源工程** — 本仓库为毕业设计/课题论文的硬件实现部分。

---

## 目录

- [功能特性](#功能特性)
- [系统架构](#系统架构)
- [硬件清单](#硬件清单)
- [引脚定义](#引脚定义)
- [软件依赖](#软件依赖)
- [快速上手](#快速上手)
- [实验说明](#实验说明)
- [phyphox 数据导出](#phyphox-数据导出)
- [工程结构](#工程结构)
- [许可证](#许可证)

---

## 功能特性

- **三大气体定律实验**：玻意耳定律（等温）、查理定律（等压）、盖-吕萨克定律（等容）
- **PID 闭环控制**：独立的体积控制、压力控制、温度控制三路 PID 回路
- **全自动实验流程**：状态机驱动，自动完成各步骤的调节、稳定判断与数据记录
- **OLED 实时显示**：128×64 SH1106 屏幕，PixelUI 框架实现多级菜单与曲线图表
- **phyphox BLE 数据流**：通过蓝牙实时推送温度/压力/体积至手机，支持导出 CSV
- **非阻塞按键输入**：支持单击、双击、长按、连发，4 键完整操控
- **安全保护机制**：80°C 超温断电、电磁阀常闭失效保护、实验超时自动终止

---

## 系统架构

```
┌─────────────────────────────────────────────┐
│                   UI 层                      │
│   AppBoyle / AppCharles / AppGayLussac       │
│   AppEnvironment / AppDebug  (PixelUI)       │
├─────────────────────────────────────────────┤
│                  实验层                      │
│   BoyleLaw / CharlesLaw / GayLussacLaw       │
│            (状态机 + 数据记录)               │
├─────────────────────────────────────────────┤
│                  控制层                      │
│   VolumeControl / PressureControl            │
│   TempControl        (PID 回路)              │
├─────────────────────────────────────────────┤
│                  硬件层                      │
│  AirPump / AirValve / Pumper / Heater        │
│  BME280sensor / Flowsensor                   │
└─────────────────────────────────────────────┘
```

所有硬件与控制器实例通过 `HardwareManager` 全局管理，由 1ms 硬件定时器驱动 ISR 回调，保证 PID 与流量计算的实时性。

---

## 硬件清单

| 类别 | 器件 | 说明 |
|------|------|------|
| 主控 | ESP32 开发板 | 推荐 ESP32-DevKitC |
| 传感器 | BME280 | I2C 温度/气压传感器 |
| 执行器 | 蠕动泵（双向） | H 桥驱动，控制液体体积 |
| 执行器 | 冷却水泵（双向） | H 桥驱动，注水/排水冷却 |
| 执行器 | 气泵（单向） | MOSFET PWM 驱动，气体循环 |
| 执行器 | 加热器 + 风扇 | MOSFET PWM 驱动 |
| 执行器 | 电磁阀（常闭） | MOSFET 驱动，失电关闭 |
| 传感器 | 流量计 ×2 | 脉冲式，进/出水各一路 |
| 显示 | SH1106 OLED 128×64 | I2C 接口 |
| 输入 | 轻触按键 ×4 | 左 / 右 / 确认 / 返回 |
| 容器 | 试剂瓶（大）325 mL | 气体腔主体 |
| 容器 | 试剂瓶（小）135 mL | 辅助腔 |

> 接线图与原理图请参见 `docs/schematic.pdf`（待补充）。

---

## 引脚定义

所有引脚定义集中在 [include/hw_config.h](include/hw_config.h)，修改此文件即可适配不同接线方案。

| 功能 | GPIO |
|------|------|
| I2C SDA | 21 |
| I2C SCL | 22 |
| 按键 LEFT | 39 |
| 按键 RIGHT | 34 |
| 按键 SELECT | 36 |
| 按键 BACK | 35 |
| 进水流量计 | 33 |
| 出水流量计 | 32 |
| 主水泵 IN1/IN2/PWM | 17 / 16 / 4 |
| 冷却泵 IN1/IN2/PWM | 18 / 19 / 23 |
| 加热器 PWM | 26 |
| 风扇 PWM | 25 |
| 气泵 PWM | 27 |
| 电磁阀 | 14 |

---

## 软件依赖

通过 PlatformIO 自动安装，无需手动下载。

```ini
platform  = espressif32 @ 6.9.0
framework = arduino

lib_deps =
    adafruit/Adafruit BME280 Library @ ^2.3.0
    olikraus/U8g2 @ ^2.36.18
    etlcpp/Embedded Template Library @ ^20.47.1
    staacks/phyphox BLE @ ^1.2.6
```

---

## 快速上手

### 环境准备

1. 安装 [VS Code](https://code.visualstudio.com/) 与 [PlatformIO 插件](https://platformio.org/install/ide?install=vscode)
2. 克隆本仓库：
   ```bash
   git clone https://github.com/<your-username>/Gaslaw_Device.git
   cd Gaslaw_Device
   ```

### 配置引脚

根据实际接线修改 [include/hw_config.h](include/hw_config.h) 中的 GPIO 编号。

### 编译与烧录

```bash
# 编译
pio run

# 编译并烧录
pio run --target upload

# 打开串口监视器（115200 baud）
pio device monitor
```

### 首次运行

1. 上电后 OLED 显示主菜单
2. 用 **LEFT / RIGHT** 切换菜单项，**SELECT** 确认，**BACK** 返回
3. 进入对应实验前，先在设置页配置实验参数
4. 打开手机 phyphox App，搜索蓝牙设备 `GaslawDevice` 并连接

---

## 实验说明

### 玻意耳定律（等温过程）

**原理**：温度恒定时，$P \times V = \text{常数}$

装置将气体温度稳定在目标值（默认 25°C），然后通过蠕动泵分 7 步改变液体体积（即改变气体体积），每步等待压力稳定后记录 $(P, V, T)$ 数据点，验证 $PV$ 乘积的恒定性。

**实验流程**：
```
初始化体积 → 温度稳定检查 → 逐步改变体积
→ 气泵循环混气 → 等待压力稳定 → 记录数据 → 完成
```

**默认参数**：7 个体积节点，初始总气体体积约 400 mL，压力稳定判据：连续 4 次采样偏差 < 0.3 hPa。

---

### 查理定律（等压过程）

**原理**：压力恒定时，$V / T = \text{常数}$

装置固定气体体积（默认 250 mL），通过加热器分步升温（30°C → 60°C，7 个节点），压力控制回路实时调节体积以维持恒压，每步同时满足温度稳定（±0.2°C）与压力稳定（±0.5 hPa）后记录数据。

**实验流程**：
```
初始化体积 → 逐步升温 → 双稳定判断（温度 + 压力）→ 记录数据 → 完成
```

---

### 盖-吕萨克定律（等容过程）

**原理**：体积恒定时，$P / T = \text{常数}$

装置固定气体体积（默认 250 mL），压力控制回路维持恒定压力，通过加热器分步升温，每步到达目标温度后稳定 1s 记录 $(P, T)$ 数据点，验证 $P/T$ 比值的恒定性。

**实验流程**：
```
初始化体积 → 初始化压力 → 逐步升温 → 节点稳定等待 → 记录数据 → 完成
```

---

## phyphox 数据导出

1. 在手机应用商店搜索并安装 **phyphox**（iOS / Android 均可）
2. 打开 App → 点击右上角 `+` → 选择**通过蓝牙添加实验**
3. 搜索到 `GaslawDevice` 后点击连接
4. 实验运行期间，App 实时显示温度（°C）、压力（hPa）、体积（mL）三路数据
5. 实验结束后，点击 App 右上角菜单 → **导出数据** → 选择 CSV 格式保存

> 数据推送间隔：500 ms

---

## 工程结构

```
Gaslaw_Device/
├── platformio.ini                  # PlatformIO 工程配置
├── .gitignore
├── include/
│   ├── README
│   └── hw_config.h                 # 硬件引脚与参数配置
├── src/                            # 主程序源码
│   ├── main.cpp
│   ├── HardwareManager.h / .cpp    # 硬件统一管理
│   ├── input/
│   │   └── key.h / .cpp            # 按键输入
│   └── apps/                       # 应用模块
│       ├── debug/
│       │   └── AppDebug.h / .cpp
│       ├── experiment/
│       │   ├── AppBoyle.h / .cpp       # 玻意耳实验界面
│       │   ├── AppCharles.h / .cpp     # 查理实验界面
│       │   └── AppGayLussac.h / .cpp   # 盖-吕萨克实验界面
│       └── sensor/
│           └── AppEnvironment.h / .cpp
├── lib/                            # 自定义库
│   ├── Hardware/                   # 硬件驱动层
│   │   ├── AirPump.h / .cpp        # 气泵驱动
│   │   ├── AirValve.h / .cpp       # 电磁阀驱动
│   │   ├── BME280sensor.h / .cpp   # 温压传感器
│   │   ├── Flowsensor.h / .cpp     # 流量传感器
│   │   ├── Heater.h / .cpp         # 加热器驱动
│   │   └── Pumper.h / .cpp         # 泵控制器封装
│   ├── Control/                    # 控制逻辑层
│   │   ├── PressureControl.h / .cpp
│   │   ├── TempControl.h / .cpp
│   │   └── VolumeControl.h / .cpp
│   ├── Experiment/                 # 气体定律实验逻辑
│   │   ├── BoyleLaw.h / .cpp       # 玻意耳定律
│   │   ├── CharlesLaw.h / .cpp     # 查理定律
│   │   └── GayLussacLaw.h / .cpp   # 盖-吕萨克定律
│   └── PixelUI-main/               # 嵌入式 UI 框架
│       ├── include/
│       │   ├── core/               # 状态机、视图管理、协程等
│       │   ├── ui/                 # AppLauncher、ListView、Popup 等
│       │   └── widgets/            # 图表、按钮、标签等控件
│       ├── src/                    # UI 框架实现
│       └── examples/               # UI 示例程序
├── test/                           # 单元测试
└── .pio/                           # PlatformIO 构建产物（不纳入版本库）
    └── libdeps/esp32dev/           # 依赖库（自动下载）
        ├── Adafruit BME280 Library
        ├── Adafruit BusIO
        ├── Adafruit Unified Sensor
        ├── phyphox BLE
        ├── U8g2
        └── Embedded Template Library
```

---

## 许可证

本项目基于 [MIT License](LICENSE) 开源。

引用本工程时，请注明出处。如本装置用于论文或课题，欢迎在致谢中提及。

---

## 致谢

- [phyphox](https://phyphox.org/) — RWTH Aachen University 开发的物理实验数据采集平台
- [U8g2](https://github.com/olikraus/u8g2) — 嵌入式单色显示驱动库
- [Adafruit BME280](https://github.com/adafruit/Adafruit_BME280_Library) — 温压传感器驱动
- [Embedded Template Library](https://www.etlcpp.com/) — 嵌入式 C++ 标准库替代
- [PixelUI](https://github.com/Lawrence-Link/PixelUI) — 面向嵌入式设备的像素级 UI 框架