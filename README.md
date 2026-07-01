# 基于 CAN 总线的分布式智能座舱系统设计与实现

基于 STM32 + ARM Linux 的分布式车载智能座舱系统，包含 CAN 总线双节点仿真固件和 Qt5 车载终端应用。支持导航、音乐、倒车影像、空调控制、WiFi 管理，具备企业级容错能力（节点存活检测、传感器故障标志、滚动计数器、CAN 诊断帧）。

## 项目结构

```
smart_cockpit/
├── shared/                  # 跨项目共享（CAN 协议权威源 + DBC 文件）
├── stm32_can_node/          # CAN 总线仿真固件（FreeRTOS，双节点）
├── vehicle-terminal/        # 车载 Linux 终端应用（Qt 5.15）
└── docs/                    # PCB 设计 + 改造总结文档
```

## 系统架构

```
┌──────────────────────────────────────────────────┐
│  vehicle-terminal (ARM Linux, Qt 5.15)            │
│  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐            │
│  │ 导航  │ │ 音乐  │ │ 倒车  │ │ 设置  │  ...       │
│  ├──────┴─┴──────┴─┴──────┴─┴──────┤            │
│  │     单例管理器层 (core/)         │            │
│  │  CanManager / EC20Manager       │            │
│  │  Rte (CAN/Mock 信号分发)        │            │
│  │  ComLayer (计数器校验)           │            │
│  └──────────────┬─────────────────┘            │
│                 │ CAN (SocketCAN)               │
└─────────────────┼──────────────────────────────┘
                  │
┌─────────────────┼──────────────────────────────┐
│  stm32_can_node (FreeRTOS, CAN 250kbps)        │
│  ┌───────────┐            ┌───────────┐        │
│  │  节点 A    │  ←CAN→    │  节点 B    │        │
│  │  动力域    │            │  车身域    │        │
│  │  上报 0x180 │           │  上报 0x200 │       │
│  │  接收 0x080 │           │  接收 0x100 │       │
│  └───────────┘            └───────────┘        │
│  CAN 容错: BusOff 自动恢复 + TEC/REC 监控       │
└────────────────────────────────────────────────┘
```

## CAN 协议

协议以 [tools/can_signals.json](tools/can_signals.json) 为单一数据源，运行 `python tools/dbc_generate_header.py` 自动生成 [can_protocol_common.h](shared/can_protocol_common.h)、[signal_id.h](tools/signal_id.h) 和 [smart_cockpit.dbc](shared/smart_cockpit.dbc)。

ID 分配方案：`bit[10:8]=node, bit[7]=dir(0=control优先), bit[6:0]=signal`

| 范围 | 优先级 | 方向 |
|---|---|---|
| 0x080-0x0FF | 最高 | 主控→节点A 控制 |
| 0x100-0x17F | 高 | 主控→节点B 控制 |
| 0x180-0x1FF | 中 | 节点A→主控 上报 |
| 0x200-0x27F | 低 | 节点B→主控 上报 |
| 0x300-0x37F | 最低 | 诊断帧(10s周期) |

## 硬件

| 组件 | 型号/规格 |
|---|---|
| 下位机 MCU | STM32F103C8Tx × 2 |
| 车机平台 | ARM Linux (Yocto/Poky) |
| CAN 总线 | 250 kbps |
| 传感器 | HC-SR04 超声波、DHT11 温湿度、ADC 电位器 |
| 执行器 | 蜂鸣器、LED、舵机 SG90、直流电机 L9110S |
| 显示屏 | SSD1306 OLED 128×32 (I2C) |
| 档位选择 | 三档拨动开关 (P=0/R=1/D=2，P=0 防御性) |

## 快速开始

### stm32_can_node

```bash
# VS Code + EIDE 插件打开 stm32_can_node/
# Ctrl+Shift+B 构建 → EIDE 任务烧录
```

### vehicle-terminal

```bash
cmake --preset x86-release
cmake --build build/x86-release -- -j$(nproc)
./build/x86-release/vehicle-terminal
```

### 代码生成

```bash
# 编辑 tools/can_signals.json 后运行
python tools/dbc_generate_header.py
# 输出: shared/can_protocol_common.h + tools/signal_id.h + .c + shared/smart_cockpit.dbc
```

## 软件分层

```
SWC 层   HomePage / ToolsPage / rearViewCamera         ← 纯业务逻辑
  │        Rte 信号 (speedChanged / fanSpeedChanged ...)
RTE 层   Rte (mode: CAN / MOCK)                         ← 虚拟功能总线
  │        onCanFrame() 解析信号 / ComLayer::processCanFrame() 校验
COM 层   comlayer.h/cpp (Qt) / com_layer.c (STM32)      ← 信号↔CAN帧 双向打包
  │        Com_SendSignal/Com_ReceiveSignal / Com_Flush
Driver   canmanager.cpp (SocketCAN) / can_drv.c (HAL)   ← 硬件抽象
```

## 文档

**入门**：
- [CAN 总线入门指南](docs/CAN总线入门指南.md) — 零基础到理解本项目 CAN 设计
- [AUTOSAR 入门指南](docs/AUTOSAR入门指南.md) — AUTOSAR 架构 + 本项目对照

**技术深入**：
- [FreeRTOS 实时操作系统实践](docs/FreeRTOS实时操作系统实践.md) — 7 任务架构 + ISR 通信模式
- [COM 层信号系统设计详解](docs/COM层信号系统设计详解.md) — 描述符表 + 单缓冲 + 代码生成
- [STM32 CAN 控制器实战](docs/STM32-CAN控制器实战.md) — 位定时 + 滤波器 + 错误管理
- [车载 Qt 架构设计](docs/车载Qt架构设计.md) — 单例 + RTE + Mock 模式
- [DBC 工具链实战](docs/DBC工具链实战.md) — DBC 结构 + cantools + SavvyCAN
- [ARM 交叉编译与部署](docs/ARM交叉编译与部署.md) — CMakePresets + Yocto SDK + SCP

**面试与总结**：
- [面试准备与项目话术](docs/面试准备与项目话术.md) — 高频问题 + 回答模板 + 简历用
- [CAN 总线企业级改造总结](docs/CAN总线企业级改造总结.md) — 完整改造过程 + 面试素材
- [毕业论文](docs/毕业论文-基于CAN总线的分布式智能座舱系统设计与实现.md)
